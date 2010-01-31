/*-
 * Copyright (c) 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright (c) 2009 Steve Bennett <steveb@workware.net.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 *
 * Jimsh - An interactive shell for Jim
 */

#ifdef WIN32
#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif /* WIN32 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JIM_EMBEDDED
#include "jim.h"


/* JimGetExePath try to get the absolute path of the directory
 * of the jim binary, in order to add this path to the library path.
 * Likely shipped libraries are in the same path too. */

/* That's simple on windows: */
#ifdef WIN32
static Jim_Obj *JimGetExePath(Jim_Interp *interp, const char *argv0)
{
    char path[MAX_PATH+1], *p;
    JIM_NOTUSED(argv0);

    GetModuleFileNameA(NULL, path, MAX_PATH);
    if ((p = strrchr(path, '\\')) != NULL)
        *p = 0;
    return Jim_NewStringObj(interp, path, -1);
}
#else /* WIN32 */
#ifndef JIM_ANSIC
/* A bit complex on POSIX */
#include <unistd.h>
static Jim_Obj *JimGetExePath(Jim_Interp *interp, const char *argv0)
{
    char path[JIM_PATH_LEN+1];

    /* Check if the executable was called with an absolute pathname */
    if (argv0[0] == '/') {
        char *p;

        strncpy(path, argv0, JIM_PATH_LEN);
        p = strrchr(path, '/');
            *(p+1) = '\0';
        return Jim_NewStringObj(interp, path, -1);
    } else {
        char cwd[JIM_PATH_LEN+1];
        char base[JIM_PATH_LEN+1], *p;
        int l;

        strncpy(base, argv0, JIM_PATH_LEN);
        if (getcwd(cwd, JIM_PATH_LEN) == NULL) {
            return Jim_NewStringObj(interp, "/usr/local/lib/jim/", -1);
        }
        l = strlen(cwd);
        if (l > 0 && cwd[l-1] == '/')
            cwd[l-1] = '\0';
        p = strrchr(base, '/');
        if (p == NULL)
            base[0] = '\0';
        else if (p != base)
            *p = '\0';
        sprintf(path, "%s/%s", cwd, base);
        l = strlen(path);
        if (l > 2 && path[l-2] == '/' && path[l-1] == '.')
            path[l-1] = '\0';
        return Jim_NewStringObj(interp, path, -1);
    }
}
#else /* JIM_ANSIC */
/* ... and impossible with just ANSI C */
static Jim_Obj *JimGetExePath(Jim_Interp *interp, const char *argv0)
{
    JIM_NOTUSED(argv0);
    return Jim_NewStringObj(interp, "/usr/local/lib/jim/", -1);
}
#endif /* JIM_ANSIC */
#endif /* WIN32 */

static void JimLoadJimRc(Jim_Interp *interp)
{
    const char *home;
    char buf [JIM_PATH_LEN+1];
    const char *names[] = {".jimrc", "jimrc.tcl", NULL};
    int i;
    FILE *fp;

    if ((home = getenv("HOME")) == NULL) return;
    for (i = 0; names[i] != NULL; i++) {
        if (strlen(home)+strlen(names[i])+1 > JIM_PATH_LEN) continue;
        sprintf(buf, "%s/%s", home, names[i]);
        if ((fp = fopen(buf, "r")) != NULL) {
            fclose(fp);
            if (Jim_EvalFile(interp, buf) != JIM_OK) {
                Jim_PrintErrorMessage(interp);
            }
            return;
        }
    }
}

int main(int argc, char *const argv[])
{
    int retcode, n;
    Jim_Interp *interp;
    Jim_Obj *listObj;

    Jim_InitEmbedded(); /* This is the first function embedders should call. */

    /* Create and initialize the interpreter */
    interp = Jim_CreateInterp();
    Jim_RegisterCoreCommands(interp);

    /* Append the path where the executed Jim binary is contained
     * in the jim_libpath list. */
    listObj = Jim_GetVariableStr(interp, "jim_libpath", JIM_NONE);
    if (Jim_IsShared(listObj))
        listObj = Jim_DuplicateObj(interp, listObj);
    Jim_ListAppendElement(interp, listObj, JimGetExePath(interp, argv[0]));
    Jim_SetVariableStr(interp, "jim_libpath", listObj);

    /* Populate argv and argv0 global vars */
    listObj = Jim_NewListObj(interp, NULL, 0);
    for (n = 2; n < argc; n++) {
        Jim_Obj *obj = Jim_NewStringObj(interp, argv[n], -1);
        Jim_ListAppendElement(interp, listObj, obj);
    }

    Jim_SetVariableStr(interp, "argv", listObj);
    
    if (argc == 1) {
        Jim_SetVariableStrWithStr(interp, "jim_interactive", "1");
        JimLoadJimRc(interp);
        retcode = Jim_InteractivePrompt(interp);
    } else {
        Jim_SetVariableStr(interp, "argv0", Jim_NewStringObj(interp, argv[1], -1));
        Jim_SetVariableStrWithStr(interp, "jim_interactive", "0");
        if ((retcode = Jim_EvalFile(interp, argv[1])) == JIM_ERR) {
            Jim_PrintErrorMessage(interp);
        }
    }
    if (retcode == JIM_OK) {
        retcode = 0;
    }
    else if (retcode == JIM_EXIT) {
        retcode = interp->exitCode;
    }
    else {
        retcode = 1;
    }
    Jim_FreeInterp(interp);
    return retcode;
}
