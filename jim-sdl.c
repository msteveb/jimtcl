/* Jim - SDL extension
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 *
 * $Id: jim-sdl.c,v 1.1 2005/03/24 11:00:44 antirez Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <SDL.h>

#define JIM_EXTENSION
#include "jim.h"

#define AIO_CMD_LEN 128

typedef struct JimSdlSurface {
    SDL_Surface *screen;
} JimSdlSurface;

static void JimSdlSetError(Jim_Interp *interp)
{
    Jim_SetResultString(interp, SDL_GetError(), -1);
}

static void JimSdlDelProc(Jim_Interp *interp, void *privData)
{
    JimSdlSurface *jss = privData;
    JIM_NOTUSED(interp);

    SDL_FreeSurface(jss->screen);
    Jim_Free(jss);
}

static void JimSdlSetPixel(SDL_Surface *screen, int x, int y,
        Uint8 r, Uint8 g, Uint8 b)
{
  Uint8 *ubuff8;
  Uint16 *ubuff16;
  Uint32 *ubuff32;
  Uint32 color;
  char c1, c2, c3;
  
  /* Lock the screen, if needed */
  if(SDL_MUSTLOCK(screen)) {
    if(SDL_LockSurface(screen) < 0) 
      return;
  }
  
  /* Get the color */
  color = SDL_MapRGB( screen->format, r, g, b );
  
  /* How we draw the pixel depends on the bitdepth */
  switch(screen->format->BytesPerPixel) 
    {
    case 1: 
      ubuff8 = (Uint8*) screen->pixels;
      ubuff8 += (y * screen->pitch) + x; 
      *ubuff8 = (Uint8) color;
      break;

    case 2:
      ubuff8 = (Uint8*) screen->pixels;
      ubuff8 += (y * screen->pitch) + (x*2);
      ubuff16 = (Uint16*) ubuff8;
      *ubuff16 = (Uint16) color; 
      break;  

    case 3:
      ubuff8 = (Uint8*) screen->pixels;
      ubuff8 += (y * screen->pitch) + (x*3);
      

      if(SDL_BYTEORDER == SDL_LIL_ENDIAN) {
	c1 = (color & 0xFF0000) >> 16;
	c2 = (color & 0x00FF00) >> 8;
	c3 = (color & 0x0000FF);
      } else {
	c3 = (color & 0xFF0000) >> 16;
	c2 = (color & 0x00FF00) >> 8;
	c1 = (color & 0x0000FF);	
      }

      ubuff8[0] = c3;
      ubuff8[1] = c2;
      ubuff8[2] = c1;
      break;
      
    case 4:
      ubuff8 = (Uint8*) screen->pixels;
      ubuff8 += (y*screen->pitch) + (x*4);
      ubuff32 = (Uint32*)ubuff8;
      *ubuff32 = color;
      break;
      
    default:
      fprintf(stderr, "Error: Unknown bitdepth!\n");
    }
  
  /* Unlock the screen if needed */
  if(SDL_MUSTLOCK(screen)) {
    SDL_UnlockSurface(screen);
  }
}

/* Calls to commands created via [sdl.surface] are implemented by this
 * C command. */
static int JimSdlHandlerCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    int option;
    const char *options[] = {
        "free", "setpixel", "flip", NULL
    };
    enum {OPT_FREE, OPT_SETPIXEL, OPT_FLIP};

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], options, &option, "SDL surface method",
                JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    /* SETPIXEL */
    if (option == OPT_SETPIXEL) {
        long x, y, red, green, blue;

        if (argc != 7) {
            Jim_WrongNumArgs(interp, 2, argv, "x y red green blue");
            return JIM_ERR;
        }
        if (Jim_GetLong(interp, argv[2], &x) != JIM_OK ||
            Jim_GetLong(interp, argv[3], &y) != JIM_OK ||
            Jim_GetLong(interp, argv[4], &red) != JIM_OK ||
            Jim_GetLong(interp, argv[5], &green) != JIM_OK ||
            Jim_GetLong(interp, argv[6], &blue) != JIM_OK)
        {
            return JIM_ERR;
        }
        JimSdlSetPixel(jss->screen, x, y, red, green, blue);
        return JIM_OK;
    } else if (option == OPT_FREE) {
    /* FREE */
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_DeleteCommand(interp, Jim_GetString(argv[0], NULL));
        return JIM_OK;
    } else if (option == OPT_FLIP) {
    /* FLIP */
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        SDL_Flip(jss->screen);
        return JIM_OK;
    }
    return JIM_OK;
}

static int JimSdlSurfaceCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    JimSdlSurface *jss;
    char buf[AIO_CMD_LEN];
    Jim_Obj *objPtr;
    long screenId, xres, yres;
    SDL_Surface *screen;

    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "xres yres");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, argv[1], &xres) != JIM_OK ||
        Jim_GetLong(interp, argv[2], &yres) != JIM_OK)
        return JIM_ERR;

    /* Try to create the surface */
    screen = SDL_SetVideoMode(xres, yres, 32, SDL_SWSURFACE|SDL_ANYFORMAT);
    if (screen == NULL) {
        JimSdlSetError(interp);
        return JIM_ERR;
    }
    /* Get the next file id */
    if (Jim_EvalGlobal(interp,
        "if {[catch {incr sdl.surfaceId}]} {set sdl.surfaceId 0}") != JIM_OK)
        return JIM_ERR;
    objPtr = Jim_GetVariableStr(interp, "sdl.surfaceId", JIM_ERRMSG);
    if (objPtr == NULL) return JIM_ERR;
    if (Jim_GetLong(interp, objPtr, &screenId) != JIM_OK) return JIM_ERR;

    /* Create the SDL screen command */
    jss = Jim_Alloc(sizeof(*jss));
    jss->screen = screen;
    sprintf(buf, "sdl.surface%ld", screenId);
    Jim_CreateCommand(interp, buf, JimSdlHandlerCommand, jss, JimSdlDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

int Jim_OnLoad(Jim_Interp *interp)
{
    Jim_InitExtension(interp, "1.0");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        JimSdlSetError(interp);
        return JIM_ERR;
    }
    atexit(SDL_Quit);
    Jim_CreateCommand(interp, "sdl.screen", JimSdlSurfaceCommand, NULL, NULL);
    return JIM_OK;
}
