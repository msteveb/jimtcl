/* Jim - POSIX extension
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
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

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define JIM_EXTENSION
#include "jim.h"

static void Jim_PosixSetError(Jim_Interp *interp)
{
    Jim_SetResultString(interp, strerror(errno), -1);
}

static int Jim_PosixForkCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    pid_t pid;
    JIM_NOTUSED(argv);
    
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    if ((pid = fork()) == -1) {
        Jim_SetResultString(interp, strerror(errno), -1);
        return JIM_ERR;
    }
    Jim_SetResult(interp, Jim_NewIntObj(interp, (jim_wide)pid));
    return JIM_OK;
}

static int Jim_PosixSleepCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    long longValue;
    
    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "?seconds?");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, argv[1], &longValue) != JIM_OK)
        return JIM_ERR;
    sleep(longValue);
    return JIM_OK;
}

static int Jim_PosixGetidsCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    Jim_Obj *objv[8];
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    objv[0] = Jim_NewStringObj(interp, "uid", -1);
    objv[1] = Jim_NewIntObj(interp, getuid());
    objv[2] = Jim_NewStringObj(interp, "euid", -1);
    objv[3] = Jim_NewIntObj(interp, geteuid());
    objv[4] = Jim_NewStringObj(interp, "gid", -1);
    objv[5] = Jim_NewIntObj(interp, getgid());
    objv[6] = Jim_NewStringObj(interp, "egid", -1);
    objv[7] = Jim_NewIntObj(interp, getegid());
    Jim_SetResult(interp, Jim_NewListObj(interp, objv, 8));
    return JIM_OK;
}

#define JIM_HOST_NAME_MAX 1024
static int Jim_PosixGethostnameCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    char buf[JIM_HOST_NAME_MAX];

    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    if (gethostname(buf, JIM_HOST_NAME_MAX) == -1) {
        Jim_PosixSetError(interp);
        return JIM_ERR;
    }
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

static int Jim_PosixSethostnameCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    const char *hostname;
    int len;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "hostname");
        return JIM_ERR;
    }
    hostname = Jim_GetString(argv[1], &len);
    if (sethostname(hostname, len) == -1) {
        Jim_PosixSetError(interp);
        return JIM_ERR;
    }
    return JIM_OK;
}
// added Commands, some linux specific

// uses POSIX.1b
#define S_NS 1000000000
#define S_US    1000000
#define MSNS    1000000
#define USNS       1000
#define NSNS          1

static int Jim_LinuxUSleepCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    int mult = NSNS ;
    double floatValue ;
    const char *units ;
    long long delay ;
    int len ;
    struct timespec tv;
	
    switch (argc) { 
    case 3: 
           units = Jim_GetString(argv[2], &len);
	   if (units == NULL)
               return JIM_ERR;
	   switch (*units) {
	   case 's': mult = S_NS; break;
	   case 'm': mult = MSNS; break;
	   case 'u': mult = USNS; break;
	   case 'n': mult = NSNS; break;
	   default:
               Jim_WrongNumArgs(interp, 1, argv, "arg3: ms us ns or empty");
	       return JIM_ERR;
	   }
	// fallthrough
    case 2: if (Jim_GetDouble(interp, argv[1], &floatValue) != JIM_OK)
               return JIM_ERR;
	   delay = (long long)(floatValue * mult);
           break;
    default:
           Jim_WrongNumArgs(interp, 1, argv, "?useconds?");
           return JIM_ERR;
    }
    tv.tv_sec = delay / S_NS;
    tv.tv_nsec = delay % S_NS;
    fprintf(stderr,"delay: %lld mult: %d  sec: %ld ns: %ld\n", delay, mult, tv.tv_sec,tv.tv_nsec );
    nanosleep(&tv,NULL);
    return JIM_OK;
}

static int Jim_PointInTimeCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    struct timezone tz = { 0, 0 };
    double pit ;
    struct timeval tv;

    gettimeofday(&tv,&tz);
    pit = (double)tv.tv_usec;
    pit /= (double)S_US;
    pit +=(double)tv.tv_sec;
    
    Jim_SetResult(interp, Jim_NewDoubleObj(interp, pit));
    return JIM_OK;
}

static int Jim_PointInTimeJulianCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    struct timezone tz = { 0, 0 };
    double pit ;
    struct timeval tv;

    gettimeofday(&tv,&tz);
    pit  = (double)tv.tv_usec;
    pit /= (double)S_US;
    pit += (double)tv.tv_sec;
    pit /= (double)( 60 * 60 * 24 );
    pit +=  2440587.5;
    
    Jim_SetResult(interp, Jim_NewDoubleObj(interp, pit));
    return JIM_OK;
}

#if 0
// signal stuff
// signal <signame>

    static const char *signames[] = {
	"SIGHUP", 	"SIGINT", 	"SIGQUIT", 	"SIGILL", 
	"SIGABRT", 	"SIGFPE", 	"SIGKILL",	"SIGSEGV",
	"SIGPIPE",	"SIGALRM", 	"SIGTERM",
	"SIGUSR1", 	"SIGUSR2",	
	"SIGCHLD",	"SIGCONT",	"SIGSTOP", 
	"SIGTSTP",	"SIGTTIN"	"SIGTTOU",	
	"SIGBUS",
#ifdef SIGPOLL
        "SIGPOLL",
#endif
        "SIGPROF",	"SIGSYS",	
	"SIGTRAP",	"SIGURG",	"SIGVTALRM",	"SIGXCPU",
	"SIGXFSZ",	
	"SIGIOT",	
#ifdef SIGEMT
	"SIGEMT",	
#endif
	"SIGSTKFLT",	"SIGIO",
	"SIGCLD",	"SIGPWR",
#ifdef SIGINFO
	"SIGINFO",
#endif
#ifdef SIGLOST
	"SIGLOST",
#endif
	"SIGWINCH",	"SIGUNUSED",
	"SIGRT32",	"SIGRT33",	"SIGRT34",	"SIGRT35",	
	"SIGRT36",      "SIGRT37",      "SIGRT38",      "SIGRT39",
	NULL
    } ;
static	int signums[] =  {
	SIGHUP, 	SIGINT, 	SIGQUIT, 	SIGILL, 
	SIGABRT, 	SIGFPE, 	SIGKILL,	SIGSEGV,
	SIGPIPE,	SIGALRM, 	SIGTERM,	
	SIGUSR1, 	SIGUSR2,	
	SIGCHLD,	SIGCONT,	SIGSTOP, 
	SIGTSTP,	SIGTTIN,	SIGTTOU,	
	SIGBUS,
#ifdef SIGPOLL
        SIGPOLL,
#endif
        SIGPROF,	SIGSYS,	
	SIGTRAP,	SIGURG,	SIGVTALRM,	SIGXCPU,
	SIGXFSZ,	
	SIGIOT,	
#ifdef SIGEMT
	SIGEMT,	
#endif
	SIGSTKFLT,	SIGIO,
	SIGCLD,	SIGPWR,	
#ifdef SIGINFO
	SIGINFO,	
#endif
#ifdef SIGLOST
	SIGLOST,
#endif
	SIGWINCH,	SIGUNUSED,
#ifdef SIGRT
	SIGRT32,	SIGRT33,	SIGRT34,	SIGRT35,	
	SIGRT36,	SIGRT37,	SIGRT38,	SIGRT39,
#endif
	0
    } ;

#if 0
enum {      
	HUP, 	INT, 	QUIT, 	ILL, 
	ABRT, 	FPE, 	KILL,	SEGV,
	PIPE,	ALRM, 	TERM,	
	USR1, 	USR2,	
	CHLD,	CONT,	STOP, 
	TSTP,	TTIN,	TTOU,	
	BUS,	POLL,	PROF,	SYS,	
	TRAP,	URG,	VTALRM,	XCPU,
	XFSZ,	
	IOT,	
#ifdef SIGEMT
	EMT,	
#endif
	STKFLT,	IO,
	CLD,	PWR,	
#ifdef SIGINFO
	INFO,
#endif
#ifdef SIGLOST
	LOST,
#endif
	WINCH,	UNUSED,
#ifdef SIGRT
	RT32,	RT33,	RT34,	RT35,	
	RT36,	RT37,	RT38,	RT39,
#endif
	ISEND
    } ;
#endif
#endif

static void Jim_Posix_SigHandler(int signal)
{
#if 0
    int i;
    for (i=0; i<sizeof(signums)/sizeof(*signums) && (signums[i] != signal);i++) ;
#endif
    //fprintf(stderr,"signal %d %s\n", signal,signames[i]);
    fprintf(stderr,"signal %d\n", signal);
}

typedef void (*sighandler_t)(int);

static int Jim_PosixSignalCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    int sig;
    int signum;
    sighandler_t lastaction;
    sighandler_t nextaction = SIG_DFL;
    const char *op;
    int strlen = 0;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "signame ?action ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], signames, &sig, "Signal Names",
                JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;

    signum = signums[sig];

    switch (argc) {
    case 3:
	if (op = Jim_GetString(argv[2], &strlen),strlen == 0) {
        	return JIM_ERR;
        }
	if (strcmp("default",op) == 0) {
		nextaction = SIG_DFL;
	} else if (strcmp("ignore",op) == 0) {
		nextaction = SIG_IGN;
	} else if (strcmp("debug",op) == 0) {
		nextaction = Jim_Posix_SigHandler;
	} else {
		// this is the place to insert a script! UK
	}
	// fall through to query case:
    case 2:
	lastaction = signal(signum, nextaction);
	if (argc==2)
		signal(signum, lastaction);
	if (lastaction == SIG_ERR) {
		return JIM_ERR;
	}
	if (lastaction == SIG_DFL) {
		Jim_SetResultString(interp, "default", -1);
		return JIM_OK;
	}
	if (lastaction == SIG_IGN) {
		Jim_SetResultString(interp, "ignore", -1);
		return JIM_OK;
	} 
	Jim_SetResultString(interp, "function", -1);
	return JIM_OK;
    }

    return JIM_OK;
}
	

	
// end added 
int Jim_posixInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "posix", "1.0", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    Jim_CreateCommand(interp, "os.fork", Jim_PosixForkCommand, NULL, NULL);
    Jim_CreateCommand(interp, "os.sleep", Jim_PosixSleepCommand, NULL, NULL);
    Jim_CreateCommand(interp, "os.getids", Jim_PosixGetidsCommand, NULL, NULL);
    Jim_CreateCommand(interp, "os.gethostname", Jim_PosixGethostnameCommand, NULL, NULL);
    Jim_CreateCommand(interp, "os.sethostname", Jim_PosixSethostnameCommand, NULL, NULL);
    Jim_CreateCommand(interp, "os.usleep", Jim_LinuxUSleepCommand, NULL, NULL);
    Jim_CreateCommand(interp, "os.signal", Jim_PosixSignalCommand, NULL, NULL);
    Jim_CreateCommand(interp, "pit",   Jim_PointInTimeCommand, NULL, NULL);
    Jim_CreateCommand(interp, "Jpit",   Jim_PointInTimeJulianCommand, NULL, NULL);
    return JIM_OK;
}
