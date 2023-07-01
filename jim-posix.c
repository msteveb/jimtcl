/*
 * Jim - POSIX extension
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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "jimautoconf.h"
#include <jim.h>

#ifdef HAVE_SYS_SYSINFO_H
#include <sys/sysinfo.h>
#endif

static void Jim_PosixSetError(Jim_Interp *interp)
{
    Jim_SetResultString(interp, strerror(errno), -1);
}

#if defined(HAVE_FORK)
static int Jim_PosixForkCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    pid_t pid;

    JIM_NOTUSED(argv);

    if ((pid = fork()) == -1) {
        Jim_PosixSetError(interp);
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, (jim_wide) pid);
    return JIM_OK;
}
#endif

#if !defined(JIM_BOOTSTRAP)
static int Jim_PosixGetidsCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objv[8];

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
static int Jim_PosixGethostnameCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char *buf;
    int rc = JIM_OK;

    buf = Jim_Alloc(JIM_HOST_NAME_MAX);
    if (gethostname(buf, JIM_HOST_NAME_MAX) == -1) {
        Jim_PosixSetError(interp);
        Jim_Free(buf);
        rc = JIM_ERR;
    }
    else {
        Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, buf, -1));
    }
    return rc;
}

static int Jim_PosixUptimeCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
#ifdef HAVE_STRUCT_SYSINFO_UPTIME
    struct sysinfo info;

    if (sysinfo(&info) == -1) {
        Jim_PosixSetError(interp);
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, info.uptime);
#else
    Jim_SetResultInt(interp, (long)time(NULL));
#endif
    return JIM_OK;
}
#endif /* JIM_BOOTSTRAP */

int Jim_posixInit(Jim_Interp *interp)
{
    Jim_PackageProvideCheck(interp, "posix");
#ifdef HAVE_FORK
    Jim_RegisterSimpleCmd(interp, "os.fork", "", 0, 0, Jim_PosixForkCommand);
#endif
#if !defined(JIM_BOOTSTRAP)
    Jim_RegisterSimpleCmd(interp, "os.gethostname", "", 0, 0, Jim_PosixGethostnameCommand);
    Jim_RegisterSimpleCmd(interp, "os.getids", "", 0, 0, Jim_PosixGetidsCommand);
    Jim_RegisterSimpleCmd(interp, "os.uptime", "", 0, 0, Jim_PosixUptimeCommand);
#endif /* JIM_BOOTSTRAP */
    return JIM_OK;
}
