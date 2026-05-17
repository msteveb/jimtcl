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

/* ------ USAGE -------
 * See jim-aio.c as an example of an event provider.
 *
 * Public event loop APIs are documented here; internal implementation
 * details belong in jim-eventloop.c.
 */

#ifndef __JIM_EVENTLOOP_H__
#define __JIM_EVENTLOOP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Callback invoked when a registered file descriptor becomes ready. */
typedef int Jim_FileProc(Jim_Interp *interp, void *clientData, int mask);

/* Callback invoked when a signal event is delivered. */
typedef int Jim_SignalProc(Jim_Interp *interp, void *clientData, void *msg);

/* Callback invoked when a registered timer expires. */
typedef void Jim_TimeProc(Jim_Interp *interp, void *clientData);

/* Cleanup callback invoked when a file or time handler is deleted. */
typedef void Jim_EventFinalizerProc(Jim_Interp *interp, void *clientData);

/* File handler mask bit for readable descriptors. */
#define JIM_EVENT_READABLE 1
/* File handler mask bit for writable descriptors. */
#define JIM_EVENT_WRITABLE 2
/* File handler mask bit for exceptional conditions. */
#define JIM_EVENT_EXCEPTION 4

/* Register a file event handler for fd and the selected JIM_EVENT_* bits. */
JIM_EXPORT void Jim_CreateFileHandler (Jim_Interp *interp,
        int fd, int mask,
        Jim_FileProc *proc, void *clientData,
        Jim_EventFinalizerProc *finalizerProc);

/* Register a file handler which evaluates scriptObj in the background. */
JIM_EXPORT void Jim_CreateScriptFileHandler(Jim_Interp *interp,
        int fd, int mask, Jim_Obj *scriptObj);

/* Delete all file handlers for fd that match any bit in mask. */
JIM_EXPORT void Jim_DeleteFileHandler (Jim_Interp *interp,
        int fd, int mask);

/* Create a one-shot timer that fires after microseconds and return its id. */
JIM_EXPORT jim_wide Jim_CreateTimeHandler (Jim_Interp *interp,
        jim_wide microseconds,
        Jim_TimeProc *proc, void *clientData,
        Jim_EventFinalizerProc *finalizerProc);

/*
 * Delete the timer identified by id.
 *
 * Returns the remaining time in microseconds, -1 if no such timer exists,
 * or -2 if id is larger than any timer id allocated so far.
 */
JIM_EXPORT jim_wide Jim_DeleteTimeHandler (Jim_Interp *interp, jim_wide id);

/* Return the clientData for the first file handler on fd matching mask, or NULL. */
JIM_EXPORT void *Jim_FindFileHandler(Jim_Interp *interp, int fd, int mask);

/* Wait until fd is readable or the timeout in milliseconds expires. */
JIM_EXPORT int Jim_ReadableTimeout(int fd, long ms);

/* Process registered file handlers. */
#define JIM_FILE_EVENTS 1
/* Process registered timer handlers. */
#define JIM_TIME_EVENTS 2
/* Process both file and timer handlers. */
#define JIM_ALL_EVENTS (JIM_FILE_EVENTS|JIM_TIME_EVENTS)
/* Poll only; do not wait for a future event to become ready. */
#define JIM_DONT_WAIT 4

/*
 * Process pending events selected by flags.
 *
 * Accepted flags are JIM_FILE_EVENTS, JIM_TIME_EVENTS, JIM_ALL_EVENTS,
 * and JIM_DONT_WAIT. Returns the number of events processed, -1 if no
 * matching handlers exist, or -2 on error.
 */
JIM_EXPORT int Jim_ProcessEvents (Jim_Interp *interp, int flags);

/* Evaluate a script as a background event callback and report errors via bgerror. */
JIM_EXPORT int Jim_EvalObjBackground (Jim_Interp *interp, Jim_Obj *scriptObjPtr);

/* Initialise the event loop package and register its core commands. */
int Jim_eventloopInit(Jim_Interp *interp);

#ifdef __cplusplus
}
#endif

#endif /* __JIM_EVENTLOOP_H__ */
