/* 
 * jim-signal.c
 *
 */

#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "jim.h"
#include "jim-subcmd.h"

#define MAX_SIGNALS 32

static int *sigloc;
static unsigned long sigsblocked; 
static struct sigaction *sa_old;
static int signal_handling[MAX_SIGNALS];

static void signal_handler(int sig)
{
    /* We just remember which signal occurred. Jim_Eval() will
     * notice this as soon as it can and throw an error
     */
    *sigloc = sig;
}

static void signal_ignorer(int sig)
{
    /* We just remember which signals occurred */
    sigsblocked |= (1 << sig);
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
    switch (sig) {
#ifdef SIGABRT
        case SIGABRT: return "SIGABRT";
#endif
#ifdef SIGALRM
        case SIGALRM: return "SIGALRM";
#endif
#ifdef SIGBUS
        case SIGBUS: return "SIGBUS";
#endif
#ifdef SIGCHLD
        case SIGCHLD: return "SIGCHLD";
#endif
#if defined(SIGCLD) && (!defined(SIGCHLD) || (SIGCLD != SIGCHLD))
        case SIGCLD: return "SIGCLD";
#endif
#ifdef SIGCONT
        case SIGCONT: return "SIGCONT";
#endif
#if defined(SIGEMT) && (!defined(SIGXCPU) || (SIGEMT != SIGXCPU))
        case SIGEMT: return "SIGEMT";
#endif
#ifdef SIGFPE
        case SIGFPE: return "SIGFPE";
#endif
#ifdef SIGHUP
        case SIGHUP: return "SIGHUP";
#endif
#ifdef SIGILL
        case SIGILL: return "SIGILL";
#endif
#ifdef SIGINT
        case SIGINT: return "SIGINT";
#endif
#ifdef SIGIO
        case SIGIO: return "SIGIO";
#endif
#if defined(SIGIOT) && (!defined(SIGABRT) || (SIGIOT != SIGABRT))
        case SIGIOT: return "SIGIOT";
#endif
#ifdef SIGKILL
        case SIGKILL: return "SIGKILL";
#endif
#if defined(SIGLOST) && (!defined(SIGIOT) || (SIGLOST != SIGIOT)) && (!defined(SIGURG) || (SIGLOST != SIGURG))
        case SIGLOST: return "SIGLOST";
#endif
#ifdef SIGPIPE
        case SIGPIPE: return "SIGPIPE";
#endif
#if defined(SIGPOLL) && (!defined(SIGIO) || (SIGPOLL != SIGIO))
        case SIGPOLL: return "SIGPOLL";
#endif
#ifdef SIGPROF
        case SIGPROF: return "SIGPROF";
#endif
#if defined(SIGPWR) && (!defined(SIGXFSZ) || (SIGPWR != SIGXFSZ))
        case SIGPWR: return "SIGPWR";
#endif
#ifdef SIGQUIT
        case SIGQUIT: return "SIGQUIT";
#endif
#ifdef SIGSEGV
        case SIGSEGV: return "SIGSEGV";
#endif
#ifdef SIGSTOP
        case SIGSTOP: return "SIGSTOP";
#endif
#ifdef SIGSYS
        case SIGSYS: return "SIGSYS";
#endif
#ifdef SIGTERM
        case SIGTERM: return "SIGTERM";
#endif
#ifdef SIGTRAP
        case SIGTRAP: return "SIGTRAP";
#endif
#ifdef SIGTSTP
        case SIGTSTP: return "SIGTSTP";
#endif
#ifdef SIGTTIN
        case SIGTTIN: return "SIGTTIN";
#endif
#ifdef SIGTTOU
        case SIGTTOU: return "SIGTTOU";
#endif
#if defined(SIGURG) && (!defined(SIGIO) || (SIGURG != SIGIO))
        case SIGURG: return "SIGURG";
#endif
#ifdef SIGUSR1
        case SIGUSR1: return "SIGUSR1";
#endif
#ifdef SIGUSR2
        case SIGUSR2: return "SIGUSR2";
#endif
#ifdef SIGVTALRM
        case SIGVTALRM: return "SIGVTALRM";
#endif
#ifdef SIGWINCH
        case SIGWINCH: return "SIGWINCH";
#endif
#ifdef SIGXCPU
        case SIGXCPU: return "SIGXCPU";
#endif
#ifdef SIGXFSZ
        case SIGXFSZ: return "SIGXFSZ";
#endif
#ifdef SIGINFO
        case SIGINFO: return "SIGINFO";
#endif
    }
    return "unknown signal";
}

/**
 * Given the name of a signal, returns the signal value if found,
 * or returns -1 (and sets an error) if not found.
 * We accept -SIGINT, SIGINT, INT or any lowercase version or a number,
 * either positive or negative.
 */
static int
find_signal_by_name(Jim_Interp *interp, const char *name)
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
    if (isdigit(pt[0])) {
        i = atoi(pt);
        if (i > 0 && i < MAX_SIGNALS) {
            return i;
        }
    }
    else {
        for (i = 1; i < MAX_SIGNALS; i++) {
            /* Jim_SignalId() returns names such as SIGINT, and
             * returns "unknown signal id" if unknown, so this will work
             */
            if (strcasecmp(Jim_SignalId(i) + 3, pt) == 0) {
                return i;
            }
        }
    }
    Jim_SetResultString(interp, "unknown signal ", -1);
    Jim_AppendString(interp, Jim_GetResult(interp), name, -1);

    return -1;
}

#define SIGNAL_ACTION_HANDLE 1
#define SIGNAL_ACTION_IGNORE -1
#define SIGNAL_ACTION_DEFAULT 0

static int do_signal_cmd(Jim_Interp *interp, int action, int argc, Jim_Obj *const *argv)
{
    struct sigaction sa;
    int i;

    if (argc == 0) {
        Jim_SetResult(interp, Jim_NewListObj(interp, NULL, 0));
        for (i = 1; i < MAX_SIGNALS; i++) {
            if (signal_handling[i] == action) {
                /* Add signal name to the list  */
                Jim_ListAppendElement(interp, Jim_GetResult(interp),
                    Jim_NewStringObj(interp, Jim_SignalId(i), -1));
            }
        }
        return JIM_OK;
    }

    /* Make sure we know where to store the signals which occur */
    if (!sigloc) {
        sigloc = &interp->signal;
    }

    /* Catch all the signals we care about */
    if (action != SIGNAL_ACTION_DEFAULT) {
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        if (action == SIGNAL_ACTION_HANDLE) {
            sa.sa_handler = signal_handler;
        }
        else {
            sa.sa_handler = signal_ignorer;
        }
    }

    /* Iterate through the provided signals */
    for (i = 0; i < argc; i++) {
        int sig = find_signal_by_name(interp, Jim_GetString(argv[i], NULL));
        if (sig < 0) {
            return JIM_ERR;
        }
        if (action != signal_handling[sig]) {
            /* Need to change the action for this signal */
            switch (action) {
                case SIGNAL_ACTION_HANDLE:
                case SIGNAL_ACTION_IGNORE:
                    if (signal_handling[sig] == SIGNAL_ACTION_DEFAULT) {
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
            signal_handling[sig] = action;
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

static int signal_cmd_default(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return do_signal_cmd(interp, SIGNAL_ACTION_DEFAULT, argc, argv);
}

static int signal_cmd_throw(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int sig = SIGINT;
    if (argc == 1) {
        if ((sig = find_signal_by_name(interp, Jim_GetString(argv[0], NULL))) < 0) {
            return JIM_ERR;
        }
    }

    /* Just set the signal */
    interp->signal = sig;

#if 1
    /* Set the canonical name of the signal as the result */
    Jim_SetResultString(interp, Jim_SignalId(sig), -1);
#endif

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
    {   .cmd = "handle",
        .args = "?signals ...?",
        .function = signal_cmd_handle,
        .minargs = 0,
        .maxargs = -1,
        .description = "Lists handled signals, or adds to handled signals"
    },
    {   .cmd = "ignore",
        .args = "?signals ...?",
        .function = signal_cmd_ignore,
        .minargs = 0,
        .maxargs = -1,
        .description = "Lists ignored signals, or adds to ignored signals"
    },
    {   .cmd = "default",
        .args = "?signals ...?",
        .function = signal_cmd_default,
        .minargs = 0,
        .maxargs = -1,
        .description = "Lists defaulted signals, or adds to defaulted signals"
    },
    {   .cmd = "throw",
        .args = "?signal?",
        .function = signal_cmd_throw,
        .minargs = 0,
        .maxargs = 1,
        .description = "Raises the given signal (default SIGINT)"
    },
    { 0 }
};

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

        ret = Jim_GetLong (interp, argv[1], &t);
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
            if (t < 1) {
                usleep(t * 1e6);
            }
            else {
                sleep(t);
            }
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
        signame = "SIGTERM";
        pidObj = argv[1];
    }
    else {
        signame = Jim_GetString(argv[1], NULL);
        pidObj = argv[2];
    }

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
    if (Jim_PackageProvide(interp, "signal", "1.0", JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }
    /* Teach the jim core how to convert signal values to names */
    interp->signal_to_name = Jim_SignalId;

    Jim_CreateCommand(interp, "signal", Jim_SubCmdProc, (void *)signal_command_table, NULL);
    Jim_CreateCommand(interp, "alarm", Jim_AlarmCmd, 0, 0);
    Jim_CreateCommand(interp, "kill", Jim_KillCmd, 0, 0);

    /* Sleep is slightly dubious here */
    Jim_CreateCommand(interp, "sleep", Jim_SleepCmd, 0, 0);
    return JIM_OK;
}
