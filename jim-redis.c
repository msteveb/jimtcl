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
static Jim_Obj *jim_redis_get_result(Jim_Interp *interp, redisReply *reply, int addtype)
{
    int i;
    Jim_Obj *obj;

    switch (reply->type) {
        case REDIS_REPLY_INTEGER:
            obj = Jim_NewIntObj(interp, reply->integer);
            break;
        case REDIS_REPLY_STATUS:
        case REDIS_REPLY_ERROR:
        case REDIS_REPLY_STRING:
            obj = Jim_NewStringObj(interp, reply->str, reply->len);
            break;
        case REDIS_REPLY_ARRAY:
            obj = Jim_NewListObj(interp, NULL, 0);
            for (i = 0; i < reply->elements; i++) {
                Jim_ListAppendElement(interp, obj, jim_redis_get_result(interp, reply->element[i], addtype));
            }
            break;
        case REDIS_REPLY_NIL:
            obj = Jim_NewStringObj(interp, NULL, 0);
            break;
        default:
            obj = Jim_NewStringObj(interp, "badtype", -1);
            break;
    }
    if (addtype) {
        const char *type;

        switch (reply->type) {
            case REDIS_REPLY_INTEGER:
                type = "int";
                break;
            case REDIS_REPLY_STATUS:
                type = "status";
                break;
            case REDIS_REPLY_ERROR:
                type = "error";
                break;
            case REDIS_REPLY_STRING:
                type = "str";
                break;
            case REDIS_REPLY_ARRAY:
                type = "array";
                break;
            case REDIS_REPLY_NIL:
                type = "nil";
                break;
            default:
                type = "invalid";
                break;
        }
        obj = Jim_NewListObj(interp, &obj, 1);
        Jim_ListAppendElement(interp, obj, Jim_NewStringObj(interp, type, -1));
    }
    return obj;
}

static int jim_redis_write_callback(Jim_Interp *interp, void *clientData, int mask)
{
    redisContext *c = clientData;

    int done;
    if (redisBufferWrite(c, &done) != REDIS_OK) {
        return JIM_ERR;
    }
    if (done) {
        /* Write has completed, so remove the callback */
        Jim_DeleteFileHandler(interp, c->fd, mask);
    }
    return JIM_OK;
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
    int addtype = 0;

    redisReply *reply;

    if (argc >= 3 && Jim_CompareStringImmediate(interp, argv[1], "-type")) {
        addtype = 1;
        argc--;
        argv++;
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
        int rc;
        if (!(c->flags & REDIS_BLOCK)) {
            redisBufferRead(c);
        }
        rc = redisGetReply(c, (void **)&reply);
        if (rc != REDIS_OK) {
            reply = NULL;
        }
    }
    else if (Jim_GetObjTaint(argv[1]) & JIM_TAINT_ANY) {
        Jim_SetTaintError(interp, 1, argv);
        return JIM_ERR;
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
        if (!(c->flags & REDIS_BLOCK)) {
            int done;
            if (redisBufferWrite(c, &done) == REDIS_OK && !done) {
                /* Couldn't write the entire command, so set up a writable callback to complete the job */
                Jim_CreateFileHandler(interp, c->fd, JIM_EVENT_WRITABLE, jim_redis_write_callback, c, NULL);
            }
        }
    }
    /* sometimes commands return NULL */
    if (reply) {
        Jim_SetResult(interp, jim_redis_get_result(interp, reply, addtype));
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
    int async = 0;

    if (argc > 2 && Jim_CompareStringImmediate(interp, argv[1], "-async")) {
        async = 1;
    }
    if (argc - async != 2) {
        return JIM_USAGE;
    }

    /* Invoke getfd to get the file descriptor */
    objv[0] = argv[1 + async];
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
    if (async) {
        c->flags &= ~REDIS_BLOCK;
    }
    /* Enable TCP_KEEPALIVE - this is the default for later redis versions */
    redisEnableKeepAlive(c);
    /* Now delete the original stream */
    Jim_DeleteCommand(interp, argv[1 + async]);
    snprintf(buf, sizeof(buf), "redis.handle%ld", Jim_GetId(interp));
    Jim_RegisterCmd(interp, buf, "subcommand ?arg ...?", 1, -1, jim_redis_subcmd, jim_redis_del_proc, c, 0);

    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return JIM_OK;
}

int
Jim_redisInit(Jim_Interp *interp)
{
    Jim_PackageProvideCheck(interp, "redis");
    Jim_RegisterSimpleCmd(interp, "redis", "?-async? socket-stream", 1, 2, jim_redis_cmd);
    return JIM_OK;
}
