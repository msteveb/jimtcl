#include <string.h>

#include <jim.h>


#ifndef JIM_ANSIC

#ifndef WIN32
# include <sys/types.h>
# include <dirent.h>
#else
# include <io.h>
/* Posix dirent.h compatiblity layer for WIN32.
 * Copyright Kevlin Henney, 1997, 2003. All rights reserved.
 * Copyright Salvatore Sanfilippo ,2005.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose is hereby granted without fee, provided
 * that this copyright and permissions notice appear in all copies and
 * derivatives.
 *
 * This software is supplied "as is" without express or implied warranty.
 * This software was modified by Salvatore Sanfilippo for the Jim Interpreter.
 */

struct dirent {
    char *d_name;
};

typedef struct DIR {
    long                handle; /* -1 for failed rewind */
    struct _finddata_t  info;
    struct dirent       result; /* d_name null iff first time */
    char                *name;  /* null-terminated char string */
} DIR;

DIR *opendir(const char *name)
{
    DIR *dir = 0;

    if(name && name[0]) {
        size_t base_length = strlen(name);
        const char *all = /* search pattern must end with suitable wildcard */
            strchr("/\\", name[base_length - 1]) ? "*" : "/*";

        if((dir = (DIR *) Jim_Alloc(sizeof *dir)) != 0 &&
           (dir->name = (char *) Jim_Alloc(base_length + strlen(all) + 1)) != 0)
        {
            strcat(strcpy(dir->name, name), all);

            if((dir->handle = (long) _findfirst(dir->name, &dir->info)) != -1)
                dir->result.d_name = 0;
            else { /* rollback */
                Jim_Free(dir->name);
                Jim_Free(dir);
                dir = 0;
            }
        } else { /* rollback */
            Jim_Free(dir);
            dir   = 0;
            errno = ENOMEM;
        }
    } else {
        errno = EINVAL;
    }
    return dir;
}

int closedir(DIR *dir)
{
    int result = -1;

    if(dir) {
        if(dir->handle != -1)
            result = _findclose(dir->handle);
        Jim_Free(dir->name);
        Jim_Free(dir);
    }
    if(result == -1) /* map all errors to EBADF */
        errno = EBADF;
    return result;
}

struct dirent *readdir(DIR *dir)
{
    struct dirent *result = 0;

    if(dir && dir->handle != -1) {
        if(!dir->result.d_name || _findnext(dir->handle, &dir->info) != -1) {
            result         = &dir->result;
            result->d_name = dir->info.name;
        }
    } else {
        errno = EBADF;
    }
    return result;
}

#endif /* WIN32 */

/* -----------------------------------------------------------------------------
 * Packages handling
 * ---------------------------------------------------------------------------*/

#define JIM_PKG_ANY_VERSION -1

/* Convert a string of the type "1.2" into an integer.
 * MAJOR.MINOR is converted as MAJOR*100+MINOR, so "1.2" is converted 
 * to the integer with value 102 */
static int JimPackageVersionToInt(Jim_Interp *interp, const char *v,
        int *intPtr, int flags)
{
    char *copy;
    jim_wide major, minor;
    char *majorStr, *minorStr, *p;

    if (v[0] == '\0') {
        *intPtr = JIM_PKG_ANY_VERSION;
        return JIM_OK;
    }

    copy = Jim_StrDup(v);
    p = strchr(copy, '.');
    if (p == NULL) goto badfmt;
    *p = '\0';
    majorStr = copy;
    minorStr = p+1;

    if (Jim_StringToWide(majorStr, &major, 10) != JIM_OK ||
        Jim_StringToWide(minorStr, &minor, 10) != JIM_OK)
        goto badfmt;
    *intPtr = (int)(major*100+minor);
    Jim_Free(copy);
    return JIM_OK;

badfmt:
    Jim_Free(copy);
    if (flags & JIM_ERRMSG) {
        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        Jim_AppendStrings(interp, Jim_GetResult(interp),
                "invalid package version '", v, "'", NULL);
    }
    return JIM_ERR;
}

#define JIM_MATCHVER_EXACT (1<<JIM_PRIV_FLAG_SHIFT)
static int JimPackageMatchVersion(int needed, int actual, int flags)
{
    if (needed == JIM_PKG_ANY_VERSION) return 1;
    if (flags & JIM_MATCHVER_EXACT) {
        return needed == actual;
    } else {
        return needed/100 == actual/100 && (needed <= actual);
    }
}

int Jim_PackageProvide(Jim_Interp *interp, const char *name, const char *ver,
        int flags)
{
    int intVersion;
    /* Check if the version format is ok */
    if (JimPackageVersionToInt(interp, ver, &intVersion, JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
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

static char *JimFindBestPackage(Jim_Interp *interp, char **prefixes,
        int prefixc, const char *pkgName, int pkgVer, int flags)
{
    int bestVer = -1, i;
    int pkgNameLen = strlen(pkgName);
    char *bestPackage = NULL;
    struct dirent *de;

    for (i = 0; i < prefixc; i++) {
        DIR *dir;
        char buf[JIM_PATH_LEN];
        int prefixLen;

        if (prefixes[i] == NULL) continue;
        strncpy(buf, prefixes[i], JIM_PATH_LEN);
        buf[JIM_PATH_LEN-1] = '\0';
        prefixLen = strlen(buf);
        if (prefixLen && buf[prefixLen-1] == '/')
            buf[prefixLen-1] = '\0';

        if ((dir = opendir(buf)) == NULL) continue;
        while ((de = readdir(dir)) != NULL) {
            char *fileName = de->d_name;
            int fileNameLen = strlen(fileName);

            if (strncmp(fileName, "jim-", 4) == 0 &&
                strncmp(fileName+4, pkgName, pkgNameLen) == 0 &&
                *(fileName+4+pkgNameLen) == '-' &&
                fileNameLen > 4 && /* note that this is not really useful */
                (strncmp(fileName+fileNameLen-4, ".tcl", 4) == 0 ||
                 strncmp(fileName+fileNameLen-4, ".dll", 4) == 0 ||
                 strncmp(fileName+fileNameLen-3, ".so", 3) == 0))
            {
                char ver[6]; /* xx.yy<nulterm> */
                char *p = strrchr(fileName, '.');
                int verLen, fileVer;

                verLen = p - (fileName+4+pkgNameLen+1);
                if (verLen < 3 || verLen > 5) continue;
                memcpy(ver, fileName+4+pkgNameLen+1, verLen);
                ver[verLen] = '\0';
                if (JimPackageVersionToInt(interp, ver, &fileVer, JIM_NONE)
                        != JIM_OK) continue;
                if (JimPackageMatchVersion(pkgVer, fileVer, flags) &&
                    (bestVer == -1 || bestVer < fileVer))
                {
                    bestVer = fileVer;
                    Jim_Free(bestPackage);
                    bestPackage = Jim_Alloc(strlen(buf)+strlen(fileName)+2);
                    sprintf(bestPackage, "%s/%s", buf, fileName);
                }
            }
        }
        closedir(dir);
    }
    return bestPackage;
}

#else /* JIM_ANSIC */

static char *JimFindBestPackage(Jim_Interp *interp, char **prefixes,
        int prefixc, const char *pkgName, int pkgVer, int flags)
{
    JIM_NOTUSED(interp);
    JIM_NOTUSED(prefixes);
    JIM_NOTUSED(prefixc);
    JIM_NOTUSED(pkgName);
    JIM_NOTUSED(pkgVer);
    JIM_NOTUSED(flags);
    return NULL;
}

#endif /* JIM_ANSIC */

/* Search for a suitable package under every dir specified by jim_libpath
 * and load it if possible. If a suitable package was loaded with success
 * JIM_OK is returned, otherwise JIM_ERR is returned. */
static int JimLoadPackage(Jim_Interp *interp, const char *name, int ver,
        int flags)
{
    Jim_Obj *libPathObjPtr;
    char **prefixes, *best;
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
    /* Scan every directory to find the "best" package. */
    best = JimFindBestPackage(interp, prefixes, prefixc, name, ver, flags);
    if (best != NULL) {
        char *p = strrchr(best, '.');
        /* Try to load/source it */
        if (p && strcmp(p, ".tcl") == 0) {
            retCode = Jim_EvalFile(interp, best);
        } else {
            retCode = Jim_LoadLibrary(interp, best);
        }
    } else {
        retCode = JIM_ERR;
    }
    Jim_Free(best);
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
    int requiredVer;

    /* Start with an empty error string */
    Jim_SetResultString(interp, "", 0);

    if (JimPackageVersionToInt(interp, ver, &requiredVer, JIM_ERRMSG) != JIM_OK)
        return NULL;
    he = Jim_FindHashEntry(&interp->packages, name);
    if (he == NULL) {
        /* Try to load the package. */
        if (JimLoadPackage(interp, name, requiredVer, flags) == JIM_OK) {
            he = Jim_FindHashEntry(&interp->packages, name);
            if (he == NULL) {
                return "?";
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
        int actualVer;
        if (JimPackageVersionToInt(interp, he->val, &actualVer, JIM_ERRMSG)
                != JIM_OK)
        {
            return NULL;
        }
        /* Check if version matches. */
        if (JimPackageMatchVersion(requiredVer, actualVer, flags) == 0) {
            Jim_AppendStrings(interp, Jim_GetResult(interp),
                    "Package '", name, "' already loaded, but with version ",
                    he->val, NULL);
            return NULL;
        }
        return he->val;
    }
}

