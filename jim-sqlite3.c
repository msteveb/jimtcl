/* Jim - Sqlite bindings
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 *
 * $Id: jim-sqlite3.c,v 1.1 2007/01/31 00:53:32 patthoyts Exp $
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
#include <sqlite3.h>

#define JIM_EXTENSION
#include "jim.h"

#define SQLITE_CMD_LEN 128

typedef struct JimSqliteHandle {
    sqlite3 *db;
} JimSqliteHandle;

static void JimSqliteDelProc(Jim_Interp *interp, void *privData)
{
    JimSqliteHandle *sh = privData;
    JIM_NOTUSED(interp);

    sqlite3_close(sh->db);
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
    newLen = len+c;
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
            fmt++; fmtLen--;
        }
        Jim_AppendString(interp, resObjPtr, p, fmt-p);
        if (fmtLen == 0)
            break;
        fmt++; fmtLen--; /* skip '%' */
        if (*fmt != '%') {
            if (objc == 0) {
                Jim_FreeNewObj(interp, resObjPtr);
                Jim_SetResultString(interp,
                        "not enough arguments for all format specifiers", -1);
                return NULL;
            } else {
                objc--;
            }
        }
        switch(*fmt) {
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
            Jim_AppendString(interp, resObjPtr, "%" , 1);
            break;
        default:
            spec[1] = *fmt; spec[2] = '\0';
            Jim_FreeNewObj(interp, resObjPtr);
            Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
            Jim_AppendStrings(interp, Jim_GetResult(interp),
                    "bad field specifier \"",  spec, "\", ",
                    "only %%s and %%%% are validd", NULL);
            return NULL;
        }
        fmt++;
        fmtLen--;
    }
    return resObjPtr;
}

/* Calls to [sqlite.open] create commands that are implemented by this
 * C command. */
static int JimSqliteHandlerCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    JimSqliteHandle *sh = Jim_CmdPrivData(interp);
    int option;
    const char *options[] = {
        "close", "query", "lastid", "changes", NULL
    };
    enum {OPT_CLOSE, OPT_QUERY, OPT_LASTID, OPT_CHANGES};

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], options, &option, "Sqlite method",
                JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    /* CLOSE */
    if (option == OPT_CLOSE) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_DeleteCommand(interp, Jim_GetString(argv[0], NULL));
        return JIM_OK;
    } else if (option == OPT_QUERY) {
    /* QUERY */
        Jim_Obj *objPtr, *rowsListPtr;
        sqlite3_stmt *stmt;
        const char *query, *tail;
        int columns, rows, len;
        char *nullstr;

        if (argc >= 4 && Jim_CompareStringImmediate(interp, argv[2], "-null")) {
            nullstr = Jim_StrDup(Jim_GetString(argv[3], NULL));
            argv += 2;
            argc -= 2;
        } else {
            nullstr = Jim_StrDup("");
        }
        if (argc < 3) {
            Jim_WrongNumArgs(interp, 2, argv, "query ?args?");
            Jim_Free(nullstr);
            return JIM_ERR;
        }
        objPtr = JimSqliteFormatQuery(interp, argv[2], argc-3, argv+3);
        if (objPtr == NULL) {
            Jim_Free(nullstr);
            return JIM_ERR;
        }
        query = Jim_GetString(objPtr, &len);
        Jim_IncrRefCount(objPtr);
        /* Compile the query into VM code */
        if (sqlite3_prepare(sh->db, query, len, &stmt, &tail) != SQLITE_OK) {
            Jim_DecrRefCount(interp, objPtr);
            Jim_SetResultString(interp, sqlite3_errmsg(sh->db), -1);
            Jim_Free(nullstr);
            return JIM_ERR;
        }
        Jim_DecrRefCount(interp, objPtr); /* query no longer needed. */
        /* Build a list of rows (that are lists in turn) */
        rowsListPtr = Jim_NewListObj(interp, NULL, 0);
        Jim_IncrRefCount(rowsListPtr);
        rows = 0;
        columns = sqlite3_column_count(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int i;

            objPtr = Jim_NewListObj(interp, NULL, 0);
            for (i = 0; i < columns; i++) {
		Jim_Obj *vObj = NULL;
                Jim_ListAppendElement(interp, objPtr,
		        Jim_NewStringObj(interp, sqlite3_column_name(stmt, i), -1));
		switch (sqlite3_column_type(stmt, i)) {
		    case SQLITE_NULL:
			vObj = Jim_NewStringObj(interp, nullstr, -1);
			break;
		    case SQLITE_INTEGER:
			vObj = Jim_NewIntObj(interp, sqlite3_column_int(stmt, i));
			break;
		    case SQLITE_FLOAT:
			vObj = Jim_NewDoubleObj(interp, sqlite3_column_double(stmt, i));
			break;
		    case SQLITE_TEXT:
		    case SQLITE_BLOB:
			vObj = Jim_NewStringObj(interp, 
			    (const unsigned char *)sqlite3_column_blob(stmt, i),
			    sqlite3_column_bytes(stmt, i));
			break;
		}
                Jim_ListAppendElement(interp, objPtr, vObj);
            }
            Jim_ListAppendElement(interp, rowsListPtr, objPtr);
            rows++;
        }
        /* Finalize */
        Jim_Free(nullstr);
        if (sqlite3_finalize(stmt) != SQLITE_OK) {
            Jim_DecrRefCount(interp, rowsListPtr);
            Jim_SetResultString(interp, sqlite3_errmsg(sh->db), -1);
            return JIM_ERR;
        }
        Jim_SetResult(interp, rowsListPtr);
        Jim_DecrRefCount(interp, rowsListPtr);
    } else if (option == OPT_LASTID) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_SetResult(interp, Jim_NewIntObj(interp,
                    sqlite3_last_insert_rowid(sh->db)));
        return JIM_OK;
    } else if (option == OPT_CHANGES) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_SetResult(interp, Jim_NewIntObj(interp,
                    sqlite3_changes(sh->db)));
        return JIM_OK;
    }
    return JIM_OK;
}

static int JimSqliteOpenCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    sqlite3 *db;
    JimSqliteHandle *sh;
    char buf[SQLITE_CMD_LEN];
    Jim_Obj *objPtr;
    long dbId;
    int r;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "dbname");
        return JIM_ERR;
    }
    r = sqlite3_open(Jim_GetString(argv[1], NULL), &db);
    if (r != SQLITE_OK) {
        Jim_SetResultString(interp, sqlite3_errmsg(db), -1);
        sqlite3_close(db);
        return JIM_ERR;
    }
    /* Get the next file id */
    if (Jim_EvalGlobal(interp,
            "if {[catch {incr sqlite.dbId}]} {set sqlite.dbId 0}") != JIM_OK)
        return JIM_ERR;
    objPtr = Jim_GetVariableStr(interp, "sqlite.dbId", JIM_ERRMSG);
    if (objPtr == NULL) return JIM_ERR;
    if (Jim_GetLong(interp, objPtr, &dbId) != JIM_OK) return JIM_ERR;

    /* Create the file command */
    sh = Jim_Alloc(sizeof(*sh));
    sh->db = db;
    sprintf(buf, "sqlite.handle%ld", dbId);
    Jim_CreateCommand(interp, buf, JimSqliteHandlerCommand, sh,
            JimSqliteDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

DLLEXPORT int
Jim_OnLoad(Jim_Interp *interp)
{
    Jim_InitExtension(interp);
    if (Jim_PackageProvide(interp, "sqlite3", "1.0", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    Jim_CreateCommand(interp, "sqlite3.open", JimSqliteOpenCommand, NULL, NULL);
    return JIM_OK;
}
