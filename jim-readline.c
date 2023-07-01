/*
 * Jim - Readline bindings for Jim
 *
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
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

#include <jim.h>

#include <stdio.h>

#include <readline/readline.h>
#include <readline/history.h>

static int JimRlReadlineCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char *line;

    line = readline(Jim_String(argv[1]));
    if (!line) {
        return JIM_EXIT;
    }
    Jim_SetResult(interp, Jim_NewStringObj(interp, line, -1));
    return JIM_OK;
}

static int JimRlAddHistoryCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    add_history(Jim_String(argv[1]));
    return JIM_OK;
}

int Jim_readlineInit(Jim_Interp *interp)
{
    Jim_PackageProvideCheck(interp, "readline");
    Jim_RegisterSimpleCmd(interp, "readline.readline", "prompt", 1, 1, JimRlReadlineCommand);
    Jim_RegisterSimpleCmd(interp, "readline.addhistory", "string", 1, 1, JimRlAddHistoryCommand);
    return JIM_OK;
}
