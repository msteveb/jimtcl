/* Jim - ANSI I/O extension
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 *
 * $Id: jim-aio.c,v 1.1 2005/03/05 15:01:38 antirez Exp $
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

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define JIM_EXTENSION
#include "jim.h"

#define AIO_CMD_LEN 128
#define AIO_BUF_LEN 1024

typedef struct AioFile {
    FILE *fp;
} AioFile;

static void JimAioSetError(Jim_Interp *interp)
{
    Jim_SetResultString(interp, strerror(errno), -1);
}

static void JimAioDelProc(void *privData)
{
    AioFile *af = privData;

    fclose(af->fp);
    Jim_Free(af);
}

/* Calls to [aio.file] create commands that are implemented by this
 * C command. */
static int JimAioHandlerCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }
    /* CLOSE */
    if (Jim_CompareStringImmediate(interp, argv[1], "close")) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_DeleteCommand(interp, Jim_GetString(argv[0], NULL));
        return JIM_OK;
    } else if (Jim_CompareStringImmediate(interp, argv[1], "gets")) {
    /* GETS */
        char buf[AIO_BUF_LEN];
        Jim_Obj *objPtr;

        if (argc != 2 && argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "?varName?");
            return JIM_ERR;
        }
        objPtr = Jim_NewStringObj(interp, NULL, 0);
        while (1) {
            int more = 0;
            buf[AIO_BUF_LEN-1] = '_';
            if (fgets(buf, AIO_BUF_LEN, af->fp) == NULL)
                break;
            if (buf[AIO_BUF_LEN-1] == '\0' && buf[AIO_BUF_LEN] == '\n')
                more = 1;
            if (more) {
                Jim_AppendString(interp, objPtr, buf, AIO_BUF_LEN-1);
            } else {
                /* strip "\n" */
                Jim_AppendString(interp, objPtr, buf, strlen(buf)-1);
            }
            if (!more)
                break;
        }
        if (ferror(af->fp)) {
            /* I/O error */
            Jim_IncrRefCount(objPtr);
            Jim_DecrRefCount(interp, objPtr);
            JimAioSetError(interp);
            return JIM_ERR;
        }
        /* On EOF returns -1 if varName was specified, or the empty string. */
        if (feof(af->fp) && Jim_Length(objPtr) == 0) {
            Jim_IncrRefCount(objPtr);
            Jim_DecrRefCount(interp, objPtr);
            if (argc == 3)
                Jim_SetResult(interp, Jim_NewIntObj(interp, -1));
            return JIM_OK;
        }
        if (argc == 3) {
            int totLen;

            Jim_GetString(objPtr, &totLen);
            if (Jim_SetVariable(interp, argv[2], objPtr) != JIM_OK) {
                Jim_IncrRefCount(objPtr);
                Jim_DecrRefCount(interp, objPtr);
                return JIM_ERR;
            }
            Jim_SetResult(interp, Jim_NewIntObj(interp, totLen));
        } else {
            Jim_SetResult(interp, objPtr);
        }
        return JIM_OK;
    } else {
        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        Jim_AppendStrings(interp, Jim_GetResult(interp),
                "Invalid option for AIO file, should be: "
                "close, gets, read, puts, seek, tell", NULL);
        return JIM_ERR;
    }
}

static int JimAioOpenCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    FILE *fp;
    AioFile *af;
    char buf[AIO_CMD_LEN];
    const char *mode = "r";
    Jim_Obj *objPtr;
    long fileId;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "filename ?mode?");
        return JIM_ERR;
    }
    if (argc == 3)
        mode = Jim_GetString(argv[2], NULL);
    fp = fopen(Jim_GetString(argv[1], NULL), mode);
    if (fp == NULL) {
        JimAioSetError(interp);
        return JIM_ERR;
    }
    /* Get the next file id */
    if (Jim_EvalGlobal(interp,
                "if {[catch {incr aio.fileId}]} {set aio.fileId 0}") != JIM_OK)
        return JIM_ERR;
    objPtr = Jim_GetVariableStr(interp, "aio.fileId", JIM_ERRMSG);
    if (objPtr == NULL) return JIM_ERR;
    if (Jim_GetLong(interp, objPtr, &fileId) != JIM_OK) return JIM_ERR;

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    af->fp = fp;
    sprintf(buf, "aio.handle%ld", fileId);
    Jim_CreateCommand(interp, buf, JimAioHandlerCommand, af, JimAioDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

int Jim_OnLoad(Jim_Interp *interp)
{
    Jim_InitExtension(interp, "1.0");
    Jim_CreateCommand(interp, "aio.open", JimAioOpenCommand, NULL, NULL);
    return JIM_OK;
}
