#include <unistd.h>
#include <string.h>

#include <jim.h>

# include <sys/types.h>
# include <dirent.h>

/* -----------------------------------------------------------------------------
 * Packages handling
 * ---------------------------------------------------------------------------*/

#define JIM_PKG_ANY_VERSION -1

int Jim_PackageProvide(Jim_Interp *interp, const char *name, const char *ver,
        int flags)
{
    /* If the package was already provided returns an error. */
    if (Jim_FindHashEntry(&interp->packages, name) != NULL) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
            Jim_AppendStrings(interp, Jim_GetResult(interp),
                    "package '", name, "' was already provided", NULL);
        }
        return JIM_ERR;
    }
    Jim_AddHashEntry(&interp->packages, name, (char*) ver);
    return JIM_OK;
}

static int Jim_PackageList(Jim_Interp *interp)
{
    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;
    Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);
    
    htiter = Jim_GetHashTableIterator(&interp->packages);
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        Jim_ListAppendElement(interp, listObjPtr,
                Jim_NewStringObj(interp, he->key, -1));
    }
    Jim_FreeHashTableIterator(htiter);

    Jim_SetResult(interp, listObjPtr);

    return JIM_OK;
}

static char *JimFindPackage(Jim_Interp *interp, char **prefixes,
        int prefixc, const char *pkgName)
{
    int i;

    for (i = 0; i < prefixc; i++) {
        char buf[JIM_PATH_LEN];

        if (prefixes[i] == NULL) continue;

        snprintf(buf, sizeof(buf), "%s/%s.tcl", prefixes[i], pkgName);
        if (access(buf, R_OK) == 0) {
            return Jim_StrDup(buf);
        }
        snprintf(buf, sizeof(buf), "%s/%s.so", prefixes[i], pkgName);
        if (access(buf, R_OK) == 0) {
            return Jim_StrDup(buf);
        }
    }
    return NULL;
}

/* Search for a suitable package under every dir specified by jim_libpath
 * and load it if possible. If a suitable package was loaded with success
 * JIM_OK is returned, otherwise JIM_ERR is returned. */
static int JimLoadPackage(Jim_Interp *interp, const char *name, int flags)
{
    Jim_Obj *libPathObjPtr;
    char **prefixes, *path;
    int prefixc, i, retCode = JIM_OK;

    libPathObjPtr = Jim_GetGlobalVariableStr(interp, "jim_libpath", JIM_NONE);
    if (libPathObjPtr == NULL) {
        prefixc = 0;
        libPathObjPtr = NULL;
    } else {
        Jim_IncrRefCount(libPathObjPtr);
        Jim_ListLength(interp, libPathObjPtr, &prefixc);
    }

    prefixes = Jim_Alloc(sizeof(char*)*prefixc);
    for (i = 0; i < prefixc; i++) {
            Jim_Obj *prefixObjPtr;
            if (Jim_ListIndex(interp, libPathObjPtr, i,
                    &prefixObjPtr, JIM_NONE) != JIM_OK)
            {
                prefixes[i] = NULL;
                continue;
            }
            prefixes[i] = Jim_StrDup(Jim_GetString(prefixObjPtr, NULL));
    }

    /* Scan every directory for the the first match */
    path = JimFindPackage(interp, prefixes, prefixc, name);
    if (path != NULL) {
        char *p = strrchr(path, '.');
        /* Try to load/source it */
        if (p && strcmp(p, ".tcl") == 0) {
            retCode = Jim_EvalFile(interp, path);
        } else {
            retCode = Jim_LoadLibrary(interp, path);
        }
    } else {
        retCode = JIM_ERR;
    }
    Jim_Free(path);
    for (i = 0; i < prefixc; i++)
        Jim_Free(prefixes[i]);
    Jim_Free(prefixes);
    if (libPathObjPtr)
        Jim_DecrRefCount(interp, libPathObjPtr);
    return retCode;
}

const char *Jim_PackageRequire(Jim_Interp *interp, const char *name,
        const char *ver, int flags)
{
    Jim_HashEntry *he;

    /* Start with an empty error string */
    Jim_SetResultString(interp, "", 0);

    he = Jim_FindHashEntry(&interp->packages, name);
    if (he == NULL) {
        /* Try to load the package. */
        if (JimLoadPackage(interp, name, flags) == JIM_OK) {
            he = Jim_FindHashEntry(&interp->packages, name);
            if (he == NULL) {
                /* Did not call package provide, so we do it for them */
                Jim_PackageProvide(interp, name, "1.0", 0);
                return "1.0";
            }
            return he->val;
        }
        /* No way... return an error. */
        if (flags & JIM_ERRMSG) {
            int len;
            Jim_GetString(Jim_GetResult(interp), &len);
            Jim_AppendStrings(interp, Jim_GetResult(interp), len ? "\n" : "",
                    "Can't find package '", name, "'", NULL);
        }
        return NULL;
    } else {
        return he->val;
    }
}

/* [package] */
int Jim_PackageCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int option;
    const char *options[] = {
        "require", "provide", "list", NULL
    };
    enum {OPT_REQUIRE, OPT_PROVIDE, OPT_LIST};

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "option ?arguments ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], options, &option, "option",
                JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;

    if (option == OPT_REQUIRE) {
        int exact = 0;
        const char *ver;

        if (Jim_CompareStringImmediate(interp, argv[2], "-exact")) {
            exact = 1;
            argv++;
            argc--;
        }
        if (argc != 3 && argc != 4) {
            Jim_WrongNumArgs(interp, 2, argv, "?-exact? package ?version?");
            return JIM_ERR;
        }
        ver = Jim_PackageRequire(interp, Jim_GetString(argv[2], NULL),
                argc == 4 ? Jim_GetString(argv[3], NULL) : "",
                JIM_ERRMSG);
        if (ver == NULL)
            return JIM_ERR_ADDSTACK;
        Jim_SetResultString(interp, ver, -1);
    } else if (option == OPT_PROVIDE) {
        if (argc != 4) {
            Jim_WrongNumArgs(interp, 2, argv, "package version");
            return JIM_ERR;
        }
        return Jim_PackageProvide(interp, Jim_GetString(argv[2], NULL),
                    Jim_GetString(argv[3], NULL), JIM_ERRMSG);
    } else if (option == OPT_LIST) {
        return Jim_PackageList(interp);
    }
    return JIM_OK;
}

int Jim_packageInit(Jim_Interp *interp)
{
    Jim_CreateCommand(interp, "package", Jim_PackageCoreCommand, NULL, NULL);
    return JIM_OK;
}
