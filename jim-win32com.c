/* Copyright (C) 2005 Pat Thoyts <patthoyts@users.sourceforge.net>
 *
 * Windows COM extension.
 *
 * Example:
 *   load jim-win32com
 *   set obj [ole32 createobject "SysInfo.SysInfo"]
 *   puts "OS Version: [ole32.invoke $obj OSVersion]"
 *   unset obj
 *
 * NOTES:
 *  We could use something ro register a shutdown function so that we can
 *  call CoUninitialize() on exit.
 *
 */


#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <windows.h>
#include <ole2.h>
#include <tchar.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>

#define JIM_EXTENSION
#include "jim.h"

#if _MSC_VER >= 1000
#pragma comment(lib, "shell32")
#pragma comment(lib, "user32")
#pragma comment(lib, "advapi32")
#pragma comment(lib, "ole32")
#pragma comment(lib, "oleaut32")
#pragma comment(lib, "uuid")
#endif /* _MSC_VER >= 1000 */

/* ----------------------------------------------------------------------
 * Debugging bits
 */

#ifndef _DEBUG
#define JIM_ASSERT(x)  ((void)0)
#define JIM_TRACE      1 ? ((void)0) : LocalTrace
#else /* _DEBUG */
#define JIM_ASSERT(x) if (!(x)) _assert(#x, __FILE__, __LINE__)
#define JIM_TRACE LocalTrace
#endif /* _DEBUG */

void
LocalTrace(LPCTSTR format, ...)
{
    int n;
    const int max = sizeof(TCHAR) * 512;
    TCHAR buffer[512];
    va_list args;
    va_start (args, format);

    n = _vsntprintf(buffer, max, format, args);
    JIM_ASSERT(n < max);
    OutputDebugString(buffer);
    va_end(args);
}

/* ---------------------------------------------------------------------- */

static Jim_Obj *
Win32ErrorObj(Jim_Interp *interp, const char * szPrefix, DWORD dwError)
{
    Jim_Obj *msgObj = NULL;
    char * lpBuffer = NULL;
    DWORD  dwLen = 0;
    
    dwLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER 
        | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwError, LANG_NEUTRAL,
        (char *)&lpBuffer, 0, NULL);
    if (dwLen < 1) {
        dwLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY,
            "code 0x%1!08X!%n", 0, LANG_NEUTRAL,
            (char *)&lpBuffer, 0, (va_list *)&dwError);
    }
    
    msgObj = Jim_NewStringObj(interp, szPrefix, -1);
    if (dwLen > 0) {
        char *p = lpBuffer + dwLen - 1;        /* remove cr-lf at end */
        for ( ; p && *p && isspace(*p); p--)
            ;
        *++p = 0;
        Jim_AppendString(interp, msgObj, ": ", 2);
        Jim_AppendString(interp, msgObj, lpBuffer, -1);
    }
    LocalFree((HLOCAL)lpBuffer);
    return msgObj;
}

/* ----------------------------------------------------------------------
 * Unicode strings
 */

static void UnicodeFreeInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void UnicodeDupInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static int  UnicodeSetFromAny(Jim_Interp *interp, Jim_Obj *objPtr);

Jim_ObjType unicodeObjType = {
    "unicode",
    UnicodeFreeInternalRep,
    UnicodeDupInternalRep,
    NULL, /*UpdateUnicodeStringProc*/
    JIM_TYPE_REFERENCES,
};

static LPOLESTR
A2OLE(LPCSTR sz)
{
    DWORD nChars = 0;
    LPOLESTR wsz = NULL;
    if (sz != NULL) {
        nChars = MultiByteToWideChar(CP_ACP, 0, sz, -1, NULL, 0);
        wsz = (LPOLESTR)Jim_Alloc((nChars + 1) * sizeof(OLECHAR));
        if (wsz != NULL) {
            nChars = MultiByteToWideChar(CP_ACP, 0, sz, nChars, wsz, nChars + 1);
            wsz[nChars] = 0;
        }
    }
    return wsz;
}

static LPSTR
OLE2A(LPCOLESTR wsz)
{
    DWORD nChars = 0;
    LPSTR sz = NULL;
    if (wsz != NULL) {
        nChars = WideCharToMultiByte(CP_ACP, 0, wsz, -1, NULL, 0, NULL, NULL);
        sz = (LPSTR)Jim_Alloc((nChars + 1) * sizeof(CHAR));
        if (sz != NULL) {
            nChars = WideCharToMultiByte(CP_ACP, 0, wsz, nChars, sz, nChars+1, NULL, NULL);
            sz[nChars] = 0;
        }
    }
    return sz;
}

void 
UnicodeFreeInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
	JIM_TRACE("UnicodeFreeInternalRep 0x%08x\n", (DWORD)objPtr);
	Jim_Free(objPtr->internalRep.binaryValue.data);
	objPtr->internalRep.binaryValue.data = NULL;
    objPtr->internalRep.binaryValue.len = 0;
	objPtr->typePtr = NULL;
}

// string rep is copied and internal dep is duplicated.
void 
UnicodeDupInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    int len = srcPtr->internalRep.binaryValue.len;
	JIM_TRACE("UnicodeDupInternalRep 0x%08x duped into 0x%08x\n", (DWORD)srcPtr, (DWORD)dupPtr);
    interp;
    dupPtr->internalRep.binaryValue.len = len;
	if (srcPtr->internalRep.binaryValue.data != NULL) {
		dupPtr->internalRep.binaryValue.data = Jim_Alloc(sizeof(WCHAR) * (len + 1));
		wcsncpy((LPWSTR)dupPtr->internalRep.binaryValue.data, 
			(LPWSTR)srcPtr->internalRep.binaryValue.data, len);
	}
}

int
UnicodeSetFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int nChars;
    LPWSTR wsz;

    JIM_TRACE("UnicodeSetFromAny 0x%08x\n", (DWORD)objPtr);
	Jim_GetString(objPtr, NULL);
    Jim_FreeIntRep(interp, objPtr);

    nChars = MultiByteToWideChar(CP_ACP, 0, objPtr->bytes, objPtr->length, NULL, 0);
    wsz = Jim_Alloc((nChars + 1) * sizeof(WCHAR));
    nChars = MultiByteToWideChar(CP_ACP, 0, objPtr->bytes, objPtr->length, wsz, nChars + 1);
    wsz[nChars] = 0;

    objPtr->internalRep.binaryValue.len = nChars;
    objPtr->internalRep.binaryValue.data = (unsigned char *)wsz;
    objPtr->typePtr = &unicodeObjType;
    return JIM_OK;
}
    
Jim_Obj *
Jim_NewUnicodeObj(Jim_Interp *interp, LPCWSTR wsz, size_t len)
{
    Jim_Obj *objPtr;
    JIM_ASSERT(wsz != NULL);
    if (wsz != NULL && len == -1)
        len = wcslen(wsz);
    if (wsz == NULL || len == 0) {
		objPtr = Jim_NewStringObj(interp, "", 0);
		objPtr->internalRep.binaryValue.data = NULL;
		objPtr->internalRep.binaryValue.len = 0;
    } else {
		objPtr = Jim_NewObj(interp);
        objPtr->internalRep.binaryValue.data = Jim_Alloc(sizeof(WCHAR) * (len + 1));
        wcsncpy((LPWSTR)objPtr->internalRep.binaryValue.data, wsz, len);
        ((LPWSTR)objPtr->internalRep.binaryValue.data)[len] = 0;
        objPtr->internalRep.binaryValue.len = len;
		objPtr->bytes = OLE2A(wsz);
		objPtr->length = len;
    }
    objPtr->typePtr = &unicodeObjType;
    return objPtr;
}

LPWSTR
Jim_GetUnicode(Jim_Obj *objPtr, int *lenPtr)
{
    if (objPtr->typePtr != &unicodeObjType) {
        if (UnicodeSetFromAny(NULL, objPtr) != JIM_OK) {
            JIM_ASSERT("Jim_GetUnicode cannot convert item to unicode rep");
            Jim_Panic("Jim_GetUnicode cannot convert item to unicode rep",
				objPtr->typePtr->name);
        }
    }
    
    return (LPWSTR)objPtr->internalRep.binaryValue.data;
}

/* ---------------------------------------------------------------------- */

static void Ole32FreeInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void Ole32DupInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);

Jim_ObjType ole32ObjType = {
    "ole32",
    Ole32FreeInternalRep,
    UnicodeDupInternalRep,
    NULL, /*UpdateUnicodeStringProc*/
    JIM_TYPE_REFERENCES,
};

void 
Ole32FreeInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int r = JIM_OK;
    IDispatch *p = (IDispatch *)Jim_GetIntRepPtr(objPtr);
    JIM_TRACE("free ole32 object 0x%08x\n", (unsigned long)p);
    p->lpVtbl->Release(p);
	p = NULL;
	objPtr->typePtr = NULL;
}

void 
Ole32DupInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    IDispatch *p = (IDispatch *)Jim_GetIntRepPtr(srcPtr);
    JIM_TRACE("dup ole32 object 0x%08x\n", (unsigned long)p);
    dupPtr->internalRep.ptr = p;
    p->lpVtbl->AddRef(p);
}

static DISPPARAMS* 
Ole32_GetDispParams(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    DISPPARAMS * dp;
    int cn;

    dp = (DISPPARAMS*)Jim_Alloc(sizeof(DISPPARAMS));
    if (dp != NULL) {
        dp->cArgs = objc;
        dp->cNamedArgs = 0;
        dp->rgdispidNamedArgs = NULL;
        dp->rgvarg = NULL;
        if (objc > 0)
            dp->rgvarg = (VARIANT*)Jim_Alloc(sizeof(VARIANT) * dp->cArgs);
		
        /* Note: this array is filled backwards */
        for (cn = 0; cn < objc; cn++) {
            LPOLESTR olestr;
			Jim_Obj *objPtr = objv[objc - cn - 1];
			const char *type = NULL;
			
			if (objPtr->typePtr != NULL)
				type = objPtr->typePtr->name;

            VariantInit(&dp->rgvarg[cn]);
			if (type != NULL) {
				if (strcmp(type, "int") == 0) {
					Jim_GetLong(interp, objPtr, &(dp->rgvarg[cn].lVal));
					dp->rgvarg[cn].vt = VT_I4;
				} else if (strcmp(type, "double") == 0) {
					Jim_GetDouble(interp, objPtr, &(dp->rgvarg[cn].dblVal));
					dp->rgvarg[cn].vt = VT_R8;
				}
			}
			if (dp->rgvarg[cn].vt == VT_EMPTY) {
				olestr = A2OLE(Jim_GetString(objv[objc - cn - 1], NULL));
				dp->rgvarg[cn].bstrVal = SysAllocString(olestr);
				dp->rgvarg[cn].vt = VT_BSTR;
				Jim_Free(olestr);
			}
        }
    }
    return dp;
}

static void
Ole32_FreeDispParams(DISPPARAMS *dp)
{
    VARIANT *pv = dp->rgvarg;
    size_t n;
    for (n = 0; n < dp->cArgs; n++, pv++) {
        VariantClear(pv);
    }
    Jim_Free(dp->rgvarg);
    Jim_Free(dp);
}

static int
Jim_GetIndexFromObj(Jim_Interp *interp, Jim_Obj *objPtr, const char **tablePtr,
					const char *msg, int flags, int *indexPtr)
{
	const char **entryPtr = NULL;
	const char *p1, *p2;
	const char *key = Jim_GetString(objPtr, NULL);
	int i;
	*indexPtr = -1;
	for (entryPtr = tablePtr, i = 0; *entryPtr != NULL; entryPtr++, i++) {
		for (p1 = key, p2 = *entryPtr; *p1 == *p2; p1++, p2++) {
			if (*p1 == '\0') {
				*indexPtr = i;
				return JIM_OK;
			}
		}
	}
	Jim_SetResultString(interp, "needs a better message", -1);
	return JIM_ERR;
}

/* $object method|prop ?args...? */
static int
Ole32_Invoke(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    HRESULT hr = S_OK;
    LPWSTR name;
    DISPID dispid;
    LPDISPATCH pdisp;
    Jim_Obj *resultObj = NULL;
	int optind, argc = 1, mode = DISPATCH_PROPERTYGET | DISPATCH_METHOD;
	static const char *options[] = {"-get", "-put", "-putref", "-call", NULL };
	enum { OPT_GET, OPT_PUT, OPT_PUTREF, OPT_CALL };
    
    if (objc < 3) {
        Jim_WrongNumArgs(interp, 1, objv, "?options? object method|property ?args ...?");
        return JIM_ERR;
    }
 
	if (Jim_GetIndexFromObj(interp, objv[1], options, "", 0, &optind) == JIM_OK) {
		argc++;
		switch (optind) {
			case OPT_GET:    mode = DISPATCH_PROPERTYGET; break;
			case OPT_PUT:    mode = DISPATCH_PROPERTYPUT; break;
			case OPT_PUTREF: mode = DISPATCH_PROPERTYPUTREF; break;
			case OPT_CALL:   mode = DISPATCH_METHOD; break;
		}
	}

    if (objv[argc]->typePtr != &ole32ObjType) {
        Jim_SetResultString(interp, "first argument must be a ole32 created object", -1);
        return JIM_ERR;
    }

    pdisp = (LPDISPATCH)Jim_GetIntRepPtr(objv[argc]);
    name = Jim_GetUnicode(objv[argc+1], NULL);
    hr = pdisp->lpVtbl->GetIDsOfNames(pdisp, &IID_NULL, &name, 1, 
                                      LOCALE_SYSTEM_DEFAULT, &dispid);

    {
        VARIANT v;
        EXCEPINFO ei;
        DISPPARAMS *dp = NULL;
        UINT uierr;

        VariantInit(&v);
        dp = Ole32_GetDispParams(interp, objc-(argc+2), objv+argc+2);

		if (mode & DISPATCH_PROPERTYPUT || mode & DISPATCH_PROPERTYPUTREF) {
			static DISPID putid = DISPID_PROPERTYPUT;
			dp->rgdispidNamedArgs = &putid;
			dp->cNamedArgs = 1;
		}

        hr = pdisp->lpVtbl->Invoke(pdisp, dispid, &IID_NULL, LOCALE_SYSTEM_DEFAULT, mode, dp, &v, &ei, &uierr);
        Ole32_FreeDispParams(dp);

        if (SUCCEEDED(hr)) {
            switch (v.vt) {
                case VT_I2:  resultObj = Jim_NewIntObj(interp, v.iVal); break;
                case VT_I4:  resultObj = Jim_NewIntObj(interp, v.lVal); break;
                case VT_R4:  resultObj = Jim_NewDoubleObj(interp, v.fltVal); break;
                case VT_R8:  resultObj = Jim_NewDoubleObj(interp, v.dblVal); break;
                default: {
                    hr = VariantChangeType(&v, &v, VARIANT_ALPHABOOL, VT_BSTR);
                    if (SUCCEEDED(hr))
                        resultObj = Jim_NewUnicodeObj(interp, v.bstrVal, -1);
                }
            }
        }
        VariantClear(&v);
    }

    if (FAILED(hr))
        resultObj = Win32ErrorObj(interp, "dispatch", (DWORD)hr);
    Jim_SetResult(interp, resultObj);
    return SUCCEEDED(hr) ? JIM_OK : JIM_ERR;
}

Jim_Obj *
Jim_NewOle32Obj(Jim_Interp *interp, LPDISPATCH pdispatch)
{
    Jim_Obj *objPtr = Jim_NewObj(interp);
    objPtr->bytes = Jim_Alloc(23);
    sprintf(objPtr->bytes, "ole32:%08x", (unsigned long)pdispatch);
    objPtr->length = strlen(objPtr->bytes);
    objPtr->internalRep.ptr = (void *)pdispatch;
    pdispatch->lpVtbl->AddRef(pdispatch);
    objPtr->typePtr = &ole32ObjType;

    //refPtr = Jim_NewReference(interp, objPtr, NULL);
    //Jim_CreateCommand(interp, Jim_GetString(refPtr, NULL), Ole32_Invoke, (void *)objPtr);
    JIM_TRACE("created ole32 object 0x%08x\n", pdispatch);
    return objPtr;
}

/* ---------------------------------------------------------------------- */

/* ole32 createobject progid
 */
int
Ole32_Command(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    HRESULT hr = S_OK;
    const char *cmd;
    
    if (objc != 3) {
        Jim_WrongNumArgs(interp, 1, objv, "createobject");
        return JIM_ERR;
    }

    cmd = Jim_GetString(objv[1], NULL);
    if (strncmp(cmd, "create", 6) == 0) {
        IDispatch *pdisp = NULL;
        CLSID clsid;
        HRESULT hr = S_OK;
        LPWSTR wsz = Jim_GetUnicode(objv[2], NULL);
        hr = CLSIDFromProgID(wsz, &clsid);
		if (SUCCEEDED(hr))
            hr = CoCreateInstance(&clsid, NULL, CLSCTX_SERVER, &IID_IDispatch, (LPVOID*)&pdisp);
        if (SUCCEEDED(hr)) {
            Jim_SetResult(interp, Jim_NewOle32Obj(interp, pdisp));
            pdisp->lpVtbl->Release(pdisp);
        } else {
            Jim_SetResult(interp, Win32ErrorObj(interp, "CreateObject", hr));
        }

    } else {
        Jim_SetResultString(interp, "bad option: must be create object", -1);
        hr = E_FAIL;
    }
    return SUCCEEDED(hr) ? JIM_OK : JIM_ERR;
}

/* ---------------------------------------------------------------------- */

__declspec(dllexport) int
Jim_OnLoad(Jim_Interp *interp)
{
    HRESULT hr;
    Jim_InitExtension(interp, "1.0");
    hr = CoInitialize(0);
    if (FAILED(hr)) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "CoInitialize", (DWORD)hr));
        return JIM_ERR;
    }
    Jim_CreateCommand(interp, "ole32", Ole32_Command, NULL);
    Jim_CreateCommand(interp, "ole32.invoke", Ole32_Invoke, NULL);
    return JIM_OK;
}

/* ----------------------------------------------------------------------
 * Local variables:
 * mode: c
 * indent-tabs-mode: nil
 * End:
 */
