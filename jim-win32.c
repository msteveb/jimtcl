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
#pragma comment(lib, "shell32")

#define JIM_EXTENSION
#include "jim.h"

__declspec(dllexport) int Jim_OnLoad(Jim_Interp *interp);

/* shellexec verb file args */
static int 
Win32_ShellExecute(Jim_Interp *interp, int objc, Jim_Obj **objv)
{
    int r;
    char *verb, *file, *parm = NULL;
    char cwd[MAX_PATH + 1];

    if (objc < 3 || objc > 4) {
	Jim_WrongNumArgs(interp, 1, objv, 
	    "shellexecute verb path ?parameters?");
	return JIM_ERR;
    }
    verb = Jim_GetString(objv[1], NULL);
    file = Jim_GetString(objv[2], NULL);
    GetCurrentDirectoryA(MAX_PATH + 1, cwd);
    if (objc == 4) 
	parm = Jim_GetString(objv[3], NULL);
    r = (int)ShellExecuteA(NULL, verb, file, parm, cwd, SW_SHOWNORMAL);
    if (r < 33)
	Jim_SetResultString(interp, "failed.", -1);
    return (r < 33) ? JIM_ERR : JIM_OK;
}

int
Jim_OnLoad(Jim_Interp *interp)
{
    Jim_InitExtension(interp, "1.0");
    Jim_CreateCommand(interp, "win32.shellexecute", Win32_ShellExecute, NULL);
    return JIM_OK;
}
