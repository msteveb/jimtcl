/*
 * Implements the tcl::prefix command for Jim Tcl
 *
 * (c) 2011 Steve Bennett <steveb@workware.net.au>
 *
 * See LICENSE for license details.
 */

#include <jim.h>
#include "utf8.h"

/**
 * Returns the common initial length of the two strings.
 */
static int JimStringCommonLength(const char *str1, int charlen1, const char *str2, int charlen2)
{
    int maxlen = 0;
    while (charlen1-- && charlen2--) {
        int c1;
        int c2;
        str1 += utf8_tounicode(str1, &c1);
        str2 += utf8_tounicode(str2, &c2);
        if (c1 != c2) {
            break;
        }
        maxlen++;
    }
    return maxlen;
}

/*
 * Like Jim_StringCompareObj() except only matches as much as the length of firstObjPtr.
 * So "abc" matches "abcdef" but "abcdef" does not match "abc".
 */
int JimStringComparePrefix(Jim_Interp *interp, Jim_Obj *firstObjPtr, Jim_Obj *secondObjPtr)
{
    /* We do this the easy way by creating a (possibly) shorter version of secondObjPtr */
    int l1 = Jim_Utf8Length(interp, firstObjPtr);
    const char *s2 = Jim_String(secondObjPtr);
    int l2 = Jim_Utf8Length(interp, secondObjPtr);
    Jim_Obj *objPtr;
    int ret;

    if (l2 > l1) {
        objPtr = Jim_NewStringObjUtf8(interp, s2, l1);
    }
    else {
        objPtr = secondObjPtr;
    }
    Jim_IncrRefCount(objPtr);

    ret = Jim_StringCompareObj(interp, firstObjPtr, objPtr, 0);
    Jim_DecrRefCount(interp, objPtr);
    return ret;
}

/* [tcl::prefix]
 */
static int Jim_TclPrefixCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    Jim_Obj *stringObj;
    int option;
    static const char * const options[] = { "match", "all", "longest", NULL };
    enum { OPT_MATCH, OPT_ALL, OPT_LONGEST };

    if (Jim_GetEnum(interp, argv[1], options, &option, NULL, JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK)
        return Jim_CheckShowCommands(interp, argv[1], options);

    switch (option) {
        case OPT_MATCH:{
            int i;
            int ret;
            int tablesize;
            const char **table;
            Jim_Obj *tableObj;
            Jim_Obj *errorObj = NULL;
            const char *message = "option";
            static const char * const matchoptions[] = { "-error", "-exact", "-message", NULL };
            enum { OPT_MATCH_ERROR, OPT_MATCH_EXACT, OPT_MATCH_MESSAGE };
            int flags = JIM_ERRMSG | JIM_ENUM_ABBREV;

            if (argc < 4) {
                Jim_WrongNumArgs(interp, 2, argv, "?options? table string");
                return JIM_ERR;
            }
            tableObj = argv[argc - 2];
            stringObj = argv[argc - 1];
            argc -= 2;
            for (i = 2; i < argc; i++) {
                int matchoption;
                if (Jim_GetEnum(interp, argv[i], matchoptions, &matchoption, "option", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK)
                    return JIM_ERR;
                switch (matchoption) {
                    case OPT_MATCH_EXACT:
                        flags &= ~JIM_ENUM_ABBREV;
                        break;

                    case OPT_MATCH_ERROR:
                        if (++i == argc) {
                            Jim_SetResultString(interp, "missing error options", -1);
                            return JIM_ERR;
                        }
                        errorObj = argv[i];
                        if (Jim_Length(errorObj) % 2) {
                            Jim_SetResultString(interp, "error options must have an even number of elements", -1);
                            return JIM_ERR;
                        }
                        break;

                    case OPT_MATCH_MESSAGE:
                        if (++i == argc) {
                            Jim_SetResultString(interp, "missing message", -1);
                            return JIM_ERR;
                        }
                        message = Jim_String(argv[i]);
                        break;
                }
            }
            /* Do the match */
            tablesize = Jim_ListLength(interp, tableObj);
            table = Jim_Alloc((tablesize + 1) * sizeof(*table));
            for (i = 0; i < tablesize; i++) {
                Jim_ListIndex(interp, tableObj, i, &objPtr, JIM_NONE);
                table[i] = Jim_String(objPtr);
            }
            table[i] = NULL;

            ret = Jim_GetEnum(interp, stringObj, table, &i, message, flags);
            Jim_Free(table);
            if (ret == JIM_OK) {
                Jim_ListIndex(interp, tableObj, i, &objPtr, JIM_NONE);
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }
            if (tablesize == 0) {
                Jim_SetResultFormatted(interp, "bad %s \"%#s\": no valid options", message, stringObj);
                return JIM_ERR;
            }
            if (errorObj) {
                if (Jim_Length(errorObj) == 0) {
                    Jim_SetEmptyResult(interp);
                    return JIM_OK;
                }
                /* Do this the easy way. Build a list to evaluate */
                objPtr = Jim_NewStringObj(interp, "return -level 0 -code error", -1);
                Jim_ListAppendList(interp, objPtr, errorObj);
                Jim_ListAppendElement(interp, objPtr, Jim_GetResult(interp));
                return Jim_EvalObjList(interp, objPtr);
            }
            return JIM_ERR;
        }

        case OPT_ALL:
            if (argc != 4) {
                Jim_WrongNumArgs(interp, 2, argv, "table string");
                return JIM_ERR;
            }
            else {
                int i;
                int listlen = Jim_ListLength(interp, argv[2]);
                objPtr = Jim_NewListObj(interp, NULL, 0);
                for (i = 0; i < listlen; i++) {
                    Jim_Obj *valObj = Jim_ListGetIndex(interp, argv[2], i);
                    if (JimStringComparePrefix(interp, argv[3], valObj) == 0) {
                        Jim_ListAppendElement(interp, objPtr, valObj);
                    }
                }
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }

        case OPT_LONGEST:
            if (argc != 4) {
                Jim_WrongNumArgs(interp, 2, argv, "table string");
                return JIM_ERR;
            }
            else if (Jim_ListLength(interp, argv[2])) {
                const char *longeststr = NULL;
                int longestlen = 0;
                int i;
                int listlen = Jim_ListLength(interp, argv[2]);

                stringObj = argv[3];

                for (i = 0; i < listlen; i++) {
                    Jim_Obj *valObj = Jim_ListGetIndex(interp, argv[2], i);

                    if (JimStringComparePrefix(interp, stringObj, valObj)) {
                        /* Does not begin with 'string' */
                        continue;
                    }

                    if (longeststr == NULL) {
                        longestlen = Jim_Utf8Length(interp, valObj);
                        longeststr = Jim_String(valObj);
                    }
                    else {
                        longestlen = JimStringCommonLength(longeststr, longestlen, Jim_String(valObj), Jim_Utf8Length(interp, valObj));
                    }
                }
                if (longeststr) {
                    Jim_SetResultString(interp, longeststr, longestlen);
                }
                return JIM_OK;
            }
    }
    return JIM_ERR; /* Cannot ever get here */
}

int Jim_tclprefixInit(Jim_Interp *interp)
{
    Jim_PackageProvideCheck(interp, "tclprefix");
    Jim_RegisterSimpleCmd(interp, "tcl::prefix", "subcommand ?arg ...?", 1, -1, Jim_TclPrefixCoreCommand);
    return JIM_OK;
}
