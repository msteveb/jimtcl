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
#include <jimautoconf.h>
#include <SDL.h>
#if SDL_MAJOR_VERSION == 2
#include <SDL2_gfxPrimitives.h>
#ifdef HAVE_PKG_SDL2_TTF
#include <SDL_ttf.h>
#endif
#else
#include <SDL_gfxPrimitives.h>
#endif

#include <jim.h>
#include <jim-subcmd.h>

static int jim_sdl_initialised;

typedef struct JimSdlSurface
{
#if SDL_MAJOR_VERSION == 2
    SDL_Window *win;
    SDL_Renderer *screen;
    SDL_Texture *texture;
#ifdef HAVE_PKG_SDL2_TTF
    TTF_Font *font;
#endif
#else
    SDL_Surface *screen;
#endif
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
#ifdef HAVE_PKG_SDL2_TTF
    if (jss->font) {
        TTF_CloseFont(jss->font);
    }
#endif
#else
    SDL_FreeSurface(jss->screen);
#endif
    Jim_Free(jss);
}

static int JimSdlGetLongs(Jim_Interp *interp, int argc, Jim_Obj *const *argv, long *dest)
{
    while (argc) {
        jim_wide w;
        if (Jim_GetWideExpr(interp, *argv, &w) != JIM_OK) {
            return JIM_ERR;
        }
        *dest++ = w;
        argc--;
        argv++;
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

/* Process the event loop, throwing away all events except quit.
 * On quit, return JIM_EXIT.
 * If necessary, this can be caught with catch -exit { ... }
 */
static int JimSdlPoll(Jim_Interp *interp)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            Jim_SetResultInt(interp, 0);
            return JIM_EXIT;
        }
    }
    return JIM_OK;
}

static int jim_sdl_subcmd_free(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_DeleteCommand(interp, argv[0]);
    return JIM_OK;
}

/* [sdl flip] - present the current image, clear the new image, poll for events.
 * Returns JIM_EXIT on quit event
 */
static int jim_sdl_subcmd_flip(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
#if SDL_MAJOR_VERSION == 2
    SDL_RenderPresent(jss->screen);
#else
    SDL_Flip(jss->screen);
#endif
    JimSdlClear(jss, 0, 0, 0, SDL_ALPHA_OPAQUE);

    return JimSdlPoll(interp);
}

/* [sdl poll ?script?] - present the current image, poll for events.
 * Returns JIM_EXIT on quit event or JIM_OK if all events processed.
 *
 * If the script is given, evaluates the script on each poll loop until
 * either quit event is received or the script returns something other than JIM_OK.
 */
static int jim_sdl_subcmd_poll(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int ret = JIM_OK;
#if SDL_MAJOR_VERSION == 2
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    SDL_RenderPresent(jss->screen);
#endif
    while (ret == JIM_OK) {
        ret = JimSdlPoll(interp);
        if (ret != JIM_OK || argc != 1) {
            break;
        }
        ret = Jim_EvalObj(interp, argv[0]);
    }
    return ret;
}

static int jim_sdl_subcmd_clear(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    long vals[4];
    if (JimSdlGetLongs(interp, argc, argv, vals) != JIM_OK) {
        return JIM_ERR;
    }
    if (argc == 3) {
        vals[3] = SDL_ALPHA_OPAQUE;
    }
    JimSdlClear(jss, vals[0], vals[1], vals[2], vals[3]);
    return JIM_OK;
}

static int jim_sdl_subcmd_pixel(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    long vals[6];
    if (JimSdlGetLongs(interp, argc, argv, vals) != JIM_OK) {
        return JIM_ERR;
    }
    if (argc == 5) {
        vals[5] = SDL_ALPHA_OPAQUE;
    }
    pixelRGBA(jss->screen, vals[0], vals[1], vals[2], vals[3], vals[4], vals[5]);
    return JIM_OK;
}

static int jim_sdl_subcmd_circle(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    long vals[7];
    if (JimSdlGetLongs(interp, argc, argv, vals) != JIM_OK) {
        return JIM_ERR;
    }
    if (argc == 6) {
        vals[6] = SDL_ALPHA_OPAQUE;
    }
    circleRGBA(jss->screen, vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6]);
    return JIM_OK;
}

static int jim_sdl_subcmd_aacircle(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    long vals[7];
    if (JimSdlGetLongs(interp, argc, argv, vals) != JIM_OK) {
        return JIM_ERR;
    }
    if (argc == 6) {
        vals[6] = SDL_ALPHA_OPAQUE;
    }
    aacircleRGBA(jss->screen, vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6]);
    return JIM_OK;
}

static int jim_sdl_subcmd_fcircle(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    long vals[7];
    if (JimSdlGetLongs(interp, argc, argv, vals) != JIM_OK) {
        return JIM_ERR;
    }
    if (argc == 6) {
        vals[6] = SDL_ALPHA_OPAQUE;
    }
    filledCircleRGBA(jss->screen, vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6]);
    return JIM_OK;
}

static int jim_sdl_subcmd_rectangle(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    long vals[8];
    if (JimSdlGetLongs(interp, argc, argv, vals) != JIM_OK) {
        return JIM_ERR;
    }
    if (argc == 7) {
        vals[7] = SDL_ALPHA_OPAQUE;
    }
    rectangleRGBA(jss->screen, vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7]);
    return JIM_OK;
}

static int jim_sdl_subcmd_box(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    long vals[8];
    if (JimSdlGetLongs(interp, argc, argv, vals) != JIM_OK) {
        return JIM_ERR;
    }
    if (argc == 7) {
        vals[7] = SDL_ALPHA_OPAQUE;
    }
    boxRGBA(jss->screen, vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7]);
    return JIM_OK;
}

static int jim_sdl_subcmd_line(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    long vals[8];
    if (JimSdlGetLongs(interp, argc, argv, vals) != JIM_OK) {
        return JIM_ERR;
    }
    if (argc == 7) {
        vals[7] = SDL_ALPHA_OPAQUE;
    }
    lineRGBA(jss->screen, vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7]);
    return JIM_OK;
}

static int jim_sdl_subcmd_aaline(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    long vals[8];
    if (JimSdlGetLongs(interp, argc, argv, vals) != JIM_OK) {
        return JIM_ERR;
    }
    if (argc == 7) {
        vals[7] = SDL_ALPHA_OPAQUE;
    }
    aalineRGBA(jss->screen, vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7]);
    return JIM_OK;
}

#ifdef HAVE_PKG_SDL2_TTF
static int jim_sdl_subcmd_font(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    long size;

    if (Jim_GetLong(interp, argv[1], &size) != JIM_OK) {
        return JIM_ERR;
    }
    if (jss->font) {
        TTF_CloseFont(jss->font);
    }
    else {
        TTF_Init();
    }
    jss->font = TTF_OpenFont(Jim_String(argv[0]), size);
    if (jss->font == NULL) {
        Jim_SetResultFormatted(interp, "Failed to load font %#s", argv[0]);
        return JIM_ERR;
    }
    TTF_SetFontHinting(jss->font, TTF_HINTING_LIGHT);
    return JIM_OK;
}

static int jim_sdl_subcmd_text(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    long vals[6];
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_Rect rect;
    SDL_Color col;

    if (!jss->font) {
        Jim_SetResultString(interp, "No font loaded", -1);
        return JIM_ERR;
    }

    if (JimSdlGetLongs(interp, argc - 1, argv + 1, vals) != JIM_OK) {
        return JIM_ERR;
    }
    col.r = vals[2];
    col.g = vals[3];
    col.b = vals[4];
    col.a = (argc == 7) ? vals[5] : SDL_ALPHA_OPAQUE;
#ifdef JIM_UTF8
    surface = TTF_RenderUTF8_Blended(jss->font, Jim_String(argv[0]), col);
#else
    surface = TTF_RenderText_Blended(jss->font, Jim_String(argv[0]), col);
#endif
    texture = SDL_CreateTextureFromSurface(jss->screen, surface);
    rect.x = vals[0];
    rect.y = vals[1];
    rect.w = surface->w;
    rect.h = surface->h;
    SDL_RenderCopy(jss->screen, texture, NULL, &rect);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    return JIM_OK;
}
#endif

static const jim_subcmd_type sdl_command_table[] = {
    {   "free",
        NULL,
        jim_sdl_subcmd_free,
        0,
        0,
        JIM_MODFLAG_FULLARGV,
    },
    {   "flip",
        NULL,
        jim_sdl_subcmd_flip,
        0,
        0,
    },
    {   "poll",
        "?script?",
        jim_sdl_subcmd_poll,
        0,
        1,
    },
    {   "clear",
        "red green blue ?alpha?",
        jim_sdl_subcmd_clear,
        3,
        4,
    },
    {   "pixel",
        "x y red green blue ?alpha?",
        jim_sdl_subcmd_pixel,
        5,
        6,
    },
    {   "circle",
        "x y radius red green blue ?alpha?",
        jim_sdl_subcmd_circle,
        6,
        7,
    },
    {   "aacircle",
        "x y radius red green blue ?alpha?",
        jim_sdl_subcmd_aacircle,
        6,
        7,
    },
    {   "fcircle",
        "x y radius red green blue ?alpha?",
        jim_sdl_subcmd_fcircle,
        6,
        7,
    },
    {   "rectangle",
        "x1 y1 x2 y2 red green blue ?alpha?",
        jim_sdl_subcmd_rectangle,
        7,
        8,
    },
    {   "box",
        "x1 y1 x2 y2 red green blue ?alpha?",
        jim_sdl_subcmd_box,
        7,
        8,
    },
    {   "line",
        "x1 y1 x2 y2 red green blue ?alpha?",
        jim_sdl_subcmd_line,
        7,
        8,
    },
    {   "aaline",
        "x1 y1 x2 y2 red green blue ?alpha?",
        jim_sdl_subcmd_aaline,
        7,
        8,
    },
#ifdef HAVE_PKG_SDL2_TTF
    {   "font",
        "filename.ttf size",
        jim_sdl_subcmd_font,
        2,
        2,
    },
    {   "text",
        "string x y red green blue ?alpha?",
        jim_sdl_subcmd_text,
        6,
        7,
    },
#endif
    { NULL }
};

/* Calls to commands created via [sdl.surface] are implemented by this
 * C command. */
static int JimSdlHandlerCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const jim_subcmd_type *ct = Jim_ParseSubCmd(interp, sdl_command_table, argc, argv);

    return Jim_CallSubCmd(interp, ct, argc, argv);
}

static int JimSdlSurfaceCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss;
    char buf[128];
    long vals[2];
    const char *title;

    if (JimSdlGetLongs(interp, 2, argv + 1, vals) != JIM_OK) {
        return JIM_ERR;
    }

    if (!jim_sdl_initialised) {
        jim_sdl_initialised++;
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            JimSdlSetError(interp);
            return JIM_ERR;
        }
#if SDL_MAJOR_VERSION == 2
        SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
#endif
        atexit(SDL_Quit);
    }

    title = (argc == 4) ? Jim_String(argv[3]) : "sdl";

    jss = Jim_Alloc(sizeof(*jss));
    memset(jss, 0, sizeof(*jss));

#if SDL_MAJOR_VERSION == 2
    /* Try to create the surface */
    jss->win = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, vals[0], vals[1], 0);
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
    jss->screen = SDL_SetVideoMode(vals[0], vals[1], 32, SDL_SWSURFACE | SDL_ANYFORMAT);
    if (jss->screen) {
        SDL_WM_SetCaption(title, title);
    }
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
    Jim_PackageProvideCheck(interp, "sdl");
    Jim_RegisterSimpleCmd(interp, "sdl.screen", "xres yres ?title?", 2, 3, JimSdlSurfaceCommand);
    return JIM_OK;
}
