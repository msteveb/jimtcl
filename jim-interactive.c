#include "jim.h"
#include <errno.h>

int Jim_InteractivePrompt(Jim_Interp *interp)
{
    int retcode = JIM_OK;
    Jim_Obj *scriptObjPtr;

    printf("Welcome to Jim version %d.%d, "
        "Copyright (c) 2005-8 Salvatore Sanfilippo" JIM_NL, JIM_VERSION / 100, JIM_VERSION % 100);
    Jim_SetVariableStrWithStr(interp, JIM_INTERACTIVE, "1");
    while (1) {
        char buf[1024];
        const char *result;
        int reslen;

        if (retcode != 0) {
            const char *retcodestr = Jim_ReturnCode(retcode);

            if (*retcodestr == '?') {
                printf("[%d] . ", retcode);
            }
            else {
                printf("[%s] . ", retcodestr);
            }
        }
        else
            printf(". ");
        fflush(stdout);
        scriptObjPtr = Jim_NewStringObj(interp, "", 0);
        Jim_IncrRefCount(scriptObjPtr);
        while (1) {
            const char *str;
            char state;
            int len;

            errno = 0;
            if (fgets(buf, 1024, stdin) == NULL) {
                if (errno == EINTR) {
                    continue;
                }
                Jim_DecrRefCount(interp, scriptObjPtr);
                goto out;
            }
            Jim_AppendString(interp, scriptObjPtr, buf, -1);
            str = Jim_GetString(scriptObjPtr, &len);
            if (Jim_ScriptIsComplete(str, len, &state))
                break;
            printf("%c> ", state);
            fflush(stdout);
        }
        retcode = Jim_EvalObj(interp, scriptObjPtr);
        Jim_DecrRefCount(interp, scriptObjPtr);
        result = Jim_GetString(Jim_GetResult(interp), &reslen);
        if (retcode == JIM_ERR) {
            Jim_PrintErrorMessage(interp);
        }
        else if (retcode == JIM_EXIT) {
            exit(Jim_GetExitCode(interp));
        }
        else {
            if (reslen) {
                printf("%s\n", result);
            }
        }
    }
  out:
    return 0;
}
