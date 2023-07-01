/* Provides a common approach to implementing Tcl commands
 * which implement subcommands
 */
#ifndef JIM_SUBCMD_H
#define JIM_SUBCMD_H

#include <jim.h>

#ifdef __cplusplus
extern "C" {
#endif


#define JIM_MODFLAG_HIDDEN   0x0001		/* Don't show the subcommand in usage or commands */
#define JIM_MODFLAG_FULLARGV 0x0002		/* Subcmd proc gets called with full argv */
#define JIM_MODFLAG_NOTAINT  0x0004		/* May not be called with tainted data */

#define JIM_SUBCMD_BADARGS -1
#define JIM_SUBCMD_TAINTED -2

/* Custom flags start at 0x0100 */

/**
 * Returns JIM_OK if OK, JIM_ERR (etc.) on error, break, continue, etc.
 * Returns -1 if invalid args.
 */
typedef int jim_subcmd_function(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

typedef struct {
	const char *cmd;				/* Name of the (sub)command */
	const char *args;				/* Textual description of allowed args */
	jim_subcmd_function *function;	/* Function implementing the subcommand */
	short minargs;					/* Minimum required arguments */
	short maxargs;					/* Maximum allowed arguments or -1 if no limit */
	unsigned short flags;			/* JIM_MODFLAG_... plus custom flags */
} jim_subcmd_type;

/* This makes it easy to define a jim_subcmd_type array with function=NULL
 * see jim-namespace.c for an example
 */
#define JIM_DEF_SUBCMD(name, args, minargs, maxargs) { name, args, NULL, minargs, maxargs }
#define JIM_DEF_SUBCMD_HIDDEN(name, args, minargs, maxargs) { name, args, NULL, minargs, maxargs, JIM_MODFLAG_HIDDEN }

/**
 * Looks up the appropriate subcommand in the given command table and return
 * the command function which implements the subcommand.
 * NULL will be returned and an appropriate error will be set if the subcommand or
 * arguments are invalid.
 *
 * Typical usage is:
 *  {
 *    const jim_subcmd_type *ct = Jim_ParseSubCmd(interp, command_table, argc, argv);
 *
 *    return Jim_CallSubCmd(interp, ct, argc, argv);
 *  }
 *
 */
const jim_subcmd_type *
Jim_ParseSubCmd(Jim_Interp *interp, const jim_subcmd_type *command_table, int argc, Jim_Obj *const *argv);

/**
 * Parses the args against the given command table and executes the subcommand if found
 * or sets an appropriate error if the subcommand or arguments is invalid.
 *
 * Typically used via Jim_RegisterCmd()
 *
 * e.g. Jim_RegisterSubCmd(interp, "mycmd", command_table, NULL);
 */
int Jim_SubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

/**
 * Invokes the given subcmd with the given args as returned
 * by Jim_ParseSubCmd()
 *
 * If ct is NULL, returns JIM_ERR, leaving any message.
 * Otherwise invokes ct->function
 *
 * If ct->function returns -1, sets an error message and returns JIM_ERR.
 * Otherwise returns the result of ct->function.
 */
int Jim_CallSubCmd(Jim_Interp *interp, const jim_subcmd_type *ct, int argc, Jim_Obj *const *argv);

/**
 * Sets an error result indicating the usage of the subcmd 'ct'.
 * Typically this will be called with the result of Jim_ParseSubCmd() after
 * additional checks if the args are wrong.
 */
void Jim_SubCmdArgError(Jim_Interp *interp, const jim_subcmd_type *ct, Jim_Obj *subcmd);

/**
 * Convenience wrapper around Jim_RegisterCmd() to register a subcmd command.
 */
Jim_Cmd *Jim_RegisterSubCmd(Jim_Interp *interp, const char *cmdname,
	const jim_subcmd_type *command_table, Jim_DelCmdProc *delProc);

#ifdef __cplusplus
}
#endif

#endif
