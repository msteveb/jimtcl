#include <stdio.h>
#include <stdlib.h>

#define JIM_EMBEDDED
#include "jim.h"

int main(int argc, char **argv)
{
    int retcode;
    Jim_Interp *interp;

    Jim_InitEmbedded(); /* This is the first function embedders should call. */

    if (argc == 1)
        return Jim_InteractivePrompt();

    /* Load the program */
    if (argc != 2) {
        fprintf(stderr, "usage: jimsh [FILENAME] [ARGUMENTS ...]\n");
        exit(1);
    }

    /* Run it */
    interp = Jim_CreateInterp();
    Jim_RegisterCoreCommands(interp);
    if ((retcode = Jim_EvalFile(interp, argv[1])) == JIM_ERR) {
        Jim_PrintErrorMessage(interp);
    }
    Jim_FreeInterp(interp);
    return retcode;
}
