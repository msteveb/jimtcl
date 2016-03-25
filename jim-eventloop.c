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
#include <jim.h>
#include <jim-eventloop.h>

#include <time.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#if defined(__MINGW32__)
#include <windows.h>
#include <winsock.h>
#ifndef HAVE_USLEEP
#define usleep(US) Sleep((US) / 1000)
#define HAVE_USLEEP
#endif
#else
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif

#ifndef HAVE_USLEEP
/* XXX: Implement this in terms of select() or nanosleep() */
#define usleep(US) sleep((US) / 1000000)
#warning "sub-second sleep not supported"
#endif

/* --- */

/* File event structure */
typedef struct Jim_FileEvent
{
    int fd;
    int mask;                   /* one of JIM_EVENT_(READABLE|WRITABLE|EXCEPTION) */
    Jim_FileProc *fileProc;
    Jim_EventFinalizerProc *finalizerProc;
    void *clientData;
    struct Jim_FileEvent *next;
} Jim_FileEvent;

/* Time event structure */
typedef struct Jim_TimeEvent
{
    jim_wide id;                /* time event identifier. */
    jim_wide initialus;         /* initial relative timer value in microseconds */
    jim_wide when;              /* microseconds */
    Jim_TimeProc *timeProc;
    Jim_EventFinalizerProc *finalizerProc;
    void *clientData;
    struct Jim_TimeEvent *next;
} Jim_TimeEvent;

/* Per-interp stucture containing the state of the event loop */
typedef struct Jim_EventLoop
{
    Jim_FileEvent *fileEventHead;
    Jim_TimeEvent *timeEventHead;
    jim_wide timeEventNextId;   /* highest event id created, starting at 1 */
    int suppress_bgerror; /* bgerror returned break, so don't call it again */
} Jim_EventLoop;

static void JimAfterTimeHandler(Jim_Interp *interp, void *clientData);
static void JimAfterTimeEventFinalizer(Jim_Interp *interp, void *clientData);

int Jim_EvalObjBackground(Jim_Interp *interp, Jim_Obj *scriptObjPtr)
{
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");
    Jim_CallFrame *savedFramePtr;
    int retval;

    savedFramePtr = interp->framePtr;
    interp->framePtr = interp->topFramePtr;
    retval = Jim_EvalObj(interp, scriptObjPtr);
    interp->framePtr = savedFramePtr;
    /* Try to report the error (if any) via the bgerror proc */
    if (retval != JIM_OK && retval != JIM_RETURN && !eventLoop->suppress_bgerror) {
        Jim_Obj *objv[2];
        int rc = JIM_ERR;

        objv[0] = Jim_NewStringObj(interp, "bgerror", -1);
        objv[1] = Jim_GetResult(interp);
        Jim_IncrRefCount(objv[0]);
        Jim_IncrRefCount(objv[1]);
        if (Jim_GetCommand(interp, objv[0], JIM_NONE) == NULL || (rc = Jim_EvalObjVector(interp, 2, objv)) != JIM_OK) {
            if (rc == JIM_BREAK) {
                /* No more bgerror calls */
                eventLoop->suppress_bgerror++;
            }
            else {
                /* Report the error to stderr. */
                Jim_MakeErrorMessage(interp);
                fprintf(stderr, "%s\n", Jim_String(Jim_GetResult(interp)));
                /* And reset the result */
                Jim_SetResultString(interp, "", -1);
            }
        }
        Jim_DecrRefCount(interp, objv[0]);
        Jim_DecrRefCount(interp, objv[1]);
    }
    return retval;
}


/**
 * Register a file event handler on the given file descriptor with the given mask
 * (may be 1 or more of JIM_EVENT_xxx)
 *
 * When the event occurs, proc is called with clientData and the mask of events that occurred.
 * When the filehandler is removed, finalizerProc is called.
 *
 * Note that no check is made that only one handler is registered for the given
 * event(s).
 */
void Jim_CreateFileHandler(Jim_Interp *interp, int fd, int mask,
    Jim_FileProc * proc, void *clientData, Jim_EventFinalizerProc * finalizerProc)
{
    Jim_FileEvent *fe;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");

    fe = Jim_Alloc(sizeof(*fe));
    fe->fd = fd;
    fe->mask = mask;
    fe->fileProc = proc;
    fe->finalizerProc = finalizerProc;
    fe->clientData = clientData;
    fe->next = eventLoop->fileEventHead;
    eventLoop->fileEventHead = fe;
}

static int JimEventHandlerScript(Jim_Interp *interp, void *clientData, int mask)
{
    return Jim_EvalObjBackground(interp, (Jim_Obj *)clientData);
}

static void JimEventHandlerScriptFinalize(Jim_Interp *interp, void *clientData)
{
    Jim_DecrRefCount(interp, (Jim_Obj *)clientData);
}

/**
 * A convenience version of Jim_CreateFileHandler() which evaluates
 * scriptObj with Jim_EvalObjBackground() when the event occurs.
 */
void Jim_CreateScriptFileHandler(Jim_Interp *interp, int fd, int mask,
    Jim_Obj *scriptObj)
{
    Jim_IncrRefCount(scriptObj);
    Jim_CreateFileHandler(interp, fd, mask, JimEventHandlerScript, scriptObj, JimEventHandlerScriptFinalize);
}

/**
 * If there is a file handler registered with the given mask, return the clientData
 * for the (first) handler.
 * Otherwise return NULL.
 */
void *Jim_FindFileHandler(Jim_Interp *interp, int fd, int mask)
{
    Jim_FileEvent *fe;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");

    for (fe = eventLoop->fileEventHead; fe; fe = fe->next) {
        if (fe->fd == fd && (fe->mask & mask)) {
            return fe->clientData;
        }
    }
    return NULL;
}

/**
 * Removes all event handlers for 'handle' that match 'mask'.
 */
void Jim_DeleteFileHandler(Jim_Interp *interp, int fd, int mask)
{
    Jim_FileEvent *fe, *next, *prev = NULL;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");

    for (fe = eventLoop->fileEventHead; fe; fe = next) {
        next = fe->next;
        if (fe->fd == fd && (fe->mask & mask)) {
            /* Remove this entry from the list */
            if (prev == NULL)
                eventLoop->fileEventHead = next;
            else
                prev->next = next;
            if (fe->finalizerProc)
                fe->finalizerProc(interp, fe->clientData);
            Jim_Free(fe);
            continue;
        }
        prev = fe;
    }
}

jim_wide Jim_CreateTimeHandler(Jim_Interp *interp, jim_wide us,
    Jim_TimeProc * proc, void *clientData, Jim_EventFinalizerProc * finalizerProc)
{
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");
    jim_wide id = ++eventLoop->timeEventNextId;
    Jim_TimeEvent *te, *e, *prev;

    te = Jim_Alloc(sizeof(*te));
    te->id = id;
    te->initialus = us;
    te->when = Jim_GetTimeUsec(CLOCK_MONOTONIC_RAW) + us;
    te->timeProc = proc;
    te->finalizerProc = finalizerProc;
    te->clientData = clientData;

    /* Add to the appropriate place in the list */
    prev = NULL;
    for (e = eventLoop->timeEventHead; e; e = e->next) {
        if (te->when < e->when) {
            break;
        }
        prev = e;
    }
    if (prev) {
        te->next = prev->next;
        prev->next = te;
    }
    else {
        te->next = eventLoop->timeEventHead;
        eventLoop->timeEventHead = te;
    }

    return id;
}

static jim_wide JimParseAfterId(Jim_Obj *idObj)
{
    const char *tok = Jim_String(idObj);
    jim_wide id;

    if (strncmp(tok, "after#", 6) == 0 && Jim_StringToWide(tok + 6, &id, 10) == JIM_OK) {
        /* Got an event by id */
        return id;
    }
    return -1;
}

static jim_wide JimFindAfterByScript(Jim_EventLoop *eventLoop, Jim_Obj *scriptObj)
{
    Jim_TimeEvent *te;

    for (te = eventLoop->timeEventHead; te; te = te->next) {
        /* Is this an 'after' event? */
        if (te->timeProc == JimAfterTimeHandler) {
            if (Jim_StringEqObj(scriptObj, te->clientData)) {
                return te->id;
            }
        }
    }
    return -1;                  /* NO event with the specified ID found */
}

static Jim_TimeEvent *JimFindTimeHandlerById(Jim_EventLoop *eventLoop, jim_wide id)
{
    Jim_TimeEvent *te;

    for (te = eventLoop->timeEventHead; te; te = te->next) {
        if (te->id == id) {
            return te;
        }
    }
    return NULL;
}

static Jim_TimeEvent *Jim_RemoveTimeHandler(Jim_EventLoop *eventLoop, jim_wide id)
{
    Jim_TimeEvent *te, *prev = NULL;

    for (te = eventLoop->timeEventHead; te; te = te->next) {
        if (te->id == id) {
            if (prev == NULL)
                eventLoop->timeEventHead = te->next;
            else
                prev->next = te->next;
            return te;
        }
        prev = te;
    }
    return NULL;
}

static void Jim_FreeTimeHandler(Jim_Interp *interp, Jim_TimeEvent *te)
{
    if (te->finalizerProc)
        te->finalizerProc(interp, te->clientData);
    Jim_Free(te);
}

jim_wide Jim_DeleteTimeHandler(Jim_Interp *interp, jim_wide id)
{
    Jim_TimeEvent *te;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");

    if (id > eventLoop->timeEventNextId) {
        return -2;              /* wrong event ID */
    }

    te = Jim_RemoveTimeHandler(eventLoop, id);
    if (te) {
        jim_wide remain;

        remain = te->when - Jim_GetTimeUsec(CLOCK_MONOTONIC_RAW);
        remain = (remain < 0) ? 0 : remain;

        Jim_FreeTimeHandler(interp, te);
        return remain;
    }
    return -1;                  /* NO event with the specified ID found */
}

/* --- POSIX version of Jim_ProcessEvents, for now the only available --- */

/* Process every pending time event, then every pending file event
 * (that may be registered by time event callbacks just processed).
 * The behaviour depends upon the setting of flags:
 *
 * If flags is 0, the function does nothing and returns.
 * if flags has JIM_ALL_EVENTS set, all event types are processed.
 * if flags has JIM_FILE_EVENTS set, file events are processed.
 * if flags has JIM_TIME_EVENTS set, time events are processed.
 * if flags has JIM_DONT_WAIT set, the function returns as soon as all
 * the events that are possible to process without waiting are processed.
 *
 * Returns the number of events processed or -1 if
 * there are no matching handlers, or -2 on error.
 */
int Jim_ProcessEvents(Jim_Interp *interp, int flags)
{
    jim_wide sleep_us = -1;
    int processed = 0;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");
    Jim_FileEvent *fe = eventLoop->fileEventHead;
    Jim_TimeEvent *te;
    jim_wide maxId;

    if ((flags & JIM_FILE_EVENTS) == 0 || fe == NULL) {
        /* No file events */
        if ((flags & JIM_TIME_EVENTS) == 0 || eventLoop->timeEventHead == NULL) {
            /* No time events */
            return -1;
        }
    }

    /* Note that we want call select() even if there are no
     * file events to process as long as we want to process time
     * events, in order to sleep until the next time event is ready
     * to fire. */

    if (flags & JIM_DONT_WAIT) {
        /* Wait no time */
        sleep_us = 0;
    }
    else if (flags & JIM_TIME_EVENTS) {
        /* The nearest timer is always at the head of the list */
        if (eventLoop->timeEventHead) {
            Jim_TimeEvent *shortest = eventLoop->timeEventHead;

            /* Calculate the time missing for the nearest
             * timer to fire. */
            sleep_us = shortest->when - Jim_GetTimeUsec(CLOCK_MONOTONIC_RAW);
            if (sleep_us < 0) {
                sleep_us = 0;
            }
        }
        else {
            /* Wait forever */
            sleep_us = -1;
        }
    }

#ifdef HAVE_SELECT
    if (flags & JIM_FILE_EVENTS) {
        int retval;
        struct timeval tv, *tvp = NULL;
        fd_set rfds, wfds, efds;
        int maxfd = -1;

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);

        /* Check file events */
        while (fe != NULL) {
            if (fe->mask & JIM_EVENT_READABLE)
                FD_SET(fe->fd, &rfds);
            if (fe->mask & JIM_EVENT_WRITABLE)
                FD_SET(fe->fd, &wfds);
            if (fe->mask & JIM_EVENT_EXCEPTION)
                FD_SET(fe->fd, &efds);
            if (maxfd < fe->fd)
                maxfd = fe->fd;
            fe = fe->next;
        }

        if (sleep_us >= 0) {
            tvp = &tv;
            tvp->tv_sec = sleep_us / 1000000;
            tvp->tv_usec = sleep_us % 1000000;
        }

        retval = select(maxfd + 1, &rfds, &wfds, &efds, tvp);

        if (retval < 0) {
            if (errno == EINVAL) {
                /* This can happen on mingw32 if a non-socket filehandle is passed */
                Jim_SetResultString(interp, "non-waitable filehandle", -1);
                return -2;
            }
        }
        else if (retval > 0) {
            fe = eventLoop->fileEventHead;
            while (fe != NULL) {
                int mask = 0;
                int fd = fe->fd;

                if ((fe->mask & JIM_EVENT_READABLE) && FD_ISSET(fd, &rfds))
                    mask |= JIM_EVENT_READABLE;
                if (fe->mask & JIM_EVENT_WRITABLE && FD_ISSET(fd, &wfds))
                    mask |= JIM_EVENT_WRITABLE;
                if (fe->mask & JIM_EVENT_EXCEPTION && FD_ISSET(fd, &efds))
                    mask |= JIM_EVENT_EXCEPTION;

                if (mask) {
                    int ret = fe->fileProc(interp, fe->clientData, mask);
                    if (ret != JIM_OK && ret != JIM_RETURN) {
                        /* Remove the element on handler error */
                        Jim_DeleteFileHandler(interp, fd, mask);
                        /* At this point fe is no longer valid - it will be assigned below */
                    }
                    processed++;
                    /* After an event is processed our file event list
                     * may no longer be the same, so what we do
                     * is to clear the bit for this file descriptor and
                     * restart again from the head. */
                    FD_CLR(fd, &rfds);
                    FD_CLR(fd, &wfds);
                    FD_CLR(fd, &efds);
                    fe = eventLoop->fileEventHead;
                }
                else {
                    fe = fe->next;
                }
            }
        }
    }
#else
    if (sleep_us > 0) {
        usleep(sleep_us);
    }
#endif

    /* Check time events */
    te = eventLoop->timeEventHead;
    maxId = eventLoop->timeEventNextId;
    while (te) {
        jim_wide id;

        if (te->id > maxId) {
            te = te->next;
            continue;
        }
        if (Jim_GetTimeUsec(CLOCK_MONOTONIC_RAW) >= te->when) {
            id = te->id;
            /* Remove from the list before executing */
            Jim_RemoveTimeHandler(eventLoop, id);
            te->timeProc(interp, te->clientData);
            /* After an event is processed our time event list may
             * no longer be the same, so we restart from head.
             * Still we make sure to don't process events registered
             * by event handlers itself in order to don't loop forever
             * even in case an [after 0] that continuously register
             * itself. To do so we saved the max ID we want to handle. */
            Jim_FreeTimeHandler(interp, te);

            te = eventLoop->timeEventHead;
            processed++;
        }
        else {
            te = te->next;
        }
    }

    return processed;
}

/* ---------------------------------------------------------------------- */

static void JimELAssocDataDeleProc(Jim_Interp *interp, void *data)
{
    void *next;
    Jim_FileEvent *fe;
    Jim_TimeEvent *te;
    Jim_EventLoop *eventLoop = data;

    fe = eventLoop->fileEventHead;
    while (fe) {
        next = fe->next;
        if (fe->finalizerProc)
            fe->finalizerProc(interp, fe->clientData);
        Jim_Free(fe);
        fe = next;
    }

    te = eventLoop->timeEventHead;
    while (te) {
        next = te->next;
        if (te->finalizerProc)
            te->finalizerProc(interp, te->clientData);
        Jim_Free(te);
        te = next;
    }
    Jim_Free(data);
}

static int JimELVwaitCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_EventLoop *eventLoop = Jim_CmdPrivData(interp);
    Jim_Obj *oldValue = NULL;
    Jim_Obj *scriptObjPtr = NULL;
    int rc;
    int signal = 0;

    if (argc > 2 && Jim_CompareStringImmediate(interp, argv[1], "-signal")) {
        signal++;
    }

    if (argc - signal == 3) {
        scriptObjPtr = argv[2 + signal];
    }
    else if (argc - signal != 2) {
        return JIM_USAGE;
    }

    oldValue = Jim_GetGlobalVariable(interp, argv[1 + signal], JIM_NONE);

    if (oldValue) {
        Jim_IncrRefCount(oldValue);
    }
    else {
        /* If a result was left, it is an error */
        if (Jim_Length(Jim_GetResult(interp))) {
            return JIM_ERR;
        }
    }

    eventLoop->suppress_bgerror = 0;

    while ((rc = Jim_ProcessEvents(interp, JIM_ALL_EVENTS)) >= 0) {
        Jim_Obj *currValue;

        if (signal && interp->sigmask) {
            /* vwait -signal and handled signals were received, so transfer them
             * to ignored signals so that 'signal check -clear' will return them.
             * It's possible that if signals aren't supported we shouldn't even
             * allow the -signal option.
             */
#ifdef jim_ext_signal
            Jim_SignalSetIgnored(interp->sigmask);
#endif
            interp->sigmask = 0;
            break;
        }

        currValue = Jim_GetGlobalVariable(interp, argv[1 + signal], JIM_NONE);
        /* Stop the loop if the vwait-ed variable changed value,
         * or if was unset and now is set (or the contrary)
         * or if a signal was caught
         */
        if ((oldValue && !currValue) ||
            (!oldValue && currValue) ||
            (oldValue && currValue && !Jim_StringEqObj(oldValue, currValue)) ||
            Jim_CheckSignal(interp)) {
            break;
        }
        if (scriptObjPtr) {
            /* Stop the loop if a provided script returns BREAK or ERR */
            int retval = Jim_EvalObj(interp, scriptObjPtr);
            if (retval == JIM_ERR || retval == JIM_BREAK) {
                if (retval == JIM_ERR) {
                    rc = -2;
                }
                break;
            }
        }
    }
    if (oldValue)
        Jim_DecrRefCount(interp, oldValue);

    if (rc == -2) {
        return JIM_ERR;
    }

    Jim_SetEmptyResult(interp);
    return JIM_OK;
}

static int JimELUpdateCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_EventLoop *eventLoop = Jim_CmdPrivData(interp);
    static const char * const options[] = {
        "idletasks", NULL
    };
    enum { UPDATE_IDLE, UPDATE_NONE };
    int option = UPDATE_NONE;
    int flags = JIM_TIME_EVENTS;

    if (argc == 1) {
        flags = JIM_ALL_EVENTS;
    }
    else if (argc > 2 || Jim_GetEnum(interp, argv[1], options, &option, NULL, JIM_ENUM_ABBREV) != JIM_OK) {
        return JIM_USAGE;
    }

    eventLoop->suppress_bgerror = 0;

    while (Jim_ProcessEvents(interp, flags | JIM_DONT_WAIT) > 0) {
    }

    return JIM_OK;
}

static void JimAfterTimeHandler(Jim_Interp *interp, void *clientData)
{
    Jim_Obj *objPtr = clientData;

    Jim_EvalObjBackground(interp, objPtr);
}

static void JimAfterTimeEventFinalizer(Jim_Interp *interp, void *clientData)
{
    Jim_Obj *objPtr = clientData;

    Jim_DecrRefCount(interp, objPtr);
}

static int JimELAfterCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_EventLoop *eventLoop = Jim_CmdPrivData(interp);
    double ms = 0;
    jim_wide id;
    Jim_Obj *objPtr, *idObjPtr;
    static const char * const options[] = {
        "cancel", "info", "idle", NULL
    };
    enum
    { AFTER_CANCEL, AFTER_INFO, AFTER_IDLE, AFTER_RESTART, AFTER_EXPIRE, AFTER_CREATE };
    int option = AFTER_CREATE;
    if (Jim_GetDouble(interp, argv[1], &ms) != JIM_OK) {
        if (Jim_GetEnum(interp, argv[1], options, &option, "argument", JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
        Jim_SetEmptyResult(interp);
    }
    else if (argc == 2) {
        /* Simply a sleep */
        usleep(ms * 1000);
        return JIM_OK;
    }

    switch (option) {
        case AFTER_IDLE:
            if (argc < 3) {
                Jim_WrongNumArgs(interp, 2, argv, "script ?script ...?");
                return JIM_ERR;
            }
            /* fall through */
        case AFTER_CREATE: {
            Jim_Obj *scriptObj = Jim_ConcatObj(interp, argc - 2, argv + 2);
            Jim_IncrRefCount(scriptObj);
            id = Jim_CreateTimeHandler(interp, (jim_wide)(ms * 1000), JimAfterTimeHandler, scriptObj,
                JimAfterTimeEventFinalizer);
            objPtr = Jim_NewStringObj(interp, NULL, 0);
            Jim_AppendString(interp, objPtr, "after#", -1);
            idObjPtr = Jim_NewIntObj(interp, id);
            Jim_IncrRefCount(idObjPtr);
            Jim_AppendObj(interp, objPtr, idObjPtr);
            Jim_DecrRefCount(interp, idObjPtr);
            Jim_SetResult(interp, objPtr);
            return JIM_OK;
        }
        case AFTER_CANCEL:
            if (argc < 3) {
                Jim_WrongNumArgs(interp, 2, argv, "id|command");
                return JIM_ERR;
            }
            else {
                jim_wide remain = 0;

                id = JimParseAfterId(argv[2]);
                if (id <= 0) {
                    /* Not an event id, so search by script */
                    Jim_Obj *scriptObj = Jim_ConcatObj(interp, argc - 2, argv + 2);
                    id = JimFindAfterByScript(eventLoop, scriptObj);
                    Jim_FreeNewObj(interp, scriptObj);
                    if (id <= 0) {
                        /* Not found */
                        break;
                    }
                }
                remain = Jim_DeleteTimeHandler(interp, id);
                if (remain >= 0) {
                    Jim_SetResultInt(interp, remain);
                }
            }
            break;

        case AFTER_INFO:
            if (argc == 2) {
                Jim_TimeEvent *te = eventLoop->timeEventHead;
                Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);
                char buf[30];
                const char *fmt = "after#%" JIM_WIDE_MODIFIER;

                while (te) {
                    snprintf(buf, sizeof(buf), fmt, te->id);
                    Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, buf, -1));
                    te = te->next;
                }
                Jim_SetResult(interp, listObj);
            }
            else if (argc == 3) {
                id = JimParseAfterId(argv[2]);
                if (id >= 0) {
                    Jim_TimeEvent *e = JimFindTimeHandlerById(eventLoop, id);
                    if (e && e->timeProc == JimAfterTimeHandler) {
                        Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);
                        Jim_ListAppendElement(interp, listObj, e->clientData);
                        Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, e->initialus ? "timer" : "idle", -1));
                        Jim_SetResult(interp, listObj);
                        return JIM_OK;
                    }
                }
                Jim_SetResultFormatted(interp, "event \"%#s\" doesn't exist", argv[2]);
                return JIM_ERR;
            }
            else {
                Jim_WrongNumArgs(interp, 2, argv, "?id?");
                return JIM_ERR;
            }
            break;
    }
    return JIM_OK;
}

int Jim_eventloopInit(Jim_Interp *interp)
{
    Jim_EventLoop *eventLoop;

    Jim_PackageProvideCheck(interp, "eventloop");

    eventLoop = Jim_Alloc(sizeof(*eventLoop));
    memset(eventLoop, 0, sizeof(*eventLoop));

    Jim_SetAssocData(interp, "eventloop", JimELAssocDataDeleProc, eventLoop);

    Jim_RegisterCmd(interp, "vwait", "?-signal? name ?script?", 1, 3, JimELVwaitCommand, NULL, eventLoop, 0);
    Jim_RegisterCmd(interp, "update", "?idletasks?", 0, 1, JimELUpdateCommand, NULL, eventLoop, 0);
    Jim_RegisterCmd(interp, "after", "option ?arg ...?", 1, -1, JimELAfterCommand, NULL, eventLoop, 0);

    return JIM_OK;
}
