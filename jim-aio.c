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

/* FIX!!! add #if's to make JIM_SUPPORT_EVENTLOOP enable/disable
 * eventloop support compile time! */
#ifndef JIM_SUPPORT_EVENTLOOP
#define JIM_SUPPORT_EVENTLOOP 1
#endif

#include "jim.h"
#include "jim-eventloop.h"


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

static int JimAioAcceptHelper(Jim_Interp *interp, AioFile *serv_af );

static void JimAioFileEventFinalizer(Jim_Interp *interp, void *clientData)
{
    Jim_Obj *objPtr = clientData;

    Jim_DecrRefCount(interp, objPtr);
}

static int JimAioFileEventHandler(Jim_Interp *interp, void *clientData, int mask)
{
    Jim_Obj *objPtr = clientData;
    Jim_Obj *scrPtr = NULL ;
    if ( mask == (JIM_EVENT_READABLE | JIM_EVENT_FEOF)) {
        Jim_ListIndex(interp, objPtr, 1, &scrPtr, 0);
    } else {
        Jim_ListIndex(interp, objPtr, 0, &scrPtr, 0);
    }
    Jim_EvalObjBackground(interp, scrPtr);
    return 0;
}

static void JimAioSetError(Jim_Interp *interp)
{
    Jim_SetResultString(interp, strerror(errno), -1);
}

static void JimAioDelProc(Jim_Interp *interp, void *privData)
{
    AioFile *af = privData;
    JIM_NOTUSED(interp);

    if (!(af->OpenFlags & AIO_KEEPOPEN))
        fclose(af->fp);
    if (!af->OpenFlags == AIO_FDOPEN) // fp = fdopen(fd) !!
        close(af->fd);
    if (af->rEvent) { // remove existing EventHandlers
    Jim_DeleteFileHandler(interp,af->fp);
    Jim_DecrRefCount(interp,af->rEvent);
    }
    if (af->wEvent) {
    Jim_DeleteFileHandler(interp,af->fp);
        Jim_DecrRefCount(interp,af->wEvent);
    }
    if (af->eEvent) {
    Jim_DeleteFileHandler(interp,af->fp);
        Jim_DecrRefCount(interp,af->eEvent);
    }
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
        "close", 
    "seek", "tell", 
    "gets", "read", "puts", 
    "flush", "eof", 
    "ndelay", 
    "readable", "writable", "onexception",
    "accept",
    NULL
    };
    enum {OPT_CLOSE, 
      OPT_SEEK, OPT_TELL, 
      OPT_GETS, OPT_READ, OPT_PUTS,
          OPT_FLUSH, OPT_EOF, 
      OPT_NDELAY,
      OPT_READABLE, OPT_WRITABLE, OPT_EXCEPTION,
      OPT_ACCEPT
    };

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
    } else if (option == OPT_READ) {
    /* READ */
        char buf[AIO_BUF_LEN];
        Jim_Obj *objPtr;
        int nonewline = 0;
        int neededLen = -1; /* -1 is "read as much as possible" */

        if (argc != 2 && argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "?-nonewline? ?len?");
            return JIM_ERR;
        }
        if (argc == 3 &&
            Jim_CompareStringImmediate(interp, argv[2], "-nonewline"))
        {
            nonewline = 1;
            argv++;
            argc--;
        }
        if (argc == 3) {
            jim_wide wideValue;
            if (Jim_GetWide(interp, argv[2], &wideValue) != JIM_OK)
                return JIM_ERR;
            if (wideValue < 0) {
                Jim_SetResultString(interp, "invalid parameter: negative len",
                        -1);
                return JIM_ERR;
            }
            neededLen = (int) wideValue;
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
    } else if (option == OPT_PUTS) {
    /* PUTS */
        int wlen;
        const char *wdata;

        if (argc != 3 && (argc != 4 || !Jim_CompareStringImmediate(
                        interp, argv[2], "-nonewline"))) {
            Jim_WrongNumArgs(interp, 2, argv, "?-nonewline? string");
            return JIM_ERR;
        }
        wdata = Jim_GetString(argv[2+(argc==4)], &wlen);
        if (fwrite(wdata, 1, wlen, af->fp) != (unsigned)wlen ||
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
    } else if (option  == OPT_EOF) {
    /* EOF */
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_SetResult(interp, Jim_NewIntObj(interp, feof(af->fp)));
        return JIM_OK;
    } else if (option  == OPT_NDELAY) {
#ifdef O_NDELAY
        int fmode = af->flags;

        if (argc == 3) {
        jim_wide wideValue;

        if (Jim_GetWide(interp, argv[2], &wideValue) != JIM_OK)
                return JIM_ERR;
        switch (wideValue) {
        case 0:
            fmode &= ~O_NDELAY; break ;
        case 1:
            fmode |=  O_NDELAY; break ;
        }
        fcntl(af->fd,F_SETFL,fmode);
        af->flags = fmode;
    }
        Jim_SetResult(interp, Jim_NewIntObj(interp, (fmode & O_NONBLOCK)?1:0));
        return JIM_OK;
#else
        return JIM_ERR;
#endif
    } else if   (  (option  == OPT_READABLE) 
        || (option  == OPT_WRITABLE) 
        || (option  == OPT_EXCEPTION) 
                ) {
    int mask = 0;
    Jim_Obj **scrListObjpp = NULL;
    Jim_Obj *listObj;
    const char *dummy = NULL;
    int scrlen = 0;

    switch (option) {
    case OPT_READABLE:  mask = JIM_EVENT_READABLE;  scrListObjpp = &af->rEvent; 
        if (argc == 4)  mask |= JIM_EVENT_FEOF ;              break;
    case OPT_WRITABLE:  mask = JIM_EVENT_WRITABLE;  scrListObjpp = &af->wEvent; break;
    case OPT_EXCEPTION: mask = JIM_EVENT_EXCEPTION; scrListObjpp = &af->eEvent; break;
    }
        switch (argc) {
    case 4:
    case 3:
        if (*scrListObjpp) {
            Jim_DeleteFileHandler(interp, af->fp); //,mask);
            Jim_DecrRefCount(interp, *scrListObjpp); 
            *scrListObjpp = NULL;
        }
        if ( dummy = Jim_GetString(argv[2],&scrlen),(scrlen == 0)) {
            break;
        } else {
            *scrListObjpp = Jim_NewListObj(interp, NULL, 0);
            Jim_IncrRefCount(*scrListObjpp);
            listObj = argv[2];
            if (Jim_IsShared(listObj))
                listObj = Jim_DuplicateObj(interp, listObj);
            // Jim_IncrRefCount(listObj);
            Jim_ListAppendElement(interp,*scrListObjpp,listObj);
            if (mask & JIM_EVENT_FEOF) {
                listObj = argv[3];
                if (Jim_IsShared(listObj))
                    listObj = Jim_DuplicateObj(interp, listObj);
                // Jim_IncrRefCount(listObj);
                Jim_ListAppendElement(interp,*scrListObjpp,listObj);
            }
            Jim_IncrRefCount(*scrListObjpp);
            Jim_CreateFileHandler(interp, af->fp, mask, 
                JimAioFileEventHandler,
                *scrListObjpp,
                JimAioFileEventFinalizer);
        }
        break;
    case 2:
        if (*scrListObjpp)
            Jim_SetResult(interp,*scrListObjpp);
        return JIM_OK;
    default:
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
    } else if (option  == OPT_ACCEPT) {
    int ret;
    ret = JimAioAcceptHelper(interp,af);
    return (ret);
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
    const char *options[] = {"input", "output", "error", NULL};
    enum {OPT_INPUT, OPT_OUTPUT, OPT_ERROR};
    int OpenFlags = 0;
    int modeLen;

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
        case OPT_INPUT: fp = stdin; break;
        case OPT_OUTPUT: fp = stdout; break;
        case OPT_ERROR: fp = stderr; break;
        default: fp = NULL; Jim_Panic(interp,"default reached in JimAioOpenCommand()");
                 break;
        }
    } else {
        fp = fopen(Jim_GetString(argv[1], NULL), mode);
        if (fp == NULL) {
            JimAioSetError(interp);
            return JIM_ERR;
        }
    }
    /* Get the next file id */
    if (Jim_EvalGlobal(interp,
                "if {[catch {incr aio.fileId}]} {set aio.fileId 0}") != JIM_OK)
        return JIM_ERR;
    objPtr = Jim_GetGlobalVariableStr(interp, "aio.fileId", JIM_ERRMSG);
    if (objPtr == NULL) return JIM_ERR;
    if (Jim_GetLong(interp, objPtr, &fileId) != JIM_OK) return JIM_ERR;

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    af->fp = fp;
    af->fd = fileno(fp);
    af->flags = fcntl(af->fd,F_GETFL);
    af->OpenFlags = OpenFlags;
    af->rEvent = NULL;
    af->wEvent = NULL;
    af->eEvent = NULL;
    sprintf(buf, "aio.handle%ld", fileId);
    Jim_CreateCommand(interp, buf, JimAioHandlerCommand, af, JimAioDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

static int JimAioSockCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    FILE *fp;
    AioFile *af;
    char buf[AIO_CMD_LEN];
    char *hdlfmt = "unknown";
    Jim_Obj *objPtr;
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
    if (!he) {
        Jim_SetResultString(interp,hstrerror(h_errno),-1);
        return JIM_ERR;
    }
     */

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
    if (Jim_EvalGlobal(interp,
                "if {[catch {incr aio.fileId}]} {set aio.fileId 0}") != JIM_OK)
        return JIM_ERR;
    objPtr = Jim_GetGlobalVariableStr(interp, "aio.fileId", JIM_ERRMSG);
    if (objPtr == NULL) return JIM_ERR;
    if (Jim_GetLong(interp, objPtr, &fileId) != JIM_OK) return JIM_ERR;

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
    Jim_CreateCommand(interp, buf, JimAioHandlerCommand, af, JimAioDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

static int JimAioAcceptHelper(Jim_Interp *interp, AioFile *serv_af )
{
    int sock;
    int addrlen = sizeof(struct sockaddr_in);
    AioFile *af;
    char buf[AIO_CMD_LEN];
    Jim_Obj *objPtr;
    long fileId;
    sock = accept(serv_af->fd,(struct sockaddr*)&serv_af->sa,&addrlen);
    if (sock < 0)
        return JIM_ERR;

    /* Get the next file id */
    if (Jim_EvalGlobal(interp,
                "if {[catch {incr aio.fileId}]} {set aio.fileId 0}") != JIM_OK)
        return JIM_ERR;
    objPtr = Jim_GetGlobalVariableStr(interp, "aio.fileId", JIM_ERRMSG);
    if (objPtr == NULL) return JIM_ERR;
    if (Jim_GetLong(interp, objPtr, &fileId) != JIM_OK) return JIM_ERR;

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
    Jim_CreateCommand(interp, buf, JimAioHandlerCommand, af, JimAioDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

int 
Jim_aioInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "aio", "1.0", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    Jim_CreateCommand(interp, "aio.open", JimAioOpenCommand, NULL, NULL);
    Jim_CreateCommand(interp, "aio.socket", JimAioSockCommand, NULL, NULL);
    return JIM_OK;
}

// end

