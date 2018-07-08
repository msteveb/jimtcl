/*
 * (c) 2008 Steve Bennett <steveb@workware.net.au>
 *
 * Implements the exec command for Jim
 *
 * Based on code originally from Tcl 6.7 by John Ousterhout.
 * From that code:
 *
 * The Tcl_Fork and Tcl_WaitPids procedures are based on code
 * contributed by Karl Lehenbauer, Mark Diekhans and Peter
 * da Silva.
 *
 * Copyright 1987-1991 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <string.h>
#include <ctype.h>

#include "jimautoconf.h"
#include <jim.h>

#if (!defined(HAVE_VFORK) || !defined(HAVE_WAITPID)) && !defined(__MINGW32__)
/* Poor man's implementation of exec with system()
 * The system() call *may* do command line redirection, etc.
 * The standard output is not available.
 * Can't redirect filehandles.
 */
static int Jim_ExecCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *cmdlineObj = Jim_NewEmptyStringObj(interp);
    int i, j;
    int rc;

    /* Create a quoted command line */
    for (i = 1; i < argc; i++) {
        int len;
        const char *arg = Jim_GetString(argv[i], &len);

        if (i > 1) {
            Jim_AppendString(interp, cmdlineObj, " ", 1);
        }
        if (strpbrk(arg, "\\\" ") == NULL) {
            /* No quoting required */
            Jim_AppendString(interp, cmdlineObj, arg, len);
            continue;
        }

        Jim_AppendString(interp, cmdlineObj, "\"", 1);
        for (j = 0; j < len; j++) {
            if (arg[j] == '\\' || arg[j] == '"') {
                Jim_AppendString(interp, cmdlineObj, "\\", 1);
            }
            Jim_AppendString(interp, cmdlineObj, &arg[j], 1);
        }
        Jim_AppendString(interp, cmdlineObj, "\"", 1);
    }
    rc = system(Jim_String(cmdlineObj));

    Jim_FreeNewObj(interp, cmdlineObj);

    if (rc) {
        Jim_Obj *errorCode = Jim_NewListObj(interp, NULL, 0);
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "CHILDSTATUS", -1));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, 0));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, rc));
        Jim_SetGlobalVariableStr(interp, "errorCode", errorCode);
        return JIM_ERR;
    }

    return JIM_OK;
}

int Jim_execInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "exec", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "exec", Jim_ExecCmd, NULL, NULL);
    return JIM_OK;
}
#else
/* Full exec implementation for unix and mingw */

#include <errno.h>
#include <signal.h>
#include "jim-signal.h"
#include "jimiocompat.h"
#include <sys/stat.h>

struct WaitInfoTable;

static char **JimOriginalEnviron(void);
static char **JimSaveEnv(char **env);
static void JimRestoreEnv(char **env);
static int JimCreatePipeline(Jim_Interp *interp, int argc, Jim_Obj *const *argv,
    pidtype **pidArrayPtr, int *inPipePtr, int *outPipePtr, int *errFilePtr);
static void JimDetachPids(struct WaitInfoTable *table, int numPids, const pidtype *pidPtr);
static int JimCleanupChildren(Jim_Interp *interp, int numPids, pidtype *pidPtr, Jim_Obj *errStrObj);
static int Jim_WaitCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

#if defined(__MINGW32__)
static pidtype JimStartWinProcess(Jim_Interp *interp, char **argv, char **env, int inputId, int outputId, int errorId);
#endif

/*
 * If the last character of 'objPtr' is a newline, then remove
 * the newline character.
 */
static void Jim_RemoveTrailingNewline(Jim_Obj *objPtr)
{
    int len;
    const char *s = Jim_GetString(objPtr, &len);

    if (len > 0 && s[len - 1] == '\n') {
        objPtr->length--;
        objPtr->bytes[objPtr->length] = '\0';
    }
}

/**
 * Read from 'fd', append the data to strObj and close 'fd'.
 * Returns 1 if data was added, 0 if not, or -1 on error.
 */
static int JimAppendStreamToString(Jim_Interp *interp, int fd, Jim_Obj *strObj)
{
    char buf[256];
    FILE *fh = fdopen(fd, "r");
    int ret = 0;

    if (fh == NULL) {
        return -1;
    }

    while (1) {
        int retval = fread(buf, 1, sizeof(buf), fh);
        if (retval > 0) {
            ret = 1;
            Jim_AppendString(interp, strObj, buf, retval);
        }
        if (retval != sizeof(buf)) {
            break;
        }
    }
    fclose(fh);
    return ret;
}

/**
 * Builds the environment array from $::env
 *
 * If $::env is not set, simply returns environ.
 *
 * Otherwise allocates the environ array from the contents of $::env
 *
 * If the exec fails, memory can be freed via JimFreeEnv()
 */
static char **JimBuildEnv(Jim_Interp *interp)
{
    int i;
    int size;
    int num;
    int n;
    char **envptr;
    char *envdata;

    Jim_Obj *objPtr = Jim_GetGlobalVariableStr(interp, "env", JIM_NONE);

    if (!objPtr) {
        return JimOriginalEnviron();
    }

    /* We build the array as a single block consisting of the pointers followed by
     * the strings. This has the advantage of being easy to allocate/free and being
     * compatible with both unix and windows
     */

    /* Calculate the required size */
    num = Jim_ListLength(interp, objPtr);
    if (num % 2) {
        /* Silently drop the last element if not a valid dictionary */
        num--;
    }
    /* We need one \0 and one equal sign for each element.
     * A list has at least one space for each element except the first.
     * We need one extra char for the extra null terminator and one for the equal sign.
     */
    size = Jim_Length(objPtr) + 2;

    envptr = Jim_Alloc(sizeof(*envptr) * (num / 2 + 1) + size);
    envdata = (char *)&envptr[num / 2 + 1];

    n = 0;
    for (i = 0; i < num; i += 2) {
        const char *s1, *s2;
        Jim_Obj *elemObj;

        Jim_ListIndex(interp, objPtr, i, &elemObj, JIM_NONE);
        s1 = Jim_String(elemObj);
        Jim_ListIndex(interp, objPtr, i + 1, &elemObj, JIM_NONE);
        s2 = Jim_String(elemObj);

        envptr[n] = envdata;
        envdata += sprintf(envdata, "%s=%s", s1, s2);
        envdata++;
        n++;
    }
    envptr[n] = NULL;
    *envdata = 0;

    return envptr;
}

/**
 * Frees the environment allocated by JimBuildEnv()
 *
 * Must pass original_environ.
 */
static void JimFreeEnv(char **env, char **original_environ)
{
    if (env != original_environ) {
        Jim_Free(env);
    }
}

static Jim_Obj *JimMakeErrorCode(Jim_Interp *interp, pidtype pid, int waitStatus, Jim_Obj *errStrObj)
{
    Jim_Obj *errorCode = Jim_NewListObj(interp, NULL, 0);

    if (pid == JIM_BAD_PID || pid == JIM_NO_PID) {
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "NONE", -1));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, (long)pid));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, -1));
    }
    else if (WIFEXITED(waitStatus)) {
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "CHILDSTATUS", -1));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, (long)pid));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, WEXITSTATUS(waitStatus)));
    }
    else {
        const char *type;
        const char *action;
        const char *signame;

        if (WIFSIGNALED(waitStatus)) {
            type = "CHILDKILLED";
            action = "killed";
            signame = Jim_SignalId(WTERMSIG(waitStatus));
        }
        else {
            type = "CHILDSUSP";
            action = "suspended";
            signame = "none";
        }

        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, type, -1));

        if (errStrObj) {
            /* Append the message to 'errStrObj' with a newline.
             * The last newline will be stripped later
             */
            Jim_AppendStrings(interp, errStrObj, "child ", action, " by signal ", Jim_SignalId(WTERMSIG(waitStatus)), "\n", NULL);
        }

        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, (long)pid));
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, signame, -1));
    }
    return errorCode;
}

/*
 * Create and store an appropriate value for the global variable $::errorCode
 * Based on pid and waitStatus.
 *
 * Returns JIM_OK for a normal exit with code 0, otherwise returns JIM_ERR.
 *
 * Note that $::errorCode is left unchanged for a normal exit.
 * Details of any abnormal exit is appended to the errStrObj, unless it is NULL.
 */
static int JimCheckWaitStatus(Jim_Interp *interp, pidtype pid, int waitStatus, Jim_Obj *errStrObj)
{
    if (WIFEXITED(waitStatus) && WEXITSTATUS(waitStatus) == 0) {
        return JIM_OK;
    }
    Jim_SetGlobalVariableStr(interp, "errorCode", JimMakeErrorCode(interp, pid, waitStatus, errStrObj));

    return JIM_ERR;
}

/*
 * Data structures of the following type are used by exec and
 * wait to keep track of child processes.
 */

struct WaitInfo
{
    pidtype pid;                /* Process id of child. */
    int status;                 /* Status returned when child exited or suspended. */
    int flags;                  /* Various flag bits;  see below for definitions. */
};

/* This table is shared by exec and wait */
struct WaitInfoTable {
    struct WaitInfo *info;      /* Table of outstanding processes */
    int size;                   /* Size of the allocated table */
    int used;                   /* Number of entries in use */
    int refcount;               /* Free the table once the refcount drops to 0 */
};

/*
 * Flag bits in WaitInfo structures:
 *
 * WI_DETACHED -        Non-zero means no-one cares about the
 *                      process anymore.  Ignore it until it
 *                      exits, then forget about it.
 */

#define WI_DETACHED 2

#define WAIT_TABLE_GROW_BY 4

static void JimFreeWaitInfoTable(struct Jim_Interp *interp, void *privData)
{
    struct WaitInfoTable *table = privData;

    if (--table->refcount == 0) {
        Jim_Free(table->info);
        Jim_Free(table);
    }
}

static struct WaitInfoTable *JimAllocWaitInfoTable(void)
{
    struct WaitInfoTable *table = Jim_Alloc(sizeof(*table));
    table->info = NULL;
    table->size = table->used = 0;
    table->refcount = 1;

    return table;
}

/**
 * Removes the given pid from the wait table.
 *
 * Returns 0 if OK or -1 if not found.
 */
static int JimWaitRemove(struct WaitInfoTable *table, pidtype pid)
{
    int i;

    /* Find it in the table */
    for (i = 0; i < table->used; i++) {
        if (pid == table->info[i].pid) {
            if (i != table->used - 1) {
                table->info[i] = table->info[table->used - 1];
            }
            table->used--;
            return 0;
        }
    }
    return -1;
}

/*
 * The main [exec] command
 */
static int Jim_ExecCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int outputId;    /* File id for output pipe. -1 means command overrode. */
    int errorId;     /* File id for temporary file containing error output. */
    pidtype *pidPtr;
    int numPids, result;
    int child_siginfo = 1;
    Jim_Obj *childErrObj;
    Jim_Obj *errStrObj;
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);

    /*
     * See if the command is to be run in the background; if so, create
     * the command, detach it, and return.
     */
    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[argc - 1], "&")) {
        Jim_Obj *listObj;
        int i;

        argc--;
        numPids = JimCreatePipeline(interp, argc - 1, argv + 1, &pidPtr, NULL, NULL, NULL);
        if (numPids < 0) {
            return JIM_ERR;
        }
        /* The return value is a list of the pids */
        listObj = Jim_NewListObj(interp, NULL, 0);
        for (i = 0; i < numPids; i++) {
            Jim_ListAppendElement(interp, listObj, Jim_NewIntObj(interp, (long)pidPtr[i]));
        }
        Jim_SetResult(interp, listObj);
        JimDetachPids(table, numPids, pidPtr);
        Jim_Free(pidPtr);
        return JIM_OK;
    }

    /*
     * Create the command's pipeline.
     */
    numPids =
        JimCreatePipeline(interp, argc - 1, argv + 1, &pidPtr, NULL, &outputId, &errorId);

    if (numPids < 0) {
        return JIM_ERR;
    }

    result = JIM_OK;

    errStrObj = Jim_NewStringObj(interp, "", 0);

    /* Read from the output pipe until EOF */
    if (outputId != -1) {
        if (JimAppendStreamToString(interp, outputId, errStrObj) < 0) {
            result = JIM_ERR;
            Jim_SetResultErrno(interp, "error reading from output pipe");
        }
    }

    /* Now wait for children to finish. Any abnormal results are appended to childErrObj */
    childErrObj = Jim_NewStringObj(interp, "", 0);
    Jim_IncrRefCount(childErrObj);

    if (JimCleanupChildren(interp, numPids, pidPtr, childErrObj) != JIM_OK) {
        result = JIM_ERR;
    }

    /*
     * Read the child's error output (if any) and put it into the result.
     *
     * Note that unlike Tcl, the presence of stderr output does not cause
     * exec to return an error.
     */
    if (errorId != -1) {
        int ret;
        lseek(errorId, 0, SEEK_SET);
        ret = JimAppendStreamToString(interp, errorId, errStrObj);
        if (ret < 0) {
            Jim_SetResultErrno(interp, "error reading from error pipe");
            result = JIM_ERR;
        }
        else if (ret > 0) {
            /* Got some error output, so discard the abnormal info string */
            child_siginfo = 0;
        }
    }

    if (child_siginfo) {
        /* Append the child siginfo to the result */
        Jim_AppendObj(interp, errStrObj, childErrObj);
    }
    Jim_DecrRefCount(interp, childErrObj);

    /* Finally remove any trailing newline from the result */
    Jim_RemoveTrailingNewline(errStrObj);

    /* Set this as the result */
    Jim_SetResult(interp, errStrObj);

    return result;
}

/**
 * Does waitpid() on the given pid, and then removes the
 * entry from the wait table.
 *
 * Returns the pid if OK and updates *statusPtr with the status,
 * or JIM_BAD_PID if the pid was not in the table.
 */
static pidtype JimWaitForProcess(struct WaitInfoTable *table, pidtype pid, int *statusPtr)
{
    if (JimWaitRemove(table, pid) == 0) {
         /* wait for it */
         waitpid(pid, statusPtr, 0);
         return pid;
    }

    /* Not found */
    return JIM_BAD_PID;
}

/**
 * Indicates that one or more child processes have been placed in
 * background and are no longer cared about.
 * These children can be cleaned up with JimReapDetachedPids().
 */
static void JimDetachPids(struct WaitInfoTable *table, int numPids, const pidtype *pidPtr)
{
    int j;

    for (j = 0; j < numPids; j++) {
        /* Find it in the table */
        int i;
        for (i = 0; i < table->used; i++) {
            if (pidPtr[j] == table->info[i].pid) {
                table->info[i].flags |= WI_DETACHED;
                break;
            }
        }
    }
}

/* Use 'name getfd' to get the file descriptor associated with channel 'name'
 * Returns the file descriptor or -1 on error
 */
static int JimGetChannelFd(Jim_Interp *interp, const char *name)
{
    Jim_Obj *objv[2];

    objv[0] = Jim_NewStringObj(interp, name, -1);
    objv[1] = Jim_NewStringObj(interp, "getfd", -1);

    if (Jim_EvalObjVector(interp, 2, objv) == JIM_OK) {
        jim_wide fd;
        if (Jim_GetWide(interp, Jim_GetResult(interp), &fd) == JIM_OK) {
            return fd;
        }
    }
    return -1;
}

static void JimReapDetachedPids(struct WaitInfoTable *table)
{
    struct WaitInfo *waitPtr;
    int count;
    int dest;

    if (!table) {
        return;
    }

    waitPtr = table->info;
    dest = 0;
    for (count = table->used; count > 0; waitPtr++, count--) {
        if (waitPtr->flags & WI_DETACHED) {
            int status;
            pidtype pid = waitpid(waitPtr->pid, &status, WNOHANG);
            if (pid == waitPtr->pid) {
                /* Process has exited, so remove it from the table */
                table->used--;
                continue;
            }
        }
        if (waitPtr != &table->info[dest]) {
            table->info[dest] = *waitPtr;
        }
        dest++;
    }
}

/*
 * wait ?-nohang? ?pid?
 *
 * An interface to waitpid(2)
 *
 * Returns a 3 element list.
 *
 * If the process has not exited or doesn't exist, returns:
 *
 *   {NONE x x}
 *
 * If the process exited normally, returns:
 *
 *   {CHILDSTATUS <pid> <exit-status>}
 *
 * If the process terminated on a signal, returns:
 *
 *   {CHILDKILLED <pid> <signal>}
 *
 * Otherwise (core dump, stopped, continued, ...), returns:
 *
 *   {CHILDSUSP <pid> none}
 *
 * With no arguments, reaps any finished background processes started by exec ... &
 */
static int Jim_WaitCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);
    int nohang = 0;
    pidtype pid;
    long pidarg;
    int status;
    Jim_Obj *errCodeObj;

    /* With no arguments, reap detached children */
    if (argc == 1) {
        JimReapDetachedPids(table);
        return JIM_OK;
    }

    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[1], "-nohang")) {
        nohang = 1;
    }
    if (argc != nohang + 2) {
        Jim_WrongNumArgs(interp, 1, argv, "?-nohang? ?pid?");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, argv[nohang + 1], &pidarg) != JIM_OK) {
        return JIM_ERR;
    }

    pid = waitpid((pidtype)pidarg, &status, nohang ? WNOHANG : 0);

    errCodeObj = JimMakeErrorCode(interp, pid, status, NULL);

    if (pid != JIM_BAD_PID && (WIFEXITED(status) || WIFSIGNALED(status))) {
        /* The process has finished. Remove it from the wait table if it exists there */
        JimWaitRemove(table, pid);
    }
    Jim_SetResult(interp, errCodeObj);
    return JIM_OK;
}

static int Jim_PidCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, (jim_wide)getpid());
    return JIM_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * JimCreatePipeline --
 *
 *  Given an argc/argv array, instantiate a pipeline of processes
 *  as described by the argv.
 *
 * Results:
 *  The return value is a count of the number of new processes
 *  created, or -1 if an error occurred while creating the pipeline.
 *  *pidArrayPtr is filled in with the address of a dynamically
 *  allocated array giving the ids of all of the processes.  It
 *  is up to the caller to free this array when it isn't needed
 *  anymore.  If inPipePtr is non-NULL, *inPipePtr is filled in
 *  with the file id for the input pipe for the pipeline (if any):
 *  the caller must eventually close this file.  If outPipePtr
 *  isn't NULL, then *outPipePtr is filled in with the file id
 *  for the output pipe from the pipeline:  the caller must close
 *  this file.  If errFilePtr isn't NULL, then *errFilePtr is filled
 *  with a file id that may be used to read error output after the
 *  pipeline completes.
 *
 * Side effects:
 *  Processes and pipes are created.
 *
 *----------------------------------------------------------------------
 */
static int
JimCreatePipeline(Jim_Interp *interp, int argc, Jim_Obj *const *argv, pidtype **pidArrayPtr,
    int *inPipePtr, int *outPipePtr, int *errFilePtr)
{
    pidtype *pidPtr = NULL;         /* Points to malloc-ed array holding all
                                 * the pids of child processes. */
    int numPids = 0;            /* Actual number of processes that exist
                                 * at *pidPtr right now. */
    int cmdCount;               /* Count of number of distinct commands
                                 * found in argc/argv. */
    const char *input = NULL;   /* Describes input for pipeline, depending
                                 * on "inputFile".  NULL means take input
                                 * from stdin/pipe. */
    int input_len = 0;          /* Length of input, if relevant */

#define FILE_NAME   0           /* input/output: filename */
#define FILE_APPEND 1           /* output only:  filename, append */
#define FILE_HANDLE 2           /* input/output: filehandle */
#define FILE_TEXT   3           /* input only:   input is actual text */

    int inputFile = FILE_NAME;  /* 1 means input is name of input file.
                                 * 2 means input is filehandle name.
                                 * 0 means input holds actual
                                 * text to be input to command. */

    int outputFile = FILE_NAME; /* 0 means output is the name of output file.
                                 * 1 means output is the name of output file, and append.
                                 * 2 means output is filehandle name.
                                 * All this is ignored if output is NULL
                                 */
    int errorFile = FILE_NAME;  /* 0 means error is the name of error file.
                                 * 1 means error is the name of error file, and append.
                                 * 2 means error is filehandle name.
                                 * All this is ignored if error is NULL
                                 */
    const char *output = NULL;  /* Holds name of output file to pipe to,
                                 * or NULL if output goes to stdout/pipe. */
    const char *error = NULL;   /* Holds name of stderr file to pipe to,
                                 * or NULL if stderr goes to stderr/pipe. */
    int inputId = -1;
                                 /* Readable file id input to current command in
                                 * pipeline (could be file or pipe).  -1
                                 * means use stdin. */
    int outputId = -1;
                                 /* Writable file id for output from current
                                 * command in pipeline (could be file or pipe).
                                 * -1 means use stdout. */
    int errorId = -1;
                                 /* Writable file id for all standard error
                                 * output from all commands in pipeline.  -1
                                 * means use stderr. */
    int lastOutputId = -1;
                                 /* Write file id for output from last command
                                 * in pipeline (could be file or pipe).
                                 * -1 means use stdout. */
    int pipeIds[2];           /* File ids for pipe that's being created. */
    int firstArg, lastArg;      /* Indexes of first and last arguments in
                                 * current command. */
    int lastBar;
    int i;
    pidtype pid;
    char **save_environ;
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);

    /* Holds the args which will be used to exec */
    char **arg_array = Jim_Alloc(sizeof(*arg_array) * (argc + 1));
    int arg_count = 0;

    if (inPipePtr != NULL) {
        *inPipePtr = -1;
    }
    if (outPipePtr != NULL) {
        *outPipePtr = -1;
    }
    if (errFilePtr != NULL) {
        *errFilePtr = -1;
    }
    pipeIds[0] = pipeIds[1] = -1;

    /*
     * First, scan through all the arguments to figure out the structure
     * of the pipeline.  Count the number of distinct processes (it's the
     * number of "|" arguments).  If there are "<", "<<", or ">" arguments
     * then make note of input and output redirection and remove these
     * arguments and the arguments that follow them.
     */
    cmdCount = 1;
    lastBar = -1;
    for (i = 0; i < argc; i++) {
        const char *arg = Jim_String(argv[i]);

        if (arg[0] == '<') {
            inputFile = FILE_NAME;
            input = arg + 1;
            if (*input == '<') {
                inputFile = FILE_TEXT;
                input_len = Jim_Length(argv[i]) - 2;
                input++;
            }
            else if (*input == '@') {
                inputFile = FILE_HANDLE;
                input++;
            }

            if (!*input && ++i < argc) {
                input = Jim_GetString(argv[i], &input_len);
            }
        }
        else if (arg[0] == '>') {
            int dup_error = 0;

            outputFile = FILE_NAME;

            output = arg + 1;
            if (*output == '>') {
                outputFile = FILE_APPEND;
                output++;
            }
            if (*output == '&') {
                /* Redirect stderr too */
                output++;
                dup_error = 1;
            }
            if (*output == '@') {
                outputFile = FILE_HANDLE;
                output++;
            }
            if (!*output && ++i < argc) {
                output = Jim_String(argv[i]);
            }
            if (dup_error) {
                errorFile = outputFile;
                error = output;
            }
        }
        else if (arg[0] == '2' && arg[1] == '>') {
            error = arg + 2;
            errorFile = FILE_NAME;

            if (*error == '@') {
                errorFile = FILE_HANDLE;
                error++;
            }
            else if (*error == '>') {
                errorFile = FILE_APPEND;
                error++;
            }
            if (!*error && ++i < argc) {
                error = Jim_String(argv[i]);
            }
        }
        else {
            if (strcmp(arg, "|") == 0 || strcmp(arg, "|&") == 0) {
                if (i == lastBar + 1 || i == argc - 1) {
                    Jim_SetResultString(interp, "illegal use of | or |& in command", -1);
                    goto badargs;
                }
                lastBar = i;
                cmdCount++;
            }
            /* Either |, |& or a "normal" arg, so store it in the arg array */
            arg_array[arg_count++] = (char *)arg;
            continue;
        }

        if (i >= argc) {
            Jim_SetResultFormatted(interp, "can't specify \"%s\" as last word in command", arg);
            goto badargs;
        }
    }

    if (arg_count == 0) {
        Jim_SetResultString(interp, "didn't specify command to execute", -1);
badargs:
        Jim_Free(arg_array);
        return -1;
    }

    /* Must do this before vfork(), so do it now */
    save_environ = JimSaveEnv(JimBuildEnv(interp));

    /*
     * Set up the redirected input source for the pipeline, if
     * so requested.
     */
    if (input != NULL) {
        if (inputFile == FILE_TEXT) {
            /*
             * Immediate data in command.  Create temporary file and
             * put data into file.
             */
            inputId = Jim_MakeTempFile(interp, NULL, 1);
            if (inputId == -1) {
                goto error;
            }
            if (write(inputId, input, input_len) != input_len) {
                Jim_SetResultErrno(interp, "couldn't write temp file");
                close(inputId);
                goto error;
            }
            lseek(inputId, 0L, SEEK_SET);
        }
        else if (inputFile == FILE_HANDLE) {
            int fd = JimGetChannelFd(interp, input);

            if (fd < 0) {
                goto error;
            }
            inputId = dup(fd);
        }
        else {
            /*
             * File redirection.  Just open the file.
             */
            inputId = Jim_OpenForRead(input);
            if (inputId == -1) {
                Jim_SetResultFormatted(interp, "couldn't read file \"%s\": %s", input, strerror(Jim_Errno()));
                goto error;
            }
        }
    }
    else if (inPipePtr != NULL) {
        if (pipe(pipeIds) != 0) {
            Jim_SetResultErrno(interp, "couldn't create input pipe for command");
            goto error;
        }
        inputId = pipeIds[0];
        *inPipePtr = pipeIds[1];
        pipeIds[0] = pipeIds[1] = -1;
    }

    /*
     * Set up the redirected output sink for the pipeline from one
     * of two places, if requested.
     */
    if (output != NULL) {
        if (outputFile == FILE_HANDLE) {
            int fd = JimGetChannelFd(interp, output);
            if (fd < 0) {
                goto error;
            }
            lastOutputId = dup(fd);
        }
        else {
            /*
             * Output is to go to a file.
             */
            lastOutputId = Jim_OpenForWrite(output, outputFile == FILE_APPEND);
            if (lastOutputId == -1) {
                Jim_SetResultFormatted(interp, "couldn't write file \"%s\": %s", output, strerror(Jim_Errno()));
                goto error;
            }
        }
    }
    else if (outPipePtr != NULL) {
        /*
         * Output is to go to a pipe.
         */
        if (pipe(pipeIds) != 0) {
            Jim_SetResultErrno(interp, "couldn't create output pipe");
            goto error;
        }
        lastOutputId = pipeIds[1];
        *outPipePtr = pipeIds[0];
        pipeIds[0] = pipeIds[1] = -1;
    }
    /* If we are redirecting stderr with 2>filename or 2>@fileId, then we ignore errFilePtr */
    if (error != NULL) {
        if (errorFile == FILE_HANDLE) {
            if (strcmp(error, "1") == 0) {
                /* Special 2>@1 */
                if (lastOutputId != -1) {
                    errorId = dup(lastOutputId);
                }
                else {
                    /* No redirection of stdout, so just use 2>@stdout */
                    error = "stdout";
                }
            }
            if (errorId == -1) {
                int fd = JimGetChannelFd(interp, error);
                if (fd < 0) {
                    goto error;
                }
                errorId = dup(fd);
            }
        }
        else {
            /*
             * Output is to go to a file.
             */
            errorId = Jim_OpenForWrite(error, errorFile == FILE_APPEND);
            if (errorId == -1) {
                Jim_SetResultFormatted(interp, "couldn't write file \"%s\": %s", error, strerror(Jim_Errno()));
                goto error;
            }
        }
    }
    else if (errFilePtr != NULL) {
        /*
         * Set up the standard error output sink for the pipeline, if
         * requested.  Use a temporary file which is opened, then deleted.
         * Could potentially just use pipe, but if it filled up it could
         * cause the pipeline to deadlock:  we'd be waiting for processes
         * to complete before reading stderr, and processes couldn't complete
         * because stderr was backed up.
         */
        errorId = Jim_MakeTempFile(interp, NULL, 1);
        if (errorId == -1) {
            goto error;
        }
        *errFilePtr = dup(errorId);
    }

    /*
     * Scan through the argc array, forking off a process for each
     * group of arguments between "|" arguments.
     */

    pidPtr = Jim_Alloc(cmdCount * sizeof(*pidPtr));
    for (i = 0; i < numPids; i++) {
        pidPtr[i] = JIM_BAD_PID;
    }
    for (firstArg = 0; firstArg < arg_count; numPids++, firstArg = lastArg + 1) {
        int pipe_dup_err = 0;
        int origErrorId = errorId;

        for (lastArg = firstArg; lastArg < arg_count; lastArg++) {
            if (strcmp(arg_array[lastArg], "|") == 0) {
                break;
            }
            if (strcmp(arg_array[lastArg], "|&") == 0) {
                pipe_dup_err = 1;
                break;
            }
        }

        if (lastArg == firstArg) {
            Jim_SetResultString(interp, "missing command to exec", -1);
            goto error;
        }

        /* Replace | with NULL for execv() */
        arg_array[lastArg] = NULL;
        if (lastArg == arg_count) {
            outputId = lastOutputId;
            lastOutputId = -1;
        }
        else {
            if (pipe(pipeIds) != 0) {
                Jim_SetResultErrno(interp, "couldn't create pipe");
                goto error;
            }
            outputId = pipeIds[1];
        }

        /* Need to do this before vfork() */
        if (pipe_dup_err) {
            errorId = outputId;
        }

        i = strlen(arg_array[firstArg]);

        /* Now fork the child */

#ifdef __MINGW32__
        pid = JimStartWinProcess(interp, &arg_array[firstArg], save_environ, inputId, outputId, errorId);
        if (pid == JIM_BAD_PID) {
            Jim_SetResultFormatted(interp, "couldn't exec \"%s\"", arg_array[firstArg]);
            goto error;
        }
#else
        /*
         * Make a new process and enter it into the table if the vfork
         * is successful.
         */
        pid = vfork();
        if (pid < 0) {
            Jim_SetResultErrno(interp, "couldn't fork child process");
            goto error;
        }
        if (pid == 0) {
            /* Child */
            /* Set up stdin, stdout, stderr */
            if (inputId != -1) {
                dup2(inputId, fileno(stdin));
                close(inputId);
            }
            if (outputId != -1) {
                dup2(outputId, fileno(stdout));
                if (outputId != errorId) {
                    close(outputId);
                }
            }
            if (errorId != -1) {
                dup2(errorId, fileno(stderr));
                close(errorId);
            }
            /* Close parent-only file descriptors */
            if (outPipePtr) {
                close(*outPipePtr);
            }
            if (errFilePtr) {
                close(*errFilePtr);
            }
            if (pipeIds[0] != -1) {
                close(pipeIds[0]);
            }
            if (lastOutputId != -1) {
                close(lastOutputId);
            }

            /* Restore SIGPIPE behaviour */
            (void)signal(SIGPIPE, SIG_DFL);

            execvpe(arg_array[firstArg], &arg_array[firstArg], save_environ);

            if (write(fileno(stderr), "couldn't exec \"", 15) &&
                write(fileno(stderr), arg_array[firstArg], i) &&
                write(fileno(stderr), "\"\n", 2)) {
                /* nothing */
            }
#ifdef JIM_MAINTAINER
            {
                /* Keep valgrind happy */
                static char *const false_argv[2] = {"false", NULL};
                execvp(false_argv[0],false_argv);
            }
#endif
            _exit(127);
        }
#endif

        /* parent */

        /*
         * Enlarge the wait table if there isn't enough space for a new
         * entry.
         */
        if (table->used == table->size) {
            table->size += WAIT_TABLE_GROW_BY;
            table->info = Jim_Realloc(table->info, table->size * sizeof(*table->info));
        }

        table->info[table->used].pid = pid;
        table->info[table->used].flags = 0;
        table->used++;

        pidPtr[numPids] = pid;

        /* Restore in case of pipe_dup_err */
        errorId = origErrorId;

        /*
         * Close off our copies of file descriptors that were set up for
         * this child, then set up the input for the next child.
         */

        if (inputId != -1) {
            close(inputId);
        }
        if (outputId != -1) {
            close(outputId);
        }
        inputId = pipeIds[0];
        pipeIds[0] = pipeIds[1] = -1;
    }
    *pidArrayPtr = pidPtr;

    /*
     * All done.  Cleanup open files lying around and then return.
     */

  cleanup:
    if (inputId != -1) {
        close(inputId);
    }
    if (lastOutputId != -1) {
        close(lastOutputId);
    }
    if (errorId != -1) {
        close(errorId);
    }
    Jim_Free(arg_array);

    JimRestoreEnv(save_environ);

    return numPids;

    /*
     * An error occurred.  There could have been extra files open, such
     * as pipes between children.  Clean them all up.  Detach any child
     * processes that have been created.
     */

  error:
    if ((inPipePtr != NULL) && (*inPipePtr != -1)) {
        close(*inPipePtr);
        *inPipePtr = -1;
    }
    if ((outPipePtr != NULL) && (*outPipePtr != -1)) {
        close(*outPipePtr);
        *outPipePtr = -1;
    }
    if ((errFilePtr != NULL) && (*errFilePtr != -1)) {
        close(*errFilePtr);
        *errFilePtr = -1;
    }
    if (pipeIds[0] != -1) {
        close(pipeIds[0]);
    }
    if (pipeIds[1] != -1) {
        close(pipeIds[1]);
    }
    if (pidPtr != NULL) {
        for (i = 0; i < numPids; i++) {
            if (pidPtr[i] != JIM_BAD_PID) {
                JimDetachPids(table, 1, &pidPtr[i]);
            }
        }
        Jim_Free(pidPtr);
    }
    numPids = -1;
    goto cleanup;
}

/*
 *----------------------------------------------------------------------
 *
 * JimCleanupChildren --
 *
 *  This is a utility procedure used to wait for child processes
 *  to exit, record information about abnormal exits.
 *
 * Results:
 *  The return value is a standard Tcl result.  If anything at
 *  weird happened with the child processes, JIM_ERR is returned
 *  and a structured message is left in $::errorCode.
 *  If errStrObj is not NULL, abnormal exit details are appended to this object.
 *
 * Side effects:
 *  pidPtr is freed
 *
 *----------------------------------------------------------------------
 */

static int JimCleanupChildren(Jim_Interp *interp, int numPids, pidtype *pidPtr, Jim_Obj *errStrObj)
{
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);
    int result = JIM_OK;
    int i;

    /* Now check the return status of each child */
    for (i = 0; i < numPids; i++) {
        int waitStatus = 0;
        if (JimWaitForProcess(table, pidPtr[i], &waitStatus) != JIM_BAD_PID) {
            if (JimCheckWaitStatus(interp, pidPtr[i], waitStatus, errStrObj) != JIM_OK) {
                result = JIM_ERR;
            }
        }
    }
    Jim_Free(pidPtr);

    return result;
}

int Jim_execInit(Jim_Interp *interp)
{
    struct WaitInfoTable *waitinfo;
    if (Jim_PackageProvide(interp, "exec", "1.0", JIM_ERRMSG))
        return JIM_ERR;

#ifdef SIGPIPE
    /*
     * Disable SIGPIPE signals:  if they were allowed, this process
     * might go away unexpectedly if children misbehave.  This code
     * can potentially interfere with other application code that
     * expects to handle SIGPIPEs.
     *
     * By doing this in the init function, applications can override
     * this later. Note that child processes have SIGPIPE restored
     * to the default after vfork().
     */
    (void)signal(SIGPIPE, SIG_IGN);
#endif

    waitinfo = JimAllocWaitInfoTable();
    Jim_CreateCommand(interp, "exec", Jim_ExecCmd, waitinfo, JimFreeWaitInfoTable);
    waitinfo->refcount++;
    Jim_CreateCommand(interp, "wait", Jim_WaitCommand, waitinfo, JimFreeWaitInfoTable);
    Jim_CreateCommand(interp, "pid", Jim_PidCommand, 0, 0);

    return JIM_OK;
}

#if defined(__MINGW32__)
/* Windows-specific (mingw) implementation */

static int
JimWinFindExecutable(const char *originalName, char fullPath[MAX_PATH])
{
    int i;
    static char extensions[][5] = {".exe", "", ".bat"};

    for (i = 0; i < (int) (sizeof(extensions) / sizeof(extensions[0])); i++) {
        snprintf(fullPath, MAX_PATH, "%s%s", originalName, extensions[i]);

        if (SearchPath(NULL, fullPath, NULL, MAX_PATH, fullPath, NULL) == 0) {
            continue;
        }
        if (GetFileAttributes(fullPath) & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        return 0;
    }

    return -1;
}

static char **JimSaveEnv(char **env)
{
    return env;
}

static void JimRestoreEnv(char **env)
{
    JimFreeEnv(env, Jim_GetEnviron());
}

static char **JimOriginalEnviron(void)
{
    return NULL;
}

static Jim_Obj *
JimWinBuildCommandLine(Jim_Interp *interp, char **argv)
{
    char *start, *special;
    int quote, i;

    Jim_Obj *strObj = Jim_NewStringObj(interp, "", 0);

    for (i = 0; argv[i]; i++) {
        if (i > 0) {
            Jim_AppendString(interp, strObj, " ", 1);
        }

        if (argv[i][0] == '\0') {
            quote = 1;
        }
        else {
            quote = 0;
            for (start = argv[i]; *start != '\0'; start++) {
                if (isspace(UCHAR(*start))) {
                    quote = 1;
                    break;
                }
            }
        }
        if (quote) {
            Jim_AppendString(interp, strObj, "\"" , 1);
        }

        start = argv[i];
        for (special = argv[i]; ; ) {
            if ((*special == '\\') && (special[1] == '\\' ||
                    special[1] == '"' || (quote && special[1] == '\0'))) {
                Jim_AppendString(interp, strObj, start, special - start);
                start = special;
                while (1) {
                    special++;
                    if (*special == '"' || (quote && *special == '\0')) {
                        /*
                         * N backslashes followed a quote -> insert
                         * N * 2 + 1 backslashes then a quote.
                         */

                        Jim_AppendString(interp, strObj, start, special - start);
                        break;
                    }
                    if (*special != '\\') {
                        break;
                    }
                }
                Jim_AppendString(interp, strObj, start, special - start);
                start = special;
            }
            if (*special == '"') {
        if (special == start) {
            Jim_AppendString(interp, strObj, "\"", 1);
        }
        else {
            Jim_AppendString(interp, strObj, start, special - start);
        }
                Jim_AppendString(interp, strObj, "\\\"", 2);
                start = special + 1;
            }
            if (*special == '\0') {
                break;
            }
            special++;
        }
        Jim_AppendString(interp, strObj, start, special - start);
        if (quote) {
            Jim_AppendString(interp, strObj, "\"", 1);
        }
    }
    return strObj;
}

/**
 * Note that inputId, etc. are osf_handles.
 */
static pidtype
JimStartWinProcess(Jim_Interp *interp, char **argv, char **env, int inputId, int outputId, int errorId)
{
    STARTUPINFO startInfo;
    PROCESS_INFORMATION procInfo;
    HANDLE hProcess;
    char execPath[MAX_PATH];
    pidtype pid = JIM_BAD_PID;
    Jim_Obj *cmdLineObj;
    char *winenv;

    if (JimWinFindExecutable(argv[0], execPath) < 0) {
        return JIM_BAD_PID;
    }
    argv[0] = execPath;

    hProcess = GetCurrentProcess();
    cmdLineObj = JimWinBuildCommandLine(interp, argv);

    /*
     * STARTF_USESTDHANDLES must be used to pass handles to child process.
     * Using SetStdHandle() and/or dup2() only works when a console mode
     * parent process is spawning an attached console mode child process.
     */

    ZeroMemory(&startInfo, sizeof(startInfo));
    startInfo.cb = sizeof(startInfo);
    startInfo.dwFlags   = STARTF_USESTDHANDLES;
    startInfo.hStdInput = INVALID_HANDLE_VALUE;
    startInfo.hStdOutput= INVALID_HANDLE_VALUE;
    startInfo.hStdError = INVALID_HANDLE_VALUE;

    /*
     * Duplicate all the handles which will be passed off as stdin, stdout
     * and stderr of the child process. The duplicate handles are set to
     * be inheritable, so the child process can use them.
     */
    /*
     * If stdin was not redirected, input should come from the parent's stdin
     */
    if (inputId == -1) {
        inputId = _fileno(stdin);
    }
    DuplicateHandle(hProcess, (HANDLE)_get_osfhandle(inputId), hProcess, &startInfo.hStdInput,
            0, TRUE, DUPLICATE_SAME_ACCESS);
    if (startInfo.hStdInput == INVALID_HANDLE_VALUE) {
        goto end;
    }

    /*
     * If stdout was not redirected, output should go to the parent's stdout
     */
    if (outputId == -1) {
        outputId = _fileno(stdout);
    }
    DuplicateHandle(hProcess, (HANDLE)_get_osfhandle(outputId), hProcess, &startInfo.hStdOutput,
            0, TRUE, DUPLICATE_SAME_ACCESS);
    if (startInfo.hStdOutput == INVALID_HANDLE_VALUE) {
        goto end;
    }

    /* Ditto stderr */
    if (errorId == -1) {
        errorId = _fileno(stderr);
    }
    DuplicateHandle(hProcess, (HANDLE)_get_osfhandle(errorId), hProcess, &startInfo.hStdError,
            0, TRUE, DUPLICATE_SAME_ACCESS);
    if (startInfo.hStdError == INVALID_HANDLE_VALUE) {
        goto end;
    }

    /* If env is NULL, use the original environment.
     * If env[0] is NULL, use an empty environment.
     * Otherwise use the environment starting at env[0]
     */
    if (env == NULL) {
        /* Use the original environment */
        winenv = NULL;
    }
    else if (env[0] == NULL) {
        winenv = (char *)"\0";
    }
    else {
        winenv = env[0];
    }

    if (!CreateProcess(NULL, (char *)Jim_String(cmdLineObj), NULL, NULL, TRUE,
            0, winenv, NULL, &startInfo, &procInfo)) {
        goto end;
    }

    /*
     * "When an application spawns a process repeatedly, a new thread
     * instance will be created for each process but the previous
     * instances may not be cleaned up.  This results in a significant
     * virtual memory loss each time the process is spawned.  If there
     * is a WaitForInputIdle() call between CreateProcess() and
     * CloseHandle(), the problem does not occur." PSS ID Number: Q124121
     */

    WaitForInputIdle(procInfo.hProcess, 5000);
    CloseHandle(procInfo.hThread);

    pid = procInfo.hProcess;

    end:
    Jim_FreeNewObj(interp, cmdLineObj);
    if (startInfo.hStdInput != INVALID_HANDLE_VALUE) {
        CloseHandle(startInfo.hStdInput);
    }
    if (startInfo.hStdOutput != INVALID_HANDLE_VALUE) {
        CloseHandle(startInfo.hStdOutput);
    }
    if (startInfo.hStdError != INVALID_HANDLE_VALUE) {
        CloseHandle(startInfo.hStdError);
    }
    return pid;
}

#else

static char **JimOriginalEnviron(void)
{
    return Jim_GetEnviron();
}

static char **JimSaveEnv(char **env)
{
    char **saveenv = Jim_GetEnviron();
    Jim_SetEnviron(env);
    return saveenv;
}

static void JimRestoreEnv(char **env)
{
    JimFreeEnv(Jim_GetEnviron(), env);
    Jim_SetEnviron(env);
}
#endif
#endif
