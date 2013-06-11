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

#define	OBJ_DESC	"hello world"

int
main(int argc, char **argv)
{
	Jim_Interp *interp;
	Jim_Obj *obj;
	const char *obj_desc;
	int obj_size;

	obj = NULL;
	obj_desc = NULL;
	obj_size = -1;

	/* Create an interpreter */
	interp = Jim_CreateInterp();

	/* We register base commands, so that we actually implement Tcl. */
	Jim_RegisterCoreCommands(interp);

	/* And initialise any static extensions */
	Jim_InitStaticExtensions(interp);

	/* Create a string object */
	obj = Jim_NewStringObj(interp, OBJ_DESC, strlen(OBJ_DESC));

	/* Obtain internal representation of an object */
	obj_desc = Jim_GetString(obj, &obj_size);
	assert(obj_desc != NULL && "Jim should return NULL as a description");
	printf("Object described as '%s'; object size is %d\n", obj_desc,
	    obj_size);

	Jim_FreeObj(interp, obj);
	Jim_FreeInterp(interp);

	return (EXIT_SUCCESS);
}
