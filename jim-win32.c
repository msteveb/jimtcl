/* WIN32 extension
 *
 * Copyright(C) 2005 Pat Thoyts.
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

#define JIM_EXTENSION
#include "jim.h"

#if _MSC_VER >= 1000
#pragma comment(lib, "shell32")
#pragma comment(lib, "user32")
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
	Jim_SetResult(interp, Win32ErrorObj(interp, "shellexecute", GetLastError()));
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
	Jim_SetResult(interp, Win32ErrorObj(interp, "findwindow", GetLastError()));
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
        Win32ErrorObj(interp, "closewindow", GetLastError()));^M
            return JIM_ERR;
    }
    return JIM_OK;
}

/* ---------------------------------------------------------------------- */
int
Jim_OnLoad(Jim_Interp *interp)
{
    Jim_InitExtension(interp, "1.0");
    Jim_CreateCommand(interp, "win32.ShellExecute", Win32_ShellExecute, NULL);
    Jim_CreateCommand(interp, "win32.FindWindow", Win32_FindWindow, NULL);
    Jim_CreateCommand(interp, "win32.CloseWindow", Win32_CloseWindow, NULL);
    return JIM_OK;
}
