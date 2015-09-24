/* Jim - A small embeddable Tcl interpreter
 *
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright 2005 Clemens Hintze <c.hintze@gmx.net>
 * Copyright 2005 patthoyts - Pat Thoyts <patthoyts@users.sf.net>
 * Copyright 2008 oharboe - Øyvind Harboe - oyvind.harboe@zylin.com
 * Copyright 2008 Andrew Lunn <andrew@lunn.ch>
 * Copyright 2008 Duane Ellis <openocd@duaneellis.com>
 * Copyright 2008 Uwe Klein <uklein@klein-messgeraete.de>
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

#include "jimautoconf.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#include <sys/stat.h>
#endif

#include "jim.h"

#if defined(HAVE_SYS_SOCKET_H) && defined(HAVE_SELECT) && defined(HAVE_NETINET_IN_H) && defined(HAVE_NETDB_H) && defined(HAVE_ARPA_INET_H)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#else
#define JIM_ANSIC
#endif

#if defined(JIM_SSL)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "jim-eventloop.h"
#include "jim-subcmd.h"

#define AIO_CMD_LEN 32      /* e.g. aio.handleXXXXXX */
#define AIO_BUF_LEN 256     /* Can keep this small and rely on stdio buffering */

#ifndef HAVE_FTELLO
    #define ftello ftell
#endif
#ifndef HAVE_FSEEKO
    #define fseeko fseek
#endif

#define AIO_KEEPOPEN 1

#if defined(JIM_IPV6)
#define IPV6 1
#else
#define IPV6 0
#ifndef PF_INET6
#define PF_INET6 0
#endif
#endif

#if !defined(JIM_ANSIC) && !defined(JIM_BOOTSTRAP)
union sockaddr_any {
    struct sockaddr sa;
    struct sockaddr_in sin;
#if IPV6
    struct sockaddr_in6 sin6;
#endif
};

#ifndef HAVE_INET_NTOP
const char *inet_ntop(int af, const void *src, char *dst, int size)
{
    if (af != PF_INET) {
        return NULL;
    }
    snprintf(dst, size, "%s", inet_ntoa(((struct sockaddr_in *)src)->sin_addr));
    return dst;
}
#endif
#endif /* JIM_BOOTSTRAP */

typedef struct AioFile
{
    FILE *fp;
    Jim_Obj *filename;
    int type;
    int openFlags;              /* AIO_KEEPOPEN? keep FILE* */
    int fd;
    Jim_Obj *rEvent;
    Jim_Obj *wEvent;
    Jim_Obj *eEvent;
    int addr_family;
    void *ssl;
} AioFile;

static int JimAioSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);
static int JimMakeChannel(Jim_Interp *interp, FILE *fh, int fd, Jim_Obj *filename,
    const char *hdlfmt, int family, const char *mode, void *ssl);

#if !defined(JIM_ANSIC) && !defined(JIM_BOOTSTRAP)
static int JimParseIPv6Address(Jim_Interp *interp, const char *hostport, union sockaddr_any *sa, int *salen)
{
#if IPV6
    /*
     * An IPv6 addr/port looks like:
     *   [::1]
     *   [::1]:2000
     *   [fe80::223:6cff:fe95:bdc0%en1]:2000
     *   [::]:2000
     *   2000
     *
     *   Note that the "any" address is ::, which is the same as when no address is specified.
     */
     char *sthost = NULL;
     const char *stport;
     int ret = JIM_OK;
     struct addrinfo req;
     struct addrinfo *ai;

    stport = strrchr(hostport, ':');
    if (!stport) {
        /* No : so, the whole thing is the port */
        stport = hostport;
        hostport = "::";
        sthost = Jim_StrDup(hostport);
    }
    else {
        stport++;
    }

    if (*hostport == '[') {
        /* This is a numeric ipv6 address */
        char *pt = strchr(++hostport, ']');
        if (pt) {
            sthost = Jim_StrDupLen(hostport, pt - hostport);
        }
    }

    if (!sthost) {
        sthost = Jim_StrDupLen(hostport, stport - hostport - 1);
    }

    memset(&req, '\0', sizeof(req));
    req.ai_family = PF_INET6;

    if (getaddrinfo(sthost, NULL, &req, &ai)) {
        Jim_SetResultFormatted(interp, "Not a valid address: %s", hostport);
        ret = JIM_ERR;
    }
    else {
        memcpy(&sa->sin, ai->ai_addr, ai->ai_addrlen);
        *salen = ai->ai_addrlen;

        sa->sin.sin_port = htons(atoi(stport));

        freeaddrinfo(ai);
    }
    Jim_Free(sthost);

    return ret;
#else
    Jim_SetResultString(interp, "ipv6 not supported", -1);
    return JIM_ERR;
#endif
}

static int JimParseIpAddress(Jim_Interp *interp, const char *hostport, union sockaddr_any *sa, int *salen)
{
    /* An IPv4 addr/port looks like:
     *   192.168.1.5
     *   192.168.1.5:2000
     *   2000
     *
     * If the address is missing, INADDR_ANY is used.
     * If the port is missing, 0 is used (only useful for server sockets).
     */
    char *sthost = NULL;
    const char *stport;
    int ret = JIM_OK;

    stport = strrchr(hostport, ':');
    if (!stport) {
        /* No : so, the whole thing is the port */
        stport = hostport;
        sthost = Jim_StrDup("0.0.0.0");
    }
    else {
        sthost = Jim_StrDupLen(hostport, stport - hostport);
        stport++;
    }

    {
#ifdef HAVE_GETADDRINFO
        struct addrinfo req;
        struct addrinfo *ai;
        memset(&req, '\0', sizeof(req));
        req.ai_family = PF_INET;

        if (getaddrinfo(sthost, NULL, &req, &ai)) {
            ret = JIM_ERR;
        }
        else {
            memcpy(&sa->sin, ai->ai_addr, ai->ai_addrlen);
            *salen = ai->ai_addrlen;
            freeaddrinfo(ai);
        }
#else
        struct hostent *he;

        ret = JIM_ERR;

        if ((he = gethostbyname(sthost)) != NULL) {
            if (he->h_length == sizeof(sa->sin.sin_addr)) {
                *salen = sizeof(sa->sin);
                sa->sin.sin_family= he->h_addrtype;
                memcpy(&sa->sin.sin_addr, he->h_addr, he->h_length); /* set address */
                ret = JIM_OK;
            }
        }
#endif

        sa->sin.sin_port = htons(atoi(stport));
    }
    Jim_Free(sthost);

    if (ret != JIM_OK) {
        Jim_SetResultFormatted(interp, "Not a valid address: %s", hostport);
    }

    return ret;
}

#ifdef HAVE_SYS_UN_H
static int JimParseDomainAddress(Jim_Interp *interp, const char *path, struct sockaddr_un *sa)
{
    sa->sun_family = PF_UNIX;
    snprintf(sa->sun_path, sizeof(sa->sun_path), "%s", path);

    return JIM_OK;
}
#endif

/**
 * Format that address in 'sa' as a string and store in variable 'varObjPtr'
 */
static int JimFormatIpAddress(Jim_Interp *interp, Jim_Obj *varObjPtr, const union sockaddr_any *sa)
{
    /* INET6_ADDRSTRLEN is 46. Add some for [] and port */
    char addrbuf[60];

#if IPV6
    if (sa->sa.sa_family == PF_INET6) {
        addrbuf[0] = '[';
        /* Allow 9 for []:65535\0 */
        inet_ntop(sa->sa.sa_family, &sa->sin6.sin6_addr, addrbuf + 1, sizeof(addrbuf) - 9);
        snprintf(addrbuf + strlen(addrbuf), 8, "]:%d", ntohs(sa->sin.sin_port));
    }
    else
#endif
    if (sa->sa.sa_family == PF_INET) {
        /* Allow 7 for :65535\0 */
        inet_ntop(sa->sa.sa_family, &sa->sin.sin_addr, addrbuf, sizeof(addrbuf) - 7);
        snprintf(addrbuf + strlen(addrbuf), 7, ":%d", ntohs(sa->sin.sin_port));
    }
    else {
        /* recvfrom still works on unix domain sockets, etc */
        addrbuf[0] = 0;
    }

    return Jim_SetVariable(interp, varObjPtr, Jim_NewStringObj(interp, addrbuf, -1));
}

#endif /* JIM_BOOTSTRAP */

static void JimAioSetError(Jim_Interp *interp, Jim_Obj *name)
{
#if defined(JIM_SSL)
    unsigned long err;

    err = ERR_get_error();
    if (err != 0) {
        if (name) {
            Jim_SetResultFormatted(interp, "%#s: %s", name, ERR_error_string(err, NULL));
        }
        else {
            Jim_SetResultString(interp, ERR_error_string(err, NULL), -1);
        }
        return;
    }
#endif

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

#ifdef jim_ext_eventloop
    /* remove all existing EventHandlers */
    Jim_DeleteFileHandler(interp, af->fp, JIM_EVENT_READABLE | JIM_EVENT_WRITABLE | JIM_EVENT_EXCEPTION);
#endif

#if defined(JIM_SSL)
    if (af->ssl != NULL) {
        SSL_free(af->ssl);
    }
#endif

    if (!(af->openFlags & AIO_KEEPOPEN)) {
        fclose(af->fp);
    }

    Jim_Free(af);
}

static int JimCheckStreamError(Jim_Interp *interp, AioFile *af)
{
#if defined(JIM_SSL)
    unsigned long err;

    if (af->ssl != NULL) {
        err = ERR_peek_error();
        if (err == 0) {
            return JIM_OK;
        }
    }
#endif

    if (!ferror(af->fp)) {
        return JIM_OK;
    }
    clearerr(af->fp);
    /* EAGAIN and similar are not error conditions. Just treat them like eof */
    if (feof(af->fp) || errno == EAGAIN || errno == EINTR) {
        return JIM_OK;
    }
#ifdef ECONNRESET
    if (errno == ECONNRESET) {
        return JIM_OK;
    }
#endif
#ifdef ECONNABORTED
    if (errno != ECONNABORTED) {
        return JIM_OK;
    }
#endif
    JimAioSetError(interp, af->filename);
    return JIM_ERR;
}

static int aio_cmd_read(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char buf[AIO_BUF_LEN];
    Jim_Obj *objPtr;
    int nonewline = 0;
    jim_wide neededLen = -1;         /* -1 is "read as much as possible" */

    if (argc && Jim_CompareStringImmediate(interp, argv[0], "-nonewline")) {
        nonewline = 1;
        argv++;
        argc--;
    }
    if (argc == 1) {
        if (Jim_GetWide(interp, argv[0], &neededLen) != JIM_OK)
            return JIM_ERR;
        if (neededLen < 0) {
            Jim_SetResultString(interp, "invalid parameter: negative len", -1);
            return JIM_ERR;
        }
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
        }
        else {
            readlen = (neededLen > AIO_BUF_LEN ? AIO_BUF_LEN : neededLen);
        }
#if defined(JIM_SSL)
        if (af->ssl == NULL) {
            retval = SSL_read(af->ssl, buf, readlen);
        } else {
#else
        if (1) {
#endif
            retval = fread(buf, 1, readlen, af->fp);
        }
        if (retval > 0) {
            Jim_AppendString(interp, objPtr, buf, retval);
            if (neededLen != -1) {
                neededLen -= retval;
            }
        }
        if (retval != readlen)
            break;
    }
    /* Check for error conditions */
    if (JimCheckStreamError(interp, af)) {
        Jim_FreeNewObj(interp, objPtr);
        return JIM_ERR;
    }
    if (nonewline) {
        int len;
        const char *s = Jim_GetString(objPtr, &len);

        if (len > 0 && s[len - 1] == '\n') {
            objPtr->length--;
            objPtr->bytes[objPtr->length] = '\0';
        }
    }
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

AioFile *Jim_AioFile(Jim_Interp *interp, Jim_Obj *command)
{
    Jim_Cmd *cmdPtr = Jim_GetCommand(interp, command, JIM_ERRMSG);

    /* XXX: There ought to be a supported API for this */
    if (cmdPtr && !cmdPtr->isproc && cmdPtr->u.native.cmdProc == JimAioSubCmdProc) {
        return (AioFile *) cmdPtr->u.native.privData;
    }
    Jim_SetResultFormatted(interp, "Not a filehandle: \"%#s\"", command);
    return NULL;
}

FILE *Jim_AioFilehandle(Jim_Interp *interp, Jim_Obj *command)
{
    AioFile *af;

    af = Jim_AioFile(interp, command);
    if (af == NULL) {
        return NULL;
    }

    return af->fp;
}

static int aio_cmd_copy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    jim_wide count = 0;
    jim_wide maxlen = JIM_WIDE_MAX;
    AioFile *outf = Jim_AioFile(interp, argv[0]);

    if (outf->fp == NULL) {
        return JIM_ERR;
    }

    if (argc == 2) {
        if (Jim_GetWide(interp, argv[1], &maxlen) != JIM_OK) {
            return JIM_ERR;
        }
    }

#if defined(JIM_SSL)
    if (af->ssl != NULL) {
        if (outf->ssl != NULL) {
            while (count < maxlen) {
                unsigned char ch;
                if (SSL_read(af->ssl, &ch, 1) != 1 || SSL_write(outf->ssl, &ch, 1) != 1) {
                    break;
                }
                count++;
            }
        } else {
            while (count < maxlen) {
                unsigned char ch;
                if (SSL_read(af->ssl, &ch, 1) != 1 || fputc((int)ch, outf->fp) == EOF) {
                    break;
                }
                count++;
            }
        }
    } else {
        if (outf->ssl != NULL) {
            while (count < maxlen) {
                int ch = fgetc(af->fp);

                if (ch == EOF || SSL_write(outf->ssl, &ch, 1) != 1) {
                    break;
                }
                count++;
            }
        } else {
#else
    {
        if (1) {
#endif
            while (count < maxlen) {
                int ch = fgetc(af->fp);

                if (ch == EOF || fputc(ch, outf->fp) == EOF) {
                    break;
                }
                count++;
            }
        }
    }

#if defined(JIM_SSL)
    if (ERR_peek_error() != 0) {
        Jim_SetResultFormatted(interp, "error while reading: %s", ERR_error_string(ERR_get_error(), NULL));
        return JIM_ERR;
    }
#endif

    if (ferror(af->fp)) {
        Jim_SetResultFormatted(interp, "error while reading: %s", strerror(errno));
        clearerr(af->fp);
        return JIM_ERR;
    }

    if (ferror(outf->fp)) {
        Jim_SetResultFormatted(interp, "error while writing: %s", strerror(errno));
        clearerr(outf->fp);
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, count);

    return JIM_OK;
}

static int aio_cmd_gets(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char buf[AIO_BUF_LEN];
    Jim_Obj *objPtr;
    int len;

    errno = 0;

    objPtr = Jim_NewStringObj(interp, NULL, 0);
#if defined(JIM_SSL)
    if (af->ssl != NULL) {
        while (1) {
            /* TODO: make this more efficient, possible using SSL_pending() and
             * SSL_peek() */
            len = SSL_read(af->ssl, buf, 1);
            if (len != 1)
                break;

            if (buf[0] == '\n')
                break;

            buf[1] = '\0';
            Jim_AppendString(interp, objPtr, buf, 1);
        }
    } else {
#else
    if (1) {
#endif
        while (1) {
            buf[AIO_BUF_LEN - 1] = '_';

            if (fgets(buf, AIO_BUF_LEN, af->fp) == NULL)
                break;

            if (buf[AIO_BUF_LEN - 1] == '\0' && buf[AIO_BUF_LEN - 2] != '\n') {
                Jim_AppendString(interp, objPtr, buf, AIO_BUF_LEN - 1);
            }
            else {
                len = strlen(buf);

                if (len && (buf[len - 1] == '\n')) {
                    /* strip "\n" */
                    len--;
                }

                Jim_AppendString(interp, objPtr, buf, len);
                break;
            }
        }
    }
    if (JimCheckStreamError(interp, af)) {
        /* I/O error */
        Jim_FreeNewObj(interp, objPtr);
        return JIM_ERR;
    }

    if (argc) {
        if (Jim_SetVariable(interp, argv[0], objPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, objPtr);
            return JIM_ERR;
        }

        len = Jim_Length(objPtr);

        if (len == 0 && feof(af->fp)) {
            /* On EOF returns -1 if varName was specified */
            len = -1;
        }
        Jim_SetResultInt(interp, len);
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
#if defined(JIM_SSL)
    if (af->ssl != NULL) {
        if (SSL_write(af->ssl, wdata, wlen) == wlen) {
            if (argc == 2 || SSL_write(af->ssl, "\n", 1) == 1) {
                return JIM_OK;
            }
        }
    } else {
#else
    if (1) {
#endif
        if (fwrite(wdata, 1, wlen, af->fp) == (unsigned)wlen) {
            if (argc == 2 || putc('\n', af->fp) != EOF) {
                return JIM_OK;
            }
        }
    }
    JimAioSetError(interp, af->filename);
    return JIM_ERR;
}

static int aio_cmd_isatty(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
#ifdef HAVE_ISATTY
    AioFile *af = Jim_CmdPrivData(interp);
    Jim_SetResultInt(interp, isatty(fileno(af->fp)));
#else
    Jim_SetResultInt(interp, 0);
#endif

    return JIM_OK;
}

#if !defined(JIM_ANSIC) && !defined(JIM_BOOTSTRAP)
static int aio_cmd_recvfrom(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char *buf;
    union sockaddr_any sa;
    long len;
    socklen_t salen = sizeof(sa);
    int rlen;

    if (Jim_GetLong(interp, argv[0], &len) != JIM_OK) {
        return JIM_ERR;
    }

    buf = Jim_Alloc(len + 1);

    rlen = recvfrom(fileno(af->fp), buf, len, 0, &sa.sa, &salen);
    if (rlen < 0) {
        Jim_Free(buf);
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }
    buf[rlen] = 0;
    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, buf, rlen));

    if (argc > 1) {
        return JimFormatIpAddress(interp, argv[1], &sa);
    }

    return JIM_OK;
}


static int aio_cmd_sendto(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int wlen;
    int len;
    const char *wdata;
    union sockaddr_any sa;
    const char *addr = Jim_String(argv[1]);
    int salen;

    if (IPV6 && af->addr_family == PF_INET6) {
        if (JimParseIPv6Address(interp, addr, &sa, &salen) != JIM_OK) {
            return JIM_ERR;
        }
    }
    else if (JimParseIpAddress(interp, addr, &sa, &salen) != JIM_OK) {
        return JIM_ERR;
    }
    wdata = Jim_GetString(argv[0], &wlen);

    /* Note that we don't validate the socket type. Rely on sendto() failing if appropriate */
    len = sendto(fileno(af->fp), wdata, wlen, 0, &sa.sa, salen);
    if (len < 0) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, len);
    return JIM_OK;
}

static int aio_cmd_accept(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int sock;
    union sockaddr_any sa;
    socklen_t addrlen = sizeof(sa);

    sock = accept(af->fd, &sa.sa, &addrlen);
    if (sock < 0) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }

    if (argc > 0) {
        if (JimFormatIpAddress(interp, argv[0], &sa) != JIM_OK) {
            return JIM_ERR;
        }
    }

    /* Create the file command */
    return JimMakeChannel(interp, NULL, sock, Jim_NewStringObj(interp, "accept", -1),
        "aio.sockstream%ld", af->addr_family, "r+", NULL);
}

static int aio_cmd_listen(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    long backlog;

    if (Jim_GetLong(interp, argv[0], &backlog) != JIM_OK) {
        return JIM_ERR;
    }

    if (listen(af->fd, backlog)) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }

    return JIM_OK;
}
#endif /* JIM_BOOTSTRAP */

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
    if (argc == 3) {
#if !defined(JIM_ANSIC) && defined(HAVE_SHUTDOWN)
        static const char * const options[] = { "r", "w", NULL };
        enum { OPT_R, OPT_W, };
        int option;
        AioFile *af = Jim_CmdPrivData(interp);

        if (Jim_GetEnum(interp, argv[2], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
        if (shutdown(af->fd, option == OPT_R ? SHUT_RD : SHUT_WR) == 0) {
            return JIM_OK;
        }
        JimAioSetError(interp, NULL);
#else
        Jim_SetResultString(interp, "async close not supported", -1);
#endif
        return JIM_ERR;
    }

    return Jim_DeleteCommand(interp, Jim_String(argv[0]));
}

static int aio_cmd_seek(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int orig = SEEK_SET;
    jim_wide offset;

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
    if (Jim_GetWide(interp, argv[0], &offset) != JIM_OK) {
        return JIM_ERR;
    }
    if (fseeko(af->fp, offset, orig) == -1) {
        JimAioSetError(interp, af->filename);
        return JIM_ERR;
    }
    return JIM_OK;
}

static int aio_cmd_tell(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    Jim_SetResultInt(interp, ftello(af->fp));
    return JIM_OK;
}

static int aio_cmd_filename(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    Jim_SetResult(interp, af->filename);
    return JIM_OK;
}

#ifdef O_NDELAY
static int aio_cmd_ndelay(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    int fmode = fcntl(af->fd, F_GETFL);

    if (argc) {
        long nb;

        if (Jim_GetLong(interp, argv[0], &nb) != JIM_OK) {
            return JIM_ERR;
        }
        if (nb) {
            fmode |= O_NDELAY;
        }
        else {
            fmode &= ~O_NDELAY;
        }
        (void)fcntl(af->fd, F_SETFL, fmode);
    }
    Jim_SetResultInt(interp, (fmode & O_NONBLOCK) ? 1 : 0);
    return JIM_OK;
}
#endif

#ifdef HAVE_FSYNC
static int aio_cmd_sync(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    fflush(af->fp);
    fsync(af->fd);
    return JIM_OK;
}
#endif

static int aio_cmd_buffering(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    static const char * const options[] = {
        "none",
        "line",
        "full",
        NULL
    };
    enum
    {
        OPT_NONE,
        OPT_LINE,
        OPT_FULL,
    };
    int option;

    if (Jim_GetEnum(interp, argv[0], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }
    switch (option) {
        case OPT_NONE:
            setvbuf(af->fp, NULL, _IONBF, 0);
            break;
        case OPT_LINE:
            setvbuf(af->fp, NULL, _IOLBF, BUFSIZ);
            break;
        case OPT_FULL:
            setvbuf(af->fp, NULL, _IOFBF, BUFSIZ);
            break;
    }
    return JIM_OK;
}

#ifdef jim_ext_eventloop
static void JimAioFileEventFinalizer(Jim_Interp *interp, void *clientData)
{
    Jim_Obj **objPtrPtr = clientData;

    Jim_DecrRefCount(interp, *objPtrPtr);
    *objPtrPtr = NULL;
}

static int JimAioFileEventHandler(Jim_Interp *interp, void *clientData, int mask)
{
    Jim_Obj **objPtrPtr = clientData;

    return Jim_EvalObjBackground(interp, *objPtrPtr);
}

static int aio_eventinfo(Jim_Interp *interp, AioFile * af, unsigned mask, Jim_Obj **scriptHandlerObj,
    int argc, Jim_Obj * const *argv)
{
    if (argc == 0) {
        /* Return current script */
        if (*scriptHandlerObj) {
            Jim_SetResult(interp, *scriptHandlerObj);
        }
        return JIM_OK;
    }

    if (*scriptHandlerObj) {
        /* Delete old handler */
        Jim_DeleteFileHandler(interp, af->fp, mask);
    }

    /* Now possibly add the new script(s) */
    if (Jim_Length(argv[0]) == 0) {
        /* Empty script, so done */
        return JIM_OK;
    }

    /* A new script to add */
    Jim_IncrRefCount(argv[0]);
    *scriptHandlerObj = argv[0];

    Jim_CreateFileHandler(interp, af->fp, mask,
        JimAioFileEventHandler, scriptHandlerObj, JimAioFileEventFinalizer);

    return JIM_OK;
}

static int aio_cmd_readable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_READABLE, &af->rEvent, argc, argv);
}

static int aio_cmd_writable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_WRITABLE, &af->wEvent, argc, argv);
}

static int aio_cmd_onexception(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_EXCEPTION, &af->eEvent, argc, argv);
}
#endif

#if defined(JIM_SSL)
static int aio_cmd_ssl(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    SSL *ssl;
    int fd;
    int server = 0;

    if (argc == 5) {
        if (!Jim_CompareStringImmediate(interp, argv[2], "-server")) {
            return JIM_ERR;
        }
        server = 1;
    }
    else {
        if (argc != 1) {
            Jim_WrongNumArgs(interp, 2, argv, "?-server? ?cert? ?priv?");
            return JIM_ERR;
        }
    }

    fd = fileno(af->fp);
#if defined(HAVE_SOCKETPAIR)
    fd = dup(fd);
    if (fd < 0) {
        return JIM_ERR;
    }
#endif

    ssl = SSL_new((SSL_CTX *)Jim_GetAssocData(interp, "ssl_ctx"));
    if (ssl == NULL) {
#if defined(HAVE_SOCKETPAIR)
        close(fd);
#endif
        Jim_SetResultString(interp, ERR_error_string(ERR_get_error(), NULL), -1);
        return JIM_ERR;
    }

    SSL_set_cipher_list(ssl, "ALL");
    SSL_set_verify(ssl, SSL_VERIFY_NONE, 0);

    if (SSL_set_fd(ssl, fileno(af->fp)) == 0) {
        goto out;
    }

    if (server) {
        if (SSL_use_certificate_file(ssl, Jim_String(argv[3]), SSL_FILETYPE_PEM) != 1) {
            goto out;
        }

        if (SSL_use_PrivateKey_file(ssl, Jim_String(argv[4]), SSL_FILETYPE_PEM) != 1) {
            goto out;
        }

        if (SSL_accept(ssl) != 1) {
            goto out;
        }
    }
    else {
        if (SSL_connect(ssl) != 1) {
            goto out;
        }
    }

    if (JimMakeChannel(interp, NULL, fd, NULL, "aio.sslstream%ld", af->addr_family, "r+", ssl) != JIM_OK) {
        goto out;
    }

    return JIM_OK;

out:
#if defined(HAVE_SOCKETPAIR)
    close(fd);
#endif
    SSL_free(ssl);
    Jim_SetResultString(interp, ERR_error_string(ERR_get_error(), NULL), -1);
    return JIM_ERR;
}

static int aio_cmd_verify(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    if (af->ssl == NULL || SSL_get_verify_result(af->ssl) == X509_V_OK) {
        return JIM_OK;
    }

    return JIM_ERR;
}
#endif

static const jim_subcmd_type aio_command_table[] = {
    {   "read",
        "?-nonewline? ?len?",
        aio_cmd_read,
        0,
        2,
        /* Description: Read and return bytes from the stream. To eof if no len. */
    },
    {   "copyto",
        "handle ?size?",
        aio_cmd_copy,
        1,
        2,
        /* Description: Copy up to 'size' bytes to the given filehandle, or to eof if no size. */
    },
    {   "gets",
        "?var?",
        aio_cmd_gets,
        0,
        1,
        /* Description: Read one line and return it or store it in the var */
    },
    {   "puts",
        "?-nonewline? str",
        aio_cmd_puts,
        1,
        2,
        /* Description: Write the string, with newline unless -nonewline */
    },
    {   "isatty",
        NULL,
        aio_cmd_isatty,
        0,
        0,
        /* Description: Is the file descriptor a tty? */
    },
#if !defined(JIM_ANSIC) && !defined(JIM_BOOTSTRAP)
    {   "recvfrom",
        "len ?addrvar?",
        aio_cmd_recvfrom,
        1,
        2,
        /* Description: Receive up to 'len' bytes on the socket. Sets 'addrvar' with receive address, if set */
    },
    {   "sendto",
        "str address",
        aio_cmd_sendto,
        2,
        2,
        /* Description: Send 'str' to the given address (dgram only) */
    },
    {   "accept",
        "?addrvar?",
        aio_cmd_accept,
        0,
        1,
        /* Description: Server socket only: Accept a connection and return stream */
    },
    {   "listen",
        "backlog",
        aio_cmd_listen,
        1,
        1,
        /* Description: Set the listen backlog for server socket */
    },
#endif /* JIM_BOOTSTRAP */
    {   "flush",
        NULL,
        aio_cmd_flush,
        0,
        0,
        /* Description: Flush the stream */
    },
    {   "eof",
        NULL,
        aio_cmd_eof,
        0,
        0,
        /* Description: Returns 1 if stream is at eof */
    },
    {   "close",
        "?r(ead)|w(rite)?",
        aio_cmd_close,
        0,
        1,
        JIM_MODFLAG_FULLARGV,
        /* Description: Closes the stream. */
    },
    {   "seek",
        "offset ?start|current|end",
        aio_cmd_seek,
        1,
        2,
        /* Description: Seeks in the stream (default 'current') */
    },
    {   "tell",
        NULL,
        aio_cmd_tell,
        0,
        0,
        /* Description: Returns the current seek position */
    },
    {   "filename",
        NULL,
        aio_cmd_filename,
        0,
        0,
        /* Description: Returns the original filename */
    },
#ifdef O_NDELAY
    {   "ndelay",
        "?0|1?",
        aio_cmd_ndelay,
        0,
        1,
        /* Description: Set O_NDELAY (if arg). Returns current/new setting. */
    },
#endif
#ifdef HAVE_FSYNC
    {   "sync",
        NULL,
        aio_cmd_sync,
        0,
        0,
        /* Description: Flush and fsync() the stream */
    },
#endif
    {   "buffering",
        "none|line|full",
        aio_cmd_buffering,
        1,
        1,
        /* Description: Sets buffering */
    },
#ifdef jim_ext_eventloop
    {   "readable",
        "?readable-script?",
        aio_cmd_readable,
        0,
        1,
        /* Description: Returns script, or invoke readable-script when readable, {} to remove */
    },
    {   "writable",
        "?writable-script?",
        aio_cmd_writable,
        0,
        1,
        /* Description: Returns script, or invoke writable-script when writable, {} to remove */
    },
    {   "onexception",
        "?exception-script?",
        aio_cmd_onexception,
        0,
        1,
        /* Description: Returns script, or invoke exception-script when oob data, {} to remove */
    },
#endif
#if defined(JIM_SSL)
    {   "ssl",
        "?-server? ?cert? ?priv?",
        aio_cmd_ssl,
        0,
        3,
        JIM_MODFLAG_FULLARGV
        /* Description: Wraps a stream socket with SSL/TLS and returns a new channel */
    },
    {   "verify",
        NULL,
        aio_cmd_verify,
        0,
        0,
        /* Description: Verifies the certificate of a SSL/TLS channel */
    },
#endif
    { NULL }
};

static int JimAioSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, aio_command_table, argc, argv), argc, argv);
}

static int JimAioOpenCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    const char *mode;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "filename ?mode?");
        return JIM_ERR;
    }

    mode = (argc == 3) ? Jim_String(argv[2]) : "r";

#ifdef jim_ext_tclcompat
    {
        const char *filename = Jim_String(argv[1]);

        /* If the filename starts with '|', use popen instead */
        if (*filename == '|') {
            Jim_Obj *evalObj[3];

            evalObj[0] = Jim_NewStringObj(interp, "::popen", -1);
            evalObj[1] = Jim_NewStringObj(interp, filename + 1, -1);
            evalObj[2] = Jim_NewStringObj(interp, mode, -1);

            return Jim_EvalObjVector(interp, 3, evalObj);
        }
    }
#endif
    return JimMakeChannel(interp, NULL, -1, argv[1], "aio.handle%ld", 0, mode, NULL);
}

/**
 * Creates a channel for fh/fd/filename.
 *
 * If fh is not NULL, uses that as the channel (and sets AIO_KEEPOPEN).
 * Otherwise, if fd is >= 0, uses that as the channel.
 * Otherwise opens 'filename' with mode 'mode'.
 *
 * hdlfmt is a sprintf format for the filehandle. Anything with %ld at the end will do.
 * mode is used for open or fdopen.
 *
 * Creates the command and sets the name as the current result.
 */
static int JimMakeChannel(Jim_Interp *interp, FILE *fh, int fd, Jim_Obj *filename,
    const char *hdlfmt, int family, const char *mode, void *ssl)
{
    AioFile *af;
    char buf[AIO_CMD_LEN];
    int openFlags = 0;

    if (fh) {
        openFlags = AIO_KEEPOPEN;
    }

    snprintf(buf, sizeof(buf), hdlfmt, Jim_GetId(interp));
    if (!filename) {
        filename = Jim_NewStringObj(interp, buf, -1);
    }

    Jim_IncrRefCount(filename);

    if (fh == NULL) {
#if !defined(JIM_ANSIC)
        if (fd >= 0) {
            fh = fdopen(fd, mode);
        }
        else
#endif
            fh = fopen(Jim_String(filename), mode);

        if (fh == NULL) {
            JimAioSetError(interp, filename);
#if !defined(JIM_ANSIC)
            if (fd >= 0) {
                close(fd);
            }
#endif
            Jim_DecrRefCount(interp, filename);
            return JIM_ERR;
        }
    }

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    memset(af, 0, sizeof(*af));
    af->fp = fh;
    af->fd = fileno(fh);
    af->filename = filename;
#ifdef FD_CLOEXEC
    if ((openFlags & AIO_KEEPOPEN) == 0) {
        (void)fcntl(af->fd, F_SETFD, FD_CLOEXEC);
    }
#endif
    af->openFlags = openFlags;
    af->addr_family = family;
    af->ssl = ssl;
    Jim_CreateCommand(interp, buf, JimAioSubCmdProc, af, JimAioDelProc);

    /* Note that the command must use the global namespace, even if
     * the current namespace is something different
     */
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return JIM_OK;
}

#if defined(HAVE_PIPE) || (defined(HAVE_SOCKETPAIR) && defined(HAVE_SYS_UN_H))
/**
 * Create a pair of channels. e.g. from pipe() or socketpair()
 */
static int JimMakeChannelPair(Jim_Interp *interp, int p[2], Jim_Obj *filename,
    const char *hdlfmt, int family, const char *mode[2])
{
    if (JimMakeChannel(interp, NULL, p[0], filename, hdlfmt, family, mode[0], NULL) == JIM_OK) {
        Jim_Obj *objPtr = Jim_NewListObj(interp, NULL, 0);
        Jim_ListAppendElement(interp, objPtr, Jim_GetResult(interp));

        if (JimMakeChannel(interp, NULL, p[1], filename, hdlfmt, family, mode[1], NULL) == JIM_OK) {
            Jim_ListAppendElement(interp, objPtr, Jim_GetResult(interp));
            Jim_SetResult(interp, objPtr);
            return JIM_OK;
        }
    }

    /* Can only be here if fdopen() failed */
    close(p[0]);
    close(p[1]);
    JimAioSetError(interp, NULL);
    return JIM_ERR;
}
#endif

#if !defined(JIM_ANSIC) && !defined(JIM_BOOTSTRAP)

static int JimAioSockCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
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
        "pair",
        NULL
    };
    enum
    {
        SOCK_UNIX,
        SOCK_UNIX_SERVER,
        SOCK_DGRAM_CLIENT,
        SOCK_DGRAM_SERVER,
        SOCK_STREAM_CLIENT,
        SOCK_STREAM_SERVER,
        SOCK_STREAM_PIPE,
        SOCK_STREAM_SOCKETPAIR,
    };
    int socktype;
    int sock;
    const char *hostportarg = NULL;
    int res;
    int on = 1;
    const char *mode = "r+";
    int family = PF_INET;
    Jim_Obj *argv0 = argv[0];
    int ipv6 = 0;

    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[1], "-ipv6")) {
        if (!IPV6) {
            Jim_SetResultString(interp, "ipv6 not supported", -1);
            return JIM_ERR;
        }
        ipv6 = 1;
        family = PF_INET6;
    }
    argc -= ipv6;
    argv += ipv6;

    if (argc < 2) {
      wrongargs:
        Jim_WrongNumArgs(interp, 1, &argv0, "?-ipv6? type ?address?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], socktypes, &socktype, "socket type", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;

    Jim_SetEmptyResult(interp);

    hdlfmt = "aio.sock%ld";

    if (argc > 2) {
        hostportarg = Jim_String(argv[2]);
    }

    switch (socktype) {
        case SOCK_DGRAM_CLIENT:
            if (argc == 2) {
                /* No address, so an unconnected dgram socket */
                sock = socket(family, SOCK_DGRAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                break;
            }
            /* fall through */
        case SOCK_STREAM_CLIENT:
            {
                union sockaddr_any sa;
                int salen;

                if (argc != 3) {
                    goto wrongargs;
                }

                if (ipv6) {
                    if (JimParseIPv6Address(interp, hostportarg, &sa, &salen) != JIM_OK) {
                        return JIM_ERR;
                    }
                }
                else if (JimParseIpAddress(interp, hostportarg, &sa, &salen) != JIM_OK) {
                    return JIM_ERR;
                }
                sock = socket(family, (socktype == SOCK_DGRAM_CLIENT) ? SOCK_DGRAM : SOCK_STREAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                res = connect(sock, &sa.sa, salen);
                if (res) {
                    JimAioSetError(interp, argv[2]);
                    close(sock);
                    return JIM_ERR;
                }
            }
            break;

        case SOCK_STREAM_SERVER:
        case SOCK_DGRAM_SERVER:
            {
                union sockaddr_any sa;
                int salen;

                if (argc != 3) {
                    goto wrongargs;
                }

                if (ipv6) {
                    if (JimParseIPv6Address(interp, hostportarg, &sa, &salen) != JIM_OK) {
                        return JIM_ERR;
                    }
                }
                else if (JimParseIpAddress(interp, hostportarg, &sa, &salen) != JIM_OK) {
                    return JIM_ERR;
                }
                sock = socket(family, (socktype == SOCK_DGRAM_SERVER) ? SOCK_DGRAM : SOCK_STREAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }

                /* Enable address reuse */
                setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on));

                res = bind(sock, &sa.sa, salen);
                if (res) {
                    JimAioSetError(interp, argv[2]);
                    close(sock);
                    return JIM_ERR;
                }
                if (socktype == SOCK_STREAM_SERVER) {
                    res = listen(sock, 5);
                    if (res) {
                        JimAioSetError(interp, NULL);
                        close(sock);
                        return JIM_ERR;
                    }
                }
                hdlfmt = "aio.socksrv%ld";
            }
            break;

#ifdef HAVE_SYS_UN_H
        case SOCK_UNIX:
            {
                struct sockaddr_un sa;
                socklen_t len;

                if (argc != 3 || ipv6) {
                    goto wrongargs;
                }

                if (JimParseDomainAddress(interp, hostportarg, &sa) != JIM_OK) {
                    JimAioSetError(interp, argv[2]);
                    return JIM_ERR;
                }
                family = PF_UNIX;
                sock = socket(PF_UNIX, SOCK_STREAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                len = strlen(sa.sun_path) + 1 + sizeof(sa.sun_family);
                res = connect(sock, (struct sockaddr *)&sa, len);
                if (res) {
                    JimAioSetError(interp, argv[2]);
                    close(sock);
                    return JIM_ERR;
                }
                hdlfmt = "aio.sockunix%ld";
                break;
            }

        case SOCK_UNIX_SERVER:
            {
                struct sockaddr_un sa;
                socklen_t len;

                if (argc != 3 || ipv6) {
                    goto wrongargs;
                }

                if (JimParseDomainAddress(interp, hostportarg, &sa) != JIM_OK) {
                    JimAioSetError(interp, argv[2]);
                    return JIM_ERR;
                }
                family = PF_UNIX;
                sock = socket(PF_UNIX, SOCK_STREAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                len = strlen(sa.sun_path) + 1 + sizeof(sa.sun_family);
                res = bind(sock, (struct sockaddr *)&sa, len);
                if (res) {
                    JimAioSetError(interp, argv[2]);
                    close(sock);
                    return JIM_ERR;
                }
                res = listen(sock, 5);
                if (res) {
                    JimAioSetError(interp, NULL);
                    close(sock);
                    return JIM_ERR;
                }
                hdlfmt = "aio.sockunixsrv%ld";
                break;
            }
#endif

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_SYS_UN_H)
        case SOCK_STREAM_SOCKETPAIR:
            {
                int p[2];
                static const char *mode[2] = { "r+", "r+" };

                if (argc != 2 || ipv6) {
                    goto wrongargs;
                }

                if (socketpair(PF_UNIX, SOCK_STREAM, 0, p) < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                return JimMakeChannelPair(interp, p, argv[1], "aio.sockpair%ld", PF_UNIX, mode);
            }
            break;
#endif

#if defined(HAVE_PIPE)
        case SOCK_STREAM_PIPE:
            {
                int p[2];
                static const char *mode[2] = { "r", "w" };

                if (argc != 2 || ipv6) {
                    goto wrongargs;
                }

                if (pipe(p) < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }

                return JimMakeChannelPair(interp, p, argv[1], "aio.pipe%ld", 0, mode);
            }
            break;
#endif

        default:
            Jim_SetResultString(interp, "Unsupported socket type", -1);
            return JIM_ERR;
    }

    return JimMakeChannel(interp, NULL, sock, argv[1], hdlfmt, family, mode, NULL);
}
#endif /* JIM_BOOTSTRAP */

/**
 * Returns the file descriptor of a writable, newly created temp file
 * or -1 on error.
 *
 * On success, leaves the filename in the interpreter result, otherwise
 * leaves an error message.
 */
int Jim_MakeTempFile(Jim_Interp *interp, const char *template)
{
#ifdef HAVE_MKSTEMP
    int fd;
    mode_t mask;
    Jim_Obj *filenameObj;

    if (template == NULL) {
        const char *tmpdir = getenv("TMPDIR");
        if (tmpdir == NULL || *tmpdir == '\0' || access(tmpdir, W_OK) != 0) {
            tmpdir = "/tmp/";
        }
        filenameObj = Jim_NewStringObj(interp, tmpdir, -1);
        if (tmpdir[0] && tmpdir[strlen(tmpdir) - 1] != '/') {
            Jim_AppendString(interp, filenameObj, "/", 1);
        }
        Jim_AppendString(interp, filenameObj, "tcl.tmp.XXXXXX", -1);
    }
    else {
        filenameObj = Jim_NewStringObj(interp, template, -1);
    }

    mask = umask(S_IXUSR | S_IRWXG | S_IRWXO);

    /* Update the template name directly with the filename */
    fd = mkstemp(filenameObj->bytes);
    umask(mask);
    if (fd < 0) {
        JimAioSetError(interp, filenameObj);
        Jim_FreeNewObj(interp, filenameObj);
        return -1;
    }

    Jim_SetResult(interp, filenameObj);
    return fd;
#else
    Jim_SetResultString(interp, "platform has no tempfile support", -1);
    return -1;
#endif
}

#if defined(JIM_SSL)
static int JimAioLoadSSLCertsCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "dir");
        return JIM_ERR;
    }

    if (SSL_CTX_load_verify_locations((SSL_CTX *)Jim_GetAssocData(interp, "ssl_ctx"), NULL, Jim_String(argv[1])) != 1) {
        Jim_SetResultString(interp, ERR_error_string(ERR_get_error(), NULL), -1);
        return JIM_ERR;
    }

    return JIM_OK;
}

static void JimAioSslContextDelProc(struct Jim_Interp *interp, void *privData)
{
    SSL_CTX_free((SSL_CTX *)privData);
    ERR_free_strings();
}
#endif

int Jim_aioInit(Jim_Interp *interp)
{
#if defined(JIM_SSL)
    SSL_CTX *ssl_ctx;
#endif

    if (Jim_PackageProvide(interp, "aio", "1.0", JIM_ERRMSG))
        return JIM_ERR;

#if defined(JIM_SSL)
    SSL_load_error_strings();
    SSL_library_init();
    ssl_ctx = SSL_CTX_new(TLSv1_2_method());
    if (ssl_ctx == NULL) {
        ERR_free_strings();
        return JIM_ERR;
    }

    if (Jim_SetAssocData(interp, "ssl_ctx", JimAioSslContextDelProc, ssl_ctx) != JIM_OK) {
        SSL_CTX_free(ssl_ctx);
        ERR_free_strings();
        return JIM_ERR;
    }

    Jim_CreateCommand(interp, "load_ssl_certs", JimAioLoadSSLCertsCommand, NULL, NULL);
#endif

    Jim_CreateCommand(interp, "open", JimAioOpenCommand, NULL, NULL);
#ifndef JIM_ANSIC
    Jim_CreateCommand(interp, "socket", JimAioSockCommand, NULL, NULL);
#endif

    /* Create filehandles for stdin, stdout and stderr */
    JimMakeChannel(interp, stdin, -1, NULL, "stdin", 0, "r", NULL);
    JimMakeChannel(interp, stdout, -1, NULL, "stdout", 0, "w", NULL);
    JimMakeChannel(interp, stderr, -1, NULL, "stderr", 0, "w", NULL);

    return JIM_OK;
}
