/* Jim - Readline bindings for Jim
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 *
 * $Id: jim-readline.c,v 1.1 2005/04/05 11:51:17 antirez Exp $
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

#define JIM_EXTENSION
#include "jim.h"

#include <readline/readline.h>
#include <readline/history.h>

static int JimRlReadlineCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    char *line;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "prompt");
        return JIM_ERR;
    }
    line = readline(Jim_GetString(argv[1], NULL));
    Jim_SetResult(interp, Jim_NewStringObj(interp, line, -1));
    return JIM_OK;
}

static int JimRlAddHistoryCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "string");
        return JIM_ERR;
    }
    add_history(Jim_GetString(argv[1], NULL));
    return JIM_OK;
}

int Jim_OnLoad(Jim_Interp *interp)
{
    Jim_InitExtension(interp);
    if (Jim_PackageProvide(interp, "readline", "1.0", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    Jim_CreateCommand(interp, "readline.readline", JimRlReadlineCommand, NULL,
            NULL);
    Jim_CreateCommand(interp, "readline.addhistory", JimRlAddHistoryCommand,
            NULL, NULL);
    return JIM_OK;
}
