/* WIN32 extension
 *
 * Copyright (C) 2005 Pat Thoyts <patthoyts@users.sourceforge.net>
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
#include <shellapi.h>
#include <lmcons.h>
#include <psapi.h>
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
Win32_ShellExecute(Jim_Interp *interp, int objc, Jim_Obj **objv)
{
    int r;
    char *verb, *file, *parm = NULL;
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
Win32_FindWindow(Jim_Interp *interp, int objc, Jim_Obj **objv)
{
    char *title = NULL, *class = NULL;
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
Win32_CloseWindow(Jim_Interp *interp, int objc, Jim_Obj **objv)
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
Win32_GetActiveWindow(Jim_Interp *interp, int objc, Jim_Obj **objv)
{
    Jim_SetResult(interp, Jim_NewIntObj(interp, (DWORD)GetActiveWindow()));
    return JIM_OK;
}

static int
Win32_SetActiveWindow(Jim_Interp *interp, int objc, Jim_Obj **objv)
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
Win32_SetForegroundWindow(Jim_Interp *interp, int objc, Jim_Obj **objv)
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
Win32_Beep(Jim_Interp *interp, int objc, Jim_Obj **objv)
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
Win32_GetComputerName(Jim_Interp *interp, int objc, Jim_Obj **objv)
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
Win32_GetUserName(Jim_Interp *interp, int objc, Jim_Obj **objv)
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
Win32_GetModuleFileName(Jim_Interp *interp, int objc, Jim_Obj **objv)
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
Win32_GetVersion(Jim_Interp *interp, int objc, Jim_Obj **objv)
{
    Jim_SetResult(interp, Jim_NewIntObj(interp, GetVersion()));
    return JIM_OK;
}

static int
Win32_GetTickCount(Jim_Interp *interp, int objc, Jim_Obj **objv)
{
    Jim_SetResult(interp, Jim_NewIntObj(interp, GetTickCount()));
    return JIM_OK;
}

static int
Win32_GetSystemTime(Jim_Interp *interp, int objc, Jim_Obj **objv)
{
    Jim_Obj *a[16];
    SYSTEMTIME t;
    GetSystemTime(&t);

    a[0]  = Jim_NewStringObj(interp, "year", -1); 
    a[1]  = Jim_NewIntObj(interp, t.wYear);
    a[2]  = Jim_NewStringObj(interp, "month", -1); 
    a[3]  = Jim_NewIntObj(interp, t.wMonth);
    a[4]  = Jim_NewStringObj(interp, "dayofweek", -1); 
    a[5]  = Jim_NewIntObj(interp, t.wDayOfWeek);
    a[6]  = Jim_NewStringObj(interp, "day", -1); 
    a[7]  = Jim_NewIntObj(interp, t.wDay);
    a[8]  = Jim_NewStringObj(interp, "hour", -1); 
    a[9]  = Jim_NewIntObj(interp, t.wHour);
    a[10] = Jim_NewStringObj(interp, "minute", -1); 
    a[11] = Jim_NewIntObj(interp, t.wMinute);
    a[12] = Jim_NewStringObj(interp, "second", -1); 
    a[13] = Jim_NewIntObj(interp, t.wSecond);
    a[14] = Jim_NewStringObj(interp, "milliseconds", -1); 
    a[15] = Jim_NewIntObj(interp, t.wMilliseconds);

    Jim_SetResult(interp, Jim_NewListObj(interp, a, 16));
    return JIM_OK;
}

// FIX ME: win2k+
static int
Win32_GetPerformanceInfo(Jim_Interp *interp, int objc, Jim_Obj **objv)
{
    Jim_Obj *a[26];
    size_t n = 0;
    PERFORMANCE_INFORMATION pi;

    if (!GetPerformanceInfo(&pi, sizeof(pi))) {
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
Win32_SetComputerName(Jim_Interp *interp, int objc, Jim_Obj **objv)
{
    int r = JIM_OK;
    char *name;
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
Win32_GetModuleHandle(Jim_Interp *interp, int objc, Jim_Obj **objv)
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
Win32_LoadLibrary(Jim_Interp *interp, int objc, Jim_Obj **objv)
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
Win32_FreeLibrary(Jim_Interp *interp, int objc, Jim_Obj **objv)
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


/* ---------------------------------------------------------------------- */

int
Jim_OnLoad(Jim_Interp *interp)
{
    Jim_InitExtension(interp, "1.0");

#define CMD(name) \
    Jim_CreateCommand(interp, "win32." #name , Win32_ ## name , NULL)

    CMD(ShellExecute);
    CMD(FindWindow);
    CMD(CloseWindow);
    CMD(GetActiveWindow);
    CMD(SetActiveWindow);
    CMD(SetForegroundWindow);
    CMD(Beep);
    CMD(GetComputerName);
    CMD(SetComputerName);
    CMD(GetUserName);
    CMD(GetModuleFileName);
    CMD(GetVersion);
    CMD(GetTickCount);
    CMD(GetSystemTime);
    CMD(GetPerformanceInfo);
    CMD(GetModuleHandle);
    CMD(LoadLibrary);
    CMD(FreeLibrary);

    return JIM_OK;
}
