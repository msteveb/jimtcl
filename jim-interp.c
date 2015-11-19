/*
 * Jim - child interpreter module
 *
 * Copyright 2015 Dima Krasner <dima@dimakrasner.com>
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
#include <jim-subcmd.h>

static void Jim_InterpDelProc(Jim_Interp *interp, void *privData)
{
    JIM_NOTUSED(interp);
    Jim_FreeInterp((Jim_Interp *)privData);
}

static int Jim_InterpDelete(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_DeleteCommand(interp, Jim_String(argv[0]));
}

static int Jim_InterpEval(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Interp *child = Jim_CmdPrivData(interp);
    int ret;

    Jim_SetEmptyResult(child);
    ret = Jim_EvalObj(child, argv[2]);
    Jim_SetResult(interp, Jim_DuplicateObj(interp, Jim_GetResult(child)));
    return ret;
}

static const jim_subcmd_type interp_subcmd_table[] = {
    {   "delete",
        NULL,
        Jim_InterpDelete,
        0,
        0,
        JIM_MODFLAG_FULLARGV
        /* Description: Deletes an interpreter */
    },
    {   "eval",
        "arg",
        Jim_InterpEval,
        1,
        1,
        JIM_MODFLAG_FULLARGV
        /* Description: Evaluates a Tcl expression inside an interpreter */
    },
    { NULL }
};

static int JimInterpHandler(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, interp_subcmd_table, argc, argv), argc, argv);
}

static int Jim_InterpCreate(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char buf[32];
    Jim_Interp *child;

    child = Jim_CreateInterp();
    if (!child) {
        return JIM_ERR;
    }

    Jim_RegisterCoreCommands(child);
    Jim_InitStaticExtensions(child);

    sprintf(buf, "interp%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimInterpHandler, child, Jim_InterpDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return JIM_OK;
}

static const jim_subcmd_type interp_command_table[] = {
    {   "create",
        NULL,
        Jim_InterpCreate,
        0,
        0,
        /* Description: Creates an interpreter */
    },
    { NULL }
};


static int Jim_InterpCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, interp_command_table, argc, argv), argc, argv);
}

int Jim_interpInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "interp", "1.0", JIM_ERRMSG)) {
        return JIM_ERR;
    }

    Jim_CreateCommand(interp, "interp", Jim_InterpCmd, 0, 0);

    return JIM_OK;
}
