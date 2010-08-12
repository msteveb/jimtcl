/* Jim - A small embeddable Tcl interpreter
 *
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright 2005 Clemens Hintze <c.hintze@gmx.net>
 * Copyright 2005 patthoyts - Pat Thoyts <patthoyts@users.sf.net> 
 * Copyright 2008 oharboe - o/yvind Harboe - oyvind.harboe@zylin.com
 * Copyright 2008 Andrew Lunn <andrew@lunn.ch>
 * Copyright 2008 Duane Ellis <openocd@duaneellis.com>
 * Copyright 2008 Uwe Klein <uklein@klein-messgeraete.de>
 * 
 * The FreeBSD license
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
 **/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "jim.h"

#ifndef JIM_ANSIC
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include "jim-eventloop.h"
#include "jim-subcmd.h"


#define AIO_CMD_LEN 128
#define AIO_BUF_LEN 1024

#define AIO_KEEPOPEN 1


typedef struct AioFile {
    FILE *fp;
    Jim_Obj *filename;
    int type;
    int OpenFlags; /* AIO_KEEPOPEN? keep FILE* */
    int fd;
#ifdef O_NDELAY
    int flags;
#endif
    Jim_Obj *rEvent;
    Jim_Obj *wEvent;
    Jim_Obj *eEvent;
#ifndef JIM_ANSIC
    struct sockaddr sa;
    struct hostent *he;
#endif
} AioFile;

static int JimAioSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

#ifndef JIM_ANSIC
static int JimParseIpAddress(Jim_Interp *interp, const char *hostport, struct sockaddr_in *sa)
{
    char a[0x20];
    char b[0x20];
    const char* sthost; 
    const char* stport;
    unsigned port;
    struct hostent *he;

    switch (sscanf(hostport,"%[^:]:%[^:]",a,b)) {
        case 2: sthost = a; stport = b; break;
        case 1: sthost = "0.0.0.0"; stport = a; break;
        default:
            return JIM_ERR;
    }
    if (0 == strncmp(sthost,"ANY",3)) {
        sthost = "0.0.0.0";
    }
    port = atol(stport);
    he = gethostbyname(sthost);

    if (!he) {
        Jim_SetResultString(interp,hstrerror(h_errno),-1);
        return JIM_ERR;
    }

    sa->sin_family= he->h_addrtype;
    memcpy(&sa->sin_addr, he->h_addr, he->h_length); /* set address */
    sa->sin_port = htons(port);

    return JIM_OK;
}

static int JimParseDomainAddress(Jim_Interp *interp, const char *path, struct sockaddr_un *sa)
{
    sa->sun_family = PF_UNIX;
    strcpy(sa->sun_path, path);

    return JIM_OK;
}
#endif

static void JimAioSetError(Jim_Interp *interp, Jim_Obj *name)
{
    if (name) {
        Jim_SetResultFormatted(interp, "%#s: %s", name, strerror(errno));
    }
    else {
        Jim_SetResultString(interp, strerror(errno), -1);
    }
}

static void JimAioDelProc(Jim_Interp *interp, void *privData)
{
    AioFile *af = privData;
    JIM_NOTUSED(interp);

    Jim_DecrRefCount(interp, af->filename);

    if (!(af->OpenFlags & AIO_KEEPOPEN)) {
        fclose(af->fp);
    }
#ifdef jim_ext_eventloop
    /* remove existing EventHandlers */
    if (af->rEvent) {
        Jim_DeleteFileHandler(interp,af->fp);
    }
    if (af->wEvent) {
        Jim_DeleteFileHandler(interp,af->fp);
    }
    if (af->eEvent) {
        Jim_DeleteFileHandler(interp,af->fp);
    }
#endif
    Jim_Free(af);
}

static int aio_cmd_read(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char buf[AIO_BUF_LEN];
    Jim_Obj *objPtr;
    int nonewline = 0;
    int neededLen = -1; /* -1 is "read as much as possible" */

    if (argc && Jim_CompareStringImmediate(interp, argv[0], "-nonewline"))
    {
        nonewline = 1;
        argv++;
        argc--;
    }
    if (argc == 1) {
        jim_wide wideValue;
        if (Jim_GetWide(interp, argv[0], &wideValue) != JIM_OK)
            return JIM_ERR;
        if (wideValue < 0) {
            Jim_SetResultString(interp, "invalid parameter: negative len",
                    -1);
            return JIM_ERR;
        }
        neededLen = (int) wideValue;
    }
    else if (argc) {
        return -1;
    }
    objPtr = Jim_NewStringObj(interp, NULL, 0);
    while (neededLen != 0) {
        int retval;
        int readlen;
       
        if (neededLen == -1) {
            readlen = AIO_BUF_LEN;
        } else {
            readlen = (neededLen > AIO_BUF_LEN ? AIO_BUF_LEN : neededLen);
        }
        retval = fread(buf, 1, readlen, af->fp);
        if (retval > 0) {
            Jim_AppendString(interp, objPtr, buf, retval);
            if (neededLen != -1) {
                neededLen -= retval;
            }
        }
        if (retval != readlen) break;
    }
    /* Check for error conditions */
    if (ferror(af->fp)) {
        /* I/O error */
        Jim_FreeNewObj(interp, objPtr);
        JimAioSetError(interp, af->filename);
        return JIM_ERR;
    }
    if (nonewline) {
        int len;
        const char *s = Jim_GetString(objPtr, &len);

        if (len > 0 && s[len-1] == '\n') {
            objPtr->length--;
            objPtr->bytes[objPtr->length] = '\0';
        }
    }
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

static int aio_cmd_gets(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char buf[AIO_BUF_LEN];
    Jim_Obj *objPtr;

    objPtr = Jim_NewStringObj(interp, NULL, 0);
    while (1) {
        int more = 0;
        buf[AIO_BUF_LEN-1] = '_';
        if (fgets(buf, AIO_BUF_LEN, af->fp) == NULL)
            break;
        if (buf[AIO_BUF_LEN-1] == '\0' && buf[AIO_BUF_LEN-2] != '\n')
            more = 1;
        if (more) {
            Jim_AppendString(interp, objPtr, buf, AIO_BUF_LEN-1);
        } else {
            int len = strlen(buf);
            if (len) {
                int hasnl = (buf[len - 1] == '\n');
                /* strip "\n" */
                Jim_AppendString(interp, objPtr, buf, strlen(buf) - hasnl);
            }
        }
        if (!more)
            break;
    }
    if (ferror(af->fp) && (errno != EAGAIN)) {
        /* I/O error */
        Jim_FreeNewObj(interp, objPtr);
        JimAioSetError(interp, af->filename);
        return JIM_ERR;
    }
    /* On EOF returns -1 if varName was specified, or the empty string. */
    if (feof(af->fp) && Jim_Length(objPtr) == 0) {
        Jim_FreeNewObj(interp, objPtr);
        if (argc) {
            Jim_SetResultInt(interp, -1);
        }
        return JIM_OK;
    }
    if (argc) {
        int totLen;

        Jim_GetString(objPtr, &totLen);
        if (Jim_SetVariable(interp, argv[0], objPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, objPtr);
            return JIM_ERR;
        }
        Jim_SetResultInt(interp, totLen);
    }
    else {
        Jim_SetResult(interp, objPtr);
    }
    return JIM_OK;
}

static int aio_cmd_puts(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int wlen;
    const char *wdata;
    Jim_Obj *strObj;

    if (argc == 2) {
        if (!Jim_CompareStringImmediate(interp, argv[0], "-nonewline")) {
            return -1;
        }
        strObj = argv[1];
    }
    else {
        strObj = argv[0];
    }

    wdata = Jim_GetString(strObj, &wlen);
    if (fwrite(wdata, 1, wlen, af->fp) == (unsigned)wlen) {
        if (argc == 2 || putc('\n', af->fp) != EOF) {
            return JIM_OK;
        }
    }
    JimAioSetError(interp, af->filename);
    return JIM_ERR;
}

#ifndef JIM_ANSIC
static int aio_cmd_recvfrom(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char *buf;
    struct sockaddr_in sa;
    long len;
    socklen_t salen = sizeof(sa);
    int rlen;

    if (Jim_GetLong(interp, argv[0], &len) != JIM_OK) {
        return JIM_ERR;
    }

    buf = Jim_Alloc(len + 1);

    rlen = recvfrom(fileno(af->fp), buf, len, 0, (struct sockaddr *)&sa, &salen);
    if (rlen < 0) {
        Jim_Free(buf);
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }
    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, buf, rlen));

    if (argc > 1) {
        char buf[50];

        inet_ntop(sa.sin_family, &sa.sin_addr, buf, sizeof(buf) - 7);
        snprintf(buf + strlen(buf), 7, ":%d", ntohs(sa.sin_port));

        if (Jim_SetVariable(interp, argv[1], Jim_NewStringObj(interp, buf, -1)) != JIM_OK) {
            return JIM_ERR;
        }
    }

    return JIM_OK;
}


static int aio_cmd_sendto(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int wlen;
    int len;
    const char *wdata;
    struct sockaddr_in sa;

    if (JimParseIpAddress(interp, Jim_GetString(argv[1], NULL), &sa) != JIM_OK) {
        return JIM_ERR;
    }
    wdata = Jim_GetString(argv[0], &wlen);

    /* Note that we don't validate the socket type. Rely on sendto() failing if appropriate */
    len = sendto(fileno(af->fp), wdata, wlen, 0, (struct sockaddr*)&sa, sizeof(sa));
    if (len < 0) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, len);
    return JIM_OK;
}

static int aio_cmd_accept(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *serv_af = Jim_CmdPrivData(interp);
    int sock;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    AioFile *af;
    char buf[AIO_CMD_LEN];
    long fileId;
    sock = accept(serv_af->fd,(struct sockaddr*)&serv_af->sa,&addrlen);
    if (sock < 0)
        return JIM_ERR;

    /* Get the next file id */
    fileId = Jim_GetId(interp);

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    af->fd = sock;
    af->filename = Jim_NewStringObj(interp, "accept", -1);
    Jim_IncrRefCount(af->filename);
    af->fp = fdopen(sock,"r+");
    af->OpenFlags = 0;
#ifdef O_NDELAY
    af->flags = fcntl(af->fd,F_GETFL);
#endif
    af->rEvent = NULL;
    af->wEvent = NULL;
    af->eEvent = NULL;
    sprintf(buf, "aio.sockstream%ld", fileId);
    Jim_CreateCommand(interp, buf, JimAioSubCmdProc, af, JimAioDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

#endif

static int aio_cmd_flush(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    if (fflush(af->fp) == EOF) {
        JimAioSetError(interp, af->filename);
        return JIM_ERR;
    }
    return JIM_OK;
}

static int aio_cmd_eof(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    Jim_SetResultInt(interp, feof(af->fp));
    return JIM_OK;
}

static int aio_cmd_close(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_DeleteCommand(interp, Jim_GetString(argv[0], NULL));
    return JIM_OK;
}

static int aio_cmd_seek(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int orig = SEEK_SET;
    long offset;

    if (argc == 2) {
        if (Jim_CompareStringImmediate(interp, argv[1], "start"))
            orig = SEEK_SET;
        else if (Jim_CompareStringImmediate(interp, argv[1], "current"))
            orig = SEEK_CUR;
        else if (Jim_CompareStringImmediate(interp, argv[1], "end"))
            orig = SEEK_END;
        else {
            return -1;
        }
    }
    if (Jim_GetLong(interp, argv[0], &offset) != JIM_OK) {
        return JIM_ERR;
    }
    if (fseek(af->fp, offset, orig) == -1) {
        JimAioSetError(interp, af->filename);
        return JIM_ERR;
    }
    return JIM_OK;
}

static int aio_cmd_tell(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    Jim_SetResultInt(interp, ftell(af->fp));
    return JIM_OK;
}

#ifdef O_NDELAY
static int aio_cmd_ndelay(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    int fmode = af->flags;

    if (argc) {
        long nb;

        if (Jim_GetLong(interp, argv[0], &nb) != JIM_OK) {
            return JIM_ERR;
        }
        if (nb) {
            fmode |=  O_NDELAY;
        }
        else {
            fmode &= ~O_NDELAY;
        }
        fcntl(af->fd, F_SETFL, fmode);
        af->flags = fmode;
    }
    Jim_SetResultInt(interp, (fmode & O_NONBLOCK) ? 1 : 0);
    return JIM_OK;
}
#endif

#ifdef jim_ext_eventloop
static void JimAioFileEventFinalizer(Jim_Interp *interp, void *clientData)
{
    Jim_Obj *objPtr = clientData;

    Jim_DecrRefCount(interp, objPtr);
}

static int JimAioFileEventHandler(Jim_Interp *interp, void *clientData, int mask)
{
    Jim_Obj *objPtr = clientData;
    Jim_Obj *scrPtr = NULL ;
    if (mask == (JIM_EVENT_READABLE | JIM_EVENT_FEOF)) {
        Jim_ListIndex(interp, objPtr, 1, &scrPtr, 0);
    }
    else {
        Jim_ListIndex(interp, objPtr, 0, &scrPtr, 0);
    }
    Jim_EvalObjBackground(interp, scrPtr);
    return 0;
}

static int aio_eventinfo(Jim_Interp *interp, AioFile *af, unsigned mask, Jim_Obj **scriptListObj, Jim_Obj *script1, Jim_Obj *script2)
{
    int scriptlen = 0;

    if (script1 == NULL) {
        /* Return current script */
        if (*scriptListObj) {
            Jim_SetResult(interp, *scriptListObj);
        }
        return JIM_OK;
    }

    if (*scriptListObj) {
        /* Delete old handler */
        Jim_DeleteFileHandler(interp, af->fp);
        *scriptListObj = NULL;
    }

    /* Now possibly add the new script(s) */
    Jim_GetString(script1, &scriptlen);
    if (scriptlen == 0) {
        /* Empty script, so done */
        return JIM_OK;
    }

    /* A new script to add */
    *scriptListObj = Jim_NewListObj(interp, NULL, 0);
    Jim_IncrRefCount(*scriptListObj);

    if (Jim_IsShared(script1)) {
        script1 = Jim_DuplicateObj(interp, script1);
    }
    Jim_ListAppendElement(interp, *scriptListObj, script1);

    if (script2) {
        if (Jim_IsShared(script2)) {
            script2 = Jim_DuplicateObj(interp, script2);
        }
        Jim_ListAppendElement(interp, *scriptListObj, script2);
    }

    Jim_CreateFileHandler(interp, af->fp, mask, 
        JimAioFileEventHandler,
        *scriptListObj,
        JimAioFileEventFinalizer);

    return JIM_OK;
}

static int aio_cmd_readable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    Jim_Obj *eofScript = NULL;
    int mask = JIM_EVENT_READABLE;


    if (argc == 2) {
        mask |= JIM_EVENT_FEOF;
        eofScript = argv[1];
    }

    return aio_eventinfo(interp, af, mask, &af->rEvent, argc ? argv[0] : NULL, eofScript);
}

static int aio_cmd_writable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int mask = JIM_EVENT_WRITABLE;

    return aio_eventinfo(interp, af, mask, &af->wEvent, argc ? argv[0] : NULL, NULL);
}

static int aio_cmd_onexception(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int mask = JIM_EVENT_EXCEPTION;

    return aio_eventinfo(interp, af, mask, &af->eEvent, argc ? argv[0] : NULL, NULL);
}
#endif

static const jim_subcmd_type command_table[] = {
    {   .cmd = "read",
        .args = "?-nonewline? ?len?",
        .function = aio_cmd_read,
        .minargs = 0,
        .maxargs = 2,
        .description = "Read and return bytes from the stream. To eof if no len."
    },
    {   .cmd = "gets",
        .args = "?var?",
        .function = aio_cmd_gets,
        .minargs = 0,
        .maxargs = 1,
        .description = "Read one line and return it or store it in the var"
    },
    {   .cmd = "puts",
        .args = "?-nonewline? str",
        .function = aio_cmd_puts,
        .minargs = 1,
        .maxargs = 2,
        .description = "Write the string, with newline unless -nonewline"
    },
#ifndef JIM_ANSIC
    {   .cmd = "recvfrom",
        .args = "len ?addrvar?",
        .function = aio_cmd_recvfrom,
        .minargs = 1,
        .maxargs = 2,
        .description = "Receive up to 'len' bytes on the socket. Sets 'addrvar' with receive address, if set"
    },
    {   .cmd = "sendto",
        .args = "str address",
        .function = aio_cmd_sendto,
        .minargs = 2,
        .maxargs = 2,
        .description = "Send 'str' to the given address (dgram only)"
    },
    {   .cmd = "accept",
        .function = aio_cmd_accept,
        .description = "Server socket only: Accept a connection and return stream"
    },
#endif
    {   .cmd = "flush",
        .function = aio_cmd_flush,
        .description = "Flush the stream"
    },
    {   .cmd = "eof",
        .function = aio_cmd_eof,
        .description = "Returns 1 if stream is at eof"
    },
    {   .cmd = "close",
        .flags = JIM_MODFLAG_FULLARGV,
        .function = aio_cmd_close,
        .description = "Closes the stream"
    },
    {   .cmd = "seek",
        .args = "offset ?start|current|end",
        .function = aio_cmd_seek,
        .minargs = 1,
        .maxargs = 2,
        .description = "Seeks in the stream (default 'current')"
    },
    {   .cmd = "tell",
        .function = aio_cmd_tell,
        .description = "Returns the current seek position"
    },
#ifdef O_NDELAY
    {   .cmd = "ndelay",
        .args = "?0|1?",
        .function = aio_cmd_ndelay,
        .minargs = 0,
        .maxargs = 1,
        .description = "Set O_NDELAY (if arg). Returns current/new setting."
    },
#endif
#ifdef jim_ext_eventloop
    {   .cmd = "readable",
        .args = "?readable-script ?eof-script??",
        .minargs = 0,
        .maxargs = 2,
        .function = aio_cmd_readable,
        .description = "Returns script, or invoke readable-script when readable, eof-script on eof, {} to remove",
    },
    {   .cmd = "writable",
        .args = "?writable-script?",
        .minargs = 0,
        .maxargs = 1,
        .function = aio_cmd_writable,
        .description = "Returns script, or invoke writable-script when writable, {} to remove",
    },
    {   .cmd = "onexception",
        .args = "?exception-script?",
        .minargs = 0,
        .maxargs = 1,
        .function = aio_cmd_onexception,
        .description = "Returns script, or invoke exception-script when oob data, {} to remove",
    },
#endif
    { 0 }
};

static int JimAioSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, command_table, argc, argv), argc, argv);
}

static int JimAioOpenCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    FILE *fp;
    AioFile *af;
    char buf[AIO_CMD_LEN];
    long fileId;
    int OpenFlags = 0;
    const char *cmdname;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "filename ?mode?");
        return JIM_ERR;
    }
    cmdname = Jim_GetString(argv[1], NULL);
    if (Jim_CompareStringImmediate(interp, argv[1], "stdin")) {
        OpenFlags |= AIO_KEEPOPEN;
        fp = stdin;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "stdout")) {
        OpenFlags |= AIO_KEEPOPEN;
        fp = stdout;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "stderr")) {
        OpenFlags |= AIO_KEEPOPEN;
        fp = stderr;
    } else {
        const char *mode = "r";
        if (argc == 3) {
            mode = Jim_GetString(argv[2], NULL);
        }
        fp = fopen(Jim_GetString(argv[1], NULL), mode);
        if (fp == NULL) {
            JimAioSetError(interp, argv[1]);
            return JIM_ERR;
        }
        /* Get the next file id */
        fileId = Jim_GetId(interp);
        sprintf(buf, "aio.handle%ld", fileId);
        cmdname = buf;
    }

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    af->fp = fp;
    af->fd = fileno(fp);
#ifdef O_NDELAY
    af->flags = fcntl(af->fd,F_GETFL);
#endif
    af->filename = argv[1];
    Jim_IncrRefCount(af->filename);
    af->OpenFlags = OpenFlags;
    af->rEvent = NULL;
    af->wEvent = NULL;
    af->eEvent = NULL;
    Jim_CreateCommand(interp, cmdname, JimAioSubCmdProc, af, JimAioDelProc);
    Jim_SetResultString(interp, cmdname, -1);
    return JIM_OK;
}

#ifndef JIM_ANSIC
/**
 * Creates a channel for fd.
 * 
 * hdlfmt is a sprintf format for the filehandle. Anything with %ld at the end will do.
 * mode is usual "r+", but may be another fdopen() mode as required.
 *
 * Creates the command and lappends the name of the command to the current result.
 *
 */
static int JimMakeChannel(Jim_Interp *interp, Jim_Obj *filename, const char *hdlfmt, int fd, const char *mode)
{
    long fileId;
    AioFile *af;
    char buf[AIO_CMD_LEN];

    FILE *fp = fdopen(fd, mode);
    if (fp == NULL) {
        close(fd);
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }

    /* Get the next file id */
    fileId = Jim_GetId(interp);

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    af->fp = fp;
    af->fd = fd;
    af->OpenFlags = 0;
    af->filename = filename;
    Jim_IncrRefCount(af->filename);
#ifdef O_NDELAY
    af->flags = fcntl(af->fd, F_GETFL);
#endif
    af->rEvent = NULL;
    af->wEvent = NULL;
    af->eEvent = NULL;
    sprintf(buf, hdlfmt, fileId);
    Jim_CreateCommand(interp, buf, JimAioSubCmdProc, af, JimAioDelProc);

    Jim_ListAppendElement(interp, Jim_GetResult(interp), Jim_NewStringObj(interp, buf, -1));

    return JIM_OK;
}

static int JimAioSockCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    const char *hdlfmt = "aio.unknown%ld";
    const char *socktypes[] = {
        "unix", 
        "unix.server", 
        "dgram", 
        "dgram.server", 
        "stream", 
        "stream.server",
        "pipe",
        NULL
    };
    enum {
        SOCK_UNIX, 
        SOCK_UNIX_SERV, 
        SOCK_DGRAM_CL, 
        SOCK_DGRAM_SERV, 
        SOCK_STREAM_CL, 
        SOCK_STREAM_SERV,
        SOCK_STREAM_PIPE,
    };
    int socktype;
    int sock;
    const char *hostportarg = NULL;
    int res;
    int on = 1;
    const char *mode = "r+";

    if (argc < 2 ) {
wrongargs:
        Jim_WrongNumArgs(interp, 1, argv, "type ?address?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], socktypes, &socktype, "socket type",
                    JIM_ERRMSG) != JIM_OK)
            return JIM_ERR;

    if (argc == 3) {
        hostportarg = Jim_GetString(argv[2], NULL);
    }
    else if (argc != 2 || (socktype != SOCK_STREAM_PIPE && socktype != SOCK_DGRAM_CL)) {
        goto wrongargs;
    }

    Jim_SetResultString(interp, "", 0);

    hdlfmt = "aio.sock%ld" ;

    switch (socktype) {
    case SOCK_DGRAM_CL:
        if (argc == 2) {
            /* No address, so an unconnected dgram socket */
            sock = socket(PF_INET,SOCK_DGRAM,0);
            if (sock < 0) {
                JimAioSetError(interp, NULL);
                return JIM_ERR;
            }
            break;
        }
        /* fall through */
    case SOCK_STREAM_CL:    
        {
            struct sockaddr_in sa;

            if (JimParseIpAddress(interp, hostportarg, &sa) != JIM_OK) {
                return JIM_ERR;
            }
            sock = socket(PF_INET,socktype == SOCK_DGRAM_CL ? SOCK_DGRAM : SOCK_STREAM,0);
            if (sock < 0) {
                JimAioSetError(interp, NULL);
                return JIM_ERR;
            }
            res = connect(sock,(struct sockaddr*)&sa,sizeof(sa));
            if (res) {
                JimAioSetError(interp, argv[2]);
                close(sock);
                return JIM_ERR;
            }
        }
        break;

    case SOCK_STREAM_SERV: 
    case SOCK_DGRAM_SERV: 
        {
            struct sockaddr_in sa;

            if (JimParseIpAddress(interp, hostportarg, &sa) != JIM_OK) {
                JimAioSetError(interp, argv[2]);
                return JIM_ERR;
            }
            sock = socket(PF_INET,socktype == SOCK_DGRAM_SERV ? SOCK_DGRAM : SOCK_STREAM,0);
            if (sock < 0) {
                JimAioSetError(interp, NULL);
                return JIM_ERR;
            }

            /* Enable address reuse */
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

            res = bind(sock,(struct sockaddr*)&sa,sizeof(sa));  
            if (res) {
                JimAioSetError(interp, argv[2]);
                close(sock);
                return JIM_ERR;
            }
            if (socktype != SOCK_DGRAM_SERV) {
                res = listen(sock,5);   
                if (res) {
                    JimAioSetError(interp, NULL);
                    close(sock);
                    return JIM_ERR;
                }
            }
            hdlfmt = "aio.socksrv%ld" ;
        }
        break;

    case SOCK_UNIX:
        {
            struct sockaddr_un sa;
            socklen_t len;

            if (JimParseDomainAddress(interp, hostportarg, &sa) != JIM_OK) {
                JimAioSetError(interp, argv[2]);
                return JIM_ERR;
            }
            sock = socket(PF_UNIX, SOCK_STREAM,0);
            if (sock < 0) {
                JimAioSetError(interp, NULL);
                return JIM_ERR;
            }
            len = strlen(sa.sun_path) + 1 + sizeof(sa.sun_family);
            res = connect(sock,(struct sockaddr*)&sa,len);
            if (res) {
                JimAioSetError(interp, argv[2]);
                close(sock);
                return JIM_ERR;
            }
            hdlfmt = "aio.sockunix%ld" ;
            break;
        }

    case SOCK_UNIX_SERV:
        {
            struct sockaddr_un sa;
            socklen_t len;

            if (JimParseDomainAddress(interp, hostportarg, &sa) != JIM_OK) {
                JimAioSetError(interp, argv[2]);
                return JIM_ERR;
            }
            sock = socket(PF_UNIX, SOCK_STREAM,0);
            if (sock < 0) {
                JimAioSetError(interp, NULL);
                return JIM_ERR;
            }
            len = strlen(sa.sun_path) + 1 + sizeof(sa.sun_family);
            res = bind(sock,(struct sockaddr*)&sa,len);
            if (res) {
                JimAioSetError(interp, argv[2]);
                close(sock);
                return JIM_ERR;
            }
            res = listen(sock,5);   
            if (res) {
                JimAioSetError(interp, NULL);
                close(sock);
                return JIM_ERR;
            }
            hdlfmt = "aio.sockunixsrv%ld" ;
            break;
        }
    case SOCK_STREAM_PIPE:
        {
            int p[2];

            if (pipe(p) < 0) {
                JimAioSetError(interp, NULL);
                return JIM_ERR;
            }

            hdlfmt = "aio.pipe%ld" ;
            if (JimMakeChannel(interp, argv[1], hdlfmt, p[0], "r") != JIM_OK) {
                close(p[0]);
                close(p[1]);
                JimAioSetError(interp, NULL);
                return JIM_ERR;
            }
            /* Note, if this fails it will leave p[0] open, but this should never happen */
            mode = "w";
            sock = p[1];
        }
        break;

    default:
        Jim_SetResultString(interp, "Unsupported socket type", -1);
        return JIM_ERR;
    }

    return JimMakeChannel(interp, argv[1], hdlfmt, sock, mode);
}
#endif

FILE *Jim_AioFilehandle(Jim_Interp *interp, Jim_Obj *command)
{
    Jim_Cmd *cmdPtr = Jim_GetCommand(interp, command, JIM_ERRMSG);

    if (cmdPtr && cmdPtr->cmdProc == JimAioSubCmdProc) {
        return ((AioFile *)cmdPtr->privData)->fp;
    }
    return NULL;
}

#ifdef JIM_TCL_COMPAT
static int JimAioTclCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *newargv[4];
    int ret;
    int i;
    int nonewline = 0;

    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[1], "-nonewline")) {
        nonewline = 1;
    }
    if (argc < 2 + nonewline || argc > 4) {
        Jim_WrongNumArgs(interp, 1, argv, "channel");
        return JIM_ERR;
    }

    if (nonewline) {
        /* read -nonewline $f ... => $f read -nonewline ... */
        newargv[0] = argv[2];
        newargv[1] = argv[0];
        newargv[2] = argv[1];
    }
    else {
        /* cmd $f ... => $f cmd ... */
        newargv[0] = argv[1];
        newargv[1] = argv[0];
    }

    for (i = 2 + nonewline; i < argc; i++) {
        newargv[i] = argv[i];
    }

    ret = Jim_EvalObjVector(interp, argc, newargv);

    return ret;
}

static int JimAioPutsCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *newargv[4];
    int nonewline = 0;

    int off = 1;

    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[1], "-nonewline")) {
        nonewline = 1;
    }

    if (argc < 2 + nonewline || argc > 3 + nonewline) {
        Jim_WrongNumArgs(interp, 1, argv, "?-nonewline? ?channel? string");
        return JIM_ERR;
    }

    /* "puts" */
    newargv[off++] = argv[0];

    if (nonewline) {
        newargv[off++] = argv[1];
        argv++;
        argc--;
    }

    if (argc == 2) {
        /* Missing channel, so use stdout */
        newargv[0] = Jim_NewStringObj(interp, "stdout", -1);
        newargv[off++] = argv[1];
    }
    else {
        newargv[0] = argv[1];
        newargv[off++] = argv[2];
    }

    return Jim_EvalObjVector(interp, off, newargv);
}

static void JimAioTclCompat(Jim_Interp *interp)
{
    static const char *tclcmds[] = { "read", "gets", "flush", "close", "eof", "seek", "tell", 0};
    int i;

    for (i = 0; tclcmds[i]; i++) {
        Jim_CreateCommand(interp, tclcmds[i], JimAioTclCmd, NULL, NULL);
    }
    Jim_CreateCommand(interp, "puts", JimAioPutsCmd, NULL, NULL);
}
#endif

int 
Jim_aioInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "aio", "1.0", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    Jim_CreateCommand(interp, "open", JimAioOpenCommand, NULL, NULL);
#ifndef JIM_ANSIC
    Jim_CreateCommand(interp, "socket", JimAioSockCommand, NULL, NULL);
#endif

    /* Takeover stdin, stdout and stderr */
    Jim_EvalGlobal(interp, "open stdin; open stdout; open stderr");

#ifdef JIM_TCL_COMPAT
    JimAioTclCompat(interp);
#endif

    return JIM_OK;
}
