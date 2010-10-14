/*-
 * Copyright (c) 2010 Wojciech A. Koszek <wkoszek@FreeBSD.org>
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
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JIM_EMBEDDED
#include <jim.h>

/*
 * Program which we want to get executed.
 */
#define JIM_PROGRAM	"if {1 < 2} { MySampleCommand sample }"

static int
MySampleCommandFunc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
	const char *str;
	int len;

	if (argc != 2) {
		Jim_WrongNumArgs(interp, 1, argv, "string");
		return (JIM_ERR);
	}

	str = Jim_GetString(argv[1], &len);
	assert(str != NULL);
	printf("%s\n", str);

	return (JIM_OK);
}

/*
 * Now we try to write big enough code to duplication our array in Jim's
 * list implementation. Later, we try to load a sample script in Tcl that
 * could print our list.
 */
int
main(int argc, char **argv)
{
	Jim_Interp *interp;
	int error;

	/* Create an interpreter. */
	interp = Jim_CreateInterp();
	assert(interp != NULL && "couldn't create interpreter");

	/* We register base commands, so that we actually implement Tcl. */
	Jim_RegisterCoreCommands(interp);

	/* And initialise any static extensions */
	Jim_InitStaticExtensions(interp);

	/* Register our Jim commands. */
	Jim_CreateCommand(interp, "MySampleCommand", MySampleCommandFunc,
	    NULL, NULL);

	/* Run a script. */
	error = Jim_Eval(interp, JIM_PROGRAM);
	if (error == JIM_ERR) {
		Jim_MakeErrorMessage(interp);
		fprintf(stderr, "%s\n", Jim_GetString(Jim_GetResult(interp), NULL));
		Jim_FreeInterp(interp);
		exit(EXIT_FAILURE);
	}

	Jim_FreeInterp(interp);
	return (EXIT_SUCCESS);
}
