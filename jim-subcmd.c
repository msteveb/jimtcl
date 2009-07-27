#include <stdio.h>
#include <string.h>

#include "jim-subcmd.h"

/**
 * Implements the common 'commands' subcommand
 */
static int subcmd_null(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    /* Nothing to do, since the result has already been created */
    return JIM_OK;
}

/**
 * Do-nothing command to support -commands and -usage
 */
static const jim_subcmd_type dummy_subcmd = {
    .cmd = "dummy",
    .function = subcmd_null,
    .flags = JIM_MODFLAG_HIDDEN,
};

static void add_commands(Jim_Interp *interp, const jim_subcmd_type *ct, const char *sep)
{
    const char *s = "";

    for (; ct->cmd; ct++) {
        if (!(ct->flags & JIM_MODFLAG_HIDDEN)) {
            Jim_AppendStrings(interp, Jim_GetResult(interp), s, ct->cmd, 0);
            s = sep;
        }
    }
}

static void bad_subcmd(Jim_Interp *interp, const jim_subcmd_type *command_table, const char *type, int argc, Jim_Obj *const *argv)
{
    Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
    Jim_AppendStrings(interp, Jim_GetResult(interp), Jim_GetString(argv[0], NULL), ", ", type, " command \"", Jim_GetString(argv[1], NULL), "\": should be ", NULL);
    add_commands(interp, command_table, ", ");
}

static void show_cmd_usage(Jim_Interp *interp, const jim_subcmd_type *command_table, int argc, Jim_Obj *const *argv)
{
    Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
    Jim_AppendStrings(interp, Jim_GetResult(interp), "Usage: \"", Jim_GetString(argv[0], NULL), " command ... \", where command is one of: ", NULL);
    add_commands(interp, command_table, ", ");
}

static void add_subcmd_usage(Jim_Interp *interp, const jim_subcmd_type *ct, int argc, Jim_Obj *const *argv)
{
    Jim_AppendStrings(interp, Jim_GetResult(interp), Jim_GetString(argv[0], NULL), " ", ct->cmd, NULL);

    if (ct->args && *ct->args) {
        Jim_AppendStrings(interp, Jim_GetResult(interp), " ", ct->args, NULL);
    }
}

static void show_full_usage(Jim_Interp *interp, const jim_subcmd_type *ct, int argc, Jim_Obj *const *argv)
{
    Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
    for (; ct->cmd; ct++) {
        if (!(ct->flags & JIM_MODFLAG_HIDDEN)) {
            add_subcmd_usage(interp, ct, argc, argv);
            if (ct->description) {
                Jim_AppendStrings(interp, Jim_GetResult(interp), "\n\n    ", ct->description, 0);
            }
            Jim_AppendStrings(interp, Jim_GetResult(interp), "\n\n", 0);
        }
    }
}

const jim_subcmd_type *
Jim_ParseSubCmd(Jim_Interp *interp, const jim_subcmd_type *command_table, int argc, Jim_Obj *const *argv)
{
    const jim_subcmd_type *ct;
    const jim_subcmd_type *partial = 0;
    int cmdlen;
    Jim_Obj *cmd;
    const char *cmdstr;
    const char *cmdname;
    int help = 0;

    cmdname = Jim_GetString(argv[0], NULL);

    if (argc < 2) {
        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        Jim_AppendStrings(interp, Jim_GetResult(interp), "wrong # args: should be \"", cmdname, " command ...\"\n", 0);
        Jim_AppendStrings(interp, Jim_GetResult(interp), "Use \"", cmdname, " -help\" or \"", cmdname, " -help command\" for help", 0);
        return 0;
    }

    cmd = argv[1];

    if (argc == 2 && Jim_CompareStringImmediate(interp, cmd, "-usage")) {
        /* Show full usage */
        show_full_usage(interp, command_table, argc, argv);
        return &dummy_subcmd;
    }

    /* Check for the help command */
    if (Jim_CompareStringImmediate(interp, cmd, "-help")) {
        if (argc == 2) {
            /* Usage for the command, not the subcommand */
            show_cmd_usage(interp, command_table, argc, argv);
            return 0;
        }
        help = 1;

        /* Skip the 'help' command */
        cmd = argv[2];
    }

    /* Check for special builtin '-commands' command first */
    if (Jim_CompareStringImmediate(interp, cmd, "-commands")) {
        /* Build the result here */
        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        add_commands(interp, command_table, " ");
        return &dummy_subcmd;
    }

    cmdstr = Jim_GetString(cmd, &cmdlen);

    for (ct = command_table; ct->cmd; ct++) {
        if (Jim_CompareStringImmediate(interp, cmd, ct->cmd)) {
            /* Found an exact match */
            break;
        }
        if (strncmp(cmdstr, ct->cmd, cmdlen) == 0) {
            if (partial) {
                /* Ambiguous */
                bad_subcmd(interp, command_table, "ambiguous", argc, argv);
                return 0;
            }
            partial = ct;
        }
        continue;
    }

    /* If we had an unambiguous partial match */
    if (partial && !ct->cmd) {
        ct = partial;
    }

    if (!ct->cmd) {
        /* No matching command */
        bad_subcmd(interp, command_table, "unknown", argc, argv);
        return 0;
    }

    if (help) {
        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        add_subcmd_usage(interp, ct, argc, argv);
        if (ct->description) {
            Jim_AppendStrings(interp, Jim_GetResult(interp), "\n\n", ct->description, 0);
        }
        return 0;
    }

    /* Check the number of args */
    if (argc - 2 < ct->minargs || (ct->maxargs >= 0 && argc - 2> ct->maxargs)) {
        Jim_SetResultString(interp, "wrong # args: should be \"", -1);
        add_subcmd_usage(interp, ct, argc, argv);
        Jim_AppendStrings(interp, Jim_GetResult(interp), "\"", NULL);

        return 0;
    }

    /* Good command */
    return ct;
}

int Jim_CallSubCmd(Jim_Interp *interp, const jim_subcmd_type *ct, int argc, Jim_Obj *const *argv)
{
    int ret = JIM_ERR;

    if (ct) {
        ret = ct->function(interp, argc - 2, argv + 2);
        if (ret < 0) {
            Jim_SetResultString(interp, "wrong # args: should be \"", -1);
            add_subcmd_usage(interp, ct, argc, argv);
            Jim_AppendStrings(interp, Jim_GetResult(interp), "\"", NULL);
            ret = JIM_ERR;
        }
    }
    return ret;
}

int Jim_SubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const jim_subcmd_type *ct = Jim_ParseSubCmd(interp, (const jim_subcmd_type *)Jim_CmdPrivData(interp), argc, argv);

    return Jim_CallSubCmd(interp, ct, argc, argv);
}
