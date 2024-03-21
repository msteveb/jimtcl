#include <string.h>
#include <stdio.h>

#include "jimautoconf.h"
#include <jim-subcmd.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#else
#define R_OK 4
#endif

/* All packages have a fixed, dummy version */
static const char *package_version_1 = "1.0";

/* -----------------------------------------------------------------------------
 * Packages handling
 * ---------------------------------------------------------------------------*/

int Jim_PackageProvide(Jim_Interp *interp, const char *name, const char *ver, int flags)
{
    /* If the package was already provided returns an error. */
    Jim_HashEntry *he = Jim_FindHashEntry(&interp->packages, name);

    /* An empty result means the automatic entry. This can be replaced */
    if (he && *(const char *)he->u.val) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp, "package \"%s\" was already provided", name);
        }
        return JIM_ERR;
    }
    Jim_ReplaceHashEntry(&interp->packages, name, (char *)ver);
    return JIM_OK;
}

/**
 * Searches along a of paths for the given package.
 *
 * Returns the allocated path to the package file if found,
 * or NULL if not found.
 */
static char *JimFindPackage(Jim_Interp *interp, Jim_Obj *prefixListObj, const char *pkgName)
{
    int i;
    char *buf = Jim_Alloc(JIM_PATH_LEN);
    int prefixc = Jim_ListLength(interp, prefixListObj);

    for (i = 0; i < prefixc; i++) {
        Jim_Obj *prefixObjPtr = Jim_ListGetIndex(interp, prefixListObj, i);
        const char *prefix;

        if (Jim_GetObjTaint(prefixObjPtr)) {
            /* This element is tainted, so ignore it */
            continue;
        }
        prefix = Jim_String(prefixObjPtr);

        /* Loadable modules are tried first */
#ifdef jim_ext_load
        snprintf(buf, JIM_PATH_LEN, "%s/%s.so", prefix, pkgName);
        if (access(buf, R_OK) == 0) {
            return buf;
        }
#endif
        if (strcmp(prefix, ".") == 0) {
            snprintf(buf, JIM_PATH_LEN, "%s.tcl", pkgName);
        }
        else {
            snprintf(buf, JIM_PATH_LEN, "%s/%s.tcl", prefix, pkgName);
        }

        if (access(buf, R_OK) == 0) {
            return buf;
        }
    }
    Jim_Free(buf);
    return NULL;
}

/* Search for a suitable package under every dir specified by JIM_LIBPATH,
 * and load it if possible. If a suitable package was loaded with success
 * JIM_OK is returned, otherwise JIM_ERR is returned. */
static int JimLoadPackage(Jim_Interp *interp, const char *name, int flags)
{
    int retCode = JIM_ERR;
    Jim_Obj *libPathObjPtr = Jim_GetGlobalVariableStr(interp, JIM_LIBPATH, JIM_NONE);
    if (libPathObjPtr) {
        char *path;

        /* Scan every directory for the the first match */
        path = JimFindPackage(interp, libPathObjPtr, name);
        if (path) {
            const char *p;

            /* Note: Even if the file fails to load, we consider the package loaded.
             *       This prevents issues with recursion.
             *       Use a dummy version of "" to signify this case.
             */
            Jim_PackageProvide(interp, name, "", 0);

            /* Try to load/source it */
            p = strrchr(path, '.');

            if (p && strcmp(p, ".tcl") == 0) {
                Jim_IncrRefCount(libPathObjPtr);
                retCode = Jim_EvalFileGlobal(interp, path);
                Jim_DecrRefCount(interp, libPathObjPtr);
            }
#ifdef jim_ext_load
            else {
                retCode = Jim_LoadLibrary(interp, path);
            }
#endif
            if (retCode != JIM_OK) {
                /* Upon failure, remove the dummy entry */
                Jim_DeleteHashEntry(&interp->packages, name);
            }
            Jim_Free(path);
        }
    }
    return retCode;
}

int Jim_PackageRequire(Jim_Interp *interp, const char *name, int flags)
{
    Jim_HashEntry *he;

    /* Start with an empty error string */
    Jim_SetEmptyResult(interp);

    he = Jim_FindHashEntry(&interp->packages, name);
    if (he == NULL) {
        /* Try to load the package. */
        int retcode = JimLoadPackage(interp, name, flags);
        if (retcode != JIM_OK) {
            if (flags & JIM_ERRMSG) {
                int len = Jim_Length(Jim_GetResult(interp));
                Jim_SetResultFormatted(interp, "%#s%sCan't load package %s",
                    Jim_GetResult(interp), len ? "\n" : "", name);
            }
            return retcode;
        }

        /* In case the package did not 'package provide' */
        Jim_PackageProvide(interp, name, package_version_1, 0);

        /* Now it must exist */
        he = Jim_FindHashEntry(&interp->packages, name);
    }

    Jim_SetResultString(interp, he->u.val, -1);
    return JIM_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * package provide name ?version?
 *
 *      This procedure is invoked to declare that
 *      a particular package is now present in an interpreter.
 *      The package must not already be provided in the interpreter.
 *
 * Results:
 *      Returns JIM_OK and sets results as "1.0" (the given version is ignored)
 *
 *----------------------------------------------------------------------
 */
static int package_cmd_provide(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_PackageProvide(interp, Jim_String(argv[0]), package_version_1, JIM_ERRMSG);
}

/*
 *----------------------------------------------------------------------
 *
 * package require name ?version?
 *
 *      This procedure is load a given package.
 *      Note that the version is ignored.
 *
 * Results:
 *      Returns JIM_OK and sets the package version.
 *
 *----------------------------------------------------------------------
 */
static int package_cmd_require(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_PackageRequire(interp, Jim_String(argv[0]), JIM_ERRMSG);
}

/*
 *----------------------------------------------------------------------
 *
 * package list|names
 *
 *      Returns a list of known packages
 *
 * Results:
 *      Returns JIM_OK and sets a list of known packages.
 *
 *----------------------------------------------------------------------
 */
static int package_cmd_names(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;
    Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);

    htiter = Jim_GetHashTableIterator(&interp->packages);
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, he->key, -1));
    }
    Jim_FreeHashTableIterator(htiter);

    Jim_SetResult(interp, listObjPtr);

    return JIM_OK;
}

static const jim_subcmd_type package_command_table[] = {
    {
        "provide",
        "name ?version?",
        package_cmd_provide,
        1,
        2,
        /* Description: Indicates that the current script provides the given package */
    },
    {
        "require",
        "name ?version?",
        package_cmd_require,
        1,
        2,
        /* Description: Loads the given package by looking in standard places */
    },
    {
        "list",
        NULL,
        package_cmd_names,
        0,
        0,
        JIM_MODFLAG_HIDDEN
        /* Description: Deprecated - Lists all known packages */
    },
    {
        "names",
        NULL,
        package_cmd_names,
        0,
        0,
        /* Description: Lists all known packages */
    },
    {
        NULL
    }
};

int Jim_packageInit(Jim_Interp *interp)
{
    Jim_CreateCommand(interp, "package", Jim_SubCmdProc, (void *)package_command_table, NULL);
    return JIM_OK;
}
