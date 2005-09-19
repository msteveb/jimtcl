/* Jim - Event Loop extension
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 *
 * $Id: jim-eventloop.c,v 1.2 2005/09/19 15:56:39 antirez Exp $
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

/* TODO:
 *
 *  - to really use flags in Jim_ProcessEvents()
 *  - more complete [after] command with [after info] and other subcommands.
 *  - Win32 port
 */

#define JIM_EXTENSION
#define __JIM_EVENTLOOP_CORE__
#include "jim.h"
#include "jim-eventloop.h"

/* POSIX includes */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
/* --- */

/* File event structure */
typedef struct Jim_FileEvent {
    void *handle;
    int mask; /* one of JIM_EVENT_(READABLE|WRITABLE|EXCEPTION) */
    Jim_FileProc *fileProc;
    Jim_EventFinalizerProc *finalizerProc;
    void *clientData;
    struct Jim_FileEvent *next;
} Jim_FileEvent;

/* Time event structure */
typedef struct Jim_TimeEvent {
    jim_wide id; /* time event identifier. */
    long when_sec; /* seconds */
    long when_ms; /* milliseconds */
    Jim_TimeProc *timeProc;
    Jim_EventFinalizerProc *finalizerProc;
    void *clientData;
    struct Jim_TimeEvent *next;
} Jim_TimeEvent;

/* Per-interp stucture containing the state of the event loop */
typedef struct Jim_EventLoop {
    jim_wide timeEventNextId;
    Jim_FileEvent *fileEventHead;
    Jim_TimeEvent *timeEventHead;
} Jim_EventLoop;

void Jim_CreateFileHandler(Jim_Interp *interp, void *handle, int mask,
        Jim_FileProc *proc, void *clientData,
        Jim_EventFinalizerProc *finalizerProc)
{
    Jim_FileEvent *fe;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");

    fe = Jim_Alloc(sizeof(*fe));
    fe->handle = handle;
    fe->mask = mask;
    fe->fileProc = proc;
    fe->finalizerProc = finalizerProc;
    fe->clientData = clientData;
    fe->next = eventLoop->fileEventHead;
    eventLoop->fileEventHead = fe;
}

void Jim_DeleteFileHandler(Jim_Interp *interp, void *handle)
{
    Jim_FileEvent *fe, *prev = NULL;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");

    fe = eventLoop->fileEventHead;
    while(fe) {
        if (fe->handle == handle) {
            if (prev == NULL)
                eventLoop->fileEventHead = fe->next;
            else
                prev->next = fe->next;
            if (fe->finalizerProc)
                fe->finalizerProc(interp, fe->clientData);
            Jim_Free(fe);
            return;
        }
        prev = fe;
        fe = fe->next;
    }
}

/* That's another part of this extension that needs to be ported
 * to WIN32. */
static void JimGetTime(long *seconds, long *milliseconds)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    *seconds = tv.tv_sec;
    *milliseconds = tv.tv_usec/1000;
}

jim_wide Jim_CreateTimeHandler(Jim_Interp *interp, jim_wide milliseconds,
        Jim_TimeProc *proc, void *clientData,
        Jim_EventFinalizerProc *finalizerProc)
{
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");
    jim_wide id = eventLoop->timeEventNextId++;
    Jim_TimeEvent *te;
    long cur_sec, cur_ms;

    JimGetTime(&cur_sec, &cur_ms);

    te = Jim_Alloc(sizeof(*te));
    te->id = id;
    te->when_sec = cur_sec + milliseconds/1000;
    te->when_ms = cur_ms + milliseconds%1000;
    if (te->when_ms >= 1000) {
        te->when_sec ++;
        te->when_ms -= 1000;
    }
    te->timeProc = proc;
    te->finalizerProc = finalizerProc;
    te->clientData = clientData;
    te->next = eventLoop->timeEventHead;
    eventLoop->timeEventHead = te;
    return id;
}

int Jim_DeleteTimeHandler(Jim_Interp *interp, jim_wide id)
{
    Jim_TimeEvent *te, *prev = NULL;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");

    te = eventLoop->timeEventHead;
    while(te) {
        if (te->id == id) {
            if (prev == NULL)
                eventLoop->timeEventHead = te->next;
            else
                prev->next = te->next;
            if (te->finalizerProc)
                te->finalizerProc(interp, te->clientData);
            Jim_Free(te);
            return JIM_OK;
        }
        prev = te;
        te = te->next;
    }
    return JIM_ERR; /* NO event with the specified ID found */
}

/* Search the first timer to fire.
 * This operation is useful to know how many time the select can be
 * put in sleep without to delay any event.
 * If there are no timers NULL is returned. */
static Jim_TimeEvent *JimSearchNearestTimer(Jim_EventLoop *eventLoop)
{
    Jim_TimeEvent *te = eventLoop->timeEventHead;
    Jim_TimeEvent *nearest = NULL;

    while(te) {
        if (!nearest || te->when_sec < nearest->when_sec ||
                (te->when_sec == nearest->when_sec &&
                 te->when_ms < nearest->when_ms))
            nearest = te;
        te = te->next;
    }
    return nearest;
}

/* --- POSIX version of Jim_ProcessEvents, for now the only available --- */
#define JIM_FILE_EVENTS 1
#define JIM_TIME_EVENTS 2
#define JIM_ALL_EVENTS (JIM_FILE_EVENTS|JIM_TIME_EVENTS)
#define JIM_DONT_WAIT 4

/* Process every pending time event, then every pending file event
 * (that may be registered by time event callbacks just processed).
 * Without special flags the function sleeps until some file event
 * fires, or when the next time event occurrs (if any).
 *
 * If flags is 0, the function does nothing and returns.
 * if flags has JIM_ALL_EVENTS set, all the kind of events are processed.
 * if flags has JIM_FILE_EVENTS set, file events are processed.
 * if flags has JIM_TIME_EVENTS set, time events are processed.
 * if flags has JIM_DONT_WAIT set the function returns ASAP until all
 * the events that's possible to process without to wait are processed.
 *
 * The function returns the number of events processed. */
int Jim_ProcessEvents(Jim_Interp *interp, int flags)
{
    int maxfd = 0, numfd = 0, processed = 0;
    fd_set rfds, wfds, efds;
    Jim_EventLoop *eventLoop = Jim_GetAssocData(interp, "eventloop");
    Jim_FileEvent *fe = eventLoop->fileEventHead;
    Jim_TimeEvent *te;
    jim_wide maxId;
    JIM_NOTUSED(flags);

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    /* Check file events */
    while (fe != NULL) {
        int fd = (int) fe->handle;

        if (fe->mask & JIM_EVENT_READABLE) FD_SET(fd, &rfds);
        if (fe->mask & JIM_EVENT_WRITABLE) FD_SET(fd, &wfds);
        if (fe->mask & JIM_EVENT_EXCEPTION) FD_SET(fd, &efds);
        if (maxfd < fd) maxfd = fd;
        numfd++;
        fe = fe->next;
    }
    /* Note that we want call select() even if there are no
     * file events to process as long as we want to process time
     * events, in order to sleep until the next time event is ready
     * to fire. */
    if (numfd || ((flags & JIM_TIME_EVENTS) && !(flags & JIM_DONT_WAIT))) {
        int retval;
        Jim_TimeEvent *shortest;
        struct timeval tv, *tvp;

        shortest = JimSearchNearestTimer(eventLoop);
        if (shortest) {
            long now_sec, now_ms;

            /* Calculate the time missing for the nearest
             * timer to fire. */
            JimGetTime(&now_sec, &now_ms);
            tvp = &tv;
            tvp->tv_sec = shortest->when_sec - now_sec;
            if (shortest->when_ms < now_ms) {
                tvp->tv_usec = ((shortest->when_ms+1000) - now_ms)*1000;
                tvp->tv_sec --;
            } else {
                tvp->tv_usec = (shortest->when_ms - now_ms)*1000;
            }
        } else {
            tvp = NULL; /* wait forever */
        }

        retval = select(maxfd+1, &rfds, &wfds, &efds, tvp);
        if (retval) {
            fe = eventLoop->fileEventHead;
            while(fe != NULL) {
                int fd = (int) fe->handle;

                if ((fe->mask & JIM_EVENT_READABLE && FD_ISSET(fd, &rfds)) ||
                    (fe->mask & JIM_EVENT_WRITABLE && FD_ISSET(fd, &wfds)) ||
                    (fe->mask & JIM_EVENT_EXCEPTION && FD_ISSET(fd, &efds)))
                {
                    int mask = 0;

                    if (fe->mask & JIM_EVENT_READABLE && FD_ISSET(fd, &rfds))
                        mask |= JIM_EVENT_READABLE;
                    if (fe->mask & JIM_EVENT_WRITABLE && FD_ISSET(fd, &wfds))
                        mask |= JIM_EVENT_WRITABLE;
                    if (fe->mask & JIM_EVENT_EXCEPTION && FD_ISSET(fd, &efds))
                        mask |= JIM_EVENT_EXCEPTION;
                    if (fe->fileProc(interp, fe->clientData, mask) == JIM_ERR) {
                        /* Remove the element on handler error */
                        Jim_DeleteFileHandler(interp, fe->handle);
                    }
                    processed++;
                    /* After an event is processed our file event list
                     * may no longer be the same, so what we do
                     * is to clear the bit for this file descriptor and
                     * restart again from the head. */
                    fe = eventLoop->fileEventHead;
                    FD_CLR(fd, &rfds);
                    FD_CLR(fd, &wfds);
                    FD_CLR(fd, &efds);
                } else {
                    fe = fe->next;
                }
            }
        }
    }
    /* Check time events */
    te = eventLoop->timeEventHead;
    maxId = eventLoop->timeEventNextId-1;
    while(te) {
        long now_sec, now_ms;
        jim_wide id;

        if (te->id > maxId) {
            te = te->next;
            continue;
        }
        JimGetTime(&now_sec, &now_ms);
        if (now_sec > te->when_sec ||
            (now_sec == te->when_sec && now_ms >= te->when_ms))
        {
            id = te->id;
            te->timeProc(interp, te->clientData);
            /* After an event is processed our time event list may
             * no longer be the same, so we restart from head.
             * Still we make sure to don't process events registered
             * by event handlers itself in order to don't loop forever
             * even in case an [after 0] that continuously register
             * itself. To do so we saved the max ID we want to handle. */
            Jim_DeleteTimeHandler(interp, id);
            te = eventLoop->timeEventHead;
        } else {
            te = te->next;
        }
    }

    return processed;
}
/* ---------------------------------------------------------------------- */

void JimELAssocDataDeleProc(Jim_Interp *interp, void *data)
{
    void *next;
    Jim_FileEvent *fe;
    Jim_TimeEvent *te;
    Jim_EventLoop *eventLoop = data;

    fe = eventLoop->fileEventHead;
    while(fe) {
        next = fe->next;
        if (fe->finalizerProc)
            fe->finalizerProc(interp, fe->clientData);
        Jim_Free(fe);
        fe = next;
    }

    te = eventLoop->timeEventHead;
    while(te) {
        next = te->next;
        if (te->finalizerProc)
            te->finalizerProc(interp, te->clientData);
        Jim_Free(te);
        te = next;
    }
    Jim_Free(data);
}

static int JimELVwaitCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    Jim_Obj *oldValue;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "name");
        return JIM_ERR;
    }
    oldValue = Jim_GetGlobalVariable(interp, argv[1], JIM_NONE);
    if (oldValue) Jim_IncrRefCount(oldValue);
    while (1) {
        Jim_Obj *currValue;

        Jim_ProcessEvents(interp, JIM_ALL_EVENTS);
        currValue = Jim_GetGlobalVariable(interp, argv[1], JIM_NONE);
        /* Stop the loop if the vwait-ed variable changed value,
         * or if was unset and now is set (or the contrary). */
        if ((oldValue && !currValue) ||
            (!oldValue && currValue) ||
            (oldValue && currValue &&
             !Jim_StringEqObj(oldValue, currValue, JIM_CASESENS)))
            break;
    }
    if (oldValue) Jim_DecrRefCount(interp, oldValue);
    return JIM_OK;
}

void JimAfterTimeHandler(Jim_Interp *interp, void *clientData)
{
    Jim_Obj *objPtr = clientData;

    Jim_EvalObjBackground(interp, objPtr);
}

void JimAfterTimeEventFinalizer(Jim_Interp *interp, void *clientData)
{
    Jim_Obj *objPtr = clientData;

    Jim_DecrRefCount(interp, objPtr);
}

static int JimELAfterCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    jim_wide ms, id;
    Jim_Obj *objPtr, *idObjPtr;

    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "after milliseconds script");
        return JIM_ERR;
    }
    if (Jim_GetWide(interp, argv[1], &ms) != JIM_OK)
        return JIM_ERR;
    Jim_IncrRefCount(argv[2]);
    id = Jim_CreateTimeHandler(interp, ms, JimAfterTimeHandler, argv[2],
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

int Jim_OnLoad(Jim_Interp *interp)
{
    Jim_EventLoop *eventLoop;

    Jim_InitExtension(interp);
    if (Jim_PackageProvide(interp, "eventloop", "1.0", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;

    eventLoop = Jim_Alloc(sizeof(*eventLoop));
    eventLoop->fileEventHead = NULL;
    eventLoop->timeEventHead = NULL;
    eventLoop->timeEventNextId = 0;
    Jim_SetAssocData(interp, "eventloop", JimELAssocDataDeleProc, eventLoop);

    Jim_CreateCommand(interp, "vwait", JimELVwaitCommand, NULL, NULL);
    Jim_CreateCommand(interp, "after", JimELAfterCommand, NULL, NULL);

    /* Export events API */
    Jim_RegisterApi(interp, "Jim_CreateFileHandler", Jim_CreateFileHandler);
    Jim_RegisterApi(interp, "Jim_DeleteFileHandler", Jim_DeleteFileHandler);
    Jim_RegisterApi(interp, "Jim_CreateTimeHandler", Jim_CreateTimeHandler);
    Jim_RegisterApi(interp, "Jim_DeleteTimeHandler", Jim_DeleteTimeHandler);
    Jim_RegisterApi(interp, "Jim_ProcessEvents", Jim_ProcessEvents);
    return JIM_OK;
}
