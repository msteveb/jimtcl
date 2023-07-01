/*
 * Implements the array command for jim
 *
 * (c) 2008 Steve Bennett <steveb@workware.net.au>
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
 *
 * Based on code originally from Tcl 6.7:
 *
 * Copyright 1987-1991 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <jim-subcmd.h>

static int array_cmd_exists(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    /* Just a regular [info exists] */
    Jim_Obj *dictObj = Jim_GetVariable(interp, argv[0], JIM_UNSHARED);
    Jim_SetResultInt(interp, dictObj && Jim_DictSize(interp, dictObj) != -1);
    return JIM_OK;
}

static int array_cmd_get(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);
    Jim_Obj *patternObj;

    if (!objPtr) {
        return JIM_OK;
    }

    patternObj = (argc == 1) ? NULL : argv[1];

    /* Optimise the "all" case */
    if (patternObj == NULL || Jim_CompareStringImmediate(interp, patternObj, "*")) {
        if (Jim_IsList(objPtr) && Jim_ListLength(interp, objPtr) % 2 == 0) {
            /* A list with an even number of elements */
            Jim_SetResult(interp, objPtr);
            return JIM_OK;
        }
    }

    return Jim_DictMatchTypes(interp, objPtr, patternObj, JIM_DICTMATCH_KEYS, JIM_DICTMATCH_KEYS | JIM_DICTMATCH_VALUES);
}

static int array_cmd_names(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);

    if (!objPtr) {
        return JIM_OK;
    }

    return Jim_DictMatchTypes(interp, objPtr, argc == 1 ? NULL : argv[1], JIM_DICTMATCH_KEYS, JIM_DICTMATCH_KEYS);
}

static int array_cmd_unset(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    int len;
    Jim_Obj *resultObj;
    Jim_Obj *objPtr;
    Jim_Obj **dictValuesObj;

    if (argc == 1 || Jim_CompareStringImmediate(interp, argv[1], "*")) {
        /* Unset the whole array */
        Jim_UnsetVariable(interp, argv[0], JIM_NONE);
        return JIM_OK;
    }

    objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);

    if (objPtr == NULL) {
        /* Doesn't exist, so nothing to do */
        return JIM_OK;
    }

    dictValuesObj = Jim_DictPairs(interp, objPtr, &len);
    if (dictValuesObj == NULL) {
        /* Variable is not an array - tclsh ignores this and returns nothing - be compatible */
        Jim_SetResultString(interp, "", -1);
        return JIM_OK;
    }

    /* Create a new object with the values which don't match */
    resultObj = Jim_NewDictObj(interp, NULL, 0);

    for (i = 0; i < len; i += 2) {
        if (!Jim_StringMatchObj(interp, argv[1], dictValuesObj[i], 0)) {
            Jim_DictAddElement(interp, resultObj, dictValuesObj[i], dictValuesObj[i + 1]);
        }
    }

    Jim_SetVariable(interp, argv[0], resultObj);
    return JIM_OK;
}

static int array_cmd_size(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    int len = 0;

    /* Not found means zero length */
    objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);
    if (objPtr) {
        len = Jim_DictSize(interp, objPtr);
        if (len < 0) {
            /* Variable is not an array - tclsh ignores this and returns 0 - be compatible */
            Jim_SetResultInt(interp, 0);
            return JIM_OK;
        }
    }

    Jim_SetResultInt(interp, len);

    return JIM_OK;
}

static int array_cmd_stat(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);
    if (objPtr) {
        return Jim_DictInfo(interp, objPtr);
    }
    Jim_SetResultFormatted(interp, "\"%#s\" isn't an array", argv[0], NULL);
    return JIM_ERR;
}

static int array_cmd_set(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    int len;
    Jim_Obj *listObj = argv[1];
    Jim_Obj *dictObj;

    len = Jim_ListLength(interp, listObj);
    if (len % 2) {
        Jim_SetResultString(interp, "list must have an even number of elements", -1);
        return JIM_ERR;
    }

    dictObj = Jim_GetVariable(interp, argv[0], JIM_UNSHARED);
    if (!dictObj) {
        /* Doesn't exist, so just set the list directly */
        return Jim_SetVariable(interp, argv[0], listObj);
    }
    else if (Jim_DictSize(interp, dictObj) < 0) {
        return JIM_ERR;
    }

    if (Jim_IsShared(dictObj)) {
        dictObj = Jim_DuplicateObj(interp, dictObj);
    }

    for (i = 0; i < len; i += 2) {
        Jim_Obj *nameObj;
        Jim_Obj *valueObj;

        Jim_ListIndex(interp, listObj, i, &nameObj, JIM_NONE);
        Jim_ListIndex(interp, listObj, i + 1, &valueObj, JIM_NONE);

        Jim_DictAddElement(interp, dictObj, nameObj, valueObj);
    }
    return Jim_SetVariable(interp, argv[0], dictObj);
}

static const jim_subcmd_type array_command_table[] = {
        {       "exists",
                "arrayName",
                array_cmd_exists,
                1,
                1,
                /* Description: Does array exist? */
        },
        {       "get",
                "arrayName ?pattern?",
                array_cmd_get,
                1,
                2,
                /* Description: Array contents as name value list */
        },
        {       "names",
                "arrayName ?pattern?",
                array_cmd_names,
                1,
                2,
                /* Description: Array keys as a list */
        },
        {       "set",
                "arrayName list",
                array_cmd_set,
                2,
                2,
                /* Description: Set array from list */
        },
        {       "size",
                "arrayName",
                array_cmd_size,
                1,
                1,
                /* Description: Number of elements in array */
        },
        {       "stat",
                "arrayName",
                array_cmd_stat,
                1,
                1,
                /* Description: Print statistics about an array */
        },
        {       "unset",
                "arrayName ?pattern?",
                array_cmd_unset,
                1,
                2,
                /* Description: Unset elements of an array */
        },
        {       NULL
        }
};

int Jim_arrayInit(Jim_Interp *interp)
{
    Jim_PackageProvideCheck(interp, "array");
    Jim_RegisterSubCmd(interp, "array", array_command_table, NULL);
    return JIM_OK;
}
