/*
 * Jim - Sqlite bindings
 *
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
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
 */

#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

#include <jim.h>

static void JimSqliteDelProc(Jim_Interp *interp, void *privData)
{
    sqlite3 *db = privData;

    JIM_NOTUSED(interp);

    sqlite3_close(db);
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
                spec[0] = *fmt;
                spec[1] = '\0';
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
    sqlite3 *db = Jim_CmdPrivData(interp);
    int option;
    static const char * const options[] = {
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
        Jim_DeleteCommand(interp, argv[0]);
        return JIM_OK;
    }
    else if (option == OPT_QUERY) {
        /* QUERY */
        Jim_Obj *objPtr, *rowsListPtr;
        sqlite3_stmt *stmt;
        const char *query, *tail;
        int columns, rows, len;
        int retcode = JIM_ERR;
        Jim_Obj *nullStrObj;

        if (argc >= 4 && Jim_CompareStringImmediate(interp, argv[2], "-null")) {
            nullStrObj = argv[3];
            argv += 2;
            argc -= 2;
        }
        else {
            nullStrObj = Jim_NewEmptyStringObj(interp);
        }
        Jim_IncrRefCount(nullStrObj);
        if (argc < 3) {
            Jim_WrongNumArgs(interp, 2, argv, "?args?");
            goto err;
        }
        objPtr = JimSqliteFormatQuery(interp, argv[2], argc - 3, argv + 3);
        if (objPtr == NULL) {
            return JIM_ERR;
        }
        query = Jim_GetString(objPtr, &len);
        Jim_IncrRefCount(objPtr);
        /* Compile the query into VM code */
        if (sqlite3_prepare_v2(db, query, len, &stmt, &tail) != SQLITE_OK) {
            Jim_DecrRefCount(interp, objPtr);
            Jim_SetResultString(interp, sqlite3_errmsg(db), -1);
            goto err;
        }
        Jim_DecrRefCount(interp, objPtr);       /* query no longer needed. */
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
                        vObj = nullStrObj;
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
                            sqlite3_column_blob(stmt, i), sqlite3_column_bytes(stmt, i));
                        break;
                }
                Jim_ListAppendElement(interp, objPtr, vObj);
            }
            Jim_ListAppendElement(interp, rowsListPtr, objPtr);
            rows++;
        }
        /* Finalize */
        if (sqlite3_finalize(stmt) != SQLITE_OK) {
            Jim_SetResultString(interp, sqlite3_errmsg(db), -1);
        }
        else {
            Jim_SetResult(interp, rowsListPtr);
            retcode = JIM_OK;
        }
        Jim_DecrRefCount(interp, rowsListPtr);
err:
        Jim_DecrRefCount(interp, nullStrObj);

        return retcode;
    }
    else if (option == OPT_LASTID) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_SetResult(interp, Jim_NewIntObj(interp, sqlite3_last_insert_rowid(db)));
        return JIM_OK;
    }
    else if (option == OPT_CHANGES) {
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_SetResult(interp, Jim_NewIntObj(interp, sqlite3_changes(db)));
        return JIM_OK;
    }
    return JIM_OK;
}

static int JimSqliteOpenCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    sqlite3 *db;
    char buf[60];
    int r;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "dbname");
        return JIM_ERR;
    }
    r = sqlite3_open(Jim_String(argv[1]), &db);
    if (r != SQLITE_OK) {
        Jim_SetResultString(interp, sqlite3_errmsg(db), -1);
        sqlite3_close(db);
        return JIM_ERR;
    }
    /* Create the file command */
    snprintf(buf, sizeof(buf), "sqlite.handle%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimSqliteHandlerCommand, db, JimSqliteDelProc);

    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return JIM_OK;
}

int Jim_sqlite3Init(Jim_Interp *interp)
{
    Jim_PackageProvideCheck(interp, "sqlite3");
    Jim_CreateCommand(interp, "sqlite3.open", JimSqliteOpenCommand, NULL, NULL);
    return JIM_OK;
}
