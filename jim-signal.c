/*
 * jim-signal.c
 *
 */

#include <signal.h>
#include <string.h>
#include <ctype.h>

#include "jimautoconf.h"
#ifdef HAVE_UNISTD_H
    #include <unistd.h>
#endif
#include <jim-subcmd.h>
#include <jim-signal.h>

#define MAX_SIGNALS_WIDE (sizeof(jim_wide) * 8)
#if defined(NSIG)
    #define MAX_SIGNALS (int)((NSIG < MAX_SIGNALS_WIDE) ? NSIG : MAX_SIGNALS_WIDE)
#else
    #define MAX_SIGNALS (int)MAX_SIGNALS_WIDE
#endif

static jim_wide *sigloc;
static jim_wide sigsignored;
static struct sigaction *sa_old;
static struct {
    int status;
    const char *name;
} siginfo[MAX_SIGNALS];

/* Make sure to do this as a wide, not int */
#define sig_to_bit(SIG) ((jim_wide)1 << (SIG))

static void signal_handler(int sig)
{
    /* Remember which signals occurred and store in *sigloc.
     * Jim_Eval() will notice this as soon as it can and throw an error
     */
    *sigloc |= sig_to_bit(sig);
}

static void signal_ignorer(int sig)
{
    /* Remember which signals occurred for access by 'signal check' */
    sigsignored |= sig_to_bit(sig);
}

static void signal_init_names(void)
{
#define SET_SIG_NAME(SIG) siginfo[SIG].name = #SIG

    SET_SIG_NAME(SIGABRT);
    SET_SIG_NAME(SIGALRM);
    SET_SIG_NAME(SIGBUS);
    SET_SIG_NAME(SIGCHLD);
    SET_SIG_NAME(SIGCONT);
    SET_SIG_NAME(SIGFPE);
    SET_SIG_NAME(SIGHUP);
    SET_SIG_NAME(SIGILL);
    SET_SIG_NAME(SIGINT);
#ifdef SIGIO
    SET_SIG_NAME(SIGIO);
#endif
    SET_SIG_NAME(SIGKILL);
    SET_SIG_NAME(SIGPIPE);
    SET_SIG_NAME(SIGPROF);
    SET_SIG_NAME(SIGQUIT);
    SET_SIG_NAME(SIGSEGV);
    SET_SIG_NAME(SIGSTOP);
    SET_SIG_NAME(SIGSYS);
    SET_SIG_NAME(SIGTERM);
    SET_SIG_NAME(SIGTRAP);
    SET_SIG_NAME(SIGTSTP);
    SET_SIG_NAME(SIGTTIN);
    SET_SIG_NAME(SIGTTOU);
    SET_SIG_NAME(SIGURG);
    SET_SIG_NAME(SIGUSR1);
    SET_SIG_NAME(SIGUSR2);
    SET_SIG_NAME(SIGVTALRM);
    SET_SIG_NAME(SIGWINCH);
    SET_SIG_NAME(SIGXCPU);
    SET_SIG_NAME(SIGXFSZ);
#ifdef SIGPWR
    SET_SIG_NAME(SIGPWR);
#endif
#ifdef SIGCLD
    SET_SIG_NAME(SIGCLD);
#endif
#ifdef SIGEMT
    SET_SIG_NAME(SIGEMT);
#endif
#ifdef SIGLOST
    SET_SIG_NAME(SIGLOST);
#endif
#ifdef SIGPOLL
    SET_SIG_NAME(SIGPOLL);
#endif
#ifdef SIGINFO
    SET_SIG_NAME(SIGINFO);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SignalId --
 *
 *      Return a textual identifier for a signal number.
 *
 * Results:
 *      This procedure returns a machine-readable textual identifier
 *      that corresponds to sig.  The identifier is the same as the
 *      #define name in signal.h.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
const char *Jim_SignalId(int sig)
{
    if (sig >=0 && sig < MAX_SIGNALS) {
        if (siginfo[sig].name) {
            return siginfo[sig].name;
        }
    }
    return "unknown signal";
}

/**
 * Given the name of a signal, returns the signal value if found,
 * or returns -1 (and sets an error) if not found.
 * We accept -SIGINT, SIGINT, INT or any lowercase version or a number,
 * either positive or negative.
 */
static int find_signal_by_name(Jim_Interp *interp, const char *name)
{
    int i;
    const char *pt = name;

    /* Remove optional - and SIG from the front of the name */
    if (*pt == '-') {
        pt++;
    }
    if (strncasecmp(name, "sig", 3) == 0) {
        pt += 3;
    }
    if (isdigit(UCHAR(pt[0]))) {
        i = atoi(pt);
        if (i > 0 && i < MAX_SIGNALS) {
            return i;
        }
    }
    else {
        for (i = 1; i < MAX_SIGNALS; i++) {
            /* Jim_SignalId() returns names such as SIGINT, and
             * returns "unknown signal" if unknown, so this will work
             */
            if (strcasecmp(Jim_SignalId(i) + 3, pt) == 0) {
                return i;
            }
        }
    }
    Jim_SetResultFormatted(interp, "unknown signal %s", name);

    return -1;
}

#define SIGNAL_ACTION_HANDLE 1
#define SIGNAL_ACTION_IGNORE -1
#define SIGNAL_ACTION_BLOCK -2
#define SIGNAL_ACTION_DEFAULT 0

static int do_signal_cmd(Jim_Interp *interp, int action, int argc, Jim_Obj *const *argv)
{
    struct sigaction sa;
    int i;

    if (argc == 0) {
        Jim_SetResult(interp, Jim_NewListObj(interp, NULL, 0));
        for (i = 1; i < MAX_SIGNALS; i++) {
            if (siginfo[i].status == action) {
                /* Add signal name to the list  */
                Jim_ListAppendElement(interp, Jim_GetResult(interp),
                    Jim_NewStringObj(interp, Jim_SignalId(i), -1));
            }
        }
        return JIM_OK;
    }

    /* Catch all the signals we care about */
    if (action != SIGNAL_ACTION_DEFAULT) {
        memset(&sa, 0, sizeof(sa));
        if (action == SIGNAL_ACTION_HANDLE) {
            sa.sa_handler = signal_handler;
        }
        else if (action == SIGNAL_ACTION_IGNORE) {
            sa.sa_handler = signal_ignorer;
        }
        else {
            /* SIGNAL_ACTION_BLOCK */
            sa.sa_handler = SIG_IGN;
        }
    }

    /* Iterate through the provided signals */
    for (i = 0; i < argc; i++) {
        int sig = find_signal_by_name(interp, Jim_String(argv[i]));

        if (sig < 0) {
            return JIM_ERR;
        }
        if (action != siginfo[sig].status) {
            /* Need to change the action for this signal */
            switch (action) {
                case SIGNAL_ACTION_BLOCK:
                case SIGNAL_ACTION_HANDLE:
                case SIGNAL_ACTION_IGNORE:
                    if (siginfo[sig].status == SIGNAL_ACTION_DEFAULT) {
                        if (!sa_old) {
                            /* Allocate the structure the first time through */
                            sa_old = Jim_Alloc(sizeof(*sa_old) * MAX_SIGNALS);
                        }
                        sigaction(sig, &sa, &sa_old[sig]);
                    }
                    else {
                        sigaction(sig, &sa, 0);
                    }
                    break;

                case SIGNAL_ACTION_DEFAULT:
                    /* Restore old handler */
                    if (sa_old) {
                        sigaction(sig, &sa_old[sig], 0);
                    }
            }
            siginfo[sig].status = action;
        }
    }

    return JIM_OK;
}

static int signal_cmd_handle(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return do_signal_cmd(interp, SIGNAL_ACTION_HANDLE, argc, argv);
}

static int signal_cmd_ignore(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return do_signal_cmd(interp, SIGNAL_ACTION_IGNORE, argc, argv);
}

static int signal_cmd_block(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return do_signal_cmd(interp, SIGNAL_ACTION_BLOCK, argc, argv);
}

static int signal_cmd_default(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return do_signal_cmd(interp, SIGNAL_ACTION_DEFAULT, argc, argv);
}

static int signal_set_sigmask_result(Jim_Interp *interp, jim_wide sigmask)
{
    int i;
    Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);

    for (i = 0; i < MAX_SIGNALS; i++) {
        if (sigmask & sig_to_bit(i)) {
            Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, Jim_SignalId(i), -1));
        }
    }
    Jim_SetResult(interp, listObj);
    return JIM_OK;
}

static int signal_cmd_check(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int clear = 0;
    jim_wide mask = 0;
    jim_wide ignored;

    if (argc > 0 && Jim_CompareStringImmediate(interp, argv[0], "-clear")) {
        clear++;
    }
    if (argc > clear) {
        int i;

        /* Signals specified */
        for (i = clear; i < argc; i++) {
            int sig = find_signal_by_name(interp, Jim_String(argv[i]));

            if (sig < 0 || sig >= MAX_SIGNALS) {
                return JIM_ERR;
            }
            mask |= sig_to_bit(sig);
        }
    }
    else {
        /* No signals specified, so check/clear all */
        mask = ~mask;
    }

    /* Be careful we don't have a race condition where signals are cleared but not returned */
    ignored = sigsignored & mask;
    if (clear) {
        sigsignored &= ~ignored;
    }
    /* Set the result */
    signal_set_sigmask_result(interp, ignored);
    return JIM_OK;
}

static int signal_cmd_throw(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int sig = SIGINT;

    if (argc == 1) {
        if ((sig = find_signal_by_name(interp, Jim_String(argv[0]))) < 0) {
            return JIM_ERR;
        }
    }

    /* If the signal is ignored ... */
    if (siginfo[sig].status == SIGNAL_ACTION_IGNORE) {
        sigsignored |= sig_to_bit(sig);
        return JIM_OK;
    }

    /* Just set the signal */
    interp->sigmask |= sig_to_bit(sig);

    /* Set the canonical name of the signal as the result */
    Jim_SetResultString(interp, Jim_SignalId(sig), -1);

    /* And simply say we caught the signal */
    return JIM_SIGNAL;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Jim_SignalCmd --
 *     Implements the TCL signal command:
 *         signal handle|ignore|default|throw ?signals ...?
 *         signal throw signal
 *
 *     Specifies which signals are handled by Tcl code.
 *     If the one of the given signals is caught, it causes a JIM_SIGNAL
 *     exception to be thrown which can be caught by catch.
 *
 *     Use 'signal ignore' to ignore the signal(s)
 *     Use 'signal default' to go back to the default behaviour
 *     Use 'signal throw signal' to raise the given signal
 *
 *     If no arguments are given, returns the list of signals which are being handled
 *
 * Results:
 *      Standard TCL results.
 *
 *-----------------------------------------------------------------------------
 */
static const jim_subcmd_type signal_command_table[] = {
    {   "handle",
        "?signals ...?",
        signal_cmd_handle,
        0,
        -1,
        /* Description: Lists handled signals, or adds to handled signals */
    },
    {   "ignore",
        "?signals ...?",
        signal_cmd_ignore,
        0,
        -1,
        /* Description: Lists ignored signals, or adds to ignored signals */
    },
    {   "block",
        "?signals ...?",
        signal_cmd_block,
        0,
        -1,
        /* Description: Lists blocked signals, or adds to blocked signals */
    },
    {   "default",
        "?signals ...?",
        signal_cmd_default,
        0,
        -1,
        /* Description: Lists defaulted signals, or adds to defaulted signals */
    },
    {   "check",
        "?-clear? ?signals ...?",
        signal_cmd_check,
        0,
        -1,
        /* Description: Returns ignored signals which have occurred, and optionally clearing them */
    },
    {   "throw",
        "?signal?",
        signal_cmd_throw,
        0,
        1,
        /* Description: Raises the given signal (default SIGINT) */
    },
    { NULL }
};

/**
 * Restore default signal handling.
 */
static void JimSignalCmdDelete(Jim_Interp *interp, void *privData)
{
    int i;
    if (sa_old) {
        for (i = 1; i < MAX_SIGNALS; i++) {
            if (siginfo[i].status != SIGNAL_ACTION_DEFAULT) {
                sigaction(i, &sa_old[i], 0);
                siginfo[i].status = SIGNAL_ACTION_DEFAULT;
            }
        }
    }
    Jim_Free(sa_old);
    sa_old = NULL;
    sigloc = NULL;
}

static int Jim_AlarmCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int ret;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "seconds");
        return JIM_ERR;
    }
    else {
#ifdef HAVE_UALARM
        double t;

        ret = Jim_GetDouble(interp, argv[1], &t);
        if (ret == JIM_OK) {
            if (t < 1) {
                ualarm(t * 1e6, 0);
            }
            else {
                alarm(t);
            }
        }
#else
        long t;

        ret = Jim_GetLong(interp, argv[1], &t);
        if (ret == JIM_OK) {
            alarm(t);
        }
#endif
    }

    return ret;
}

static int Jim_SleepCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int ret;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "seconds");
        return JIM_ERR;
    }
    else {
        double t;

        ret = Jim_GetDouble(interp, argv[1], &t);
        if (ret == JIM_OK) {
#ifdef HAVE_USLEEP
            usleep((int)((t - (int)t) * 1e6));
#endif
            sleep(t);
        }
    }

    return ret;
}

static int Jim_KillCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int sig;
    long pid;
    Jim_Obj *pidObj;
    const char *signame;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "?SIG|-0? pid");
        return JIM_ERR;
    }

    if (argc == 2) {
        sig = SIGTERM;
        pidObj = argv[1];
    }
    else {
        signame = Jim_String(argv[1]);
        pidObj = argv[2];

        /* Special 'kill -0 pid' to determine if a pid exists */
        if (strcmp(signame, "-0") == 0 || strcmp(signame, "0") == 0) {
            sig = 0;
        }
        else {
            sig = find_signal_by_name(interp, signame);
            if (sig < 0) {
                return JIM_ERR;
            }
        }
    }

    if (Jim_GetLong(interp, pidObj, &pid) != JIM_OK) {
        return JIM_ERR;
    }

    if (kill(pid, sig) == 0) {
        return JIM_OK;
    }

    Jim_SetResultString(interp, "kill: Failed to deliver signal", -1);
    return JIM_ERR;
}

int Jim_signalInit(Jim_Interp *interp)
{
    Jim_PackageProvideCheck(interp, "signal");
    Jim_CreateCommand(interp, "alarm", Jim_AlarmCmd, 0, 0);
    Jim_CreateCommand(interp, "kill", Jim_KillCmd, 0, 0);
    /* Sleep is slightly dubious here */
    Jim_CreateCommand(interp, "sleep", Jim_SleepCmd, 0, 0);

    /* Teach the jim core how to set a result from a sigmask */
    interp->signal_set_result = signal_set_sigmask_result;

    /* Currently only the top level interp supports signals */
    if (!sigloc) {
        signal_init_names();

        /* Make sure we know where to store the signals which occur */
        sigloc = &interp->sigmask;

        Jim_CreateCommand(interp, "signal", Jim_SubCmdProc, (void *)signal_command_table, JimSignalCmdDelete);
    }

    return JIM_OK;
}
