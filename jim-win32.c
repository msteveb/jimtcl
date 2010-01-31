/*-
 * Copyright (c) 2005 Pat Thoyts <patthoyts@users.sourceforge.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 *
 * WIN32 extension
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

static HINSTANCE g_hInstance = 0;

BOOL APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID reserved)
{
    JIM_NOTUSED(reserved);
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
Win32_DestroyWindow(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    long hwnd;

    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "?windowHandle?");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, objv[1], &hwnd) != JIM_OK)
        return JIM_ERR;
    if (!DestroyWindow((HWND)hwnd)) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "DestroyWindow", GetLastError()));
        return JIM_ERR;
    }
    return JIM_OK;
}

static LRESULT CALLBACK
JimWin32WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int objc = 0, r;
    Jim_Obj *objv[16];
    Jim_Interp *interp = (Jim_Interp *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    if (interp) {
        objv[objc++] = Jim_NewStringObj(interp, "WndProc", -1);
        switch (uMsg) {
            case WM_CREATE:
                //Jim_EvalObjVector(interp, objc, objv);
                break;
            case WM_COMMAND:
                objv[objc++] = Jim_NewIntObj(interp, HIWORD(wParam));
                objv[objc++] = Jim_NewIntObj(interp, LOWORD(wParam));
                objv[objc++] = Jim_NewIntObj(interp, lParam);
                r = Jim_EvalObjVector(interp, objc, objv);
                if (r == JIM_BREAK) return 0L;
                break;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static int
Win32_RegisterClass(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    WNDCLASSEXA wc;

    if (objc < 2 || objc > 3) {
        Jim_WrongNumArgs(interp, 1, objv, "classname ?windowproc?");
        return JIM_ERR;
    }

    // FIX ME: deal with the windowproc

    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = DefWindowProc;
    wc.cbClsExtra    = 16;
    wc.cbWndExtra    = 0;
    wc.hInstance     = g_hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszMenuName  =  wc.lpszClassName = Jim_GetString(objv[1], NULL);

    if (objc == 3) {
        wc.lpfnWndProc = JimWin32WindowProc;
    }
    
    if (!RegisterClassExA(&wc)) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "RegisterClassEx", GetLastError()));
        return JIM_ERR;
    }

    return JIM_OK;
}

static int
Win32_CreateWindow(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    int r = JIM_ERR;
    HWND hwnd, hwndParent = HWND_DESKTOP;
    DWORD style = WS_VISIBLE | WS_OVERLAPPEDWINDOW;
    UINT id = 0;
    const char *class, *title;

    if (objc < 3 || objc > 5) {
        Jim_WrongNumArgs(interp, 1, objv, "class title ?parent? ?id?");
        return JIM_ERR;
    }

    class = Jim_GetString(objv[1], NULL);
    title = Jim_GetString(objv[2], NULL);
    if (objc == 4) {
        if (Jim_GetLong(interp, objv[3], (long *)&hwndParent) != JIM_OK)
            return JIM_ERR;
        style = WS_VISIBLE | WS_CHILD  | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    }
    if (objc == 5) {
        if (Jim_GetLong(interp, objv[4], (long *)&id) != JIM_OK)
            return JIM_ERR;
    }

    hwnd = CreateWindowA(class, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hwndParent, (HMENU)id, g_hInstance, NULL);
    if (hwnd) {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)interp);
        Jim_SetResult(interp, Jim_NewIntObj(interp, (DWORD)hwnd));
        r = JIM_OK;
    } else {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "CreateWindow", GetLastError()));
        r = JIM_ERR;
    }

    return r;
}

static int
Win32_UpdateWindow(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    HWND hwnd;

    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "hwnd");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, objv[1], (long *)&hwnd) != JIM_OK)
        return JIM_ERR;
    if (!UpdateWindow(hwnd)) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "UpdateWindow", GetLastError()));
        return JIM_ERR;
    }
    return JIM_OK;
}

static int
Win32_MoveWindow(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    HWND hwnd;
    int n, param[4];

    if (objc != 6) {
        Jim_WrongNumArgs(interp, 1, objv, "hwnd x y width height");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, objv[1], (long *)&hwnd) != JIM_OK)
        return JIM_ERR;
    for (n = 2; n < 6; n++) {
        if (Jim_GetLong(interp, objv[n], (long*)&param[n-2]) != JIM_OK)
            return JIM_ERR;
    }

    if (!MoveWindow(hwnd, param[0], param[1], param[2], param[3], TRUE)) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "MoveWindow", GetLastError()));
        return JIM_ERR;
    }
    return JIM_OK;
}

static int
Win32_ShowWindow(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    HWND hwnd;
    int cmd;
    const char *cmds[] = { 
        "SW_HIDE", "SW_SHOWNORMAL", "SW_SHOWMINIMIZED", "SW_MAXIMIZE",
        "SW_SHOWNOACTIVATE", "SW_SHOW", "SW_MINIMIZE", "SW_SHOWMINNOACTIVE",
        "SW_SHOWNA", "SW_RESTORE", "SW_SHOWDEFAULT", "SW_FORCEMINIMIZE",
        NULL
    };
    if (objc != 3) {
        Jim_WrongNumArgs(interp, 1, objv, "windowhandle option");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, objv[1], (long *)&hwnd) != JIM_OK)
        return JIM_ERR;
    if (Jim_GetEnum(interp, objv[2], cmds, &cmd, "command", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    
    
    if (!ShowWindow(hwnd, cmd)) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "ShowWindow", GetLastError()));
        return JIM_ERR;
    }
    return JIM_OK;
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

static int
Win32_GetSystemInfo(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    Jim_Obj *a[20];
    SYSTEM_INFO si;
    int n = 0;
    struct pa_map { int arch; const char *name; };
    struct pa_map *p, map[] = {
        { PROCESSOR_ARCHITECTURE_INTEL,  "intel" },
        { PROCESSOR_ARCHITECTURE_MIPS,   "mips" },
        { PROCESSOR_ARCHITECTURE_ALPHA,  "alpha" },
        { PROCESSOR_ARCHITECTURE_PPC,    "ppc" },
        { PROCESSOR_ARCHITECTURE_SHX,    "shx" },
        { PROCESSOR_ARCHITECTURE_ARM,    "arm" },
        { PROCESSOR_ARCHITECTURE_IA64,   "ia64" },
        { PROCESSOR_ARCHITECTURE_ALPHA64,"alpha64" },
        { PROCESSOR_ARCHITECTURE_MSIL,   "msil" },
        { PROCESSOR_ARCHITECTURE_AMD64,  "amd64"},
        { PROCESSOR_ARCHITECTURE_IA32_ON_WIN64, "ia32onwin64" },
        { PROCESSOR_ARCHITECTURE_UNKNOWN,"unknown" }
    };
    JIM_NOTUSED(objc);
    JIM_NOTUSED(objv);
    
    GetSystemInfo(&si);

    a[n++] = Jim_NewStringObj(interp, "ProcessorArchitecture", -1);
    for (p = map; p->arch != PROCESSOR_ARCHITECTURE_UNKNOWN; ++p) {
        if (p->arch == si.wProcessorArchitecture) {
            break;
        }
    }
    a[n++] = Jim_NewStringObj(interp, p->name, -1);

#define JIMADD(name,element) \
    a[n++] = Jim_NewStringObj(interp, #name, -1); \
    a[n++] = Jim_NewIntObj(interp, (jim_wide)si. ## element )

    JIMADD(PageSize, dwPageSize);
    JIMADD(MinimumApplicationAddress, lpMinimumApplicationAddress);
    JIMADD(MaximumApplicationAddress, lpMaximumApplicationAddress);
    JIMADD(ActiveProcessorMask, dwActiveProcessorMask);
    JIMADD(NumberOfProcessors, dwNumberOfProcessors);
    JIMADD(ProcessorType, dwProcessorType);
    JIMADD(AllocationGranularity, dwAllocationGranularity);
    JIMADD(ProcessorLevel, wProcessorLevel);
    JIMADD(ProcessorRevision, wProcessorRevision);
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
typedef BOOL (__stdcall  *LPFNGETPERFORMANCEINFO)(PERFORMANCE_INFORMATION *, DWORD);

static int
Win32_GetPerformanceInfo(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    Jim_Obj *a[26];
    size_t n = 0;
    PERFORMANCE_INFORMATION pi;
    LPFNGETPERFORMANCEINFO lpfnGetPerformanceInfo = NULL;
    HMODULE hLib = (HMODULE)Jim_CmdPrivData(interp);
    JIM_NOTUSED(objc);
    JIM_NOTUSED(objv);

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
    a[n++] = Jim_NewIntObj(interp, pi.name )

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

#if WINVER >= 0x0500
static int
Win32_GetCursorInfo(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    Jim_Obj *a[8];
    size_t n = 0;
    CURSORINFO ci;

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

static BOOL
Win32_GetLastInputInfo(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    struct lastinputinfo_t {
        UINT cbSize;
        DWORD dwTime;
    } lii;
    typedef BOOL (__stdcall *LPFNGETLASTINPUTINFO)(struct lastinputinfo_t *);
    LPFNGETLASTINPUTINFO lpfnGetLastInputInfo = NULL;
    HMODULE hLib = (HMODULE)Jim_CmdPrivData(interp);
    JIM_NOTUSED(objc);
    JIM_NOTUSED(objv);

    if (hLib != NULL)
        lpfnGetLastInputInfo = (LPFNGETLASTINPUTINFO)GetProcAddress(hLib, "GetLastInputInfo");
    if (lpfnGetLastInputInfo == NULL) {
        Jim_SetResultString(interp, "command not available on this platform", -1);
        return JIM_ERR;
    }

    lii.cbSize = sizeof(lii);
    if (!lpfnGetLastInputInfo(&lii)) {
        Jim_SetResult(interp,
            Win32ErrorObj(interp, "GetLastInputInfo", GetLastError()));
        return JIM_ERR;
    }
    Jim_SetResult(interp, Jim_NewIntObj(interp, lii.dwTime));
    return JIM_OK;
}

#define F(x) { #x , x }
typedef struct { const char *s; unsigned long f; } ANIMATEWINDOWFLAGSMAP;
static ANIMATEWINDOWFLAGSMAP AnimateWindowFlagsMap[] = {
    F(AW_SLIDE), F(AW_ACTIVATE), F(AW_BLEND), F(AW_HIDE), F(AW_CENTER),
    F(AW_HOR_POSITIVE), F(AW_HOR_NEGATIVE), F(AW_VER_POSITIVE),
    F(AW_VER_NEGATIVE), { NULL, 0 }
};
#undef F

static int
GetAnimateWindowFlagsFromObj(Jim_Interp *interp, Jim_Obj *listObj, DWORD *flags)
{
    int r = JIM_OK, n, nLength;
    *flags = 0;
    Jim_ListLength(interp, listObj, &nLength);
    if (r == JIM_OK) {
        for (n = 0; n < nLength; n++) {
            ANIMATEWINDOWFLAGSMAP *p;
            Jim_Obj *obj;
            r = Jim_ListIndex(interp, listObj, n, &obj, 1);
            for (p = AnimateWindowFlagsMap; r == JIM_OK && p->s != NULL; p++) {
                size_t len;
                const char *name = Jim_GetString(obj, &len);
                if (strncmp(p->s, name, len) == 0) {
                    *flags |= p->f;
                    break;
                }
            }
            if (p->s == NULL) {
                Jim_SetResultString(interp, "invalid option", -1);
                return JIM_ERR;
            }
        }
    }
        
    return r;
}

static int
Win32_AnimateWindow(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    HWND hwnd;
    DWORD dwTime = 0, dwFlags = 0;
    struct map_t { const char* s; DWORD f; };
    
    if (objc != 4) {
        Jim_WrongNumArgs(interp, 1, objv, "windowhandle time flags");
        return JIM_ERR;
    }

    if (Jim_GetLong(interp, objv[1], (long *)&hwnd) != JIM_OK)
        return JIM_ERR;
    if (Jim_GetLong(interp, objv[2], &dwTime) != JIM_OK)
        return JIM_ERR;
    if (GetAnimateWindowFlagsFromObj(interp, objv[3], &dwFlags) != JIM_OK)
        return JIM_ERR;

    if (!AnimateWindow(hwnd, dwTime, dwFlags)) {
        DWORD err = GetLastError();
        Jim_Obj *errObj;
        if (err == ERROR_SUCCESS)
            errObj = Jim_NewStringObj(interp, "error: "
                " calling thread does not own the window", -1);
        else
            errObj = Win32ErrorObj(interp, "AnimateWindow", err);
        Jim_SetResult(interp, errObj);
        return JIM_ERR;
    }
    return JIM_OK;
}

#endif /* WINVER >= 0x0500 */

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
    POINT pt;

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
 * Filesystem APIs
 */

static int
Win32_CreateDirectory(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    const char * path;

    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "path");
        return JIM_ERR;
    }

    path = Jim_GetString(objv[1], NULL);
    if (!CreateDirectoryA(path, NULL)) {
        Jim_SetResult(interp, 
            Win32ErrorObj(interp, "CreateDirectory", GetLastError()));
        return JIM_ERR;
    }
    
    return JIM_OK;
}

static int
Win32_RemoveDirectory(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    const char * path;

    if (objc != 2) {
        Jim_WrongNumArgs(interp, 1, objv, "path");
        return JIM_ERR;
    }

    path = Jim_GetString(objv[1], NULL);
    if (!RemoveDirectoryA(path)) {
        Jim_SetResult(interp, 
            Win32ErrorObj(interp, "RemoveDirectory", GetLastError()));
        return JIM_ERR;
    }
    
    return JIM_OK;
}

/*
 * CopyFile, MoveFile, CreateFile, DeleteFile, LockFile,
 * ReadFile and WriteFile should get rolled into a stream/channel thing.
 */

/* ----------------------------------------------------------------------
 * Cleanup for dynamically loaded commands.
 */

static void
Win32_ReleasePrivLib(Jim_Interp *interp, void *clientData)
{
    HMODULE hLib = (HMODULE)clientData;
    JIM_NOTUSED(interp);
    if (hLib)
        FreeLibrary(hLib);
}

/* ----------------------------------------------------------------------
 * package load function.
 */

DLLEXPORT int
Jim_OnLoad(Jim_Interp *interp)
{
    HMODULE hLib;

    Jim_InitExtension(interp);
    if (Jim_PackageProvide(interp, "win32", "1.0", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;

#define CMD(name) \
    Jim_CreateCommand(interp, "win32." #name , Win32_ ## name , NULL, NULL)

    CMD(ShellExecute);
    CMD(RegisterClass);
    CMD(CreateWindow);
    CMD(FindWindow);
    CMD(CloseWindow);
    CMD(ShowWindow);
    CMD(MoveWindow);
    CMD(UpdateWindow);
    CMD(DestroyWindow);
    CMD(GetActiveWindow);
    CMD(SetActiveWindow);
    CMD(SetForegroundWindow);
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
    CMD(GetSystemInfo);
    CMD(GetModuleHandle);
    CMD(LoadLibrary);
    CMD(FreeLibrary);
    CMD(CreateDirectory);
    CMD(RemoveDirectory);

#if WINVER >= 0x0500
    CMD(GetCursorInfo);
    CMD(AnimateWindow);
#endif

    /* Check that this DLL is available before creating the command. */
    hLib = LoadLibrary(_T("psapi"));
    if (hLib != NULL) {
        Jim_CreateCommand(interp, "win32.GetPerformanceInfo",
            Win32_GetPerformanceInfo, hLib, Win32_ReleasePrivLib);
    }
#if WINVER >= 0x0500
    hLib = LoadLibrary(_T("user32"));
    if (hLib != NULL) {
        Jim_CreateCommand(interp, "win32.GetLastInputInfo",
            Win32_GetLastInputInfo, hLib, Win32_ReleasePrivLib);
    }
#endif /* WINVER >= 0x0500 */

    return JIM_OK;
}
