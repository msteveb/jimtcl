/* Jim eventloop extension
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 *
 * $Id: jim-eventloop.h,v 1.1 2005/04/18 08:31:26 antirez Exp $
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
 *
 * ------ USAGE -------
 *
 * In order to use this file from other extensions include it in every
 * file where you need to call the eventloop API, also in the init
 * function of your extension call Jim_ImportEventloopAPI(interp)
 * after the Jim_InitExtension() call.
 *
 * See the UDP extension as example.
 */


#ifndef __JIM_EVENTLOOP_H__
#define __JIM_EVENTLOOP_H__

typedef int Jim_FileProc(Jim_Interp *interp, void *clientData, int mask);
typedef void Jim_TimeProc(Jim_Interp *interp, void *clientData);
typedef void Jim_EventFinalizerProc(Jim_Interp *interp, void *clientData);

/* File event structure */
#define JIM_EVENT_READABLE 1
#define JIM_EVENT_WRITABLE 2
#define JIM_EVENT_EXCEPTION 4

#ifndef __JIM_EVENTLOOP_CORE__
# if defined JIM_EXTENSION || defined JIM_EMBEDDED
#  define JIM_API(x) (*x)
#  define JIM_STATIC
# else
#  define JIM_API(x) (*x)
#  define JIM_STATIC extern
# endif
#else
# define JIM_API(x) x
# define JIM_STATIC static
#endif /* __JIM_EVENTLOOP_CORE__ */

JIM_STATIC void JIM_API(Jim_CreateFileHandler) (Jim_Interp *interp,
        void *handle, int mask,
        Jim_FileProc *proc, void *clientData,
        Jim_EventFinalizerProc *finalizerProc);
JIM_STATIC void JIM_API(Jim_DeleteFileHandler) (Jim_Interp *interp,
        void *handle);
JIM_STATIC jim_wide JIM_API(Jim_CreateTimeHandler) (Jim_Interp *interp,
        jim_wide milliseconds,
        Jim_TimeProc *proc, void *clientData,
        Jim_EventFinalizerProc *finalizerProc);
JIM_STATIC int JIM_API(Jim_DeleteTimeHandler) (Jim_Interp *interp, jim_wide id);
JIM_STATIC int JIM_API(Jim_ProcessEvents) (Jim_Interp *interp, int flags);

#undef JIM_STATIC
#undef JIM_API

#ifndef __JIM_EVENTLOOP_CORE__

#define JIM_GET_API(name) \
    Jim_GetApi(interp, "Jim_" #name, ((void *)&Jim_ ## name))

#if defined JIM_EXTENSION || defined JIM_EMBEDDED
/* This must be included "inline" inside the extension */
static void Jim_ImportEventloopAPI(Jim_Interp *interp)
{
  JIM_GET_API(Jim_CreateFileHandler);
  JIM_GET_API(Jim_DeleteFileHandler);
  JIM_GET_API(Jim_CreateTimeHandler);
  JIM_GET_API(Jim_DeleteTimeHandler);
  JIM_GET_API(Jim_ProcessEvents);
}
#endif /* defined JIM_EXTENSION || defined JIM_EMBEDDED */
#undef JIM_GET_API
#endif /* __JIM_EVENTLOOP_CORE__ */

#endif /* __JIM_EVENTLOOP_H__ */
