/* Jimsh - An interactive shell for Jim
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 *
 * $Id: jimsh.c,v 1.4 2005/04/05 11:51:18 antirez Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JIM_EMBEDDED
#include "jim.h"

void JimLoadJimRc(Jim_Interp *interp)
{
    const char *home;
    char buf [JIM_PATH_LEN+1];
    const char *names[] = {".jimrc", "jimrc.tcl", NULL};
    int i;
    FILE *fp;

    if ((home = getenv("HOME")) == NULL) return;
    for (i = 0; names[i] != NULL; i++) {
        if (strlen(home)+strlen(names[i])+1 > JIM_PATH_LEN) continue;
        sprintf(buf, "%s/%s", home, names[i]);
        if ((fp = fopen(buf, "r")) != NULL) {
            fclose(fp);
            if (Jim_EvalFile(interp, buf) != JIM_OK) {
                Jim_PrintErrorMessage(interp);
            }
            return;
        }
    }
}

int main(int argc, char *const argv[])
{
    int retcode, n;
    Jim_Interp *interp;
    Jim_Obj *listObj;

    Jim_InitEmbedded(); /* This is the first function embedders should call. */

    /* Create and initialize the interpreter */
    interp = Jim_CreateInterp();
    Jim_RegisterCoreCommands(interp);

    listObj = Jim_NewListObj(interp, NULL, 0);
    for (n = 2; n < argc; n++) {
        Jim_Obj *obj = Jim_NewStringObj(interp, argv[n], -1);
        Jim_ListAppendElement(interp, listObj, obj);
    }

    Jim_SetVariableStr(interp, "argv0", Jim_NewStringObj(interp, argv[0], -1));
    Jim_SetVariableStr(interp, "argv", listObj);
    
    if (argc == 1) {
        Jim_SetVariableStrWithStr(interp, "jim_interactive", "1");
        JimLoadJimRc(interp);
        retcode = Jim_InteractivePrompt(interp);
    } else {
        Jim_SetVariableStrWithStr(interp, "jim_interactive", "0");
        if ((retcode = Jim_EvalFile(interp, argv[1])) == JIM_ERR) {
            Jim_PrintErrorMessage(interp);
        }
    }
    Jim_FreeInterp(interp);
    return retcode;
}
