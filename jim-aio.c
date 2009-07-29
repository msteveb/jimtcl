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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "jim.h"
#include "jim-eventloop.h"
#include "jim-subcmd.h"


#define AIO_CMD_LEN 128
#define AIO_BUF_LEN 1024

#define AIO_KEEPOPEN 1
#define AIO_FDOPEN   2


typedef struct AioFile {
    FILE *fp;
    int type;
    int OpenFlags; /* AIO_KEEPOPEN? keep FILE*, AIO_FDOPEN? FILE* created via fdopen */
    int fd;
    int flags;
    Jim_Obj *rEvent;
    Jim_Obj *wEvent;
    Jim_Obj *eEvent;
    struct sockaddr sa;
    struct hostent *he;
} AioFile;

static int JimAioSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

static void JimAioSetError(Jim_Interp *interp)
{
    Jim_SetResultString(interp, strerror(errno), -1);
}

static void JimAioDelProc(Jim_Interp *interp, void *privData)
{
    AioFile *af = privData;
    JIM_NOTUSED(interp);

    if (!(af->OpenFlags & AIO_KEEPOPEN)) {
        fclose(af->fp);
    }
    if (!af->OpenFlags == AIO_FDOPEN) {
        close(af->fd);
    }
#ifdef with_jim_ext_eventloop
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
        JimAioSetError(interp);
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
            /* strip "\n" */
            Jim_AppendString(interp, objPtr, buf, strlen(buf)-1);
        }
        if (!more)
            break;
    }
    if (ferror(af->fp) && (errno != EAGAIN)) {
        /* I/O error */
        Jim_FreeNewObj(interp, objPtr);
        JimAioSetError(interp);
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
    JimAioSetError(interp);
    return JIM_ERR;
}

static int aio_cmd_flush(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    if (fflush(af->fp) == EOF) {
        JimAioSetError(interp);
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
        JimAioSetError(interp);
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

        if (Jim_GetLong(interp, argv[2], &nb) != JIM_OK) {
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
    af->fp = fdopen(sock,"r+");
    af->OpenFlags = AIO_FDOPEN;
    af->flags = fcntl(af->fd,F_GETFL);
    af->rEvent = NULL;
    af->wEvent = NULL;
    af->eEvent = NULL;
    sprintf(buf, "aio.sockstream%ld", fileId);
    Jim_CreateCommand(interp, buf, JimAioSubCmdProc, af, JimAioDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

#ifdef with_jim_ext_eventloop
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
    {   .cmd = "ndelay",
        .args = "?0|1?",
        .function = aio_cmd_ndelay,
        .minargs = 0,
        .maxargs = 1,
        .description = "Set O_NDELAY (if arg). Returns current/new setting."
    },
    {   .cmd = "accept",
        .function = aio_cmd_accept,
        .description = "Server socket only: Accept a connection and return stream"
    },
#ifdef with_jim_ext_eventloop
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
    const char *mode = "r";
    long fileId;
    const char *options[] = {"input", "output", "error", NULL};
    enum {OPT_INPUT, OPT_OUTPUT, OPT_ERROR};
    int OpenFlags = 0;
    int modeLen;
    const char *cmdname = buf;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "filename ?mode?");
        return JIM_ERR;
    }
    if (argc == 3)
        mode = Jim_GetString(argv[2], &modeLen);
    if (argc == 3 && Jim_CompareStringImmediate(interp, argv[1], "standard") &&
            modeLen >= 3) {
            int option;
        if (Jim_GetEnum(interp, argv[2], options, &option, "standard channel",
                    JIM_ERRMSG) != JIM_OK)
            return JIM_ERR;
        OpenFlags |= AIO_KEEPOPEN;
        switch (option) {
        case OPT_INPUT: fp = stdin; cmdname = "stdin"; break;
        case OPT_OUTPUT: fp = stdout; cmdname = "stdout"; break;
        case OPT_ERROR: fp = stderr; cmdname = "stderr"; break;
        default: fp = NULL; Jim_Panic(interp,"default reached in JimAioOpenCommand()");
                 break;
        }
    } else {
        fp = fopen(Jim_GetString(argv[1], NULL), mode);
        if (fp == NULL) {
            JimAioSetError(interp);
            return JIM_ERR;
        }
        /* Get the next file id */
        fileId = Jim_GetId(interp);
        sprintf(buf, "aio.handle%ld", fileId);
    }

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    af->fp = fp;
    af->fd = fileno(fp);
    af->flags = fcntl(af->fd,F_GETFL);
    af->OpenFlags = OpenFlags;
    af->rEvent = NULL;
    af->wEvent = NULL;
    af->eEvent = NULL;
    Jim_CreateCommand(interp, cmdname, JimAioSubCmdProc, af, JimAioDelProc);
    Jim_SetResultString(interp, cmdname, -1);
    return JIM_OK;
}

static int JimAioSockCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    FILE *fp;
    AioFile *af;
    char buf[AIO_CMD_LEN];
    char *hdlfmt = "aio.unknown%ld";
    long fileId;
    const char *socktypes[] = {
        "file",
        "pipe",
        "tty",
        "domain", 
        "dgram", 
        "stream", 
        "stream.server",
        
        NULL
    };
    enum {
        FILE_FILE,
        FILE_PIPE,
        FILE_TTY,
        SOCK_DOMAIN, 
        SOCK_DGRAM_CL, 
        SOCK_STREAM_CL, 
        SOCK_STREAM_SERV 
    };
    int socktype;
    int sock;
    const char *hostportarg;
    int hostportlen;
    char a[0x20];
    char b[0x20];
    char c[0x20];
    char np[] = "0";
    char nh[] = "0.0.0.0";
    char* stsrcport; 
    char* sthost; 
    char* stport;
    unsigned int srcport;
    unsigned int port;
    struct sockaddr_in sa;
    struct hostent *he;
    int res;
    int on = 1;

    if (argc <= 2 ) {
        Jim_WrongNumArgs(interp, 1, argv, "sockspec  ?script?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], socktypes, &socktype, "socket type",
                    JIM_ERRMSG) != JIM_OK)
            return JIM_ERR;
    hostportarg = Jim_GetString(argv[2], &hostportlen);
    switch (sscanf(hostportarg,"%[^:]:%[^:]:%[^:]",a,b,c)) {
        case 3: stsrcport = a; sthost = b; stport = c; break;
        case 2: stsrcport = np; sthost = a; stport = b; break;
        case 1: stsrcport = np; sthost = nh; stport = a; break;
        default:
            return JIM_ERR;
    }
    if (0 == strncmp(sthost,"ANY",3))
    sthost = "0.0.0.0";
    srcport = atol(stsrcport);
    port = atol(stport);
    he = gethostbyname(sthost);
    /* FIX!!!! this still results in null pointer exception here.  
       FIXED!!!! debug output but no JIM_ERR done UK.  
     */
    if (!he) {
        Jim_SetResultString(interp,hstrerror(h_errno),-1);
        return JIM_ERR;
    }

    sock = socket(PF_INET,SOCK_STREAM,0);
    switch (socktype) {
    case SOCK_DGRAM_CL:
            hdlfmt = "aio.sockdgram%ld" ;
        break;
    case SOCK_STREAM_CL:    
            sa.sin_family= he->h_addrtype;
        bcopy(he->h_addr,(char *)&sa.sin_addr,he->h_length); /* set address */
        sa.sin_port = htons(port);
        res = connect(sock,(struct sockaddr*)&sa,sizeof(sa));
        if (res) {
            close(sock);
            JimAioSetError(interp);
            return JIM_ERR;
        }
        hdlfmt = "aio.sockstrm%ld" ;
        break;
    case SOCK_STREAM_SERV: 
            sa.sin_family= he->h_addrtype;
        bcopy(he->h_addr,(char *)&sa.sin_addr,he->h_length); /* set address */
        sa.sin_port = htons(port);

        /* Enable address reuse */
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

        res = bind(sock,(struct sockaddr*)&sa,sizeof(sa));  
        if (res) {
            close(sock);
            JimAioSetError(interp);
            return JIM_ERR;
        }
        res = listen(sock,5);   
        if (res) {
            close(sock);
            JimAioSetError(interp);
            return JIM_ERR;
        }
        hdlfmt = "aio.socksrv%ld" ;
        break;
    }
    fp = fdopen(sock, "r+" );
    if (fp == NULL) {
    close(sock);
        JimAioSetError(interp);
        return JIM_ERR;
    }
    /* Get the next file id */
    fileId = Jim_GetId(interp);

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    af->fp = fp;
    af->fd = sock;
    af->OpenFlags = AIO_FDOPEN;
    af->flags = fcntl(af->fd,F_GETFL);
    af->rEvent = NULL;
    af->wEvent = NULL;
    af->eEvent = NULL;
    sprintf(buf, hdlfmt, fileId);
    Jim_CreateCommand(interp, buf, JimAioSubCmdProc, af, JimAioDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

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
    Jim_Obj **newargv = Jim_Alloc(sizeof(Jim_Obj*)*argc);
    int ret;
    int i;

    /* cmd channel ?args? */
    newargv[0] = argv[1];
    newargv[1] = argv[0];

    for (i = 2; i < argc; i++) {
        newargv[i] = argv[i];
    }

    ret = Jim_EvalObjVector(interp, argc, newargv);

    Jim_Free(newargv);

    return ret;
}

static int JimAioPutsCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *newargv[4];

    int off = 1;

    /* "puts" */
    newargv[off++] = argv[0];

    /* puts ?-nonewline? ?channel? msg */
    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[1], "-nonewline")) {
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
    Jim_CreateCommand(interp, "open", JimAioOpenCommand, NULL, NULL);
}
#endif

int 
Jim_aioInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "aio", "1.0", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    Jim_CreateCommand(interp, "aio.open", JimAioOpenCommand, NULL, NULL);
    Jim_CreateCommand(interp, "aio.socket", JimAioSockCommand, NULL, NULL);

    /* Takeover stdin, stdout and stderr */
    Jim_EvalGlobal(interp,
        "aio.open standard input; aio.open standard output; aio.open standard error");

#ifdef JIM_TCL_COMPAT
    JimAioTclCompat(interp);
#endif

    return JIM_OK;
}
