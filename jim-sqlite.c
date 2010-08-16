
/* Jim - Sqlite bindings
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
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

#include <stdio.h>
#include <string.h>
#include <sqlite.h>

#define JIM_EXTENSION
#include "jim.h"

#define SQLITE_CMD_LEN 128

typedef struct JimSqliteHandle
{
    sqlite *db;
} JimSqliteHandle;

static void JimSqliteDelProc(Jim_Interp *interp, void *privData)
{
    JimSqliteHandle *sh = privData;

    JIM_NOTUSED(interp);

    sqlite_close(sh->db);
    Jim_Free(sh);
}

static char *JimSqliteQuoteString(const char *str, int len, int *newLenPtr)
{
    int i, newLen, c = 0;
    const char *s;
    char *d, *buf;

    for (i = 0; i < len; i++)
        if (str[i] == '\'')
            c++;
    newLen = len + c;
    s = str;
    d = buf = Jim_Alloc(newLen);
    while (len--) {
        if (*s == '\'')
            *d++ = '\'';
        *d++ = *s++;
    }
    *newLenPtr = newLen;
    return buf;
}

static Jim_Obj *JimSqliteFormatQuery(Jim_Interp *interp, Jim_Obj *fmtObjPtr,
    int objc, Jim_Obj *const *objv)
{
    const char *fmt;
    int fmtLen;
    Jim_Obj *resObjPtr;

    fmt = Jim_GetString(fmtObjPtr, &fmtLen);
    resObjPtr = Jim_NewStringObj(interp, "", 0);
    while (fmtLen) {
        const char *p = fmt;
        char spec[2];

        while (*fmt != '%' && fmtLen) {
            fmt++;
            fmtLen--;
        }
        Jim_AppendString(interp, resObjPtr, p, fmt - p);
        if (fmtLen == 0)
            break;
        fmt++;
        fmtLen--;               /* skip '%' */
        if (*fmt != '%') {
            if (objc == 0) {
                Jim_FreeNewObj(interp, resObjPtr);
                Jim_SetResultString(interp, "not enough arguments for all format specifiers", -1);
                return NULL;
            }
            else {
                objc--;
            }
        }
        switch (*fmt) {
            case 's':
                {
                    const char *str;
                    char *quoted;
                    int len, newLen;

                    str = Jim_GetString(objv[0], &len);
                    quoted = JimSqliteQuoteString(str, len, &newLen);
                    Jim_AppendString(interp, resObjPtr, quoted, newLen);
                    Jim_Free(quoted);
                }
                objv++;
                break;
            case '%':
                Jim_AppendString(interp, resObjPtr, "%", 1);
                break;
            default:
                spec[1] = *fmt;
                spec[2] = '\0';
                Jim_FreeNewObj(interp, resObjPtr);
                Jim_SetResultFormatted(interp,
                    "bad field specifier \"%s\", only %%s and %%%% are valid", spec);
                return NULL;
        }
        fmt++;
        fmtLen--;
    }
    return resObjPtr;
}

/* Calls to [sqlite.open] create commands that are implemented by this
 * C command. */
static int JimSqliteHandlerCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSqliteHandle *sh = Jim_CmdPrivData(interp);
    int option;
    const char *options[] = {
        "close", "query", "lastid", "changes", NULL
    };
    enum
    { OPT_CLOSE, OPT_QUERY, OPT_LASTID, OPT_CHANGES };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], options, &option, "Sqlite method", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    /* CLOSE */
    if (option == OPT_CLOSE) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_DeleteCommand(interp, Jim_GetString(argv[0], NULL));
        return JIM_OK;
    }
    else if (option == OPT_QUERY) {
        /* QUERY */
        Jim_Obj *objPtr, *rowsListPtr;
        sqlite_vm *vm;
        char *errMsg;
        const char *query, *tail, **values, **names;
        int columns, rows;
        char *nullstr;

        if (argc >= 4 && Jim_CompareStringImmediate(interp, argv[2], "-null")) {
            nullstr = Jim_StrDup(Jim_GetString(argv[3], NULL));
            argv += 2;
            argc -= 2;
        }
        else {
            nullstr = Jim_StrDup("");
        }
        if (argc < 3) {
            Jim_WrongNumArgs(interp, 2, argv, "query ?args?");
            Jim_Free(nullstr);
            return JIM_ERR;
        }
        objPtr = JimSqliteFormatQuery(interp, argv[2], argc - 3, argv + 3);
        if (objPtr == NULL) {
            Jim_Free(nullstr);
            return JIM_ERR;
        }
        query = Jim_GetString(objPtr, NULL);
        Jim_IncrRefCount(objPtr);
        /* Compile the query into VM code */
        if (sqlite_compile(sh->db, query, &tail, &vm, &errMsg) != SQLITE_OK) {
            Jim_DecrRefCount(interp, objPtr);
            Jim_SetResultString(interp, errMsg, -1);
            sqlite_freemem(errMsg);
            Jim_Free(nullstr);
            return JIM_ERR;
        }
        Jim_DecrRefCount(interp, objPtr);       /* query no longer needed. */
        /* Build a list of rows (that are lists in turn) */
        rowsListPtr = Jim_NewListObj(interp, NULL, 0);
        Jim_IncrRefCount(rowsListPtr);
        rows = 0;
        while (sqlite_step(vm, &columns, &values, &names) == SQLITE_ROW) {
            int i;

            objPtr = Jim_NewListObj(interp, NULL, 0);
            for (i = 0; i < columns; i++) {
                Jim_ListAppendElement(interp, objPtr, Jim_NewStringObj(interp, names[i], -1));
                Jim_ListAppendElement(interp, objPtr,
                    Jim_NewStringObj(interp, values[i] != NULL ? values[i] : nullstr, -1));
            }
            Jim_ListAppendElement(interp, rowsListPtr, objPtr);
            rows++;
        }
        /* Finalize */
        Jim_Free(nullstr);
        if (sqlite_finalize(vm, &errMsg) != SQLITE_OK) {
            Jim_DecrRefCount(interp, rowsListPtr);
            Jim_SetResultString(interp, errMsg, -1);
            sqlite_freemem(errMsg);
            return JIM_ERR;
        }
        Jim_SetResult(interp, rowsListPtr);
        Jim_DecrRefCount(interp, rowsListPtr);
    }
    else if (option == OPT_LASTID) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_SetResult(interp, Jim_NewIntObj(interp, sqlite_last_insert_rowid(sh->db)));
        return JIM_OK;
    }
    else if (option == OPT_CHANGES) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_SetResult(interp, Jim_NewIntObj(interp, sqlite_changes(sh->db)));
        return JIM_OK;
    }
    return JIM_OK;
}

static int JimSqliteOpenCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    sqlite *db;
    JimSqliteHandle *sh;
    char buf[SQLITE_CMD_LEN], *errMsg;
    Jim_Obj *objPtr;
    long dbId;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "dbname");
        return JIM_ERR;
    }
    db = sqlite_open(Jim_GetString(argv[1], NULL), 0, &errMsg);
    if (db == NULL) {
        Jim_SetResultString(interp, errMsg, -1);
        sqlite_freemem(errMsg);
        return JIM_ERR;
    }
    /* Get the next file id */
    if (Jim_EvalGlobal(interp, "if {[catch {incr sqlite.dbId}]} {set sqlite.dbId 0}") != JIM_OK)
        return JIM_ERR;
    objPtr = Jim_GetVariableStr(interp, "sqlite.dbId", JIM_ERRMSG);
    if (objPtr == NULL)
        return JIM_ERR;
    if (Jim_GetLong(interp, objPtr, &dbId) != JIM_OK)
        return JIM_ERR;

    /* Create the file command */
    sh = Jim_Alloc(sizeof(*sh));
    sh->db = db;
    sprintf(buf, "sqlite.handle%ld", dbId);
    Jim_CreateCommand(interp, buf, JimSqliteHandlerCommand, sh, JimSqliteDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

int Jim_sqliteInit(Jim_Interp *interp)
{
    Jim_CreateCommand(interp, "sqlite.open", JimSqliteOpenCommand, NULL, NULL);
    return JIM_OK;
}
