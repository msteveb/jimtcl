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
    Jim_CreateCommand(interp, "win32.shellexecute", Win32_ShellExecute, 3, 4, NULL);
    return JIM_OK;
}
