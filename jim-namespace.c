/*
 * Support for namespaces in jim
 *
 * (c) 2011 Steve Bennett <steveb@workware.net.au>
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
#include <assert.h>

#include "jim.h"
#include "jimautoconf.h"
#include "jim-subcmd.h"

/* -----------------------------------------------------------------------------
 * Namespace support
 * ---------------------------------------------------------------------------*/

/**
 * nsObj is a canonical namespace name (.e.g. "" for root, "abc" for ::abc)
 *
 * The given name is appended to the namespace name to produce a complete canonical name.
 *
 * e.g. "" "abc"         => abc
 *      "" "::abc"       => abc
 *      "" "abc::def"    => abc::def
 *      "abc" "def"      => abc::def
 *      "abc" "::def"    => def
 *
 */
Jim_Obj *JimCanonicalNamespace(Jim_Interp *interp, Jim_Obj *nsObj, Jim_Obj *nameObj)
{
    Jim_Obj *objPtr;
    const char *name = Jim_String(nameObj);
    assert(nameObj->refCount != 0);
    assert(nsObj->refCount != 0);
    if (name[0] == ':' && name[1] == ':') {
        /* Absolute namespace */
        while (*++name == ':') {
        }
        return Jim_NewStringObj(interp, name, -1);
    }
    if (Jim_Length(nsObj) == 0) {
        /* Relative to the global namespace */
        return nameObj;
    }
    /* Relative to non-global namespace */
    objPtr = Jim_DuplicateObj(interp, nsObj);
    Jim_AppendString(interp, objPtr, "::", 2);
    Jim_AppendObj(interp, objPtr, nameObj);
    return objPtr;
}

int Jim_CreateNamespaceVariable(Jim_Interp *interp, Jim_Obj *varNameObj, Jim_Obj *targetNameObj)
{
    int rc;
    Jim_IncrRefCount(varNameObj);
    Jim_IncrRefCount(targetNameObj);

    /* push non-namespace vars if in namespace eval? */
    rc  = Jim_SetVariableLink(interp, varNameObj, targetNameObj, interp->topFramePtr);
    if (rc == JIM_ERR) {
        /* This is the only reason the link can fail */
        Jim_SetResultFormatted(interp, "can't define \"%#s\": name refers to an element in an array", varNameObj);
    }

    Jim_DecrRefCount(interp, varNameObj);
    Jim_DecrRefCount(interp, targetNameObj);

    return rc;
}

/**
 * Returns the parent of the given namespace.
 *
 * ::bob::tom => ::bob
 * bob::tom   => bob
 * ::bob      => ::
 * bob        => ""
 * ::         => ""
 * ""         => ""
 */
Jim_Obj *Jim_NamespaceQualifiers(Jim_Interp *interp, Jim_Obj *ns)
{
    const char *name = Jim_String(ns);
    const char *pt = strrchr(name, ':');
    if (pt && pt != name && pt[-1] == ':') {
        return Jim_NewStringObj(interp, name, (int)(pt - name - 1));
    }
    else {
        return interp->emptyObj;
    }
}

Jim_Obj *Jim_NamespaceTail(Jim_Interp *interp, Jim_Obj *ns)
{
    const char *name = Jim_String(ns);
    const char *pt = strrchr(name, ':');
    if (pt && pt != name && pt[-1] == ':') {
        return Jim_NewStringObj(interp, pt + 1, -1);
    }
    else {
        return ns;
    }
}

static Jim_Obj *JimNamespaceCurrent(Jim_Interp *interp)
{
    Jim_Obj *objPtr = Jim_NewStringObj(interp, "::", 2);
    Jim_AppendObj(interp, objPtr, interp->framePtr->nsObj);
    return objPtr;
}

static int JimVariableCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retcode = JIM_OK;

    if (argc > 3) {
        Jim_WrongNumArgs(interp, 1, argv, "name ?value?");
        return JIM_ERR;
    }
    if (argc > 1) {
        Jim_Obj *targetNameObj;
        Jim_Obj *localNameObj;

        targetNameObj = JimCanonicalNamespace(interp, interp->framePtr->nsObj, argv[1]);

        localNameObj = Jim_NamespaceTail(interp, argv[1]);
        Jim_IncrRefCount(localNameObj);
        if (interp->framePtr->level != 0 || Jim_Length(interp->framePtr->nsObj) != 0) {
            retcode = Jim_CreateNamespaceVariable(interp, localNameObj, targetNameObj);
        }

        /* Set the variable via the local name */
        if (retcode == JIM_OK && argc > 2) {
            retcode = Jim_SetVariable(interp, localNameObj, argv[2]);
        }
        Jim_DecrRefCount(interp, localNameObj);
    }
    return retcode;
}

/* Used to invoke script-based helpers.
 * It would be ideal if ensembles were supported in the core
 */
static int Jim_EvalEnsemble(Jim_Interp *interp, const char *basecmd, const char *subcmd, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *prefixObj = Jim_NewStringObj(interp, basecmd, -1);

    Jim_AppendString(interp, prefixObj, " ", 1);
    Jim_AppendString(interp, prefixObj, subcmd, -1);

    return Jim_EvalObjPrefix(interp, prefixObj, argc, argv);
}

static int JimNamespaceCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *nsObj;
    Jim_Obj *objPtr;
    int option;
    static const char * const options[] = {
        "eval", "current", "canonical", "qualifiers", "parent", "tail", "delete",
        "origin", "code", "inscope", "import", "export",
        "which", "upvar", NULL
    };
    enum
    {
        OPT_EVAL, OPT_CURRENT, OPT_CANONICAL, OPT_QUALIFIERS, OPT_PARENT, OPT_TAIL, OPT_DELETE,
        OPT_ORIGIN, OPT_CODE, OPT_INSCOPE, OPT_IMPORT, OPT_EXPORT,
        OPT_WHICH, OPT_UPVAR,
    };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "subcommand ?arg ...?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], options, &option, "subcommand", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
        return JIM_ERR;
    }

    switch (option) {
        case OPT_EVAL:
            if (argc < 4) {
                Jim_WrongNumArgs(interp, 2, argv, "name arg ?arg...?");
                return JIM_ERR;
            }
            if (argc == 4) {
                objPtr = argv[3];
            }
            else {
                objPtr = Jim_ConcatObj(interp, argc - 3, argv + 3);
            }

            nsObj = JimCanonicalNamespace(interp, interp->framePtr->nsObj, argv[2]);
            return Jim_EvalNamespace(interp, objPtr, nsObj);

        case OPT_CURRENT:
            if (argc != 2) {
                Jim_WrongNumArgs(interp, 2, argv, "");
                return JIM_ERR;
            }
            Jim_SetResult(interp, JimNamespaceCurrent(interp));
            return JIM_OK;

        case OPT_CANONICAL:
            if (argc > 4) {
                Jim_WrongNumArgs(interp, 2, argv, "?current? ?name?");
                return JIM_ERR;
            }
            if (argc == 2) {
                Jim_SetResult(interp, interp->framePtr->nsObj);
            }
            else if (argc == 3) {
                Jim_SetResult(interp, JimCanonicalNamespace(interp, interp->framePtr->nsObj, argv[2]));
            }
            else {
                Jim_SetResult(interp, JimCanonicalNamespace(interp, argv[2], argv[3]));
            }
            return JIM_OK;

        case OPT_QUALIFIERS:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "string");
                return JIM_ERR;
            }
            Jim_SetResult(interp, Jim_NamespaceQualifiers(interp, argv[2]));
            return JIM_OK;

        case OPT_EXPORT:
            return JIM_OK;

        case OPT_TAIL:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "string");
                return JIM_ERR;
            }
            Jim_SetResult(interp, Jim_NamespaceTail(interp, argv[2]));
            return JIM_OK;

        case OPT_PARENT:
            if (argc != 2 && argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "?name?");
                return JIM_ERR;
            }
            else {
                const char *name;

                if (argc == 3) {
                    objPtr = argv[2];
                }
                else {
                    objPtr = interp->framePtr->nsObj;
                }
                if (Jim_Length(objPtr) == 0 || Jim_CompareStringImmediate(interp, objPtr, "::")) {
                    return JIM_OK;
                }
                objPtr = Jim_NamespaceQualifiers(interp, objPtr);

                name = Jim_String(objPtr);

                if (name[0] != ':' || name[1] != ':') {
                    /* Make it fully scoped */
                    Jim_SetResultString(interp, "::", 2);
                    Jim_AppendObj(interp, Jim_GetResult(interp), objPtr);
                    Jim_IncrRefCount(objPtr);
                    Jim_DecrRefCount(interp, objPtr);
                }
                else {
                    Jim_SetResult(interp, objPtr);
                }
            }
            return JIM_OK;
    }

    /* Implemented as a Tcl helper proc.
     * Note that calling a proc will change the current namespace,
     * so helper procs must call [uplevel namespace canon] to get the callers
     * namespace.
     */
    return Jim_EvalEnsemble(interp, "namespace", options[option], argc - 2, argv + 2);
}

int Jim_namespaceInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "namespace", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "namespace", JimNamespaceCmd, NULL, NULL);
    Jim_CreateCommand(interp, "variable", JimVariableCmd, NULL, NULL);
    return JIM_OK;
}

