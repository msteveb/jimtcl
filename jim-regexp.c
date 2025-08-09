/*
 * Implements the regexp and regsub commands for Jim
 *
 * (c) 2008 Steve Bennett <steveb@workware.net.au>
 *
 * Uses C library regcomp()/regexec() for the matching.
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
 *
 * Based on code originally from Tcl 6.7:
 *
 * Copyright 1987-1991 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include <stdlib.h>
#include <string.h>

#include "jimautoconf.h"
#if defined(JIM_REGEXP)
    #include "jimregexp.h"
#else
    #include <regex.h>
    #define jim_regcomp regcomp
    #define jim_regexec regexec
    #define jim_regerror regerror
    #define jim_regfree regfree
#endif
#include "jim.h"
#include "utf8.h"

static void FreeRegexpInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    jim_regfree(objPtr->internalRep.ptrIntValue.ptr);
    Jim_Free(objPtr->internalRep.ptrIntValue.ptr);
}

/* internal rep is stored in ptrIntvalue
 *  ptr = compiled regex
 *  int1 = flags
 */
static const Jim_ObjType regexpObjType = {
    "regexp",
    FreeRegexpInternalRep,
    NULL,
    NULL,
    JIM_TYPE_NONE
};

static regex_t *SetRegexpFromAny(Jim_Interp *interp, Jim_Obj *objPtr, unsigned flags)
{
    regex_t *compre;
    const char *pattern;
    int ret;

    /* Check if the object is already an uptodate variable */
    if (objPtr->typePtr == &regexpObjType &&
        objPtr->internalRep.ptrIntValue.ptr && objPtr->internalRep.ptrIntValue.int1 == flags) {
        /* nothing to do */
        return objPtr->internalRep.ptrIntValue.ptr;
    }

    /* Not a regexp or the flags do not match */

    /* Get the string representation */
    pattern = Jim_String(objPtr);
    compre = Jim_Alloc(sizeof(regex_t));

    if ((ret = jim_regcomp(compre, pattern, REG_EXTENDED | flags)) != 0) {
        char buf[100];

        jim_regerror(ret, compre, buf, sizeof(buf));
        Jim_SetResultFormatted(interp, "couldn't compile regular expression pattern: %s", buf);
        jim_regfree(compre);
        Jim_Free(compre);
        return NULL;
    }

    Jim_FreeIntRep(interp, objPtr);

    objPtr->typePtr = &regexpObjType;
    objPtr->internalRep.ptrIntValue.int1 = flags;
    objPtr->internalRep.ptrIntValue.ptr = compre;

    return compre;
}

int Jim_RegexpCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int opt_indices = 0;
    int opt_all = 0;
    int opt_inline = 0;
    int opt_lineanchor = 0;
    regex_t *regex;
    int match, i, j;
    int offset = 0;
    regmatch_t *pmatch = NULL;
    int source_len;
    int result = JIM_OK;
    const char *pattern;
    const char *source_str;
    int num_matches = 0;
    int num_vars;
    Jim_Obj *resultListObj = NULL;
    int regcomp_flags = 0;
    int eflags = 0;
    int option;
    enum {
        OPT_INDICES,  OPT_NOCASE, OPT_LINE, OPT_LINESTOP, OPT_LINEANCHOR, OPT_ALL, OPT_INLINE, OPT_START, OPT_EXPANDED, OPT_END
    };
    static const char * const options[] = {
        "-indices", "-nocase", "-line", "-linestop", "-lineanchor", "-all", "-inline", "-start", "-expanded", "--", NULL
    };

    for (i = 1; i < argc; i++) {
        const char *opt = Jim_String(argv[i]);

        if (*opt != '-') {
            break;
        }
        if (Jim_GetEnum(interp, argv[i], options, &option, "option", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }
        if (option == OPT_END) {
            i++;
            break;
        }
        switch (option) {
            case OPT_INDICES:
                opt_indices = 1;
                break;

            case OPT_NOCASE:
                regcomp_flags |= REG_ICASE;
                break;

            case OPT_LINE:
                regcomp_flags |= REG_NEWLINE;
                opt_lineanchor = 1;
                break;

#ifdef REG_NEWLINE_STOP
            case OPT_LINESTOP:
                regcomp_flags |= REG_NEWLINE_STOP;
                break;
#endif
#ifdef REG_NEWLINE_ANCHOR
            case OPT_LINEANCHOR:
                regcomp_flags |= REG_NEWLINE_ANCHOR;
                opt_lineanchor = 1;
                break;
#endif
            case OPT_ALL:
                opt_all = 1;
                break;

            case OPT_INLINE:
                opt_inline = 1;
                break;

            case OPT_START:
                if (++i == argc) {
                    return JIM_USAGE;
                }
                if (Jim_GetIndex(interp, argv[i], &offset) != JIM_OK) {
                    return JIM_ERR;
                }
                break;

#ifdef REG_EXPANDED
            case OPT_EXPANDED:
                regcomp_flags |= REG_EXPANDED;
                break;
#endif
            default:
                /* Could get here if -linestop or -lineanchor or -expanded is not supported */
                Jim_SetResultFormatted(interp, "not supported: %#s", argv[i]);
                return JIM_ERR;
        }
    }
    if (argc - i < 2) {
        return JIM_USAGE;
    }

    regex = SetRegexpFromAny(interp, argv[i], regcomp_flags);
    if (!regex) {
        return JIM_ERR;
    }

    pattern = Jim_String(argv[i]);
    source_str = Jim_GetString(argv[i + 1], &source_len);

    num_vars = argc - i - 2;

    if (opt_inline) {
        if (num_vars) {
            Jim_SetResultString(interp, "regexp match variables not allowed when using -inline",
                -1);
            result = JIM_ERR;
            goto done;
        }
        num_vars = regex->re_nsub + 1;
    }

    pmatch = Jim_Alloc((num_vars + 1) * sizeof(*pmatch));

    /* If an offset has been specified, adjust for that now.
     * If it points past the end of the string, point to the terminating null
     */
    if (offset) {
        if (offset < 0) {
            offset += source_len + 1;
        }
        if (offset > source_len) {
            source_str += source_len;
        }
        else if (offset > 0) {
            source_str += utf8_index(source_str, offset);
        }
        eflags |= REG_NOTBOL;
    }

    if (opt_inline) {
        resultListObj = Jim_NewListObj(interp, NULL, 0);
    }

  next_match:
    match = jim_regexec(regex, source_str, num_vars + 1, pmatch, eflags);
    if (match >= REG_BADPAT) {
        char buf[100];

        jim_regerror(match, regex, buf, sizeof(buf));
        Jim_SetResultFormatted(interp, "error while matching pattern: %s", buf);
        result = JIM_ERR;
        goto done;
    }

    if (match == REG_NOMATCH) {
        goto done;
    }

    num_matches++;

    /* We used to not assign vars for -all if not -inline, since we can't
     * really assign capture groups for multiple matches, but Tcl does this,
     * just setting the last value for each capture group, so we will do the
     * same for compatibility
     */

    /*
     * If additional variable names have been specified, return
     * index information in those variables.
     */

    j = 0;
    for (j = 0; j < num_vars; j++) {
        Jim_Obj *resultObj;

        if (opt_indices) {
            resultObj = Jim_NewListObj(interp, NULL, 0);
        }
        else {
            resultObj = Jim_NewStringObj(interp, "", 0);
        }

        if (pmatch[j].rm_so == -1) {
            if (opt_indices) {
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp, -1));
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp, -1));
            }
        }
        else {
            if (opt_indices) {
                /* rm_so and rm_eo are byte offsets. We need char offsets */
                int so = utf8_strlen(source_str, pmatch[j].rm_so);
                int eo = utf8_strlen(source_str, pmatch[j].rm_eo);
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp, offset + so));
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp, offset + eo - 1));
            }
            else {
                Jim_AppendString(interp, resultObj, source_str + pmatch[j].rm_so, pmatch[j].rm_eo - pmatch[j].rm_so);
            }
        }

        if (opt_inline) {
            Jim_ListAppendElement(interp, resultListObj, resultObj);
        }
        else {
            /* And now set the result variable */
            result = Jim_SetVariable(interp, argv[i + 2 + j], resultObj);

            if (result != JIM_OK) {
                break;
            }
        }
    }

    if (opt_all && (pattern[0] != '^' || opt_lineanchor) && *source_str) {
        if (pmatch[0].rm_eo) {
            offset += utf8_strlen(source_str, pmatch[0].rm_eo);
            source_str += pmatch[0].rm_eo;
        }
        else {
            source_str++;
            offset++;
        }
        if (*source_str) {
            eflags = REG_NOTBOL;
            goto next_match;
        }
    }

  done:
    if (result == JIM_OK) {
        if (opt_inline) {
            Jim_SetResult(interp, resultListObj);
        }
        else {
            Jim_SetResultInt(interp, num_matches);
        }
    }

    Jim_Free(pmatch);
    return result;
}

#define MAX_SUB_MATCHES 50

int Jim_RegsubCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int regcomp_flags = 0;
    int regexec_flags = 0;
    int opt_all = 0;
    int opt_command = 0;
    int offset = 0;
    regex_t *regex;
    const char *p;
    int result = JIM_OK;
    regmatch_t pmatch[MAX_SUB_MATCHES + 1];
    int num_matches = 0;

    int i, j, n;
    Jim_Obj *varname;
    Jim_Obj *resultObj;
    Jim_Obj *cmd_prefix = NULL;
    Jim_Obj *regcomp_obj = NULL;
    const char *source_str;
    int source_len;
    const char *replace_str = NULL;
    int replace_len;
    const char *pattern;
    int option;
    enum {
        OPT_NOCASE, OPT_LINE, OPT_LINESTOP, OPT_LINEANCHOR, OPT_ALL, OPT_START, OPT_COMMAND, OPT_EXPANDED, OPT_END
    };
    static const char * const options[] = {
        "-nocase", "-line", "-linestop", "-lineanchor", "-all", "-start", "-command", "-expanded", "--", NULL
    };

    for (i = 1; i < argc; i++) {
        const char *opt = Jim_String(argv[i]);

        if (*opt != '-') {
            break;
        }
        if (Jim_GetEnum(interp, argv[i], options, &option, "option", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }
        if (option == OPT_END) {
            i++;
            break;
        }
        switch (option) {
            case OPT_NOCASE:
                regcomp_flags |= REG_ICASE;
                break;

            case OPT_LINE:
                regcomp_flags |= REG_NEWLINE;
                break;

#ifdef REG_NEWLINE_STOP
            case OPT_LINESTOP:
                regcomp_flags |= REG_NEWLINE_STOP;
                break;
#endif
#ifdef REG_NEWLINE_ANCHOR
            case OPT_LINEANCHOR:
                regcomp_flags |= REG_NEWLINE_ANCHOR;
                break;
#endif
            case OPT_ALL:
                opt_all = 1;
                break;

            case OPT_START:
                if (++i == argc) {
                    return JIM_USAGE;
                }
                if (Jim_GetIndex(interp, argv[i], &offset) != JIM_OK) {
                    return JIM_ERR;
                }
                break;

            case OPT_COMMAND:
                opt_command = 1;
                break;

#ifdef REG_EXPANDED
            case OPT_EXPANDED:
                regcomp_flags |= REG_EXPANDED;
                break;
#endif

            default:
                /* Could get here if -linestop or -lineanchor or -expanded is not supported */
                Jim_SetResultFormatted(interp, "not supported: %#s", argv[i]);
                return JIM_ERR;
        }
    }
    if (argc - i != 3 && argc - i != 4) {
        return JIM_USAGE;
    }

	/* Need to ensure that this is unshared, so just duplicate it always */
    regcomp_obj = Jim_DuplicateObj(interp, argv[i]);
	Jim_IncrRefCount(regcomp_obj);
    regex = SetRegexpFromAny(interp, regcomp_obj, regcomp_flags);
    if (!regex) {
		Jim_DecrRefCount(interp, regcomp_obj);
        return JIM_ERR;
    }
    pattern = Jim_String(argv[i]);

    source_str = Jim_GetString(argv[i + 1], &source_len);
    if (opt_command) {
        cmd_prefix = argv[i + 2];
        if (Jim_ListLength(interp, cmd_prefix) == 0) {
            Jim_SetResultString(interp, "command prefix must be a list of at least one element", -1);
			Jim_DecrRefCount(interp, regcomp_obj);
            return JIM_ERR;
        }
        Jim_IncrRefCount(cmd_prefix);
    }
    else {
        replace_str = Jim_GetString(argv[i + 2], &replace_len);
    }
    varname = argv[i + 3];

    /* Create the result string */
    resultObj = Jim_NewStringObj(interp, "", 0);

    /* If an offset has been specified, adjust for that now.
     * If it points past the end of the string, point to the terminating null
     */
    if (offset) {
        if (offset < 0) {
            offset += source_len + 1;
        }
        if (offset > source_len) {
            offset = source_len;
        }
        else if (offset < 0) {
            offset = 0;
        }
    }
    /* Convert from character offset to byte offset */
    offset = utf8_index(source_str, offset);

    /* Copy the part before -start */
    Jim_AppendString(interp, resultObj, source_str, offset);

    /*
     * The following loop is to handle multiple matches within the
     * same source string;  each iteration handles one match and its
     * corresponding substitution.  If "-all" hasn't been specified
     * then the loop body only gets executed once.
     */

    n = source_len - offset;
    p = source_str + offset;

    /* To match Tcl, an empty pattern does not match at the end
     * of the string.
     */
    while (n || pattern[0]) {
        int match = jim_regexec(regex, p, MAX_SUB_MATCHES, pmatch, regexec_flags);

        if (match >= REG_BADPAT) {
            char buf[100];

            jim_regerror(match, regex, buf, sizeof(buf));
            Jim_SetResultFormatted(interp, "error while matching pattern: %s", buf);
            return JIM_ERR;
        }
        if (match == REG_NOMATCH) {
            break;
        }

        num_matches++;

        /*
         * Copy the portion of the source string before the match to the
         * result variable.
         */
        Jim_AppendString(interp, resultObj, p, pmatch[0].rm_so);

        if (opt_command) {
            /* construct the command as a list */
            Jim_Obj *cmdListObj = Jim_DuplicateObj(interp, cmd_prefix);
            for (j = 0; j < MAX_SUB_MATCHES; j++) {
                if (pmatch[j].rm_so == -1) {
                    break;
                }
                else {
                    Jim_Obj *srcObj = Jim_NewStringObj(interp, p + pmatch[j].rm_so, pmatch[j].rm_eo - pmatch[j].rm_so);
                    Jim_ListAppendElement(interp, cmdListObj, srcObj);
                }
            }
            Jim_IncrRefCount(cmdListObj);

            result = Jim_EvalObj(interp, cmdListObj);
            Jim_DecrRefCount(interp, cmdListObj);
            if (result != JIM_OK) {
                goto cmd_error;
            }
            Jim_AppendString(interp, resultObj, Jim_String(Jim_GetResult(interp)), -1);
        }
        else {
            /*
             * Append the subSpec (replace_str) argument to the variable, making appropriate
             * substitutions.  This code is a bit hairy because of the backslash
             * conventions and because the code saves up ranges of characters in
             * subSpec to reduce the number of calls to Jim_SetVar.
             */

            for (j = 0; j < replace_len; j++) {
                int idx;
                int c = replace_str[j];

                if (c == '&') {
                    idx = 0;
                }
                else if (c == '\\' && j < replace_len) {
                    c = replace_str[++j];
                    if ((c >= '0') && (c <= '9')) {
                        idx = c - '0';
                    }
                    else if ((c == '\\') || (c == '&')) {
                        Jim_AppendString(interp, resultObj, replace_str + j, 1);
                        continue;
                    }
                    else {
                        /* If the replacement is a trailing backslash, just replace with a backslash, otherwise
                         * with the literal backslash and the following character
                         */
                        Jim_AppendString(interp, resultObj, replace_str + j - 1, (j == replace_len) ? 1 : 2);
                        continue;
                    }
                }
                else {
                    Jim_AppendString(interp, resultObj, replace_str + j, 1);
                    continue;
                }
                if ((idx < MAX_SUB_MATCHES) && pmatch[idx].rm_so != -1 && pmatch[idx].rm_eo != -1) {
                    Jim_AppendString(interp, resultObj, p + pmatch[idx].rm_so,
                        pmatch[idx].rm_eo - pmatch[idx].rm_so);
                }
            }
        }

        p += pmatch[0].rm_eo;
        n -= pmatch[0].rm_eo;

        /* If -all is not specified, or there is no source left, we are done */
        if (!opt_all || n == 0) {
            break;
        }

        regexec_flags = 0;
        if (pmatch[0].rm_eo == pmatch[0].rm_so) {
            /* Matched a zero length string. Need to avoid matching the same position again */
            if (pattern[0] == '^') {
                /* An anchored search sets REG_BOL */
                regexec_flags = REG_NOTBOL;
            }
            else {
                /* A non-anchored search advances by one char */
                int charlen = utf8_charlen(p[0]);
                Jim_AppendString(interp, resultObj, p, charlen);
                p += charlen;
                n -= charlen;
            }
        }
    }

    /*
     * Copy the portion of the string after the last match to the
     * result variable.
     */
    Jim_AppendString(interp, resultObj, p, -1);

cmd_error:
    if (result == JIM_OK) {
        /* And now set or return the result variable */
        if (argc - i == 4) {
            result = Jim_SetVariable(interp, varname, resultObj);

            if (result == JIM_OK) {
                Jim_SetResultInt(interp, num_matches);
            }
        }
        else {
            Jim_SetResult(interp, resultObj);
            result = JIM_OK;
        }
    }
    else {
        Jim_FreeObj(interp, resultObj);
    }

    if (opt_command) {
        Jim_DecrRefCount(interp, cmd_prefix);
    }

	Jim_DecrRefCount(interp, regcomp_obj);

    return result;
}

int Jim_regexpInit(Jim_Interp *interp)
{
    Jim_PackageProvideCheck(interp, "regexp");
    Jim_RegisterSimpleCmd(interp, "regexp", "?-option ...? exp string ?matchVar? ?subMatchVar ...?", 2, -1, Jim_RegexpCmd);
    Jim_RegisterSimpleCmd(interp, "regsub", "?-option ...? exp string subSpec ?varName?", 3, -1, Jim_RegsubCmd);
    return JIM_OK;
}
