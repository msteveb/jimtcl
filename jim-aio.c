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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#include <sys/stat.h>
#endif
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
#ifdef HAVE_PTY_H
#include <pty.h>
#endif

#include "jim.h"
#include "jimiocompat.h"

#if defined(HAVE_SYS_SOCKET_H) && defined(HAVE_SELECT) && defined(HAVE_NETINET_IN_H) && defined(HAVE_NETDB_H) && defined(HAVE_ARPA_INET_H)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#define HAVE_SOCKETS
#elif defined (__MINGW32__)
/* currently mingw32 doesn't support sockets, but has pipe, fdopen */
#else
#define JIM_ANSIC
#endif

#if defined(JIM_SSL)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#ifdef HAVE_TERMIOS_H
#include <jim-tty.h>
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
#define AIO_NODELETE 2
#define AIO_EOF 4

#if defined(JIM_IPV6)
#define IPV6 1
#else
#define IPV6 0
#ifndef PF_INET6
#define PF_INET6 0
#endif
#endif
#if defined(HAVE_SYS_UN_H) && defined(PF_UNIX)
#define UNIX_SOCKETS 1
#else
#define UNIX_SOCKETS 0
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN JIM_PATH_LEN
#endif

#ifdef JIM_ANSIC
/* no fdopen() with ANSIC, so can't support these */
#undef HAVE_PIPE
#undef HAVE_SOCKETPAIR
#undef Jim_FileStat
#endif

#if defined(HAVE_SOCKETS) && !defined(JIM_BOOTSTRAP)
/* Avoid type punned pointers */
union sockaddr_any {
    struct sockaddr sa;
    struct sockaddr_in sin;
#if IPV6
    struct sockaddr_in6 sin6;
#endif
#if UNIX_SOCKETS
    struct sockaddr_un sun;
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

struct AioFile;

typedef struct {
    int (*writer)(struct AioFile *af, const char *buf, int len);
    int (*reader)(struct AioFile *af, char *buf, int len);
    const char *(*getline)(struct AioFile *af, char *buf, int len);
    int (*error)(const struct AioFile *af);
    const char *(*strerror)(struct AioFile *af);
    int (*verify)(struct AioFile *af);
    int (*eof)(struct AioFile *af);
    int (*pending)(struct AioFile *af);
} JimAioFopsType;

typedef struct AioFile
{
    FILE *fp;
    Jim_Obj *filename;
    int type;
    int flags;              /* AIO_KEEPOPEN? keep FILE* */
    int fd;
    int addr_family;
    void *ssl;
    const JimAioFopsType *fops;
    Jim_Obj *getline_partial;      /* In case of fgets() returning EAGAIN, partial line stored here */
} AioFile;

static int stdio_writer(struct AioFile *af, const char *buf, int len)
{
    return fwrite(buf, 1, len, af->fp);
}

static int stdio_reader(struct AioFile *af, char *buf, int len)
{
    return fread(buf, 1, len, af->fp);
}

static const char *stdio_getline(struct AioFile *af, char *buf, int len)
{
    return fgets(buf, len, af->fp);
}

static int stdio_error(const AioFile *af)
{
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
    if (errno == ECONNABORTED) {
        return JIM_OK;
    }
#endif
    return JIM_ERR;
}

static const char *stdio_strerror(struct AioFile *af)
{
    return strerror(errno);
}

static int stdio_eof(struct AioFile *af)
{
    return feof(af->fp);
}

static const JimAioFopsType stdio_fops = {
    stdio_writer,
    stdio_reader,
    stdio_getline,
    stdio_error,
    stdio_strerror,
    NULL, /* verify */
    stdio_eof,
    NULL, /* pending */
};

#if defined(JIM_SSL) && !defined(JIM_BOOTSTRAP)

static SSL_CTX *JimAioSslCtx(Jim_Interp *interp);

static int ssl_writer(struct AioFile *af, const char *buf, int len)
{
    return SSL_write(af->ssl, buf, len);
}

static int ssl_pending(struct AioFile *af)
{
    return SSL_pending(af->ssl);
}

static int ssl_reader(struct AioFile *af, char *buf, int len)
{
    int ret = SSL_read(af->ssl, buf, len);
    switch (SSL_get_error(af->ssl, ret)) {
    case SSL_ERROR_NONE:
        return ret;
    case SSL_ERROR_SYSCALL:
    case SSL_ERROR_ZERO_RETURN:
        if (errno != EAGAIN) {
            af->flags |= AIO_EOF;
        }
        return 0;
    case SSL_ERROR_SSL:
    default:
        if (errno == EAGAIN) {
            return 0;
        }
        af->flags |= AIO_EOF;
        return -1;
    }
}

static int ssl_eof(struct AioFile *af)
{
    return (af->flags & AIO_EOF);
}

static const char *ssl_getline(struct AioFile *af, char *buf, int len)
{
    size_t i;
    for (i = 0; i < len - 1 && !ssl_eof(af); i++) {
        int ret = ssl_reader(af, &buf[i], 1);
        if (ret != 1) {
            break;
        }
        if (buf[i] == '\n') {
            i++;
            break;
        }
    }
    buf[i] = '\0';
    if (i == 0 && ssl_eof(af)) {
        return NULL;
    }
    return buf;
}

static int ssl_error(const struct AioFile *af)
{
    int ret = SSL_get_error(af->ssl, 0);
	/* XXX should we be following the same logic as ssl_reader() here? */
    if (ret == SSL_ERROR_ZERO_RETURN || ret == SSL_ERROR_NONE) {
		return JIM_OK;
	}
    if (ret == SSL_ERROR_SYSCALL) {
        return stdio_error(af);
    }
    return JIM_ERR;
}

static const char *ssl_strerror(struct AioFile *af)
{
    int err = ERR_get_error();

    if (err) {
        return ERR_error_string(err, NULL);
    }
    else {
        return stdio_strerror(af);
    }
}

static int ssl_verify(struct AioFile *af)
{
    X509 *cert;

    cert = SSL_get_peer_certificate(af->ssl);
    if (!cert) {
        return JIM_ERR;
    }
    X509_free(cert);

    if (SSL_get_verify_result(af->ssl) == X509_V_OK) {
        return JIM_OK;
    }

    return JIM_ERR;
}

static const JimAioFopsType ssl_fops = {
    ssl_writer,
    ssl_reader,
    ssl_getline,
    ssl_error,
    ssl_strerror,
    ssl_verify,
    ssl_eof,
    ssl_pending,
};
#endif /* JIM_BOOTSTRAP */

static int JimAioSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);
static AioFile *JimMakeChannel(Jim_Interp *interp, FILE *fh, int fd, Jim_Obj *filename,
    const char *hdlfmt, int family, const char *mode, int flags);

#if defined(HAVE_SOCKETS) && !defined(JIM_BOOTSTRAP)
#ifndef HAVE_GETADDRINFO
/*
 * Poor man's getaddrinfo().
 * hints->ai_family must be set and must be PF_INET or PF_INET6
 * Only returns the first matching result.
 * servname must be numeric.
 */
struct addrinfo {
    int ai_family;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr; /* simply points to ai_storage */
    union sockaddr_any ai_storage;
};

static int getaddrinfo(const char *hostname, const char *servname,
    const struct addrinfo *hints, struct addrinfo **res)
{
    struct hostent *he;
    char *end;
    unsigned long port = strtoul(servname, &end, 10);
    if (port == 0 || port > 65536 || *end) {
        errno = EINVAL;
        return -1;
    }

    if ((he = gethostbyname(hostname)) != NULL) {
        int i;
        for (i = 0; he->h_addr_list[i]; i++) {
            if (he->h_addrtype == hints->ai_family) {
                struct addrinfo *ai = malloc(sizeof(*ai));
                memset(ai, 0, sizeof(*ai));
                ai->ai_family = he->h_addrtype;
                ai->ai_addr = &ai->ai_storage.sa;
                if (ai->ai_family == PF_INET) {
                    ai->ai_addrlen = sizeof(ai->ai_storage.sin);
                    ai->ai_storage.sin.sin_family = he->h_addrtype;
                    assert(sizeof(ai->ai_storage.sin.sin_addr) == he->h_length);
                    memcpy(&ai->ai_storage.sin.sin_addr, he->h_addr_list[i], he->h_length);
                    ai->ai_storage.sin.sin_port = htons(port);
                }
#if IPV6
                else {
                    ai->ai_addrlen = sizeof(ai->ai_storage.sin6);
                    ai->ai_storage.sin6.sin6_family = he->h_addrtype;
                    assert(sizeof(ai->ai_storage.sin6.sin6_addr) == he->h_length);
                    memcpy(&ai->ai_storage.sin6.sin6_addr, he->h_addr_list[i], he->h_length);
                    ai->ai_storage.sin6.sin6_port = htons(port);
                }
#endif
                *res = ai;
                return 0;
            }
        }
    }
    return ENOENT;
}

static void freeaddrinfo(struct addrinfo *ai)
{
    free(ai);
}
#endif

static int JimParseIPv6Address(Jim_Interp *interp, int socktype, const char *hostport, union sockaddr_any *sa, socklen_t *salen)
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
    req.ai_socktype = socktype;

    if (getaddrinfo(sthost, stport, &req, &ai)) {
        Jim_SetResultFormatted(interp, "Not a valid address: %s:%s", sthost, stport);
        ret = JIM_ERR;
    }
    else {
        memcpy(&sa->sin6, ai->ai_addr, ai->ai_addrlen);
        *salen = ai->ai_addrlen;
        freeaddrinfo(ai);
    }
    Jim_Free(sthost);

    return ret;
#else
    Jim_SetResultString(interp, "ipv6 not supported", -1);
    return JIM_ERR;
#endif
}

static int JimParseIpAddress(Jim_Interp *interp, int socktype, const char *hostport, union sockaddr_any *sa, socklen_t *salen)
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
    struct addrinfo req;
    struct addrinfo *ai;

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

    memset(&req, '\0', sizeof(req));
    req.ai_family = PF_INET;
    req.ai_socktype = socktype;

    if (getaddrinfo(sthost, stport, &req, &ai)) {
        ret = JIM_ERR;
    }
    else {
        memcpy(&sa->sin, ai->ai_addr, ai->ai_addrlen);
        *salen = ai->ai_addrlen;
        freeaddrinfo(ai);
    }
    Jim_Free(sthost);

    if (ret != JIM_OK) {
        Jim_SetResultFormatted(interp, "Not a valid address: %s", hostport);
    }

    return ret;
}

#if UNIX_SOCKETS
static int JimParseDomainAddress(Jim_Interp *interp, const char *path, union sockaddr_any *sa, socklen_t *salen)
{
    sa->sun.sun_family = PF_UNIX;
    snprintf(sa->sun.sun_path, sizeof(sa->sun.sun_path), "%s", path);
    *salen = strlen(sa->sun.sun_path) + 1 + sizeof(sa->sun.sun_family);

    return JIM_OK;
}
#endif

static int JimParseSocketAddress(Jim_Interp *interp, int family, int socktype, const char *addr, union sockaddr_any *sa, socklen_t *salen)
{
    switch (family) {
#if UNIX_SOCKETS
        case PF_UNIX:
            return JimParseDomainAddress(interp, addr, sa, salen);
#endif
        case PF_INET6:
            return JimParseIPv6Address(interp, socktype, addr, sa, salen);
        case PF_INET:
            return JimParseIpAddress(interp, socktype, addr, sa, salen);
    }
    return JIM_ERR;
}

/**
 * Format that address in 'sa' as a string and return it as a zero-refcount object.
 *
 */
static Jim_Obj *JimFormatSocketAddress(Jim_Interp *interp, const union sockaddr_any *sa, socklen_t salen)
{
    /* INET6_ADDRSTRLEN is 46. Add some for [] and port */
    char addrbuf[60];
    const char *addr = addrbuf;
    int addrlen = -1;

    switch (sa->sa.sa_family) {
#if UNIX_SOCKETS
        case PF_UNIX:
            addr = sa->sun.sun_path;
            addrlen = salen - 1 - sizeof(sa->sun.sun_family);
            if (addrlen < 0) {
                addrlen = 0;
            }
            break;
#endif
#if IPV6
        case PF_INET6:
            addrbuf[0] = '[';
            /* Allow 9 for []:65535\0 */
            inet_ntop(sa->sa.sa_family, &sa->sin6.sin6_addr, addrbuf + 1, sizeof(addrbuf) - 9);
            snprintf(addrbuf + strlen(addrbuf), 8, "]:%d", ntohs(sa->sin6.sin6_port));
            break;
#endif
        case PF_INET:
            /* Allow 7 for :65535\0 */
            inet_ntop(sa->sa.sa_family, &sa->sin.sin_addr, addrbuf, sizeof(addrbuf) - 7);
            snprintf(addrbuf + strlen(addrbuf), 7, ":%d", ntohs(sa->sin.sin_port));
            break;

        default:
            /* Otherwise just an empty address */
            addr = "";
            break;
    }

    return Jim_NewStringObj(interp, addr, addrlen);
}

static int JimSetVariableSocketAddress(Jim_Interp *interp, Jim_Obj *varObjPtr, const union sockaddr_any *sa, socklen_t salen)
{
    int ret;
    Jim_Obj *objPtr = JimFormatSocketAddress(interp, sa, salen);
    Jim_IncrRefCount(objPtr);
    ret = Jim_SetVariable(interp, varObjPtr, objPtr);
    Jim_DecrRefCount(interp, objPtr);
    return ret;
}

static Jim_Obj *aio_sockname(Jim_Interp *interp, AioFile *af)
{
    union sockaddr_any sa;
    socklen_t salen = sizeof(sa);

    if (getsockname(af->fd, &sa.sa, &salen) < 0) {
        return NULL;
    }
    return JimFormatSocketAddress(interp, &sa, salen);
}
#endif /* JIM_BOOTSTRAP */

static const char *JimAioErrorString(AioFile *af)
{
    if (af && af->fops)
        return af->fops->strerror(af);

    return strerror(errno);
}

static void JimAioSetError(Jim_Interp *interp, Jim_Obj *name)
{
    AioFile *af = Jim_CmdPrivData(interp);

    if (name) {
        Jim_SetResultFormatted(interp, "%#s: %s", name, JimAioErrorString(af));
    }
    else {
        Jim_SetResultString(interp, JimAioErrorString(af), -1);
    }
}

static int JimCheckStreamError(Jim_Interp *interp, AioFile *af)
{
    int ret = 0;
    if (!af->fops->eof(af)) {
        ret = af->fops->error(af);
        if (ret) {
            JimAioSetError(interp, af->filename);
        }
    }
    return ret;
}

static void JimAioDelProc(Jim_Interp *interp, void *privData)
{
    AioFile *af = privData;

    JIM_NOTUSED(interp);

#if UNIX_SOCKETS
    if (af->addr_family == PF_UNIX && (af->flags & AIO_NODELETE) == 0) {
        /* If this is bound, delete the socket file now */
        Jim_Obj *filenameObj = aio_sockname(interp, af);
        if (filenameObj) {
            if (Jim_Length(filenameObj)) {
                remove(Jim_String(filenameObj));
            }
            Jim_FreeNewObj(interp, filenameObj);
        }
    }
#endif

    Jim_DecrRefCount(interp, af->filename);

#ifdef jim_ext_eventloop
    /* remove all existing EventHandlers */
    Jim_DeleteFileHandler(interp, af->fd, JIM_EVENT_READABLE | JIM_EVENT_WRITABLE | JIM_EVENT_EXCEPTION);
#endif

#if defined(JIM_SSL)
    if (af->ssl != NULL) {
        SSL_free(af->ssl);
    }
#endif
    if (!(af->flags & AIO_KEEPOPEN)) {
        fclose(af->fp);
    }
    if (af->getline_partial) {
        Jim_FreeNewObj(interp, af->getline_partial);
    }

    Jim_Free(af);
}

static int aio_cmd_read(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char buf[AIO_BUF_LEN];
    Jim_Obj *objPtr;
    int nonewline = 0;
    int pending = 0;
    jim_wide neededLen = -1;         /* -1 is "read as much as possible" */
    static const char * const options[] = { "-pending", "-nonewline", NULL };
    enum { OPT_PENDING, OPT_NONEWLINE };
    int option;

    if (argc) {
        if (*Jim_String(argv[0]) == '-') {
            if (Jim_GetEnum(interp, argv[0], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
                return JIM_ERR;
            }
            switch (option) {
                case OPT_PENDING:
                    if (!af->fops->pending) {
                        Jim_SetResultString(interp, "-pending not supported on this connection type", -1);
                        return JIM_ERR;
                    }
                    pending++;
                    break;
                case OPT_NONEWLINE:
                    nonewline++;
                    break;
            }
        }
        else {
            if (Jim_GetWide(interp, argv[0], &neededLen) != JIM_OK)
                return JIM_ERR;
            if (neededLen < 0) {
                Jim_SetResultString(interp, "invalid parameter: negative len", -1);
                return JIM_ERR;
            }
        }
        argc--;
        argv++;
    }
    if (argc) {
        return -1;
    }
    objPtr = Jim_NewStringObj(interp, NULL, 0);
    while (neededLen != 0) {
        int retval;
        int readlen;

        if (pending) {
            readlen = 1;
        }
        else if (neededLen == -1) {
            readlen = AIO_BUF_LEN;
        }
        else {
            readlen = (neededLen > AIO_BUF_LEN ? AIO_BUF_LEN : neededLen);
        }
        retval = af->fops->reader(af, buf, readlen);
        if (retval > 0) {
            Jim_AppendString(interp, objPtr, buf, retval);
            if (neededLen != -1) {
                neededLen -= retval;
            }
            else if (pending) {
                /* If pending was specified, after we do the initial read,
                 * we do a second read to fetch any buffered data
                 */
                neededLen = af->fops->pending(af);
                pending = 0;
            }
        }
        if (retval <= 0) {
            break;
        }
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

static int aio_cmd_getfd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    fflush(af->fp);
    Jim_SetResultInt(interp, fileno(af->fp));

    return JIM_OK;
}

static int aio_cmd_copy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    jim_wide count = 0;
    jim_wide maxlen = JIM_WIDE_MAX;
    AioFile *outf = Jim_AioFile(interp, argv[0]);
    /* Small, static buffer for small files */
    char buf[AIO_BUF_LEN];
    /* Will be allocated if the file is large */
    char *bufp = buf;
    int buflen = sizeof(buf);

    if (outf == NULL) {
        return JIM_ERR;
    }

    if (argc == 2) {
        if (Jim_GetWide(interp, argv[1], &maxlen) != JIM_OK) {
            return JIM_ERR;
        }
    }

    while (count < maxlen) {
        jim_wide len = maxlen - count;
        if (len > buflen) {
            len = buflen;
        }

        len = af->fops->reader(af, bufp, len);
        if (len <= 0) {
            break;
        }
        if (outf->fops->writer(outf, bufp, len) != len) {
            break;
        }
        count += len;
        if (count >= 16384 && bufp == buf) {
            /* Heuristic check - for large copy speed-up */
            buflen = 65536;
            bufp = malloc(buflen);
        }
    }

    if (bufp != buf) {
        free(bufp);
    }

    if (JimCheckStreamError(interp, af) || JimCheckStreamError(interp, outf)) {
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
    int eof_or_partial = 0;

    errno = 0;

    if (af->getline_partial) {
        /* A partial line was read previously, so append to it */
        objPtr = af->getline_partial;
        af->getline_partial = NULL;
    }
    else {
        objPtr = Jim_NewStringObj(interp, NULL, 0);
    }

    while (1) {
        if (af->fops->getline(af, buf, AIO_BUF_LEN)) {
            len = strlen(buf);
            if (len && buf[len - 1] == '\n') {
                /* strip "\n" and we are done */
                Jim_AppendString(interp, objPtr, buf, len - 1);
                break;
            }

            /* Otherwise just append what we have */
            Jim_AppendString(interp, objPtr, buf, len);
        }

        if (errno == EAGAIN) {
            if (Jim_Length(objPtr)) {
                /* Stash the partial line */
                af->getline_partial = objPtr;
                /* And indicate that no line is (yet) available */
                objPtr = Jim_NewStringObj(interp, NULL, 0);
            }
            eof_or_partial = 1;
            break;
        }
        else if (af->fops->eof(af)) {
            eof_or_partial = 1;
            break;
        }
    }

    if (Jim_Length(objPtr) == 0 && JimCheckStreamError(interp, af)) {
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

        if (eof_or_partial && len == 0) {
            /* On EOF or partial line with empty result, returns -1 if varName was specified */
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
    if (af->fops->writer(af, wdata, wlen) == wlen) {
        if (argc == 2 || af->fops->writer(af, "\n", 1) == 1) {
            return JIM_OK;
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

#if defined(HAVE_SOCKETS) && !defined(JIM_BOOTSTRAP)
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
        return JimSetVariableSocketAddress(interp, argv[1], &sa, salen);
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
    socklen_t salen;

    if (JimParseSocketAddress(interp, af->addr_family, SOCK_DGRAM, addr, &sa, &salen) != JIM_OK) {
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
    socklen_t salen = sizeof(sa);

    sock = accept(af->fd, &sa.sa, &salen);
    if (sock < 0) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }

    if (argc > 0) {
        if (JimSetVariableSocketAddress(interp, argv[0], &sa, salen) != JIM_OK) {
            return JIM_ERR;
        }
    }

    /* Create the file command */
    return JimMakeChannel(interp, NULL, sock, Jim_NewStringObj(interp, "accept", -1),
        "aio.sockstream%ld", af->addr_family, "r+", AIO_NODELETE) ? JIM_OK : JIM_ERR;
}

static int aio_cmd_sockname(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    Jim_Obj *objPtr = aio_sockname(interp, af);

    if (objPtr == NULL) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

static int aio_cmd_peername(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    union sockaddr_any sa;
    socklen_t salen = sizeof(sa);

    if (getpeername(af->fd, &sa.sa, &salen) < 0) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }
    Jim_SetResult(interp, JimFormatSocketAddress(interp, &sa, salen));
    return JIM_OK;
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

    Jim_SetResultInt(interp, !!af->fops->eof(af));
    return JIM_OK;
}

static int aio_cmd_close(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc == 3) {
        int option = -1;
#if defined(HAVE_SOCKETS)
        static const char * const options[] = { "r", "w", "-nodelete", NULL };
        enum { OPT_R, OPT_W, OPT_NODELETE };
        AioFile *af = Jim_CmdPrivData(interp);

        if (Jim_GetEnum(interp, argv[2], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
#endif
        switch (option) {
#if defined(HAVE_SHUTDOWN)
            case OPT_R:
            case OPT_W:
                if (shutdown(af->fd, option == OPT_R ? SHUT_RD : SHUT_WR) == 0) {
                    return JIM_OK;
                }
                JimAioSetError(interp, NULL);
                return JIM_ERR;
#endif
#if UNIX_SOCKETS
            case OPT_NODELETE:
                if (af->addr_family == PF_UNIX) {
                    af->flags |= AIO_NODELETE;
                    break;
                }
                /* fall through */
#endif
            default:
                Jim_SetResultString(interp, "not supported", -1);
                return JIM_ERR;
        }
    }

    return Jim_DeleteCommand(interp, argv[0]);
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
static int aio_set_nonblocking(int fd, int nb)
{
    int fmode = fcntl(fd, F_GETFL);
    if (nb) {
        fmode |= O_NDELAY;
    }
    else {
        fmode &= ~O_NDELAY;
    }
    (void)fcntl(fd, F_SETFL, fmode);
    return fmode;
}

static int aio_cmd_ndelay(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    if (argc) {
        long nb;

        if (Jim_GetLong(interp, argv[0], &nb) != JIM_OK) {
            return JIM_ERR;
        }
        aio_set_nonblocking(af->fd, nb);
    }
    int fmode = fcntl(af->fd, F_GETFL);
    Jim_SetResultInt(interp, (fmode & O_NONBLOCK) ? 1 : 0);
    return JIM_OK;
}
#endif

#if defined(HAVE_SOCKETS) && !defined(JIM_BOOTSTRAP)
#define SOCKOPT_BOOL 0
#define SOCKOPT_INT 1
#define SOCKOPT_TIMEVAL 2   /* not currently supported */

static const struct sockopt_def {
    const char *name;
    int level;
    int opt;
    int type;   /* SOCKOPT_xxx */
} sockopts[] = {
#ifdef SOL_SOCKET
#ifdef SO_BROADCAST
    { "broadcast", SOL_SOCKET, SO_BROADCAST },
#endif
#ifdef SO_DEBUG
    { "debug", SOL_SOCKET, SO_DEBUG },
#endif
#ifdef SO_KEEPALIVE
    { "keepalive", SOL_SOCKET, SO_KEEPALIVE },
#endif
#ifdef SO_NOSIGPIPE
    { "nosigpipe", SOL_SOCKET, SO_NOSIGPIPE },
#endif
#ifdef SO_OOBINLINE
    { "oobinline", SOL_SOCKET, SO_OOBINLINE },
#endif
#ifdef SO_SNDBUF
    { "sndbuf", SOL_SOCKET, SO_SNDBUF, SOCKOPT_INT },
#endif
#ifdef SO_RCVBUF
    { "rcvbuf", SOL_SOCKET, SO_RCVBUF, SOCKOPT_INT },
#endif
#if 0 && defined(SO_SNDTIMEO)
    { "sndtimeo", SOL_SOCKET, SO_SNDTIMEO, SOCKOPT_TIMEVAL },
#endif
#if 0 && defined(SO_RCVTIMEO)
    { "rcvtimeo", SOL_SOCKET, SO_RCVTIMEO, SOCKOPT_TIMEVAL },
#endif
#endif
#ifdef IPPROTO_TCP
#ifdef TCP_NODELAY
    { "tcp_nodelay", IPPROTO_TCP, TCP_NODELAY },
#endif
#endif
};

static int aio_cmd_sockopt(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    size_t i;

    if (argc == 0) {
        Jim_Obj *dictObjPtr = Jim_NewListObj(interp, NULL, 0);
        for (i = 0; i < sizeof(sockopts) / sizeof(*sockopts); i++) {
            int value = 0;
            socklen_t len = sizeof(value);
            if (getsockopt(af->fd, sockopts[i].level, sockopts[i].opt, (void *)&value, &len) == 0) {
                if (sockopts[i].type == SOCKOPT_BOOL) {
                    value = !!value;
                }
                Jim_ListAppendElement(interp, dictObjPtr, Jim_NewStringObj(interp, sockopts[i].name, -1));
                Jim_ListAppendElement(interp, dictObjPtr, Jim_NewIntObj(interp, value));
            }
        }
        Jim_SetResult(interp, dictObjPtr);
        return JIM_OK;
    }
    if (argc == 1) {
        return -1;
    }

    /* Set an option */
    for (i = 0; i < sizeof(sockopts) / sizeof(*sockopts); i++) {
        if (strcmp(Jim_String(argv[0]), sockopts[i].name) == 0) {
            int on;
            if (sockopts[i].type == SOCKOPT_BOOL) {
                if (Jim_GetBoolean(interp, argv[1], &on) != JIM_OK) {
                    return JIM_ERR;
                }
            }
            else {
                long longval;
                if (Jim_GetLong(interp, argv[1], &longval) != JIM_OK) {
                    return JIM_ERR;
                }
                on = longval;
            }
            if (setsockopt(af->fd, sockopts[i].level, sockopts[i].opt, (void *)&on, sizeof(on)) < 0) {
                Jim_SetResultFormatted(interp, "Failed to set %#s: %s", argv[0], strerror(errno));
                return JIM_ERR;
            }
            return JIM_OK;
        }
    }
    /* Not found */
    Jim_SetResultFormatted(interp, "Unknown sockopt %#s", argv[0]);
    return JIM_ERR;
}
#endif /* JIM_BOOTSTRAP */

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
static int aio_eventinfo(Jim_Interp *interp, AioFile * af, unsigned mask,
    int argc, Jim_Obj * const *argv)
{
    if (argc == 0) {
        /* Return current script */
        Jim_Obj *objPtr = Jim_FindFileHandler(interp, af->fd, mask);
        if (objPtr) {
            Jim_SetResult(interp, objPtr);
        }
        return JIM_OK;
    }

    /* Delete old handler */
    Jim_DeleteFileHandler(interp, af->fd, mask);

    /* Now possibly add the new script(s) */
    if (Jim_Length(argv[0])) {
        Jim_CreateScriptFileHandler(interp, af->fd, mask, argv[0]);
    }

    return JIM_OK;
}

static int aio_cmd_readable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_READABLE, argc, argv);
}

static int aio_cmd_writable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_WRITABLE, argc, argv);
}

static int aio_cmd_onexception(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_EXCEPTION, argc, argv);
}
#endif

#if defined(jim_ext_file) && defined(Jim_FileStat)
static int aio_cmd_stat(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_stat_t sb;
    AioFile *af = Jim_CmdPrivData(interp);

    if (Jim_FileStat(af->fd, &sb) == -1) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }
    return Jim_FileStoreStatData(interp, argc == 0 ? NULL : argv[0], &sb);
}
#endif

#if defined(JIM_SSL) && !defined(JIM_BOOTSTRAP)
static int aio_cmd_ssl(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    int server = 0;
    const char *sni = NULL;

    if (argc > 2) {
        static const char * const options[] = { "-server", "-sni", NULL };
        enum { OPT_SERVER, OPT_SNI };
        int option;

        if (Jim_GetEnum(interp, argv[2], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
        switch (option) {
            case OPT_SERVER:
                if (argc != 4 && argc != 5) {
                    return JIM_ERR;
                }
                server = 1;
                break;

            case OPT_SNI:
                if (argc != 4) {
                    return JIM_ERR;
                }
                sni = Jim_String(argv[3]);
                break;
        }
    }

    if (af->ssl) {
        Jim_SetResultFormatted(interp, "%#s: stream is already ssl", argv[0]);
        return JIM_ERR;
    }

    ssl_ctx = JimAioSslCtx(interp);
    if (ssl_ctx == NULL) {
        return JIM_ERR;
    }

    ssl = SSL_new(ssl_ctx);
    if (ssl == NULL) {
        goto out;
    }

    SSL_set_cipher_list(ssl, "ALL");

    if (SSL_set_fd(ssl, fileno(af->fp)) == 0) {
        goto out;
    }

    if (server) {
        const char *certfile = Jim_String(argv[3]);
        const char *keyfile = (argc == 4) ? certfile : Jim_String(argv[4]);
        if (SSL_use_certificate_file(ssl, certfile, SSL_FILETYPE_PEM) != 1) {
            goto out;
        }
        if (SSL_use_PrivateKey_file(ssl, keyfile, SSL_FILETYPE_PEM) != 1) {
            goto out;
        }

        if (SSL_accept(ssl) != 1) {
            goto out;
        }
    }
    else {
        if (sni) {
            /* Set server name indication if requested */
            SSL_set_tlsext_host_name(ssl, sni);
        }
        if (SSL_connect(ssl) != 1) {
            goto out;
        }
    }

    af->ssl = ssl;
    af->fops = &ssl_fops;

    /* Set the command name as the result */
    Jim_SetResult(interp, argv[0]);

    return JIM_OK;

out:
    if (ssl) {
        SSL_free(ssl);
    }
    Jim_SetResultString(interp, ERR_error_string(ERR_get_error(), NULL), -1);
    return JIM_ERR;
}

static int aio_cmd_verify(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int ret;

    if (!af->fops->verify) {
        return JIM_OK;
    }

    ret = af->fops->verify(af);
    if (ret != JIM_OK) {
        if (JimCheckStreamError(interp, af) == JIM_OK) {
            Jim_SetResultString(interp, "failed to verify the connection authenticity", -1);
        }
    }
    return ret;
}
#endif /* JIM_BOOTSTRAP */

#if defined(HAVE_STRUCT_FLOCK) && !defined(JIM_BOOTSTRAP)
static int aio_cmd_lock(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    struct flock fl;
    int lockmode = F_SETLK;

    if (argc == 1) {
        if (!Jim_CompareStringImmediate(interp, argv[0], "-wait")) {
            return -1;
        }
        lockmode = F_SETLKW;
    }

    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;

    switch (fcntl(af->fd, lockmode, &fl))
    {
        case 0:
            Jim_SetResultInt(interp, 1);
            break;
        case -1:
            if (errno == EACCES || errno == EAGAIN)
                Jim_SetResultInt(interp, 0);
            else
            {
                Jim_SetResultFormatted(interp, "lock failed: %s",
                    strerror(errno));
                return JIM_ERR;
            }
            break;
        default:
            Jim_SetResultInt(interp, 0);
            break;
    }

    return JIM_OK;
}

static int aio_cmd_unlock(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    struct flock fl;
    fl.l_start = 0;
    fl.l_len = 0;
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;

    Jim_SetResultInt(interp, fcntl(af->fd, F_SETLK, &fl) == 0);
    return JIM_OK;
}
#endif /* JIM_BOOTSTRAP */

#if defined(HAVE_TERMIOS_H) && !defined(JIM_BOOTSTRAP)
static int aio_cmd_tty(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    Jim_Obj *dictObjPtr;
    int ret;

    if (argc == 0) {
        /* get the current settings as a dictionary */
        dictObjPtr = Jim_GetTtySettings(interp, af->fd);
        if (dictObjPtr == NULL) {
            JimAioSetError(interp, NULL);
            return JIM_ERR;
        }
        Jim_SetResult(interp, dictObjPtr);
        return JIM_OK;
    }

    if (argc > 1) {
        /* Convert name value arguments to a dictionary */
        dictObjPtr = Jim_NewListObj(interp, argv, argc);
    }
    else {
        /* The settings are already given as a list */
        dictObjPtr = argv[0];
    }
    Jim_IncrRefCount(dictObjPtr);

    if (Jim_ListLength(interp, dictObjPtr) % 2) {
        /* Must be a valid dictionary */
        Jim_DecrRefCount(interp, dictObjPtr);
        return -1;
    }

    ret = Jim_SetTtySettings(interp, af->fd, dictObjPtr);
    if (ret < 0) {
        JimAioSetError(interp, NULL);
        ret = JIM_ERR;
    }
    Jim_DecrRefCount(interp, dictObjPtr);

    return ret;
}
#endif /* JIM_BOOTSTRAP */

static const jim_subcmd_type aio_command_table[] = {
    {   "read",
        "?-nonewline|-pending|len?",
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
    {   "getfd",
        NULL,
        aio_cmd_getfd,
        0,
        0,
        /* Description: Internal command to return the underlying file descriptor. */
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
#if defined(HAVE_SOCKETS) && !defined(JIM_BOOTSTRAP)
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
    {   "sockopt",
        "?opt 0|1?",
        aio_cmd_sockopt,
        0,
        2,
        /* Description: Return a dictionary of sockopts, or set the value of a sockopt */
    },
    {   "sockname",
        NULL,
        aio_cmd_sockname,
        0,
        0,
        /* Description: Returns the local address of the socket, if any */
    },
    {   "peername",
        NULL,
        aio_cmd_peername,
        0,
        0,
        /* Description: Returns the remote address of the socket, if any */
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
#if defined(jim_ext_file) && defined(Jim_FileStat)
    {   "stat",
        "?var?",
        aio_cmd_stat,
        0,
        1,
        /* Description: 'file stat' on the open file */
    },
#endif
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
#if !defined(JIM_BOOTSTRAP)
#if defined(JIM_SSL)
    {   "ssl",
        "?-server cert ?priv?|-sni servername?",
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
#if defined(HAVE_STRUCT_FLOCK)
    {   "lock",
        "?-wait?",
        aio_cmd_lock,
        0,
        1,
        /* Description: Attempt to get a lock, possibly waiting */
    },
    {   "unlock",
        NULL,
        aio_cmd_unlock,
        0,
        0,
        /* Description: Relase a lock. */
    },
#endif
#if defined(HAVE_TERMIOS_H)
    {   "tty",
        "?baud rate? ?data bits? ?stop bits? ?parity even|odd|none? ?handshake xonxoff|rtscts|none? ?input raw|cooked? ?output raw|cooked? ?echo 0|1? ?vmin n? ?vtime n?",
        aio_cmd_tty,
        0,
        -1,
        /* Description: Get or set tty settings - valid only on a tty */
    },
#endif
#endif /* JIM_BOOTSTRAP */
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
    FILE *fh = NULL;
    const char *filename;
    int fd = -1;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "filename ?mode?");
        return JIM_ERR;
    }

    filename = Jim_String(argv[1]);
    mode = (argc == 3) ? Jim_String(argv[2]) : "r";

#ifdef jim_ext_tclcompat
    {

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
#ifndef JIM_ANSIC
    if (*mode == 'R' || *mode == 'W') {
        /* POSIX flags */
        #ifndef O_NOCTTY
            /* mingw doesn't support this flag */
            #define O_NOCTTY 0
        #endif
        static const char * const modetypes[] = {
            "RDONLY", "WRONLY", "RDWR", "APPEND", "BINARY", "CREAT", "EXCL", "NOCTTY", "TRUNC", NULL
        };
        static const char * const simplemodes[] = {
            "r", "w", "w+"
        };
        static const int modeflags[] = {
            O_RDONLY, O_WRONLY, O_RDWR, O_APPEND, 0, O_CREAT, O_EXCL, O_NOCTTY, O_TRUNC,
        };
        int posixflags = 0;
        int len = Jim_ListLength(interp, argv[2]);
        int i;
        int opt;

        mode = NULL;

        for (i = 0; i < len; i++) {
            Jim_Obj *objPtr = Jim_ListGetIndex(interp, argv[2], i);
            if (Jim_GetEnum(interp, objPtr, modetypes, &opt, "access mode", JIM_ERRMSG) != JIM_OK) {
                return JIM_ERR;
            }
            if (opt < 3) {
                mode = simplemodes[opt];
            }
            posixflags |= modeflags[opt];
        }
        /* mode must be set here if it started with 'R' or 'W' and passed the enum check above */
        assert(mode);
        fd = open(filename, posixflags, 0666);
        if (fd >= 0) {
            fh = fdopen(fd, mode);
            if (fh == NULL) {
                close(fd);
            }
        }
    }
    else
#endif
    {
        fh = fopen(filename, mode);
    }

    if (fh == NULL) {
        JimAioSetError(interp, argv[1]);
        return JIM_ERR;
    }

    return JimMakeChannel(interp, fh, fd, argv[1], "aio.handle%ld", 0, mode, 0) ? JIM_OK : JIM_ERR;
}

#if defined(JIM_SSL) && !defined(JIM_BOOTSTRAP)
static void JimAioSslContextDelProc(struct Jim_Interp *interp, void *privData)
{
    SSL_CTX_free((SSL_CTX *)privData);
    ERR_free_strings();
}

#ifdef USE_TLSv1_2_method
#define TLS_method TLSv1_2_method
#endif

static SSL_CTX *JimAioSslCtx(Jim_Interp *interp)
{
    SSL_CTX *ssl_ctx = (SSL_CTX *)Jim_GetAssocData(interp, "ssl_ctx");
    if (ssl_ctx == NULL) {
        SSL_load_error_strings();
        SSL_library_init();
        ssl_ctx = SSL_CTX_new(TLS_method());
        if (ssl_ctx && SSL_CTX_set_default_verify_paths(ssl_ctx)) {
            SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
            Jim_SetAssocData(interp, "ssl_ctx", JimAioSslContextDelProc, ssl_ctx);
        } else {
            Jim_SetResultString(interp, ERR_error_string(ERR_get_error(), NULL), -1);
        }
    }
    return ssl_ctx;
}
#endif /* JIM_BOOTSTRAP */

/**
 * Creates a channel for fh/fd/filename.
 *
 * If fh is not NULL, uses that as the channel (and sets AIO_KEEPOPEN).
 * Otherwise fd must be >= 0, in which case it uses that as the channel.
 *
 * hdlfmt is a sprintf format for the filehandle. Anything with %ld at the end will do.
 * mode is used for open or fdopen.
 *
 * Creates the command and sets the name as the current result.
 * Returns the AioFile pointer on sucess or NULL on failure (only if fdopen fails).
 */
static AioFile *JimMakeChannel(Jim_Interp *interp, FILE *fh, int fd, Jim_Obj *filename,
    const char *hdlfmt, int family, const char *mode, int flags)
{
    AioFile *af;
    char buf[AIO_CMD_LEN];
    Jim_Obj *cmdname;

    if (fh == NULL) {
        assert(fd >= 0);
#ifndef JIM_ANSIC
        fh = fdopen(fd, mode);

        if (fh == NULL) {
            JimAioSetError(interp, filename);
            close(fd);
            return NULL;
        }
#endif
    }

    snprintf(buf, sizeof(buf), hdlfmt, Jim_GetId(interp));
    cmdname = Jim_NewStringObj(interp, buf, -1);
    if (!filename) {
        filename = cmdname;
    }
    Jim_IncrRefCount(filename);

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    memset(af, 0, sizeof(*af));
    af->fp = fh;
    af->filename = filename;
    af->flags = flags;
#ifndef JIM_ANSIC
    af->fd = fileno(fh);
#ifdef FD_CLOEXEC
    if ((flags & AIO_KEEPOPEN) == 0) {
        (void)fcntl(af->fd, F_SETFD, FD_CLOEXEC);
    }
#endif
#endif
    af->addr_family = family;
    af->fops = &stdio_fops;
    af->ssl = NULL;

    Jim_CreateCommand(interp, buf, JimAioSubCmdProc, af, JimAioDelProc);

    /* Note that the command must use the global namespace, even if
     * the current namespace is something different
     */
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, cmdname));

    return af;
}

#if defined(HAVE_PIPE) || (defined(HAVE_SOCKETPAIR) && UNIX_SOCKETS) || defined(HAVE_OPENPTY)
/**
 * Create a pair of channels. e.g. from pipe() or socketpair()
 */
static int JimMakeChannelPair(Jim_Interp *interp, int p[2], Jim_Obj *filename,
    const char *hdlfmt, int family, const char * const mode[2])
{
    if (JimMakeChannel(interp, NULL, p[0], filename, hdlfmt, family, mode[0], 0)) {
        Jim_Obj *objPtr = Jim_NewListObj(interp, NULL, 0);
        Jim_ListAppendElement(interp, objPtr, Jim_GetResult(interp));
        if (JimMakeChannel(interp, NULL, p[1], filename, hdlfmt, family, mode[1], 0)) {
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

#ifdef HAVE_PIPE
static int JimAioPipeCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int p[2];
    static const char * const mode[2] = { "r", "w" };

    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }

    if (pipe(p) != 0) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }

    return JimMakeChannelPair(interp, p, argv[0], "aio.pipe%ld", 0, mode);
}
#endif

#ifdef HAVE_OPENPTY
static int JimAioOpenPtyCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int p[2];
    char path[MAXPATHLEN];
    static const char * const mode[2] = { "r+", "w+" };

    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }

    if (openpty(&p[0], &p[1], path, NULL, NULL) != 0) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }

    /* Note: The replica path will be used for both handles slave */
    return JimMakeChannelPair(interp, p, Jim_NewStringObj(interp, path, -1), "aio.pty%ld", 0, mode);
}
#endif

#if defined(HAVE_SOCKETS) && !defined(JIM_BOOTSTRAP)

static int JimAioSockCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *socktypes[] = {
        "unix",
        "unix.server",
        "unix.dgram",
        "unix.dgram.server",
        "dgram",
        "dgram.server",
        "stream",
        "stream.server",
        "pipe",
        "pair",
        "pty",
        NULL
    };
    enum
    {
        SOCK_UNIX,
        SOCK_UNIX_SERVER,
        SOCK_UNIX_DGRAM,
        SOCK_UNIX_DGRAM_SERVER,
        SOCK_DGRAM_CLIENT,
        SOCK_DGRAM_SERVER,
        SOCK_STREAM_CLIENT,
        SOCK_STREAM_SERVER,
        SOCK_STREAM_PIPE,
        SOCK_STREAM_SOCKETPAIR,
        SOCK_STREAM_PTY,
    };
    int socktype;
    int sock;
    const char *addr = NULL;
    const char *bind_addr = NULL;
    const char *connect_addr = NULL;
    Jim_Obj *filename = NULL;
    union sockaddr_any sa;
    socklen_t salen;
    int on = 1;
    int reuse = 0;
    int do_listen = 0;
    int family = PF_INET;
    int type = SOCK_STREAM;
    Jim_Obj *argv0 = argv[0];
    int ipv6 = 0;
    int async = 0;


    while (argc > 1 && Jim_String(argv[1])[0] == '-') {
        static const char * const options[] = { "-async", "-ipv6", NULL };
        enum { OPT_ASYNC, OPT_IPV6 };
        int option;

        if (Jim_GetEnum(interp, argv[1], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
        switch (option) {
            case OPT_ASYNC:
                async = 1;
                break;

            case OPT_IPV6:
                if (!IPV6) {
                    Jim_SetResultString(interp, "ipv6 not supported", -1);
                    return JIM_ERR;
                }
                ipv6 = 1;
                family = PF_INET6;
                break;
        }
        argc--;
        argv++;
    }

    if (argc < 2) {
      wrongargs:
        Jim_WrongNumArgs(interp, 1, &argv0, "?-async? ?-ipv6? type ?address?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], socktypes, &socktype, "socket type", JIM_ERRMSG) != JIM_OK)
        return Jim_CheckShowCommands(interp, argv[1], socktypes);

    Jim_SetEmptyResult(interp);

    if (argc > 2) {
        addr = Jim_String(argv[2]);
		filename = argv[2];
    }

#if defined(HAVE_SOCKETPAIR) && UNIX_SOCKETS
    if (socktype == SOCK_STREAM_SOCKETPAIR) {
        int p[2];
        static const char * const mode[2] = { "r+", "r+" };

        if (addr || ipv6) {
            goto wrongargs;
        }

        if (socketpair(PF_UNIX, SOCK_STREAM, 0, p) < 0) {
            JimAioSetError(interp, NULL);
            return JIM_ERR;
        }
        return JimMakeChannelPair(interp, p, argv[1], "aio.sockpair%ld", PF_UNIX, mode);
    }
#endif

#if defined(HAVE_PIPE)
    if (socktype == SOCK_STREAM_PIPE) {
        if (addr || ipv6) {
            goto wrongargs;
        }
        return JimAioPipeCommand(interp, 1, &argv[1]);
    }
#endif

    /* Now all these socket types are very similar */
    switch (socktype) {
        case SOCK_DGRAM_CLIENT:
            connect_addr = addr;
            type =  SOCK_DGRAM;
            break;

        case SOCK_STREAM_CLIENT:
            if (addr == NULL) {
                goto wrongargs;
            }
            connect_addr = addr;
            break;

        case SOCK_STREAM_SERVER:
            if (addr == NULL) {
                goto wrongargs;
            }
            bind_addr = addr;
            reuse = 1;
            do_listen = 1;
            break;

        case SOCK_DGRAM_SERVER:
            if (addr == NULL) {
                goto wrongargs;
            }
            bind_addr = addr;
            type = SOCK_DGRAM;
            reuse = 1;
            break;

#if UNIX_SOCKETS
        case SOCK_UNIX:
            if (addr == NULL) {
                goto wrongargs;
            }
            connect_addr = addr;
            family = PF_UNIX;
            break;

        case SOCK_UNIX_DGRAM:
            connect_addr = addr;
            type = SOCK_DGRAM;
            family = PF_UNIX;
            /* A dgram unix domain socket client needs to bind
             * to a temporary address to allow the server to
             * send responses
             */
             {
                int tmpfd = Jim_MakeTempFile(interp, NULL, 1);
                if (tmpfd < 0) {
                    return JIM_ERR;
                }
                close(tmpfd);
                /* This will be valid until a result is next set, which is long enough here */
                bind_addr = Jim_String(Jim_GetResult(interp));
            }
            break;

        case SOCK_UNIX_SERVER:
            if (addr == NULL) {
                goto wrongargs;
            }
            bind_addr = addr;
            family = PF_UNIX;
            do_listen = 1;
            break;

        case SOCK_UNIX_DGRAM_SERVER:
            if (addr == NULL) {
                goto wrongargs;
            }
            bind_addr = addr;
            type = SOCK_DGRAM;
            family = PF_UNIX;
            break;
#endif
#ifdef HAVE_OPENPTY
        case SOCK_STREAM_PTY:
            if (addr || ipv6) {
                goto wrongargs;
            }
            return JimAioOpenPtyCommand(interp, 1, &argv[1]);
#endif

        default:
            Jim_SetResultString(interp, "Unsupported socket type", -1);
            return JIM_ERR;
    }

    /* Now do all the steps necessary for the given socket type */
    sock = socket(family, type, 0);
    if (sock < 0) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }
    if (async) {
        aio_set_nonblocking(sock, 1);
    }
    if (bind_addr) {
        if (JimParseSocketAddress(interp, family, type, bind_addr, &sa, &salen) != JIM_OK) {
            close(sock);
            return JIM_ERR;
        }
        if (reuse) {
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on));
        }
        if (bind(sock, &sa.sa, salen)) {
            Jim_SetResultFormatted(interp, "%s: bind: %s", bind_addr, strerror(errno));
            close(sock);
            return JIM_ERR;
        }
    }
    if (connect_addr) {
        if (JimParseSocketAddress(interp, family, type, connect_addr, &sa, &salen) != JIM_OK) {
            close(sock);
            return JIM_ERR;
        }
        if (connect(sock, &sa.sa, salen)) {
            if (async && errno == EINPROGRESS) {
                /* OK */
            }
            else {
                Jim_SetResultFormatted(interp, "%s: connect: %s", connect_addr, strerror(errno));
                close(sock);
                return JIM_ERR;
            }
        }
    }
    if (do_listen) {
        if (listen(sock, 5)) {
            Jim_SetResultFormatted(interp, "listen: %s", strerror(errno));
            close(sock);
            return JIM_ERR;
        }
    }
	if (!filename) {
		filename = argv[1];
	}

    return JimMakeChannel(interp, NULL, sock, filename, "aio.sock%ld", family, "r+", 0) ? JIM_OK : JIM_ERR;
}
#endif /* JIM_BOOTSTRAP */

#if defined(JIM_SSL) && !defined(JIM_BOOTSTRAP)
static int JimAioLoadSSLCertsCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    SSL_CTX *ssl_ctx;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "dir");
        return JIM_ERR;
    }

    ssl_ctx = JimAioSslCtx(interp);
    if (!ssl_ctx) {
        return JIM_ERR;
    }
    if (SSL_CTX_load_verify_locations(ssl_ctx, NULL, Jim_String(argv[1])) == 1) {
        return JIM_OK;
    }
    Jim_SetResultString(interp, ERR_error_string(ERR_get_error(), NULL), -1);
    return JIM_ERR;
}
#endif /* JIM_BOOTSTRAP */

int Jim_aioInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "aio", "1.0", JIM_ERRMSG))
        return JIM_ERR;

#if defined(JIM_SSL)
    Jim_CreateCommand(interp, "load_ssl_certs", JimAioLoadSSLCertsCommand, NULL, NULL);
#endif

    Jim_CreateCommand(interp, "open", JimAioOpenCommand, NULL, NULL);
#ifdef HAVE_SOCKETS
    Jim_CreateCommand(interp, "socket", JimAioSockCommand, NULL, NULL);
#endif
#ifdef HAVE_PIPE
    Jim_CreateCommand(interp, "pipe", JimAioPipeCommand, NULL, NULL);
#endif

    /* Create filehandles for stdin, stdout and stderr */
    JimMakeChannel(interp, stdin, -1, NULL, "stdin", 0, "r", AIO_KEEPOPEN);
    JimMakeChannel(interp, stdout, -1, NULL, "stdout", 0, "w", AIO_KEEPOPEN);
    JimMakeChannel(interp, stderr, -1, NULL, "stderr", 0, "w", AIO_KEEPOPEN);

    return JIM_OK;
}
