/* Jim - POSIX extension
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 *
 * $Id: jim-posix.c,v 1.8 2005/03/04 12:32:21 antirez Exp $
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
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define JIM_EXTENSION
#include "jim.h"

extern int errno;

static void Jim_PosixSetError(Jim_Interp *interp)
{
    Jim_SetResultString(interp, strerror(errno), -1);
}

static int Jim_PosixForkCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    JIM_NOTUSED(argv);
    pid_t pid;
    
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

int Jim_OnLoad(Jim_Interp *interp)
{
    Jim_InitExtension(interp, "1.0");
    Jim_CreateCommand(interp, "os.fork", Jim_PosixForkCommand, NULL);
    Jim_CreateCommand(interp, "os.sleep", Jim_PosixSleepCommand, NULL);
    Jim_CreateCommand(interp, "os.getids", Jim_PosixGetidsCommand, NULL);
    Jim_CreateCommand(interp, "os.gethostname", Jim_PosixGethostnameCommand, NULL);
    Jim_CreateCommand(interp, "os.sethostname", Jim_PosixSethostnameCommand, NULL);
    return JIM_OK;
}
