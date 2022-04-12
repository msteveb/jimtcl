/*
 * Simple redis interface
 *
 * (c) 2020 Steve Bennett <steveb@workware.net.au>
 *
 * See LICENSE for license details.
 */
#include <jim.h>
#include <jim-eventloop.h>
#include <unistd.h>
#include <hiredis.h>

/**
 * Recursively decode a redis reply as Tcl data structure.
 */
static Jim_Obj *jim_redis_get_result(Jim_Interp *interp, redisReply *reply)
{
    int i;
    switch (reply->type) {
        case REDIS_REPLY_INTEGER:
            return Jim_NewIntObj(interp, reply->integer);
        case REDIS_REPLY_STATUS:
        case REDIS_REPLY_ERROR:
        case REDIS_REPLY_STRING:
            return Jim_NewStringObj(interp, reply->str, reply->len);
            break;
        case REDIS_REPLY_ARRAY:
            {
                Jim_Obj *obj = Jim_NewListObj(interp, NULL, 0);
                for (i = 0; i < reply->elements; i++) {
                    Jim_ListAppendElement(interp, obj, jim_redis_get_result(interp, reply->element[i]));
                }
                return obj;
            }
        case REDIS_REPLY_NIL:
            return Jim_NewStringObj(interp, NULL, 0);
        default:
            return Jim_NewStringObj(interp, "badtype", -1);
    }
}

/**
 * $r readable ?script?
 * - set or clear a readable script
 * $r close
 * - close (delete) the handle
 * $r read
 * - synchronously read a SUBSCRIBE response (typically from within readable)
 * $r <redis-command> ...
 * - invoke the redis command and return the decoded result
 */
static int jim_redis_subcmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    redisContext *c = Jim_CmdPrivData(interp);
    const char **args;
    size_t *arglens;
    int ret = JIM_OK;

    redisReply *reply;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "cmd ?args ...?");
        return JIM_ERR;
    }

    if (Jim_CompareStringImmediate(interp, argv[1], "readable")) {
        /* Remove any existing handler */
        Jim_DeleteFileHandler(interp, c->fd, JIM_EVENT_READABLE);
        if (argc > 2) {
            Jim_CreateScriptFileHandler(interp, c->fd, JIM_EVENT_READABLE, argv[2]);
        }
        return JIM_OK;
    }
    if (Jim_CompareStringImmediate(interp, argv[1], "close")) {
        return Jim_DeleteCommand(interp, argv[0]);
    }
    if (Jim_CompareStringImmediate(interp, argv[1], "read")) {
        if (redisGetReply(c, (void **)&reply) != REDIS_OK) {
            reply = NULL;
        }
    }
    else {
        int nargs = argc - 1;
        args = Jim_Alloc(sizeof(*args) * nargs);
        arglens = Jim_Alloc(sizeof(*arglens) * nargs);
        for (i = 0; i < nargs; i++) {
            args[i] = Jim_String(argv[i + 1]);
            arglens[i] = Jim_Length(argv[i + 1]);
        }
        reply = redisCommandArgv(c, nargs, args, arglens);
        Jim_Free(args);
        Jim_Free(arglens);
    }
    /* sometimes commands return NULL */
    if (reply) {
        Jim_SetResult(interp, jim_redis_get_result(interp, reply));
        if (reply->type == REDIS_REPLY_ERROR) {
            ret = JIM_ERR;
        }
        freeReplyObject(reply);
    }
    else if (c->err) {
        Jim_SetResultFormatted(interp, "%#s: %s", argv[1], c->errstr);
        ret = JIM_ERR;
    }
    return ret;
}

static void jim_redis_del_proc(Jim_Interp *interp, void *privData)
{
    redisContext *c = privData;
    JIM_NOTUSED(interp);
    Jim_DeleteFileHandler(interp, c->fd, JIM_EVENT_READABLE);
    redisFree(c);
}

/**
 * redis <socket-stream>
 *
 * Returns a handle that can be used to communicate with the redis
 * instance over the socket.
 * The original socket handle is closed.
 */
static int jim_redis_cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    redisContext *c;
    char buf[60];
    Jim_Obj *objv[2];
    long fd;
    int ret;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "socket-stream");
        return JIM_ERR;
    }

    /* Invoke getfd to get the file descriptor */
    objv[0] = argv[1];
    objv[1] = Jim_NewStringObj(interp, "getfd", -1);
    ret = Jim_EvalObjVector(interp, 2, objv);
    if (ret == JIM_OK) {
        ret = Jim_GetLong(interp, Jim_GetResult(interp), &fd) == JIM_ERR;
    }
    if (ret != JIM_OK) {
        Jim_SetResultFormatted(interp, "%#s: not a valid stream handle: %#s", argv[0], argv[1]);
        return ret;
    }

    /* Note that we dup the file descriptor here so that we can close the original */
    fd = dup(fd);
    /* Can't fail */
    c = redisConnectFd(fd);
    /* Enable TCP_KEEPALIVE - this is the default for later redis versions */
    redisEnableKeepAlive(c);
    /* Now delete the original stream */
    Jim_DeleteCommand(interp, argv[1]);
    snprintf(buf, sizeof(buf), "redis.handle%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, jim_redis_subcmd, c, jim_redis_del_proc);

    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return JIM_OK;
}

int
Jim_redisInit(Jim_Interp *interp)
{
    Jim_PackageProvideCheck(interp, "redis");
    Jim_CreateCommand(interp, "redis", jim_redis_cmd, NULL, NULL);
    return JIM_OK;
}
