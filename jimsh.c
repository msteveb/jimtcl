#include <stdio.h>
#include <stdlib.h>

#define JIM_EMBEDDED
#include "jim.h"

int main(int argc, char *const argv[])
{
    int retcode, n;
    Jim_Interp *interp;
    Jim_Obj *listObj, *argObj[2];

    Jim_InitEmbedded(); /* This is the first function embedders should call. */

    /* Create and initialize the interpreter */
    interp = Jim_CreateInterp();
    Jim_RegisterCoreCommands(interp);

    listObj = Jim_NewListObj(interp, NULL, 0);
    for (n = 2; n < argc; n++) {
        Jim_Obj *obj = Jim_NewStringObjNoAlloc(interp, argv[n], -1);
        Jim_ListAppendElement(interp, listObj, obj);
    }

    argObj[0] = Jim_NewStringObj(interp, "argv0", -1);
    argObj[1] = Jim_NewStringObj(interp, "argv", -1);
    for (n = 0; n < 2; n++) Jim_IncrRefCount(argObj[n]);
    Jim_SetVariable(interp, argObj[0], Jim_NewStringObjNoAlloc(interp, argv[0], -1));
    Jim_SetVariable(interp, argObj[1], listObj);
    
    if (argc == 1) {
        retcode = Jim_InteractivePrompt(interp);
    } else {
        if ((retcode = Jim_EvalFile(interp, argv[1])) == JIM_ERR) {
            Jim_PrintErrorMessage(interp);
        }
    }

    for (n = 0; n < 2; n++) Jim_DecrRefCount(interp, argObj[n]);
    Jim_FreeInterp(interp);
    return retcode;
}
