#include <assert.h>

#include "jim.h"
#include "jimautoconf.h"
#include "jim-subcmd.h"

static void JimInterpDelProc(Jim_Interp *interp, void *privData)
{
    Jim_FreeInterp((Jim_Interp *)privData);
}

/* Everything passing between interpreters must be converted to a string */
static Jim_Obj *JimInterpCopyObj(Jim_Interp *target, Jim_Obj *obj)
{
    const char *rep;
    int len;

    rep = Jim_GetString(obj, &len);
    return Jim_NewStringObj(target, rep, len);
}

#define JimInterpCopyResult(to, from) Jim_SetResult((to), JimInterpCopyObj((to), Jim_GetResult((from))))

static int interp_cmd_eval(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int ret;
    Jim_Interp *child = Jim_CmdPrivData(interp);
    Jim_Obj *scriptObj;
    Jim_Obj *targetScriptObj;

    scriptObj = Jim_ConcatObj(interp, argc, argv);
    targetScriptObj = JimInterpCopyObj(child, scriptObj);
    Jim_FreeNewObj(interp, scriptObj);

    Jim_IncrRefCount(targetScriptObj);
    ret = Jim_EvalObj(child, targetScriptObj);
    Jim_DecrRefCount(child, targetScriptObj);

    JimInterpCopyResult(interp, child);
    return ret;
}

static int interp_cmd_delete(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_DeleteCommand(interp, Jim_String(argv[0]));
}

static void JimInterpDelAlias(Jim_Interp *interp, void *privData)
{
    Jim_Interp *parent = Jim_GetAssocData(interp, "interp.parent");
    Jim_DecrRefCount(parent, (Jim_Obj *)privData);
}

static int JimInterpAliasProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i, ret;
    Jim_Interp *parent = Jim_GetAssocData(interp, "interp.parent");
    Jim_Obj *targetPrefixObj = Jim_CmdPrivData(interp);
    Jim_Obj *targetScriptObj;

    assert(parent);

    /* Build the complete command */
    targetScriptObj = Jim_DuplicateObj(parent, targetPrefixObj);
    for (i = 1; i < argc; i++) {
        Jim_ListAppendElement(parent, targetScriptObj,
            JimInterpCopyObj(parent, argv[i]));
    }

    Jim_IncrRefCount(targetScriptObj);
    ret = Jim_EvalObj(parent, targetScriptObj);
    Jim_DecrRefCount(parent, targetScriptObj);

    JimInterpCopyResult(interp, parent);
    return ret;
}

static int interp_cmd_alias(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Interp *child = Jim_CmdPrivData(interp);
    Jim_Obj *aliasPrefixList;

    /* The prefix list will be held inside the child, but it still belongs
     * to the parent!
     */

    aliasPrefixList = Jim_NewListObj(interp, argv + 1, argc - 1);
    Jim_IncrRefCount(aliasPrefixList);

    Jim_CreateCommand(child, Jim_String(argv[0]), JimInterpAliasProc, aliasPrefixList, JimInterpDelAlias);
    return JIM_OK;
}

static const jim_subcmd_type interp_command_table[] = {
    {   "eval",
        "script ...",
        interp_cmd_eval,
        1,
        -1,
        /* Description: Concat the args and evaluate the script in the interpreter */
    },
    {   "delete",
        NULL,
        interp_cmd_delete,
        0,
        0,
        JIM_MODFLAG_FULLARGV,
        /* Description: Delete this interpreter */
    },
    {   "alias",
        "childcmd parentcmd ?arg ...?",
        interp_cmd_alias,
        2,
        -1,
        /* Description: Create an alias which refers to a script in the parent interpreter */
    },
    { NULL }
};

static int JimInterpSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, interp_command_table, argc, argv), argc, argv);
}

static void JimInterpCopyVariable(Jim_Interp *target, Jim_Interp *source, const char *var, const char *default_value)
{
    Jim_Obj *value = Jim_GetGlobalVariableStr(source, var, JIM_NONE);
    const char *str;

    str = value ? Jim_String(value) : default_value;
    if (str) {
        Jim_SetGlobalVariableStr(target, var, Jim_NewStringObj(target, str, -1));
    }
}

/**
 * [interp] creates a new interpreter.
 */
static int JimInterpCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Interp *child;
    char buf[34];

    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }

    /* Create the interpreter command */
    child = Jim_CreateInterp();
    Jim_RegisterCoreCommands(child);
    Jim_InitStaticExtensions(child);

    /* Copy some core variables to the new interpreter */
    JimInterpCopyVariable(child, interp, "argv", NULL);
    JimInterpCopyVariable(child, interp, "argc", NULL);
    JimInterpCopyVariable(child, interp, "argv0", NULL);
    JimInterpCopyVariable(child, interp, "jim::argv0", NULL);
    JimInterpCopyVariable(child, interp, "jim::exe", NULL);

    /* Allow the child interpreter to find the parent */
    Jim_SetAssocData(child, "interp.parent", NULL, interp);

    snprintf(buf, sizeof(buf), "interp.handle%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimInterpSubCmdProc, child, JimInterpDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));
    return JIM_OK;
}

int Jim_interpInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "interp", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "interp", JimInterpCommand, NULL, NULL);

    return JIM_OK;
}
