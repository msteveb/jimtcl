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

#ifndef JIM_INTEGER_SPACE
#define JIM_INTEGER_SPACE 24
#endif

#if _MSC_VER >= 1000
#pragma comment(lib, "shell32")
#pragma comment(lib, "user32")
#pragma comment(lib, "advapi32")
#pragma comment(lib, "ole32")
#pragma comment(lib, "oleaut32")
#pragma comment(lib, "uuid")
#endif /* _MSC_VER >= 1000 */

static int Ole32_Create(Jim_Interp *interp, int objc, Jim_Obj *const objv[]);
static int Ole32_Foreach(Jim_Interp *interp, int objc, Jim_Obj *const objv[]);
static int Ole32_Finalizer(Jim_Interp *interp, int objc, Jim_Obj *const objv[]);
static int Ole32_GetObject(Jim_Interp *interp, int objc, Jim_Obj *const objv[]);
static int Ole32_GetActiveObject(Jim_Interp *interp, int objc, Jim_Obj *const objv[]);
static int Ole32_Invoke(Jim_Interp *interp, int objc, Jim_Obj *const objv[]);

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
    JIM_NOTUSED(interp);

    JIM_TRACE("UnicodeFreeInternalRep 0x%08x\n", (DWORD)objPtr);
    Jim_Free(objPtr->internalRep.binaryValue.data);
    objPtr->internalRep.binaryValue.data = NULL;
    objPtr->internalRep.binaryValue.len = 0;
	objPtr->typePtr = NULL;
}

/*
 * string rep is copied and internal rep is duplicated. 
 */
void 
UnicodeDupInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    int len = srcPtr->internalRep.binaryValue.len;
    JIM_TRACE("UnicodeDupInternalRep 0x%08x duped into 0x%08x\n",
        (DWORD)srcPtr, (DWORD)dupPtr);
    interp = interp;
    dupPtr->internalRep.binaryValue.len = len;
    if (srcPtr->internalRep.binaryValue.data != NULL) {
        dupPtr->internalRep.binaryValue.data = 
            Jim_Alloc(sizeof(WCHAR) * (len + 1));
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
Jim_NewUnicodeObj(Jim_Interp *interp, LPCWSTR wsz, int len)
{
    Jim_Obj *objPtr;
    JIM_ASSERT(wsz != NULL);
    if (wsz != NULL && len < 0)
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
            Jim_Panic(NULL, "Jim_GetUnicode cannot convert item to unicode rep",
                objPtr->typePtr->name);
        }
    }
    if (lenPtr != NULL)
        *lenPtr = objPtr->internalRep.binaryValue.len;
    return (LPWSTR)objPtr->internalRep.binaryValue.data;
}

/* ----------------------------------------------------------------------
 * Package interp associated data
 */

 typedef struct Ole32PackageData {
	Jim_HashTable  table;
	jim_wide       uid;
} Ole32PackageData;

static void Ole32PackageDataDelete(Jim_Interp *interp, void *data)
{
	Ole32PackageData *pkgPtr = (Ole32PackageData *)data;
	Jim_FreeHashTable(&pkgPtr->table);
	Jim_Free(data);
}

/* ----------------------------------------------------------------------
 * Ole32 object hash table
 */

typedef struct Ole32ObjectData {
	DWORD      refcount;
	LPDISPATCH pdispatch;
	LPTYPEINFO ptypeinfo;
} Ole32ObjectData;

static unsigned int Ole32HashTableHash(const void *key)
{
    /*return Jim_DjbHashFunction(key, strlen(key));*/
    unsigned int h = 5381;
	size_t len = strlen(key);
    while(len--)
        h = (h + (h << 5)) ^ *((const char *)key)++;
    return h;
}

static const void *Ole32HashTableCopyKey(void *privdata, const void *key)
{
	int len = strlen(key);
    char *copy = Jim_Alloc(len + 1);
    JIM_NOTUSED(privdata);
    memcpy(copy, key, len);
	copy[len] = '\0';
    return copy;
}

static int Ole32HashTableCompare(void *privdata, const void *key1, const void *key2)
{
    JIM_NOTUSED(privdata);
    return strcmp(key1, key2) == 0;
}

static void Ole32HashTableDestroyKey(void *privdata, const void *key)
{
    JIM_NOTUSED(privdata);
    Jim_Free((void*)key); /* ATTENTION: const cast */
}

static void Ole32HashTableDestroyValue(void *interp, void *val)
{
	Ole32ObjectData *entryPtr = (Ole32ObjectData *)val;
	JIM_NOTUSED(interp);
	entryPtr->pdispatch->lpVtbl->Release(entryPtr->pdispatch);
	if (entryPtr->ptypeinfo != NULL)
		entryPtr->ptypeinfo->lpVtbl->Release(entryPtr->ptypeinfo);
	Jim_Free((void*)entryPtr);
}

static Jim_HashTableType Ole32HashTableType = {
    Ole32HashTableHash,         /* hash function */
    Ole32HashTableCopyKey,      /* key dup */
    NULL,                       /* val dup */
    Ole32HashTableCompare,      /* key compare */
    Ole32HashTableDestroyKey,   /* key destructor */
    Ole32HashTableDestroyValue  /* val destructor */
};

/* ---------------------------------------------------------------------- */

static void Ole32FreeInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void Ole32DupInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static int  Ole32SetFromAny(Jim_Interp *interp, Jim_Obj *objPtr);
Jim_Obj *Jim_NewOle32Obj(Jim_Interp *interp, LPDISPATCH pdispatch);

#define Ole32_DispatchPtr(o) (((Ole32ObjectData *)((o)->internalRep.ptr))->pdispatch)
#define Ole32_TypeInfoPtr(o) (((Ole32ObjectData *)((o)->internalRep.ptr))->ptypeinfo)

Jim_ObjType ole32ObjType = {
    "ole32",
    Ole32FreeInternalRep,
    Ole32DupInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

void 
Ole32FreeInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
	Ole32ObjectData *entryPtr;
	entryPtr = objPtr->internalRep.ptr;
	--entryPtr->refcount;
	if (entryPtr->refcount == 0) {
		//Ole32PackageData *pkgPtr = Jim_GetAssocData(interp, "ole32:package");
		//Jim_DeleteHashEntry(&pkgPtr->table, objPtr->bytes);
		JIM_TRACE("free ole32 object 0x%08x\n", entryPtr->pdispatch);
	}
	objPtr->internalRep.ptr = NULL;
}

void 
Ole32DupInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
	Ole32ObjectData *entryPtr;
	JIM_NOTUSED(interp);

	entryPtr = srcPtr->internalRep.ptr;
	++entryPtr->refcount;
	dupPtr->internalRep.ptr = entryPtr;
    JIM_TRACE("dup ole32 object 0x%08x from 0x%08x to 0x%08x\n", entryPtr->pdispatch, srcPtr, dupPtr);
}

int
Ole32SetFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
	Ole32PackageData *pkgPtr;
	Ole32ObjectData *entryPtr;

	JIM_TRACE("Ole32SetFromAny from 0x%08x\n", objPtr);
	if (objPtr->typePtr != &ole32ObjType) {
		pkgPtr = Jim_GetAssocData(interp, "ole32:package");

		Jim_GetString(objPtr, NULL);
		entryPtr = (Ole32ObjectData *)Jim_FindHashEntry(&pkgPtr->table, objPtr->bytes);
		if (entryPtr == NULL) {
			Jim_SetResultString(interp, "not a ole32 object", -1);
			return JIM_ERR;
		}

		Jim_FreeIntRep(interp, objPtr);
		objPtr->internalRep.ptr = entryPtr;
		++entryPtr->refcount;
		objPtr->typePtr = &ole32ObjType;
	}
	return JIM_OK;
}

static int
Ole32_Finalizer(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    //objv[1] == referencesobj.
    JIM_TRACE("Ole32_Finalizer for %s\n", Jim_GetString(objv[1], NULL));
    Jim_DeleteCommand(interp, Jim_GetString(objv[1], NULL));
    return JIM_OK;
}

/*
 *  Jim_NewOle32Obj --
 *
 *      This is the only way to create Ole32 objects in Jim. These
 *      hold a reference to the IDispatch interface of the COM
 *      object. We also attempt to acquire the typelibrary information
 *      if this is available. Objects with typeinfo know how many
 *      arguments are required for a method/property call and can
 *      manage without programmer hints.
 *
 *      The string rep never changes and when this object is destroyed
 *      we release our COM references.  
 */
Jim_Obj *
Jim_NewOle32Obj(Jim_Interp *interp, LPDISPATCH pdispatch)
{
    unsigned int n = 0;
	jim_wide id;
	char *name;
    Jim_Obj *objPtr, *refPtr;
	Ole32PackageData *pkgPtr;
	Ole32ObjectData *entryPtr;

    pkgPtr = Jim_GetAssocData(interp, "ole32:package");
	id = pkgPtr->uid++;
    name = Jim_Alloc(23);
    sprintf(name, "ole32:%" JIM_WIDE_MODIFIER, id);
    entryPtr = (Ole32ObjectData *)Jim_Alloc(sizeof(Ole32ObjectData));
    entryPtr->pdispatch = pdispatch;
    entryPtr->ptypeinfo = NULL;
	entryPtr->refcount = 1;
    pdispatch->lpVtbl->AddRef(pdispatch);

    pdispatch->lpVtbl->GetTypeInfoCount(pdispatch, &n);
    if (n != 0)
        pdispatch->lpVtbl->GetTypeInfo(pdispatch, 0, LOCALE_SYSTEM_DEFAULT,
            &entryPtr->ptypeinfo);

	//Jim_AddHashEntry(&pkgPtr->table, name, entryPtr);
	objPtr = Jim_NewStringObj(interp, name, -1);
	objPtr->internalRep.ptr = entryPtr;
    objPtr->typePtr = &ole32ObjType;

    refPtr = Jim_NewReference(interp, objPtr, Jim_NewStringObj(interp, "ole32", -1),
        Jim_NewStringObj(interp, "ole32.finalizer", -1));
    if (Jim_CreateCommand(interp, Jim_GetString(refPtr, NULL), Ole32_Invoke,
		objPtr->internalRep.ptr, NULL /*Ole32CmdDeleteProc*/) != JIM_OK) {
        JIM_ASSERT(FALSE && "Its all going wrong");
	}

    JIM_TRACE("created ole32 object 0x%08x in Jim obj 0x%08x\n", pdispatch, objPtr);
    return refPtr;
}

/* ---------------------------------------------------------------------- */

static DISPPARAMS* 
Ole32_GetDispParams(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
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

static HRESULT
VariantToJim(Jim_Interp *interp, VARIANT v, Jim_Obj **resultPtr)
{
	HRESULT hr = S_OK;

	/*
	 * FIX ME: Needs to handle VT_ARRAY and VT_BYREF flags
	 */

    switch (v.vt) {
		case VT_BOOL: 
			*resultPtr = Jim_NewStringObj(interp, (v.boolVal == VARIANT_TRUE) ? "True" : "False", -1); 
			break;
        case VT_I2:
			*resultPtr = Jim_NewIntObj(interp, v.iVal);
			break;
        case VT_I4:
			*resultPtr = Jim_NewIntObj(interp, v.lVal);
			break;
        case VT_R4:
			*resultPtr = Jim_NewDoubleObj(interp, v.fltVal);
			break;
        case VT_R8:
			*resultPtr = Jim_NewDoubleObj(interp, v.dblVal);
			break;
		case VT_UNKNOWN:
			hr = VariantChangeType(&v, &v, 0, VT_DISPATCH);
			if (SUCCEEDED(hr))
				*resultPtr = Jim_NewOle32Obj(interp, v.pdispVal);
			break;
		case VT_DISPATCH:
			*resultPtr = Jim_NewOle32Obj(interp, v.pdispVal);
			break;
		case VT_CY:	case VT_DATE: case VT_DECIMAL:
        default: {
            hr = VariantChangeType(&v, &v, VARIANT_ALPHABOOL, VT_BSTR);
            if (SUCCEEDED(hr))
                *resultPtr = Jim_NewUnicodeObj(interp, v.bstrVal, -1);
        }
    }
	return hr;
}

static int
Jim_GetIndexFromObj(Jim_Interp *interp, Jim_Obj *objPtr, const char **tablePtr,
					const char *msg, int flags, int *indexPtr)
{
    const char **entryPtr = NULL;
    const char *p1, *p2;
    const char *key = Jim_GetString(objPtr, NULL);
    int i;
    JIM_NOTUSED(msg);
    JIM_NOTUSED(flags);

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
Ole32_Invoke(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    HRESULT hr = S_OK;
    LPWSTR name;
    DISPID dispid;
    LPDISPATCH pdisp;
    Jim_Obj *resultObj = NULL;
    Ole32ObjectData *ole32Ptr = (Ole32ObjectData*)Jim_CmdPrivData(interp);
	int optind, argc = 1;
    WORD mode = DISPATCH_PROPERTYGET | DISPATCH_METHOD;
	static const char *options[] = {"-get", "-put", "-putref", "-call", NULL };
	enum { OPT_GET, OPT_PUT, OPT_PUTREF, OPT_CALL };
    
    if (objc < 2) {
        Jim_WrongNumArgs(interp, 1, objv, "?options? method|property ?args ...?");
        return JIM_ERR;
    }
 
    if (Jim_GetEnum(interp, objv[1], options, &optind, NULL, 0) == JIM_OK) {
		argc++;
		switch (optind) {
			case OPT_GET:    mode = DISPATCH_PROPERTYGET; break;
			case OPT_PUT:    mode = DISPATCH_PROPERTYPUT; break;
			case OPT_PUTREF: mode = DISPATCH_PROPERTYPUTREF; break;
			case OPT_CALL:   mode = DISPATCH_METHOD; break;
		}
	}

    /*
      if (objv[argc]->typePtr != &ole32ObjType) {
		if (Ole32SetFromAny(interp, objv[argc]) != JIM_OK) {
			Jim_SetResultString(interp, "first argument must be a ole32 created object", -1);
			return JIM_ERR;
		}
    }
    */

    pdisp = ole32Ptr->pdispatch; // Ole32_DispatchPtr(objv[argc]);
    name = Jim_GetUnicode(objv[argc], NULL);
    hr = pdisp->lpVtbl->GetIDsOfNames(pdisp, &IID_NULL, &name, 1, 
                                      LOCALE_SYSTEM_DEFAULT, &dispid);

    {
        VARIANT v;
        EXCEPINFO ei;
        DISPPARAMS *dp = NULL;
        UINT uierr;

        VariantInit(&v);
        dp = Ole32_GetDispParams(interp, objc-(argc+1), objv+argc+1);

		if (mode & DISPATCH_PROPERTYPUT || mode & DISPATCH_PROPERTYPUTREF) {
			static DISPID putid = DISPID_PROPERTYPUT;
			dp->rgdispidNamedArgs = &putid;
			dp->cNamedArgs = 1;
		}

        hr = pdisp->lpVtbl->Invoke(pdisp, dispid, &IID_NULL, LOCALE_SYSTEM_DEFAULT, mode, dp, &v, &ei, &uierr);
        Ole32_FreeDispParams(dp);

        if (SUCCEEDED(hr)) {
			hr = VariantToJim(interp, v, &resultObj);
        }
        VariantClear(&v);
    }

    if (FAILED(hr))
        resultObj = Win32ErrorObj(interp, "dispatch", (DWORD)hr);
    Jim_SetResult(interp, resultObj);
    return SUCCEEDED(hr) ? JIM_OK : JIM_ERR;
}

/* ---------------------------------------------------------------------- */

static void
Ole32CmdDeleteProc(void *privData)
{
    Ole32ObjectData *ole32Ptr = (Ole32ObjectData *)privData;
    ole32Ptr->pdispatch->lpVtbl->Release(ole32Ptr->pdispatch);
    Jim_Free(privData);
}

/* ---------------------------------------------------------------------- */

static int
Ole32_Create(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    HRESULT hr = S_OK;
    IDispatch *pdisp = NULL;
    CLSID clsid;
    
    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "progid");
        return JIM_ERR;
    }

    hr = CLSIDFromProgID(Jim_GetUnicode(objv[1], NULL), &clsid);
	if (SUCCEEDED(hr))
        hr = CoCreateInstance(&clsid, NULL, CLSCTX_SERVER, 
                              &IID_IDispatch, (LPVOID*)&pdisp);
    if (SUCCEEDED(hr)) {
        Jim_SetResult(interp, Jim_NewOle32Obj(interp, pdisp));
        pdisp->lpVtbl->Release(pdisp);
    } else {
        Jim_SetResult(interp, Win32ErrorObj(interp, "CreateObject", hr));
    }
    return SUCCEEDED(hr) ? JIM_OK : JIM_ERR;
}

static int
Ole32_GetActiveObject(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    HRESULT hr = S_OK;
    LPUNKNOWN punk = NULL;
    CLSID clsid;

    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "progid");
        return JIM_ERR;
    }

    hr = CLSIDFromProgID(Jim_GetUnicode(objv[1], NULL), &clsid);
	if (SUCCEEDED(hr))
        hr = GetActiveObject(&clsid, NULL, &punk);
    if (SUCCEEDED(hr)) {
        LPDISPATCH pdisp;
        hr = punk->lpVtbl->QueryInterface(punk, &IID_IDispatch, (LPVOID*)&pdisp);
        if (SUCCEEDED(hr)) {
            Jim_SetResult(interp, Jim_NewOle32Obj(interp, pdisp));
            pdisp->lpVtbl->Release(pdisp);
        }
        punk->lpVtbl->Release(punk);
    }
    if (FAILED(hr))
        Jim_SetResult(interp, Win32ErrorObj(interp, "GetActiveObject", hr));
    return SUCCEEDED(hr) ? JIM_OK : JIM_ERR;
}

static int
Ole32_GetObject(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    HRESULT hr = S_OK;
    LPDISPATCH pdisp = NULL;
    LPCOLESTR name;

    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "progid");
        return JIM_ERR;
    }

    name = Jim_GetUnicode(objv[1], NULL);
    hr = CoGetObject(name, NULL, &IID_IDispatch, (LPVOID*)&pdisp);
    if (SUCCEEDED(hr)) {
        Jim_SetResult(interp, Jim_NewOle32Obj(interp, pdisp));
        pdisp->lpVtbl->Release(pdisp);
    }
    if (FAILED(hr))
        Jim_SetResult(interp, Win32ErrorObj(interp, "GetObject", hr));
    return SUCCEEDED(hr) ? JIM_OK : JIM_ERR;
}

/* ole32.foreach varname $object body */
static int
Ole32_Foreach(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
	HRESULT hr = S_OK;
	IDispatch *pdisp;
	VARIANT vEnum;
	DISPPARAMS dpNull = {NULL, NULL, 0, 0};
	int result = JIM_OK;

    if (objc != 4) {
        Jim_WrongNumArgs(interp, 1, objv, "varname ole32object script");
        return JIM_ERR;
    }
	 
    if (objv[2]->typePtr != &ole32ObjType) {
        Jim_SetResultString(interp, "second argument must be a ole32 created object", -1);
        return JIM_ERR;
    }

	VariantInit(&vEnum);
    pdisp = Ole32_DispatchPtr(objv[2]);
	hr = pdisp->lpVtbl->Invoke(pdisp, DISPID_NEWENUM, &IID_NULL, LOCALE_SYSTEM_DEFAULT, 
		DISPATCH_PROPERTYGET, &dpNull, &vEnum, NULL, NULL);
	if (SUCCEEDED(hr)) {
		IEnumVARIANT *pEnum;
		hr = vEnum.punkVal->lpVtbl->QueryInterface(vEnum.punkVal, &IID_IEnumVARIANT, (LPVOID*)&pEnum);
		if (SUCCEEDED(hr)) {
			HRESULT hrLoop = S_OK;
			ULONG n, cbElt;
			VARIANT rgVar[16];
			for (n = 0; n < 16; n++) VariantInit(&rgVar[n]);
			do {
				hrLoop = pEnum->lpVtbl->Next(pEnum, 16, rgVar, &cbElt);
				for (n = 0; SUCCEEDED(hr) && n < cbElt; n++) {
					Jim_Obj *valPtr = NULL;
					hr = VariantToJim(interp, rgVar[n], &valPtr);
					if (SUCCEEDED(hr)) {
						Jim_SetVariable(interp, objv[1], valPtr);
				        switch (result = Jim_EvalObj(interp, objv[3])) {
							case JIM_OK: case JIM_CONTINUE: break;
							case JIM_BREAK:
							default:
								goto break_for;
						}
					}
				}
break_for:
				for (n = 0; n < cbElt; n++) VariantClear(&rgVar[n]);
			} while ((result == JIM_OK || result == JIM_CONTINUE) && hrLoop == S_OK && SUCCEEDED(hr));
			pEnum->lpVtbl->Release(pEnum);
		}
		VariantClear(&vEnum);
	}
	if (FAILED(hr))
		Jim_SetResult(interp, Win32ErrorObj(interp, "ole32.foreach", (DWORD)hr));
	if (result == JIM_BREAK) result = JIM_OK;
	return SUCCEEDED(hr) ? result : JIM_ERR;
}

/* ---------------------------------------------------------------------- */
static Jim_ObjType origCommandObjType;
static Jim_ObjType ole32CommandObjType;

void Ole32CommandFreeIntRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (origCommandObjType.freeIntRepProc != NULL)
        origCommandObjType.freeIntRepProc(interp, objPtr);
}


DLLEXPORT int
Jim_OnLoad(Jim_Interp *interp)
{
    HRESULT hr;
	Ole32PackageData *pkgPtr;
    //Jim_ObjType *commandObjType;

    Jim_InitExtension(interp);
    if (Jim_PackageProvide(interp, "win32com", "1.0", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;

    hr = CoInitialize(0);
    if (FAILED(hr)) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "CoInitialize", (DWORD)hr));
        return JIM_ERR;
    }

	pkgPtr = (Ole32PackageData *)Jim_Alloc(sizeof(Ole32PackageData));
	pkgPtr->uid = 0;
	Jim_InitHashTable(&pkgPtr->table, &Ole32HashTableType, interp);
	Jim_SetAssocData(interp, "ole32:package", Ole32PackageDataDelete, pkgPtr);

    /*
    commandObjType = Jim_GetObjType(interp, "command");
    memcpy(&origCommandObjType, commandObjType, sizeof(Jim_ObjType));
    memcpy(&ole32CommandObjType, commandObjType, sizeof(Jim_ObjType));
    ole32CommandObjType.freeIntRepProc = Ole32CommandFreeIntRep;
    memcpy(commandObjType, &ole32CommandObjType, sizeof(Jim_ObjType));
    */

    Jim_CreateCommand(interp, "ole32.create",    Ole32_Create, NULL, NULL);
    Jim_CreateCommand(interp, "ole32.invoke",    Ole32_Invoke, NULL, NULL);
    Jim_CreateCommand(interp, "ole32.foreach",   Ole32_Foreach, NULL, NULL);
    Jim_CreateCommand(interp, "ole32.finalizer", Ole32_Finalizer, NULL, NULL);
    return JIM_OK;
}

/* ----------------------------------------------------------------------
 * Local variables:
 * mode: c
 * indent-tabs-mode: nil
 * End:
 */
