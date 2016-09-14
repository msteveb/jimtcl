/*
 * jimsh - An interactive shell for Jim
 *
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright 2009 Steve Bennett <steveb@workware.net.au>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE JIM TCL PROJECT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * JIM TCL PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the Jim Tcl Project.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jim.h"
#include "jimautoconf.h"

/* From initjimsh.tcl */
extern int Jim_initjimshInit(Jim_Interp *interp);

static void JimSetArgv(Jim_Interp *interp, int argc, char *const argv[])
{
    int n;
    Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);

    /* Populate argv global var */
    for (n = 0; n < argc; n++) {
        Jim_Obj *obj = Jim_NewStringObj(interp, argv[n], -1);

        Jim_ListAppendElement(interp, listObj, obj);
    }

    Jim_SetVariableStr(interp, "argv", listObj);
    Jim_SetVariableStr(interp, "argc", Jim_NewIntObj(interp, argc));
}

static void JimPrintErrorMessage(Jim_Interp *interp)
{
    Jim_MakeErrorMessage(interp);
    fprintf(stderr, "%s\n", Jim_String(Jim_GetResult(interp)));
}

void usage(const char* executable_name)
{
    printf("jimsh version %d.%d\n", JIM_VERSION / 100, JIM_VERSION % 100);
    printf("Usage: %s\n", executable_name);
    printf("or   : %s [options] [filename]\n", executable_name);
    printf("\n");
    printf("Without options: Interactive mode\n");
    printf("\n");
    printf("Options:\n");
    printf("      --version  : prints the version string\n");
    printf("      --help     : prints this text\n");
    printf("      -e CMD     : executes command CMD\n");
    printf("                   NOTE: all subsequent options will be passed as arguments to the command\n");
    printf("    [filename|-] : executes the script contained in the named file, or from stdin if \"-\"\n");
    printf("                   NOTE: all subsequent options will be passed to the script\n\n");
}

int main(int argc, char *const argv[])
{
    int retcode;
    Jim_Interp *interp;
    char *const orig_argv0 = argv[0];

    /* Parse initial arguments before interpreter is started */
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("%d.%d\n", JIM_VERSION / 100, JIM_VERSION % 100);
        return 0;
    }
    else if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
        return 0;
    }

    /* Create and initialize the interpreter */
    interp = Jim_CreateInterp();
    Jim_RegisterCoreCommands(interp);

    /* Register static extensions */
    if (Jim_InitStaticExtensions(interp) != JIM_OK) {
        JimPrintErrorMessage(interp);
    }

    Jim_SetVariableStrWithStr(interp, "jim::argv0", orig_argv0);
    Jim_SetVariableStrWithStr(interp, JIM_INTERACTIVE, argc == 1 ? "1" : "0");
    retcode = Jim_initjimshInit(interp);

    if (argc == 1) {
        /* Executable name is the only argument - start interactive prompt */
        if (retcode == JIM_ERR) {
            JimPrintErrorMessage(interp);
        }
        if (retcode != JIM_EXIT) {
            JimSetArgv(interp, 0, NULL);
            retcode = Jim_InteractivePrompt(interp);
        }
    }
    else {
        /* Additional arguments - interpret them */
        if (argc > 2 && strcmp(argv[1], "-e") == 0) {
            /* Evaluate code in subsequent argument */
            JimSetArgv(interp, argc - 3, argv + 3);
            retcode = Jim_Eval(interp, argv[2]);
            if (retcode != JIM_ERR) {
                printf("%s\n", Jim_String(Jim_GetResult(interp)));
            }
        }
        else {
            Jim_SetVariableStr(interp, "argv0", Jim_NewStringObj(interp, argv[1], -1));
            JimSetArgv(interp, argc - 2, argv + 2);
            if (strcmp(argv[1], "-") == 0) {
                retcode = Jim_Eval(interp, "eval [info source [stdin read] stdin 1]");
            } else {
                retcode = Jim_EvalFile(interp, argv[1]);
            }
        }
        if (retcode == JIM_ERR) {
            JimPrintErrorMessage(interp);
        }
    }
    if (retcode == JIM_EXIT) {
        retcode = Jim_GetExitCode(interp);
    }
    else if (retcode == JIM_ERR) {
        retcode = 1;
    }
    else {
        retcode = 0;
    }
    Jim_FreeInterp(interp);
    return retcode;
}
