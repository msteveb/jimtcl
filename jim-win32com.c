/* Copyright (C) 2005 Pat Thoyts <patthoyts@users.sourceforge.net>
 *
 * Windows COM extension.
 *
 * 
 *
 * NOTES:
 *  We could use something ro register a shutdown function so that we can
 *  call CoUninitialize() on exit.
 *
 */


#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <tchar.h>
#include <assert.h>
#include <stdarg.h>

#define JIM_EXTENSION
#include "jim.h"
#include <stdio.h>

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

void 
UnicodeFreeInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_Free(objPtr->internalRep.binaryValue.data);
    objPtr->internalRep.binaryValue.len = 0;
}

// string rep is copied and internal dep is duplicated.
void 
UnicodeDupInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    int len = srcPtr->internalRep.binaryValue.len;
    interp;
    dupPtr->internalRep.binaryValue.len = len;
    dupPtr->internalRep.binaryValue.data = Jim_Alloc(len + sizeof(WCHAR));
    wcsncpy((LPWSTR)dupPtr->internalRep.binaryValue.data, 
        (LPWSTR)srcPtr->internalRep.binaryValue.data, len);
}

int
UnicodeSetFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int nChars;
    LPWSTR wsz;

    Jim_GetString(objPtr, NULL);
    //Jim_FreeIntRep(interp, objPtr);

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
    Jim_Obj *objPtr = Jim_NewObj(interp);
    if (len < 0)
        len = wcslen(wsz);
    if (len == 0) {
        objPtr->bytes = "";
        objPtr->length = 0;
    } else {
        objPtr->internalRep.binaryValue.data = Jim_Alloc(len + sizeof(WCHAR));
        wcsncpy((LPWSTR)objPtr->internalRep.binaryValue.data, wsz, len);
        ((LPWSTR)objPtr->internalRep.binaryValue.data)[len] = 0;
        objPtr->internalRep.binaryValue.len = len;
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
            //Jim_Panic("Jim_GetUnicode cannot convert item to unicode rep",
            //objPtr->typePtr->name);
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
    IDispatch *p = (IDispatch *)objPtr->internalRep.ptr;
    r = Jim_DeleteCommand(interp, objPtr->bytes);
    JIM_ASSERT(r == JIM_OK && "Failed to delete ole32 command instance.");
    p->lpVtbl->Release(p);
    JIM_TRACE("free ole32 object 0x%08x\n", (unsigned long)p);
}

void 
Ole32DupInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    IDispatch *p = (IDispatch *)srcPtr->internalRep.ptr;
    dupPtr->internalRep.ptr = p;
    p->lpVtbl->AddRef(p);
    JIM_TRACE("dup ole32 object 0x%08x\n", (unsigned long)p);
}

/* $object method|prop ?args...? */
static int
Ole32_Invoke(Jim_Interp *interp, int objc, Jim_Obj **objv)
{
    HRESULT hr = S_OK;
    LPWSTR name;
    DISPID dispid;
    Jim_Obj *objPtr = (Jim_Obj *)Jim_CmdPrivData(interp);
    IDispatch *pdisp = (IDispatch *)objPtr->internalRep.ptr;
    
    if (objc < 2) {
        Jim_WrongNumArgs(interp, 1, objv, " method|property ?args ...?");
        return JIM_ERR;
    }
    
    name = Jim_GetUnicode(objv[1], NULL);
    hr = pdisp->lpVtbl->GetIDsOfNames(pdisp, &IID_NULL, &name, 1, 
                                      LOCALE_SYSTEM_DEFAULT, &dispid);

    if (SUCCEEDED(hr)) {
        Jim_SetResultString(interp, "ook", -1);
    }

    if (FAILED(hr))
        Jim_SetResult(interp, Win32ErrorObj(interp, "dispatch", (DWORD)hr));
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
    Jim_CreateCommand(interp, objPtr->bytes, Ole32_Invoke, (void *)objPtr);
    JIM_TRACE("created ole32 object 0x%08x\n", pdispatch);
    return objPtr;
}

/* ---------------------------------------------------------------------- */

/* ole32 createobject progid
 */
int
Ole32_Command(Jim_Interp *interp, int objc, Jim_Obj **objv)
{
    HRESULT hr = S_OK;
    char *cmd;
    
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
            hr = CoCreateInstance(&clsid, NULL, CLSCTX_ALL, &IID_IDispatch, (LPVOID*)&pdisp);
        if (hr == E_NOINTERFACE)
            hr = CoCreateInstance(&clsid, NULL, CLSCTX_LOCAL_SERVER, &IID_IDispatch, (LPVOID*)&pdisp);
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
    return JIM_OK;
}

/* ----------------------------------------------------------------------
 * Local variables:
 * mode: c
 * indent-tabs-mode: nil
 * End:
 */
