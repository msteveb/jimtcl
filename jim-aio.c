/* Jim - ANSI I/O extension
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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifdef __ECOS
#include <pkgconf/jimtcl.h>
#endif
#ifndef JIM_STATICEXT
#define JIM_EXTENSION
#endif
#ifdef __ECOS
#include <cyg/jimtcl/jim.h>
#else
#include "jim.h"
#endif
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
    // fprintf(stderr,"mask:%d\n",mask);
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
	fprintf(stderr,"deleting ReadEvent\n");
	Jim_DeleteFileHandler(interp,af->fp);
	Jim_DecrRefCount(interp,af->rEvent);
    }
    if (af->wEvent) {
	fprintf(stderr,"deleting WriteEvent\n");
	Jim_DeleteFileHandler(interp,af->fp);
    	Jim_DecrRefCount(interp,af->wEvent);
    }
    if (af->eEvent) {
	fprintf(stderr,"deleting ExceptionEvent\n");
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
    } else if   (  (option  == OPT_READABLE) 
		|| (option  == OPT_WRITABLE) 
		|| (option  == OPT_EXCEPTION) 
                ) {
	int mask = 0;
	Jim_Obj **scrListObjpp = NULL;
	Jim_Obj *listObj;
	const char *dummy = NULL;
	int scrlen = 0;

	if (!(Jim_CreateFileHandler && Jim_DeleteFileHandler)) {
		Jim_SetResultString(interp, "Eventloop not present ( or loaded too late ) !", -1);
        	return JIM_ERR;
	}
	switch (option) {
	case OPT_READABLE:  mask = JIM_EVENT_READABLE;  scrListObjpp = &af->rEvent; 
		if (argc == 4)  mask |= JIM_EVENT_FEOF ; 			  break;
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
			// fprintf(stderr,"0 %p \n",*scrListObjpp);
			listObj = argv[2];
			if (Jim_IsShared(listObj))
				listObj = Jim_DuplicateObj(interp, listObj);
			// Jim_IncrRefCount(listObj);
			// fprintf(stderr,"script:\"%s\" argp: %p objp1: %p\n", Jim_GetString(argv[2], NULL),argv[2],listObj);
			// fprintf(stderr,"1");
			Jim_ListAppendElement(interp,*scrListObjpp,listObj);
			// fprintf(stderr,"2");
			if (mask & JIM_EVENT_FEOF) {
				listObj = argv[3];
				if (Jim_IsShared(listObj))
					listObj = Jim_DuplicateObj(interp, listObj);
				// Jim_IncrRefCount(listObj);
				// fprintf(stderr,"script:\"%s\" argp: %p objp2: %p\n", Jim_GetString(argv[3], NULL),argv[3],listObj);
				// fprintf(stderr,"3");
				Jim_ListAppendElement(interp,*scrListObjpp,listObj);
				// fprintf(stderr,"4");
			}
			// fprintf(stderr,"event readable fd: %d, script:\"%s\" objp3: %p\n",af->fd, Jim_GetString(argv[2], NULL),argv[2]);
			Jim_IncrRefCount(*scrListObjpp);
			// fprintf(stderr,"6 %p \n",Jim_CreateFileHandler);
			Jim_CreateFileHandler(interp, af->fp, mask, 
				JimAioFileEventHandler,
				*scrListObjpp,
				JimAioFileEventFinalizer);
			// fprintf(stderr,"7");
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
	fprintf(stderr,"ACCEPT\n");
	ret = JimAioAcceptHelper(interp,af);
	fprintf(stderr,"ret %d\n",ret);
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
    // fprintf(stderr,"hallo\n");
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
    char *hdlfmt;
    // const char *mode = "r";
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
    int modeLen;
    int socktype;
    int sock;
    FILE *fsock;
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
    int elemcnt;
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
    fprintf(stderr,"socktype: %s \n",socktypes[socktype]);
    hostportarg = Jim_GetString(argv[2], &hostportlen);
    fprintf(stderr,"hostportarg: %s %d \n",hostportarg,hostportlen);
    switch (sscanf(hostportarg,"%[^:]:%[^:]:%[^:]",a,b,c)) {
	case 3: stsrcport = a; sthost = b; stport = c; break;
	case 2: stsrcport = np; sthost = a; stport = b; break;
	case 1: stsrcport = np; sthost = nh; stport = a; break;
	default:
            return JIM_ERR;
    }
    fprintf(stderr,"socktype: %d srcport: %s host:%s port %s \n",
		socktype,stsrcport,sthost,stport);
    if (0 == strncmp(sthost,"ANY",3))
	sthost = "0.0.0.0";
    srcport = atol(stsrcport);
    port = atol(stport);
    he = gethostbyname(sthost);
    if (!he)
	herror("gethostbyname");
    fprintf(stderr,"Official name is: %s\n", he->h_name);
    fprintf(stderr,"IP address: %s\n", inet_ntoa(*(struct in_addr*)he->h_addr));

    sock = socket(PF_INET,SOCK_STREAM,0);
    fprintf(stderr,"srcp: %x port: %x IP: %x sock: %d type: %d\n",srcport,port,he->h_addr,sock,socktype);
    switch (socktype) {
    case SOCK_DGRAM_CL:
		hdlfmt = "aio.sockdgram%ld" ;
		break;
    case SOCK_STREAM_CL:	
    		fprintf(stderr,"setting up client socket\n");
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
    		fprintf(stderr,"setting up listening socket\n");
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
    fprintf(stderr,"fp: %p \n",fp);
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
    fprintf(stderr,"hallo\n");
    af->rEvent = NULL;
    af->wEvent = NULL;
    af->eEvent = NULL;
    sprintf(buf, hdlfmt, fileId);
    fprintf(stderr,"hallo:%s\n",buf);
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
	fprintf(stderr,"accepting connection for %d \n",serv_af->fd);
    sock = accept(serv_af->fd,(struct sockaddr*)&serv_af->sa,&addrlen);
	fprintf(stderr,"done, got %d \n",sock);
    if (sock < 0)
	return JIM_ERR;

    /* Get the next file id */
    fprintf(stderr,"getting fileid:");
    if (Jim_EvalGlobal(interp,
                "if {[catch {incr aio.fileId}]} {set aio.fileId 0}") != JIM_OK)
        return JIM_ERR;
    objPtr = Jim_GetGlobalVariableStr(interp, "aio.fileId", JIM_ERRMSG);
    if (objPtr == NULL) return JIM_ERR;
    if (Jim_GetLong(interp, objPtr, &fileId) != JIM_OK) return JIM_ERR;
    fprintf(stderr," %ld\n", fileId);

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    af->fd = sock;
    af->fp = fdopen(sock,"r+");
    af->OpenFlags = AIO_FDOPEN;
    af->flags = fcntl(af->fd,F_GETFL);
    // fprintf(stderr,"hallo\n");
    af->rEvent = NULL;
    af->wEvent = NULL;
    af->eEvent = NULL;
    sprintf(buf, "aio.sockstream%ld", fileId);
    Jim_CreateCommand(interp, buf, JimAioHandlerCommand, af, JimAioDelProc);
    Jim_SetResultString(interp, buf, -1);
    fprintf(stderr,"returning\n");
    return JIM_OK;
}

DLLEXPORT int 
#ifndef JIM_STATICEXT
Jim_OnLoad(Jim_Interp *interp)
#else
Jim_AioInit(Jim_Interp *interp)
#endif
{
    #ifndef JIM_STATICEXT
    Jim_InitExtension(interp);
    #endif
	Jim_ImportEventloopAPI(interp);
    if (Jim_PackageProvide(interp, "aio", "1.0", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    Jim_CreateCommand(interp, "aio.open", JimAioOpenCommand, NULL, NULL);
    Jim_CreateCommand(interp, "aio.socket", JimAioSockCommand, NULL, NULL);
    return JIM_OK;
}

// end

