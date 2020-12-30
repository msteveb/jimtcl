/*
 * Jim - SDL extension
 *
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <SDL.h>
#if SDL_MAJOR_VERSION == 2
#include <SDL2_gfxPrimitives.h>
#else
#include <SDL_gfxPrimitives.h>
#endif

#include <jim.h>

typedef struct JimSdlSurface
{
#if SDL_MAJOR_VERSION == 2
    SDL_Window *win;
    SDL_Renderer *screen;
    SDL_Texture *texture;
#else
    SDL_Surface *screen;
#endif
    long background[4];
} JimSdlSurface;

static void JimSdlSetError(Jim_Interp *interp)
{
    Jim_SetResultString(interp, SDL_GetError(), -1);
}

static void JimSdlDelProc(Jim_Interp *interp, void *privData)
{
    JimSdlSurface *jss = privData;

    JIM_NOTUSED(interp);

#if SDL_MAJOR_VERSION == 2
    SDL_DestroyRenderer(jss->screen);
    SDL_DestroyWindow(jss->win);
#else
    SDL_FreeSurface(jss->screen);
#endif
    Jim_Free(jss);
}

static int JimSdlGetLongs(Jim_Interp *interp, int argc, Jim_Obj *const *argv, long *dest)
{
    while (argc) {
        if (Jim_GetLong(interp, *argv, dest) != JIM_OK) {
            return JIM_ERR;
        }
        argc--;
        argv++;
        dest++;
    }
    return JIM_OK;
}

static void JimSdlClear(JimSdlSurface *jss, int r, int g, int b, int alpha)
{
#if SDL_MAJOR_VERSION == 2
    SDL_SetRenderDrawColor(jss->screen, r, g, b, alpha);
    SDL_RenderClear(jss->screen);
#else
    SDL_FillRect(jss->screen, NULL, SDL_MapRGBA(jss->screen->format, r, g, b, alpha));
#endif
}

/* Calls to commands created via [sdl.surface] are implemented by this
 * C command. */
static int JimSdlHandlerCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    int option;
    static const char * const options[] = {
        "free", "flip", "pixel", "rectangle", "box", "line", "aaline",
        "circle", "aacircle", "fcircle", "clear", NULL
    };
    enum
    { OPT_FREE, OPT_FLIP, OPT_PIXEL, OPT_RECTANGLE, OPT_BOX, OPT_LINE,
        OPT_AALINE, OPT_CIRCLE, OPT_AACIRCLE, OPT_FCIRCLE, OPT_CLEAR
    };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], options, &option, "SDL surface method", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    if (option == OPT_PIXEL) {
        /* PIXEL */
        /* x, y, red, green, blue, alpha = 255 */
        long vals[7];

        if (argc != 7 && argc != 8) {
            Jim_WrongNumArgs(interp, 2, argv, "x y red green blue ?alpha?");
            return JIM_ERR;
        }
        if (JimSdlGetLongs(interp, argc - 3, argv + 3, vals) != JIM_OK) {
            return JIM_ERR;
        }
        pixelRGBA(jss->screen, vals[0], vals[1], vals[2], vals[3], vals[4], argc == 8 ? vals[5] : SDL_ALPHA_OPAQUE);
        return JIM_OK;
    }
    else if (option == OPT_RECTANGLE || option == OPT_BOX ||
        option == OPT_LINE || option == OPT_AALINE) {
        /* RECTANGLE, BOX, LINE, AALINE */
        long x1, y1, x2, y2, red, green, blue, alpha = 255;

        if (argc != 9 && argc != 10) {
            Jim_WrongNumArgs(interp, 2, argv, "x y red green blue ?alpha?");
            return JIM_ERR;
        }
        if (Jim_GetLong(interp, argv[2], &x1) != JIM_OK ||
            Jim_GetLong(interp, argv[3], &y1) != JIM_OK ||
            Jim_GetLong(interp, argv[4], &x2) != JIM_OK ||
            Jim_GetLong(interp, argv[5], &y2) != JIM_OK ||
            Jim_GetLong(interp, argv[6], &red) != JIM_OK ||
            Jim_GetLong(interp, argv[7], &green) != JIM_OK ||
            Jim_GetLong(interp, argv[8], &blue) != JIM_OK) {
            return JIM_ERR;
        }
        if (argc == 10 && Jim_GetLong(interp, argv[9], &alpha) != JIM_OK)
            return JIM_ERR;
        switch (option) {
            case OPT_RECTANGLE:
                rectangleRGBA(jss->screen, x1, y1, x2, y2, red, green, blue, alpha);
                break;
            case OPT_BOX:
                boxRGBA(jss->screen, x1, y1, x2, y2, red, green, blue, alpha);
                break;
            case OPT_LINE:
                lineRGBA(jss->screen, x1, y1, x2, y2, red, green, blue, alpha);
                break;
            case OPT_AALINE:
                aalineRGBA(jss->screen, x1, y1, x2, y2, red, green, blue, alpha);
                break;
        }
        return JIM_OK;
    }
    else if (option == OPT_CIRCLE || option == OPT_AACIRCLE || option == OPT_FCIRCLE) {
        /* CIRCLE, AACIRCLE, FCIRCLE */
        long x, y, radius, red, green, blue, alpha = 255;

        if (argc != 8 && argc != 9) {
            Jim_WrongNumArgs(interp, 2, argv, "x y radius red green blue ?alpha?");
            return JIM_ERR;
        }
        if (Jim_GetLong(interp, argv[2], &x) != JIM_OK ||
            Jim_GetLong(interp, argv[3], &y) != JIM_OK ||
            Jim_GetLong(interp, argv[4], &radius) != JIM_OK ||
            Jim_GetLong(interp, argv[5], &red) != JIM_OK ||
            Jim_GetLong(interp, argv[6], &green) != JIM_OK ||
            Jim_GetLong(interp, argv[7], &blue) != JIM_OK) {
            return JIM_ERR;
        }
        if (argc == 9 && Jim_GetLong(interp, argv[8], &alpha) != JIM_OK)
            return JIM_ERR;
        switch (option) {
            case OPT_CIRCLE:
                circleRGBA(jss->screen, x, y, radius, red, green, blue, alpha);
                break;
            case OPT_AACIRCLE:
                aacircleRGBA(jss->screen, x, y, radius, red, green, blue, alpha);
                break;
            case OPT_FCIRCLE:
                filledCircleRGBA(jss->screen, x, y, radius, red, green, blue, alpha);
                break;
        }
        return JIM_OK;
    }
    else if (option == OPT_FREE) {
        /* FREE */
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_DeleteCommand(interp, argv[0]);
        return JIM_OK;
    }
    else if (option == OPT_CLEAR) {
        long vals[4];
        if (argc != 5 && argc != 6) {
            Jim_WrongNumArgs(interp, 2, argv, "red green blue ?alpha?");
            return JIM_ERR;
        }
        if (JimSdlGetLongs(interp, argc - 2, argv + 2, vals) != JIM_OK) {
            return JIM_ERR;
        }
        if (argc == 5) {
            vals[3] = SDL_ALPHA_OPAQUE;
        }
        JimSdlClear(jss, vals[0], vals[1], vals[2], vals[3]);
    }
    else if (option == OPT_FLIP) {
        /* FLIP */
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        {
            SDL_Event e;
#if SDL_MAJOR_VERSION == 2
            SDL_RenderPresent(jss->screen);
#else
            SDL_Flip(jss->screen);
#endif
            JimSdlClear(jss, 0, 0, 0, SDL_ALPHA_OPAQUE);
            /* Throw away all events except quit, and pass this back as JIM_EXIT.
             * If necessary, this can be caught with catch -exit { ... }
             */
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    Jim_SetResultInt(interp, 0);
                    return JIM_EXIT;
                }
            }
        }
        return JIM_OK;
    }
    return JIM_OK;
}

static int JimSdlSurfaceCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss;
    char buf[128];
    long xres, yres;

    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "xres yres");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, argv[1], &xres) != JIM_OK ||
        Jim_GetLong(interp, argv[2], &yres) != JIM_OK)
        return JIM_ERR;

    jss = Jim_Alloc(sizeof(*jss));
    memset(jss, 0, sizeof(*jss));
    jss->background[3] = SDL_ALPHA_OPAQUE;

#if SDL_MAJOR_VERSION == 2
    /* Try to create the surface */
    jss->win = SDL_CreateWindow("sdl", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, xres, yres, 0);
    if (jss->win) {
        jss->screen = SDL_CreateRenderer(jss->win, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
        if (jss->screen) {
            /* Need an initial SDL_PollEvent() to make the window display */
            SDL_PollEvent(NULL);
        }
        else {
            SDL_DestroyWindow(jss->win);
        }
    }
#else
    jss->screen = SDL_SetVideoMode(xres, yres, 32, SDL_SWSURFACE | SDL_ANYFORMAT);
#endif
    if (jss->screen) {
        JimSdlClear(jss, 0, 0, 0, SDL_ALPHA_OPAQUE);
    }
    else {
        JimSdlSetError(interp);
        Jim_Free(jss);
        return JIM_ERR;
    }

    /* Create the SDL command */
    snprintf(buf, sizeof(buf), "sdl.surface%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimSdlHandlerCommand, jss, JimSdlDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));
    return JIM_OK;
}

int Jim_sdlInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "sdl", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        JimSdlSetError(interp);
        return JIM_ERR;
    }
    atexit(SDL_Quit);
    Jim_CreateCommand(interp, "sdl.screen", JimSdlSurfaceCommand, NULL, NULL);
    return JIM_OK;
}
