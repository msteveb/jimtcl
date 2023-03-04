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

#include <jim.h>

/*
 * We have a list of sample words in 'C'..
 */
const char *strings[] = {
	"simple",
	"strings",
	"which",
	"should",
	"get",
	"interpreted",
	"by",
	"Jim",
};

/* 
 * We have macros which let us to easily obtain of array presented above
 */
#define	ARRAY_SIZE(a)	(sizeof((a)) / sizeof((a)[0]))
#define	SAMPLE_OBJS	ARRAY_SIZE(strings)

/*
 * Now we try to write big enough code to duplication our array in Jim's
 * list implementation. Later, we try to load a sample script in Tcl that
 * could print our list.
 */
int
main(int argc, char **argv)
{
	Jim_Interp *interp;
	Jim_Obj	*obj[SAMPLE_OBJS];
	Jim_Obj	*list;
	int i;
	int error;

	/* Create an interpreter */
	interp = Jim_CreateInterp();

	/* We register base commands, so that we actually implement Tcl. */
	Jim_RegisterCoreCommands(interp);

	/* And initialise any static extensions */
	Jim_InitStaticExtensions(interp);

	/* Create an empty list */
	list = Jim_NewListObj(interp, NULL, 0);
	assert(list != NULL);

	/*
	 * For each string..
	 */
	for (i = 0; i < SAMPLE_OBJS; i++) {
		/* Duplicate it as an array member. */
		obj[i] = Jim_NewStringObj(interp, strings[i], -1);
		assert(obj[i] != NULL);

		/* We append newly created object to the list */
		Jim_ListAppendElement(interp, list, obj[i]);
	}

	/* 
	 * We bind a Tcl's name with our list, so that Tcl script can
	 * identify the variable.
	 */
	Jim_SetVariableStr(interp, "MYLIST", list);

	/*
	 * Parse a script
	 */
	error = Jim_EvalFile(interp, "./print.tcl");
	if (error == JIM_ERR) {
		Jim_MakeErrorMessage(interp);
		fprintf(stderr, "%s\n", Jim_GetString(Jim_GetResult(interp), NULL));
	}
	Jim_FreeInterp(interp);
	return (EXIT_SUCCESS);
}
