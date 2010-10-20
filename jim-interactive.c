#include "jim.h"
#include <errno.h>

#define MAX_LINE_LEN 512

int Jim_InteractivePrompt(Jim_Interp *interp)
{
    int retcode = JIM_OK;
    Jim_Obj *scriptObjPtr;
    char *buf = Jim_Alloc(MAX_LINE_LEN);

    printf("Welcome to Jim version %d.%d, "
        "Copyright (c) 2005-8 Salvatore Sanfilippo" JIM_NL, JIM_VERSION / 100, JIM_VERSION % 100);
    Jim_SetVariableStrWithStr(interp, JIM_INTERACTIVE, "1");
    while (1) {
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

            if (fgets(buf, MAX_LINE_LEN, stdin) == NULL) {
                if (errno == EINTR) {
                    continue;
                }
                Jim_DecrRefCount(interp, scriptObjPtr);
                goto out;
            }
            if (Jim_Length(scriptObjPtr) != 0) {
                Jim_AppendString(interp, scriptObjPtr, "\n", 1);
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
        if (retcode == JIM_EXIT) {
            Jim_Free(buf);
            exit(Jim_GetExitCode(interp));
        }
        if (retcode == JIM_ERR) {
            Jim_MakeErrorMessage(interp);
        }
        result = Jim_GetString(Jim_GetResult(interp), &reslen);
        if (reslen) {
            printf("%s\n", result);
        }
    }
  out:
    Jim_Free(buf);
    return 0;
}
