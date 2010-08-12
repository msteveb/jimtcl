#include <jim.h>
#include <string.h>

/* -----------------------------------------------------------------------------
 * Dynamic libraries support (WIN32 not supported)
 * ---------------------------------------------------------------------------*/

#ifdef JIM_DYNLIB

#include <dlfcn.h>

int Jim_LoadLibrary(Jim_Interp *interp, const char *pathName)
{
    Jim_Obj *libPathObjPtr;
    int prefixc, i;
    void *handle;
    int (*onload)(Jim_Interp *interp);

    libPathObjPtr = Jim_GetGlobalVariableStr(interp, JIM_LIBPATH, JIM_NONE);
    if (libPathObjPtr == NULL) {
        prefixc = 0;
        libPathObjPtr = NULL;
    } else {
        Jim_IncrRefCount(libPathObjPtr);
        prefixc = Jim_ListLength(interp, libPathObjPtr);
    }

    for (i = -1; i < prefixc; i++) {
        if (i < 0) {
            handle = dlopen(pathName, RTLD_LAZY);
        } else {
            FILE *fp;
            char buf[JIM_PATH_LEN];
            const char *prefix;
            int prefixlen;
            Jim_Obj *prefixObjPtr;
            
            buf[0] = '\0';
            if (Jim_ListIndex(interp, libPathObjPtr, i,
                    &prefixObjPtr, JIM_NONE) != JIM_OK)
                continue;
            prefix = Jim_GetString(prefixObjPtr, &prefixlen);
            if (prefixlen+strlen(pathName)+1 >= JIM_PATH_LEN)
                continue;
            if (*pathName == '/') {
                strcpy(buf, pathName);
            }    
            else if (prefixlen && prefix[prefixlen-1] == '/')
                sprintf(buf, "%s%s", prefix, pathName);
            else
                sprintf(buf, "%s/%s", prefix, pathName);
            fp = fopen(buf, "r");
            if (fp == NULL)
                continue;
            fclose(fp);
            handle = dlopen(buf, RTLD_LAZY);
        }
        if (handle == NULL) {
            Jim_SetResultFormatted(interp, "error loading extension \"%s\": %s", pathName, dlerror());
            if (i < 0)
                continue;
            goto err;
        }

        /* Now, we use a unique init symbol depending on the extension name.
         * This is done for compatibility between static and dynamic extensions.
         * For extension readline.so, the init symbol is "Jim_readlineInit"
         */
        {
            const char *pt;
            const char *pkgname;
            int pkgnamelen;
            char initsym[50];

            pt = strrchr(pathName, '/');
            if (pt) {
                pkgname = pt + 1;
            } else {
                pkgname = pathName;
            }
            pt = strchr(pkgname, '.');
            if (pt) {
                pkgnamelen = pt - pkgname;
            }
            else {
                pkgnamelen = strlen(pkgname);
            }
            snprintf(initsym, sizeof(initsym), "Jim_%.*sInit", pkgnamelen, pkgname);

            if ((onload = dlsym(handle, initsym)) == NULL) {
                Jim_SetResultFormatted(interp,
                        "No %s symbol found in extension %s", initsym, pathName);
                goto err;
            }
        }
        if (onload(interp) == JIM_ERR) {
            dlclose(handle);
            goto err;
        }
        Jim_SetEmptyResult(interp);
        if (libPathObjPtr != NULL)
            Jim_DecrRefCount(interp, libPathObjPtr);
        return JIM_OK;
    }
err:
    if (libPathObjPtr != NULL)
        Jim_DecrRefCount(interp, libPathObjPtr);
    return JIM_ERR;
}
#else /* JIM_DYNLIB */
int Jim_LoadLibrary(Jim_Interp *interp, const char *pathName)
{
    JIM_NOTUSED(interp);
    JIM_NOTUSED(pathName);

    Jim_SetResultString(interp, "the Jim binary has no support for [load]", -1);
    return JIM_ERR;
}
#endif/* JIM_DYNLIB */

/* [load] */
static int Jim_LoadCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "libaryFile");
        return JIM_ERR;
    }
    return Jim_LoadLibrary(interp, Jim_GetString(argv[1], NULL));
}

int Jim_loadInit(Jim_Interp *interp)
{
    Jim_CreateCommand(interp, "load", Jim_LoadCoreCommand, NULL, NULL);
    return JIM_OK;
}
