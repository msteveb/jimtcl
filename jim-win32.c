/* WIN32 extension
 *
 * Copyright (C) 2005 Pat Thoyts <patthoyts@users.sourceforge.net>
 *
 * $Id: jim-win32.c,v 1.21 2005/04/06 10:13:03 patthoyts Exp $
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * A copy of the license is also included in the source distribution
 * of Jim, as a TXT file name called LICENSE.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <shellapi.h>
#include <lmcons.h>
#include <ctype.h>

#define JIM_EXTENSION
#include "jim.h"

#if _MSC_VER >= 1000
#pragma comment(lib, "shell32")
#pragma comment(lib, "user32")
#pragma comment(lib, "advapi32")
#pragma comment(lib, "psapi")
#endif /* _MSC_VER >= 1000 */

__declspec(dllexport) int Jim_OnLoad(Jim_Interp *interp);

static HINSTANCE g_hInstance = 0;

BOOL APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID reserved)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInstance = hInstance;
    }
    return TRUE;
}

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

/* win32.ShellExecute verb file args */
static int 
Win32_ShellExecute(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    int r;
    const char *verb, *file, *parm = NULL;
    char cwd[MAX_PATH + 1];
    
    if (objc < 3 || objc > 4) {
        Jim_WrongNumArgs(interp, 1, objv, "verb path ?parameters?");
        return JIM_ERR;
    }
    verb = Jim_GetString(objv[1], NULL);
    file = Jim_GetString(objv[2], NULL);
    GetCurrentDirectoryA(MAX_PATH + 1, cwd);
    if (objc == 4) 
        parm = Jim_GetString(objv[3], NULL);
    r = (int)ShellExecuteA(NULL, verb, file, parm, cwd, SW_SHOWNORMAL);
    if (r < 33)
        Jim_SetResult(interp, 
            Win32ErrorObj(interp, "ShellExecute", GetLastError()));
    return (r < 33) ? JIM_ERR : JIM_OK;
}


/* win32.FindWindow title ?class? */
static int
Win32_FindWindow(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    const char *title = NULL, *class = NULL;
    HWND hwnd = NULL;
    int r = JIM_OK;

    if (objc < 2 || objc > 3) {
        Jim_WrongNumArgs(interp, 1, objv, "title ?class?");
        return JIM_ERR;
    }
    title = Jim_GetString(objv[1], NULL);
    if (objc == 3)
        class = Jim_GetString(objv[2], NULL);
    hwnd = FindWindowA(class, title);

    if (hwnd == NULL) {
        Jim_SetResult(interp, 
            Win32ErrorObj(interp, "FindWindow", GetLastError()));
        r = JIM_ERR;
    } else {
        Jim_SetResult(interp, Jim_NewIntObj(interp, (long)hwnd));
    }
    return r;
}

/* win32.CloseWindow windowHandle */
static int
Win32_CloseWindow(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    long hwnd;

    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "?windowHandle?");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, objv[1], &hwnd) != JIM_OK)
        return JIM_ERR;
    if (!CloseWindow((HWND)hwnd)) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "CloseWindow", GetLastError()));
        return JIM_ERR;
    }
    return JIM_OK;
}

static int
Win32_CreateWindow(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
#if 0
    Jim_SetResultString(interp, "not implemented", -1);
    return JIM_ERR;
#else
    int r = JIM_ERR;
    HINSTANCE hInst;
    HWND hwnd;
    WNDCLASSEX wc;
    TCHAR szClass[16] = _T("JimWindowClass");
    TCHAR szTitle[16] = _T("JimWindow");

    hInst = g_hInstance; //(HINSTANCE)GetModuleHandle(NULL);

    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = DefWindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszMenuName  = szClass;
    wc.lpszClassName = szClass;
    
    if (! RegisterClassEx(&wc)) {
        Jim_SetResult(interp,
                      Win32ErrorObj(interp, "RegisterClassEx", GetLastError()));
        return JIM_ERR;
    }

    hwnd = CreateWindow(szClass, szTitle,
                        WS_VISIBLE | WS_OVERLAPPED,
                        CW_USEDEFAULT, CW_USEDEFAULT, 
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        HWND_DESKTOP, NULL, hInst, NULL);
    if (hwnd) {
        SetWindowLong(hwnd, GWL_USERDATA, (LONG)interp);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        Jim_SetResult(interp, Jim_NewIntObj(interp, (DWORD)hwnd));
        r = JIM_OK;
    } else {
        Jim_SetResult(interp,
                      Win32ErrorObj(interp, "CreateWindow", GetLastError()));
        r = JIM_ERR;
    }

    return r;
#endif
}

static int
Win32_GetActiveWindow(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    JIM_NOTUSED(objc);
    JIM_NOTUSED(objv);

    Jim_SetResult(interp, Jim_NewIntObj(interp, (DWORD)GetActiveWindow()));
    return JIM_OK;
}

static int
Win32_SetActiveWindow(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    HWND hwnd, old;
    int r = JIM_OK;

    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "windowHandle");
        return JIM_ERR;
    }
    r = Jim_GetLong(interp, objv[1], (long *)&hwnd);
    if (r == JIM_OK) {
        old = SetActiveWindow(hwnd);
        if (old == NULL) {
            Jim_SetResult(interp,
                Win32ErrorObj(interp, "SetActiveWindow", GetLastError()));
            r = JIM_ERR;
        } else {
            Jim_SetResult(interp, Jim_NewIntObj(interp, (long)old));
        }
    }
    return r;
}

static int
Win32_SetForegroundWindow(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    HWND hwnd;
    int r = JIM_OK;

    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "windowHandle");
        return JIM_ERR;
    }
    r = Jim_GetLong(interp, objv[1], (long *)&hwnd);
    if (r == JIM_OK) {
        if (!SetForegroundWindow(hwnd)) {
            Jim_SetResult(interp,
                Win32ErrorObj(interp, "SetForegroundWindow", GetLastError()));
            r = JIM_ERR;
        }
    }
    return r;
}

static int
Win32_Beep(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    long freq, duration;
    int r = JIM_OK;

    if (objc != 3) {
        Jim_WrongNumArgs(interp, 1, objv, "freq duration");
        return JIM_ERR;
    }
    r = Jim_GetLong(interp, objv[1], &freq);
    if (r == JIM_OK)
        r = Jim_GetLong(interp, objv[2], &duration);
    if (freq < 0x25) freq = 0x25;
    if (freq > 0x7fff) freq = 0x7fff;
    if (r == JIM_OK) {
        if (!Beep(freq, duration)) {
            Jim_SetResult(interp, 
                Win32ErrorObj(interp, "Beep", GetLastError()));
            r = JIM_ERR;
        }
    }
    return r;
}

static int
Win32_GetComputerName(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    char name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH;
    int r = JIM_OK;

    if (objc != 1) {
        Jim_WrongNumArgs(interp, 1, objv, "");
        return JIM_ERR;
    }

    if (GetComputerNameA(name, &size)) {
        Jim_Obj *nameObj = Jim_NewStringObj(interp, name, size);
        Jim_SetResult(interp, nameObj);
    } else {
        Jim_SetResult(interp, 
            Win32ErrorObj(interp, "GetComputerName", GetLastError()));
        r = JIM_ERR;
    }
    
    return r;
}

static int
Win32_GetUserName(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    char name[UNLEN + 1];
    DWORD size = UNLEN;
    int r = JIM_OK;

    if (objc != 1) {
        Jim_WrongNumArgs(interp, 1, objv, "");
        return JIM_ERR;
    }

    if (GetUserNameA(name, &size)) {
        Jim_Obj *nameObj = Jim_NewStringObj(interp, name, size);
        Jim_SetResult(interp, nameObj);
    } else {
        Jim_SetResult(interp, 
            Win32ErrorObj(interp, "GetUserName", GetLastError()));
        r = JIM_ERR;
    }
    
    return r;
}

static int
Win32_GetModuleFileName(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    HMODULE hModule = NULL;
    char path[MAX_PATH];
    DWORD len = 0;

    if (objc > 2) {
        Jim_WrongNumArgs(interp, 1, objv, "?moduleid?");
        return JIM_ERR;
    }

    if (objc == 2) {
        if (Jim_GetLong(interp, objv[1], (long *)&hModule) != JIM_OK) {
            return JIM_ERR;
        }
    }

    len = GetModuleFileNameA(hModule, path, MAX_PATH);
    if (len != 0) {
        Jim_Obj *pathObj = Jim_NewStringObj(interp, path, len);
        Jim_SetResult(interp, pathObj);
    } else {
        Jim_SetResult(interp, 
            Win32ErrorObj(interp, "GetModuleFileName", GetLastError()));
        return JIM_ERR;
    }
    
    return JIM_OK;
}

static int
Win32_GetVersion(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    JIM_NOTUSED(objc);
    JIM_NOTUSED(objv);

    Jim_SetResult(interp, Jim_NewIntObj(interp, GetVersion()));
    return JIM_OK;
}

static int
Win32_GetTickCount(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    JIM_NOTUSED(objc);
    JIM_NOTUSED(objv);

    Jim_SetResult(interp, Jim_NewIntObj(interp, GetTickCount()));
    return JIM_OK;
}

static int
Win32_GetSystemTime(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    Jim_Obj *a[16];
    size_t n = 0;
    SYSTEMTIME t;
    JIM_NOTUSED(objc);
    JIM_NOTUSED(objv);

    GetSystemTime(&t);

#define JIMADD(name) \
    a[n++] = Jim_NewStringObj(interp, #name, -1); \
    a[n++] = Jim_NewIntObj(interp, t.w ## name )
    
    JIMADD(Year);
    JIMADD(Month);
    JIMADD(DayOfWeek);
    JIMADD(Day);
    JIMADD(Hour);
    JIMADD(Minute);
    JIMADD(Second);
    JIMADD(Milliseconds);
#undef JIMADD

    Jim_SetResult(interp, Jim_NewListObj(interp, a, n));
    return JIM_OK;
}

// Declared here because its not available without recent versions of the
// Platform SDK. mingw32 doesn't declare it all either.
typedef struct _PERFORMANCE_INFORMATION {
    DWORD cb;
    SIZE_T CommitTotal;
    SIZE_T CommitLimit;
    SIZE_T CommitPeak;
    SIZE_T PhysicalTotal;
    SIZE_T PhysicalAvailable;
    SIZE_T SystemCache;
    SIZE_T KernelTotal;
    SIZE_T KernelPaged;
    SIZE_T KernelNonpaged;
    SIZE_T PageSize;
    DWORD HandleCount;
    DWORD ProcessCount;
    DWORD ThreadCount;
} PERFORMANCE_INFORMATION;
typedef BOOL (STDAPICALLTYPE *LPFNGETPERFORMANCEINFO)(PERFORMANCE_INFORMATION *, DWORD);

static int
Win32_GetPerformanceInfo(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    Jim_Obj *a[26];
    size_t n = 0;
    PERFORMANCE_INFORMATION pi;
    LPFNGETPERFORMANCEINFO lpfnGetPerformanceInfo = NULL;
    HMODULE hLib = (HMODULE)Jim_CmdPrivData(interp);

    if (hLib != NULL)
        lpfnGetPerformanceInfo = (LPFNGETPERFORMANCEINFO)GetProcAddress(hLib, "GetPerformanceInfo");
    if (lpfnGetPerformanceInfo == NULL) {
        /* should never happen */
        Jim_SetResultString(interp, "argh!", -1);
        return JIM_ERR;
    }

    pi.cb = sizeof(pi);
    if (!lpfnGetPerformanceInfo(&pi, sizeof(pi))) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "GetPerformanceInfo", GetLastError()));
        return JIM_ERR;
    }

#define JIMADD(name) \
    a[n++] = Jim_NewStringObj(interp, #name, -1); \
    a[n++] = Jim_NewIntObj(interp, pi. ## name )

    JIMADD(CommitTotal);
    JIMADD(CommitLimit);
    JIMADD(CommitPeak);
    JIMADD(PhysicalTotal);
    JIMADD(PhysicalAvailable);
    JIMADD(SystemCache);
    JIMADD(KernelTotal);
    JIMADD(KernelPaged);
    JIMADD(KernelNonpaged);
    JIMADD(PageSize);
    JIMADD(HandleCount);
    JIMADD(ProcessCount);
    JIMADD(ThreadCount);
#undef JIMADD

    Jim_SetResult(interp, Jim_NewListObj(interp, a, n));
    return JIM_OK;
}

static int
Win32_GetCursorInfo(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    Jim_Obj *a[8];
    size_t n = 0;
    CURSORINFO ci = {0};

    JIM_NOTUSED(objc);
    JIM_NOTUSED(objv);

    ci.cbSize = sizeof(ci);
    if (!GetCursorInfo(&ci)) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "GetCursorInfo", GetLastError()));
        return JIM_ERR;
    }
    
#define JIMADDN(name) a[n++] = Jim_NewStringObj(interp, #name, -1);
#define JIMADDV(v)    a[n++] = Jim_NewIntObj(interp, (v));
    JIMADDN(flags);   JIMADDV(ci.flags);
    JIMADDN(hCursor); JIMADDV((DWORD)ci.hCursor);
    JIMADDN(x);       JIMADDV(ci.ptScreenPos.x);
    JIMADDN(y);       JIMADDV(ci.ptScreenPos.y);
#undef JIMADDN
#undef JIMADDV

    Jim_SetResult(interp, Jim_NewListObj(interp, a, n));
    return JIM_OK;
}

static int
Win32_GetCursorPos(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    Jim_Obj *a[2];
    POINT pt;
    JIM_NOTUSED(objc);
    JIM_NOTUSED(objv);

    if (!GetCursorPos(&pt)) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "GetCursorPos", GetLastError()));
        return JIM_ERR;
    }
    a[0] = Jim_NewIntObj(interp, pt.x);
    a[1] = Jim_NewIntObj(interp, pt.y);
    Jim_SetResult(interp, Jim_NewListObj(interp, a, 2));
    return JIM_OK;
}

static int
Win32_SetCursorPos(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    int r = JIM_OK;
    POINT pt = {0};

    if (objc != 3) {
        Jim_WrongNumArgs(interp, 1, objv, "x y");
        return JIM_ERR;
    }
    
    r = Jim_GetLong(interp, objv[1], &pt.x);
    if (r == JIM_OK)
        r = Jim_GetLong(interp, objv[2], &pt.y);
    if (r == JIM_OK) {
        if (!SetCursorPos(pt.x, pt.y)) {
            Jim_SetResult(interp, 
                Win32ErrorObj(interp, "SetCursorPos", GetLastError()));
            r = JIM_ERR;
        }
    }
    return r;
}

static int
Win32_GetCursor(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    HCURSOR hCursor;
    JIM_NOTUSED(objc);
    JIM_NOTUSED(objv);

    hCursor = GetCursor();
    Jim_SetResult(interp, Jim_NewIntObj(interp, (DWORD)hCursor));
    return JIM_OK;
}

static int
Win32_SetCursor(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    HCURSOR hCursor;
    int r = JIM_OK;

    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "hCursor");
        return JIM_ERR;
    }
    
    r = Jim_GetLong(interp, objv[1], (long *)&hCursor);
    if (r == JIM_OK) {
        hCursor = SetCursor(hCursor);
        Jim_SetResult(interp, Jim_NewIntObj(interp, (DWORD)hCursor));
    }
    return r;
}

#ifndef IDC_HAND
#define IDC_HAND MAKEINTRESOURCE(32649)
#endif

static int
Win32_LoadCursor(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    HCURSOR hCursor;
    int ndx;
    static const char *name[] = {
        "appstarting", "arrow", "cross", "hand", "help", "ibeam",
        "icon", "no", "size", "sizeall", "sizenesw", "sizens",
        "sizenwse", "sizewe", "uparrow", "wait", NULL
    };
    static LPCTSTR id[] = {
        IDC_APPSTARTING, IDC_ARROW, IDC_CROSS, IDC_HAND, IDC_HELP, IDC_IBEAM,
        IDC_ICON, IDC_NO, IDC_SIZEALL, IDC_SIZEALL, IDC_SIZENESW, IDC_SIZENS,
        IDC_SIZENWSE, IDC_UPARROW, IDC_WAIT, NULL
    };

    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "name");
        return JIM_ERR;
    }
    
    if (Jim_GetEnum(interp, objv[1], name, &ndx, "cursor name", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    
    hCursor = LoadCursor((HINSTANCE)NULL, id[ndx]);
    if (hCursor == NULL) {
        Jim_SetResult(interp,
                      Win32ErrorObj(interp, "LoadCursor", GetLastError()));
        return JIM_ERR;
    }
    
    Jim_SetResult(interp, Jim_NewIntObj(interp, (DWORD)hCursor));
    return JIM_OK;
}

static int
Win32_SetComputerName(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    int r = JIM_OK;
    const char *name;
    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "computername");
        return JIM_ERR;
    }
    name = Jim_GetString(objv[1], NULL);
    if (!SetComputerNameA(name)) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "SetComputerName", GetLastError()));
        r = JIM_ERR;
    }
    return r;
}

static int
Win32_GetModuleHandle(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    HMODULE hModule = NULL;
    const char *name = NULL;

    if (objc < 1 || objc >  2) {
        Jim_WrongNumArgs(interp, 1, objv, "?name?");
        return JIM_ERR;
    }
    if (objc == 2)
        name = Jim_GetString(objv[1], NULL);
    hModule = GetModuleHandleA(name);
    if (hModule == NULL) {
        Jim_SetResult(interp, 
            Win32ErrorObj(interp, "GetModuleHandle", GetLastError()));
        return JIM_ERR;
    }
    Jim_SetResult(interp, Jim_NewIntObj(interp, (unsigned long)hModule));
    return JIM_OK;
}

static int
Win32_LoadLibrary(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    HMODULE hLib = NULL;
    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "path");
        return JIM_ERR;
    }
    hLib = LoadLibraryA(Jim_GetString(objv[1], NULL));
    if (hLib == NULL) {
        Jim_SetResult(interp, 
            Win32ErrorObj(interp, "LoadLibrary", GetLastError()));
        return JIM_ERR;
    }
    Jim_SetResult(interp, Jim_NewIntObj(interp, (unsigned long)hLib));
    return JIM_OK;
}

static int
Win32_FreeLibrary(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    HMODULE hModule = NULL;
    int r = JIM_OK;
    
    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "hmodule");
        return JIM_ERR;
    }
    
    r = Jim_GetLong(interp, objv[1], (long *)&hModule);
    if (r == JIM_OK) {
        if (!FreeLibrary(hModule)) {
            Jim_SetResult(interp, 
                Win32ErrorObj(interp, "FreeLibrary", GetLastError()));
            r = JIM_ERR;
        }
    }
    
    return r;
}


/* ----------------------------------------------------------------------
 * Cleanup for dynamically loaded commands.
 */

static void
Win32_ReleasePrivLib(Jim_Interp *interp, void *clientData)
{
    HMODULE hLib = (HMODULE)clientData;
    if (hLib)
        FreeLibrary(hLib);
}

/* ----------------------------------------------------------------------
 * package load function.
 */

int
Jim_OnLoad(Jim_Interp *interp)
{
    HMODULE hLib;

    Jim_InitExtension(interp);
    if (Jim_PackageProvide(interp, "win32", "1.0", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;

#define CMD(name) \
    Jim_CreateCommand(interp, "win32." #name , Win32_ ## name , NULL, NULL)

    CMD(ShellExecute);
    CMD(FindWindow);
    CMD(CloseWindow);
    CMD(CreateWindow);
    CMD(GetActiveWindow);
    CMD(SetActiveWindow);
    CMD(SetForegroundWindow);
    CMD(GetCursorInfo);
    CMD(GetCursorPos);
    CMD(SetCursorPos);
    CMD(GetCursor);
    CMD(SetCursor);
    CMD(LoadCursor);
    CMD(Beep);
    CMD(GetComputerName);
    CMD(SetComputerName);
    CMD(GetUserName);
    CMD(GetModuleFileName);
    CMD(GetVersion);
    CMD(GetTickCount);
    CMD(GetSystemTime);
    CMD(GetModuleHandle);
    CMD(LoadLibrary);
    CMD(FreeLibrary);

    /* Check that this DLL is available before creating the command. */
    hLib = LoadLibrary(_T("psapi"));
    if (hLib != NULL) {
        Jim_CreateCommand(interp, "win32.GetPerformanceInfo",
            Win32_GetPerformanceInfo, hLib, Win32_ReleasePrivLib);
    }

    return JIM_OK;
}
