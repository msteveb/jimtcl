/* Jim - ANSI I/O extension
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 *
 * $Id: jim-aio.c,v 1.4 2005/03/06 08:31:42 antirez Exp $
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
    int keepOpen; /* If set, the file is not fclosed on cleanup (stdin, ...) */
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
    int option;
    const char *options[] = {
        "close", "seek", "tell", "gets", "puts", "flush", NULL
    };
    enum {OPT_CLOSE, OPT_SEEK, OPT_TELL, OPT_GETS, OPT_PUTS, OPT_FLUSH};

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], options, &option, "AIO method",
                JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    /* CLOSE */
    if (option == OPT_CLOSE) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_DeleteCommand(interp, Jim_GetString(argv[0], NULL));
        return JIM_OK;
    } else if (option == OPT_SEEK) {
    /* SEEK */
        int orig = SEEK_SET;
        long offset;

        if (argc != 3 && argc != 4) {
            Jim_WrongNumArgs(interp, 2, argv, "offset ?origin?");
            return JIM_ERR;
        }
        if (argc == 4) {
            if (Jim_CompareStringImmediate(interp, argv[3], "start"))
                orig = SEEK_SET;
            else if (Jim_CompareStringImmediate(interp, argv[3], "current"))
                orig = SEEK_CUR;
            else if (Jim_CompareStringImmediate(interp, argv[3], "end"))
                orig = SEEK_END;
            else {
                Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
                Jim_AppendStrings(interp, Jim_GetResult(interp),
                        "bad origin \"", Jim_GetString(argv[3], NULL),
                        "\" must be: start, current, or end", NULL);
                return JIM_ERR;
            }
        }
        if (Jim_GetLong(interp, argv[2], &offset) != JIM_OK)
            return JIM_ERR;
        if (fseek(af->fp, offset, orig) == -1) {
            JimAioSetError(interp);
            return JIM_ERR;
        }
        return JIM_OK;
    } else if (option == OPT_TELL) {
    /* TELL */
        long position;

        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        position = ftell(af->fp);
        Jim_SetResult(interp, Jim_NewIntObj(interp, position));
        return JIM_OK;
    } else if (option == OPT_GETS) {
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
    } else if (option == OPT_PUTS) {
    /* PUTS */
        unsigned int wlen;
        const char *wdata;

        if (argc != 3 && (argc != 4 || !Jim_CompareStringImmediate(
                        interp, argv[2], "-nonewline"))) {
            Jim_WrongNumArgs(interp, 2, argv, "?-nonewline? string");
            return JIM_ERR;
        }
        wdata = Jim_GetString(argv[2+(argc==4)], &wlen);
        if (fwrite(wdata, 1, wlen, af->fp) != wlen ||
            (argc == 3 && fwrite("\n", 1, 1, af->fp) != 1)) {
            JimAioSetError(interp);
            return JIM_ERR;
        }
        return JIM_OK;
    } else if (option  == OPT_FLUSH) {
    /* FLUSH */
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        if (fflush(af->fp) == EOF) {
            JimAioSetError(interp);
            return JIM_ERR;
        }
        return JIM_OK;
    }
    return JIM_OK;
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
