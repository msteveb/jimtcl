#include <jim.h>

/* -----------------------------------------------------------------------------
 * Dynamic libraries support (WIN32 not supported)
 * ---------------------------------------------------------------------------*/

#ifdef JIM_DYNLIB
#ifdef WIN32
#define RTLD_LAZY 0
void * dlopen(const char *path, int mode) 
{
    JIM_NOTUSED(mode);

    return (void *)LoadLibraryA(path);
}
int dlclose(void *handle)
{
    FreeLibrary((HANDLE)handle);
    return 0;
}
void *dlsym(void *handle, const char *symbol)
{
    return GetProcAddress((HMODULE)handle, symbol);
}
static char win32_dlerror_string[121];
const char *dlerror(void)
{
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
                   LANG_NEUTRAL, win32_dlerror_string, 120, NULL);
    return win32_dlerror_string;
}
#endif /* WIN32 */

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
        Jim_ListLength(interp, libPathObjPtr, &prefixc);
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
            Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
            Jim_AppendStrings(interp, Jim_GetResult(interp),
                "error loading extension \"", pathName,
                "\": ", dlerror(), NULL);
            if (i < 0)
                continue;
            goto err;
        }
        if ((onload = dlsym(handle, "Jim_OnLoad")) == NULL) {
            Jim_SetResultString(interp,
                    "No Jim_OnLoad symbol found on extension", -1);
            goto err;
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
