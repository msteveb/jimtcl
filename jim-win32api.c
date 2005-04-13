/* WIN32 API extension
 *
 * Copyright (C) 2005 Pat Thoyts <patthoyts@users.sourceforge.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * A copy of the license is also included in the source distribution
 * of Jim, as a TXT file name called LICENSE.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <lmcons.h>

#include <stdio.h>

#define JIM_EXTENSION
#include "jim.h"

#if _MSC_VER >= 1000
#pragma comment(lib, "shell32")
#pragma comment(lib, "user32")
#pragma comment(lib, "advapi32")
#endif /* _MSC_VER >= 1000 */

static const char win32api_type_hash[] = "win32api:typemap";

__declspec(dllexport) int Jim_OnLoad(Jim_Interp *interp);

/* ----------------------------------------------------------------------
 * Convert Win32 error codes into an error message string object.
 */

static Jim_Obj *
Win32ErrorObj(Jim_Interp *interp, const char * szPrefix, DWORD dwError)
{
    Jim_Obj *msgObj = NULL;
    char * lpBuffer = NULL;
    DWORD  dwLen = 0;
    
    dwLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER 
        | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwError, LANG_NEUTRAL,
        (char *)&lpBuffer, 0, NULL);
    if (dwLen < 1) {
        dwLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY,
            "code 0x%1!08X!%n", 0, LANG_NEUTRAL,
            (char *)&lpBuffer, 0, (va_list *)&dwError);
    }
    
    msgObj = Jim_NewStringObj(interp, szPrefix, -1);
    if (dwLen > 0) {
        char *p = lpBuffer + dwLen - 1;        /* remove cr-lf at end */
        for ( ; p && *p && isspace(*p); p--)
            ;
        *++p = 0;
        Jim_AppendString(interp, msgObj, ": ", 2);
        Jim_AppendString(interp, msgObj, lpBuffer, -1);
    }
    LocalFree((HLOCAL)lpBuffer);
    return msgObj;
}

/* ----------------------------------------------------------------------
 * Information recorded for API calls that we can call.
 */

typedef struct Win32ApiDeclaration {
    HMODULE module;    /* Reference counted handle to the library */
    LPSTR  symbol;     /* The function name as used in the library */
    FARPROC lpfn;      /* Generic function pointer for the API */
    LPSTR   rtype;     /* The return type */
    Jim_Obj *typeList; /* List of parameters as type name type ... */
} Win32ApiDeclaration;


/* ----------------------------------------------------------------------
 * Type hash table
 *
 * This is generic string:thing* hash.
 * Keys are a typename. Values are pointers Win32ApiTypeInfo structs
 */

typedef struct Win32ApiTypeInfo {
    size_t type_size;   /* The size of the type in bytes (not used yet) */
    char   type_spec[]; /* packing details for the type */
} Win32ApiTypeInfo;

static unsigned int Win32ApiTypeInfoHashTableHash(const void *key)
{
    /*return Jim_DjbHashFunction(key, strlen(key));*/
    unsigned int h = 5381;
	size_t len = strlen(key);
    while(len--)
        h = (h + (h << 5)) ^ *((const char *)key)++;
    return h;
}

static const void *Win32ApiTypeInfoHashTableCopyKey(void *privdata, const void *key)
{
    JIM_NOTUSED(privdata);
    return Jim_StrDup(key);
}

static int Win32ApiTypeInfoHashTableCompare(void *privdata, const void *key1, const void *key2)
{
    JIM_NOTUSED(privdata);
    return strcmp(key1, key2) == 0;
}

static void Win32ApiTypeInfoHashTableDestroyKey(void *privdata, const void *key)
{
    JIM_NOTUSED(privdata);
    Jim_Free((void*)key);
}

static void Win32ApiTypeInfoHashTableDestroyValue(void *interp, void *val)
{
	Win32ApiTypeInfo *entryPtr = (Win32ApiTypeInfo *)val;
	JIM_NOTUSED(interp);
	Jim_Free((void*)entryPtr);
}

static Jim_HashTableType Win32ApiTypeHashTableType = {
    Win32ApiTypeInfoHashTableHash,         /* hash function */
    Win32ApiTypeInfoHashTableCopyKey,      /* key dup */
    NULL,                                  /* val dup */
    Win32ApiTypeInfoHashTableCompare,      /* key compare */
    Win32ApiTypeInfoHashTableDestroyKey,   /* key destructor */
    Win32ApiTypeInfoHashTableDestroyValue  /* val destructor */
};

/* ----------------------------------------------------------------------
 * The typedef object type is a caching internal rep to avoid repeat 
 * lookups into the hash table.
 */

Jim_ObjType typedefObjType = {
    "win32.typedef",
    NULL,
    NULL,
    NULL,
    JIM_TYPE_REFERENCES,
};

int
TypedefSetFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
	if (objPtr->typePtr != &typedefObjType) {
		const char *tname = Jim_GetString(objPtr, NULL);
		Jim_HashTable *hashPtr = Jim_GetAssocData(interp, win32api_type_hash);
		Jim_HashEntry *entryPtr = Jim_FindHashEntry(hashPtr, tname);
		if (entryPtr == NULL) {
            Jim_Obj *errObj = Jim_NewEmptyStringObj(interp);
			Jim_AppendStrings(interp, errObj, "type \"", tname, "\" is not defined", NULL);
            Jim_SetResult(interp, errObj);
			return JIM_ERR;
		}

		Jim_FreeIntRep(interp, objPtr);
		objPtr->internalRep.ptr = entryPtr;
		objPtr->typePtr = &typedefObjType;
	}
	return JIM_OK;
}

/* ---------------------------------------------------------------------- */

/* Clean up the package's interp associated data */
static void
Win32_PackageDeleteProc(Jim_Interp *interp, void *clientData)
{
    Jim_HashTable *hashPtr = (Jim_HashTable *)clientData;
    Jim_FreeHashTable(hashPtr);
    Jim_Free(clientData);
}

/* Update and introspect the type table
 *
 * Perls pack supports...
   a char binary data (null padded)   u uuencoded string
   A cha binary data (space padded)   U unicode char
   Z asciiz
   c char    s int16    i int    l int32    n be u_int16 v le uint16  q int64   f float  p pointer (null term)
   C uchar   S u_int16  I u_int  L uint_32  N be u_int32 V le uint32  Q u_int64 d double P block pointer
*/
static int
Win32_Typedef(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    enum {OPT_NULL, OPT_TYPE_NAMES, OPT_TYPE_GET, OPT_TYPE_SET};
    Jim_HashTable *hashPtr = (Jim_HashTable *)Jim_CmdPrivData(interp);
    Win32ApiTypeInfo *typePtr;
    const char *name = NULL, *spec = NULL;
    int name_len, spec_len, n, r = JIM_OK;
    struct def { char c; unsigned char n; } defs[] = { 
        'c', 1, 'C', 1, 's', 2, 'S', 2, 'i', 4, 'I', 4,
        'l', 4, 'L', 4, 'n', 4, 'N', 4, 'v', 4, 'V', 4,
        'q', 8, 'Q', 8, 'f', 4, 'd', 8, 'p', 4, 'P', 4,
        'a', 1, 'A', 1, 'Z', 0, 'U', 2,  0, 0};
    struct def *sp;
    const char *p;
    size_t size, m = 0;

    if (objc > 3) {
        Jim_WrongNumArgs(interp, 1, objv, "typename ?pack_spec?");
        return JIM_ERR;
    }

    switch (objc) {
        /* Assign a new type. We validate the pack format and calculate the type size at the same time */
        case OPT_TYPE_SET: {
            name = Jim_GetString(objv[1], &name_len);
            spec = Jim_GetString(objv[2], &spec_len);
            
            size = 0;
            p = spec;
            while (p && *p) {
                for (sp = defs; sp->c != 0; sp++) {
                    if (sp->c == *p)
                        break;
                }
                if (sp->c == 0) {
                    Jim_Obj *errObj = Jim_NewStringObj(interp, "invalid pack character \"0\"", -1);
                    errObj->bytes[24] = *p; /* NOTE: if you change the text in the above string this must be fixed */
                    Jim_AppendStrings(interp, errObj, " while defining type \"", name, "\"", NULL);
                    Jim_SetResult(interp, errObj);
                    return JIM_ERR;
                }
                if (isdigit(*(p+1))) {
                    char *pe = NULL;
                    m = strtol(p+1, &pe, 0);
                    p = pe;
                } else {
                    m = 1;
                    p++;
                }
                size += (sp->n * m);
            }

            n = name_len + spec_len + 1;
            typePtr = (Win32ApiTypeInfo *)Jim_Alloc(n);
            typePtr->type_size = size;
            strcpy(typePtr->type_spec, spec);
            Jim_AddHashEntry(hashPtr, name, typePtr);
            Jim_SetResultString(interp, name, name_len);
            break;
        }
        case OPT_TYPE_GET: {
            Jim_HashEntry *entryPtr;
            name = Jim_GetString(objv[1], &name_len);
            if ((entryPtr = Jim_FindHashEntry(hashPtr, name)) == NULL) {
                Jim_Obj *resObj = Jim_NewEmptyStringObj(interp);
                Jim_AppendStrings(interp, resObj, "type \"", name, "\" not found", NULL);
                Jim_SetResult(interp, resObj);
                r = JIM_ERR;
            } else {
                typePtr = entryPtr->val;
                Jim_SetResultString(interp, typePtr->type_spec, -1);
            }
            break;
        }
        case OPT_TYPE_NAMES: {
            Jim_Obj *listPtr;
            Jim_HashEntry *entryPtr;
            Jim_HashTableIterator *it = Jim_GetHashTableIterator(hashPtr);
            listPtr = Jim_NewListObj(interp, NULL, 0);
            while ((entryPtr = Jim_NextHashEntry(it)) != NULL) {
                Jim_ListAppendElement(interp, listPtr, Jim_NewStringObj(interp, entryPtr->key, -1));
            }
            Jim_SetResult(interp, listPtr);
            break;
        }
    }
    return r;
}

/* ---------------------------------------------------------------------- */


static void
Win32_ApiCleanup(Jim_Interp *interp, void *clientData)
{
    Win32ApiDeclaration *declPtr = (Win32ApiDeclaration *)clientData;
    FreeLibrary(declPtr->module);
    Jim_Free((void *)declPtr->symbol);
    Jim_Free((void *)declPtr->rtype);
    Jim_DecrRefCount(interp, declPtr->typeList);
}

static int
Win32_ApiHandler(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    Win32ApiDeclaration *declPtr = (Win32ApiDeclaration *)Jim_CmdPrivData(interp);
    Jim_HashTable *hashPtr;
    int nargs = 0, n, np, r;
    long lval;
    double dblval;
    float fltval;
    struct {
        unsigned long params[16];
    } param;

    Jim_ListLength(interp, declPtr->typeList, &nargs);
    if (objc-1 != nargs/2) {
        int tlen = 0;
        const char *types = Jim_GetString(declPtr->typeList, &tlen);
        char *sz = (char *)_alloca(tlen + 3);
        sprintf(sz, "(%s)", types);
        Jim_WrongNumArgs(interp, 1, objv, sz);
        return JIM_ERR;
    }

    ZeroMemory(&param, sizeof(param));
    hashPtr = Jim_GetAssocData(interp, win32api_type_hash);

    for (n = 1, np = 0; n < objc; n++) {
        Jim_HashEntry *entryPtr;
        Jim_Obj *tnameObj, *pnameObj;
        const char *tname, *pname;

        r = Jim_ListIndex(interp, declPtr->typeList, (np*2), &tnameObj, JIM_ERRMSG);
        tname = Jim_GetString(tnameObj, NULL);
        r = Jim_ListIndex(interp, declPtr->typeList, (np*2)+1, &pnameObj, JIM_ERRMSG);
        pname = Jim_GetString(pnameObj, NULL);

        entryPtr = Jim_FindHashEntry(hashPtr, tname);
        switch (((Win32ApiTypeInfo *)entryPtr->val)->type_spec[0]) {
            case 'q': case 'Q':
                Jim_GetWide(interp, objv[n], (jim_wide *)&param.params[np]);
                np += 2;
                break;
            case 's': case 'v':
                Jim_GetLong(interp, objv[n], &lval);
                param.params[np] = (unsigned short)lval;
                np++;
                break;
            case 'd':
                Jim_GetDouble(interp, objv[n], &dblval);
                memcpy(&param.params[np], &dblval, 8);
                np+=2;
                break;
            case 'f':
                Jim_GetDouble(interp, objv[n], &dblval);
                fltval = (float)dblval;
                memcpy(&param.params[np], &fltval, 4);
                np+=2;
                break;
            case 'l': case 'L': case 'i': case 'I': case 'V':
            default:
                Jim_GetLong(interp, objv[n], &param.params[np]);
                np++;
                break;
        }
    }

    if (nargs == 0)
        r = declPtr->lpfn();
    else
        r = declPtr->lpfn(param);

    Jim_SetResult(interp, Jim_NewIntObj(interp, r));
    return JIM_OK;
}

static int
Win32_Declare(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    Jim_HashTable *hashPtr = (Jim_HashTable *)Jim_CmdPrivData(interp); /* type hash map */
    Win32ApiDeclaration *declPtr;
    HMODULE hLib = NULL;
    FARPROC lpfn = NULL;
    const char *lib, *rtype, *name;
    Jim_Obj *cmdObj = NULL;
    
    if (objc != 5) {
        Jim_WrongNumArgs(interp, 1, objv, "lib return_type name typelist");
        return JIM_ERR;
    }

    lib = Jim_GetString(objv[1], NULL);
    rtype = Jim_GetString(objv[2], NULL);
    name = Jim_GetString(objv[3], NULL);
    hLib = LoadLibraryA(lib);
    if (hLib == NULL) {
        Jim_SetResultString(interp, "failed to load library", -1);
        return JIM_ERR;
    }

    if ((lpfn = GetProcAddress(hLib, name)) == NULL) {
        Jim_Obj *errObj = Jim_NewEmptyStringObj(interp);
        FreeLibrary(hLib);
        Jim_AppendStrings(interp, errObj, "could not load \"", name, "\" from \"", lib, "\"", NULL);
        Jim_SetResult(interp, errObj);
        return JIM_ERR;
    }

    declPtr = (Win32ApiDeclaration *)Jim_Alloc(sizeof(Win32ApiDeclaration));
    declPtr->module = hLib;
    declPtr->lpfn = lpfn;
    declPtr->symbol = Jim_StrDup(name);
    declPtr->rtype = Jim_StrDup(rtype);
    declPtr->typeList = Jim_DuplicateObj(interp, objv[4]);
    Jim_IncrRefCount(declPtr->typeList);
    
    cmdObj = Jim_NewStringObj(interp, "", strlen(name) + 9);
    sprintf(cmdObj->bytes, "win32api.%s", name);
    Jim_CreateCommand(interp, cmdObj->bytes, Win32_ApiHandler, declPtr, Win32_ApiCleanup);
    Jim_SetResult(interp, cmdObj);
    return JIM_OK;
}

/* ---------------------------------------------------------------------- */

int
Jim_OnLoad(Jim_Interp *interp)
{
    Jim_HashTable *hashPtr;
    Jim_InitExtension(interp);
    if (Jim_PackageProvide(interp, "win32api", "1.0", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;

    hashPtr = (Jim_HashTable *)Jim_Alloc(sizeof(Jim_HashTable));
    Jim_InitHashTable(hashPtr, &Win32ApiTypeHashTableType, NULL);
    Jim_SetAssocData(interp, win32api_type_hash, Win32_PackageDeleteProc, hashPtr);

    Jim_CreateCommand(interp, "win32.declare", Win32_Declare, hashPtr, NULL);
    Jim_CreateCommand(interp, "win32.typedef", Win32_Typedef, hashPtr, NULL);

    return JIM_OK;
}

/* ----------------------------------------------------------------------
 * Local variables:
 * indent-tabs-mode: nil
 * End:
 */
