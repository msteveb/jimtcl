#include <jim.h>

int Jim_InteractivePrompt(Jim_Interp *interp)
{
    int retcode = JIM_OK;
    Jim_Obj *scriptObjPtr;

    printf("Welcome to Jim version %d.%d, "
           "Copyright (c) 2005-8 Salvatore Sanfilippo" JIM_NL,
           JIM_VERSION / 100, JIM_VERSION % 100);
     Jim_SetVariableStrWithStr(interp, JIM_INTERACTIVE, "1");
    while (1) {
        char buf[1024];
        const char *result;
        const char *retcodestr[] = {
            "ok", "error", "return", "break", "continue", "signal", "eval", "exit"
        };
        int reslen;

        if (retcode != 0) {
            if (retcode >= 1 && retcode < sizeof(retcodestr) / sizeof(*retcodestr))
                printf("[%s] . ", retcodestr[retcode]);
            else
                printf("[%d] . ", retcode);
        } else
            printf(". ");
        fflush(stdout);
        scriptObjPtr = Jim_NewStringObj(interp, "", 0);
        Jim_IncrRefCount(scriptObjPtr);
        while(1) {
            const char *str;
            char state;
            int len;

            if ( fgets(buf, 1024, stdin) == NULL) {
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
        } else if (retcode == JIM_EXIT) {
            exit(Jim_GetExitCode(interp));
        } else {
            if (reslen) {
                printf("%s\n", result);
            }
        }
    }
out:
    return 0;
}

