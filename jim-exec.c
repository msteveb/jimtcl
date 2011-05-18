
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

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

#include "jim.h"
#include "jimautoconf.h"
#include "jim-subcmd.h"
#include "jim-signal.h"

extern char **environ;

/* These two could be moved into the Tcl core */
static void Jim_SetResultErrno(Jim_Interp *interp, const char *msg)
{
    Jim_SetResultFormatted(interp, "%s: %s", msg, strerror(errno));
}

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
 * Read from 'fd' and append the data to strObj
 * Returns JIM_OK if OK, or JIM_ERR on error.
 */
static int JimAppendStreamToString(Jim_Interp *interp, int fd, Jim_Obj *strObj)
{
    while (1) {
        char buffer[256];
        int count;

        count = read(fd, buffer, sizeof(buffer));

        if (count == 0) {
            Jim_RemoveTrailingNewline(strObj);
            return JIM_OK;
        }
        if (count < 0) {
            return JIM_ERR;
        }
        Jim_AppendString(interp, strObj, buffer, count);
    }
}

/*
 * If the last character of the result is a newline, then remove
 * the newline character (the newline would just confuse things).
 *
 * Note: Ideally we could do this by just reducing the length of stringrep
 *       by 1, but there is no API for this :-(
 */
static void JimTrimTrailingNewline(Jim_Interp *interp)
{
    int len;
    const char *p = Jim_GetString(Jim_GetResult(interp), &len);

    if (len > 0 && p[len - 1] == '\n') {
        Jim_SetResultString(interp, p, len - 1);
    }
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
#ifdef jim_ext_tclcompat
    int i;
    int len;
    int n;
    char **env;

    Jim_Obj *objPtr = Jim_GetGlobalVariableStr(interp, "env", JIM_NONE);

    if (!objPtr) {
        return environ;
    }

    /* Calculate the required size */
    len = Jim_ListLength(interp, objPtr);
    if (len % 2) {
        len--;
    }

    env = Jim_Alloc(sizeof(*env) * (len / 2 + 1));

    n = 0;
    for (i = 0; i < len; i += 2) {
        int l1, l2;
        const char *s1, *s2;
        Jim_Obj *elemObj;

        Jim_ListIndex(interp, objPtr, i, &elemObj, JIM_NONE);
        s1 = Jim_GetString(elemObj, &l1);
        Jim_ListIndex(interp, objPtr, i + 1, &elemObj, JIM_NONE);
        s2 = Jim_GetString(elemObj, &l2);

        env[n] = Jim_Alloc(l1 + l2 + 2);
        sprintf(env[n], "%s=%s", s1, s2);
        n++;
    }
    env[n] = NULL;

    return env;
#else
    return environ;
#endif
}

/**
 * Frees the environment allocated by JimBuildEnv()
 *
 * Must pass original_environ.
 */
static void JimFreeEnv(Jim_Interp *interp, char **env, char **original_environ)
{
#ifdef jim_ext_tclcompat
    if (env != original_environ) {
        int i;
        for (i = 0; env[i]; i++) {
            Jim_Free(env[i]);
        }
        Jim_Free(env);
    }
#endif
}

/*
 * Create error messages for unusual process exits.  An
 * extra newline gets appended to each error message, but
 * it gets removed below (in the same fashion that an
 * extra newline in the command's output is removed).
 */
static int JimCheckWaitStatus(Jim_Interp *interp, int pid, int waitStatus)
{
    Jim_Obj *errorCode = Jim_NewListObj(interp, NULL, 0);
    int rc = JIM_ERR;

    if (WIFEXITED(waitStatus)) {
        if (WEXITSTATUS(waitStatus) == 0) {
            Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "NONE", -1));
            rc = JIM_OK;
        }
        else {
            Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "CHILDSTATUS", -1));
            Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, pid));
            Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, WEXITSTATUS(waitStatus)));
        }
    }
    else {
        const char *type;
        const char *action;

        if (WIFSIGNALED(waitStatus)) {
            type = "CHILDKILLED";
            action = "killed";
        }
        else {
            type = "CHILDSUSP";
            action = "suspended";
        }

        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, type, -1));

#ifdef jim_ext_signal
        Jim_SetResultFormatted(interp, "child %s by signal %s", action, Jim_SignalId(WTERMSIG(waitStatus)));
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, Jim_SignalId(WTERMSIG(waitStatus)), -1));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, pid));
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, Jim_SignalName(WTERMSIG(waitStatus)), -1));
#else
        Jim_SetResultFormatted(interp, "child %s by signal %d", action, WTERMSIG(waitStatus));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, WTERMSIG(waitStatus)));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, pid));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, WTERMSIG(waitStatus)));
#endif
    }
    Jim_SetGlobalVariableStr(interp, "errorCode", errorCode);
    return rc;
}

/*
 * Data structures of the following type are used by JimFork and
 * JimWaitPids to keep track of child processes.
 */

struct WaitInfo
{
    int pid;                    /* Process id of child. */
    int status;                 /* Status returned when child exited or suspended. */
    int flags;                  /* Various flag bits;  see below for definitions. */
};

struct WaitInfoTable {
    struct WaitInfo *info;
    int size;
    int used;
};

/*
 * Flag bits in WaitInfo structures:
 *
 * WI_READY -           Non-zero means process has exited or
 *                      suspended since it was forked or last
 *                      returned by JimWaitPids.
 * WI_DETACHED -        Non-zero means no-one cares about the
 *                      process anymore.  Ignore it until it
 *                      exits, then forget about it.
 */

#define WI_READY    1
#define WI_DETACHED 2

#define WAIT_TABLE_GROW_BY 4

static void JimFreeWaitInfoTable(struct Jim_Interp *interp, void *privData)
{
    struct WaitInfoTable *table = privData;

    Jim_Free(table->info);
    Jim_Free(table);
}

static struct WaitInfoTable *JimAllocWaitInfoTable(void)
{
    struct WaitInfoTable *table = Jim_Alloc(sizeof(*table));
    table->info = NULL;
    table->size = table->used = 0;

    return table;
}

static int Jim_CreatePipeline(Jim_Interp *interp, int argc, Jim_Obj *const *argv,
    int **pidArrayPtr, int *inPipePtr, int *outPipePtr, int *errFilePtr);
static void JimDetachPids(Jim_Interp *interp, int numPids, int *pidPtr);
static int Jim_CleanupChildren(Jim_Interp *interp, int numPids, int *pidPtr, int errorId);

static int Jim_ExecCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int outputId;               /* File id for output pipe.  -1
                                 * means command overrode. */
    int errorId;                /* File id for temporary file
                                 * containing error output. */
    int *pidPtr;
    int numPids, result;

    /*
     * See if the command is to be run in background;  if so, create
     * the command, detach it, and return.
     */
    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[argc - 1], "&")) {
        Jim_Obj *listObj;
        int i;

        argc--;
        numPids = Jim_CreatePipeline(interp, argc - 1, argv + 1, &pidPtr, NULL, NULL, NULL);
        if (numPids < 0) {
            return JIM_ERR;
        }
        /* The return value is a list of the pids */
        listObj = Jim_NewListObj(interp, NULL, 0);
        for (i = 0; i < numPids; i++) {
            Jim_ListAppendElement(interp, listObj, Jim_NewIntObj(interp, pidPtr[i]));
        }
        Jim_SetResult(interp, listObj);
        JimDetachPids(interp, numPids, pidPtr);
        Jim_Free(pidPtr);
        return JIM_OK;
    }

    /*
     * Create the command's pipeline.
     */
    numPids =
        Jim_CreatePipeline(interp, argc - 1, argv + 1, &pidPtr, (int *)NULL, &outputId, &errorId);
    if (numPids < 0) {
        return JIM_ERR;
    }

    /*
     * Read the child's output (if any) and put it into the result.
     */
    Jim_SetResultString(interp, "", 0);

    result = JIM_OK;
    if (outputId != -1) {
        result = JimAppendStreamToString(interp, outputId, Jim_GetResult(interp));
        if (result < 0) {
            Jim_SetResultErrno(interp, "error reading from output pipe");
        }
        close(outputId);
    }

    if (Jim_CleanupChildren(interp, numPids, pidPtr, errorId) != JIM_OK) {
        result = JIM_ERR;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * JimWaitPids --
 *
 *  This procedure is used to wait for one or more processes created
 *  by JimFork to exit or suspend.  It records information about
 *  all processes that exit or suspend, even those not waited for,
 *  so that later waits for them will be able to get the status
 *  information.
 *
 * Results:
 *  -1 is returned if there is an error in the wait kernel call.
 *  Otherwise the pid of an exited/suspended process from *pidPtr
 *  is returned and *statusPtr is set to the status value returned
 *  by the wait kernel call.
 *
 * Side effects:
 *  Doesn't return until one of the pids at *pidPtr exits or suspends.
 *
 *----------------------------------------------------------------------
 */
static int JimWaitPids(struct WaitInfoTable *table, int numPids, int *pidPtr, int *statusPtr)
{
    int i, count, pid;
    struct WaitInfo *waitPtr;
    int anyProcesses;
    int status;

    while (1) {
        /*
         * Scan the table of child processes to see if one of the
         * specified children has already exited or suspended.  If so,
         * remove it from the table and return its status.
         */

        anyProcesses = 0;
        for (waitPtr = table->info, count = table->used; count > 0; waitPtr++, count--) {
            for (i = 0; i < numPids; i++) {
                if (pidPtr[i] != waitPtr->pid) {
                    continue;
                }
                anyProcesses = 1;
                if (waitPtr->flags & WI_READY) {
                    *statusPtr = *((int *)&waitPtr->status);
                    pid = waitPtr->pid;
                    if (WIFEXITED(waitPtr->status) || WIFSIGNALED(waitPtr->status)) {
                        if (waitPtr != &table->info[table->used - 1]) {
                            *waitPtr = table->info[table->used - 1];
                        }
                        table->used--;
                    }
                    else {
                        waitPtr->flags &= ~WI_READY;
                    }
                    return pid;
                }
            }
        }

        /*
         * Make sure that the caller at least specified one valid
         * process to wait for.
         */
        if (!anyProcesses) {
            errno = ECHILD;
            return -1;
        }

        /*
         * Wait for a process to exit or suspend, then update its
         * entry in the table and go back to the beginning of the
         * loop to see if it's one of the desired processes.
         */

        pid = wait(&status);
        if (pid < 0) {
            return pid;
        }
        for (waitPtr = table->info, count = table->used;; waitPtr++, count--) {
            if (count == 0) {
                break;          /* Ignore unknown processes. */
            }
            if (pid != waitPtr->pid) {
                continue;
            }

            /*
             * If the process has been detached, then ignore anything
             * other than an exit, and drop the entry on exit.
             */
            if (waitPtr->flags & WI_DETACHED) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    *waitPtr = table->info[table->used - 1];
                    table->used--;
                }
            }
            else {
                waitPtr->status = status;
                waitPtr->flags |= WI_READY;
            }
            break;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * JimDetachPids --
 *
 *  This procedure is called to indicate that one or more child
 *  processes have been placed in background and are no longer
 *  cared about.  They should be ignored in future calls to
 *  JimWaitPids.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static void JimDetachPids(Jim_Interp *interp, int numPids, int *pidPtr)
{
    int i, count, pid;
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);
    struct WaitInfo *waitPtr;

    for (i = 0; i < numPids; i++) {
        pid = pidPtr[i];
        for (waitPtr = table->info, count = table->used; count > 0; waitPtr++, count--) {
            if (pid != waitPtr->pid) {
                continue;
            }

            /*
             * If the process has already exited then destroy its
             * table entry now.
             */

            if ((waitPtr->flags & WI_READY) && (WIFEXITED(waitPtr->status)
                    || WIFSIGNALED(waitPtr->status))) {
                *waitPtr = table->info[table->used - 1];
                table->used--;
            }
            else {
                waitPtr->flags |= WI_DETACHED;
            }
            goto nextPid;
        }
        Jim_Panic(interp, "Jim_Detach couldn't find process");

      nextPid:
        continue;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Jim_CreatePipeline --
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
Jim_CreatePipeline(Jim_Interp *interp, int argc, Jim_Obj *const *argv, int **pidArrayPtr,
    int *inPipePtr, int *outPipePtr, int *errFilePtr)
{
    int *pidPtr = NULL;         /* Points to malloc-ed array holding all
                                 * the pids of child processes. */
    int numPids = 0;            /* Actual number of processes that exist
                                 * at *pidPtr right now. */
    int cmdCount;               /* Count of number of distinct commands
                                 * found in argc/argv. */
    const char *input = NULL;   /* Describes input for pipeline, depending
                                 * on "inputFile".  NULL means take input
                                 * from stdin/pipe. */

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
    int inputId = -1;           /* Readable file id input to current command in
                                 * pipeline (could be file or pipe).  -1
                                 * means use stdin. */
    int outputId = -1;          /* Writable file id for output from current
                                 * command in pipeline (could be file or pipe).
                                 * -1 means use stdout. */
    int errorId = -1;           /* Writable file id for all standard error
                                 * output from all commands in pipeline.  -1
                                 * means use stderr. */
    int lastOutputId = -1;      /* Write file id for output from last command
                                 * in pipeline (could be file or pipe).
                                 * -1 means use stdout. */
    int pipeIds[2];             /* File ids for pipe that's being created. */
    int firstArg, lastArg;      /* Indexes of first and last arguments in
                                 * current command. */
    int lastBar;
    char *execName;
    int i, pid;
    char **orig_environ;
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
        const char *arg = Jim_GetString(argv[i], NULL);

        if (arg[0] == '<') {
            inputFile = FILE_NAME;
            input = arg + 1;
            if (*input == '<') {
                inputFile = FILE_TEXT;
                input++;
            }
            else if (*input == '@') {
                inputFile = FILE_HANDLE;
                input++;
            }

            if (!*input && ++i < argc) {
                input = Jim_GetString(argv[i], NULL);
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
                output = Jim_GetString(argv[i], NULL);
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
                error = Jim_GetString(argv[i], NULL);
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
    orig_environ = environ;
    environ = JimBuildEnv(interp);

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

#define TMP_STDIN_NAME "/tmp/tcl.in.XXXXXX"
            char inName[sizeof(TMP_STDIN_NAME) + 1];
            int length;

            strcpy(inName, TMP_STDIN_NAME);
            inputId = mkstemp(inName);
            if (inputId < 0) {
                Jim_SetResultErrno(interp, "couldn't create input file for command");
                goto error;
            }
            length = strlen(input);
            if (write(inputId, input, length) != length) {
                Jim_SetResultErrno(interp, "couldn't write file input for command");
                goto error;
            }
            if (lseek(inputId, 0L, SEEK_SET) == -1 || unlink(inName) == -1) {
                Jim_SetResultErrno(interp, "couldn't reset or remove input file for command");
                goto error;
            }
        }
        else if (inputFile == FILE_HANDLE) {
            /* Should be a file descriptor */
            Jim_Obj *fhObj = Jim_NewStringObj(interp, input, -1);
            FILE *fh = Jim_AioFilehandle(interp, fhObj);

            Jim_FreeNewObj(interp, fhObj);
            if (fh == NULL) {
                goto error;
            }
            inputId = dup(fileno(fh));
        }
        else {
            /*
             * File redirection.  Just open the file.
             */
            inputId = open(input, O_RDONLY, 0);
            if (inputId < 0) {
                Jim_SetResultFormatted(interp, "couldn't read file \"%s\": %s", input,
                    strerror(errno));
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
            Jim_Obj *fhObj = Jim_NewStringObj(interp, output, -1);
            FILE *fh = Jim_AioFilehandle(interp, fhObj);

            Jim_FreeNewObj(interp, fhObj);
            if (fh == NULL) {
                goto error;
            }
            fflush(fh);
            lastOutputId = dup(fileno(fh));
        }
        else {
            /*
             * Output is to go to a file.
             */
            int mode = O_WRONLY | O_CREAT | O_TRUNC;

            if (outputFile == FILE_APPEND) {
                mode = O_WRONLY | O_CREAT | O_APPEND;
            }

            lastOutputId = open(output, mode, 0666);
            if (lastOutputId < 0) {
                Jim_SetResultFormatted(interp, "couldn't write file \"%s\": %s", output,
                    strerror(errno));
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
                if (lastOutputId >= 0) {
                    errorId = dup(lastOutputId);
                }
                else {
                    /* No redirection of stdout, so just use 2>@stdout */
                    error = "stdout";
                }
            }
            if (errorId < 0) {
                Jim_Obj *fhObj = Jim_NewStringObj(interp, error, -1);
                FILE *fh = Jim_AioFilehandle(interp, fhObj);

                Jim_FreeNewObj(interp, fhObj);
                if (fh == NULL) {
                    goto error;
                }
                fflush(fh);
                errorId = dup(fileno(fh));
            }
        }
        else {
            /*
             * Output is to go to a file.
             */
            int mode = O_WRONLY | O_CREAT | O_TRUNC;

            if (errorFile == FILE_APPEND) {
                mode = O_WRONLY | O_CREAT | O_APPEND;
            }

            errorId = open(error, mode, 0666);
            if (errorId < 0) {
                Jim_SetResultFormatted(interp, "couldn't write file \"%s\": %s", error,
                    strerror(errno));
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

#define TMP_STDERR_NAME "/tmp/tcl.err.XXXXXX"
        char errName[sizeof(TMP_STDERR_NAME) + 1];

        strcpy(errName, TMP_STDERR_NAME);
        errorId = mkstemp(errName);
        if (errorId < 0) {
          errFileError:
            Jim_SetResultErrno(interp, "couldn't create error file for command");
            goto error;
        }
        *errFilePtr = open(errName, O_RDONLY, 0);
        if (*errFilePtr < 0) {
            goto errFileError;
        }
        if (unlink(errName) == -1) {
            Jim_SetResultErrno(interp, "couldn't remove error file for command");
            goto error;
        }
    }

    /*
     * Scan through the argc array, forking off a process for each
     * group of arguments between "|" arguments.
     */

    pidPtr = (int *)Jim_Alloc(cmdCount * sizeof(*pidPtr));
    for (i = 0; i < numPids; i++) {
        pidPtr[i] = -1;
    }
    for (firstArg = 0; firstArg < arg_count; numPids++, firstArg = lastArg + 1) {
        int pipe_dup_err = 0;
        int origErrorId = errorId;
        char execerr[64];
        int execerrlen;

        for (lastArg = firstArg; lastArg < arg_count; lastArg++) {
            if (arg_array[lastArg][0] == '|') {
                if (arg_array[lastArg][1] == '&') {
                    pipe_dup_err = 1;
                }
                break;
            }
        }
        /* Replace | with NULL for execv() */
        arg_array[lastArg] = NULL;
        if (lastArg == arg_count) {
            outputId = lastOutputId;
        }
        else {
            if (pipe(pipeIds) != 0) {
                Jim_SetResultErrno(interp, "couldn't create pipe");
                goto error;
            }
            outputId = pipeIds[1];
        }
        execName = arg_array[firstArg];

        /* Now fork the child */

        /*
         * Disable SIGPIPE signals:  if they were allowed, this process
         * might go away unexpectedly if children misbehave.  This code
         * can potentially interfere with other application code that
         * expects to handle SIGPIPEs;  what's really needed is an
         * arbiter for signals to allow them to be "shared".
         */
        if (table->info == NULL) {
            (void)signal(SIGPIPE, SIG_IGN);
        }

        /*
         * Enlarge the wait table if there isn't enough space for a new
         * entry.
         */
        if (table->used == table->size) {
            table->size += WAIT_TABLE_GROW_BY;
            table->info = Jim_Realloc(table->info, table->size * sizeof(*table->info));
        }

        /* Need to do this befor vfork() */
        if (pipe_dup_err) {
            errorId = outputId;
        }

        /* Need to prep an error message before vfork(), just in case */
        snprintf(execerr, sizeof(execerr), "couldn't exec \"%s\"", execName);
        execerrlen = strlen(execerr);

        /*
         * Make a new process and enter it into the table if the fork
         * is successful.
         */
        pid = vfork();
        if (pid < 0) {
            Jim_SetResultErrno(interp, "couldn't fork child process");
            goto error;
        }
        if (pid == 0) {
            /* Child */

            if (inputId != -1) dup2(inputId, 0);
            if (outputId != -1) dup2(outputId, 1);
            if (errorId != -1) dup2(errorId, 2);

            for (i = 3; (i <= outputId) || (i <= inputId) || (i <= errorId); i++) {
                close(i);
            }

            execvp(execName, &arg_array[firstArg]);

            /* we really can ignore the error here! */
            write(2, execerr, execerrlen) < 0 ? -1 : 0;
            _exit(127);
        }

        /* parent */
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

    JimFreeEnv(interp, environ, orig_environ);
    environ = orig_environ;

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
            if (pidPtr[i] != -1) {
                JimDetachPids(interp, 1, &pidPtr[i]);
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
 * CleanupChildren --
 *
 *  This is a utility procedure used to wait for child processes
 *  to exit, record information about abnormal exits, and then
 *  collect any stderr output generated by them.
 *
 * Results:
 *  The return value is a standard Tcl result.  If anything at
 *  weird happened with the child processes, JIM_ERROR is returned
 *  and a message is left in interp->result.
 *
 * Side effects:
 *  If the last character of interp->result is a newline, then it
 *  is removed.  File errorId gets closed, and pidPtr is freed
 *  back to the storage allocator.
 *
 *----------------------------------------------------------------------
 */

static int Jim_CleanupChildren(Jim_Interp *interp, int numPids, int *pidPtr, int errorId)
{
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);
    int result = JIM_OK;
    int i;

    for (i = 0; i < numPids; i++) {
        int waitStatus = 0;
        int pid = JimWaitPids(table, 1, &pidPtr[i], &waitStatus);

        if (pid >= 0 && JimCheckWaitStatus(interp, pid, waitStatus) != JIM_OK) {
            result = JIM_ERR;
        }
    }
    Jim_Free(pidPtr);

    /*
     * Read the standard error file.  If there's anything there,
     * then add the file's contents to the result
     * string.
     */
    if (errorId >= 0) {
        if (JimAppendStreamToString(interp, errorId, Jim_GetResult(interp)) != JIM_OK) {
            Jim_SetResultErrno(interp, "error reading from stderr output file");
            result = JIM_ERR;
        }
        close(errorId);
    }

    JimTrimTrailingNewline(interp);

    return result;
}

int Jim_execInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "exec", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "exec", Jim_ExecCmd, JimAllocWaitInfoTable(), JimFreeWaitInfoTable);
    return JIM_OK;
}
