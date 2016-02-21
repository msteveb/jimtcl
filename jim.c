/* Jim - A small embeddable Tcl interpreter
 *
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright 2005 Clemens Hintze <c.hintze@gmx.net>
 * Copyright 2005 patthoyts - Pat Thoyts <patthoyts@users.sf.net>
 * Copyright 2008,2009 oharboe - Øyvind Harboe - oyvind.harboe@zylin.com
 * Copyright 2008 Andrew Lunn <andrew@lunn.ch>
 * Copyright 2008 Duane Ellis <openocd@duaneellis.com>
 * Copyright 2008 Uwe Klein <uklein@klein-messgeraete.de>
 * Copyright 2008 Steve Bennett <steveb@workware.net.au>
 * Copyright 2009 Nico Coesel <ncoesel@dealogic.nl>
 * Copyright 2009 Zachary T Welch zw@superlucidity.net
 * Copyright 2009 David Brownell
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
 **/
#define JIM_OPTIMIZATION        /* comment to avoid optimizations and reduce size */

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <setjmp.h>

#include "jim.h"
#include "jimautoconf.h"
#include "utf8.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif
#ifdef HAVE_CRT_EXTERNS_H
#include <crt_externs.h>
#endif

/* For INFINITY, even if math functions are not enabled */
#include <math.h>

/* We may decide to switch to using $[...] after all, so leave it as an option */
/*#define EXPRSUGAR_BRACKET*/

/* For the no-autoconf case */
#ifndef TCL_LIBRARY
#define TCL_LIBRARY "."
#endif
#ifndef TCL_PLATFORM_OS
#define TCL_PLATFORM_OS "unknown"
#endif
#ifndef TCL_PLATFORM_PLATFORM
#define TCL_PLATFORM_PLATFORM "unknown"
#endif
#ifndef TCL_PLATFORM_PATH_SEPARATOR
#define TCL_PLATFORM_PATH_SEPARATOR ":"
#endif

/*#define DEBUG_SHOW_SCRIPT*/
/*#define DEBUG_SHOW_SCRIPT_TOKENS*/
/*#define DEBUG_SHOW_SUBST*/
/*#define DEBUG_SHOW_EXPR*/
/*#define DEBUG_SHOW_EXPR_TOKENS*/
/*#define JIM_DEBUG_GC*/
#ifdef JIM_MAINTAINER
#define JIM_DEBUG_COMMAND
#define JIM_DEBUG_PANIC
#endif
/* Enable this (in conjunction with valgrind) to help debug
 * reference counting issues
 */
/*#define JIM_DISABLE_OBJECT_POOL*/

/* Maximum size of an integer */
#define JIM_INTEGER_SPACE 24

const char *jim_tt_name(int type);

#ifdef JIM_DEBUG_PANIC
static void JimPanicDump(int fail_condition, const char *fmt, ...);
#define JimPanic(X) JimPanicDump X
#else
#define JimPanic(X)
#endif

/* -----------------------------------------------------------------------------
 * Global variables
 * ---------------------------------------------------------------------------*/

/* A shared empty string for the objects string representation.
 * Jim_InvalidateStringRep knows about it and doesn't try to free it. */
static char JimEmptyStringRep[] = "";

/* -----------------------------------------------------------------------------
 * Required prototypes of not exported functions
 * ---------------------------------------------------------------------------*/
static void JimFreeCallFrame(Jim_Interp *interp, Jim_CallFrame *cf, int action);
static int ListSetIndex(Jim_Interp *interp, Jim_Obj *listPtr, int listindex, Jim_Obj *newObjPtr,
    int flags);
static int JimDeleteLocalProcs(Jim_Interp *interp, Jim_Stack *localCommands);
static Jim_Obj *JimExpandDictSugar(Jim_Interp *interp, Jim_Obj *objPtr);
static void SetDictSubstFromAny(Jim_Interp *interp, Jim_Obj *objPtr);
static Jim_Obj **JimDictPairs(Jim_Obj *dictPtr, int *len);
static void JimSetFailedEnumResult(Jim_Interp *interp, const char *arg, const char *badtype,
    const char *prefix, const char *const *tablePtr, const char *name);
static int JimCallProcedure(Jim_Interp *interp, Jim_Cmd *cmd, int argc, Jim_Obj *const *argv);
static int JimGetWideNoErr(Jim_Interp *interp, Jim_Obj *objPtr, jim_wide * widePtr);
static int JimSign(jim_wide w);
static int JimValidName(Jim_Interp *interp, const char *type, Jim_Obj *nameObjPtr);
static void JimPrngSeed(Jim_Interp *interp, unsigned char *seed, int seedLen);
static void JimRandomBytes(Jim_Interp *interp, void *dest, unsigned int len);


/* Fast access to the int (wide) value of an object which is known to be of int type */
#define JimWideValue(objPtr) (objPtr)->internalRep.wideValue

#define JimObjTypeName(O) ((O)->typePtr ? (O)->typePtr->name : "none")

static int utf8_tounicode_case(const char *s, int *uc, int upper)
{
    int l = utf8_tounicode(s, uc);
    if (upper) {
        *uc = utf8_upper(*uc);
    }
    return l;
}

/* These can be used in addition to JIM_CASESENS/JIM_NOCASE */
#define JIM_CHARSET_SCAN 2
#define JIM_CHARSET_GLOB 0

/**
 * pattern points to a string like "[^a-z\ub5]"
 *
 * The pattern may contain trailing chars, which are ignored.
 *
 * The pattern is matched against unicode char 'c'.
 *
 * If (flags & JIM_NOCASE), case is ignored when matching.
 * If (flags & JIM_CHARSET_SCAN), the considers ^ and ] special at the start
 * of the charset, per scan, rather than glob/string match.
 *
 * If the unicode char 'c' matches that set, returns a pointer to the ']' character,
 * or the null character if the ']' is missing.
 *
 * Returns NULL on no match.
 */
static const char *JimCharsetMatch(const char *pattern, int c, int flags)
{
    int not = 0;
    int pchar;
    int match = 0;
    int nocase = 0;

    if (flags & JIM_NOCASE) {
        nocase++;
        c = utf8_upper(c);
    }

    if (flags & JIM_CHARSET_SCAN) {
        if (*pattern == '^') {
            not++;
            pattern++;
        }

        /* Special case. If the first char is ']', it is part of the set */
        if (*pattern == ']') {
            goto first;
        }
    }

    while (*pattern && *pattern != ']') {
        /* Exact match */
        if (pattern[0] == '\\') {
first:
            pattern += utf8_tounicode_case(pattern, &pchar, nocase);
        }
        else {
            /* Is this a range? a-z */
            int start;
            int end;

            pattern += utf8_tounicode_case(pattern, &start, nocase);
            if (pattern[0] == '-' && pattern[1]) {
                /* skip '-' */
                pattern += utf8_tounicode(pattern, &pchar);
                pattern += utf8_tounicode_case(pattern, &end, nocase);

                /* Handle reversed range too */
                if ((c >= start && c <= end) || (c >= end && c <= start)) {
                    match = 1;
                }
                continue;
            }
            pchar = start;
        }

        if (pchar == c) {
            match = 1;
        }
    }
    if (not) {
        match = !match;
    }

    return match ? pattern : NULL;
}

/* Glob-style pattern matching. */

/* Note: string *must* be valid UTF-8 sequences
 */
static int JimGlobMatch(const char *pattern, const char *string, int nocase)
{
    int c;
    int pchar;
    while (*pattern) {
        switch (pattern[0]) {
            case '*':
                while (pattern[1] == '*') {
                    pattern++;
                }
                pattern++;
                if (!pattern[0]) {
                    return 1;   /* match */
                }
                while (*string) {
                    /* Recursive call - Does the remaining pattern match anywhere? */
                    if (JimGlobMatch(pattern, string, nocase))
                        return 1;       /* match */
                    string += utf8_tounicode(string, &c);
                }
                return 0;       /* no match */

            case '?':
                string += utf8_tounicode(string, &c);
                break;

            case '[': {
                    string += utf8_tounicode(string, &c);
                    pattern = JimCharsetMatch(pattern + 1, c, nocase ? JIM_NOCASE : 0);
                    if (!pattern) {
                        return 0;
                    }
                    if (!*pattern) {
                        /* Ran out of pattern (no ']') */
                        continue;
                    }
                    break;
                }
            case '\\':
                if (pattern[1]) {
                    pattern++;
                }
                /* fall through */
            default:
                string += utf8_tounicode_case(string, &c, nocase);
                utf8_tounicode_case(pattern, &pchar, nocase);
                if (pchar != c) {
                    return 0;
                }
                break;
        }
        pattern += utf8_tounicode_case(pattern, &pchar, nocase);
        if (!*string) {
            while (*pattern == '*') {
                pattern++;
            }
            break;
        }
    }
    if (!*pattern && !*string) {
        return 1;
    }
    return 0;
}

/**
 * string comparison. Works on binary data.
 *
 * Returns -1, 0 or 1
 *
 * Note that the lengths are byte lengths, not char lengths.
 */
static int JimStringCompare(const char *s1, int l1, const char *s2, int l2)
{
    if (l1 < l2) {
        return memcmp(s1, s2, l1) <= 0 ? -1 : 1;
    }
    else if (l2 < l1) {
        return memcmp(s1, s2, l2) >= 0 ? 1 : -1;
    }
    else {
        return JimSign(memcmp(s1, s2, l1));
    }
}

/**
 * Compare null terminated strings, up to a maximum of 'maxchars' characters,
 * (or end of string if 'maxchars' is -1).
 *
 * Returns -1, 0, 1 for s1 < s2, s1 == s2, s1 > s2 respectively.
 *
 * Note: does not support embedded nulls.
 */
static int JimStringCompareLen(const char *s1, const char *s2, int maxchars, int nocase)
{
    while (*s1 && *s2 && maxchars) {
        int c1, c2;
        s1 += utf8_tounicode_case(s1, &c1, nocase);
        s2 += utf8_tounicode_case(s2, &c2, nocase);
        if (c1 != c2) {
            return JimSign(c1 - c2);
        }
        maxchars--;
    }
    if (!maxchars) {
        return 0;
    }
    /* One string or both terminated */
    if (*s1) {
        return 1;
    }
    if (*s2) {
        return -1;
    }
    return 0;
}

/* Search 's1' inside 's2', starting to search from char 'index' of 's2'.
 * The index of the first occurrence of s1 in s2 is returned.
 * If s1 is not found inside s2, -1 is returned. */
static int JimStringFirst(const char *s1, int l1, const char *s2, int l2, int idx)
{
    int i;
    int l1bytelen;

    if (!l1 || !l2 || l1 > l2) {
        return -1;
    }
    if (idx < 0)
        idx = 0;
    s2 += utf8_index(s2, idx);

    l1bytelen = utf8_index(s1, l1);

    for (i = idx; i <= l2 - l1; i++) {
        int c;
        if (memcmp(s2, s1, l1bytelen) == 0) {
            return i;
        }
        s2 += utf8_tounicode(s2, &c);
    }
    return -1;
}

/**
 * Note: Lengths and return value are in bytes, not chars.
 */
static int JimStringLast(const char *s1, int l1, const char *s2, int l2)
{
    const char *p;

    if (!l1 || !l2 || l1 > l2)
        return -1;

    /* Now search for the needle */
    for (p = s2 + l2 - 1; p != s2 - 1; p--) {
        if (*p == *s1 && memcmp(s1, p, l1) == 0) {
            return p - s2;
        }
    }
    return -1;
}

#ifdef JIM_UTF8
/**
 * Note: Lengths and return value are in chars.
 */
static int JimStringLastUtf8(const char *s1, int l1, const char *s2, int l2)
{
    int n = JimStringLast(s1, utf8_index(s1, l1), s2, utf8_index(s2, l2));
    if (n > 0) {
        n = utf8_strlen(s2, n);
    }
    return n;
}
#endif

/**
 * After an strtol()/strtod()-like conversion,
 * check whether something was converted and that
 * the only thing left is white space.
 *
 * Returns JIM_OK or JIM_ERR.
 */
static int JimCheckConversion(const char *str, const char *endptr)
{
    if (str[0] == '\0' || str == endptr) {
        return JIM_ERR;
    }

    if (endptr[0] != '\0') {
        while (*endptr) {
            if (!isspace(UCHAR(*endptr))) {
                return JIM_ERR;
            }
            endptr++;
        }
    }
    return JIM_OK;
}

/* Parses the front of a number to determine it's sign and base
 * Returns the index to start parsing according to the given base
 */
static int JimNumberBase(const char *str, int *base, int *sign)
{
    int i = 0;

    *base = 10;

    while (isspace(UCHAR(str[i]))) {
        i++;
    }

    if (str[i] == '-') {
        *sign = -1;
        i++;
    }
    else {
        if (str[i] == '+') {
            i++;
        }
        *sign = 1;
    }

    if (str[i] != '0') {
        /* base 10 */
        return 0;
    }

    /* We have 0<x>, so see if we can convert it */
    switch (str[i + 1]) {
        case 'x': case 'X': *base = 16; break;
        case 'o': case 'O': *base = 8; break;
        case 'b': case 'B': *base = 2; break;
        default: return 0;
    }
    i += 2;
    /* Ensure that (e.g.) 0x-5 fails to parse */
    if (str[i] != '-' && str[i] != '+' && !isspace(UCHAR(str[i]))) {
        /* Parse according to this base */
        return i;
    }
    /* Parse as base 10 */
    *base = 10;
    return 0;
}

/* Converts a number as per strtol(..., 0) except leading zeros do *not*
 * imply octal. Instead, decimal is assumed unless the number begins with 0x, 0o or 0b
 */
static long jim_strtol(const char *str, char **endptr)
{
    int sign;
    int base;
    int i = JimNumberBase(str, &base, &sign);

    if (base != 10) {
        long value = strtol(str + i, endptr, base);
        if (endptr == NULL || *endptr != str + i) {
            return value * sign;
        }
    }

    /* Can just do a regular base-10 conversion */
    return strtol(str, endptr, 10);
}


/* Converts a number as per strtoull(..., 0) except leading zeros do *not*
 * imply octal. Instead, decimal is assumed unless the number begins with 0x, 0o or 0b
 */
static jim_wide jim_strtoull(const char *str, char **endptr)
{
#ifdef HAVE_LONG_LONG
    int sign;
    int base;
    int i = JimNumberBase(str, &base, &sign);

    if (base != 10) {
        jim_wide value = strtoull(str + i, endptr, base);
        if (endptr == NULL || *endptr != str + i) {
            return value * sign;
        }
    }

    /* Can just do a regular base-10 conversion */
    return strtoull(str, endptr, 10);
#else
    return (unsigned long)jim_strtol(str, endptr);
#endif
}

int Jim_StringToWide(const char *str, jim_wide * widePtr, int base)
{
    char *endptr;

    if (base) {
        *widePtr = strtoull(str, &endptr, base);
    }
    else {
        *widePtr = jim_strtoull(str, &endptr);
    }

    return JimCheckConversion(str, endptr);
}

int Jim_StringToDouble(const char *str, double *doublePtr)
{
    char *endptr;

    /* Callers can check for underflow via ERANGE */
    errno = 0;

    *doublePtr = strtod(str, &endptr);

    return JimCheckConversion(str, endptr);
}

static jim_wide JimPowWide(jim_wide b, jim_wide e)
{
    jim_wide i, res = 1;

    if ((b == 0 && e != 0) || (e < 0))
        return 0;
    for (i = 0; i < e; i++) {
        res *= b;
    }
    return res;
}

/* -----------------------------------------------------------------------------
 * Special functions
 * ---------------------------------------------------------------------------*/
#ifdef JIM_DEBUG_PANIC
static void JimPanicDump(int condition, const char *fmt, ...)
{
    va_list ap;

    if (!condition) {
        return;
    }

    va_start(ap, fmt);

    fprintf(stderr, "\nJIM INTERPRETER PANIC: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n\n");
    va_end(ap);

#ifdef HAVE_BACKTRACE
    {
        void *array[40];
        int size, i;
        char **strings;

        size = backtrace(array, 40);
        strings = backtrace_symbols(array, size);
        for (i = 0; i < size; i++)
            fprintf(stderr, "[backtrace] %s\n", strings[i]);
        fprintf(stderr, "[backtrace] Include the above lines and the output\n");
        fprintf(stderr, "[backtrace] of 'nm <executable>' in the bug report.\n");
    }
#endif

    exit(1);
}
#endif

/* -----------------------------------------------------------------------------
 * Memory allocation
 * ---------------------------------------------------------------------------*/

void *Jim_Alloc(int size)
{
    return size ? malloc(size) : NULL;
}

void Jim_Free(void *ptr)
{
    free(ptr);
}

void *Jim_Realloc(void *ptr, int size)
{
    return realloc(ptr, size);
}

char *Jim_StrDup(const char *s)
{
    return strdup(s);
}

char *Jim_StrDupLen(const char *s, int l)
{
    char *copy = Jim_Alloc(l + 1);

    memcpy(copy, s, l + 1);
    copy[l] = 0;                /* Just to be sure, original could be substring */
    return copy;
}

/* -----------------------------------------------------------------------------
 * Time related functions
 * ---------------------------------------------------------------------------*/

/* Returns current time in microseconds */
static jim_wide JimClock(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (jim_wide) tv.tv_sec * 1000000 + tv.tv_usec;
}

/* -----------------------------------------------------------------------------
 * Hash Tables
 * ---------------------------------------------------------------------------*/

/* -------------------------- private prototypes ---------------------------- */
static void JimExpandHashTableIfNeeded(Jim_HashTable *ht);
static unsigned int JimHashTableNextPower(unsigned int size);
static Jim_HashEntry *JimInsertHashEntry(Jim_HashTable *ht, const void *key, int replace);

/* -------------------------- hash functions -------------------------------- */

/* Thomas Wang's 32 bit Mix Function */
unsigned int Jim_IntHashFunction(unsigned int key)
{
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}

/* Generic hash function (we are using to multiply by 9 and add the byte
 * as Tcl) */
unsigned int Jim_GenHashFunction(const unsigned char *buf, int len)
{
    unsigned int h = 0;

    while (len--)
        h += (h << 3) + *buf++;
    return h;
}

/* ----------------------------- API implementation ------------------------- */

/* reset a hashtable already initialized */
static void JimResetHashTable(Jim_HashTable *ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
    ht->collisions = 0;
#ifdef JIM_RANDOMISE_HASH
    /* This is initialised to a random value to avoid a hash collision attack.
     * See: n.runs-SA-2011.004
     */
    ht->uniq = (rand() ^ time(NULL) ^ clock());
#else
    ht->uniq = 0;
#endif
}

static void JimInitHashTableIterator(Jim_HashTable *ht, Jim_HashTableIterator *iter)
{
    iter->ht = ht;
    iter->index = -1;
    iter->entry = NULL;
    iter->nextEntry = NULL;
}

/* Initialize the hash table */
int Jim_InitHashTable(Jim_HashTable *ht, const Jim_HashTableType *type, void *privDataPtr)
{
    JimResetHashTable(ht);
    ht->type = type;
    ht->privdata = privDataPtr;
    return JIM_OK;
}

/* Resize the table to the minimal size that contains all the elements,
 * but with the invariant of a USER/BUCKETS ration near to <= 1 */
void Jim_ResizeHashTable(Jim_HashTable *ht)
{
    int minimal = ht->used;

    if (minimal < JIM_HT_INITIAL_SIZE)
        minimal = JIM_HT_INITIAL_SIZE;
    Jim_ExpandHashTable(ht, minimal);
}

/* Expand or create the hashtable */
void Jim_ExpandHashTable(Jim_HashTable *ht, unsigned int size)
{
    Jim_HashTable n;            /* the new hashtable */
    unsigned int realsize = JimHashTableNextPower(size), i;

    /* the size is invalid if it is smaller than the number of
     * elements already inside the hashtable */
     if (size <= ht->used)
        return;

    Jim_InitHashTable(&n, ht->type, ht->privdata);
    n.size = realsize;
    n.sizemask = realsize - 1;
    n.table = Jim_Alloc(realsize * sizeof(Jim_HashEntry *));
    /* Keep the same 'uniq' as the original */
    n.uniq = ht->uniq;

    /* Initialize all the pointers to NULL */
    memset(n.table, 0, realsize * sizeof(Jim_HashEntry *));

    /* Copy all the elements from the old to the new table:
     * note that if the old hash table is empty ht->used is zero,
     * so Jim_ExpandHashTable just creates an empty hash table. */
    n.used = ht->used;
    for (i = 0; ht->used > 0; i++) {
        Jim_HashEntry *he, *nextHe;

        if (ht->table[i] == NULL)
            continue;

        /* For each hash entry on this slot... */
        he = ht->table[i];
        while (he) {
            unsigned int h;

            nextHe = he->next;
            /* Get the new element index */
            h = Jim_HashKey(ht, he->key) & n.sizemask;
            he->next = n.table[h];
            n.table[h] = he;
            ht->used--;
            /* Pass to the next element */
            he = nextHe;
        }
    }
    assert(ht->used == 0);
    Jim_Free(ht->table);

    /* Remap the new hashtable in the old */
    *ht = n;
}

/* Add an element to the target hash table */
int Jim_AddHashEntry(Jim_HashTable *ht, const void *key, void *val)
{
    Jim_HashEntry *entry;

    /* Get the index of the new element, or -1 if
     * the element already exists. */
    entry = JimInsertHashEntry(ht, key, 0);
    if (entry == NULL)
        return JIM_ERR;

    /* Set the hash entry fields. */
    Jim_SetHashKey(ht, entry, key);
    Jim_SetHashVal(ht, entry, val);
    return JIM_OK;
}

/* Add an element, discarding the old if the key already exists */
int Jim_ReplaceHashEntry(Jim_HashTable *ht, const void *key, void *val)
{
    int existed;
    Jim_HashEntry *entry;

    /* Get the index of the new element, or -1 if
     * the element already exists. */
    entry = JimInsertHashEntry(ht, key, 1);
    if (entry->key) {
        /* It already exists, so only replace the value.
         * Note if both a destructor and a duplicate function exist,
         * need to dup before destroy. perhaps they are the same
         * reference counted object
         */
        if (ht->type->valDestructor && ht->type->valDup) {
            void *newval = ht->type->valDup(ht->privdata, val);
            ht->type->valDestructor(ht->privdata, entry->u.val);
            entry->u.val = newval;
        }
        else {
            Jim_FreeEntryVal(ht, entry);
            Jim_SetHashVal(ht, entry, val);
        }
        existed = 1;
    }
    else {
        /* Doesn't exist, so set the key */
        Jim_SetHashKey(ht, entry, key);
        Jim_SetHashVal(ht, entry, val);
        existed = 0;
    }

    return existed;
}

/* Search and remove an element */
int Jim_DeleteHashEntry(Jim_HashTable *ht, const void *key)
{
    unsigned int h;
    Jim_HashEntry *he, *prevHe;

    if (ht->used == 0)
        return JIM_ERR;
    h = Jim_HashKey(ht, key) & ht->sizemask;
    he = ht->table[h];

    prevHe = NULL;
    while (he) {
        if (Jim_CompareHashKeys(ht, key, he->key)) {
            /* Unlink the element from the list */
            if (prevHe)
                prevHe->next = he->next;
            else
                ht->table[h] = he->next;
            Jim_FreeEntryKey(ht, he);
            Jim_FreeEntryVal(ht, he);
            Jim_Free(he);
            ht->used--;
            return JIM_OK;
        }
        prevHe = he;
        he = he->next;
    }
    return JIM_ERR;             /* not found */
}

/* Destroy an entire hash table and leave it ready for reuse */
int Jim_FreeHashTable(Jim_HashTable *ht)
{
    unsigned int i;

    /* Free all the elements */
    for (i = 0; ht->used > 0; i++) {
        Jim_HashEntry *he, *nextHe;

        if ((he = ht->table[i]) == NULL)
            continue;
        while (he) {
            nextHe = he->next;
            Jim_FreeEntryKey(ht, he);
            Jim_FreeEntryVal(ht, he);
            Jim_Free(he);
            ht->used--;
            he = nextHe;
        }
    }
    /* Free the table and the allocated cache structure */
    Jim_Free(ht->table);
    /* Re-initialize the table */
    JimResetHashTable(ht);
    return JIM_OK;              /* never fails */
}

Jim_HashEntry *Jim_FindHashEntry(Jim_HashTable *ht, const void *key)
{
    Jim_HashEntry *he;
    unsigned int h;

    if (ht->used == 0)
        return NULL;
    h = Jim_HashKey(ht, key) & ht->sizemask;
    he = ht->table[h];
    while (he) {
        if (Jim_CompareHashKeys(ht, key, he->key))
            return he;
        he = he->next;
    }
    return NULL;
}

Jim_HashTableIterator *Jim_GetHashTableIterator(Jim_HashTable *ht)
{
    Jim_HashTableIterator *iter = Jim_Alloc(sizeof(*iter));
    JimInitHashTableIterator(ht, iter);
    return iter;
}

Jim_HashEntry *Jim_NextHashEntry(Jim_HashTableIterator *iter)
{
    while (1) {
        if (iter->entry == NULL) {
            iter->index++;
            if (iter->index >= (signed)iter->ht->size)
                break;
            iter->entry = iter->ht->table[iter->index];
        }
        else {
            iter->entry = iter->nextEntry;
        }
        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            iter->nextEntry = iter->entry->next;
            return iter->entry;
        }
    }
    return NULL;
}

/* ------------------------- private functions ------------------------------ */

/* Expand the hash table if needed */
static void JimExpandHashTableIfNeeded(Jim_HashTable *ht)
{
    /* If the hash table is empty expand it to the intial size,
     * if the table is "full" dobule its size. */
    if (ht->size == 0)
        Jim_ExpandHashTable(ht, JIM_HT_INITIAL_SIZE);
    if (ht->size == ht->used)
        Jim_ExpandHashTable(ht, ht->size * 2);
}

/* Our hash table capability is a power of two */
static unsigned int JimHashTableNextPower(unsigned int size)
{
    unsigned int i = JIM_HT_INITIAL_SIZE;

    if (size >= 2147483648U)
        return 2147483648U;
    while (1) {
        if (i >= size)
            return i;
        i *= 2;
    }
}

/* Returns the index of a free slot that can be populated with
 * a hash entry for the given 'key'.
 * If the key already exists, -1 is returned. */
static Jim_HashEntry *JimInsertHashEntry(Jim_HashTable *ht, const void *key, int replace)
{
    unsigned int h;
    Jim_HashEntry *he;

    /* Expand the hashtable if needed */
    JimExpandHashTableIfNeeded(ht);

    /* Compute the key hash value */
    h = Jim_HashKey(ht, key) & ht->sizemask;
    /* Search if this slot does not already contain the given key */
    he = ht->table[h];
    while (he) {
        if (Jim_CompareHashKeys(ht, key, he->key))
            return replace ? he : NULL;
        he = he->next;
    }

    /* Allocates the memory and stores key */
    he = Jim_Alloc(sizeof(*he));
    he->next = ht->table[h];
    ht->table[h] = he;
    ht->used++;
    he->key = NULL;

    return he;
}

/* ----------------------- StringCopy Hash Table Type ------------------------*/

static unsigned int JimStringCopyHTHashFunction(const void *key)
{
    return Jim_GenHashFunction(key, strlen(key));
}

static void *JimStringCopyHTDup(void *privdata, const void *key)
{
    return Jim_StrDup(key);
}

static int JimStringCopyHTKeyCompare(void *privdata, const void *key1, const void *key2)
{
    return strcmp(key1, key2) == 0;
}

static void JimStringCopyHTKeyDestructor(void *privdata, void *key)
{
    Jim_Free(key);
}

static const Jim_HashTableType JimPackageHashTableType = {
    JimStringCopyHTHashFunction,     /* hash function */
    JimStringCopyHTDup,              /* key dup */
    NULL,                            /* val dup */
    JimStringCopyHTKeyCompare,       /* key compare */
    JimStringCopyHTKeyDestructor,    /* key destructor */
    NULL                             /* val destructor */
};

typedef struct AssocDataValue
{
    Jim_InterpDeleteProc *delProc;
    void *data;
} AssocDataValue;

static void JimAssocDataHashTableValueDestructor(void *privdata, void *data)
{
    AssocDataValue *assocPtr = (AssocDataValue *) data;

    if (assocPtr->delProc != NULL)
        assocPtr->delProc((Jim_Interp *)privdata, assocPtr->data);
    Jim_Free(data);
}

static const Jim_HashTableType JimAssocDataHashTableType = {
    JimStringCopyHTHashFunction,    /* hash function */
    JimStringCopyHTDup,             /* key dup */
    NULL,                           /* val dup */
    JimStringCopyHTKeyCompare,      /* key compare */
    JimStringCopyHTKeyDestructor,   /* key destructor */
    JimAssocDataHashTableValueDestructor        /* val destructor */
};

/* -----------------------------------------------------------------------------
 * Stack - This is a simple generic stack implementation. It is used for
 * example in the 'expr' expression compiler.
 * ---------------------------------------------------------------------------*/
void Jim_InitStack(Jim_Stack *stack)
{
    stack->len = 0;
    stack->maxlen = 0;
    stack->vector = NULL;
}

void Jim_FreeStack(Jim_Stack *stack)
{
    Jim_Free(stack->vector);
}

int Jim_StackLen(Jim_Stack *stack)
{
    return stack->len;
}

void Jim_StackPush(Jim_Stack *stack, void *element)
{
    int neededLen = stack->len + 1;

    if (neededLen > stack->maxlen) {
        stack->maxlen = neededLen < 20 ? 20 : neededLen * 2;
        stack->vector = Jim_Realloc(stack->vector, sizeof(void *) * stack->maxlen);
    }
    stack->vector[stack->len] = element;
    stack->len++;
}

void *Jim_StackPop(Jim_Stack *stack)
{
    if (stack->len == 0)
        return NULL;
    stack->len--;
    return stack->vector[stack->len];
}

void *Jim_StackPeek(Jim_Stack *stack)
{
    if (stack->len == 0)
        return NULL;
    return stack->vector[stack->len - 1];
}

void Jim_FreeStackElements(Jim_Stack *stack, void (*freeFunc) (void *ptr))
{
    int i;

    for (i = 0; i < stack->len; i++)
        freeFunc(stack->vector[i]);
}

/* -----------------------------------------------------------------------------
 * Tcl Parser
 * ---------------------------------------------------------------------------*/

/* Token types */
#define JIM_TT_NONE    0          /* No token returned */
#define JIM_TT_STR     1          /* simple string */
#define JIM_TT_ESC     2          /* string that needs escape chars conversion */
#define JIM_TT_VAR     3          /* var substitution */
#define JIM_TT_DICTSUGAR   4      /* Syntax sugar for [dict get], $foo(bar) */
#define JIM_TT_CMD     5          /* command substitution */
/* Note: Keep these three together for TOKEN_IS_SEP() */
#define JIM_TT_SEP     6          /* word separator (white space) */
#define JIM_TT_EOL     7          /* line separator */
#define JIM_TT_EOF     8          /* end of script */

#define JIM_TT_LINE    9          /* special 'start-of-line' token. arg is # of arguments to the command. -ve if {*} */
#define JIM_TT_WORD   10          /* special 'start-of-word' token. arg is # of tokens to combine. -ve if {*} */

/* Additional token types needed for expressions */
#define JIM_TT_SUBEXPR_START  11
#define JIM_TT_SUBEXPR_END    12
#define JIM_TT_SUBEXPR_COMMA  13
#define JIM_TT_EXPR_INT       14
#define JIM_TT_EXPR_DOUBLE    15

#define JIM_TT_EXPRSUGAR      16  /* $(expression) */

/* Operator token types start here */
#define JIM_TT_EXPR_OP        20

#define TOKEN_IS_SEP(type) (type >= JIM_TT_SEP && type <= JIM_TT_EOF)

/**
 * Results of missing quotes, braces, etc. from parsing.
 */
struct JimParseMissing {
    int ch;             /* At end of parse, ' ' if complete or '{', '[', '"', '\\' , '{' if incomplete */
    int line;           /* Line number starting the missing token */
};

/* Parser context structure. The same context is used both to parse
 * Tcl scripts and lists. */
struct JimParserCtx
{
    const char *p;              /* Pointer to the point of the program we are parsing */
    int len;                    /* Remaining length */
    int linenr;                 /* Current line number */
    const char *tstart;
    const char *tend;           /* Returned token is at tstart-tend in 'prg'. */
    int tline;                  /* Line number of the returned token */
    int tt;                     /* Token type */
    int eof;                    /* Non zero if EOF condition is true. */
    int inquote;                /* Parsing a quoted string */
    int comment;                /* Non zero if the next chars may be a comment. */
    struct JimParseMissing missing;   /* Details of any missing quotes, etc. */
};

static int JimParseScript(struct JimParserCtx *pc);
static int JimParseSep(struct JimParserCtx *pc);
static int JimParseEol(struct JimParserCtx *pc);
static int JimParseCmd(struct JimParserCtx *pc);
static int JimParseQuote(struct JimParserCtx *pc);
static int JimParseVar(struct JimParserCtx *pc);
static int JimParseBrace(struct JimParserCtx *pc);
static int JimParseStr(struct JimParserCtx *pc);
static int JimParseComment(struct JimParserCtx *pc);
static void JimParseSubCmd(struct JimParserCtx *pc);
static int JimParseSubQuote(struct JimParserCtx *pc);
static Jim_Obj *JimParserGetTokenObj(Jim_Interp *interp, struct JimParserCtx *pc);

/* Initialize a parser context.
 * 'prg' is a pointer to the program text, linenr is the line
 * number of the first line contained in the program. */
static void JimParserInit(struct JimParserCtx *pc, const char *prg, int len, int linenr)
{
    pc->p = prg;
    pc->len = len;
    pc->tstart = NULL;
    pc->tend = NULL;
    pc->tline = 0;
    pc->tt = JIM_TT_NONE;
    pc->eof = 0;
    pc->inquote = 0;
    pc->linenr = linenr;
    pc->comment = 1;
    pc->missing.ch = ' ';
    pc->missing.line = linenr;
}

static int JimParseScript(struct JimParserCtx *pc)
{
    while (1) {                 /* the while is used to reiterate with continue if needed */
        if (!pc->len) {
            pc->tstart = pc->p;
            pc->tend = pc->p - 1;
            pc->tline = pc->linenr;
            pc->tt = JIM_TT_EOL;
            pc->eof = 1;
            return JIM_OK;
        }
        switch (*(pc->p)) {
            case '\\':
                if (*(pc->p + 1) == '\n' && !pc->inquote) {
                    return JimParseSep(pc);
                }
                pc->comment = 0;
                return JimParseStr(pc);
            case ' ':
            case '\t':
            case '\r':
            case '\f':
                if (!pc->inquote)
                    return JimParseSep(pc);
                pc->comment = 0;
                return JimParseStr(pc);
            case '\n':
            case ';':
                pc->comment = 1;
                if (!pc->inquote)
                    return JimParseEol(pc);
                return JimParseStr(pc);
            case '[':
                pc->comment = 0;
                return JimParseCmd(pc);
            case '$':
                pc->comment = 0;
                if (JimParseVar(pc) == JIM_ERR) {
                    /* An orphan $. Create as a separate token */
                    pc->tstart = pc->tend = pc->p++;
                    pc->len--;
                    pc->tt = JIM_TT_ESC;
                }
                return JIM_OK;
            case '#':
                if (pc->comment) {
                    JimParseComment(pc);
                    continue;
                }
                return JimParseStr(pc);
            default:
                pc->comment = 0;
                return JimParseStr(pc);
        }
        return JIM_OK;
    }
}

static int JimParseSep(struct JimParserCtx *pc)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (isspace(UCHAR(*pc->p)) || (*pc->p == '\\' && *(pc->p + 1) == '\n')) {
        if (*pc->p == '\n') {
            break;
        }
        if (*pc->p == '\\') {
            pc->p++;
            pc->len--;
            pc->linenr++;
        }
        pc->p++;
        pc->len--;
    }
    pc->tend = pc->p - 1;
    pc->tt = JIM_TT_SEP;
    return JIM_OK;
}

static int JimParseEol(struct JimParserCtx *pc)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (isspace(UCHAR(*pc->p)) || *pc->p == ';') {
        if (*pc->p == '\n')
            pc->linenr++;
        pc->p++;
        pc->len--;
    }
    pc->tend = pc->p - 1;
    pc->tt = JIM_TT_EOL;
    return JIM_OK;
}

/*
** Here are the rules for parsing:
** {braced expression}
** - Count open and closing braces
** - Backslash escapes meaning of braces
**
** "quoted expression"
** - First double quote at start of word terminates the expression
** - Backslash escapes quote and bracket
** - [commands brackets] are counted/nested
** - command rules apply within [brackets], not quoting rules (i.e. quotes have their own rules)
**
** [command expression]
** - Count open and closing brackets
** - Backslash escapes quote, bracket and brace
** - [commands brackets] are counted/nested
** - "quoted expressions" are parsed according to quoting rules
** - {braced expressions} are parsed according to brace rules
**
** For everything, backslash escapes the next char, newline increments current line
*/

/**
 * Parses a braced expression starting at pc->p.
 *
 * Positions the parser at the end of the braced expression,
 * sets pc->tend and possibly pc->missing.
 */
static void JimParseSubBrace(struct JimParserCtx *pc)
{
    int level = 1;

    /* Skip the brace */
    pc->p++;
    pc->len--;
    while (pc->len) {
        switch (*pc->p) {
            case '\\':
                if (pc->len > 1) {
                    if (*++pc->p == '\n') {
                        pc->linenr++;
                    }
                    pc->len--;
                }
                break;

            case '{':
                level++;
                break;

            case '}':
                if (--level == 0) {
                    pc->tend = pc->p - 1;
                    pc->p++;
                    pc->len--;
                    return;
                }
                break;

            case '\n':
                pc->linenr++;
                break;
        }
        pc->p++;
        pc->len--;
    }
    pc->missing.ch = '{';
    pc->missing.line = pc->tline;
    pc->tend = pc->p - 1;
}

/**
 * Parses a quoted expression starting at pc->p.
 *
 * Positions the parser at the end of the quoted expression,
 * sets pc->tend and possibly pc->missing.
 *
 * Returns the type of the token of the string,
 * either JIM_TT_ESC (if it contains values which need to be [subst]ed)
 * or JIM_TT_STR.
 */
static int JimParseSubQuote(struct JimParserCtx *pc)
{
    int tt = JIM_TT_STR;
    int line = pc->tline;

    /* Skip the quote */
    pc->p++;
    pc->len--;
    while (pc->len) {
        switch (*pc->p) {
            case '\\':
                if (pc->len > 1) {
                    if (*++pc->p == '\n') {
                        pc->linenr++;
                    }
                    pc->len--;
                    tt = JIM_TT_ESC;
                }
                break;

            case '"':
                pc->tend = pc->p - 1;
                pc->p++;
                pc->len--;
                return tt;

            case '[':
                JimParseSubCmd(pc);
                tt = JIM_TT_ESC;
                continue;

            case '\n':
                pc->linenr++;
                break;

            case '$':
                tt = JIM_TT_ESC;
                break;
        }
        pc->p++;
        pc->len--;
    }
    pc->missing.ch = '"';
    pc->missing.line = line;
    pc->tend = pc->p - 1;
    return tt;
}

/**
 * Parses a [command] expression starting at pc->p.
 *
 * Positions the parser at the end of the command expression,
 * sets pc->tend and possibly pc->missing.
 */
static void JimParseSubCmd(struct JimParserCtx *pc)
{
    int level = 1;
    int startofword = 1;
    int line = pc->tline;

    /* Skip the bracket */
    pc->p++;
    pc->len--;
    while (pc->len) {
        switch (*pc->p) {
            case '\\':
                if (pc->len > 1) {
                    if (*++pc->p == '\n') {
                        pc->linenr++;
                    }
                    pc->len--;
                }
                break;

            case '[':
                level++;
                break;

            case ']':
                if (--level == 0) {
                    pc->tend = pc->p - 1;
                    pc->p++;
                    pc->len--;
                    return;
                }
                break;

            case '"':
                if (startofword) {
                    JimParseSubQuote(pc);
                    continue;
                }
                break;

            case '{':
                JimParseSubBrace(pc);
                startofword = 0;
                continue;

            case '\n':
                pc->linenr++;
                break;
        }
        startofword = isspace(UCHAR(*pc->p));
        pc->p++;
        pc->len--;
    }
    pc->missing.ch = '[';
    pc->missing.line = line;
    pc->tend = pc->p - 1;
}

static int JimParseBrace(struct JimParserCtx *pc)
{
    pc->tstart = pc->p + 1;
    pc->tline = pc->linenr;
    pc->tt = JIM_TT_STR;
    JimParseSubBrace(pc);
    return JIM_OK;
}

static int JimParseCmd(struct JimParserCtx *pc)
{
    pc->tstart = pc->p + 1;
    pc->tline = pc->linenr;
    pc->tt = JIM_TT_CMD;
    JimParseSubCmd(pc);
    return JIM_OK;
}

static int JimParseQuote(struct JimParserCtx *pc)
{
    pc->tstart = pc->p + 1;
    pc->tline = pc->linenr;
    pc->tt = JimParseSubQuote(pc);
    return JIM_OK;
}

static int JimParseVar(struct JimParserCtx *pc)
{
    /* skip the $ */
    pc->p++;
    pc->len--;

#ifdef EXPRSUGAR_BRACKET
    if (*pc->p == '[') {
        /* Parse $[...] expr shorthand syntax */
        JimParseCmd(pc);
        pc->tt = JIM_TT_EXPRSUGAR;
        return JIM_OK;
    }
#endif

    pc->tstart = pc->p;
    pc->tt = JIM_TT_VAR;
    pc->tline = pc->linenr;

    if (*pc->p == '{') {
        pc->tstart = ++pc->p;
        pc->len--;

        while (pc->len && *pc->p != '}') {
            if (*pc->p == '\n') {
                pc->linenr++;
            }
            pc->p++;
            pc->len--;
        }
        pc->tend = pc->p - 1;
        if (pc->len) {
            pc->p++;
            pc->len--;
        }
    }
    else {
        while (1) {
            /* Skip double colon, but not single colon! */
            if (pc->p[0] == ':' && pc->p[1] == ':') {
                while (*pc->p == ':') {
                    pc->p++;
                    pc->len--;
                }
                continue;
            }
            /* Note that any char >= 0x80 must be part of a utf-8 char.
             * We consider all unicode points outside of ASCII as letters
             */
            if (isalnum(UCHAR(*pc->p)) || *pc->p == '_' || UCHAR(*pc->p) >= 0x80) {
                pc->p++;
                pc->len--;
                continue;
            }
            break;
        }
        /* Parse [dict get] syntax sugar. */
        if (*pc->p == '(') {
            int count = 1;
            const char *paren = NULL;

            pc->tt = JIM_TT_DICTSUGAR;

            while (count && pc->len) {
                pc->p++;
                pc->len--;
                if (*pc->p == '\\' && pc->len >= 1) {
                    pc->p++;
                    pc->len--;
                }
                else if (*pc->p == '(') {
                    count++;
                }
                else if (*pc->p == ')') {
                    paren = pc->p;
                    count--;
                }
            }
            if (count == 0) {
                pc->p++;
                pc->len--;
            }
            else if (paren) {
                /* Did not find a matching paren. Back up */
                paren++;
                pc->len += (pc->p - paren);
                pc->p = paren;
            }
#ifndef EXPRSUGAR_BRACKET
            if (*pc->tstart == '(') {
                pc->tt = JIM_TT_EXPRSUGAR;
            }
#endif
        }
        pc->tend = pc->p - 1;
    }
    /* Check if we parsed just the '$' character.
     * That's not a variable so an error is returned
     * to tell the state machine to consider this '$' just
     * a string. */
    if (pc->tstart == pc->p) {
        pc->p--;
        pc->len++;
        return JIM_ERR;
    }
    return JIM_OK;
}

static int JimParseStr(struct JimParserCtx *pc)
{
    if (pc->tt == JIM_TT_SEP || pc->tt == JIM_TT_EOL ||
        pc->tt == JIM_TT_NONE || pc->tt == JIM_TT_STR) {
        /* Starting a new word */
        if (*pc->p == '{') {
            return JimParseBrace(pc);
        }
        if (*pc->p == '"') {
            pc->inquote = 1;
            pc->p++;
            pc->len--;
            /* In case the end quote is missing */
            pc->missing.line = pc->tline;
        }
    }
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (1) {
        if (pc->len == 0) {
            if (pc->inquote) {
                pc->missing.ch = '"';
            }
            pc->tend = pc->p - 1;
            pc->tt = JIM_TT_ESC;
            return JIM_OK;
        }
        switch (*pc->p) {
            case '\\':
                if (!pc->inquote && *(pc->p + 1) == '\n') {
                    pc->tend = pc->p - 1;
                    pc->tt = JIM_TT_ESC;
                    return JIM_OK;
                }
                if (pc->len >= 2) {
                    if (*(pc->p + 1) == '\n') {
                        pc->linenr++;
                    }
                    pc->p++;
                    pc->len--;
                }
                else if (pc->len == 1) {
                    /* End of script with trailing backslash */
                    pc->missing.ch = '\\';
                }
                break;
            case '(':
                /* If the following token is not '$' just keep going */
                if (pc->len > 1 && pc->p[1] != '$') {
                    break;
                }
                /* fall through */
            case ')':
                /* Only need a separate ')' token if the previous was a var */
                if (*pc->p == '(' || pc->tt == JIM_TT_VAR) {
                    if (pc->p == pc->tstart) {
                        /* At the start of the token, so just return this char */
                        pc->p++;
                        pc->len--;
                    }
                    pc->tend = pc->p - 1;
                    pc->tt = JIM_TT_ESC;
                    return JIM_OK;
                }
                break;

            case '$':
            case '[':
                pc->tend = pc->p - 1;
                pc->tt = JIM_TT_ESC;
                return JIM_OK;
            case ' ':
            case '\t':
            case '\n':
            case '\r':
            case '\f':
            case ';':
                if (!pc->inquote) {
                    pc->tend = pc->p - 1;
                    pc->tt = JIM_TT_ESC;
                    return JIM_OK;
                }
                else if (*pc->p == '\n') {
                    pc->linenr++;
                }
                break;
            case '"':
                if (pc->inquote) {
                    pc->tend = pc->p - 1;
                    pc->tt = JIM_TT_ESC;
                    pc->p++;
                    pc->len--;
                    pc->inquote = 0;
                    return JIM_OK;
                }
                break;
        }
        pc->p++;
        pc->len--;
    }
    return JIM_OK;              /* unreached */
}

static int JimParseComment(struct JimParserCtx *pc)
{
    while (*pc->p) {
        if (*pc->p == '\\') {
            pc->p++;
            pc->len--;
            if (pc->len == 0) {
                pc->missing.ch = '\\';
                return JIM_OK;
            }
            if (*pc->p == '\n') {
                pc->linenr++;
            }
        }
        else if (*pc->p == '\n') {
            pc->p++;
            pc->len--;
            pc->linenr++;
            break;
        }
        pc->p++;
        pc->len--;
    }
    return JIM_OK;
}

/* xdigitval and odigitval are helper functions for JimEscape() */
static int xdigitval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static int odigitval(int c)
{
    if (c >= '0' && c <= '7')
        return c - '0';
    return -1;
}

/* Perform Tcl escape substitution of 's', storing the result
 * string into 'dest'. The escaped string is guaranteed to
 * be the same length or shorted than the source string.
 * Slen is the length of the string at 's'.
 *
 * The function returns the length of the resulting string. */
static int JimEscape(char *dest, const char *s, int slen)
{
    char *p = dest;
    int i, len;

    for (i = 0; i < slen; i++) {
        switch (s[i]) {
            case '\\':
                switch (s[i + 1]) {
                    case 'a':
                        *p++ = 0x7;
                        i++;
                        break;
                    case 'b':
                        *p++ = 0x8;
                        i++;
                        break;
                    case 'f':
                        *p++ = 0xc;
                        i++;
                        break;
                    case 'n':
                        *p++ = 0xa;
                        i++;
                        break;
                    case 'r':
                        *p++ = 0xd;
                        i++;
                        break;
                    case 't':
                        *p++ = 0x9;
                        i++;
                        break;
                    case 'u':
                    case 'U':
                    case 'x':
                        /* A unicode or hex sequence.
                         * \x Expect 1-2 hex chars and convert to hex.
                         * \u Expect 1-4 hex chars and convert to utf-8.
                         * \U Expect 1-8 hex chars and convert to utf-8.
                         * \u{NNN} supports 1-6 hex chars and convert to utf-8.
                         * An invalid sequence means simply the escaped char.
                         */
                        {
                            unsigned val = 0;
                            int k;
                            int maxchars = 2;

                            i++;

                            if (s[i] == 'U') {
                                maxchars = 8;
                            }
                            else if (s[i] == 'u') {
                                if (s[i + 1] == '{') {
                                    maxchars = 6;
                                    i++;
                                }
                                else {
                                    maxchars = 4;
                                }
                            }

                            for (k = 0; k < maxchars; k++) {
                                int c = xdigitval(s[i + k + 1]);
                                if (c == -1) {
                                    break;
                                }
                                val = (val << 4) | c;
                            }
                            /* The \u{nnn} syntax supports up to 21 bit codepoints. */
                            if (s[i] == '{') {
                                if (k == 0 || val > 0x1fffff || s[i + k + 1] != '}') {
                                    /* Back up */
                                    i--;
                                    k = 0;
                                }
                                else {
                                    /* Skip the closing brace */
                                    k++;
                                }
                            }
                            if (k) {
                                /* Got a valid sequence, so convert */
                                if (s[i] == 'x') {
                                    *p++ = val;
                                }
                                else {
                                    p += utf8_fromunicode(p, val);
                                }
                                i += k;
                                break;
                            }
                            /* Not a valid codepoint, just an escaped char */
                            *p++ = s[i];
                        }
                        break;
                    case 'v':
                        *p++ = 0xb;
                        i++;
                        break;
                    case '\0':
                        *p++ = '\\';
                        i++;
                        break;
                    case '\n':
                        /* Replace all spaces and tabs after backslash newline with a single space*/
                        *p++ = ' ';
                        do {
                            i++;
                        } while (s[i + 1] == ' ' || s[i + 1] == '\t');
                        break;
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                        /* octal escape */
                        {
                            int val = 0;
                            int c = odigitval(s[i + 1]);

                            val = c;
                            c = odigitval(s[i + 2]);
                            if (c == -1) {
                                *p++ = val;
                                i++;
                                break;
                            }
                            val = (val * 8) + c;
                            c = odigitval(s[i + 3]);
                            if (c == -1) {
                                *p++ = val;
                                i += 2;
                                break;
                            }
                            val = (val * 8) + c;
                            *p++ = val;
                            i += 3;
                        }
                        break;
                    default:
                        *p++ = s[i + 1];
                        i++;
                        break;
                }
                break;
            default:
                *p++ = s[i];
                break;
        }
    }
    len = p - dest;
    *p = '\0';
    return len;
}

/* Returns a dynamically allocated copy of the current token in the
 * parser context. The function performs conversion of escapes if
 * the token is of type JIM_TT_ESC.
 *
 * Note that after the conversion, tokens that are grouped with
 * braces in the source code, are always recognizable from the
 * identical string obtained in a different way from the type.
 *
 * For example the string:
 *
 * {*}$a
 *
 * will return as first token "*", of type JIM_TT_STR
 *
 * While the string:
 *
 * *$a
 *
 * will return as first token "*", of type JIM_TT_ESC
 */
static Jim_Obj *JimParserGetTokenObj(Jim_Interp *interp, struct JimParserCtx *pc)
{
    const char *start, *end;
    char *token;
    int len;

    start = pc->tstart;
    end = pc->tend;
    if (start > end) {
        len = 0;
        token = Jim_Alloc(1);
        token[0] = '\0';
    }
    else {
        len = (end - start) + 1;
        token = Jim_Alloc(len + 1);
        if (pc->tt != JIM_TT_ESC) {
            /* No escape conversion needed? Just copy it. */
            memcpy(token, start, len);
            token[len] = '\0';
        }
        else {
            /* Else convert the escape chars. */
            len = JimEscape(token, start, len);
        }
    }

    return Jim_NewStringObjNoAlloc(interp, token, len);
}

/* -----------------------------------------------------------------------------
 * Tcl Lists parsing
 * ---------------------------------------------------------------------------*/
static int JimParseListSep(struct JimParserCtx *pc);
static int JimParseListStr(struct JimParserCtx *pc);
static int JimParseListQuote(struct JimParserCtx *pc);

static int JimParseList(struct JimParserCtx *pc)
{
    if (isspace(UCHAR(*pc->p))) {
        return JimParseListSep(pc);
    }
    switch (*pc->p) {
        case '"':
            return JimParseListQuote(pc);

        case '{':
            return JimParseBrace(pc);

        default:
            if (pc->len) {
                return JimParseListStr(pc);
            }
            break;
    }

    pc->tstart = pc->tend = pc->p;
    pc->tline = pc->linenr;
    pc->tt = JIM_TT_EOL;
    pc->eof = 1;
    return JIM_OK;
}

static int JimParseListSep(struct JimParserCtx *pc)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (isspace(UCHAR(*pc->p))) {
        if (*pc->p == '\n') {
            pc->linenr++;
        }
        pc->p++;
        pc->len--;
    }
    pc->tend = pc->p - 1;
    pc->tt = JIM_TT_SEP;
    return JIM_OK;
}

static int JimParseListQuote(struct JimParserCtx *pc)
{
    pc->p++;
    pc->len--;

    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    pc->tt = JIM_TT_STR;

    while (pc->len) {
        switch (*pc->p) {
            case '\\':
                pc->tt = JIM_TT_ESC;
                if (--pc->len == 0) {
                    /* Trailing backslash */
                    pc->tend = pc->p;
                    return JIM_OK;
                }
                pc->p++;
                break;
            case '\n':
                pc->linenr++;
                break;
            case '"':
                pc->tend = pc->p - 1;
                pc->p++;
                pc->len--;
                return JIM_OK;
        }
        pc->p++;
        pc->len--;
    }

    pc->tend = pc->p - 1;
    return JIM_OK;
}

static int JimParseListStr(struct JimParserCtx *pc)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    pc->tt = JIM_TT_STR;

    while (pc->len) {
        if (isspace(UCHAR(*pc->p))) {
            pc->tend = pc->p - 1;
            return JIM_OK;
        }
        if (*pc->p == '\\') {
            if (--pc->len == 0) {
                /* Trailing backslash */
                pc->tend = pc->p;
                return JIM_OK;
            }
            pc->tt = JIM_TT_ESC;
            pc->p++;
        }
        pc->p++;
        pc->len--;
    }
    pc->tend = pc->p - 1;
    return JIM_OK;
}

/* -----------------------------------------------------------------------------
 * Jim_Obj related functions
 * ---------------------------------------------------------------------------*/

/* Return a new initialized object. */
Jim_Obj *Jim_NewObj(Jim_Interp *interp)
{
    Jim_Obj *objPtr;

    /* -- Check if there are objects in the free list -- */
    if (interp->freeList != NULL) {
        /* -- Unlink the object from the free list -- */
        objPtr = interp->freeList;
        interp->freeList = objPtr->nextObjPtr;
    }
    else {
        /* -- No ready to use objects: allocate a new one -- */
        objPtr = Jim_Alloc(sizeof(*objPtr));
    }

    /* Object is returned with refCount of 0. Every
     * kind of GC implemented should take care to don't try
     * to scan objects with refCount == 0. */
    objPtr->refCount = 0;
    /* All the other fields are left not initialized to save time.
     * The caller will probably want to set them to the right
     * value anyway. */

    /* -- Put the object into the live list -- */
    objPtr->prevObjPtr = NULL;
    objPtr->nextObjPtr = interp->liveList;
    if (interp->liveList)
        interp->liveList->prevObjPtr = objPtr;
    interp->liveList = objPtr;

    return objPtr;
}

/* Free an object. Actually objects are never freed, but
 * just moved to the free objects list, where they will be
 * reused by Jim_NewObj(). */
void Jim_FreeObj(Jim_Interp *interp, Jim_Obj *objPtr)
{
    /* Check if the object was already freed, panic. */
    JimPanic((objPtr->refCount != 0, "!!!Object %p freed with bad refcount %d, type=%s", objPtr,
        objPtr->refCount, objPtr->typePtr ? objPtr->typePtr->name : "<none>"));

    /* Free the internal representation */
    Jim_FreeIntRep(interp, objPtr);
    /* Free the string representation */
    if (objPtr->bytes != NULL) {
        if (objPtr->bytes != JimEmptyStringRep)
            Jim_Free(objPtr->bytes);
    }
    /* Unlink the object from the live objects list */
    if (objPtr->prevObjPtr)
        objPtr->prevObjPtr->nextObjPtr = objPtr->nextObjPtr;
    if (objPtr->nextObjPtr)
        objPtr->nextObjPtr->prevObjPtr = objPtr->prevObjPtr;
    if (interp->liveList == objPtr)
        interp->liveList = objPtr->nextObjPtr;
#ifdef JIM_DISABLE_OBJECT_POOL
    Jim_Free(objPtr);
#else
    /* Link the object into the free objects list */
    objPtr->prevObjPtr = NULL;
    objPtr->nextObjPtr = interp->freeList;
    if (interp->freeList)
        interp->freeList->prevObjPtr = objPtr;
    interp->freeList = objPtr;
    objPtr->refCount = -1;
#endif
}

/* Invalidate the string representation of an object. */
void Jim_InvalidateStringRep(Jim_Obj *objPtr)
{
    if (objPtr->bytes != NULL) {
        if (objPtr->bytes != JimEmptyStringRep)
            Jim_Free(objPtr->bytes);
    }
    objPtr->bytes = NULL;
}

/* Duplicate an object. The returned object has refcount = 0. */
Jim_Obj *Jim_DuplicateObj(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_Obj *dupPtr;

    dupPtr = Jim_NewObj(interp);
    if (objPtr->bytes == NULL) {
        /* Object does not have a valid string representation. */
        dupPtr->bytes = NULL;
    }
    else if (objPtr->length == 0) {
        /* Zero length, so don't even bother with the type-specific dup, since all zero length objects look the same */
        dupPtr->bytes = JimEmptyStringRep;
        dupPtr->length = 0;
        dupPtr->typePtr = NULL;
        return dupPtr;
    }
    else {
        dupPtr->bytes = Jim_Alloc(objPtr->length + 1);
        dupPtr->length = objPtr->length;
        /* Copy the null byte too */
        memcpy(dupPtr->bytes, objPtr->bytes, objPtr->length + 1);
    }

    /* By default, the new object has the same type as the old object */
    dupPtr->typePtr = objPtr->typePtr;
    if (objPtr->typePtr != NULL) {
        if (objPtr->typePtr->dupIntRepProc == NULL) {
            dupPtr->internalRep = objPtr->internalRep;
        }
        else {
            /* The dup proc may set a different type, e.g. NULL */
            objPtr->typePtr->dupIntRepProc(interp, objPtr, dupPtr);
        }
    }
    return dupPtr;
}

/* Return the string representation for objPtr. If the object's
 * string representation is invalid, calls the updateStringProc method to create
 * a new one from the internal representation of the object.
 */
const char *Jim_GetString(Jim_Obj *objPtr, int *lenPtr)
{
    if (objPtr->bytes == NULL) {
        /* Invalid string repr. Generate it. */
        JimPanic((objPtr->typePtr->updateStringProc == NULL, "UpdateStringProc called against '%s' type.", objPtr->typePtr->name));
        objPtr->typePtr->updateStringProc(objPtr);
    }
    if (lenPtr)
        *lenPtr = objPtr->length;
    return objPtr->bytes;
}

/* Just returns the length of the object's string rep */
int Jim_Length(Jim_Obj *objPtr)
{
    if (objPtr->bytes == NULL) {
        /* Invalid string repr. Generate it. */
        JimPanic((objPtr->typePtr->updateStringProc == NULL, "UpdateStringProc called against '%s' type.", objPtr->typePtr->name));
        objPtr->typePtr->updateStringProc(objPtr);
    }
    return objPtr->length;
}

/* Just returns object's string rep */
const char *Jim_String(Jim_Obj *objPtr)
{
    if (objPtr->bytes == NULL) {
        /* Invalid string repr. Generate it. */
        JimPanic((objPtr->typePtr == NULL, "UpdateStringProc called against typeless value."));
        JimPanic((objPtr->typePtr->updateStringProc == NULL, "UpdateStringProc called against '%s' type.", objPtr->typePtr->name));
        objPtr->typePtr->updateStringProc(objPtr);
    }
    return objPtr->bytes;
}

static void JimSetStringBytes(Jim_Obj *objPtr, const char *str)
{
    objPtr->bytes = Jim_StrDup(str);
    objPtr->length = strlen(str);
}

static void FreeDictSubstInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupDictSubstInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);

static const Jim_ObjType dictSubstObjType = {
    "dict-substitution",
    FreeDictSubstInternalRep,
    DupDictSubstInternalRep,
    NULL,
    JIM_TYPE_NONE,
};

static void FreeInterpolatedInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_DecrRefCount(interp, objPtr->internalRep.dictSubstValue.indexObjPtr);
}

static const Jim_ObjType interpolatedObjType = {
    "interpolated",
    FreeInterpolatedInternalRep,
    NULL,
    NULL,
    JIM_TYPE_NONE,
};

/* -----------------------------------------------------------------------------
 * String Object
 * ---------------------------------------------------------------------------*/
static void DupStringInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static int SetStringFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

static const Jim_ObjType stringObjType = {
    "string",
    NULL,
    DupStringInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

static void DupStringInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    JIM_NOTUSED(interp);

    /* This is a bit subtle: the only caller of this function
     * should be Jim_DuplicateObj(), that will copy the
     * string representaion. After the copy, the duplicated
     * object will not have more room in the buffer than
     * srcPtr->length bytes. So we just set it to length. */
    dupPtr->internalRep.strValue.maxLength = srcPtr->length;
    dupPtr->internalRep.strValue.charLength = srcPtr->internalRep.strValue.charLength;
}

static int SetStringFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (objPtr->typePtr != &stringObjType) {
        /* Get a fresh string representation. */
        if (objPtr->bytes == NULL) {
            /* Invalid string repr. Generate it. */
            JimPanic((objPtr->typePtr->updateStringProc == NULL, "UpdateStringProc called against '%s' type.", objPtr->typePtr->name));
            objPtr->typePtr->updateStringProc(objPtr);
        }
        /* Free any other internal representation. */
        Jim_FreeIntRep(interp, objPtr);
        /* Set it as string, i.e. just set the maxLength field. */
        objPtr->typePtr = &stringObjType;
        objPtr->internalRep.strValue.maxLength = objPtr->length;
        /* Don't know the utf-8 length yet */
        objPtr->internalRep.strValue.charLength = -1;
    }
    return JIM_OK;
}

/**
 * Returns the length of the object string in chars, not bytes.
 *
 * These may be different for a utf-8 string.
 */
int Jim_Utf8Length(Jim_Interp *interp, Jim_Obj *objPtr)
{
#ifdef JIM_UTF8
    SetStringFromAny(interp, objPtr);

    if (objPtr->internalRep.strValue.charLength < 0) {
        objPtr->internalRep.strValue.charLength = utf8_strlen(objPtr->bytes, objPtr->length);
    }
    return objPtr->internalRep.strValue.charLength;
#else
    return Jim_Length(objPtr);
#endif
}

/* len is in bytes -- see also Jim_NewStringObjUtf8() */
Jim_Obj *Jim_NewStringObj(Jim_Interp *interp, const char *s, int len)
{
    Jim_Obj *objPtr = Jim_NewObj(interp);

    /* Need to find out how many bytes the string requires */
    if (len == -1)
        len = strlen(s);
    /* Alloc/Set the string rep. */
    if (len == 0) {
        objPtr->bytes = JimEmptyStringRep;
    }
    else {
        objPtr->bytes = Jim_Alloc(len + 1);
        memcpy(objPtr->bytes, s, len);
        objPtr->bytes[len] = '\0';
    }
    objPtr->length = len;

    /* No typePtr field for the vanilla string object. */
    objPtr->typePtr = NULL;
    return objPtr;
}

/* charlen is in characters -- see also Jim_NewStringObj() */
Jim_Obj *Jim_NewStringObjUtf8(Jim_Interp *interp, const char *s, int charlen)
{
#ifdef JIM_UTF8
    /* Need to find out how many bytes the string requires */
    int bytelen = utf8_index(s, charlen);

    Jim_Obj *objPtr = Jim_NewStringObj(interp, s, bytelen);

    /* Remember the utf8 length, so set the type */
    objPtr->typePtr = &stringObjType;
    objPtr->internalRep.strValue.maxLength = bytelen;
    objPtr->internalRep.strValue.charLength = charlen;

    return objPtr;
#else
    return Jim_NewStringObj(interp, s, charlen);
#endif
}

/* This version does not try to duplicate the 's' pointer, but
 * use it directly. */
Jim_Obj *Jim_NewStringObjNoAlloc(Jim_Interp *interp, char *s, int len)
{
    Jim_Obj *objPtr = Jim_NewObj(interp);

    objPtr->bytes = s;
    objPtr->length = (len == -1) ? strlen(s) : len;
    objPtr->typePtr = NULL;
    return objPtr;
}

/* Low-level string append. Use it only against unshared objects
 * of type "string". */
static void StringAppendString(Jim_Obj *objPtr, const char *str, int len)
{
    int needlen;

    if (len == -1)
        len = strlen(str);
    needlen = objPtr->length + len;
    if (objPtr->internalRep.strValue.maxLength < needlen ||
        objPtr->internalRep.strValue.maxLength == 0) {
        needlen *= 2;
        /* Inefficient to malloc() for less than 8 bytes */
        if (needlen < 7) {
            needlen = 7;
        }
        if (objPtr->bytes == JimEmptyStringRep) {
            objPtr->bytes = Jim_Alloc(needlen + 1);
        }
        else {
            objPtr->bytes = Jim_Realloc(objPtr->bytes, needlen + 1);
        }
        objPtr->internalRep.strValue.maxLength = needlen;
    }
    memcpy(objPtr->bytes + objPtr->length, str, len);
    objPtr->bytes[objPtr->length + len] = '\0';

    if (objPtr->internalRep.strValue.charLength >= 0) {
        /* Update the utf-8 char length */
        objPtr->internalRep.strValue.charLength += utf8_strlen(objPtr->bytes + objPtr->length, len);
    }
    objPtr->length += len;
}

/* Higher level API to append strings to objects.
 * Object must not be unshared for each of these.
 */
void Jim_AppendString(Jim_Interp *interp, Jim_Obj *objPtr, const char *str, int len)
{
    JimPanic((Jim_IsShared(objPtr), "Jim_AppendString called with shared object"));
    SetStringFromAny(interp, objPtr);
    StringAppendString(objPtr, str, len);
}

void Jim_AppendObj(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *appendObjPtr)
{
    int len;
    const char *str = Jim_GetString(appendObjPtr, &len);
    Jim_AppendString(interp, objPtr, str, len);
}

void Jim_AppendStrings(Jim_Interp *interp, Jim_Obj *objPtr, ...)
{
    va_list ap;

    SetStringFromAny(interp, objPtr);
    va_start(ap, objPtr);
    while (1) {
        const char *s = va_arg(ap, const char *);

        if (s == NULL)
            break;
        Jim_AppendString(interp, objPtr, s, -1);
    }
    va_end(ap);
}

int Jim_StringEqObj(Jim_Obj *aObjPtr, Jim_Obj *bObjPtr)
{
    if (aObjPtr == bObjPtr) {
        return 1;
    }
    else {
        int Alen, Blen;
        const char *sA = Jim_GetString(aObjPtr, &Alen);
        const char *sB = Jim_GetString(bObjPtr, &Blen);

        return Alen == Blen && memcmp(sA, sB, Alen) == 0;
    }
}

/**
 * Note. Does not support embedded nulls in either the pattern or the object.
 */
int Jim_StringMatchObj(Jim_Interp *interp, Jim_Obj *patternObjPtr, Jim_Obj *objPtr, int nocase)
{
    return JimGlobMatch(Jim_String(patternObjPtr), Jim_String(objPtr), nocase);
}

/*
 * Note: does not support embedded nulls for the nocase option.
 */
int Jim_StringCompareObj(Jim_Interp *interp, Jim_Obj *firstObjPtr, Jim_Obj *secondObjPtr, int nocase)
{
    int l1, l2;
    const char *s1 = Jim_GetString(firstObjPtr, &l1);
    const char *s2 = Jim_GetString(secondObjPtr, &l2);

    if (nocase) {
        /* Do a character compare for nocase */
        return JimStringCompareLen(s1, s2, -1, nocase);
    }
    return JimStringCompare(s1, l1, s2, l2);
}

/**
 * Like Jim_StringCompareObj() except compares to a maximum of the length of firstObjPtr.
 *
 * Note: does not support embedded nulls
 */
int Jim_StringCompareLenObj(Jim_Interp *interp, Jim_Obj *firstObjPtr, Jim_Obj *secondObjPtr, int nocase)
{
    const char *s1 = Jim_String(firstObjPtr);
    const char *s2 = Jim_String(secondObjPtr);

    return JimStringCompareLen(s1, s2, Jim_Utf8Length(interp, firstObjPtr), nocase);
}

/* Convert a range, as returned by Jim_GetRange(), into
 * an absolute index into an object of the specified length.
 * This function may return negative values, or values
 * greater than or equal to the length of the list if the index
 * is out of range. */
static int JimRelToAbsIndex(int len, int idx)
{
    if (idx < 0)
        return len + idx;
    return idx;
}

/* Convert a pair of indexes (*firstPtr, *lastPtr) as normalized by JimRelToAbsIndex(),
 * into a form suitable for implementation of commands like [string range] and [lrange].
 *
 * The resulting range is guaranteed to address valid elements of
 * the structure.
 */
static void JimRelToAbsRange(int len, int *firstPtr, int *lastPtr, int *rangeLenPtr)
{
    int rangeLen;

    if (*firstPtr > *lastPtr) {
        rangeLen = 0;
    }
    else {
        rangeLen = *lastPtr - *firstPtr + 1;
        if (rangeLen) {
            if (*firstPtr < 0) {
                rangeLen += *firstPtr;
                *firstPtr = 0;
            }
            if (*lastPtr >= len) {
                rangeLen -= (*lastPtr - (len - 1));
                *lastPtr = len - 1;
            }
        }
    }
    if (rangeLen < 0)
        rangeLen = 0;

    *rangeLenPtr = rangeLen;
}

static int JimStringGetRange(Jim_Interp *interp, Jim_Obj *firstObjPtr, Jim_Obj *lastObjPtr,
    int len, int *first, int *last, int *range)
{
    if (Jim_GetIndex(interp, firstObjPtr, first) != JIM_OK) {
        return JIM_ERR;
    }
    if (Jim_GetIndex(interp, lastObjPtr, last) != JIM_OK) {
        return JIM_ERR;
    }
    *first = JimRelToAbsIndex(len, *first);
    *last = JimRelToAbsIndex(len, *last);
    JimRelToAbsRange(len, first, last, range);
    return JIM_OK;
}

Jim_Obj *Jim_StringByteRangeObj(Jim_Interp *interp,
    Jim_Obj *strObjPtr, Jim_Obj *firstObjPtr, Jim_Obj *lastObjPtr)
{
    int first, last;
    const char *str;
    int rangeLen;
    int bytelen;

    str = Jim_GetString(strObjPtr, &bytelen);

    if (JimStringGetRange(interp, firstObjPtr, lastObjPtr, bytelen, &first, &last, &rangeLen) != JIM_OK) {
        return NULL;
    }

    if (first == 0 && rangeLen == bytelen) {
        return strObjPtr;
    }
    return Jim_NewStringObj(interp, str + first, rangeLen);
}

Jim_Obj *Jim_StringRangeObj(Jim_Interp *interp,
    Jim_Obj *strObjPtr, Jim_Obj *firstObjPtr, Jim_Obj *lastObjPtr)
{
#ifdef JIM_UTF8
    int first, last;
    const char *str;
    int len, rangeLen;
    int bytelen;

    str = Jim_GetString(strObjPtr, &bytelen);
    len = Jim_Utf8Length(interp, strObjPtr);

    if (JimStringGetRange(interp, firstObjPtr, lastObjPtr, len, &first, &last, &rangeLen) != JIM_OK) {
        return NULL;
    }

    if (first == 0 && rangeLen == len) {
        return strObjPtr;
    }
    if (len == bytelen) {
        /* ASCII optimisation */
        return Jim_NewStringObj(interp, str + first, rangeLen);
    }
    return Jim_NewStringObjUtf8(interp, str + utf8_index(str, first), rangeLen);
#else
    return Jim_StringByteRangeObj(interp, strObjPtr, firstObjPtr, lastObjPtr);
#endif
}

Jim_Obj *JimStringReplaceObj(Jim_Interp *interp,
    Jim_Obj *strObjPtr, Jim_Obj *firstObjPtr, Jim_Obj *lastObjPtr, Jim_Obj *newStrObj)
{
    int first, last;
    const char *str;
    int len, rangeLen;
    Jim_Obj *objPtr;

    len = Jim_Utf8Length(interp, strObjPtr);

    if (JimStringGetRange(interp, firstObjPtr, lastObjPtr, len, &first, &last, &rangeLen) != JIM_OK) {
        return NULL;
    }

    if (last < first) {
        return strObjPtr;
    }

    str = Jim_String(strObjPtr);

    /* Before part */
    objPtr = Jim_NewStringObjUtf8(interp, str, first);

    /* Replacement */
    if (newStrObj) {
        Jim_AppendObj(interp, objPtr, newStrObj);
    }

    /* After part */
    Jim_AppendString(interp, objPtr, str + utf8_index(str, last + 1), len - last - 1);

    return objPtr;
}

/**
 * Note: does not support embedded nulls.
 */
static void JimStrCopyUpperLower(char *dest, const char *str, int uc)
{
    while (*str) {
        int c;
        str += utf8_tounicode(str, &c);
        dest += utf8_getchars(dest, uc ? utf8_upper(c) : utf8_lower(c));
    }
    *dest = 0;
}

/**
 * Note: does not support embedded nulls.
 */
static Jim_Obj *JimStringToLower(Jim_Interp *interp, Jim_Obj *strObjPtr)
{
    char *buf;
    int len;
    const char *str;

    SetStringFromAny(interp, strObjPtr);

    str = Jim_GetString(strObjPtr, &len);

#ifdef JIM_UTF8
    /* Case mapping can change the utf-8 length of the string.
     * But at worst it will be by one extra byte per char
     */
    len *= 2;
#endif
    buf = Jim_Alloc(len + 1);
    JimStrCopyUpperLower(buf, str, 0);
    return Jim_NewStringObjNoAlloc(interp, buf, -1);
}

/**
 * Note: does not support embedded nulls.
 */
static Jim_Obj *JimStringToUpper(Jim_Interp *interp, Jim_Obj *strObjPtr)
{
    char *buf;
    const char *str;
    int len;

    if (strObjPtr->typePtr != &stringObjType) {
        SetStringFromAny(interp, strObjPtr);
    }

    str = Jim_GetString(strObjPtr, &len);

#ifdef JIM_UTF8
    /* Case mapping can change the utf-8 length of the string.
     * But at worst it will be by one extra byte per char
     */
    len *= 2;
#endif
    buf = Jim_Alloc(len + 1);
    JimStrCopyUpperLower(buf, str, 1);
    return Jim_NewStringObjNoAlloc(interp, buf, -1);
}

/**
 * Note: does not support embedded nulls.
 */
static Jim_Obj *JimStringToTitle(Jim_Interp *interp, Jim_Obj *strObjPtr)
{
    char *buf, *p;
    int len;
    int c;
    const char *str;

    str = Jim_GetString(strObjPtr, &len);
    if (len == 0) {
        return strObjPtr;
    }
#ifdef JIM_UTF8
    /* Case mapping can change the utf-8 length of the string.
     * But at worst it will be by one extra byte per char
     */
    len *= 2;
#endif
    buf = p = Jim_Alloc(len + 1);

    str += utf8_tounicode(str, &c);
    p += utf8_getchars(p, utf8_title(c));

    JimStrCopyUpperLower(p, str, 0);

    return Jim_NewStringObjNoAlloc(interp, buf, -1);
}

/* Similar to memchr() except searches a UTF-8 string 'str' of byte length 'len'
 * for unicode character 'c'.
 * Returns the position if found or NULL if not
 */
static const char *utf8_memchr(const char *str, int len, int c)
{
#ifdef JIM_UTF8
    while (len) {
        int sc;
        int n = utf8_tounicode(str, &sc);
        if (sc == c) {
            return str;
        }
        str += n;
        len -= n;
    }
    return NULL;
#else
    return memchr(str, c, len);
#endif
}

/**
 * Searches for the first non-trim char in string (str, len)
 *
 * If none is found, returns just past the last char.
 *
 * Lengths are in bytes.
 */
static const char *JimFindTrimLeft(const char *str, int len, const char *trimchars, int trimlen)
{
    while (len) {
        int c;
        int n = utf8_tounicode(str, &c);

        if (utf8_memchr(trimchars, trimlen, c) == NULL) {
            /* Not a trim char, so stop */
            break;
        }
        str += n;
        len -= n;
    }
    return str;
}

/**
 * Searches backwards for a non-trim char in string (str, len).
 *
 * Returns a pointer to just after the non-trim char, or NULL if not found.
 *
 * Lengths are in bytes.
 */
static const char *JimFindTrimRight(const char *str, int len, const char *trimchars, int trimlen)
{
    str += len;

    while (len) {
        int c;
        int n = utf8_prev_len(str, len);

        len -= n;
        str -= n;

        n = utf8_tounicode(str, &c);

        if (utf8_memchr(trimchars, trimlen, c) == NULL) {
            return str + n;
        }
    }

    return NULL;
}

static const char default_trim_chars[] = " \t\n\r";
/* sizeof() here includes the null byte */
static int default_trim_chars_len = sizeof(default_trim_chars);

static Jim_Obj *JimStringTrimLeft(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *trimcharsObjPtr)
{
    int len;
    const char *str = Jim_GetString(strObjPtr, &len);
    const char *trimchars = default_trim_chars;
    int trimcharslen = default_trim_chars_len;
    const char *newstr;

    if (trimcharsObjPtr) {
        trimchars = Jim_GetString(trimcharsObjPtr, &trimcharslen);
    }

    newstr = JimFindTrimLeft(str, len, trimchars, trimcharslen);
    if (newstr == str) {
        return strObjPtr;
    }

    return Jim_NewStringObj(interp, newstr, len - (newstr - str));
}

static Jim_Obj *JimStringTrimRight(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *trimcharsObjPtr)
{
    int len;
    const char *trimchars = default_trim_chars;
    int trimcharslen = default_trim_chars_len;
    const char *nontrim;

    if (trimcharsObjPtr) {
        trimchars = Jim_GetString(trimcharsObjPtr, &trimcharslen);
    }

    SetStringFromAny(interp, strObjPtr);

    len = Jim_Length(strObjPtr);
    nontrim = JimFindTrimRight(strObjPtr->bytes, len, trimchars, trimcharslen);

    if (nontrim == NULL) {
        /* All trim, so return a zero-length string */
        return Jim_NewEmptyStringObj(interp);
    }
    if (nontrim == strObjPtr->bytes + len) {
        /* All non-trim, so return the original object */
        return strObjPtr;
    }

    if (Jim_IsShared(strObjPtr)) {
        strObjPtr = Jim_NewStringObj(interp, strObjPtr->bytes, (nontrim - strObjPtr->bytes));
    }
    else {
        /* Can modify this string in place */
        strObjPtr->bytes[nontrim - strObjPtr->bytes] = 0;
        strObjPtr->length = (nontrim - strObjPtr->bytes);
    }

    return strObjPtr;
}

static Jim_Obj *JimStringTrim(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *trimcharsObjPtr)
{
    /* First trim left. */
    Jim_Obj *objPtr = JimStringTrimLeft(interp, strObjPtr, trimcharsObjPtr);

    /* Now trim right */
    strObjPtr = JimStringTrimRight(interp, objPtr, trimcharsObjPtr);

    /* Note: refCount check is needed since objPtr may be emptyObj */
    if (objPtr != strObjPtr && objPtr->refCount == 0) {
        /* We don't want this object to be leaked */
        Jim_FreeNewObj(interp, objPtr);
    }

    return strObjPtr;
}

/* Some platforms don't have isascii - need a non-macro version */
#ifdef HAVE_ISASCII
#define jim_isascii isascii
#else
static int jim_isascii(int c)
{
    return !(c & ~0x7f);
}
#endif

static int JimStringIs(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *strClass, int strict)
{
    static const char * const strclassnames[] = {
        "integer", "alpha", "alnum", "ascii", "digit",
        "double", "lower", "upper", "space", "xdigit",
        "control", "print", "graph", "punct",
        NULL
    };
    enum {
        STR_IS_INTEGER, STR_IS_ALPHA, STR_IS_ALNUM, STR_IS_ASCII, STR_IS_DIGIT,
        STR_IS_DOUBLE, STR_IS_LOWER, STR_IS_UPPER, STR_IS_SPACE, STR_IS_XDIGIT,
        STR_IS_CONTROL, STR_IS_PRINT, STR_IS_GRAPH, STR_IS_PUNCT
    };
    int strclass;
    int len;
    int i;
    const char *str;
    int (*isclassfunc)(int c) = NULL;

    if (Jim_GetEnum(interp, strClass, strclassnames, &strclass, "class", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
        return JIM_ERR;
    }

    str = Jim_GetString(strObjPtr, &len);
    if (len == 0) {
        Jim_SetResultBool(interp, !strict);
        return JIM_OK;
    }

    switch (strclass) {
        case STR_IS_INTEGER:
            {
                jim_wide w;
                Jim_SetResultBool(interp, JimGetWideNoErr(interp, strObjPtr, &w) == JIM_OK);
                return JIM_OK;
            }

        case STR_IS_DOUBLE:
            {
                double d;
                Jim_SetResultBool(interp, Jim_GetDouble(interp, strObjPtr, &d) == JIM_OK && errno != ERANGE);
                return JIM_OK;
            }

        case STR_IS_ALPHA: isclassfunc = isalpha; break;
        case STR_IS_ALNUM: isclassfunc = isalnum; break;
        case STR_IS_ASCII: isclassfunc = jim_isascii; break;
        case STR_IS_DIGIT: isclassfunc = isdigit; break;
        case STR_IS_LOWER: isclassfunc = islower; break;
        case STR_IS_UPPER: isclassfunc = isupper; break;
        case STR_IS_SPACE: isclassfunc = isspace; break;
        case STR_IS_XDIGIT: isclassfunc = isxdigit; break;
        case STR_IS_CONTROL: isclassfunc = iscntrl; break;
        case STR_IS_PRINT: isclassfunc = isprint; break;
        case STR_IS_GRAPH: isclassfunc = isgraph; break;
        case STR_IS_PUNCT: isclassfunc = ispunct; break;
        default:
            return JIM_ERR;
    }

    for (i = 0; i < len; i++) {
        if (!isclassfunc(str[i])) {
            Jim_SetResultBool(interp, 0);
            return JIM_OK;
        }
    }
    Jim_SetResultBool(interp, 1);
    return JIM_OK;
}

/* -----------------------------------------------------------------------------
 * Compared String Object
 * ---------------------------------------------------------------------------*/

/* This is strange object that allows comparison of a C literal string
 * with a Jim object in a very short time if the same comparison is done
 * multiple times. For example every time the [if] command is executed,
 * Jim has to check if a given argument is "else".
 * If the code has no errors, this comparison is true most of the time,
 * so we can cache the pointer of the string of the last matching
 * comparison inside the object. Because most C compilers perform literal sharing,
 * so that: char *x = "foo", char *y = "foo", will lead to x == y,
 * this works pretty well even if comparisons are at different places
 * inside the C code. */

static const Jim_ObjType comparedStringObjType = {
    "compared-string",
    NULL,
    NULL,
    NULL,
    JIM_TYPE_REFERENCES,
};

/* The only way this object is exposed to the API is via the following
 * function. Returns true if the string and the object string repr.
 * are the same, otherwise zero is returned.
 *
 * Note: this isn't binary safe, but it hardly needs to be.*/
int Jim_CompareStringImmediate(Jim_Interp *interp, Jim_Obj *objPtr, const char *str)
{
    if (objPtr->typePtr == &comparedStringObjType && objPtr->internalRep.ptr == str) {
        return 1;
    }
    else {
        const char *objStr = Jim_String(objPtr);

        if (strcmp(str, objStr) != 0)
            return 0;

        if (objPtr->typePtr != &comparedStringObjType) {
            Jim_FreeIntRep(interp, objPtr);
            objPtr->typePtr = &comparedStringObjType;
        }
        objPtr->internalRep.ptr = (char *)str;  /*ATTENTION: const cast */
        return 1;
    }
}

static int qsortCompareStringPointers(const void *a, const void *b)
{
    char *const *sa = (char *const *)a;
    char *const *sb = (char *const *)b;

    return strcmp(*sa, *sb);
}


/* -----------------------------------------------------------------------------
 * Source Object
 *
 * This object is just a string from the language point of view, but
 * the internal representation contains the filename and line number
 * where this token was read. This information is used by
 * Jim_EvalObj() if the object passed happens to be of type "source".
 *
 * This allows propagation of the information about line numbers and file
 * names and gives error messages with absolute line numbers.
 *
 * Note that this object uses the internal representation of the Jim_Object,
 * so there is almost no memory overhead. (One Jim_Obj for each filename).
 *
 * Also the object will be converted to something else if the given
 * token it represents in the source file is not something to be
 * evaluated (not a script), and will be specialized in some other way,
 * so the time overhead is also almost zero.
 * ---------------------------------------------------------------------------*/

static void FreeSourceInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupSourceInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);

static const Jim_ObjType sourceObjType = {
    "source",
    FreeSourceInternalRep,
    DupSourceInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

void FreeSourceInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_DecrRefCount(interp, objPtr->internalRep.sourceValue.fileNameObj);
}

void DupSourceInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    dupPtr->internalRep.sourceValue = srcPtr->internalRep.sourceValue;
    Jim_IncrRefCount(dupPtr->internalRep.sourceValue.fileNameObj);
}

static void JimSetSourceInfo(Jim_Interp *interp, Jim_Obj *objPtr,
    Jim_Obj *fileNameObj, int lineNumber)
{
    JimPanic((Jim_IsShared(objPtr), "JimSetSourceInfo called with shared object"));
    JimPanic((objPtr->typePtr != NULL, "JimSetSourceInfo called with typed object"));
    Jim_IncrRefCount(fileNameObj);
    objPtr->internalRep.sourceValue.fileNameObj = fileNameObj;
    objPtr->internalRep.sourceValue.lineNumber = lineNumber;
    objPtr->typePtr = &sourceObjType;
}

/* -----------------------------------------------------------------------------
 * ScriptLine Object
 *
 * This object is used only in the Script internal represenation.
 * For each line of the script, it holds the number of tokens on the line
 * and the source line number.
 */
static const Jim_ObjType scriptLineObjType = {
    "scriptline",
    NULL,
    NULL,
    NULL,
    JIM_NONE,
};

static Jim_Obj *JimNewScriptLineObj(Jim_Interp *interp, int argc, int line)
{
    Jim_Obj *objPtr;

#ifdef DEBUG_SHOW_SCRIPT
    char buf[100];
    snprintf(buf, sizeof(buf), "line=%d, argc=%d", line, argc);
    objPtr = Jim_NewStringObj(interp, buf, -1);
#else
    objPtr = Jim_NewEmptyStringObj(interp);
#endif
    objPtr->typePtr = &scriptLineObjType;
    objPtr->internalRep.scriptLineValue.argc = argc;
    objPtr->internalRep.scriptLineValue.line = line;

    return objPtr;
}

/* -----------------------------------------------------------------------------
 * Script Object
 *
 * This object holds the parsed internal representation of a script.
 * This representation is help within an allocated ScriptObj (see below)
 */
static void FreeScriptInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupScriptInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);

static const Jim_ObjType scriptObjType = {
    "script",
    FreeScriptInternalRep,
    DupScriptInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

/* Each token of a script is represented by a ScriptToken.
 * The ScriptToken contains a type and a Jim_Obj. The Jim_Obj
 * can be specialized by commands operating on it.
 */
typedef struct ScriptToken
{
    Jim_Obj *objPtr;
    int type;
} ScriptToken;

/* This is the script object internal representation. An array of
 * ScriptToken structures, including a pre-computed representation of the
 * command length and arguments.
 *
 * For example the script:
 *
 * puts hello
 * set $i $x$y [foo]BAR
 *
 * will produce a ScriptObj with the following ScriptToken's:
 *
 * LIN 2
 * ESC puts
 * ESC hello
 * LIN 4
 * ESC set
 * VAR i
 * WRD 2
 * VAR x
 * VAR y
 * WRD 2
 * CMD foo
 * ESC BAR
 *
 * "puts hello" has two args (LIN 2), composed of single tokens.
 * (Note that the WRD token is omitted for the common case of a single token.)
 *
 * "set $i $x$y [foo]BAR" has four (LIN 4) args, the first word
 * has 1 token (ESC SET), and the last has two tokens (WRD 2 CMD foo ESC BAR)
 *
 * The precomputation of the command structure makes Jim_Eval() faster,
 * and simpler because there aren't dynamic lengths / allocations.
 *
 * -- {expand}/{*} handling --
 *
 * Expand is handled in a special way.
 *
 *   If a "word" begins with {*}, the word token count is -ve.
 *
 * For example the command:
 *
 * list {*}{a b}
 *
 * Will produce the following cmdstruct array:
 *
 * LIN 2
 * ESC list
 * WRD -1
 * STR a b
 *
 * Note that the 'LIN' token also contains the source information for the
 * first word of the line for error reporting purposes
 *
 * -- the substFlags field of the structure --
 *
 * The scriptObj structure is used to represent both "script" objects
 * and "subst" objects. In the second case, there are no LIN and WRD
 * tokens. Instead SEP and EOL tokens are added as-is.
 * In addition, the field 'substFlags' is used to represent the flags used to turn
 * the string into the internal representation.
 * If these flags do not match what the application requires,
 * the scriptObj is created again. For example the script:
 *
 * subst -nocommands $string
 * subst -novariables $string
 *
 * Will (re)create the internal representation of the $string object
 * two times.
 */
typedef struct ScriptObj
{
    ScriptToken *token;         /* Tokens array. */
    Jim_Obj *fileNameObj;       /* Filename */
    int len;                    /* Length of token[] */
    int substFlags;             /* flags used for the compilation of "subst" objects */
    int inUse;                  /* Used to share a ScriptObj. Currently
                                   only used by Jim_EvalObj() as protection against
                                   shimmering of the currently evaluated object. */
    int firstline;              /* Line number of the first line */
    int linenr;                 /* Error line number, if any */
    int missing;                /* Missing char if script failed to parse, (or space or backslash if OK) */
} ScriptObj;

static void JimSetScriptFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);
static int JimParseCheckMissing(Jim_Interp *interp, int ch);
static ScriptObj *JimGetScript(Jim_Interp *interp, Jim_Obj *objPtr);

void FreeScriptInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int i;
    struct ScriptObj *script = (void *)objPtr->internalRep.ptr;

    if (--script->inUse != 0)
        return;
    for (i = 0; i < script->len; i++) {
        Jim_DecrRefCount(interp, script->token[i].objPtr);
    }
    Jim_Free(script->token);
    Jim_DecrRefCount(interp, script->fileNameObj);
    Jim_Free(script);
}

void DupScriptInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    JIM_NOTUSED(interp);
    JIM_NOTUSED(srcPtr);

    /* Just return a simple string. We don't try to preserve the source info
     * since in practice scripts are never duplicated
     */
    dupPtr->typePtr = NULL;
}

/* A simple parse token.
 * As the script is parsed, the created tokens point into the script string rep.
 */
typedef struct
{
    const char *token;          /* Pointer to the start of the token */
    int len;                    /* Length of this token */
    int type;                   /* Token type */
    int line;                   /* Line number */
} ParseToken;

/* A list of parsed tokens representing a script.
 * Tokens are added to this list as the script is parsed.
 * It grows as needed.
 */
typedef struct
{
    /* Start with a statically allocated list of tokens which will be expanded with realloc if needed */
    ParseToken *list;           /* Array of tokens */
    int size;                   /* Current size of the list */
    int count;                  /* Number of entries used */
    ParseToken static_list[20]; /* Small initial token space to avoid allocation */
} ParseTokenList;

static void ScriptTokenListInit(ParseTokenList *tokenlist)
{
    tokenlist->list = tokenlist->static_list;
    tokenlist->size = sizeof(tokenlist->static_list) / sizeof(ParseToken);
    tokenlist->count = 0;
}

static void ScriptTokenListFree(ParseTokenList *tokenlist)
{
    if (tokenlist->list != tokenlist->static_list) {
        Jim_Free(tokenlist->list);
    }
}

/**
 * Adds the new token to the tokenlist.
 * The token has the given length, type and line number.
 * The token list is resized as necessary.
 */
static void ScriptAddToken(ParseTokenList *tokenlist, const char *token, int len, int type,
    int line)
{
    ParseToken *t;

    if (tokenlist->count == tokenlist->size) {
        /* Resize the list */
        tokenlist->size *= 2;
        if (tokenlist->list != tokenlist->static_list) {
            tokenlist->list =
                Jim_Realloc(tokenlist->list, tokenlist->size * sizeof(*tokenlist->list));
        }
        else {
            /* The list needs to become allocated */
            tokenlist->list = Jim_Alloc(tokenlist->size * sizeof(*tokenlist->list));
            memcpy(tokenlist->list, tokenlist->static_list,
                tokenlist->count * sizeof(*tokenlist->list));
        }
    }
    t = &tokenlist->list[tokenlist->count++];
    t->token = token;
    t->len = len;
    t->type = type;
    t->line = line;
}

/* Counts the number of adjoining non-separator tokens.
 *
 * Returns -ve if the first token is the expansion
 * operator (in which case the count doesn't include
 * that token).
 */
static int JimCountWordTokens(ParseToken *t)
{
    int expand = 1;
    int count = 0;

    /* Is the first word {*} or {expand}? */
    if (t->type == JIM_TT_STR && !TOKEN_IS_SEP(t[1].type)) {
        if ((t->len == 1 && *t->token == '*') || (t->len == 6 && strncmp(t->token, "expand", 6) == 0)) {
            /* Create an expand token */
            expand = -1;
            t++;
        }
    }

    /* Now count non-separator words */
    while (!TOKEN_IS_SEP(t->type)) {
        t++;
        count++;
    }

    return count * expand;
}

/**
 * Create a script/subst object from the given token.
 */
static Jim_Obj *JimMakeScriptObj(Jim_Interp *interp, const ParseToken *t)
{
    Jim_Obj *objPtr;

    if (t->type == JIM_TT_ESC && memchr(t->token, '\\', t->len) != NULL) {
        /* Convert backlash escapes. The result will never be longer than the original */
        int len = t->len;
        char *str = Jim_Alloc(len + 1);
        len = JimEscape(str, t->token, len);
        objPtr = Jim_NewStringObjNoAlloc(interp, str, len);
    }
    else {
        /* XXX: For strict Tcl compatibility, JIM_TT_STR should replace <backslash><newline><whitespace>
         *         with a single space.
         */
        objPtr = Jim_NewStringObj(interp, t->token, t->len);
    }
    return objPtr;
}

/**
 * Takes a tokenlist and creates the allocated list of script tokens
 * in script->token, of length script->len.
 *
 * Unnecessary tokens are discarded, and LINE and WORD tokens are inserted
 * as required.
 *
 * Also sets script->line to the line number of the first token
 */
static void ScriptObjAddTokens(Jim_Interp *interp, struct ScriptObj *script,
    ParseTokenList *tokenlist)
{
    int i;
    struct ScriptToken *token;
    /* Number of tokens so far for the current command */
    int lineargs = 0;
    /* This is the first token for the current command */
    ScriptToken *linefirst;
    int count;
    int linenr;

#ifdef DEBUG_SHOW_SCRIPT_TOKENS
    printf("==== Tokens ====\n");
    for (i = 0; i < tokenlist->count; i++) {
        printf("[%2d]@%d %s '%.*s'\n", i, tokenlist->list[i].line, jim_tt_name(tokenlist->list[i].type),
            tokenlist->list[i].len, tokenlist->list[i].token);
    }
#endif

    /* May need up to one extra script token for each EOL in the worst case */
    count = tokenlist->count;
    for (i = 0; i < tokenlist->count; i++) {
        if (tokenlist->list[i].type == JIM_TT_EOL) {
            count++;
        }
    }
    linenr = script->firstline = tokenlist->list[0].line;

    token = script->token = Jim_Alloc(sizeof(ScriptToken) * count);

    /* This is the first token for the current command */
    linefirst = token++;

    for (i = 0; i < tokenlist->count; ) {
        /* Look ahead to find out how many tokens make up the next word */
        int wordtokens;

        /* Skip any leading separators */
        while (tokenlist->list[i].type == JIM_TT_SEP) {
            i++;
        }

        wordtokens = JimCountWordTokens(tokenlist->list + i);

        if (wordtokens == 0) {
            /* None, so at end of line */
            if (lineargs) {
                linefirst->type = JIM_TT_LINE;
                linefirst->objPtr = JimNewScriptLineObj(interp, lineargs, linenr);
                Jim_IncrRefCount(linefirst->objPtr);

                /* Reset for new line */
                lineargs = 0;
                linefirst = token++;
            }
            i++;
            continue;
        }
        else if (wordtokens != 1) {
            /* More than 1, or {*}, so insert a WORD token */
            token->type = JIM_TT_WORD;
            token->objPtr = Jim_NewIntObj(interp, wordtokens);
            Jim_IncrRefCount(token->objPtr);
            token++;
            if (wordtokens < 0) {
                /* Skip the expand token */
                i++;
                wordtokens = -wordtokens - 1;
                lineargs--;
            }
        }

        if (lineargs == 0) {
            /* First real token on the line, so record the line number */
            linenr = tokenlist->list[i].line;
        }
        lineargs++;

        /* Add each non-separator word token to the line */
        while (wordtokens--) {
            const ParseToken *t = &tokenlist->list[i++];

            token->type = t->type;
            token->objPtr = JimMakeScriptObj(interp, t);
            Jim_IncrRefCount(token->objPtr);

            /* Every object is initially a string of type 'source', but the
             * internal type may be specialized during execution of the
             * script. */
            JimSetSourceInfo(interp, token->objPtr, script->fileNameObj, t->line);
            token++;
        }
    }

    if (lineargs == 0) {
        token--;
    }

    script->len = token - script->token;

    JimPanic((script->len >= count, "allocated script array is too short"));

#ifdef DEBUG_SHOW_SCRIPT
    printf("==== Script (%s) ====\n", Jim_String(script->fileNameObj));
    for (i = 0; i < script->len; i++) {
        const ScriptToken *t = &script->token[i];
        printf("[%2d] %s %s\n", i, jim_tt_name(t->type), Jim_String(t->objPtr));
    }
#endif

}

/* Parses the given string object to determine if it represents a complete script.
 *
 * This is useful for interactive shells implementation, for [info complete].
 *
 * If 'stateCharPtr' != NULL, the function stores ' ' on complete script,
 * '{' on scripts incomplete missing one or more '}' to be balanced.
 * '[' on scripts incomplete missing one or more ']' to be balanced.
 * '"' on scripts incomplete missing a '"' char.
 * '\\' on scripts with a trailing backslash.
 *
 * If the script is complete, 1 is returned, otherwise 0.
 */
int Jim_ScriptIsComplete(Jim_Interp *interp, Jim_Obj *scriptObj, char *stateCharPtr)
{
    ScriptObj *script = JimGetScript(interp, scriptObj);
    if (stateCharPtr) {
        *stateCharPtr = script->missing;
    }
    return (script->missing == ' ');
}

/**
 * Sets an appropriate error message for a missing script/expression terminator.
 *
 * Returns JIM_ERR if 'ch' represents an unmatched/missing character.
 *
 * Note that a trailing backslash is not considered to be an error.
 */
static int JimParseCheckMissing(Jim_Interp *interp, int ch)
{
    const char *msg;

    switch (ch) {
        case '\\':
        case ' ':
            return JIM_OK;

        case '[':
            msg = "unmatched \"[\"";
            break;
        case '{':
            msg = "missing close-brace";
            break;
        case '"':
        default:
            msg = "missing quote";
            break;
    }

    Jim_SetResultString(interp, msg, -1);
    return JIM_ERR;
}

/**
 * Similar to ScriptObjAddTokens(), but for subst objects.
 */
static void SubstObjAddTokens(Jim_Interp *interp, struct ScriptObj *script,
    ParseTokenList *tokenlist)
{
    int i;
    struct ScriptToken *token;

    token = script->token = Jim_Alloc(sizeof(ScriptToken) * tokenlist->count);

    for (i = 0; i < tokenlist->count; i++) {
        const ParseToken *t = &tokenlist->list[i];

        /* Create a token for 't' */
        token->type = t->type;
        token->objPtr = JimMakeScriptObj(interp, t);
        Jim_IncrRefCount(token->objPtr);
        token++;
    }

    script->len = i;
}

/* This method takes the string representation of an object
 * as a Tcl script, and generates the pre-parsed internal representation
 * of the script.
 *
 * On parse error, sets an error message and returns JIM_ERR
 * (Note: the object is still converted to a script, even if an error occurs)
 */
static void JimSetScriptFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    int scriptTextLen;
    const char *scriptText = Jim_GetString(objPtr, &scriptTextLen);
    struct JimParserCtx parser;
    struct ScriptObj *script;
    ParseTokenList tokenlist;
    int line = 1;

    /* Try to get information about filename / line number */
    if (objPtr->typePtr == &sourceObjType) {
        line = objPtr->internalRep.sourceValue.lineNumber;
    }

    /* Initially parse the script into tokens (in tokenlist) */
    ScriptTokenListInit(&tokenlist);

    JimParserInit(&parser, scriptText, scriptTextLen, line);
    while (!parser.eof) {
        JimParseScript(&parser);
        ScriptAddToken(&tokenlist, parser.tstart, parser.tend - parser.tstart + 1, parser.tt,
            parser.tline);
    }

    /* Add a final EOF token */
    ScriptAddToken(&tokenlist, scriptText + scriptTextLen, 0, JIM_TT_EOF, 0);

    /* Create the "real" script tokens from the parsed tokens */
    script = Jim_Alloc(sizeof(*script));
    memset(script, 0, sizeof(*script));
    script->inUse = 1;
    if (objPtr->typePtr == &sourceObjType) {
        script->fileNameObj = objPtr->internalRep.sourceValue.fileNameObj;
    }
    else {
        script->fileNameObj = interp->emptyObj;
    }
    Jim_IncrRefCount(script->fileNameObj);
    script->missing = parser.missing.ch;
    script->linenr = parser.missing.line;

    ScriptObjAddTokens(interp, script, &tokenlist);

    /* No longer need the token list */
    ScriptTokenListFree(&tokenlist);

    /* Free the old internal rep and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    Jim_SetIntRepPtr(objPtr, script);
    objPtr->typePtr = &scriptObjType;
}

static void JimAddErrorToStack(Jim_Interp *interp, ScriptObj *script);

/**
 * Returns the parsed script.
 * Note that if there is any possibility that the script is not valid,
 * call JimScriptValid() to check
 */
static ScriptObj *JimGetScript(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (objPtr == interp->emptyObj) {
        /* Avoid converting emptyObj to a script. use nullScriptObj instead. */
        objPtr = interp->nullScriptObj;
    }

    if (objPtr->typePtr != &scriptObjType || ((struct ScriptObj *)Jim_GetIntRepPtr(objPtr))->substFlags) {
        JimSetScriptFromAny(interp, objPtr);
    }

    return (ScriptObj *)Jim_GetIntRepPtr(objPtr);
}

/**
 * Returns 1 if the script is valid (parsed ok), otherwise returns 0
 * and leaves an error message in the interp result.
 *
 */
static int JimScriptValid(Jim_Interp *interp, ScriptObj *script)
{
    if (JimParseCheckMissing(interp, script->missing) == JIM_ERR) {
        JimAddErrorToStack(interp, script);
        return 0;
    }
    return 1;
}


/* -----------------------------------------------------------------------------
 * Commands
 * ---------------------------------------------------------------------------*/
static void JimIncrCmdRefCount(Jim_Cmd *cmdPtr)
{
    cmdPtr->inUse++;
}

static void JimDecrCmdRefCount(Jim_Interp *interp, Jim_Cmd *cmdPtr)
{
    if (--cmdPtr->inUse == 0) {
        if (cmdPtr->isproc) {
            Jim_DecrRefCount(interp, cmdPtr->u.proc.argListObjPtr);
            Jim_DecrRefCount(interp, cmdPtr->u.proc.bodyObjPtr);
            Jim_DecrRefCount(interp, cmdPtr->u.proc.nsObj);
            if (cmdPtr->u.proc.staticVars) {
                Jim_FreeHashTable(cmdPtr->u.proc.staticVars);
                Jim_Free(cmdPtr->u.proc.staticVars);
            }
        }
        else {
            /* native (C) */
            if (cmdPtr->u.native.delProc) {
                cmdPtr->u.native.delProc(interp, cmdPtr->u.native.privData);
            }
        }
        if (cmdPtr->prevCmd) {
            /* Delete any pushed command too */
            JimDecrCmdRefCount(interp, cmdPtr->prevCmd);
        }
        Jim_Free(cmdPtr);
    }
}

/* Variables HashTable Type.
 *
 * Keys are dynamically allocated strings, Values are Jim_Var structures.
 */

/* Variables HashTable Type.
 *
 * Keys are dynamic allocated strings, Values are Jim_Var structures. */
static void JimVariablesHTValDestructor(void *interp, void *val)
{
    Jim_DecrRefCount(interp, ((Jim_Var *)val)->objPtr);
    Jim_Free(val);
}

static const Jim_HashTableType JimVariablesHashTableType = {
    JimStringCopyHTHashFunction,        /* hash function */
    JimStringCopyHTDup,                 /* key dup */
    NULL,                               /* val dup */
    JimStringCopyHTKeyCompare,  /* key compare */
    JimStringCopyHTKeyDestructor,       /* key destructor */
    JimVariablesHTValDestructor /* val destructor */
};

/* Commands HashTable Type.
 *
 * Keys are dynamic allocated strings, Values are Jim_Cmd structures.
 */
static void JimCommandsHT_ValDestructor(void *interp, void *val)
{
    JimDecrCmdRefCount(interp, val);
}

static const Jim_HashTableType JimCommandsHashTableType = {
    JimStringCopyHTHashFunction,    /* hash function */
    JimStringCopyHTDup,             /* key dup */
    NULL,                           /* val dup */
    JimStringCopyHTKeyCompare,      /* key compare */
    JimStringCopyHTKeyDestructor,   /* key destructor */
    JimCommandsHT_ValDestructor     /* val destructor */
};

/* ------------------------- Commands related functions --------------------- */

#ifdef jim_ext_namespace
/**
 * Returns the "unscoped" version of the given namespace.
 * That is, the fully qualified name without the leading ::
 * The returned value is either nsObj, or an object with a zero ref count.
 */
static Jim_Obj *JimQualifyNameObj(Jim_Interp *interp, Jim_Obj *nsObj)
{
    const char *name = Jim_String(nsObj);
    if (name[0] == ':' && name[1] == ':') {
        /* This command is being defined in the global namespace */
        while (*++name == ':') {
        }
        nsObj = Jim_NewStringObj(interp, name, -1);
    }
    else if (Jim_Length(interp->framePtr->nsObj)) {
        /* This command is being defined in a non-global namespace */
        nsObj = Jim_DuplicateObj(interp, interp->framePtr->nsObj);
        Jim_AppendStrings(interp, nsObj, "::", name, NULL);
    }
    return nsObj;
}

Jim_Obj *Jim_MakeGlobalNamespaceName(Jim_Interp *interp, Jim_Obj *nameObjPtr)
{
    Jim_Obj *resultObj;

    const char *name = Jim_String(nameObjPtr);
    if (name[0] == ':' && name[1] == ':') {
        return nameObjPtr;
    }
    Jim_IncrRefCount(nameObjPtr);
    resultObj = Jim_NewStringObj(interp, "::", -1);
    Jim_AppendObj(interp, resultObj, nameObjPtr);
    Jim_DecrRefCount(interp, nameObjPtr);

    return resultObj;
}

/**
 * An efficient version of JimQualifyNameObj() where the name is
 * available (and needed) as a 'const char *'.
 * Avoids creating an object if not necessary.
 * The object stored in *objPtrPtr should be disposed of with JimFreeQualifiedName() after use.
 */
static const char *JimQualifyName(Jim_Interp *interp, const char *name, Jim_Obj **objPtrPtr)
{
    Jim_Obj *objPtr = interp->emptyObj;

    if (name[0] == ':' && name[1] == ':') {
        /* This command is being defined in the global namespace */
        while (*++name == ':') {
        }
    }
    else if (Jim_Length(interp->framePtr->nsObj)) {
        /* This command is being defined in a non-global namespace */
        objPtr = Jim_DuplicateObj(interp, interp->framePtr->nsObj);
        Jim_AppendStrings(interp, objPtr, "::", name, NULL);
        name = Jim_String(objPtr);
    }
    Jim_IncrRefCount(objPtr);
    *objPtrPtr = objPtr;
    return name;
}

    #define JimFreeQualifiedName(INTERP, OBJ) Jim_DecrRefCount((INTERP), (OBJ))

#else
    /* We can be more efficient in the no-namespace case */
    #define JimQualifyName(INTERP, NAME, DUMMY) (((NAME)[0] == ':' && (NAME)[1] == ':') ? (NAME) + 2 : (NAME))
    #define JimFreeQualifiedName(INTERP, DUMMY) (void)(DUMMY)

Jim_Obj *Jim_MakeGlobalNamespaceName(Jim_Interp *interp, Jim_Obj *nameObjPtr)
{
    return nameObjPtr;
}
#endif

static int JimCreateCommand(Jim_Interp *interp, const char *name, Jim_Cmd *cmd)
{
    /* It may already exist, so we try to delete the old one.
     * Note that reference count means that it won't be deleted yet if
     * it exists in the call stack.
     *
     * BUT, if 'local' is in force, instead of deleting the existing
     * proc, we stash a reference to the old proc here.
     */
    Jim_HashEntry *he = Jim_FindHashEntry(&interp->commands, name);
    if (he) {
        /* There was an old cmd with the same name,
         * so this requires a 'proc epoch' update. */

        /* If a procedure with the same name didn't exist there is no need
         * to increment the 'proc epoch' because creation of a new procedure
         * can never affect existing cached commands. We don't do
         * negative caching. */
        Jim_InterpIncrProcEpoch(interp);
    }

    if (he && interp->local) {
        /* Push this command over the top of the previous one */
        cmd->prevCmd = Jim_GetHashEntryVal(he);
        Jim_SetHashVal(&interp->commands, he, cmd);
    }
    else {
        if (he) {
            /* Replace the existing command */
            Jim_DeleteHashEntry(&interp->commands, name);
        }

        Jim_AddHashEntry(&interp->commands, name, cmd);
    }
    return JIM_OK;
}


int Jim_CreateCommand(Jim_Interp *interp, const char *cmdNameStr,
    Jim_CmdProc *cmdProc, void *privData, Jim_DelCmdProc *delProc)
{
    Jim_Cmd *cmdPtr = Jim_Alloc(sizeof(*cmdPtr));

    /* Store the new details for this command */
    memset(cmdPtr, 0, sizeof(*cmdPtr));
    cmdPtr->inUse = 1;
    cmdPtr->u.native.delProc = delProc;
    cmdPtr->u.native.cmdProc = cmdProc;
    cmdPtr->u.native.privData = privData;

    JimCreateCommand(interp, cmdNameStr, cmdPtr);

    return JIM_OK;
}

static int JimCreateProcedureStatics(Jim_Interp *interp, Jim_Cmd *cmdPtr, Jim_Obj *staticsListObjPtr)
{
    int len, i;

    len = Jim_ListLength(interp, staticsListObjPtr);
    if (len == 0) {
        return JIM_OK;
    }

    cmdPtr->u.proc.staticVars = Jim_Alloc(sizeof(Jim_HashTable));
    Jim_InitHashTable(cmdPtr->u.proc.staticVars, &JimVariablesHashTableType, interp);
    for (i = 0; i < len; i++) {
        Jim_Obj *objPtr, *initObjPtr, *nameObjPtr;
        Jim_Var *varPtr;
        int subLen;

        objPtr = Jim_ListGetIndex(interp, staticsListObjPtr, i);
        /* Check if it's composed of two elements. */
        subLen = Jim_ListLength(interp, objPtr);
        if (subLen == 1 || subLen == 2) {
            /* Try to get the variable value from the current
             * environment. */
            nameObjPtr = Jim_ListGetIndex(interp, objPtr, 0);
            if (subLen == 1) {
                initObjPtr = Jim_GetVariable(interp, nameObjPtr, JIM_NONE);
                if (initObjPtr == NULL) {
                    Jim_SetResultFormatted(interp,
                        "variable for initialization of static \"%#s\" not found in the local context",
                        nameObjPtr);
                    return JIM_ERR;
                }
            }
            else {
                initObjPtr = Jim_ListGetIndex(interp, objPtr, 1);
            }
            if (JimValidName(interp, "static variable", nameObjPtr) != JIM_OK) {
                return JIM_ERR;
            }

            varPtr = Jim_Alloc(sizeof(*varPtr));
            varPtr->objPtr = initObjPtr;
            Jim_IncrRefCount(initObjPtr);
            varPtr->linkFramePtr = NULL;
            if (Jim_AddHashEntry(cmdPtr->u.proc.staticVars,
                Jim_String(nameObjPtr), varPtr) != JIM_OK) {
                Jim_SetResultFormatted(interp,
                    "static variable name \"%#s\" duplicated in statics list", nameObjPtr);
                Jim_DecrRefCount(interp, initObjPtr);
                Jim_Free(varPtr);
                return JIM_ERR;
            }
        }
        else {
            Jim_SetResultFormatted(interp, "too many fields in static specifier \"%#s\"",
                objPtr);
            return JIM_ERR;
        }
    }
    return JIM_OK;
}

static void JimUpdateProcNamespace(Jim_Interp *interp, Jim_Cmd *cmdPtr, const char *cmdname)
{
#ifdef jim_ext_namespace
    if (cmdPtr->isproc) {
        /* XXX: Really need JimNamespaceSplit() */
        const char *pt = strrchr(cmdname, ':');
        if (pt && pt != cmdname && pt[-1] == ':') {
            Jim_DecrRefCount(interp, cmdPtr->u.proc.nsObj);
            cmdPtr->u.proc.nsObj = Jim_NewStringObj(interp, cmdname, pt - cmdname - 1);
            Jim_IncrRefCount(cmdPtr->u.proc.nsObj);

            if (Jim_FindHashEntry(&interp->commands, pt + 1)) {
                /* This commands shadows a global command, so a proc epoch update is required */
                Jim_InterpIncrProcEpoch(interp);
            }
        }
    }
#endif
}

static Jim_Cmd *JimCreateProcedureCmd(Jim_Interp *interp, Jim_Obj *argListObjPtr,
    Jim_Obj *staticsListObjPtr, Jim_Obj *bodyObjPtr, Jim_Obj *nsObj)
{
    Jim_Cmd *cmdPtr;
    int argListLen;
    int i;

    argListLen = Jim_ListLength(interp, argListObjPtr);

    /* Allocate space for both the command pointer and the arg list */
    cmdPtr = Jim_Alloc(sizeof(*cmdPtr) + sizeof(struct Jim_ProcArg) * argListLen);
    memset(cmdPtr, 0, sizeof(*cmdPtr));
    cmdPtr->inUse = 1;
    cmdPtr->isproc = 1;
    cmdPtr->u.proc.argListObjPtr = argListObjPtr;
    cmdPtr->u.proc.argListLen = argListLen;
    cmdPtr->u.proc.bodyObjPtr = bodyObjPtr;
    cmdPtr->u.proc.argsPos = -1;
    cmdPtr->u.proc.arglist = (struct Jim_ProcArg *)(cmdPtr + 1);
    cmdPtr->u.proc.nsObj = nsObj ? nsObj : interp->emptyObj;
    Jim_IncrRefCount(argListObjPtr);
    Jim_IncrRefCount(bodyObjPtr);
    Jim_IncrRefCount(cmdPtr->u.proc.nsObj);

    /* Create the statics hash table. */
    if (staticsListObjPtr && JimCreateProcedureStatics(interp, cmdPtr, staticsListObjPtr) != JIM_OK) {
        goto err;
    }

    /* Parse the args out into arglist, validating as we go */
    /* Examine the argument list for default parameters and 'args' */
    for (i = 0; i < argListLen; i++) {
        Jim_Obj *argPtr;
        Jim_Obj *nameObjPtr;
        Jim_Obj *defaultObjPtr;
        int len;

        /* Examine a parameter */
        argPtr = Jim_ListGetIndex(interp, argListObjPtr, i);
        len = Jim_ListLength(interp, argPtr);
        if (len == 0) {
            Jim_SetResultString(interp, "argument with no name", -1);
err:
            JimDecrCmdRefCount(interp, cmdPtr);
            return NULL;
        }
        if (len > 2) {
            Jim_SetResultFormatted(interp, "too many fields in argument specifier \"%#s\"", argPtr);
            goto err;
        }

        if (len == 2) {
            /* Optional parameter */
            nameObjPtr = Jim_ListGetIndex(interp, argPtr, 0);
            defaultObjPtr = Jim_ListGetIndex(interp, argPtr, 1);
        }
        else {
            /* Required parameter */
            nameObjPtr = argPtr;
            defaultObjPtr = NULL;
        }


        if (Jim_CompareStringImmediate(interp, nameObjPtr, "args")) {
            if (cmdPtr->u.proc.argsPos >= 0) {
                Jim_SetResultString(interp, "'args' specified more than once", -1);
                goto err;
            }
            cmdPtr->u.proc.argsPos = i;
        }
        else {
            if (len == 2) {
                cmdPtr->u.proc.optArity++;
            }
            else {
                cmdPtr->u.proc.reqArity++;
            }
        }

        cmdPtr->u.proc.arglist[i].nameObjPtr = nameObjPtr;
        cmdPtr->u.proc.arglist[i].defaultObjPtr = defaultObjPtr;
    }

    return cmdPtr;
}

int Jim_DeleteCommand(Jim_Interp *interp, const char *name)
{
    int ret = JIM_OK;
    Jim_Obj *qualifiedNameObj;
    const char *qualname = JimQualifyName(interp, name, &qualifiedNameObj);

    if (Jim_DeleteHashEntry(&interp->commands, qualname) == JIM_ERR) {
        Jim_SetResultFormatted(interp, "can't delete \"%s\": command doesn't exist", name);
        ret = JIM_ERR;
    }
    else {
        Jim_InterpIncrProcEpoch(interp);
    }

    JimFreeQualifiedName(interp, qualifiedNameObj);

    return ret;
}

int Jim_RenameCommand(Jim_Interp *interp, const char *oldName, const char *newName)
{
    int ret = JIM_ERR;
    Jim_HashEntry *he;
    Jim_Cmd *cmdPtr;
    Jim_Obj *qualifiedOldNameObj;
    Jim_Obj *qualifiedNewNameObj;
    const char *fqold;
    const char *fqnew;

    if (newName[0] == 0) {
        return Jim_DeleteCommand(interp, oldName);
    }

    fqold = JimQualifyName(interp, oldName, &qualifiedOldNameObj);
    fqnew = JimQualifyName(interp, newName, &qualifiedNewNameObj);

    /* Does it exist? */
    he = Jim_FindHashEntry(&interp->commands, fqold);
    if (he == NULL) {
        Jim_SetResultFormatted(interp, "can't rename \"%s\": command doesn't exist", oldName);
    }
    else if (Jim_FindHashEntry(&interp->commands, fqnew)) {
        Jim_SetResultFormatted(interp, "can't rename to \"%s\": command already exists", newName);
    }
    else {
        /* Add the new name first */
        cmdPtr = Jim_GetHashEntryVal(he);
        JimIncrCmdRefCount(cmdPtr);
        JimUpdateProcNamespace(interp, cmdPtr, fqnew);
        Jim_AddHashEntry(&interp->commands, fqnew, cmdPtr);

        /* Now remove the old name */
        Jim_DeleteHashEntry(&interp->commands, fqold);

        /* Increment the epoch */
        Jim_InterpIncrProcEpoch(interp);

        ret = JIM_OK;
    }

    JimFreeQualifiedName(interp, qualifiedOldNameObj);
    JimFreeQualifiedName(interp, qualifiedNewNameObj);

    return ret;
}

/* -----------------------------------------------------------------------------
 * Command object
 * ---------------------------------------------------------------------------*/

static void FreeCommandInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_DecrRefCount(interp, objPtr->internalRep.cmdValue.nsObj);
}

static void DupCommandInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    dupPtr->internalRep.cmdValue = srcPtr->internalRep.cmdValue;
    dupPtr->typePtr = srcPtr->typePtr;
    Jim_IncrRefCount(dupPtr->internalRep.cmdValue.nsObj);
}

static const Jim_ObjType commandObjType = {
    "command",
    FreeCommandInternalRep,
    DupCommandInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

/* This function returns the command structure for the command name
 * stored in objPtr. It tries to specialize the objPtr to contain
 * a cached info instead to perform the lookup into the hash table
 * every time. The information cached may not be uptodate, in such
 * a case the lookup is performed and the cache updated.
 *
 * Respects the 'upcall' setting
 */
Jim_Cmd *Jim_GetCommand(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    Jim_Cmd *cmd;

    /* In order to be valid, the proc epoch must match and
     * the lookup must have occurred in the same namespace
     */
    if (objPtr->typePtr != &commandObjType ||
            objPtr->internalRep.cmdValue.procEpoch != interp->procEpoch
#ifdef jim_ext_namespace
            || !Jim_StringEqObj(objPtr->internalRep.cmdValue.nsObj, interp->framePtr->nsObj)
#endif
        ) {
        /* Not cached or out of date, so lookup */

        /* Do we need to try the local namespace? */
        const char *name = Jim_String(objPtr);
        Jim_HashEntry *he;

        if (name[0] == ':' && name[1] == ':') {
            while (*++name == ':') {
            }
        }
#ifdef jim_ext_namespace
        else if (Jim_Length(interp->framePtr->nsObj)) {
            /* This command is being defined in a non-global namespace */
            Jim_Obj *nameObj = Jim_DuplicateObj(interp, interp->framePtr->nsObj);
            Jim_AppendStrings(interp, nameObj, "::", name, NULL);
            he = Jim_FindHashEntry(&interp->commands, Jim_String(nameObj));
            Jim_FreeNewObj(interp, nameObj);
            if (he) {
                goto found;
            }
        }
#endif

        /* Lookup in the global namespace */
        he = Jim_FindHashEntry(&interp->commands, name);
        if (he == NULL) {
            if (flags & JIM_ERRMSG) {
                Jim_SetResultFormatted(interp, "invalid command name \"%#s\"", objPtr);
            }
            return NULL;
        }
#ifdef jim_ext_namespace
found:
#endif
        cmd = Jim_GetHashEntryVal(he);

        /* Free the old internal repr and set the new one. */
        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &commandObjType;
        objPtr->internalRep.cmdValue.procEpoch = interp->procEpoch;
        objPtr->internalRep.cmdValue.cmdPtr = cmd;
        objPtr->internalRep.cmdValue.nsObj = interp->framePtr->nsObj;
        Jim_IncrRefCount(interp->framePtr->nsObj);
    }
    else {
        cmd = objPtr->internalRep.cmdValue.cmdPtr;
    }
    while (cmd->u.proc.upcall) {
        cmd = cmd->prevCmd;
    }
    return cmd;
}

/* -----------------------------------------------------------------------------
 * Variables
 * ---------------------------------------------------------------------------*/

/* -----------------------------------------------------------------------------
 * Variable object
 * ---------------------------------------------------------------------------*/

#define JIM_DICT_SUGAR 100      /* Only returned by SetVariableFromAny() */

static int SetVariableFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

static const Jim_ObjType variableObjType = {
    "variable",
    NULL,
    NULL,
    NULL,
    JIM_TYPE_REFERENCES,
};

/**
 * Check that the name does not contain embedded nulls.
 *
 * Variable and procedure names are manipulated as null terminated strings, so
 * don't allow names with embedded nulls.
 */
static int JimValidName(Jim_Interp *interp, const char *type, Jim_Obj *nameObjPtr)
{
    /* Variable names and proc names can't contain embedded nulls */
    if (nameObjPtr->typePtr != &variableObjType) {
        int len;
        const char *str = Jim_GetString(nameObjPtr, &len);
        if (memchr(str, '\0', len)) {
            Jim_SetResultFormatted(interp, "%s name contains embedded null", type);
            return JIM_ERR;
        }
    }
    return JIM_OK;
}

/* This method should be called only by the variable API.
 * It returns JIM_OK on success (variable already exists),
 * JIM_ERR if it does not exist, JIM_DICT_SUGAR if it's not
 * a variable name, but syntax glue for [dict] i.e. the last
 * character is ')' */
static int SetVariableFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    const char *varName;
    Jim_CallFrame *framePtr;
    Jim_HashEntry *he;
    int global;
    int len;

    /* Check if the object is already an uptodate variable */
    if (objPtr->typePtr == &variableObjType) {
        framePtr = objPtr->internalRep.varValue.global ? interp->topFramePtr : interp->framePtr;
        if (objPtr->internalRep.varValue.callFrameId == framePtr->id) {
            /* nothing to do */
            return JIM_OK;
        }
        /* Need to re-resolve the variable in the updated callframe */
    }
    else if (objPtr->typePtr == &dictSubstObjType) {
        return JIM_DICT_SUGAR;
    }
    else if (JimValidName(interp, "variable", objPtr) != JIM_OK) {
        return JIM_ERR;
    }


    varName = Jim_GetString(objPtr, &len);

    /* Make sure it's not syntax glue to get/set dict. */
    if (len && varName[len - 1] == ')' && strchr(varName, '(') != NULL) {
        return JIM_DICT_SUGAR;
    }

    if (varName[0] == ':' && varName[1] == ':') {
        while (*++varName == ':') {
        }
        global = 1;
        framePtr = interp->topFramePtr;
    }
    else {
        global = 0;
        framePtr = interp->framePtr;
    }

    /* Resolve this name in the variables hash table */
    he = Jim_FindHashEntry(&framePtr->vars, varName);
    if (he == NULL) {
        if (!global && framePtr->staticVars) {
            /* Try with static vars. */
            he = Jim_FindHashEntry(framePtr->staticVars, varName);
        }
        if (he == NULL) {
            return JIM_ERR;
        }
    }

    /* Free the old internal repr and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &variableObjType;
    objPtr->internalRep.varValue.callFrameId = framePtr->id;
    objPtr->internalRep.varValue.varPtr = Jim_GetHashEntryVal(he);
    objPtr->internalRep.varValue.global = global;
    return JIM_OK;
}

/* -------------------- Variables related functions ------------------------- */
static int JimDictSugarSet(Jim_Interp *interp, Jim_Obj *ObjPtr, Jim_Obj *valObjPtr);
static Jim_Obj *JimDictSugarGet(Jim_Interp *interp, Jim_Obj *ObjPtr, int flags);

static Jim_Var *JimCreateVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr, Jim_Obj *valObjPtr)
{
    const char *name;
    Jim_CallFrame *framePtr;
    int global;

    /* New variable to create */
    Jim_Var *var = Jim_Alloc(sizeof(*var));

    var->objPtr = valObjPtr;
    Jim_IncrRefCount(valObjPtr);
    var->linkFramePtr = NULL;

    name = Jim_String(nameObjPtr);
    if (name[0] == ':' && name[1] == ':') {
        while (*++name == ':') {
        }
        framePtr = interp->topFramePtr;
        global = 1;
    }
    else {
        framePtr = interp->framePtr;
        global = 0;
    }

    /* Insert the new variable */
    Jim_AddHashEntry(&framePtr->vars, name, var);

    /* Make the object int rep a variable */
    Jim_FreeIntRep(interp, nameObjPtr);
    nameObjPtr->typePtr = &variableObjType;
    nameObjPtr->internalRep.varValue.callFrameId = framePtr->id;
    nameObjPtr->internalRep.varValue.varPtr = var;
    nameObjPtr->internalRep.varValue.global = global;

    return var;
}

/* For now that's dummy. Variables lookup should be optimized
 * in many ways, with caching of lookups, and possibly with
 * a table of pre-allocated vars in every CallFrame for local vars.
 * All the caching should also have an 'epoch' mechanism similar
 * to the one used by Tcl for procedures lookup caching. */

int Jim_SetVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr, Jim_Obj *valObjPtr)
{
    int err;
    Jim_Var *var;

    switch (SetVariableFromAny(interp, nameObjPtr)) {
        case JIM_DICT_SUGAR:
            return JimDictSugarSet(interp, nameObjPtr, valObjPtr);

        case JIM_ERR:
            if (JimValidName(interp, "variable", nameObjPtr) != JIM_OK) {
                return JIM_ERR;
            }
            JimCreateVariable(interp, nameObjPtr, valObjPtr);
            break;

        case JIM_OK:
            var = nameObjPtr->internalRep.varValue.varPtr;
            if (var->linkFramePtr == NULL) {
                Jim_IncrRefCount(valObjPtr);
                Jim_DecrRefCount(interp, var->objPtr);
                var->objPtr = valObjPtr;
            }
            else {                  /* Else handle the link */
                Jim_CallFrame *savedCallFrame;

                savedCallFrame = interp->framePtr;
                interp->framePtr = var->linkFramePtr;
                err = Jim_SetVariable(interp, var->objPtr, valObjPtr);
                interp->framePtr = savedCallFrame;
                if (err != JIM_OK)
                    return err;
            }
    }
    return JIM_OK;
}

int Jim_SetVariableStr(Jim_Interp *interp, const char *name, Jim_Obj *objPtr)
{
    Jim_Obj *nameObjPtr;
    int result;

    nameObjPtr = Jim_NewStringObj(interp, name, -1);
    Jim_IncrRefCount(nameObjPtr);
    result = Jim_SetVariable(interp, nameObjPtr, objPtr);
    Jim_DecrRefCount(interp, nameObjPtr);
    return result;
}

int Jim_SetGlobalVariableStr(Jim_Interp *interp, const char *name, Jim_Obj *objPtr)
{
    Jim_CallFrame *savedFramePtr;
    int result;

    savedFramePtr = interp->framePtr;
    interp->framePtr = interp->topFramePtr;
    result = Jim_SetVariableStr(interp, name, objPtr);
    interp->framePtr = savedFramePtr;
    return result;
}

int Jim_SetVariableStrWithStr(Jim_Interp *interp, const char *name, const char *val)
{
    Jim_Obj *nameObjPtr, *valObjPtr;
    int result;

    nameObjPtr = Jim_NewStringObj(interp, name, -1);
    valObjPtr = Jim_NewStringObj(interp, val, -1);
    Jim_IncrRefCount(nameObjPtr);
    Jim_IncrRefCount(valObjPtr);
    result = Jim_SetVariable(interp, nameObjPtr, valObjPtr);
    Jim_DecrRefCount(interp, nameObjPtr);
    Jim_DecrRefCount(interp, valObjPtr);
    return result;
}

int Jim_SetVariableLink(Jim_Interp *interp, Jim_Obj *nameObjPtr,
    Jim_Obj *targetNameObjPtr, Jim_CallFrame *targetCallFrame)
{
    const char *varName;
    const char *targetName;
    Jim_CallFrame *framePtr;
    Jim_Var *varPtr;

    /* Check for an existing variable or link */
    switch (SetVariableFromAny(interp, nameObjPtr)) {
        case JIM_DICT_SUGAR:
            /* XXX: This message seem unnecessarily verbose, but it matches Tcl */
            Jim_SetResultFormatted(interp, "bad variable name \"%#s\": upvar won't create a scalar variable that looks like an array element", nameObjPtr);
            return JIM_ERR;

        case JIM_OK:
            varPtr = nameObjPtr->internalRep.varValue.varPtr;

            if (varPtr->linkFramePtr == NULL) {
                Jim_SetResultFormatted(interp, "variable \"%#s\" already exists", nameObjPtr);
                return JIM_ERR;
            }

            /* It exists, but is a link, so first delete the link */
            varPtr->linkFramePtr = NULL;
            break;
    }

    /* Resolve the call frames for both variables */
    /* XXX: SetVariableFromAny() already did this! */
    varName = Jim_String(nameObjPtr);

    if (varName[0] == ':' && varName[1] == ':') {
        while (*++varName == ':') {
        }
        /* Linking a global var does nothing */
        framePtr = interp->topFramePtr;
    }
    else {
        framePtr = interp->framePtr;
    }

    targetName = Jim_String(targetNameObjPtr);
    if (targetName[0] == ':' && targetName[1] == ':') {
        while (*++targetName == ':') {
        }
        targetNameObjPtr = Jim_NewStringObj(interp, targetName, -1);
        targetCallFrame = interp->topFramePtr;
    }
    Jim_IncrRefCount(targetNameObjPtr);

    if (framePtr->level < targetCallFrame->level) {
        Jim_SetResultFormatted(interp,
            "bad variable name \"%#s\": upvar won't create namespace variable that refers to procedure variable",
            nameObjPtr);
        Jim_DecrRefCount(interp, targetNameObjPtr);
        return JIM_ERR;
    }

    /* Check for cycles. */
    if (framePtr == targetCallFrame) {
        Jim_Obj *objPtr = targetNameObjPtr;

        /* Cycles are only possible with 'uplevel 0' */
        while (1) {
            if (strcmp(Jim_String(objPtr), varName) == 0) {
                Jim_SetResultString(interp, "can't upvar from variable to itself", -1);
                Jim_DecrRefCount(interp, targetNameObjPtr);
                return JIM_ERR;
            }
            if (SetVariableFromAny(interp, objPtr) != JIM_OK)
                break;
            varPtr = objPtr->internalRep.varValue.varPtr;
            if (varPtr->linkFramePtr != targetCallFrame)
                break;
            objPtr = varPtr->objPtr;
        }
    }

    /* Perform the binding */
    Jim_SetVariable(interp, nameObjPtr, targetNameObjPtr);
    /* We are now sure 'nameObjPtr' type is variableObjType */
    nameObjPtr->internalRep.varValue.varPtr->linkFramePtr = targetCallFrame;
    Jim_DecrRefCount(interp, targetNameObjPtr);
    return JIM_OK;
}

/* Return the Jim_Obj pointer associated with a variable name,
 * or NULL if the variable was not found in the current context.
 * The same optimization discussed in the comment to the
 * 'SetVariable' function should apply here.
 *
 * If JIM_UNSHARED is set and the variable is an array element (dict sugar)
 * in a dictionary which is shared, the array variable value is duplicated first.
 * This allows the array element to be updated (e.g. append, lappend) without
 * affecting other references to the dictionary.
 */
Jim_Obj *Jim_GetVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr, int flags)
{
    switch (SetVariableFromAny(interp, nameObjPtr)) {
        case JIM_OK:{
                Jim_Var *varPtr = nameObjPtr->internalRep.varValue.varPtr;

                if (varPtr->linkFramePtr == NULL) {
                    return varPtr->objPtr;
                }
                else {
                    Jim_Obj *objPtr;

                    /* The variable is a link? Resolve it. */
                    Jim_CallFrame *savedCallFrame = interp->framePtr;

                    interp->framePtr = varPtr->linkFramePtr;
                    objPtr = Jim_GetVariable(interp, varPtr->objPtr, flags);
                    interp->framePtr = savedCallFrame;
                    if (objPtr) {
                        return objPtr;
                    }
                    /* Error, so fall through to the error message */
                }
            }
            break;

        case JIM_DICT_SUGAR:
            /* [dict] syntax sugar. */
            return JimDictSugarGet(interp, nameObjPtr, flags);
    }
    if (flags & JIM_ERRMSG) {
        Jim_SetResultFormatted(interp, "can't read \"%#s\": no such variable", nameObjPtr);
    }
    return NULL;
}

Jim_Obj *Jim_GetGlobalVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr, int flags)
{
    Jim_CallFrame *savedFramePtr;
    Jim_Obj *objPtr;

    savedFramePtr = interp->framePtr;
    interp->framePtr = interp->topFramePtr;
    objPtr = Jim_GetVariable(interp, nameObjPtr, flags);
    interp->framePtr = savedFramePtr;

    return objPtr;
}

Jim_Obj *Jim_GetVariableStr(Jim_Interp *interp, const char *name, int flags)
{
    Jim_Obj *nameObjPtr, *varObjPtr;

    nameObjPtr = Jim_NewStringObj(interp, name, -1);
    Jim_IncrRefCount(nameObjPtr);
    varObjPtr = Jim_GetVariable(interp, nameObjPtr, flags);
    Jim_DecrRefCount(interp, nameObjPtr);
    return varObjPtr;
}

Jim_Obj *Jim_GetGlobalVariableStr(Jim_Interp *interp, const char *name, int flags)
{
    Jim_CallFrame *savedFramePtr;
    Jim_Obj *objPtr;

    savedFramePtr = interp->framePtr;
    interp->framePtr = interp->topFramePtr;
    objPtr = Jim_GetVariableStr(interp, name, flags);
    interp->framePtr = savedFramePtr;

    return objPtr;
}

/* Unset a variable.
 * Note: On success unset invalidates all the variable objects created
 * in the current call frame incrementing. */
int Jim_UnsetVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr, int flags)
{
    Jim_Var *varPtr;
    int retval;
    Jim_CallFrame *framePtr;

    retval = SetVariableFromAny(interp, nameObjPtr);
    if (retval == JIM_DICT_SUGAR) {
        /* [dict] syntax sugar. */
        return JimDictSugarSet(interp, nameObjPtr, NULL);
    }
    else if (retval == JIM_OK) {
        varPtr = nameObjPtr->internalRep.varValue.varPtr;

        /* If it's a link call UnsetVariable recursively */
        if (varPtr->linkFramePtr) {
            framePtr = interp->framePtr;
            interp->framePtr = varPtr->linkFramePtr;
            retval = Jim_UnsetVariable(interp, varPtr->objPtr, JIM_NONE);
            interp->framePtr = framePtr;
        }
        else {
            const char *name = Jim_String(nameObjPtr);
            if (nameObjPtr->internalRep.varValue.global) {
                name += 2;
                framePtr = interp->topFramePtr;
            }
            else {
                framePtr = interp->framePtr;
            }

            retval = Jim_DeleteHashEntry(&framePtr->vars, name);
            if (retval == JIM_OK) {
                /* Change the callframe id, invalidating var lookup caching */
                framePtr->id = interp->callFrameEpoch++;
            }
        }
    }
    if (retval != JIM_OK && (flags & JIM_ERRMSG)) {
        Jim_SetResultFormatted(interp, "can't unset \"%#s\": no such variable", nameObjPtr);
    }
    return retval;
}

/* ----------  Dict syntax sugar (similar to array Tcl syntax) -------------- */

/* Given a variable name for [dict] operation syntax sugar,
 * this function returns two objects, the first with the name
 * of the variable to set, and the second with the respective key.
 * For example "foo(bar)" will return objects with string repr. of
 * "foo" and "bar".
 *
 * The returned objects have refcount = 1. The function can't fail. */
static void JimDictSugarParseVarKey(Jim_Interp *interp, Jim_Obj *objPtr,
    Jim_Obj **varPtrPtr, Jim_Obj **keyPtrPtr)
{
    const char *str, *p;
    int len, keyLen;
    Jim_Obj *varObjPtr, *keyObjPtr;

    str = Jim_GetString(objPtr, &len);

    p = strchr(str, '(');
    JimPanic((p == NULL, "JimDictSugarParseVarKey() called for non-dict-sugar (%s)", str));

    varObjPtr = Jim_NewStringObj(interp, str, p - str);

    p++;
    keyLen = (str + len) - p;
    if (str[len - 1] == ')') {
        keyLen--;
    }

    /* Create the objects with the variable name and key. */
    keyObjPtr = Jim_NewStringObj(interp, p, keyLen);

    Jim_IncrRefCount(varObjPtr);
    Jim_IncrRefCount(keyObjPtr);
    *varPtrPtr = varObjPtr;
    *keyPtrPtr = keyObjPtr;
}

/* Helper of Jim_SetVariable() to deal with dict-syntax variable names.
 * Also used by Jim_UnsetVariable() with valObjPtr = NULL. */
static int JimDictSugarSet(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *valObjPtr)
{
    int err;

    SetDictSubstFromAny(interp, objPtr);

    err = Jim_SetDictKeysVector(interp, objPtr->internalRep.dictSubstValue.varNameObjPtr,
        &objPtr->internalRep.dictSubstValue.indexObjPtr, 1, valObjPtr, JIM_MUSTEXIST);

    if (err == JIM_OK) {
        /* Don't keep an extra ref to the result */
        Jim_SetEmptyResult(interp);
    }
    else {
        if (!valObjPtr) {
            /* Better error message for unset a(2) where a exists but a(2) doesn't */
            if (Jim_GetVariable(interp, objPtr->internalRep.dictSubstValue.varNameObjPtr, JIM_NONE)) {
                Jim_SetResultFormatted(interp, "can't unset \"%#s\": no such element in array",
                    objPtr);
                return err;
            }
        }
        /* Make the error more informative and Tcl-compatible */
        Jim_SetResultFormatted(interp, "can't %s \"%#s\": variable isn't array",
            (valObjPtr ? "set" : "unset"), objPtr);
    }
    return err;
}

/**
 * Expands the array variable (dict sugar) and returns the result, or NULL on error.
 *
 * If JIM_UNSHARED is set and the dictionary is shared, it will be duplicated
 * and stored back to the variable before expansion.
 */
static Jim_Obj *JimDictExpandArrayVariable(Jim_Interp *interp, Jim_Obj *varObjPtr,
    Jim_Obj *keyObjPtr, int flags)
{
    Jim_Obj *dictObjPtr;
    Jim_Obj *resObjPtr = NULL;
    int ret;

    dictObjPtr = Jim_GetVariable(interp, varObjPtr, JIM_ERRMSG);
    if (!dictObjPtr) {
        return NULL;
    }

    ret = Jim_DictKey(interp, dictObjPtr, keyObjPtr, &resObjPtr, JIM_NONE);
    if (ret != JIM_OK) {
        Jim_SetResultFormatted(interp,
            "can't read \"%#s(%#s)\": %s array", varObjPtr, keyObjPtr,
            ret < 0 ? "variable isn't" : "no such element in");
    }
    else if ((flags & JIM_UNSHARED) && Jim_IsShared(dictObjPtr)) {
        /* Update the variable to have an unshared copy */
        Jim_SetVariable(interp, varObjPtr, Jim_DuplicateObj(interp, dictObjPtr));
    }

    return resObjPtr;
}

/* Helper of Jim_GetVariable() to deal with dict-syntax variable names */
static Jim_Obj *JimDictSugarGet(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    SetDictSubstFromAny(interp, objPtr);

    return JimDictExpandArrayVariable(interp,
        objPtr->internalRep.dictSubstValue.varNameObjPtr,
        objPtr->internalRep.dictSubstValue.indexObjPtr, flags);
}

/* --------- $var(INDEX) substitution, using a specialized object ----------- */

void FreeDictSubstInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_DecrRefCount(interp, objPtr->internalRep.dictSubstValue.varNameObjPtr);
    Jim_DecrRefCount(interp, objPtr->internalRep.dictSubstValue.indexObjPtr);
}

void DupDictSubstInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    JIM_NOTUSED(interp);

    dupPtr->internalRep.dictSubstValue.varNameObjPtr =
        srcPtr->internalRep.dictSubstValue.varNameObjPtr;
    dupPtr->internalRep.dictSubstValue.indexObjPtr = srcPtr->internalRep.dictSubstValue.indexObjPtr;
    dupPtr->typePtr = &dictSubstObjType;
}

/* Note: The object *must* be in dict-sugar format */
static void SetDictSubstFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (objPtr->typePtr != &dictSubstObjType) {
        Jim_Obj *varObjPtr, *keyObjPtr;

        if (objPtr->typePtr == &interpolatedObjType) {
            /* An interpolated object in dict-sugar form */

            varObjPtr = objPtr->internalRep.dictSubstValue.varNameObjPtr;
            keyObjPtr = objPtr->internalRep.dictSubstValue.indexObjPtr;

            Jim_IncrRefCount(varObjPtr);
            Jim_IncrRefCount(keyObjPtr);
        }
        else {
            JimDictSugarParseVarKey(interp, objPtr, &varObjPtr, &keyObjPtr);
        }

        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &dictSubstObjType;
        objPtr->internalRep.dictSubstValue.varNameObjPtr = varObjPtr;
        objPtr->internalRep.dictSubstValue.indexObjPtr = keyObjPtr;
    }
}

/* This function is used to expand [dict get] sugar in the form
 * of $var(INDEX). The function is mainly used by Jim_EvalObj()
 * to deal with tokens of type JIM_TT_DICTSUGAR. objPtr points to an
 * object that is *guaranteed* to be in the form VARNAME(INDEX).
 * The 'index' part is [subst]ituted, and is used to lookup a key inside
 * the [dict]ionary contained in variable VARNAME. */
static Jim_Obj *JimExpandDictSugar(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_Obj *resObjPtr = NULL;
    Jim_Obj *substKeyObjPtr = NULL;

    SetDictSubstFromAny(interp, objPtr);

    if (Jim_SubstObj(interp, objPtr->internalRep.dictSubstValue.indexObjPtr,
            &substKeyObjPtr, JIM_NONE)
        != JIM_OK) {
        return NULL;
    }
    Jim_IncrRefCount(substKeyObjPtr);
    resObjPtr =
        JimDictExpandArrayVariable(interp, objPtr->internalRep.dictSubstValue.varNameObjPtr,
        substKeyObjPtr, 0);
    Jim_DecrRefCount(interp, substKeyObjPtr);

    return resObjPtr;
}

static Jim_Obj *JimExpandExprSugar(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_Obj *resultObjPtr;

    if (Jim_EvalExpression(interp, objPtr, &resultObjPtr) == JIM_OK) {
        /* Note that the result has a ref count of 1, but we need a ref count of 0 */
        resultObjPtr->refCount--;
        return resultObjPtr;
    }
    return NULL;
}

/* -----------------------------------------------------------------------------
 * CallFrame
 * ---------------------------------------------------------------------------*/

static Jim_CallFrame *JimCreateCallFrame(Jim_Interp *interp, Jim_CallFrame *parent, Jim_Obj *nsObj)
{
    Jim_CallFrame *cf;

    if (interp->freeFramesList) {
        cf = interp->freeFramesList;
        interp->freeFramesList = cf->next;

        cf->argv = NULL;
        cf->argc = 0;
        cf->procArgsObjPtr = NULL;
        cf->procBodyObjPtr = NULL;
        cf->next = NULL;
        cf->staticVars = NULL;
        cf->localCommands = NULL;
        cf->tailcallObj = NULL;
        cf->tailcallCmd = NULL;
    }
    else {
        cf = Jim_Alloc(sizeof(*cf));
        memset(cf, 0, sizeof(*cf));

        Jim_InitHashTable(&cf->vars, &JimVariablesHashTableType, interp);
    }

    cf->id = interp->callFrameEpoch++;
    cf->parent = parent;
    cf->level = parent ? parent->level + 1 : 0;
    cf->nsObj = nsObj;
    Jim_IncrRefCount(nsObj);

    return cf;
}

static int JimDeleteLocalProcs(Jim_Interp *interp, Jim_Stack *localCommands)
{
    /* Delete any local procs */
    if (localCommands) {
        Jim_Obj *cmdNameObj;

        while ((cmdNameObj = Jim_StackPop(localCommands)) != NULL) {
            Jim_HashEntry *he;
            Jim_Obj *fqObjName;
            Jim_HashTable *ht = &interp->commands;

            const char *fqname = JimQualifyName(interp, Jim_String(cmdNameObj), &fqObjName);

            he = Jim_FindHashEntry(ht, fqname);

            if (he) {
                Jim_Cmd *cmd = Jim_GetHashEntryVal(he);
                if (cmd->prevCmd) {
                    Jim_Cmd *prevCmd = cmd->prevCmd;
                    cmd->prevCmd = NULL;

                    /* Delete the old command */
                    JimDecrCmdRefCount(interp, cmd);

                    /* And restore the original */
                    Jim_SetHashVal(ht, he, prevCmd);
                }
                else {
                    Jim_DeleteHashEntry(ht, fqname);
                    Jim_InterpIncrProcEpoch(interp);
                }
            }
            Jim_DecrRefCount(interp, cmdNameObj);
            JimFreeQualifiedName(interp, fqObjName);
        }
        Jim_FreeStack(localCommands);
        Jim_Free(localCommands);
    }
    return JIM_OK;
}


#define JIM_FCF_FULL 0          /* Always free the vars hash table */
#define JIM_FCF_REUSE 1         /* Reuse the vars hash table if possible */
static void JimFreeCallFrame(Jim_Interp *interp, Jim_CallFrame *cf, int action)
 {
    JimDeleteLocalProcs(interp, cf->localCommands);

    if (cf->procArgsObjPtr)
        Jim_DecrRefCount(interp, cf->procArgsObjPtr);
    if (cf->procBodyObjPtr)
        Jim_DecrRefCount(interp, cf->procBodyObjPtr);
    Jim_DecrRefCount(interp, cf->nsObj);
    if (action == JIM_FCF_FULL || cf->vars.size != JIM_HT_INITIAL_SIZE)
        Jim_FreeHashTable(&cf->vars);
    else {
        int i;
        Jim_HashEntry **table = cf->vars.table, *he;

        for (i = 0; i < JIM_HT_INITIAL_SIZE; i++) {
            he = table[i];
            while (he != NULL) {
                Jim_HashEntry *nextEntry = he->next;
                Jim_Var *varPtr = Jim_GetHashEntryVal(he);

                Jim_DecrRefCount(interp, varPtr->objPtr);
                Jim_Free(Jim_GetHashEntryKey(he));
                Jim_Free(varPtr);
                Jim_Free(he);
                table[i] = NULL;
                he = nextEntry;
            }
        }
        cf->vars.used = 0;
    }
    cf->next = interp->freeFramesList;
    interp->freeFramesList = cf;
}


/* -----------------------------------------------------------------------------
 * References
 * ---------------------------------------------------------------------------*/
#ifdef JIM_REFERENCES

/* References HashTable Type.
 *
 * Keys are unsigned long integers, dynamically allocated for now but in the
 * future it's worth to cache this 4 bytes objects. Values are pointers
 * to Jim_References. */
static void JimReferencesHTValDestructor(void *interp, void *val)
{
    Jim_Reference *refPtr = (void *)val;

    Jim_DecrRefCount(interp, refPtr->objPtr);
    if (refPtr->finalizerCmdNamePtr != NULL) {
        Jim_DecrRefCount(interp, refPtr->finalizerCmdNamePtr);
    }
    Jim_Free(val);
}

static unsigned int JimReferencesHTHashFunction(const void *key)
{
    /* Only the least significant bits are used. */
    const unsigned long *widePtr = key;
    unsigned int intValue = (unsigned int)*widePtr;

    return Jim_IntHashFunction(intValue);
}

static void *JimReferencesHTKeyDup(void *privdata, const void *key)
{
    void *copy = Jim_Alloc(sizeof(unsigned long));

    JIM_NOTUSED(privdata);

    memcpy(copy, key, sizeof(unsigned long));
    return copy;
}

static int JimReferencesHTKeyCompare(void *privdata, const void *key1, const void *key2)
{
    JIM_NOTUSED(privdata);

    return memcmp(key1, key2, sizeof(unsigned long)) == 0;
}

static void JimReferencesHTKeyDestructor(void *privdata, void *key)
{
    JIM_NOTUSED(privdata);

    Jim_Free(key);
}

static const Jim_HashTableType JimReferencesHashTableType = {
    JimReferencesHTHashFunction,        /* hash function */
    JimReferencesHTKeyDup,      /* key dup */
    NULL,                       /* val dup */
    JimReferencesHTKeyCompare,  /* key compare */
    JimReferencesHTKeyDestructor,       /* key destructor */
    JimReferencesHTValDestructor        /* val destructor */
};

/* -----------------------------------------------------------------------------
 * Reference object type and References API
 * ---------------------------------------------------------------------------*/

/* The string representation of references has two features in order
 * to make the GC faster. The first is that every reference starts
 * with a non common character '<', in order to make the string matching
 * faster. The second is that the reference string rep is 42 characters
 * in length, this means that it is not necessary to check any object with a string
 * repr < 42, and usually there aren't many of these objects. */

#define JIM_REFERENCE_SPACE (35+JIM_REFERENCE_TAGLEN)

static int JimFormatReference(char *buf, Jim_Reference *refPtr, unsigned long id)
{
    const char *fmt = "<reference.<%s>.%020lu>";

    sprintf(buf, fmt, refPtr->tag, id);
    return JIM_REFERENCE_SPACE;
}

static void UpdateStringOfReference(struct Jim_Obj *objPtr);

static const Jim_ObjType referenceObjType = {
    "reference",
    NULL,
    NULL,
    UpdateStringOfReference,
    JIM_TYPE_REFERENCES,
};

static void UpdateStringOfReference(struct Jim_Obj *objPtr)
{
    char buf[JIM_REFERENCE_SPACE + 1];

    JimFormatReference(buf, objPtr->internalRep.refValue.refPtr, objPtr->internalRep.refValue.id);
    JimSetStringBytes(objPtr, buf);
}

/* returns true if 'c' is a valid reference tag character.
 * i.e. inside the range [_a-zA-Z0-9] */
static int isrefchar(int c)
{
    return (c == '_' || isalnum(c));
}

static int SetReferenceFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    unsigned long value;
    int i, len;
    const char *str, *start, *end;
    char refId[21];
    Jim_Reference *refPtr;
    Jim_HashEntry *he;
    char *endptr;

    /* Get the string representation */
    str = Jim_GetString(objPtr, &len);
    /* Check if it looks like a reference */
    if (len < JIM_REFERENCE_SPACE)
        goto badformat;
    /* Trim spaces */
    start = str;
    end = str + len - 1;
    while (*start == ' ')
        start++;
    while (*end == ' ' && end > start)
        end--;
    if (end - start + 1 != JIM_REFERENCE_SPACE)
        goto badformat;
    /* <reference.<1234567>.%020> */
    if (memcmp(start, "<reference.<", 12) != 0)
        goto badformat;
    if (start[12 + JIM_REFERENCE_TAGLEN] != '>' || end[0] != '>')
        goto badformat;
    /* The tag can't contain chars other than a-zA-Z0-9 + '_'. */
    for (i = 0; i < JIM_REFERENCE_TAGLEN; i++) {
        if (!isrefchar(start[12 + i]))
            goto badformat;
    }
    /* Extract info from the reference. */
    memcpy(refId, start + 14 + JIM_REFERENCE_TAGLEN, 20);
    refId[20] = '\0';
    /* Try to convert the ID into an unsigned long */
    value = strtoul(refId, &endptr, 10);
    if (JimCheckConversion(refId, endptr) != JIM_OK)
        goto badformat;
    /* Check if the reference really exists! */
    he = Jim_FindHashEntry(&interp->references, &value);
    if (he == NULL) {
        Jim_SetResultFormatted(interp, "invalid reference id \"%#s\"", objPtr);
        return JIM_ERR;
    }
    refPtr = Jim_GetHashEntryVal(he);
    /* Free the old internal repr and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &referenceObjType;
    objPtr->internalRep.refValue.id = value;
    objPtr->internalRep.refValue.refPtr = refPtr;
    return JIM_OK;

  badformat:
    Jim_SetResultFormatted(interp, "expected reference but got \"%#s\"", objPtr);
    return JIM_ERR;
}

/* Returns a new reference pointing to objPtr, having cmdNamePtr
 * as finalizer command (or NULL if there is no finalizer).
 * The returned reference object has refcount = 0. */
Jim_Obj *Jim_NewReference(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *tagPtr, Jim_Obj *cmdNamePtr)
{
    struct Jim_Reference *refPtr;
    unsigned long id;
    Jim_Obj *refObjPtr;
    const char *tag;
    int tagLen, i;

    /* Perform the Garbage Collection if needed. */
    Jim_CollectIfNeeded(interp);

    refPtr = Jim_Alloc(sizeof(*refPtr));
    refPtr->objPtr = objPtr;
    Jim_IncrRefCount(objPtr);
    refPtr->finalizerCmdNamePtr = cmdNamePtr;
    if (cmdNamePtr)
        Jim_IncrRefCount(cmdNamePtr);
    id = interp->referenceNextId++;
    Jim_AddHashEntry(&interp->references, &id, refPtr);
    refObjPtr = Jim_NewObj(interp);
    refObjPtr->typePtr = &referenceObjType;
    refObjPtr->bytes = NULL;
    refObjPtr->internalRep.refValue.id = id;
    refObjPtr->internalRep.refValue.refPtr = refPtr;
    interp->referenceNextId++;
    /* Set the tag. Trimmed at JIM_REFERENCE_TAGLEN. Everything
     * that does not pass the 'isrefchar' test is replaced with '_' */
    tag = Jim_GetString(tagPtr, &tagLen);
    if (tagLen > JIM_REFERENCE_TAGLEN)
        tagLen = JIM_REFERENCE_TAGLEN;
    for (i = 0; i < JIM_REFERENCE_TAGLEN; i++) {
        if (i < tagLen && isrefchar(tag[i]))
            refPtr->tag[i] = tag[i];
        else
            refPtr->tag[i] = '_';
    }
    refPtr->tag[JIM_REFERENCE_TAGLEN] = '\0';
    return refObjPtr;
}

Jim_Reference *Jim_GetReference(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (objPtr->typePtr != &referenceObjType && SetReferenceFromAny(interp, objPtr) == JIM_ERR)
        return NULL;
    return objPtr->internalRep.refValue.refPtr;
}

int Jim_SetFinalizer(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *cmdNamePtr)
{
    Jim_Reference *refPtr;

    if ((refPtr = Jim_GetReference(interp, objPtr)) == NULL)
        return JIM_ERR;
    Jim_IncrRefCount(cmdNamePtr);
    if (refPtr->finalizerCmdNamePtr)
        Jim_DecrRefCount(interp, refPtr->finalizerCmdNamePtr);
    refPtr->finalizerCmdNamePtr = cmdNamePtr;
    return JIM_OK;
}

int Jim_GetFinalizer(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj **cmdNamePtrPtr)
{
    Jim_Reference *refPtr;

    if ((refPtr = Jim_GetReference(interp, objPtr)) == NULL)
        return JIM_ERR;
    *cmdNamePtrPtr = refPtr->finalizerCmdNamePtr;
    return JIM_OK;
}

/* -----------------------------------------------------------------------------
 * References Garbage Collection
 * ---------------------------------------------------------------------------*/

/* This the hash table type for the "MARK" phase of the GC */
static const Jim_HashTableType JimRefMarkHashTableType = {
    JimReferencesHTHashFunction,        /* hash function */
    JimReferencesHTKeyDup,      /* key dup */
    NULL,                       /* val dup */
    JimReferencesHTKeyCompare,  /* key compare */
    JimReferencesHTKeyDestructor,       /* key destructor */
    NULL                        /* val destructor */
};

/* Performs the garbage collection. */
int Jim_Collect(Jim_Interp *interp)
{
    int collected = 0;
#ifndef JIM_BOOTSTRAP
    Jim_HashTable marks;
    Jim_HashTableIterator htiter;
    Jim_HashEntry *he;
    Jim_Obj *objPtr;

    /* Avoid recursive calls */
    if (interp->lastCollectId == -1) {
        /* Jim_Collect() already running. Return just now. */
        return 0;
    }
    interp->lastCollectId = -1;

    /* Mark all the references found into the 'mark' hash table.
     * The references are searched in every live object that
     * is of a type that can contain references. */
    Jim_InitHashTable(&marks, &JimRefMarkHashTableType, NULL);
    objPtr = interp->liveList;
    while (objPtr) {
        if (objPtr->typePtr == NULL || objPtr->typePtr->flags & JIM_TYPE_REFERENCES) {
            const char *str, *p;
            int len;

            /* If the object is of type reference, to get the
             * Id is simple... */
            if (objPtr->typePtr == &referenceObjType) {
                Jim_AddHashEntry(&marks, &objPtr->internalRep.refValue.id, NULL);
#ifdef JIM_DEBUG_GC
                printf("MARK (reference): %d refcount: %d\n",
                    (int)objPtr->internalRep.refValue.id, objPtr->refCount);
#endif
                objPtr = objPtr->nextObjPtr;
                continue;
            }
            /* Get the string repr of the object we want
             * to scan for references. */
            p = str = Jim_GetString(objPtr, &len);
            /* Skip objects too little to contain references. */
            if (len < JIM_REFERENCE_SPACE) {
                objPtr = objPtr->nextObjPtr;
                continue;
            }
            /* Extract references from the object string repr. */
            while (1) {
                int i;
                unsigned long id;

                if ((p = strstr(p, "<reference.<")) == NULL)
                    break;
                /* Check if it's a valid reference. */
                if (len - (p - str) < JIM_REFERENCE_SPACE)
                    break;
                if (p[41] != '>' || p[19] != '>' || p[20] != '.')
                    break;
                for (i = 21; i <= 40; i++)
                    if (!isdigit(UCHAR(p[i])))
                        break;
                /* Get the ID */
                id = strtoul(p + 21, NULL, 10);

                /* Ok, a reference for the given ID
                 * was found. Mark it. */
                Jim_AddHashEntry(&marks, &id, NULL);
#ifdef JIM_DEBUG_GC
                printf("MARK: %d\n", (int)id);
#endif
                p += JIM_REFERENCE_SPACE;
            }
        }
        objPtr = objPtr->nextObjPtr;
    }

    /* Run the references hash table to destroy every reference that
     * is not referenced outside (not present in the mark HT). */
    JimInitHashTableIterator(&interp->references, &htiter);
    while ((he = Jim_NextHashEntry(&htiter)) != NULL) {
        const unsigned long *refId;
        Jim_Reference *refPtr;

        refId = he->key;
        /* Check if in the mark phase we encountered
         * this reference. */
        if (Jim_FindHashEntry(&marks, refId) == NULL) {
#ifdef JIM_DEBUG_GC
            printf("COLLECTING %d\n", (int)*refId);
#endif
            collected++;
            /* Drop the reference, but call the
             * finalizer first if registered. */
            refPtr = Jim_GetHashEntryVal(he);
            if (refPtr->finalizerCmdNamePtr) {
                char *refstr = Jim_Alloc(JIM_REFERENCE_SPACE + 1);
                Jim_Obj *objv[3], *oldResult;

                JimFormatReference(refstr, refPtr, *refId);

                objv[0] = refPtr->finalizerCmdNamePtr;
                objv[1] = Jim_NewStringObjNoAlloc(interp, refstr, JIM_REFERENCE_SPACE);
                objv[2] = refPtr->objPtr;

                /* Drop the reference itself */
                /* Avoid the finaliser being freed here */
                Jim_IncrRefCount(objv[0]);
                /* Don't remove the reference from the hash table just yet
                 * since that will free refPtr, and hence refPtr->objPtr
                 */

                /* Call the finalizer. Errors ignored. (should we use bgerror?) */
                oldResult = interp->result;
                Jim_IncrRefCount(oldResult);
                Jim_EvalObjVector(interp, 3, objv);
                Jim_SetResult(interp, oldResult);
                Jim_DecrRefCount(interp, oldResult);

                Jim_DecrRefCount(interp, objv[0]);
            }
            Jim_DeleteHashEntry(&interp->references, refId);
        }
    }
    Jim_FreeHashTable(&marks);
    interp->lastCollectId = interp->referenceNextId;
    interp->lastCollectTime = time(NULL);
#endif /* JIM_BOOTSTRAP */
    return collected;
}

#define JIM_COLLECT_ID_PERIOD 5000
#define JIM_COLLECT_TIME_PERIOD 300

void Jim_CollectIfNeeded(Jim_Interp *interp)
{
    unsigned long elapsedId;
    int elapsedTime;

    elapsedId = interp->referenceNextId - interp->lastCollectId;
    elapsedTime = time(NULL) - interp->lastCollectTime;


    if (elapsedId > JIM_COLLECT_ID_PERIOD || elapsedTime > JIM_COLLECT_TIME_PERIOD) {
        Jim_Collect(interp);
    }
}
#endif

int Jim_IsBigEndian(void)
{
    union {
        unsigned short s;
        unsigned char c[2];
    } uval = {0x0102};

    return uval.c[0] == 1;
}

/* -----------------------------------------------------------------------------
 * Interpreter related functions
 * ---------------------------------------------------------------------------*/

Jim_Interp *Jim_CreateInterp(void)
{
    Jim_Interp *i = Jim_Alloc(sizeof(*i));

    memset(i, 0, sizeof(*i));

    i->maxCallFrameDepth = JIM_MAX_CALLFRAME_DEPTH;
    i->maxEvalDepth = JIM_MAX_EVAL_DEPTH;
    i->lastCollectTime = time(NULL);

    /* Note that we can create objects only after the
     * interpreter liveList and freeList pointers are
     * initialized to NULL. */
    Jim_InitHashTable(&i->commands, &JimCommandsHashTableType, i);
#ifdef JIM_REFERENCES
    Jim_InitHashTable(&i->references, &JimReferencesHashTableType, i);
#endif
    Jim_InitHashTable(&i->assocData, &JimAssocDataHashTableType, i);
    Jim_InitHashTable(&i->packages, &JimPackageHashTableType, NULL);
    i->emptyObj = Jim_NewEmptyStringObj(i);
    i->trueObj = Jim_NewIntObj(i, 1);
    i->falseObj = Jim_NewIntObj(i, 0);
    i->framePtr = i->topFramePtr = JimCreateCallFrame(i, NULL, i->emptyObj);
    i->errorFileNameObj = i->emptyObj;
    i->result = i->emptyObj;
    i->stackTrace = Jim_NewListObj(i, NULL, 0);
    i->unknown = Jim_NewStringObj(i, "unknown", -1);
    i->errorProc = i->emptyObj;
    i->currentScriptObj = Jim_NewEmptyStringObj(i);
    i->nullScriptObj = Jim_NewEmptyStringObj(i);
    Jim_IncrRefCount(i->emptyObj);
    Jim_IncrRefCount(i->errorFileNameObj);
    Jim_IncrRefCount(i->result);
    Jim_IncrRefCount(i->stackTrace);
    Jim_IncrRefCount(i->unknown);
    Jim_IncrRefCount(i->currentScriptObj);
    Jim_IncrRefCount(i->nullScriptObj);
    Jim_IncrRefCount(i->errorProc);
    Jim_IncrRefCount(i->trueObj);
    Jim_IncrRefCount(i->falseObj);

    /* Initialize key variables every interpreter should contain */
    Jim_SetVariableStrWithStr(i, JIM_LIBPATH, TCL_LIBRARY);
    Jim_SetVariableStrWithStr(i, JIM_INTERACTIVE, "0");

    Jim_SetVariableStrWithStr(i, "tcl_platform(engine)", "Jim");
    Jim_SetVariableStrWithStr(i, "tcl_platform(os)", TCL_PLATFORM_OS);
    Jim_SetVariableStrWithStr(i, "tcl_platform(platform)", TCL_PLATFORM_PLATFORM);
    Jim_SetVariableStrWithStr(i, "tcl_platform(pathSeparator)", TCL_PLATFORM_PATH_SEPARATOR);
    Jim_SetVariableStrWithStr(i, "tcl_platform(byteOrder)", Jim_IsBigEndian() ? "bigEndian" : "littleEndian");
    Jim_SetVariableStrWithStr(i, "tcl_platform(threaded)", "0");
    Jim_SetVariableStr(i, "tcl_platform(pointerSize)", Jim_NewIntObj(i, sizeof(void *)));
    Jim_SetVariableStr(i, "tcl_platform(wordSize)", Jim_NewIntObj(i, sizeof(jim_wide)));

    return i;
}

void Jim_FreeInterp(Jim_Interp *i)
{
    Jim_CallFrame *cf, *cfx;

    Jim_Obj *objPtr, *nextObjPtr;

    /* Free the active call frames list - must be done before i->commands is destroyed */
    for (cf = i->framePtr; cf; cf = cfx) {
        cfx = cf->parent;
        JimFreeCallFrame(i, cf, JIM_FCF_FULL);
    }

    Jim_DecrRefCount(i, i->emptyObj);
    Jim_DecrRefCount(i, i->trueObj);
    Jim_DecrRefCount(i, i->falseObj);
    Jim_DecrRefCount(i, i->result);
    Jim_DecrRefCount(i, i->stackTrace);
    Jim_DecrRefCount(i, i->errorProc);
    Jim_DecrRefCount(i, i->unknown);
    Jim_DecrRefCount(i, i->errorFileNameObj);
    Jim_DecrRefCount(i, i->currentScriptObj);
    Jim_DecrRefCount(i, i->nullScriptObj);
    Jim_FreeHashTable(&i->commands);
#ifdef JIM_REFERENCES
    Jim_FreeHashTable(&i->references);
#endif
    Jim_FreeHashTable(&i->packages);
    Jim_Free(i->prngState);
    Jim_FreeHashTable(&i->assocData);

    /* Check that the live object list is empty, otherwise
     * there is a memory leak. */
#ifdef JIM_MAINTAINER
    if (i->liveList != NULL) {
        objPtr = i->liveList;

        printf("\n-------------------------------------\n");
        printf("Objects still in the free list:\n");
        while (objPtr) {
            const char *type = objPtr->typePtr ? objPtr->typePtr->name : "string";

            if (objPtr->bytes && strlen(objPtr->bytes) > 20) {
                printf("%p (%d) %-10s: '%.20s...'\n",
                    (void *)objPtr, objPtr->refCount, type, objPtr->bytes);
            }
            else {
                printf("%p (%d) %-10s: '%s'\n",
                    (void *)objPtr, objPtr->refCount, type, objPtr->bytes ? objPtr->bytes : "(null)");
            }
            if (objPtr->typePtr == &sourceObjType) {
                printf("FILE %s LINE %d\n",
                    Jim_String(objPtr->internalRep.sourceValue.fileNameObj),
                    objPtr->internalRep.sourceValue.lineNumber);
            }
            objPtr = objPtr->nextObjPtr;
        }
        printf("-------------------------------------\n\n");
        JimPanic((1, "Live list non empty freeing the interpreter! Leak?"));
    }
#endif

    /* Free all the freed objects. */
    objPtr = i->freeList;
    while (objPtr) {
        nextObjPtr = objPtr->nextObjPtr;
        Jim_Free(objPtr);
        objPtr = nextObjPtr;
    }

    /* Free the free call frames list */
    for (cf = i->freeFramesList; cf; cf = cfx) {
        cfx = cf->next;
        if (cf->vars.table)
            Jim_FreeHashTable(&cf->vars);
        Jim_Free(cf);
    }

    /* Free the interpreter structure. */
    Jim_Free(i);
}

/* Returns the call frame relative to the level represented by
 * levelObjPtr. If levelObjPtr == NULL, the level is assumed to be '1'.
 *
 * This function accepts the 'level' argument in the form
 * of the commands [uplevel] and [upvar].
 *
 * Returns NULL on error.
 *
 * Note: for a function accepting a relative integer as level suitable
 * for implementation of [info level ?level?], see JimGetCallFrameByInteger()
 */
Jim_CallFrame *Jim_GetCallFrameByLevel(Jim_Interp *interp, Jim_Obj *levelObjPtr)
{
    long level;
    const char *str;
    Jim_CallFrame *framePtr;

    if (levelObjPtr) {
        str = Jim_String(levelObjPtr);
        if (str[0] == '#') {
            char *endptr;

            level = jim_strtol(str + 1, &endptr);
            if (str[1] == '\0' || endptr[0] != '\0') {
                level = -1;
            }
        }
        else {
            if (Jim_GetLong(interp, levelObjPtr, &level) != JIM_OK || level < 0) {
                level = -1;
            }
            else {
                /* Convert from a relative to an absolute level */
                level = interp->framePtr->level - level;
            }
        }
    }
    else {
        str = "1";              /* Needed to format the error message. */
        level = interp->framePtr->level - 1;
    }

    if (level == 0) {
        return interp->topFramePtr;
    }
    if (level > 0) {
        /* Lookup */
        for (framePtr = interp->framePtr; framePtr; framePtr = framePtr->parent) {
            if (framePtr->level == level) {
                return framePtr;
            }
        }
    }

    Jim_SetResultFormatted(interp, "bad level \"%s\"", str);
    return NULL;
}

/* Similar to Jim_GetCallFrameByLevel() but the level is specified
 * as a relative integer like in the [info level ?level?] command.
 **/
static Jim_CallFrame *JimGetCallFrameByInteger(Jim_Interp *interp, Jim_Obj *levelObjPtr)
{
    long level;
    Jim_CallFrame *framePtr;

    if (Jim_GetLong(interp, levelObjPtr, &level) == JIM_OK) {
        if (level <= 0) {
            /* Convert from a relative to an absolute level */
            level = interp->framePtr->level + level;
        }

        if (level == 0) {
            return interp->topFramePtr;
        }

        /* Lookup */
        for (framePtr = interp->framePtr; framePtr; framePtr = framePtr->parent) {
            if (framePtr->level == level) {
                return framePtr;
            }
        }
    }

    Jim_SetResultFormatted(interp, "bad level \"%#s\"", levelObjPtr);
    return NULL;
}

static void JimResetStackTrace(Jim_Interp *interp)
{
    Jim_DecrRefCount(interp, interp->stackTrace);
    interp->stackTrace = Jim_NewListObj(interp, NULL, 0);
    Jim_IncrRefCount(interp->stackTrace);
}

static void JimSetStackTrace(Jim_Interp *interp, Jim_Obj *stackTraceObj)
{
    int len;

    /* Increment reference first in case these are the same object */
    Jim_IncrRefCount(stackTraceObj);
    Jim_DecrRefCount(interp, interp->stackTrace);
    interp->stackTrace = stackTraceObj;
    interp->errorFlag = 1;

    /* This is a bit ugly.
     * If the filename of the last entry of the stack trace is empty,
     * the next stack level should be added.
     */
    len = Jim_ListLength(interp, interp->stackTrace);
    if (len >= 3) {
        if (Jim_Length(Jim_ListGetIndex(interp, interp->stackTrace, len - 2)) == 0) {
            interp->addStackTrace = 1;
        }
    }
}

static void JimAppendStackTrace(Jim_Interp *interp, const char *procname,
    Jim_Obj *fileNameObj, int linenr)
{
    if (strcmp(procname, "unknown") == 0) {
        procname = "";
    }
    if (!*procname && !Jim_Length(fileNameObj)) {
        /* No useful info here */
        return;
    }

    if (Jim_IsShared(interp->stackTrace)) {
        Jim_DecrRefCount(interp, interp->stackTrace);
        interp->stackTrace = Jim_DuplicateObj(interp, interp->stackTrace);
        Jim_IncrRefCount(interp->stackTrace);
    }

    /* If we have no procname but the previous element did, merge with that frame */
    if (!*procname && Jim_Length(fileNameObj)) {
        /* Just a filename. Check the previous entry */
        int len = Jim_ListLength(interp, interp->stackTrace);

        if (len >= 3) {
            Jim_Obj *objPtr = Jim_ListGetIndex(interp, interp->stackTrace, len - 3);
            if (Jim_Length(objPtr)) {
                /* Yes, the previous level had procname */
                objPtr = Jim_ListGetIndex(interp, interp->stackTrace, len - 2);
                if (Jim_Length(objPtr) == 0) {
                    /* But no filename, so merge the new info with that frame */
                    ListSetIndex(interp, interp->stackTrace, len - 2, fileNameObj, 0);
                    ListSetIndex(interp, interp->stackTrace, len - 1, Jim_NewIntObj(interp, linenr), 0);
                    return;
                }
            }
        }
    }

    Jim_ListAppendElement(interp, interp->stackTrace, Jim_NewStringObj(interp, procname, -1));
    Jim_ListAppendElement(interp, interp->stackTrace, fileNameObj);
    Jim_ListAppendElement(interp, interp->stackTrace, Jim_NewIntObj(interp, linenr));
}

int Jim_SetAssocData(Jim_Interp *interp, const char *key, Jim_InterpDeleteProc * delProc,
    void *data)
{
    AssocDataValue *assocEntryPtr = (AssocDataValue *) Jim_Alloc(sizeof(AssocDataValue));

    assocEntryPtr->delProc = delProc;
    assocEntryPtr->data = data;
    return Jim_AddHashEntry(&interp->assocData, key, assocEntryPtr);
}

void *Jim_GetAssocData(Jim_Interp *interp, const char *key)
{
    Jim_HashEntry *entryPtr = Jim_FindHashEntry(&interp->assocData, key);

    if (entryPtr != NULL) {
        AssocDataValue *assocEntryPtr = Jim_GetHashEntryVal(entryPtr);
        return assocEntryPtr->data;
    }
    return NULL;
}

int Jim_DeleteAssocData(Jim_Interp *interp, const char *key)
{
    return Jim_DeleteHashEntry(&interp->assocData, key);
}

int Jim_GetExitCode(Jim_Interp *interp)
{
    return interp->exitCode;
}

/* -----------------------------------------------------------------------------
 * Integer object
 * ---------------------------------------------------------------------------*/
static void UpdateStringOfInt(struct Jim_Obj *objPtr);
static int SetIntFromAny(Jim_Interp *interp, Jim_Obj *objPtr, int flags);

static const Jim_ObjType intObjType = {
    "int",
    NULL,
    NULL,
    UpdateStringOfInt,
    JIM_TYPE_NONE,
};

/* A coerced double is closer to an int than a double.
 * It is an int value temporarily masquerading as a double value.
 * i.e. it has the same string value as an int and Jim_GetWide()
 * succeeds, but also Jim_GetDouble() returns the value directly.
 */
static const Jim_ObjType coercedDoubleObjType = {
    "coerced-double",
    NULL,
    NULL,
    UpdateStringOfInt,
    JIM_TYPE_NONE,
};


static void UpdateStringOfInt(struct Jim_Obj *objPtr)
{
    char buf[JIM_INTEGER_SPACE + 1];
    jim_wide wideValue = JimWideValue(objPtr);
    int pos = 0;

    if (wideValue == 0) {
        buf[pos++] = '0';
    }
    else {
        char tmp[JIM_INTEGER_SPACE];
        int num = 0;
        int i;

        if (wideValue < 0) {
            buf[pos++] = '-';
            i = wideValue % 10;
            /* C89 is implementation defined as to whether (-106 % 10) is -6 or 4,
             * whereas C99 is always -6
             * coverity[dead_error_line]
             */
            tmp[num++] = (i > 0) ? (10 - i) : -i;
            wideValue /= -10;
        }

        while (wideValue) {
            tmp[num++] = wideValue % 10;
            wideValue /= 10;
        }

        for (i = 0; i < num; i++) {
            buf[pos++] = '0' + tmp[num - i - 1];
        }
    }
    buf[pos] = 0;

    JimSetStringBytes(objPtr, buf);
}

static int SetIntFromAny(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    jim_wide wideValue;
    const char *str;

    if (objPtr->typePtr == &coercedDoubleObjType) {
        /* Simple switch */
        objPtr->typePtr = &intObjType;
        return JIM_OK;
    }

    /* Get the string representation */
    str = Jim_String(objPtr);
    /* Try to convert into a jim_wide */
    if (Jim_StringToWide(str, &wideValue, 0) != JIM_OK) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp, "expected integer but got \"%#s\"", objPtr);
        }
        return JIM_ERR;
    }
    if ((wideValue == JIM_WIDE_MIN || wideValue == JIM_WIDE_MAX) && errno == ERANGE) {
        Jim_SetResultString(interp, "Integer value too big to be represented", -1);
        return JIM_ERR;
    }
    /* Free the old internal repr and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &intObjType;
    objPtr->internalRep.wideValue = wideValue;
    return JIM_OK;
}

#ifdef JIM_OPTIMIZATION
static int JimIsWide(Jim_Obj *objPtr)
{
    return objPtr->typePtr == &intObjType;
}
#endif

int Jim_GetWide(Jim_Interp *interp, Jim_Obj *objPtr, jim_wide * widePtr)
{
    if (objPtr->typePtr != &intObjType && SetIntFromAny(interp, objPtr, JIM_ERRMSG) == JIM_ERR)
        return JIM_ERR;
    *widePtr = JimWideValue(objPtr);
    return JIM_OK;
}

/* Get a wide but does not set an error if the format is bad. */
static int JimGetWideNoErr(Jim_Interp *interp, Jim_Obj *objPtr, jim_wide * widePtr)
{
    if (objPtr->typePtr != &intObjType && SetIntFromAny(interp, objPtr, JIM_NONE) == JIM_ERR)
        return JIM_ERR;
    *widePtr = JimWideValue(objPtr);
    return JIM_OK;
}

int Jim_GetLong(Jim_Interp *interp, Jim_Obj *objPtr, long *longPtr)
{
    jim_wide wideValue;
    int retval;

    retval = Jim_GetWide(interp, objPtr, &wideValue);
    if (retval == JIM_OK) {
        *longPtr = (long)wideValue;
        return JIM_OK;
    }
    return JIM_ERR;
}

Jim_Obj *Jim_NewIntObj(Jim_Interp *interp, jim_wide wideValue)
{
    Jim_Obj *objPtr;

    objPtr = Jim_NewObj(interp);
    objPtr->typePtr = &intObjType;
    objPtr->bytes = NULL;
    objPtr->internalRep.wideValue = wideValue;
    return objPtr;
}

/* -----------------------------------------------------------------------------
 * Double object
 * ---------------------------------------------------------------------------*/
#define JIM_DOUBLE_SPACE 30

static void UpdateStringOfDouble(struct Jim_Obj *objPtr);
static int SetDoubleFromAny(Jim_Interp *interp, Jim_Obj *objPtr);

static const Jim_ObjType doubleObjType = {
    "double",
    NULL,
    NULL,
    UpdateStringOfDouble,
    JIM_TYPE_NONE,
};

#ifndef HAVE_ISNAN
#undef isnan
#define isnan(X) ((X) != (X))
#endif
#ifndef HAVE_ISINF
#undef isinf
#define isinf(X) (1.0 / (X) == 0.0)
#endif

static void UpdateStringOfDouble(struct Jim_Obj *objPtr)
{
    double value = objPtr->internalRep.doubleValue;

    if (isnan(value)) {
        JimSetStringBytes(objPtr, "NaN");
        return;
    }
    if (isinf(value)) {
        if (value < 0) {
            JimSetStringBytes(objPtr, "-Inf");
        }
        else {
            JimSetStringBytes(objPtr, "Inf");
        }
        return;
    }
    {
        char buf[JIM_DOUBLE_SPACE + 1];
        int i;
        int len = sprintf(buf, "%.12g", value);

        /* Add a final ".0" if necessary */
        for (i = 0; i < len; i++) {
            if (buf[i] == '.' || buf[i] == 'e') {
#if defined(JIM_SPRINTF_DOUBLE_NEEDS_FIX)
                /* If 'buf' ends in e-0nn or e+0nn, remove
                 * the 0 after the + or - and reduce the length by 1
                 */
                char *e = strchr(buf, 'e');
                if (e && (e[1] == '-' || e[1] == '+') && e[2] == '0') {
                    /* Move it up */
                    e += 2;
                    memmove(e, e + 1, len - (e - buf));
                }
#endif
                break;
            }
        }
        if (buf[i] == '\0') {
            buf[i++] = '.';
            buf[i++] = '0';
            buf[i] = '\0';
        }
        JimSetStringBytes(objPtr, buf);
    }
}

static int SetDoubleFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    double doubleValue;
    jim_wide wideValue;
    const char *str;

    /* Preserve the string representation.
     * Needed so we can convert back to int without loss
     */
    str = Jim_String(objPtr);

#ifdef HAVE_LONG_LONG
    /* Assume a 53 bit mantissa */
#define MIN_INT_IN_DOUBLE -(1LL << 53)
#define MAX_INT_IN_DOUBLE -(MIN_INT_IN_DOUBLE + 1)

    if (objPtr->typePtr == &intObjType
        && JimWideValue(objPtr) >= MIN_INT_IN_DOUBLE
        && JimWideValue(objPtr) <= MAX_INT_IN_DOUBLE) {

        /* Direct conversion to coerced double */
        objPtr->typePtr = &coercedDoubleObjType;
        return JIM_OK;
    }
    else
#endif
    if (Jim_StringToWide(str, &wideValue, 10) == JIM_OK) {
        /* Managed to convert to an int, so we can use this as a cooerced double */
        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &coercedDoubleObjType;
        objPtr->internalRep.wideValue = wideValue;
        return JIM_OK;
    }
    else {
        /* Try to convert into a double */
        if (Jim_StringToDouble(str, &doubleValue) != JIM_OK) {
            Jim_SetResultFormatted(interp, "expected floating-point number but got \"%#s\"", objPtr);
            return JIM_ERR;
        }
        /* Free the old internal repr and set the new one. */
        Jim_FreeIntRep(interp, objPtr);
    }
    objPtr->typePtr = &doubleObjType;
    objPtr->internalRep.doubleValue = doubleValue;
    return JIM_OK;
}

int Jim_GetDouble(Jim_Interp *interp, Jim_Obj *objPtr, double *doublePtr)
{
    if (objPtr->typePtr == &coercedDoubleObjType) {
        *doublePtr = JimWideValue(objPtr);
        return JIM_OK;
    }
    if (objPtr->typePtr != &doubleObjType && SetDoubleFromAny(interp, objPtr) == JIM_ERR)
        return JIM_ERR;

    if (objPtr->typePtr == &coercedDoubleObjType) {
        *doublePtr = JimWideValue(objPtr);
    }
    else {
        *doublePtr = objPtr->internalRep.doubleValue;
    }
    return JIM_OK;
}

Jim_Obj *Jim_NewDoubleObj(Jim_Interp *interp, double doubleValue)
{
    Jim_Obj *objPtr;

    objPtr = Jim_NewObj(interp);
    objPtr->typePtr = &doubleObjType;
    objPtr->bytes = NULL;
    objPtr->internalRep.doubleValue = doubleValue;
    return objPtr;
}

/* -----------------------------------------------------------------------------
 * List object
 * ---------------------------------------------------------------------------*/
static void ListInsertElements(Jim_Obj *listPtr, int idx, int elemc, Jim_Obj *const *elemVec);
static void ListAppendElement(Jim_Obj *listPtr, Jim_Obj *objPtr);
static void FreeListInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupListInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static void UpdateStringOfList(struct Jim_Obj *objPtr);
static int SetListFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

/* Note that while the elements of the list may contain references,
 * the list object itself can't. This basically means that the
 * list object string representation as a whole can't contain references
 * that are not presents in the single elements. */
static const Jim_ObjType listObjType = {
    "list",
    FreeListInternalRep,
    DupListInternalRep,
    UpdateStringOfList,
    JIM_TYPE_NONE,
};

void FreeListInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int i;

    for (i = 0; i < objPtr->internalRep.listValue.len; i++) {
        Jim_DecrRefCount(interp, objPtr->internalRep.listValue.ele[i]);
    }
    Jim_Free(objPtr->internalRep.listValue.ele);
}

void DupListInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    int i;

    JIM_NOTUSED(interp);

    dupPtr->internalRep.listValue.len = srcPtr->internalRep.listValue.len;
    dupPtr->internalRep.listValue.maxLen = srcPtr->internalRep.listValue.maxLen;
    dupPtr->internalRep.listValue.ele =
        Jim_Alloc(sizeof(Jim_Obj *) * srcPtr->internalRep.listValue.maxLen);
    memcpy(dupPtr->internalRep.listValue.ele, srcPtr->internalRep.listValue.ele,
        sizeof(Jim_Obj *) * srcPtr->internalRep.listValue.len);
    for (i = 0; i < dupPtr->internalRep.listValue.len; i++) {
        Jim_IncrRefCount(dupPtr->internalRep.listValue.ele[i]);
    }
    dupPtr->typePtr = &listObjType;
}

/* The following function checks if a given string can be encoded
 * into a list element without any kind of quoting, surrounded by braces,
 * or using escapes to quote. */
#define JIM_ELESTR_SIMPLE 0
#define JIM_ELESTR_BRACE 1
#define JIM_ELESTR_QUOTE 2
static unsigned char ListElementQuotingType(const char *s, int len)
{
    int i, level, blevel, trySimple = 1;

    /* Try with the SIMPLE case */
    if (len == 0)
        return JIM_ELESTR_BRACE;
    if (s[0] == '"' || s[0] == '{') {
        trySimple = 0;
        goto testbrace;
    }
    for (i = 0; i < len; i++) {
        switch (s[i]) {
            case ' ':
            case '$':
            case '"':
            case '[':
            case ']':
            case ';':
            case '\\':
            case '\r':
            case '\n':
            case '\t':
            case '\f':
            case '\v':
                trySimple = 0;
                /* fall through */
            case '{':
            case '}':
                goto testbrace;
        }
    }
    return JIM_ELESTR_SIMPLE;

  testbrace:
    /* Test if it's possible to do with braces */
    if (s[len - 1] == '\\')
        return JIM_ELESTR_QUOTE;
    level = 0;
    blevel = 0;
    for (i = 0; i < len; i++) {
        switch (s[i]) {
            case '{':
                level++;
                break;
            case '}':
                level--;
                if (level < 0)
                    return JIM_ELESTR_QUOTE;
                break;
            case '[':
                blevel++;
                break;
            case ']':
                blevel--;
                break;
            case '\\':
                if (s[i + 1] == '\n')
                    return JIM_ELESTR_QUOTE;
                else if (s[i + 1] != '\0')
                    i++;
                break;
        }
    }
    if (blevel < 0) {
        return JIM_ELESTR_QUOTE;
    }

    if (level == 0) {
        if (!trySimple)
            return JIM_ELESTR_BRACE;
        for (i = 0; i < len; i++) {
            switch (s[i]) {
                case ' ':
                case '$':
                case '"':
                case '[':
                case ']':
                case ';':
                case '\\':
                case '\r':
                case '\n':
                case '\t':
                case '\f':
                case '\v':
                    return JIM_ELESTR_BRACE;
                    break;
            }
        }
        return JIM_ELESTR_SIMPLE;
    }
    return JIM_ELESTR_QUOTE;
}

/* Backslashes-escapes the null-terminated string 's' into the buffer at 'q'
 * The buffer must be at least strlen(s) * 2 + 1 bytes long for the worst-case
 * scenario.
 * Returns the length of the result.
 */
static int BackslashQuoteString(const char *s, int len, char *q)
{
    char *p = q;

    while (len--) {
        switch (*s) {
            case ' ':
            case '$':
            case '"':
            case '[':
            case ']':
            case '{':
            case '}':
            case ';':
            case '\\':
                *p++ = '\\';
                *p++ = *s++;
                break;
            case '\n':
                *p++ = '\\';
                *p++ = 'n';
                s++;
                break;
            case '\r':
                *p++ = '\\';
                *p++ = 'r';
                s++;
                break;
            case '\t':
                *p++ = '\\';
                *p++ = 't';
                s++;
                break;
            case '\f':
                *p++ = '\\';
                *p++ = 'f';
                s++;
                break;
            case '\v':
                *p++ = '\\';
                *p++ = 'v';
                s++;
                break;
            default:
                *p++ = *s++;
                break;
        }
    }
    *p = '\0';

    return p - q;
}

static void JimMakeListStringRep(Jim_Obj *objPtr, Jim_Obj **objv, int objc)
{
    #define STATIC_QUOTING_LEN 32
    int i, bufLen, realLength;
    const char *strRep;
    char *p;
    unsigned char *quotingType, staticQuoting[STATIC_QUOTING_LEN];

    /* Estimate the space needed. */
    if (objc > STATIC_QUOTING_LEN) {
        quotingType = Jim_Alloc(objc);
    }
    else {
        quotingType = staticQuoting;
    }
    bufLen = 0;
    for (i = 0; i < objc; i++) {
        int len;

        strRep = Jim_GetString(objv[i], &len);
        quotingType[i] = ListElementQuotingType(strRep, len);
        switch (quotingType[i]) {
            case JIM_ELESTR_SIMPLE:
                if (i != 0 || strRep[0] != '#') {
                    bufLen += len;
                    break;
                }
                /* Special case '#' on first element needs braces */
                quotingType[i] = JIM_ELESTR_BRACE;
                /* fall through */
            case JIM_ELESTR_BRACE:
                bufLen += len + 2;
                break;
            case JIM_ELESTR_QUOTE:
                bufLen += len * 2;
                break;
        }
        bufLen++;               /* elements separator. */
    }
    bufLen++;

    /* Generate the string rep. */
    p = objPtr->bytes = Jim_Alloc(bufLen + 1);
    realLength = 0;
    for (i = 0; i < objc; i++) {
        int len, qlen;

        strRep = Jim_GetString(objv[i], &len);

        switch (quotingType[i]) {
            case JIM_ELESTR_SIMPLE:
                memcpy(p, strRep, len);
                p += len;
                realLength += len;
                break;
            case JIM_ELESTR_BRACE:
                *p++ = '{';
                memcpy(p, strRep, len);
                p += len;
                *p++ = '}';
                realLength += len + 2;
                break;
            case JIM_ELESTR_QUOTE:
                if (i == 0 && strRep[0] == '#') {
                    *p++ = '\\';
                    realLength++;
                }
                qlen = BackslashQuoteString(strRep, len, p);
                p += qlen;
                realLength += qlen;
                break;
        }
        /* Add a separating space */
        if (i + 1 != objc) {
            *p++ = ' ';
            realLength++;
        }
    }
    *p = '\0';                  /* nul term. */
    objPtr->length = realLength;

    if (quotingType != staticQuoting) {
        Jim_Free(quotingType);
    }
}

static void UpdateStringOfList(struct Jim_Obj *objPtr)
{
    JimMakeListStringRep(objPtr, objPtr->internalRep.listValue.ele, objPtr->internalRep.listValue.len);
}

static int SetListFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    struct JimParserCtx parser;
    const char *str;
    int strLen;
    Jim_Obj *fileNameObj;
    int linenr;

    if (objPtr->typePtr == &listObjType) {
        return JIM_OK;
    }

    /* Optimise dict -> list for object with no string rep. Note that this may only save a little time, but
     * it also preserves any source location of the dict elements
     * which can be very useful
     */
    if (Jim_IsDict(objPtr) && objPtr->bytes == NULL) {
        Jim_Obj **listObjPtrPtr;
        int len;
        int i;

        listObjPtrPtr = JimDictPairs(objPtr, &len);
        for (i = 0; i < len; i++) {
            Jim_IncrRefCount(listObjPtrPtr[i]);
        }

        /* Now just switch the internal rep */
        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &listObjType;
        objPtr->internalRep.listValue.len = len;
        objPtr->internalRep.listValue.maxLen = len;
        objPtr->internalRep.listValue.ele = listObjPtrPtr;

        return JIM_OK;
    }

    /* Try to preserve information about filename / line number */
    if (objPtr->typePtr == &sourceObjType) {
        fileNameObj = objPtr->internalRep.sourceValue.fileNameObj;
        linenr = objPtr->internalRep.sourceValue.lineNumber;
    }
    else {
        fileNameObj = interp->emptyObj;
        linenr = 1;
    }
    Jim_IncrRefCount(fileNameObj);

    /* Get the string representation */
    str = Jim_GetString(objPtr, &strLen);

    /* Free the old internal repr just now and initialize the
     * new one just now. The string->list conversion can't fail. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &listObjType;
    objPtr->internalRep.listValue.len = 0;
    objPtr->internalRep.listValue.maxLen = 0;
    objPtr->internalRep.listValue.ele = NULL;

    /* Convert into a list */
    if (strLen) {
        JimParserInit(&parser, str, strLen, linenr);
        while (!parser.eof) {
            Jim_Obj *elementPtr;

            JimParseList(&parser);
            if (parser.tt != JIM_TT_STR && parser.tt != JIM_TT_ESC)
                continue;
            elementPtr = JimParserGetTokenObj(interp, &parser);
            JimSetSourceInfo(interp, elementPtr, fileNameObj, parser.tline);
            ListAppendElement(objPtr, elementPtr);
        }
    }
    Jim_DecrRefCount(interp, fileNameObj);
    return JIM_OK;
}

Jim_Obj *Jim_NewListObj(Jim_Interp *interp, Jim_Obj *const *elements, int len)
{
    Jim_Obj *objPtr;

    objPtr = Jim_NewObj(interp);
    objPtr->typePtr = &listObjType;
    objPtr->bytes = NULL;
    objPtr->internalRep.listValue.ele = NULL;
    objPtr->internalRep.listValue.len = 0;
    objPtr->internalRep.listValue.maxLen = 0;

    if (len) {
        ListInsertElements(objPtr, 0, len, elements);
    }

    return objPtr;
}

/* Return a vector of Jim_Obj with the elements of a Jim list, and the
 * length of the vector. Note that the user of this function should make
 * sure that the list object can't shimmer while the vector returned
 * is in use, this vector is the one stored inside the internal representation
 * of the list object. This function is not exported, extensions should
 * always access to the List object elements using Jim_ListIndex(). */
static void JimListGetElements(Jim_Interp *interp, Jim_Obj *listObj, int *listLen,
    Jim_Obj ***listVec)
{
    *listLen = Jim_ListLength(interp, listObj);
    *listVec = listObj->internalRep.listValue.ele;
}

/* Sorting uses ints, but commands may return wide */
static int JimSign(jim_wide w)
{
    if (w == 0) {
        return 0;
    }
    else if (w < 0) {
        return -1;
    }
    return 1;
}

/* ListSortElements type values */
struct lsort_info {
    jmp_buf jmpbuf;
    Jim_Obj *command;
    Jim_Interp *interp;
    enum {
        JIM_LSORT_ASCII,
        JIM_LSORT_NOCASE,
        JIM_LSORT_INTEGER,
        JIM_LSORT_REAL,
        JIM_LSORT_COMMAND
    } type;
    int order;
    int index;
    int indexed;
    int unique;
    int (*subfn)(Jim_Obj **, Jim_Obj **);
};

static struct lsort_info *sort_info;

static int ListSortIndexHelper(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    Jim_Obj *lObj, *rObj;

    if (Jim_ListIndex(sort_info->interp, *lhsObj, sort_info->index, &lObj, JIM_ERRMSG) != JIM_OK ||
        Jim_ListIndex(sort_info->interp, *rhsObj, sort_info->index, &rObj, JIM_ERRMSG) != JIM_OK) {
        longjmp(sort_info->jmpbuf, JIM_ERR);
    }
    return sort_info->subfn(&lObj, &rObj);
}

/* Sort the internal rep of a list. */
static int ListSortString(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    return Jim_StringCompareObj(sort_info->interp, *lhsObj, *rhsObj, 0) * sort_info->order;
}

static int ListSortStringNoCase(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    return Jim_StringCompareObj(sort_info->interp, *lhsObj, *rhsObj, 1) * sort_info->order;
}

static int ListSortInteger(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    jim_wide lhs = 0, rhs = 0;

    if (Jim_GetWide(sort_info->interp, *lhsObj, &lhs) != JIM_OK ||
        Jim_GetWide(sort_info->interp, *rhsObj, &rhs) != JIM_OK) {
        longjmp(sort_info->jmpbuf, JIM_ERR);
    }

    return JimSign(lhs - rhs) * sort_info->order;
}

static int ListSortReal(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    double lhs = 0, rhs = 0;

    if (Jim_GetDouble(sort_info->interp, *lhsObj, &lhs) != JIM_OK ||
        Jim_GetDouble(sort_info->interp, *rhsObj, &rhs) != JIM_OK) {
        longjmp(sort_info->jmpbuf, JIM_ERR);
    }
    if (lhs == rhs) {
        return 0;
    }
    if (lhs > rhs) {
        return sort_info->order;
    }
    return -sort_info->order;
}

static int ListSortCommand(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    Jim_Obj *compare_script;
    int rc;

    jim_wide ret = 0;

    /* This must be a valid list */
    compare_script = Jim_DuplicateObj(sort_info->interp, sort_info->command);
    Jim_ListAppendElement(sort_info->interp, compare_script, *lhsObj);
    Jim_ListAppendElement(sort_info->interp, compare_script, *rhsObj);

    rc = Jim_EvalObj(sort_info->interp, compare_script);

    if (rc != JIM_OK || Jim_GetWide(sort_info->interp, Jim_GetResult(sort_info->interp), &ret) != JIM_OK) {
        longjmp(sort_info->jmpbuf, rc);
    }

    return JimSign(ret) * sort_info->order;
}

/* Remove duplicate elements from the (sorted) list in-place, according to the
 * comparison function, comp.
 *
 * Note that the last unique value is kept, not the first
 */
static void ListRemoveDuplicates(Jim_Obj *listObjPtr, int (*comp)(Jim_Obj **lhs, Jim_Obj **rhs))
{
    int src;
    int dst = 0;
    Jim_Obj **ele = listObjPtr->internalRep.listValue.ele;

    for (src = 1; src < listObjPtr->internalRep.listValue.len; src++) {
        if (comp(&ele[dst], &ele[src]) == 0) {
            /* Match, so replace the dest with the current source */
            Jim_DecrRefCount(sort_info->interp, ele[dst]);
        }
        else {
            /* No match, so keep the current source and move to the next destination */
            dst++;
        }
        ele[dst] = ele[src];
    }
    /* At end of list, keep the final element */
    ele[++dst] = ele[src];

    /* Set the new length */
    listObjPtr->internalRep.listValue.len = dst;
}

/* Sort a list *in place*. MUST be called with a non-shared list. */
static int ListSortElements(Jim_Interp *interp, Jim_Obj *listObjPtr, struct lsort_info *info)
{
    struct lsort_info *prev_info;

    typedef int (qsort_comparator) (const void *, const void *);
    int (*fn) (Jim_Obj **, Jim_Obj **);
    Jim_Obj **vector;
    int len;
    int rc;

    JimPanic((Jim_IsShared(listObjPtr), "ListSortElements called with shared object"));
    SetListFromAny(interp, listObjPtr);

    /* Allow lsort to be called reentrantly */
    prev_info = sort_info;
    sort_info = info;

    vector = listObjPtr->internalRep.listValue.ele;
    len = listObjPtr->internalRep.listValue.len;
    switch (info->type) {
        case JIM_LSORT_ASCII:
            fn = ListSortString;
            break;
        case JIM_LSORT_NOCASE:
            fn = ListSortStringNoCase;
            break;
        case JIM_LSORT_INTEGER:
            fn = ListSortInteger;
            break;
        case JIM_LSORT_REAL:
            fn = ListSortReal;
            break;
        case JIM_LSORT_COMMAND:
            fn = ListSortCommand;
            break;
        default:
            fn = NULL;          /* avoid warning */
            JimPanic((1, "ListSort called with invalid sort type"));
    }

    if (info->indexed) {
        /* Need to interpose a "list index" function */
        info->subfn = fn;
        fn = ListSortIndexHelper;
    }

    if ((rc = setjmp(info->jmpbuf)) == 0) {
        qsort(vector, len, sizeof(Jim_Obj *), (qsort_comparator *) fn);

        if (info->unique && len > 1) {
            ListRemoveDuplicates(listObjPtr, fn);
        }

        Jim_InvalidateStringRep(listObjPtr);
    }
    sort_info = prev_info;

    return rc;
}

/* This is the low-level function to insert elements into a list.
 * The higher-level Jim_ListInsertElements() performs shared object
 * check and invalidates the string repr. This version is used
 * in the internals of the List Object and is not exported.
 *
 * NOTE: this function can be called only against objects
 * with internal type of List.
 *
 * An insertion point (idx) of -1 means end-of-list.
 */
static void ListInsertElements(Jim_Obj *listPtr, int idx, int elemc, Jim_Obj *const *elemVec)
{
    int currentLen = listPtr->internalRep.listValue.len;
    int requiredLen = currentLen + elemc;
    int i;
    Jim_Obj **point;

    if (requiredLen > listPtr->internalRep.listValue.maxLen) {
        if (requiredLen < 2) {
            /* Don't do allocations of under 4 pointers. */
            requiredLen = 4;
        }
        else {
            requiredLen *= 2;
        }

        listPtr->internalRep.listValue.ele = Jim_Realloc(listPtr->internalRep.listValue.ele,
            sizeof(Jim_Obj *) * requiredLen);

        listPtr->internalRep.listValue.maxLen = requiredLen;
    }
    if (idx < 0) {
        idx = currentLen;
    }
    point = listPtr->internalRep.listValue.ele + idx;
    memmove(point + elemc, point, (currentLen - idx) * sizeof(Jim_Obj *));
    for (i = 0; i < elemc; ++i) {
        point[i] = elemVec[i];
        Jim_IncrRefCount(point[i]);
    }
    listPtr->internalRep.listValue.len += elemc;
}

/* Convenience call to ListInsertElements() to append a single element.
 */
static void ListAppendElement(Jim_Obj *listPtr, Jim_Obj *objPtr)
{
    ListInsertElements(listPtr, -1, 1, &objPtr);
}

/* Appends every element of appendListPtr into listPtr.
 * Both have to be of the list type.
 * Convenience call to ListInsertElements()
 */
static void ListAppendList(Jim_Obj *listPtr, Jim_Obj *appendListPtr)
{
    ListInsertElements(listPtr, -1,
        appendListPtr->internalRep.listValue.len, appendListPtr->internalRep.listValue.ele);
}

void Jim_ListAppendElement(Jim_Interp *interp, Jim_Obj *listPtr, Jim_Obj *objPtr)
{
    JimPanic((Jim_IsShared(listPtr), "Jim_ListAppendElement called with shared object"));
    SetListFromAny(interp, listPtr);
    Jim_InvalidateStringRep(listPtr);
    ListAppendElement(listPtr, objPtr);
}

void Jim_ListAppendList(Jim_Interp *interp, Jim_Obj *listPtr, Jim_Obj *appendListPtr)
{
    JimPanic((Jim_IsShared(listPtr), "Jim_ListAppendList called with shared object"));
    SetListFromAny(interp, listPtr);
    SetListFromAny(interp, appendListPtr);
    Jim_InvalidateStringRep(listPtr);
    ListAppendList(listPtr, appendListPtr);
}

int Jim_ListLength(Jim_Interp *interp, Jim_Obj *objPtr)
{
    SetListFromAny(interp, objPtr);
    return objPtr->internalRep.listValue.len;
}

void Jim_ListInsertElements(Jim_Interp *interp, Jim_Obj *listPtr, int idx,
    int objc, Jim_Obj *const *objVec)
{
    JimPanic((Jim_IsShared(listPtr), "Jim_ListInsertElement called with shared object"));
    SetListFromAny(interp, listPtr);
    if (idx >= 0 && idx > listPtr->internalRep.listValue.len)
        idx = listPtr->internalRep.listValue.len;
    else if (idx < 0)
        idx = 0;
    Jim_InvalidateStringRep(listPtr);
    ListInsertElements(listPtr, idx, objc, objVec);
}

Jim_Obj *Jim_ListGetIndex(Jim_Interp *interp, Jim_Obj *listPtr, int idx)
{
    SetListFromAny(interp, listPtr);
    if ((idx >= 0 && idx >= listPtr->internalRep.listValue.len) ||
        (idx < 0 && (-idx - 1) >= listPtr->internalRep.listValue.len)) {
        return NULL;
    }
    if (idx < 0)
        idx = listPtr->internalRep.listValue.len + idx;
    return listPtr->internalRep.listValue.ele[idx];
}

int Jim_ListIndex(Jim_Interp *interp, Jim_Obj *listPtr, int idx, Jim_Obj **objPtrPtr, int flags)
{
    *objPtrPtr = Jim_ListGetIndex(interp, listPtr, idx);
    if (*objPtrPtr == NULL) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultString(interp, "list index out of range", -1);
        }
        return JIM_ERR;
    }
    return JIM_OK;
}

static int ListSetIndex(Jim_Interp *interp, Jim_Obj *listPtr, int idx,
    Jim_Obj *newObjPtr, int flags)
{
    SetListFromAny(interp, listPtr);
    if ((idx >= 0 && idx >= listPtr->internalRep.listValue.len) ||
        (idx < 0 && (-idx - 1) >= listPtr->internalRep.listValue.len)) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultString(interp, "list index out of range", -1);
        }
        return JIM_ERR;
    }
    if (idx < 0)
        idx = listPtr->internalRep.listValue.len + idx;
    Jim_DecrRefCount(interp, listPtr->internalRep.listValue.ele[idx]);
    listPtr->internalRep.listValue.ele[idx] = newObjPtr;
    Jim_IncrRefCount(newObjPtr);
    return JIM_OK;
}

/* Modify the list stored in the variable named 'varNamePtr'
 * setting the element specified by the 'indexc' indexes objects in 'indexv',
 * with the new element 'newObjptr'. (implements the [lset] command) */
int Jim_ListSetIndex(Jim_Interp *interp, Jim_Obj *varNamePtr,
    Jim_Obj *const *indexv, int indexc, Jim_Obj *newObjPtr)
{
    Jim_Obj *varObjPtr, *objPtr, *listObjPtr;
    int shared, i, idx;

    varObjPtr = objPtr = Jim_GetVariable(interp, varNamePtr, JIM_ERRMSG | JIM_UNSHARED);
    if (objPtr == NULL)
        return JIM_ERR;
    if ((shared = Jim_IsShared(objPtr)))
        varObjPtr = objPtr = Jim_DuplicateObj(interp, objPtr);
    for (i = 0; i < indexc - 1; i++) {
        listObjPtr = objPtr;
        if (Jim_GetIndex(interp, indexv[i], &idx) != JIM_OK)
            goto err;
        if (Jim_ListIndex(interp, listObjPtr, idx, &objPtr, JIM_ERRMSG) != JIM_OK) {
            goto err;
        }
        if (Jim_IsShared(objPtr)) {
            objPtr = Jim_DuplicateObj(interp, objPtr);
            ListSetIndex(interp, listObjPtr, idx, objPtr, JIM_NONE);
        }
        Jim_InvalidateStringRep(listObjPtr);
    }
    if (Jim_GetIndex(interp, indexv[indexc - 1], &idx) != JIM_OK)
        goto err;
    if (ListSetIndex(interp, objPtr, idx, newObjPtr, JIM_ERRMSG) == JIM_ERR)
        goto err;
    Jim_InvalidateStringRep(objPtr);
    Jim_InvalidateStringRep(varObjPtr);
    if (Jim_SetVariable(interp, varNamePtr, varObjPtr) != JIM_OK)
        goto err;
    Jim_SetResult(interp, varObjPtr);
    return JIM_OK;
  err:
    if (shared) {
        Jim_FreeNewObj(interp, varObjPtr);
    }
    return JIM_ERR;
}

Jim_Obj *Jim_ListJoin(Jim_Interp *interp, Jim_Obj *listObjPtr, const char *joinStr, int joinStrLen)
{
    int i;
    int listLen = Jim_ListLength(interp, listObjPtr);
    Jim_Obj *resObjPtr = Jim_NewEmptyStringObj(interp);

    for (i = 0; i < listLen; ) {
        Jim_AppendObj(interp, resObjPtr, Jim_ListGetIndex(interp, listObjPtr, i));
        if (++i != listLen) {
            Jim_AppendString(interp, resObjPtr, joinStr, joinStrLen);
        }
    }
    return resObjPtr;
}

Jim_Obj *Jim_ConcatObj(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    int i;

    /* If all the objects in objv are lists,
     * it's possible to return a list as result, that's the
     * concatenation of all the lists. */
    for (i = 0; i < objc; i++) {
        if (!Jim_IsList(objv[i]))
            break;
    }
    if (i == objc) {
        Jim_Obj *objPtr = Jim_NewListObj(interp, NULL, 0);

        for (i = 0; i < objc; i++)
            ListAppendList(objPtr, objv[i]);
        return objPtr;
    }
    else {
        /* Else... we have to glue strings together */
        int len = 0, objLen;
        char *bytes, *p;

        /* Compute the length */
        for (i = 0; i < objc; i++) {
            len += Jim_Length(objv[i]);
        }
        if (objc)
            len += objc - 1;
        /* Create the string rep, and a string object holding it. */
        p = bytes = Jim_Alloc(len + 1);
        for (i = 0; i < objc; i++) {
            const char *s = Jim_GetString(objv[i], &objLen);

            /* Remove leading space */
            while (objLen && isspace(UCHAR(*s))) {
                s++;
                objLen--;
                len--;
            }
            /* And trailing space */
            while (objLen && isspace(UCHAR(s[objLen - 1]))) {
                /* Handle trailing backslash-space case */
                if (objLen > 1 && s[objLen - 2] == '\\') {
                    break;
                }
                objLen--;
                len--;
            }
            memcpy(p, s, objLen);
            p += objLen;
            if (i + 1 != objc) {
                if (objLen)
                    *p++ = ' ';
                else {
                    /* Drop the space calculated for this
                     * element that is instead null. */
                    len--;
                }
            }
        }
        *p = '\0';
        return Jim_NewStringObjNoAlloc(interp, bytes, len);
    }
}

/* Returns a list composed of the elements in the specified range.
 * first and start are directly accepted as Jim_Objects and
 * processed for the end?-index? case. */
Jim_Obj *Jim_ListRange(Jim_Interp *interp, Jim_Obj *listObjPtr, Jim_Obj *firstObjPtr,
    Jim_Obj *lastObjPtr)
{
    int first, last;
    int len, rangeLen;

    if (Jim_GetIndex(interp, firstObjPtr, &first) != JIM_OK ||
        Jim_GetIndex(interp, lastObjPtr, &last) != JIM_OK)
        return NULL;
    len = Jim_ListLength(interp, listObjPtr);   /* will convert into list */
    first = JimRelToAbsIndex(len, first);
    last = JimRelToAbsIndex(len, last);
    JimRelToAbsRange(len, &first, &last, &rangeLen);
    if (first == 0 && last == len) {
        return listObjPtr;
    }
    return Jim_NewListObj(interp, listObjPtr->internalRep.listValue.ele + first, rangeLen);
}

/* -----------------------------------------------------------------------------
 * Dict object
 * ---------------------------------------------------------------------------*/
static void FreeDictInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupDictInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static void UpdateStringOfDict(struct Jim_Obj *objPtr);
static int SetDictFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

/* Dict HashTable Type.
 *
 * Keys and Values are Jim objects. */

static unsigned int JimObjectHTHashFunction(const void *key)
{
    int len;
    const char *str = Jim_GetString((Jim_Obj *)key, &len);
    return Jim_GenHashFunction((const unsigned char *)str, len);
}

static int JimObjectHTKeyCompare(void *privdata, const void *key1, const void *key2)
{
    return Jim_StringEqObj((Jim_Obj *)key1, (Jim_Obj *)key2);
}

static void *JimObjectHTKeyValDup(void *privdata, const void *val)
{
    Jim_IncrRefCount((Jim_Obj *)val);
    return (void *)val;
}

static void JimObjectHTKeyValDestructor(void *interp, void *val)
{
    Jim_DecrRefCount(interp, (Jim_Obj *)val);
}

static const Jim_HashTableType JimDictHashTableType = {
    JimObjectHTHashFunction,    /* hash function */
    JimObjectHTKeyValDup,       /* key dup */
    JimObjectHTKeyValDup,       /* val dup */
    JimObjectHTKeyCompare,      /* key compare */
    JimObjectHTKeyValDestructor,    /* key destructor */
    JimObjectHTKeyValDestructor /* val destructor */
};

/* Note that while the elements of the dict may contain references,
 * the list object itself can't. This basically means that the
 * dict object string representation as a whole can't contain references
 * that are not presents in the single elements. */
static const Jim_ObjType dictObjType = {
    "dict",
    FreeDictInternalRep,
    DupDictInternalRep,
    UpdateStringOfDict,
    JIM_TYPE_NONE,
};

void FreeDictInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    JIM_NOTUSED(interp);

    Jim_FreeHashTable(objPtr->internalRep.ptr);
    Jim_Free(objPtr->internalRep.ptr);
}

void DupDictInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    Jim_HashTable *ht, *dupHt;
    Jim_HashTableIterator htiter;
    Jim_HashEntry *he;

    /* Create a new hash table */
    ht = srcPtr->internalRep.ptr;
    dupHt = Jim_Alloc(sizeof(*dupHt));
    Jim_InitHashTable(dupHt, &JimDictHashTableType, interp);
    if (ht->size != 0)
        Jim_ExpandHashTable(dupHt, ht->size);
    /* Copy every element from the source to the dup hash table */
    JimInitHashTableIterator(ht, &htiter);
    while ((he = Jim_NextHashEntry(&htiter)) != NULL) {
        Jim_AddHashEntry(dupHt, he->key, he->u.val);
    }

    dupPtr->internalRep.ptr = dupHt;
    dupPtr->typePtr = &dictObjType;
}

static Jim_Obj **JimDictPairs(Jim_Obj *dictPtr, int *len)
{
    Jim_HashTable *ht;
    Jim_HashTableIterator htiter;
    Jim_HashEntry *he;
    Jim_Obj **objv;
    int i;

    ht = dictPtr->internalRep.ptr;

    /* Turn the hash table into a flat vector of Jim_Objects. */
    objv = Jim_Alloc((ht->used * 2) * sizeof(Jim_Obj *));
    JimInitHashTableIterator(ht, &htiter);
    i = 0;
    while ((he = Jim_NextHashEntry(&htiter)) != NULL) {
        objv[i++] = Jim_GetHashEntryKey(he);
        objv[i++] = Jim_GetHashEntryVal(he);
    }
    *len = i;
    return objv;
}

static void UpdateStringOfDict(struct Jim_Obj *objPtr)
{
    /* Turn the hash table into a flat vector of Jim_Objects. */
    int len;
    Jim_Obj **objv = JimDictPairs(objPtr, &len);

    /* And now generate the string rep as a list */
    JimMakeListStringRep(objPtr, objv, len);

    Jim_Free(objv);
}

static int SetDictFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    int listlen;

    if (objPtr->typePtr == &dictObjType) {
        return JIM_OK;
    }

    if (Jim_IsList(objPtr) && Jim_IsShared(objPtr)) {
        /* A shared list, so get the string representation now to avoid
         * changing the order in case of fast conversion to dict.
         */
        Jim_String(objPtr);
    }

    /* For simplicity, convert a non-list object to a list and then to a dict */
    listlen = Jim_ListLength(interp, objPtr);
    if (listlen % 2) {
        Jim_SetResultString(interp, "missing value to go with key", -1);
        return JIM_ERR;
    }
    else {
        /* Converting from a list to a dict can't fail */
        Jim_HashTable *ht;
        int i;

        ht = Jim_Alloc(sizeof(*ht));
        Jim_InitHashTable(ht, &JimDictHashTableType, interp);

        for (i = 0; i < listlen; i += 2) {
            Jim_Obj *keyObjPtr = Jim_ListGetIndex(interp, objPtr, i);
            Jim_Obj *valObjPtr = Jim_ListGetIndex(interp, objPtr, i + 1);

            Jim_ReplaceHashEntry(ht, keyObjPtr, valObjPtr);
        }

        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &dictObjType;
        objPtr->internalRep.ptr = ht;

        return JIM_OK;
    }
}

/* Dict object API */

/* Add an element to a dict. objPtr must be of the "dict" type.
 * The higher-level exported function is Jim_DictAddElement().
 * If an element with the specified key already exists, the value
 * associated is replaced with the new one.
 *
 * if valueObjPtr == NULL, the key is instead removed if it exists. */
static int DictAddElement(Jim_Interp *interp, Jim_Obj *objPtr,
    Jim_Obj *keyObjPtr, Jim_Obj *valueObjPtr)
{
    Jim_HashTable *ht = objPtr->internalRep.ptr;

    if (valueObjPtr == NULL) {  /* unset */
        return Jim_DeleteHashEntry(ht, keyObjPtr);
    }
    Jim_ReplaceHashEntry(ht, keyObjPtr, valueObjPtr);
    return JIM_OK;
}

/* Add an element, higher-level interface for DictAddElement().
 * If valueObjPtr == NULL, the key is removed if it exists. */
int Jim_DictAddElement(Jim_Interp *interp, Jim_Obj *objPtr,
    Jim_Obj *keyObjPtr, Jim_Obj *valueObjPtr)
{
    JimPanic((Jim_IsShared(objPtr), "Jim_DictAddElement called with shared object"));
    if (SetDictFromAny(interp, objPtr) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_InvalidateStringRep(objPtr);
    return DictAddElement(interp, objPtr, keyObjPtr, valueObjPtr);
}

Jim_Obj *Jim_NewDictObj(Jim_Interp *interp, Jim_Obj *const *elements, int len)
{
    Jim_Obj *objPtr;
    int i;

    JimPanic((len % 2, "Jim_NewDictObj() 'len' argument must be even"));

    objPtr = Jim_NewObj(interp);
    objPtr->typePtr = &dictObjType;
    objPtr->bytes = NULL;
    objPtr->internalRep.ptr = Jim_Alloc(sizeof(Jim_HashTable));
    Jim_InitHashTable(objPtr->internalRep.ptr, &JimDictHashTableType, interp);
    for (i = 0; i < len; i += 2)
        DictAddElement(interp, objPtr, elements[i], elements[i + 1]);
    return objPtr;
}

/* Return the value associated to the specified dict key
 * Returns JIM_OK if OK, JIM_ERR if entry not found or -1 if can't create dict value
 *
 * Sets *objPtrPtr to non-NULL only upon success.
 */
int Jim_DictKey(Jim_Interp *interp, Jim_Obj *dictPtr, Jim_Obj *keyPtr,
    Jim_Obj **objPtrPtr, int flags)
{
    Jim_HashEntry *he;
    Jim_HashTable *ht;

    if (SetDictFromAny(interp, dictPtr) != JIM_OK) {
        return -1;
    }
    ht = dictPtr->internalRep.ptr;
    if ((he = Jim_FindHashEntry(ht, keyPtr)) == NULL) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp, "key \"%#s\" not known in dictionary", keyPtr);
        }
        return JIM_ERR;
    }
    *objPtrPtr = he->u.val;
    return JIM_OK;
}

/* Return an allocated array of key/value pairs for the dictionary. Stores the length in *len */
int Jim_DictPairs(Jim_Interp *interp, Jim_Obj *dictPtr, Jim_Obj ***objPtrPtr, int *len)
{
    if (SetDictFromAny(interp, dictPtr) != JIM_OK) {
        return JIM_ERR;
    }
    *objPtrPtr = JimDictPairs(dictPtr, len);

    return JIM_OK;
}


/* Return the value associated to the specified dict keys */
int Jim_DictKeysVector(Jim_Interp *interp, Jim_Obj *dictPtr,
    Jim_Obj *const *keyv, int keyc, Jim_Obj **objPtrPtr, int flags)
{
    int i;

    if (keyc == 0) {
        *objPtrPtr = dictPtr;
        return JIM_OK;
    }

    for (i = 0; i < keyc; i++) {
        Jim_Obj *objPtr;

        int rc = Jim_DictKey(interp, dictPtr, keyv[i], &objPtr, flags);
        if (rc != JIM_OK) {
            return rc;
        }
        dictPtr = objPtr;
    }
    *objPtrPtr = dictPtr;
    return JIM_OK;
}

/* Modify the dict stored into the variable named 'varNamePtr'
 * setting the element specified by the 'keyc' keys objects in 'keyv',
 * with the new value of the element 'newObjPtr'.
 *
 * If newObjPtr == NULL the operation is to remove the given key
 * from the dictionary.
 *
 * If flags & JIM_ERRMSG, then failure to remove the key is considered an error
 * and JIM_ERR is returned. Otherwise it is ignored and JIM_OK is returned.
 */
int Jim_SetDictKeysVector(Jim_Interp *interp, Jim_Obj *varNamePtr,
    Jim_Obj *const *keyv, int keyc, Jim_Obj *newObjPtr, int flags)
{
    Jim_Obj *varObjPtr, *objPtr, *dictObjPtr;
    int shared, i;

    varObjPtr = objPtr = Jim_GetVariable(interp, varNamePtr, flags);
    if (objPtr == NULL) {
        if (newObjPtr == NULL && (flags & JIM_MUSTEXIST)) {
            /* Cannot remove a key from non existing var */
            return JIM_ERR;
        }
        varObjPtr = objPtr = Jim_NewDictObj(interp, NULL, 0);
        if (Jim_SetVariable(interp, varNamePtr, objPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, varObjPtr);
            return JIM_ERR;
        }
    }
    if ((shared = Jim_IsShared(objPtr)))
        varObjPtr = objPtr = Jim_DuplicateObj(interp, objPtr);
    for (i = 0; i < keyc; i++) {
        dictObjPtr = objPtr;

        /* Check if it's a valid dictionary */
        if (SetDictFromAny(interp, dictObjPtr) != JIM_OK) {
            goto err;
        }

        if (i == keyc - 1) {
            /* Last key: Note that error on unset with missing last key is OK */
            if (Jim_DictAddElement(interp, objPtr, keyv[keyc - 1], newObjPtr) != JIM_OK) {
                if (newObjPtr || (flags & JIM_MUSTEXIST)) {
                    goto err;
                }
            }
            break;
        }

        /* Check if the given key exists. */
        Jim_InvalidateStringRep(dictObjPtr);
        if (Jim_DictKey(interp, dictObjPtr, keyv[i], &objPtr,
                newObjPtr ? JIM_NONE : JIM_ERRMSG) == JIM_OK) {
            /* This key exists at the current level.
             * Make sure it's not shared!. */
            if (Jim_IsShared(objPtr)) {
                objPtr = Jim_DuplicateObj(interp, objPtr);
                DictAddElement(interp, dictObjPtr, keyv[i], objPtr);
            }
        }
        else {
            /* Key not found. If it's an [unset] operation
             * this is an error. Only the last key may not
             * exist. */
            if (newObjPtr == NULL) {
                goto err;
            }
            /* Otherwise set an empty dictionary
             * as key's value. */
            objPtr = Jim_NewDictObj(interp, NULL, 0);
            DictAddElement(interp, dictObjPtr, keyv[i], objPtr);
        }
    }
    /* XXX: Is this necessary? */
    Jim_InvalidateStringRep(objPtr);
    Jim_InvalidateStringRep(varObjPtr);
    if (Jim_SetVariable(interp, varNamePtr, varObjPtr) != JIM_OK) {
        goto err;
    }
    Jim_SetResult(interp, varObjPtr);
    return JIM_OK;
  err:
    if (shared) {
        Jim_FreeNewObj(interp, varObjPtr);
    }
    return JIM_ERR;
}

/* -----------------------------------------------------------------------------
 * Index object
 * ---------------------------------------------------------------------------*/
static void UpdateStringOfIndex(struct Jim_Obj *objPtr);
static int SetIndexFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

static const Jim_ObjType indexObjType = {
    "index",
    NULL,
    NULL,
    UpdateStringOfIndex,
    JIM_TYPE_NONE,
};

static void UpdateStringOfIndex(struct Jim_Obj *objPtr)
{
    if (objPtr->internalRep.intValue == -1) {
        JimSetStringBytes(objPtr, "end");
    }
    else {
        char buf[JIM_INTEGER_SPACE + 1];
        if (objPtr->internalRep.intValue >= 0) {
            sprintf(buf, "%d", objPtr->internalRep.intValue);
        }
        else {
            /* Must be <= -2 */
            sprintf(buf, "end%d", objPtr->internalRep.intValue + 1);
        }
        JimSetStringBytes(objPtr, buf);
    }
}

static int SetIndexFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int idx, end = 0;
    const char *str;
    char *endptr;

    /* Get the string representation */
    str = Jim_String(objPtr);

    /* Try to convert into an index */
    if (strncmp(str, "end", 3) == 0) {
        end = 1;
        str += 3;
        idx = 0;
    }
    else {
        idx = jim_strtol(str, &endptr);

        if (endptr == str) {
            goto badindex;
        }
        str = endptr;
    }

    /* Now str may include or +<num> or -<num> */
    if (*str == '+' || *str == '-') {
        int sign = (*str == '+' ? 1 : -1);

        idx += sign * jim_strtol(++str, &endptr);
        if (str == endptr || *endptr) {
            goto badindex;
        }
        str = endptr;
    }
    /* The only thing left should be spaces */
    while (isspace(UCHAR(*str))) {
        str++;
    }
    if (*str) {
        goto badindex;
    }
    if (end) {
        if (idx > 0) {
            idx = INT_MAX;
        }
        else {
            /* end-1 is repesented as -2 */
            idx--;
        }
    }
    else if (idx < 0) {
        idx = -INT_MAX;
    }

    /* Free the old internal repr and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &indexObjType;
    objPtr->internalRep.intValue = idx;
    return JIM_OK;

  badindex:
    Jim_SetResultFormatted(interp,
        "bad index \"%#s\": must be integer?[+-]integer? or end?[+-]integer?", objPtr);
    return JIM_ERR;
}

int Jim_GetIndex(Jim_Interp *interp, Jim_Obj *objPtr, int *indexPtr)
{
    /* Avoid shimmering if the object is an integer. */
    if (objPtr->typePtr == &intObjType) {
        jim_wide val = JimWideValue(objPtr);

        if (val < 0)
            *indexPtr = -INT_MAX;
        else if (val > INT_MAX)
            *indexPtr = INT_MAX;
        else
            *indexPtr = (int)val;
        return JIM_OK;
    }
    if (objPtr->typePtr != &indexObjType && SetIndexFromAny(interp, objPtr) == JIM_ERR)
        return JIM_ERR;
    *indexPtr = objPtr->internalRep.intValue;
    return JIM_OK;
}

/* -----------------------------------------------------------------------------
 * Return Code Object.
 * ---------------------------------------------------------------------------*/

/* NOTE: These must be kept in the same order as JIM_OK, JIM_ERR, ... */
static const char * const jimReturnCodes[] = {
    "ok",
    "error",
    "return",
    "break",
    "continue",
    "signal",
    "exit",
    "eval",
    NULL
};

#define jimReturnCodesSize (sizeof(jimReturnCodes)/sizeof(*jimReturnCodes))

static const Jim_ObjType returnCodeObjType = {
    "return-code",
    NULL,
    NULL,
    NULL,
    JIM_TYPE_NONE,
};

/* Converts a (standard) return code to a string. Returns "?" for
 * non-standard return codes.
 */
const char *Jim_ReturnCode(int code)
{
    if (code < 0 || code >= (int)jimReturnCodesSize) {
        return "?";
    }
    else {
        return jimReturnCodes[code];
    }
}

static int SetReturnCodeFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int returnCode;
    jim_wide wideValue;

    /* Try to convert into an integer */
    if (JimGetWideNoErr(interp, objPtr, &wideValue) != JIM_ERR)
        returnCode = (int)wideValue;
    else if (Jim_GetEnum(interp, objPtr, jimReturnCodes, &returnCode, NULL, JIM_NONE) != JIM_OK) {
        Jim_SetResultFormatted(interp, "expected return code but got \"%#s\"", objPtr);
        return JIM_ERR;
    }
    /* Free the old internal repr and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &returnCodeObjType;
    objPtr->internalRep.intValue = returnCode;
    return JIM_OK;
}

int Jim_GetReturnCode(Jim_Interp *interp, Jim_Obj *objPtr, int *intPtr)
{
    if (objPtr->typePtr != &returnCodeObjType && SetReturnCodeFromAny(interp, objPtr) == JIM_ERR)
        return JIM_ERR;
    *intPtr = objPtr->internalRep.intValue;
    return JIM_OK;
}

/* -----------------------------------------------------------------------------
 * Expression Parsing
 * ---------------------------------------------------------------------------*/
static int JimParseExprOperator(struct JimParserCtx *pc);
static int JimParseExprNumber(struct JimParserCtx *pc);
static int JimParseExprIrrational(struct JimParserCtx *pc);

/* Exrp's Stack machine operators opcodes. */

/* Binary operators (numbers) */
enum
{
    /* Continues on from the JIM_TT_ space */
    /* Operations */
    JIM_EXPROP_MUL = JIM_TT_EXPR_OP,             /* 20 */
    JIM_EXPROP_DIV,
    JIM_EXPROP_MOD,
    JIM_EXPROP_SUB,
    JIM_EXPROP_ADD,
    JIM_EXPROP_LSHIFT,
    JIM_EXPROP_RSHIFT,
    JIM_EXPROP_ROTL,
    JIM_EXPROP_ROTR,
    JIM_EXPROP_LT,
    JIM_EXPROP_GT,
    JIM_EXPROP_LTE,
    JIM_EXPROP_GTE,
    JIM_EXPROP_NUMEQ,
    JIM_EXPROP_NUMNE,
    JIM_EXPROP_BITAND,          /* 35 */
    JIM_EXPROP_BITXOR,
    JIM_EXPROP_BITOR,

    /* Note must keep these together */
    JIM_EXPROP_LOGICAND,        /* 38 */
    JIM_EXPROP_LOGICAND_LEFT,
    JIM_EXPROP_LOGICAND_RIGHT,

    /* and these */
    JIM_EXPROP_LOGICOR,         /* 41 */
    JIM_EXPROP_LOGICOR_LEFT,
    JIM_EXPROP_LOGICOR_RIGHT,

    /* and these */
    /* Ternary operators */
    JIM_EXPROP_TERNARY,         /* 44 */
    JIM_EXPROP_TERNARY_LEFT,
    JIM_EXPROP_TERNARY_RIGHT,

    /* and these */
    JIM_EXPROP_COLON,           /* 47 */
    JIM_EXPROP_COLON_LEFT,
    JIM_EXPROP_COLON_RIGHT,

    JIM_EXPROP_POW,             /* 50 */

/* Binary operators (strings) */
    JIM_EXPROP_STREQ,           /* 51 */
    JIM_EXPROP_STRNE,
    JIM_EXPROP_STRIN,
    JIM_EXPROP_STRNI,

/* Unary operators (numbers) */
    JIM_EXPROP_NOT,             /* 55 */
    JIM_EXPROP_BITNOT,
    JIM_EXPROP_UNARYMINUS,
    JIM_EXPROP_UNARYPLUS,

    /* Functions */
    JIM_EXPROP_FUNC_FIRST,      /* 59 */
    JIM_EXPROP_FUNC_INT = JIM_EXPROP_FUNC_FIRST,
    JIM_EXPROP_FUNC_WIDE,
    JIM_EXPROP_FUNC_ABS,
    JIM_EXPROP_FUNC_DOUBLE,
    JIM_EXPROP_FUNC_ROUND,
    JIM_EXPROP_FUNC_RAND,
    JIM_EXPROP_FUNC_SRAND,

    /* math functions from libm */
    JIM_EXPROP_FUNC_SIN,        /* 65 */
    JIM_EXPROP_FUNC_COS,
    JIM_EXPROP_FUNC_TAN,
    JIM_EXPROP_FUNC_ASIN,
    JIM_EXPROP_FUNC_ACOS,
    JIM_EXPROP_FUNC_ATAN,
    JIM_EXPROP_FUNC_SINH,
    JIM_EXPROP_FUNC_COSH,
    JIM_EXPROP_FUNC_TANH,
    JIM_EXPROP_FUNC_CEIL,
    JIM_EXPROP_FUNC_FLOOR,
    JIM_EXPROP_FUNC_EXP,
    JIM_EXPROP_FUNC_LOG,
    JIM_EXPROP_FUNC_LOG10,
    JIM_EXPROP_FUNC_SQRT,
    JIM_EXPROP_FUNC_POW,
};

struct JimExprState
{
    Jim_Obj **stack;
    int stacklen;
    int opcode;
    int skip;
};

/* Operators table */
typedef struct Jim_ExprOperator
{
    const char *name;
    int (*funcop) (Jim_Interp *interp, struct JimExprState * e);
    unsigned char precedence;
    unsigned char arity;
    unsigned char lazy;
    unsigned char namelen;
} Jim_ExprOperator;

static void ExprPush(struct JimExprState *e, Jim_Obj *obj)
{
    Jim_IncrRefCount(obj);
    e->stack[e->stacklen++] = obj;
}

static Jim_Obj *ExprPop(struct JimExprState *e)
{
    return e->stack[--e->stacklen];
}

static int JimExprOpNumUnary(Jim_Interp *interp, struct JimExprState *e)
{
    int intresult = 1;
    int rc = JIM_OK;
    Jim_Obj *A = ExprPop(e);
    double dA, dC = 0;
    jim_wide wA, wC = 0;

    if ((A->typePtr != &doubleObjType || A->bytes) && JimGetWideNoErr(interp, A, &wA) == JIM_OK) {
        switch (e->opcode) {
            case JIM_EXPROP_FUNC_INT:
            case JIM_EXPROP_FUNC_WIDE:
            case JIM_EXPROP_FUNC_ROUND:
            case JIM_EXPROP_UNARYPLUS:
                wC = wA;
                break;
            case JIM_EXPROP_FUNC_DOUBLE:
                dC = wA;
                intresult = 0;
                break;
            case JIM_EXPROP_FUNC_ABS:
                wC = wA >= 0 ? wA : -wA;
                break;
            case JIM_EXPROP_UNARYMINUS:
                wC = -wA;
                break;
            case JIM_EXPROP_NOT:
                wC = !wA;
                break;
            default:
                abort();
        }
    }
    else if ((rc = Jim_GetDouble(interp, A, &dA)) == JIM_OK) {
        switch (e->opcode) {
            case JIM_EXPROP_FUNC_INT:
            case JIM_EXPROP_FUNC_WIDE:
                wC = dA;
                break;
            case JIM_EXPROP_FUNC_ROUND:
                wC = dA < 0 ? (dA - 0.5) : (dA + 0.5);
                break;
            case JIM_EXPROP_FUNC_DOUBLE:
            case JIM_EXPROP_UNARYPLUS:
                dC = dA;
                intresult = 0;
                break;
            case JIM_EXPROP_FUNC_ABS:
                dC = dA >= 0 ? dA : -dA;
                intresult = 0;
                break;
            case JIM_EXPROP_UNARYMINUS:
                dC = -dA;
                intresult = 0;
                break;
            case JIM_EXPROP_NOT:
                wC = !dA;
                break;
            default:
                abort();
        }
    }

    if (rc == JIM_OK) {
        if (intresult) {
            ExprPush(e, Jim_NewIntObj(interp, wC));
        }
        else {
            ExprPush(e, Jim_NewDoubleObj(interp, dC));
        }
    }

    Jim_DecrRefCount(interp, A);

    return rc;
}

static double JimRandDouble(Jim_Interp *interp)
{
    unsigned long x;
    JimRandomBytes(interp, &x, sizeof(x));

    return (double)x / (unsigned long)~0;
}

static int JimExprOpIntUnary(Jim_Interp *interp, struct JimExprState *e)
{
    Jim_Obj *A = ExprPop(e);
    jim_wide wA;

    int rc = Jim_GetWide(interp, A, &wA);
    if (rc == JIM_OK) {
        switch (e->opcode) {
            case JIM_EXPROP_BITNOT:
                ExprPush(e, Jim_NewIntObj(interp, ~wA));
                break;
            case JIM_EXPROP_FUNC_SRAND:
                JimPrngSeed(interp, (unsigned char *)&wA, sizeof(wA));
                ExprPush(e, Jim_NewDoubleObj(interp, JimRandDouble(interp)));
                break;
            default:
                abort();
        }
    }

    Jim_DecrRefCount(interp, A);

    return rc;
}

static int JimExprOpNone(Jim_Interp *interp, struct JimExprState *e)
{
    JimPanic((e->opcode != JIM_EXPROP_FUNC_RAND, "JimExprOpNone only support rand()"));

    ExprPush(e, Jim_NewDoubleObj(interp, JimRandDouble(interp)));

    return JIM_OK;
}

#ifdef JIM_MATH_FUNCTIONS
static int JimExprOpDoubleUnary(Jim_Interp *interp, struct JimExprState *e)
{
    int rc;
    Jim_Obj *A = ExprPop(e);
    double dA, dC;

    rc = Jim_GetDouble(interp, A, &dA);
    if (rc == JIM_OK) {
        switch (e->opcode) {
            case JIM_EXPROP_FUNC_SIN:
                dC = sin(dA);
                break;
            case JIM_EXPROP_FUNC_COS:
                dC = cos(dA);
                break;
            case JIM_EXPROP_FUNC_TAN:
                dC = tan(dA);
                break;
            case JIM_EXPROP_FUNC_ASIN:
                dC = asin(dA);
                break;
            case JIM_EXPROP_FUNC_ACOS:
                dC = acos(dA);
                break;
            case JIM_EXPROP_FUNC_ATAN:
                dC = atan(dA);
                break;
            case JIM_EXPROP_FUNC_SINH:
                dC = sinh(dA);
                break;
            case JIM_EXPROP_FUNC_COSH:
                dC = cosh(dA);
                break;
            case JIM_EXPROP_FUNC_TANH:
                dC = tanh(dA);
                break;
            case JIM_EXPROP_FUNC_CEIL:
                dC = ceil(dA);
                break;
            case JIM_EXPROP_FUNC_FLOOR:
                dC = floor(dA);
                break;
            case JIM_EXPROP_FUNC_EXP:
                dC = exp(dA);
                break;
            case JIM_EXPROP_FUNC_LOG:
                dC = log(dA);
                break;
            case JIM_EXPROP_FUNC_LOG10:
                dC = log10(dA);
                break;
            case JIM_EXPROP_FUNC_SQRT:
                dC = sqrt(dA);
                break;
            default:
                abort();
        }
        ExprPush(e, Jim_NewDoubleObj(interp, dC));
    }

    Jim_DecrRefCount(interp, A);

    return rc;
}
#endif

/* A binary operation on two ints */
static int JimExprOpIntBin(Jim_Interp *interp, struct JimExprState *e)
{
    Jim_Obj *B = ExprPop(e);
    Jim_Obj *A = ExprPop(e);
    jim_wide wA, wB;
    int rc = JIM_ERR;

    if (Jim_GetWide(interp, A, &wA) == JIM_OK && Jim_GetWide(interp, B, &wB) == JIM_OK) {
        jim_wide wC;

        rc = JIM_OK;

        switch (e->opcode) {
            case JIM_EXPROP_LSHIFT:
                wC = wA << wB;
                break;
            case JIM_EXPROP_RSHIFT:
                wC = wA >> wB;
                break;
            case JIM_EXPROP_BITAND:
                wC = wA & wB;
                break;
            case JIM_EXPROP_BITXOR:
                wC = wA ^ wB;
                break;
            case JIM_EXPROP_BITOR:
                wC = wA | wB;
                break;
            case JIM_EXPROP_MOD:
                if (wB == 0) {
                    wC = 0;
                    Jim_SetResultString(interp, "Division by zero", -1);
                    rc = JIM_ERR;
                }
                else {
                    /*
                     * From Tcl 8.x
                     *
                     * This code is tricky: C doesn't guarantee much
                     * about the quotient or remainder, but Tcl does.
                     * The remainder always has the same sign as the
                     * divisor and a smaller absolute value.
                     */
                    int negative = 0;

                    if (wB < 0) {
                        wB = -wB;
                        wA = -wA;
                        negative = 1;
                    }
                    wC = wA % wB;
                    if (wC < 0) {
                        wC += wB;
                    }
                    if (negative) {
                        wC = -wC;
                    }
                }
                break;
            case JIM_EXPROP_ROTL:
            case JIM_EXPROP_ROTR:{
                    /* uint32_t would be better. But not everyone has inttypes.h? */
                    unsigned long uA = (unsigned long)wA;
                    unsigned long uB = (unsigned long)wB;
                    const unsigned int S = sizeof(unsigned long) * 8;

                    /* Shift left by the word size or more is undefined. */
                    uB %= S;

                    if (e->opcode == JIM_EXPROP_ROTR) {
                        uB = S - uB;
                    }
                    wC = (unsigned long)(uA << uB) | (uA >> (S - uB));
                    break;
                }
            default:
                abort();
        }
        ExprPush(e, Jim_NewIntObj(interp, wC));

    }

    Jim_DecrRefCount(interp, A);
    Jim_DecrRefCount(interp, B);

    return rc;
}


/* A binary operation on two ints or two doubles (or two strings for some ops) */
static int JimExprOpBin(Jim_Interp *interp, struct JimExprState *e)
{
    int intresult = 1;
    int rc = JIM_OK;
    double dA, dB, dC = 0;
    jim_wide wA, wB, wC = 0;

    Jim_Obj *B = ExprPop(e);
    Jim_Obj *A = ExprPop(e);

    if ((A->typePtr != &doubleObjType || A->bytes) &&
        (B->typePtr != &doubleObjType || B->bytes) &&
        JimGetWideNoErr(interp, A, &wA) == JIM_OK && JimGetWideNoErr(interp, B, &wB) == JIM_OK) {

        /* Both are ints */

        switch (e->opcode) {
            case JIM_EXPROP_POW:
            case JIM_EXPROP_FUNC_POW:
                wC = JimPowWide(wA, wB);
                break;
            case JIM_EXPROP_ADD:
                wC = wA + wB;
                break;
            case JIM_EXPROP_SUB:
                wC = wA - wB;
                break;
            case JIM_EXPROP_MUL:
                wC = wA * wB;
                break;
            case JIM_EXPROP_DIV:
                if (wB == 0) {
                    Jim_SetResultString(interp, "Division by zero", -1);
                    rc = JIM_ERR;
                }
                else {
                    /*
                     * From Tcl 8.x
                     *
                     * This code is tricky: C doesn't guarantee much
                     * about the quotient or remainder, but Tcl does.
                     * The remainder always has the same sign as the
                     * divisor and a smaller absolute value.
                     */
                    if (wB < 0) {
                        wB = -wB;
                        wA = -wA;
                    }
                    wC = wA / wB;
                    if (wA % wB < 0) {
                        wC--;
                    }
                }
                break;
            case JIM_EXPROP_LT:
                wC = wA < wB;
                break;
            case JIM_EXPROP_GT:
                wC = wA > wB;
                break;
            case JIM_EXPROP_LTE:
                wC = wA <= wB;
                break;
            case JIM_EXPROP_GTE:
                wC = wA >= wB;
                break;
            case JIM_EXPROP_NUMEQ:
                wC = wA == wB;
                break;
            case JIM_EXPROP_NUMNE:
                wC = wA != wB;
                break;
            default:
                abort();
        }
    }
    else if (Jim_GetDouble(interp, A, &dA) == JIM_OK && Jim_GetDouble(interp, B, &dB) == JIM_OK) {
        intresult = 0;
        switch (e->opcode) {
            case JIM_EXPROP_POW:
            case JIM_EXPROP_FUNC_POW:
#ifdef JIM_MATH_FUNCTIONS
                dC = pow(dA, dB);
#else
                Jim_SetResultString(interp, "unsupported", -1);
                rc = JIM_ERR;
#endif
                break;
            case JIM_EXPROP_ADD:
                dC = dA + dB;
                break;
            case JIM_EXPROP_SUB:
                dC = dA - dB;
                break;
            case JIM_EXPROP_MUL:
                dC = dA * dB;
                break;
            case JIM_EXPROP_DIV:
                if (dB == 0) {
#ifdef INFINITY
                    dC = dA < 0 ? -INFINITY : INFINITY;
#else
                    dC = (dA < 0 ? -1.0 : 1.0) * strtod("Inf", NULL);
#endif
                }
                else {
                    dC = dA / dB;
                }
                break;
            case JIM_EXPROP_LT:
                wC = dA < dB;
                intresult = 1;
                break;
            case JIM_EXPROP_GT:
                wC = dA > dB;
                intresult = 1;
                break;
            case JIM_EXPROP_LTE:
                wC = dA <= dB;
                intresult = 1;
                break;
            case JIM_EXPROP_GTE:
                wC = dA >= dB;
                intresult = 1;
                break;
            case JIM_EXPROP_NUMEQ:
                wC = dA == dB;
                intresult = 1;
                break;
            case JIM_EXPROP_NUMNE:
                wC = dA != dB;
                intresult = 1;
                break;
            default:
                abort();
        }
    }
    else {
        /* Handle the string case */

        /* XXX: Could optimise the eq/ne case by checking lengths */
        int i = Jim_StringCompareObj(interp, A, B, 0);

        switch (e->opcode) {
            case JIM_EXPROP_LT:
                wC = i < 0;
                break;
            case JIM_EXPROP_GT:
                wC = i > 0;
                break;
            case JIM_EXPROP_LTE:
                wC = i <= 0;
                break;
            case JIM_EXPROP_GTE:
                wC = i >= 0;
                break;
            case JIM_EXPROP_NUMEQ:
                wC = i == 0;
                break;
            case JIM_EXPROP_NUMNE:
                wC = i != 0;
                break;
            default:
                rc = JIM_ERR;
                break;
        }
    }

    if (rc == JIM_OK) {
        if (intresult) {
            ExprPush(e, Jim_NewIntObj(interp, wC));
        }
        else {
            ExprPush(e, Jim_NewDoubleObj(interp, dC));
        }
    }

    Jim_DecrRefCount(interp, A);
    Jim_DecrRefCount(interp, B);

    return rc;
}

static int JimSearchList(Jim_Interp *interp, Jim_Obj *listObjPtr, Jim_Obj *valObj)
{
    int listlen;
    int i;

    listlen = Jim_ListLength(interp, listObjPtr);
    for (i = 0; i < listlen; i++) {
        if (Jim_StringEqObj(Jim_ListGetIndex(interp, listObjPtr, i), valObj)) {
            return 1;
        }
    }
    return 0;
}

static int JimExprOpStrBin(Jim_Interp *interp, struct JimExprState *e)
{
    Jim_Obj *B = ExprPop(e);
    Jim_Obj *A = ExprPop(e);

    jim_wide wC;

    switch (e->opcode) {
        case JIM_EXPROP_STREQ:
        case JIM_EXPROP_STRNE:
            wC = Jim_StringEqObj(A, B);
            if (e->opcode == JIM_EXPROP_STRNE) {
                wC = !wC;
            }
            break;
        case JIM_EXPROP_STRIN:
            wC = JimSearchList(interp, B, A);
            break;
        case JIM_EXPROP_STRNI:
            wC = !JimSearchList(interp, B, A);
            break;
        default:
            abort();
    }
    ExprPush(e, Jim_NewIntObj(interp, wC));

    Jim_DecrRefCount(interp, A);
    Jim_DecrRefCount(interp, B);

    return JIM_OK;
}

static int ExprBool(Jim_Interp *interp, Jim_Obj *obj)
{
    long l;
    double d;

    if (Jim_GetLong(interp, obj, &l) == JIM_OK) {
        return l != 0;
    }
    if (Jim_GetDouble(interp, obj, &d) == JIM_OK) {
        return d != 0;
    }
    return -1;
}

static int JimExprOpAndLeft(Jim_Interp *interp, struct JimExprState *e)
{
    Jim_Obj *skip = ExprPop(e);
    Jim_Obj *A = ExprPop(e);
    int rc = JIM_OK;

    switch (ExprBool(interp, A)) {
        case 0:
            /* false, so skip RHS opcodes with a 0 result */
            e->skip = JimWideValue(skip);
            ExprPush(e, Jim_NewIntObj(interp, 0));
            break;

        case 1:
            /* true so continue */
            break;

        case -1:
            /* Invalid */
            rc = JIM_ERR;
    }
    Jim_DecrRefCount(interp, A);
    Jim_DecrRefCount(interp, skip);

    return rc;
}

static int JimExprOpOrLeft(Jim_Interp *interp, struct JimExprState *e)
{
    Jim_Obj *skip = ExprPop(e);
    Jim_Obj *A = ExprPop(e);
    int rc = JIM_OK;

    switch (ExprBool(interp, A)) {
        case 0:
            /* false, so do nothing */
            break;

        case 1:
            /* true so skip RHS opcodes with a 1 result */
            e->skip = JimWideValue(skip);
            ExprPush(e, Jim_NewIntObj(interp, 1));
            break;

        case -1:
            /* Invalid */
            rc = JIM_ERR;
            break;
    }
    Jim_DecrRefCount(interp, A);
    Jim_DecrRefCount(interp, skip);

    return rc;
}

static int JimExprOpAndOrRight(Jim_Interp *interp, struct JimExprState *e)
{
    Jim_Obj *A = ExprPop(e);
    int rc = JIM_OK;

    switch (ExprBool(interp, A)) {
        case 0:
            ExprPush(e, Jim_NewIntObj(interp, 0));
            break;

        case 1:
            ExprPush(e, Jim_NewIntObj(interp, 1));
            break;

        case -1:
            /* Invalid */
            rc = JIM_ERR;
            break;
    }
    Jim_DecrRefCount(interp, A);

    return rc;
}

static int JimExprOpTernaryLeft(Jim_Interp *interp, struct JimExprState *e)
{
    Jim_Obj *skip = ExprPop(e);
    Jim_Obj *A = ExprPop(e);
    int rc = JIM_OK;

    /* Repush A */
    ExprPush(e, A);

    switch (ExprBool(interp, A)) {
        case 0:
            /* false, skip RHS opcodes */
            e->skip = JimWideValue(skip);
            /* Push a dummy value */
            ExprPush(e, Jim_NewIntObj(interp, 0));
            break;

        case 1:
            /* true so do nothing */
            break;

        case -1:
            /* Invalid */
            rc = JIM_ERR;
            break;
    }
    Jim_DecrRefCount(interp, A);
    Jim_DecrRefCount(interp, skip);

    return rc;
}

static int JimExprOpColonLeft(Jim_Interp *interp, struct JimExprState *e)
{
    Jim_Obj *skip = ExprPop(e);
    Jim_Obj *B = ExprPop(e);
    Jim_Obj *A = ExprPop(e);

    /* No need to check for A as non-boolean */
    if (ExprBool(interp, A)) {
        /* true, so skip RHS opcodes */
        e->skip = JimWideValue(skip);
        /* Repush B as the answer */
        ExprPush(e, B);
    }

    Jim_DecrRefCount(interp, skip);
    Jim_DecrRefCount(interp, A);
    Jim_DecrRefCount(interp, B);
    return JIM_OK;
}

static int JimExprOpNull(Jim_Interp *interp, struct JimExprState *e)
{
    return JIM_OK;
}

enum
{
    LAZY_NONE,
    LAZY_OP,
    LAZY_LEFT,
    LAZY_RIGHT
};

/* name - precedence - arity - opcode
 *
 * This array *must* be kept in sync with the JIM_EXPROP enum.
 *
 * The following macros pre-compute the string length at compile time.
 */
#define OPRINIT(N, P, A, F) {N, F, P, A, LAZY_NONE, sizeof(N) - 1}
#define OPRINIT_LAZY(N, P, A, F, L) {N, F, P, A, L, sizeof(N) - 1}

static const struct Jim_ExprOperator Jim_ExprOperators[] = {
    OPRINIT("*", 110, 2, JimExprOpBin),
    OPRINIT("/", 110, 2, JimExprOpBin),
    OPRINIT("%", 110, 2, JimExprOpIntBin),

    OPRINIT("-", 100, 2, JimExprOpBin),
    OPRINIT("+", 100, 2, JimExprOpBin),

    OPRINIT("<<", 90, 2, JimExprOpIntBin),
    OPRINIT(">>", 90, 2, JimExprOpIntBin),

    OPRINIT("<<<", 90, 2, JimExprOpIntBin),
    OPRINIT(">>>", 90, 2, JimExprOpIntBin),

    OPRINIT("<", 80, 2, JimExprOpBin),
    OPRINIT(">", 80, 2, JimExprOpBin),
    OPRINIT("<=", 80, 2, JimExprOpBin),
    OPRINIT(">=", 80, 2, JimExprOpBin),

    OPRINIT("==", 70, 2, JimExprOpBin),
    OPRINIT("!=", 70, 2, JimExprOpBin),

    OPRINIT("&", 50, 2, JimExprOpIntBin),
    OPRINIT("^", 49, 2, JimExprOpIntBin),
    OPRINIT("|", 48, 2, JimExprOpIntBin),

    OPRINIT_LAZY("&&", 10, 2, NULL, LAZY_OP),
    OPRINIT_LAZY(NULL, 10, 2, JimExprOpAndLeft, LAZY_LEFT),
    OPRINIT_LAZY(NULL, 10, 2, JimExprOpAndOrRight, LAZY_RIGHT),

    OPRINIT_LAZY("||", 9, 2, NULL, LAZY_OP),
    OPRINIT_LAZY(NULL, 9, 2, JimExprOpOrLeft, LAZY_LEFT),
    OPRINIT_LAZY(NULL, 9, 2, JimExprOpAndOrRight, LAZY_RIGHT),

    OPRINIT_LAZY("?", 5, 2, JimExprOpNull, LAZY_OP),
    OPRINIT_LAZY(NULL, 5, 2, JimExprOpTernaryLeft, LAZY_LEFT),
    OPRINIT_LAZY(NULL, 5, 2, JimExprOpNull, LAZY_RIGHT),

    OPRINIT_LAZY(":", 5, 2, JimExprOpNull, LAZY_OP),
    OPRINIT_LAZY(NULL, 5, 2, JimExprOpColonLeft, LAZY_LEFT),
    OPRINIT_LAZY(NULL, 5, 2, JimExprOpNull, LAZY_RIGHT),

    OPRINIT("**", 250, 2, JimExprOpBin),

    OPRINIT("eq", 60, 2, JimExprOpStrBin),
    OPRINIT("ne", 60, 2, JimExprOpStrBin),

    OPRINIT("in", 55, 2, JimExprOpStrBin),
    OPRINIT("ni", 55, 2, JimExprOpStrBin),

    OPRINIT("!", 150, 1, JimExprOpNumUnary),
    OPRINIT("~", 150, 1, JimExprOpIntUnary),
    OPRINIT(NULL, 150, 1, JimExprOpNumUnary),
    OPRINIT(NULL, 150, 1, JimExprOpNumUnary),



    OPRINIT("int", 200, 1, JimExprOpNumUnary),
    OPRINIT("wide", 200, 1, JimExprOpNumUnary),
    OPRINIT("abs", 200, 1, JimExprOpNumUnary),
    OPRINIT("double", 200, 1, JimExprOpNumUnary),
    OPRINIT("round", 200, 1, JimExprOpNumUnary),
    OPRINIT("rand", 200, 0, JimExprOpNone),
    OPRINIT("srand", 200, 1, JimExprOpIntUnary),

#ifdef JIM_MATH_FUNCTIONS
    OPRINIT("sin", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("cos", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("tan", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("asin", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("acos", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("atan", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("sinh", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("cosh", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("tanh", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("ceil", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("floor", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("exp", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("log", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("log10", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("sqrt", 200, 1, JimExprOpDoubleUnary),
    OPRINIT("pow", 200, 2, JimExprOpBin),
#endif
};
#undef OPRINIT
#undef OPRINIT_LAZY

#define JIM_EXPR_OPERATORS_NUM \
    (sizeof(Jim_ExprOperators)/sizeof(struct Jim_ExprOperator))

static int JimParseExpression(struct JimParserCtx *pc)
{
    /* Discard spaces and quoted newline */
    while (isspace(UCHAR(*pc->p)) || (*(pc->p) == '\\' && *(pc->p + 1) == '\n')) {
        if (*pc->p == '\n') {
            pc->linenr++;
        }
        pc->p++;
        pc->len--;
    }

    /* Common case */
    pc->tline = pc->linenr;
    pc->tstart = pc->p;

    if (pc->len == 0) {
        pc->tend = pc->p;
        pc->tt = JIM_TT_EOL;
        pc->eof = 1;
        return JIM_OK;
    }
    switch (*(pc->p)) {
        case '(':
                pc->tt = JIM_TT_SUBEXPR_START;
                goto singlechar;
        case ')':
                pc->tt = JIM_TT_SUBEXPR_END;
                goto singlechar;
        case ',':
            pc->tt = JIM_TT_SUBEXPR_COMMA;
singlechar:
            pc->tend = pc->p;
            pc->p++;
            pc->len--;
            break;
        case '[':
            return JimParseCmd(pc);
        case '$':
            if (JimParseVar(pc) == JIM_ERR)
                return JimParseExprOperator(pc);
            else {
                /* Don't allow expr sugar in expressions */
                if (pc->tt == JIM_TT_EXPRSUGAR) {
                    return JIM_ERR;
                }
                return JIM_OK;
            }
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '.':
            return JimParseExprNumber(pc);
        case '"':
            return JimParseQuote(pc);
        case '{':
            return JimParseBrace(pc);

        case 'N':
        case 'I':
        case 'n':
        case 'i':
            if (JimParseExprIrrational(pc) == JIM_ERR)
                return JimParseExprOperator(pc);
            break;
        default:
            return JimParseExprOperator(pc);
            break;
    }
    return JIM_OK;
}

static int JimParseExprNumber(struct JimParserCtx *pc)
{
    char *end;

    /* Assume an integer for now */
    pc->tt = JIM_TT_EXPR_INT;

    jim_strtoull(pc->p, (char **)&pc->p);
    /* Tried as an integer, but perhaps it parses as a double */
    if (strchr("eENnIi.", *pc->p) || pc->p == pc->tstart) {
        /* Some stupid compilers insist they are cleverer that
         * we are. Even a (void) cast doesn't prevent this warning!
         */
        if (strtod(pc->tstart, &end)) { /* nothing */ }
        if (end == pc->tstart)
            return JIM_ERR;
        if (end > pc->p) {
            /* Yes, double captured more chars */
            pc->tt = JIM_TT_EXPR_DOUBLE;
            pc->p = end;
        }
    }
    pc->tend = pc->p - 1;
    pc->len -= (pc->p - pc->tstart);
    return JIM_OK;
}

static int JimParseExprIrrational(struct JimParserCtx *pc)
{
    const char *irrationals[] = { "NaN", "nan", "NAN", "Inf", "inf", "INF", NULL };
    int i;

    for (i = 0; irrationals[i]; i++) {
        const char *irr = irrationals[i];

        if (strncmp(irr, pc->p, 3) == 0) {
            pc->p += 3;
            pc->len -= 3;
            pc->tend = pc->p - 1;
            pc->tt = JIM_TT_EXPR_DOUBLE;
            return JIM_OK;
        }
    }
    return JIM_ERR;
}

static int JimParseExprOperator(struct JimParserCtx *pc)
{
    int i;
    int bestIdx = -1, bestLen = 0;

    /* Try to get the longest match. */
    for (i = 0; i < (signed)JIM_EXPR_OPERATORS_NUM; i++) {
        const char * const opname = Jim_ExprOperators[i].name;
        const int oplen = Jim_ExprOperators[i].namelen;

        if (opname == NULL || opname[0] != pc->p[0]) {
            continue;
        }

        if (oplen > bestLen && strncmp(opname, pc->p, oplen) == 0) {
            bestIdx = i + JIM_TT_EXPR_OP;
            bestLen = oplen;
        }
    }
    if (bestIdx == -1) {
        return JIM_ERR;
    }

    /* Validate paretheses around function arguments */
    if (bestIdx >= JIM_EXPROP_FUNC_FIRST) {
        const char *p = pc->p + bestLen;
        int len = pc->len - bestLen;

        while (len && isspace(UCHAR(*p))) {
            len--;
            p++;
        }
        if (*p != '(') {
            return JIM_ERR;
        }
    }
    pc->tend = pc->p + bestLen - 1;
    pc->p += bestLen;
    pc->len -= bestLen;

    pc->tt = bestIdx;
    return JIM_OK;
}

static const struct Jim_ExprOperator *JimExprOperatorInfoByOpcode(int opcode)
{
    static Jim_ExprOperator dummy_op;
    if (opcode < JIM_TT_EXPR_OP) {
        return &dummy_op;
    }
    return &Jim_ExprOperators[opcode - JIM_TT_EXPR_OP];
}

const char *jim_tt_name(int type)
{
    static const char * const tt_names[JIM_TT_EXPR_OP] =
        { "NIL", "STR", "ESC", "VAR", "ARY", "CMD", "SEP", "EOL", "EOF", "LIN", "WRD", "(((", ")))", ",,,", "INT",
            "DBL", "$()" };
    if (type < JIM_TT_EXPR_OP) {
        return tt_names[type];
    }
    else {
        const struct Jim_ExprOperator *op = JimExprOperatorInfoByOpcode(type);
        static char buf[20];

        if (op->name) {
            return op->name;
        }
        sprintf(buf, "(%d)", type);
        return buf;
    }
}

/* -----------------------------------------------------------------------------
 * Expression Object
 * ---------------------------------------------------------------------------*/
static void FreeExprInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupExprInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static int SetExprFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

static const Jim_ObjType exprObjType = {
    "expression",
    FreeExprInternalRep,
    DupExprInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

/* Expr bytecode structure */
typedef struct ExprByteCode
{
    ScriptToken *token;         /* Tokens array. */
    int len;                    /* Length as number of tokens. */
    int inUse;                  /* Used for sharing. */
} ExprByteCode;

static void ExprFreeByteCode(Jim_Interp *interp, ExprByteCode * expr)
{
    int i;

    for (i = 0; i < expr->len; i++) {
        Jim_DecrRefCount(interp, expr->token[i].objPtr);
    }
    Jim_Free(expr->token);
    Jim_Free(expr);
}

static void FreeExprInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    ExprByteCode *expr = (void *)objPtr->internalRep.ptr;

    if (expr) {
        if (--expr->inUse != 0) {
            return;
        }

        ExprFreeByteCode(interp, expr);
    }
}

static void DupExprInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    JIM_NOTUSED(interp);
    JIM_NOTUSED(srcPtr);

    /* Just returns an simple string. */
    dupPtr->typePtr = NULL;
}

/* Check if an expr program looks correct. */
static int ExprCheckCorrectness(ExprByteCode * expr)
{
    int i;
    int stacklen = 0;
    int ternary = 0;

    /* Try to check if there are stack underflows,
     * and make sure at the end of the program there is
     * a single result on the stack. */
    for (i = 0; i < expr->len; i++) {
        ScriptToken *t = &expr->token[i];
        const struct Jim_ExprOperator *op = JimExprOperatorInfoByOpcode(t->type);

        stacklen -= op->arity;
        if (stacklen < 0) {
            break;
        }
        if (t->type == JIM_EXPROP_TERNARY || t->type == JIM_EXPROP_TERNARY_LEFT) {
            ternary++;
        }
        else if (t->type == JIM_EXPROP_COLON || t->type == JIM_EXPROP_COLON_LEFT) {
            ternary--;
        }

        /* All operations and operands add one to the stack */
        stacklen++;
    }
    if (stacklen != 1 || ternary != 0) {
        return JIM_ERR;
    }
    return JIM_OK;
}

/* This procedure converts every occurrence of || and && opereators
 * in lazy unary versions.
 *
 * a b || is converted into:
 *
 * a <offset> |L b |R
 *
 * a b && is converted into:
 *
 * a <offset> &L b &R
 *
 * "|L" checks if 'a' is true:
 *   1) if it is true pushes 1 and skips <offset> instructions to reach
 *      the opcode just after |R.
 *   2) if it is false does nothing.
 * "|R" checks if 'b' is true:
 *   1) if it is true pushes 1, otherwise pushes 0.
 *
 * "&L" checks if 'a' is true:
 *   1) if it is true does nothing.
 *   2) If it is false pushes 0 and skips <offset> instructions to reach
 *      the opcode just after &R
 * "&R" checks if 'a' is true:
 *      if it is true pushes 1, otherwise pushes 0.
 */
static int ExprAddLazyOperator(Jim_Interp *interp, ExprByteCode * expr, ParseToken *t)
{
    int i;

    int leftindex, arity, offset;

    /* Search for the end of the first operator */
    leftindex = expr->len - 1;

    arity = 1;
    while (arity) {
        ScriptToken *tt = &expr->token[leftindex];

        if (tt->type >= JIM_TT_EXPR_OP) {
            arity += JimExprOperatorInfoByOpcode(tt->type)->arity;
        }
        arity--;
        if (--leftindex < 0) {
            return JIM_ERR;
        }
    }
    leftindex++;

    /* Move them up */
    memmove(&expr->token[leftindex + 2], &expr->token[leftindex],
        sizeof(*expr->token) * (expr->len - leftindex));
    expr->len += 2;
    offset = (expr->len - leftindex) - 1;

    /* Now we rely on the fact that the left and right version have opcodes
     * 1 and 2 after the main opcode respectively
     */
    expr->token[leftindex + 1].type = t->type + 1;
    expr->token[leftindex + 1].objPtr = interp->emptyObj;

    expr->token[leftindex].type = JIM_TT_EXPR_INT;
    expr->token[leftindex].objPtr = Jim_NewIntObj(interp, offset);

    /* Now add the 'R' operator */
    expr->token[expr->len].objPtr = interp->emptyObj;
    expr->token[expr->len].type = t->type + 2;
    expr->len++;

    /* Do we need to adjust the skip count for any &L, |L, ?L or :L in the left operand? */
    for (i = leftindex - 1; i > 0; i--) {
        const struct Jim_ExprOperator *op = JimExprOperatorInfoByOpcode(expr->token[i].type);
        if (op->lazy == LAZY_LEFT) {
            if (JimWideValue(expr->token[i - 1].objPtr) + i - 1 >= leftindex) {
                JimWideValue(expr->token[i - 1].objPtr) += 2;
            }
        }
    }
    return JIM_OK;
}

static int ExprAddOperator(Jim_Interp *interp, ExprByteCode * expr, ParseToken *t)
{
    struct ScriptToken *token = &expr->token[expr->len];
    const struct Jim_ExprOperator *op = JimExprOperatorInfoByOpcode(t->type);

    if (op->lazy == LAZY_OP) {
        if (ExprAddLazyOperator(interp, expr, t) != JIM_OK) {
            Jim_SetResultFormatted(interp, "Expression has bad operands to %s", op->name);
            return JIM_ERR;
        }
    }
    else {
        token->objPtr = interp->emptyObj;
        token->type = t->type;
        expr->len++;
    }
    return JIM_OK;
}

/**
 * Returns the index of the COLON_LEFT to the left of 'right_index'
 * taking into account nesting.
 *
 * The expression *must* be well formed, thus a COLON_LEFT will always be found.
 */
static int ExprTernaryGetColonLeftIndex(ExprByteCode *expr, int right_index)
{
    int ternary_count = 1;

    right_index--;

    while (right_index > 1) {
        if (expr->token[right_index].type == JIM_EXPROP_TERNARY_LEFT) {
            ternary_count--;
        }
        else if (expr->token[right_index].type == JIM_EXPROP_COLON_RIGHT) {
            ternary_count++;
        }
        else if (expr->token[right_index].type == JIM_EXPROP_COLON_LEFT && ternary_count == 1) {
            return right_index;
        }
        right_index--;
    }

    /*notreached*/
    return -1;
}

/**
 * Find the left/right indices for the ternary expression to the left of 'right_index'.
 *
 * Returns 1 if found, and fills in *prev_right_index and *prev_left_index.
 * Otherwise returns 0.
 */
static int ExprTernaryGetMoveIndices(ExprByteCode *expr, int right_index, int *prev_right_index, int *prev_left_index)
{
    int i = right_index - 1;
    int ternary_count = 1;

    while (i > 1) {
        if (expr->token[i].type == JIM_EXPROP_TERNARY_LEFT) {
            if (--ternary_count == 0 && expr->token[i - 2].type == JIM_EXPROP_COLON_RIGHT) {
                *prev_right_index = i - 2;
                *prev_left_index = ExprTernaryGetColonLeftIndex(expr, *prev_right_index);
                return 1;
            }
        }
        else if (expr->token[i].type == JIM_EXPROP_COLON_RIGHT) {
            if (ternary_count == 0) {
                return 0;
            }
            ternary_count++;
        }
        i--;
    }
    return 0;
}

/*
* ExprTernaryReorderExpression description
* ========================================
*
* ?: is right-to-left associative which doesn't work with the stack-based
* expression engine. The fix is to reorder the bytecode.
*
* The expression:
*
*    expr 1?2:0?3:4
*
* Has initial bytecode:
*
*    '1' '2' (40=TERNARY_LEFT) '2' (41=TERNARY_RIGHT) '2' (43=COLON_LEFT) '0' (44=COLON_RIGHT)
*    '2' (40=TERNARY_LEFT) '3' (41=TERNARY_RIGHT) '2' (43=COLON_LEFT) '4' (44=COLON_RIGHT)
*
* The fix involves simulating this expression instead:
*
*    expr 1?2:(0?3:4)
*
* With the following bytecode:
*
*    '1' '2' (40=TERNARY_LEFT) '2' (41=TERNARY_RIGHT) '10' (43=COLON_LEFT) '0' '2' (40=TERNARY_LEFT)
*    '3' (41=TERNARY_RIGHT) '2' (43=COLON_LEFT) '4' (44=COLON_RIGHT) (44=COLON_RIGHT)
*
* i.e. The token COLON_RIGHT at index 8 is moved towards the end of the stack, all tokens above 8
*      are shifted down and the skip count of the token JIM_EXPROP_COLON_LEFT at index 5 is
*      incremented by the amount tokens shifted down. The token JIM_EXPROP_COLON_RIGHT that is moved
*      is identified as immediately preceeding a token JIM_EXPROP_TERNARY_LEFT
*
* ExprTernaryReorderExpression works thus as follows :
* - start from the end of the stack
* - while walking towards the beginning of the stack
*     if token=JIM_EXPROP_COLON_RIGHT then
*        find the associated token JIM_EXPROP_TERNARY_LEFT, which allows to
*            find the associated token previous(JIM_EXPROP_COLON_RIGHT)
*            find the associated token previous(JIM_EXPROP_LEFT_RIGHT)
*        if all found then
*            perform the rotation
*            update the skip count of the token previous(JIM_EXPROP_LEFT_RIGHT)
*        end if
*    end if
*
* Note: care has to be taken for nested ternary constructs!!!
*/
static void ExprTernaryReorderExpression(Jim_Interp *interp, ExprByteCode *expr)
{
    int i;

    for (i = expr->len - 1; i > 1; i--) {
        int prev_right_index;
        int prev_left_index;
        int j;
        ScriptToken tmp;

        if (expr->token[i].type != JIM_EXPROP_COLON_RIGHT) {
            continue;
        }

        /* COLON_RIGHT found: get the indexes needed to move the tokens in the stack (if any) */
        if (ExprTernaryGetMoveIndices(expr, i, &prev_right_index, &prev_left_index) == 0) {
            continue;
        }

        /*
        ** rotate tokens down
        **
        ** +->  [i]                         : JIM_EXPROP_COLON_RIGHT
        ** |     |                             |
        ** |     V                             V
        ** |   [...]                        : ...
        ** |     |                             |
        ** |     V                             V
        ** |   [...]                        : ...
        ** |     |                             |
        ** |     V                             V
        ** +-  [prev_right_index]           : JIM_EXPROP_COLON_RIGHT
        */
        tmp = expr->token[prev_right_index];
        for (j = prev_right_index; j < i; j++) {
            expr->token[j] = expr->token[j + 1];
        }
        expr->token[i] = tmp;

        /* Increment the 'skip' count associated to the previous JIM_EXPROP_COLON_LEFT token
         *
         * This is 'colon left increment' = i - prev_right_index
         *
         * [prev_left_index]      : JIM_EXPROP_LEFT_RIGHT
         * [prev_left_index-1]    : skip_count
         *
         */
        JimWideValue(expr->token[prev_left_index-1].objPtr) += (i - prev_right_index);

        /* Adjust for i-- in the loop */
        i++;
    }
}

static ExprByteCode *ExprCreateByteCode(Jim_Interp *interp, const ParseTokenList *tokenlist, Jim_Obj *fileNameObj)
{
    Jim_Stack stack;
    ExprByteCode *expr;
    int ok = 1;
    int i;
    int prevtt = JIM_TT_NONE;
    int have_ternary = 0;

    /* -1 for EOL */
    int count = tokenlist->count - 1;

    expr = Jim_Alloc(sizeof(*expr));
    expr->inUse = 1;
    expr->len = 0;

    Jim_InitStack(&stack);

    /* Need extra bytecodes for lazy operators.
     * Also check for the ternary operator
     */
    for (i = 0; i < tokenlist->count; i++) {
        ParseToken *t = &tokenlist->list[i];
        const struct Jim_ExprOperator *op = JimExprOperatorInfoByOpcode(t->type);

        if (op->lazy == LAZY_OP) {
            count += 2;
            /* Ternary is a lazy op but also needs reordering */
            if (t->type == JIM_EXPROP_TERNARY) {
                have_ternary = 1;
            }
        }
    }

    expr->token = Jim_Alloc(sizeof(ScriptToken) * count);

    for (i = 0; i < tokenlist->count && ok; i++) {
        ParseToken *t = &tokenlist->list[i];

        /* Next token will be stored here */
        struct ScriptToken *token = &expr->token[expr->len];

        if (t->type == JIM_TT_EOL) {
            break;
        }

        switch (t->type) {
            case JIM_TT_STR:
            case JIM_TT_ESC:
            case JIM_TT_VAR:
            case JIM_TT_DICTSUGAR:
            case JIM_TT_EXPRSUGAR:
            case JIM_TT_CMD:
                token->type = t->type;
strexpr:
                token->objPtr = Jim_NewStringObj(interp, t->token, t->len);
                if (t->type == JIM_TT_CMD) {
                    /* Only commands need source info */
                    JimSetSourceInfo(interp, token->objPtr, fileNameObj, t->line);
                }
                expr->len++;
                break;

            case JIM_TT_EXPR_INT:
            case JIM_TT_EXPR_DOUBLE:
                {
                    char *endptr;
                    if (t->type == JIM_TT_EXPR_INT) {
                        token->objPtr = Jim_NewIntObj(interp, jim_strtoull(t->token, &endptr));
                    }
                    else {
                        token->objPtr = Jim_NewDoubleObj(interp, strtod(t->token, &endptr));
                    }
                    if (endptr != t->token + t->len) {
                        /* Conversion failed, so just store it as a string */
                        Jim_FreeNewObj(interp, token->objPtr);
                        token->type = JIM_TT_STR;
                        goto strexpr;
                    }
                    token->type = t->type;
                    expr->len++;
                }
                break;

            case JIM_TT_SUBEXPR_START:
                Jim_StackPush(&stack, t);
                prevtt = JIM_TT_NONE;
                continue;

            case JIM_TT_SUBEXPR_COMMA:
                /* Simple approach. Comma is simply ignored */
                continue;

            case JIM_TT_SUBEXPR_END:
                ok = 0;
                while (Jim_StackLen(&stack)) {
                    ParseToken *tt = Jim_StackPop(&stack);

                    if (tt->type == JIM_TT_SUBEXPR_START) {
                        ok = 1;
                        break;
                    }

                    if (ExprAddOperator(interp, expr, tt) != JIM_OK) {
                        goto err;
                    }
                }
                if (!ok) {
                    Jim_SetResultString(interp, "Unexpected close parenthesis", -1);
                    goto err;
                }
                break;


            default:{
                    /* Must be an operator */
                    const struct Jim_ExprOperator *op;
                    ParseToken *tt;

                    /* Convert -/+ to unary minus or unary plus if necessary */
                    if (prevtt == JIM_TT_NONE || prevtt >= JIM_TT_EXPR_OP) {
                        if (t->type == JIM_EXPROP_SUB) {
                            t->type = JIM_EXPROP_UNARYMINUS;
                        }
                        else if (t->type == JIM_EXPROP_ADD) {
                            t->type = JIM_EXPROP_UNARYPLUS;
                        }
                    }

                    op = JimExprOperatorInfoByOpcode(t->type);

                    /* Now handle precedence */
                    while ((tt = Jim_StackPeek(&stack)) != NULL) {
                        const struct Jim_ExprOperator *tt_op =
                            JimExprOperatorInfoByOpcode(tt->type);

                        /* Note that right-to-left associativity of ?: operator is handled later */

                        if (op->arity != 1 && tt_op->precedence >= op->precedence) {
                            if (ExprAddOperator(interp, expr, tt) != JIM_OK) {
                                ok = 0;
                                goto err;
                            }
                            Jim_StackPop(&stack);
                        }
                        else {
                            break;
                        }
                    }
                    Jim_StackPush(&stack, t);
                    break;
                }
        }
        prevtt = t->type;
    }

    /* Reduce any remaining subexpr */
    while (Jim_StackLen(&stack)) {
        ParseToken *tt = Jim_StackPop(&stack);

        if (tt->type == JIM_TT_SUBEXPR_START) {
            ok = 0;
            Jim_SetResultString(interp, "Missing close parenthesis", -1);
            goto err;
        }
        if (ExprAddOperator(interp, expr, tt) != JIM_OK) {
            ok = 0;
            goto err;
        }
    }

    if (have_ternary) {
        ExprTernaryReorderExpression(interp, expr);
    }

  err:
    /* Free the stack used for the compilation. */
    Jim_FreeStack(&stack);

    for (i = 0; i < expr->len; i++) {
        Jim_IncrRefCount(expr->token[i].objPtr);
    }

    if (!ok) {
        ExprFreeByteCode(interp, expr);
        return NULL;
    }

    return expr;
}


/* This method takes the string representation of an expression
 * and generates a program for the Expr's stack-based VM. */
static int SetExprFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    int exprTextLen;
    const char *exprText;
    struct JimParserCtx parser;
    struct ExprByteCode *expr;
    ParseTokenList tokenlist;
    int line;
    Jim_Obj *fileNameObj;
    int rc = JIM_ERR;

    /* Try to get information about filename / line number */
    if (objPtr->typePtr == &sourceObjType) {
        fileNameObj = objPtr->internalRep.sourceValue.fileNameObj;
        line = objPtr->internalRep.sourceValue.lineNumber;
    }
    else {
        fileNameObj = interp->emptyObj;
        line = 1;
    }
    Jim_IncrRefCount(fileNameObj);

    exprText = Jim_GetString(objPtr, &exprTextLen);

    /* Initially tokenise the expression into tokenlist */
    ScriptTokenListInit(&tokenlist);

    JimParserInit(&parser, exprText, exprTextLen, line);
    while (!parser.eof) {
        if (JimParseExpression(&parser) != JIM_OK) {
            ScriptTokenListFree(&tokenlist);
          invalidexpr:
            Jim_SetResultFormatted(interp, "syntax error in expression: \"%#s\"", objPtr);
            expr = NULL;
            goto err;
        }

        ScriptAddToken(&tokenlist, parser.tstart, parser.tend - parser.tstart + 1, parser.tt,
            parser.tline);
    }

#ifdef DEBUG_SHOW_EXPR_TOKENS
    {
        int i;
        printf("==== Expr Tokens (%s) ====\n", Jim_String(fileNameObj));
        for (i = 0; i < tokenlist.count; i++) {
            printf("[%2d]@%d %s '%.*s'\n", i, tokenlist.list[i].line, jim_tt_name(tokenlist.list[i].type),
                tokenlist.list[i].len, tokenlist.list[i].token);
        }
    }
#endif

    if (JimParseCheckMissing(interp, parser.missing.ch) == JIM_ERR) {
        ScriptTokenListFree(&tokenlist);
        Jim_DecrRefCount(interp, fileNameObj);
        return JIM_ERR;
    }

    /* Now create the expression bytecode from the tokenlist */
    expr = ExprCreateByteCode(interp, &tokenlist, fileNameObj);

    /* No longer need the token list */
    ScriptTokenListFree(&tokenlist);

    if (!expr) {
        goto err;
    }

#ifdef DEBUG_SHOW_EXPR
    {
        int i;

        printf("==== Expr ====\n");
        for (i = 0; i < expr->len; i++) {
            ScriptToken *t = &expr->token[i];

            printf("[%2d] %s '%s'\n", i, jim_tt_name(t->type), Jim_String(t->objPtr));
        }
    }
#endif

    /* Check program correctness. */
    if (ExprCheckCorrectness(expr) != JIM_OK) {
        ExprFreeByteCode(interp, expr);
        goto invalidexpr;
    }

    rc = JIM_OK;

  err:
    /* Free the old internal rep and set the new one. */
    Jim_DecrRefCount(interp, fileNameObj);
    Jim_FreeIntRep(interp, objPtr);
    Jim_SetIntRepPtr(objPtr, expr);
    objPtr->typePtr = &exprObjType;
    return rc;
}

static ExprByteCode *JimGetExpression(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (objPtr->typePtr != &exprObjType) {
        if (SetExprFromAny(interp, objPtr) != JIM_OK) {
            return NULL;
        }
    }
    return (ExprByteCode *) Jim_GetIntRepPtr(objPtr);
}

#ifdef JIM_OPTIMIZATION
static Jim_Obj *JimExprIntValOrVar(Jim_Interp *interp, const ScriptToken *token)
{
    if (token->type == JIM_TT_EXPR_INT)
        return token->objPtr;
    else if (token->type == JIM_TT_VAR)
        return Jim_GetVariable(interp, token->objPtr, JIM_NONE);
    else if (token->type == JIM_TT_DICTSUGAR)
        return JimExpandDictSugar(interp, token->objPtr);
    else
        return NULL;
}
#endif

/* -----------------------------------------------------------------------------
 * Expressions evaluation.
 * Jim uses a specialized stack-based virtual machine for expressions,
 * that takes advantage of the fact that expr's operators
 * can't be redefined.
 *
 * Jim_EvalExpression() uses the bytecode compiled by
 * SetExprFromAny() method of the "expression" object.
 *
 * On success a Tcl Object containing the result of the evaluation
 * is stored into expResultPtrPtr (having refcount of 1), and JIM_OK is
 * returned.
 * On error the function returns a retcode != to JIM_OK and set a suitable
 * error on the interp.
 * ---------------------------------------------------------------------------*/
#define JIM_EE_STATICSTACK_LEN 10

int Jim_EvalExpression(Jim_Interp *interp, Jim_Obj *exprObjPtr, Jim_Obj **exprResultPtrPtr)
{
    ExprByteCode *expr;
    Jim_Obj *staticStack[JIM_EE_STATICSTACK_LEN];
    int i;
    int retcode = JIM_OK;
    struct JimExprState e;

    expr = JimGetExpression(interp, exprObjPtr);
    if (!expr) {
        return JIM_ERR;         /* error in expression. */
    }

#ifdef JIM_OPTIMIZATION
    /* Check for one of the following common expressions used by while/for
     *
     *   CONST
     *   $a
     *   !$a
     *   $a < CONST, $a < $b
     *   $a <= CONST, $a <= $b
     *   $a > CONST, $a > $b
     *   $a >= CONST, $a >= $b
     *   $a != CONST, $a != $b
     *   $a == CONST, $a == $b
     */
    {
        Jim_Obj *objPtr;

        /* STEP 1 -- Check if there are the conditions to run the specialized
         * version of while */

        switch (expr->len) {
            case 1:
                objPtr = JimExprIntValOrVar(interp, &expr->token[0]);
                if (objPtr) {
                    Jim_IncrRefCount(objPtr);
                    *exprResultPtrPtr = objPtr;
                    return JIM_OK;
                }
                break;

            case 2:
                if (expr->token[1].type == JIM_EXPROP_NOT) {
                    objPtr = JimExprIntValOrVar(interp, &expr->token[0]);

                    if (objPtr && JimIsWide(objPtr)) {
                        *exprResultPtrPtr = JimWideValue(objPtr) ? interp->falseObj : interp->trueObj;
                        Jim_IncrRefCount(*exprResultPtrPtr);
                        return JIM_OK;
                    }
                }
                break;

            case 3:
                objPtr = JimExprIntValOrVar(interp, &expr->token[0]);
                if (objPtr && JimIsWide(objPtr)) {
                    Jim_Obj *objPtr2 = JimExprIntValOrVar(interp, &expr->token[1]);
                    if (objPtr2 && JimIsWide(objPtr2)) {
                        jim_wide wideValueA = JimWideValue(objPtr);
                        jim_wide wideValueB = JimWideValue(objPtr2);
                        int cmpRes;
                        switch (expr->token[2].type) {
                            case JIM_EXPROP_LT:
                                cmpRes = wideValueA < wideValueB;
                                break;
                            case JIM_EXPROP_LTE:
                                cmpRes = wideValueA <= wideValueB;
                                break;
                            case JIM_EXPROP_GT:
                                cmpRes = wideValueA > wideValueB;
                                break;
                            case JIM_EXPROP_GTE:
                                cmpRes = wideValueA >= wideValueB;
                                break;
                            case JIM_EXPROP_NUMEQ:
                                cmpRes = wideValueA == wideValueB;
                                break;
                            case JIM_EXPROP_NUMNE:
                                cmpRes = wideValueA != wideValueB;
                                break;
                            default:
                                goto noopt;
                        }
                        *exprResultPtrPtr = cmpRes ? interp->trueObj : interp->falseObj;
                        Jim_IncrRefCount(*exprResultPtrPtr);
                        return JIM_OK;
                    }
                }
                break;
        }
    }
noopt:
#endif

    /* In order to avoid that the internal repr gets freed due to
     * shimmering of the exprObjPtr's object, we make the internal rep
     * shared. */
    expr->inUse++;

    /* The stack-based expr VM itself */

    /* Stack allocation. Expr programs have the feature that
     * a program of length N can't require a stack longer than
     * N. */
    if (expr->len > JIM_EE_STATICSTACK_LEN)
        e.stack = Jim_Alloc(sizeof(Jim_Obj *) * expr->len);
    else
        e.stack = staticStack;

    e.stacklen = 0;

    /* Execute every instruction */
    for (i = 0; i < expr->len && retcode == JIM_OK; i++) {
        Jim_Obj *objPtr;

        switch (expr->token[i].type) {
            case JIM_TT_EXPR_INT:
            case JIM_TT_EXPR_DOUBLE:
            case JIM_TT_STR:
                ExprPush(&e, expr->token[i].objPtr);
                break;

            case JIM_TT_VAR:
                objPtr = Jim_GetVariable(interp, expr->token[i].objPtr, JIM_ERRMSG);
                if (objPtr) {
                    ExprPush(&e, objPtr);
                }
                else {
                    retcode = JIM_ERR;
                }
                break;

            case JIM_TT_DICTSUGAR:
                objPtr = JimExpandDictSugar(interp, expr->token[i].objPtr);
                if (objPtr) {
                    ExprPush(&e, objPtr);
                }
                else {
                    retcode = JIM_ERR;
                }
                break;

            case JIM_TT_ESC:
                retcode = Jim_SubstObj(interp, expr->token[i].objPtr, &objPtr, JIM_NONE);
                if (retcode == JIM_OK) {
                    ExprPush(&e, objPtr);
                }
                break;

            case JIM_TT_CMD:
                retcode = Jim_EvalObj(interp, expr->token[i].objPtr);
                if (retcode == JIM_OK) {
                    ExprPush(&e, Jim_GetResult(interp));
                }
                break;

            default:{
                    /* Find and execute the operation */
                    e.skip = 0;
                    e.opcode = expr->token[i].type;

                    retcode = JimExprOperatorInfoByOpcode(e.opcode)->funcop(interp, &e);
                    /* Skip some opcodes if necessary */
                    i += e.skip;
                    continue;
                }
        }
    }

    expr->inUse--;

    if (retcode == JIM_OK) {
        *exprResultPtrPtr = ExprPop(&e);
    }
    else {
        for (i = 0; i < e.stacklen; i++) {
            Jim_DecrRefCount(interp, e.stack[i]);
        }
    }
    if (e.stack != staticStack) {
        Jim_Free(e.stack);
    }
    return retcode;
}

int Jim_GetBoolFromExpr(Jim_Interp *interp, Jim_Obj *exprObjPtr, int *boolPtr)
{
    int retcode;
    jim_wide wideValue;
    double doubleValue;
    Jim_Obj *exprResultPtr;

    retcode = Jim_EvalExpression(interp, exprObjPtr, &exprResultPtr);
    if (retcode != JIM_OK)
        return retcode;

    if (JimGetWideNoErr(interp, exprResultPtr, &wideValue) != JIM_OK) {
        if (Jim_GetDouble(interp, exprResultPtr, &doubleValue) != JIM_OK) {
            Jim_DecrRefCount(interp, exprResultPtr);
            return JIM_ERR;
        }
        else {
            Jim_DecrRefCount(interp, exprResultPtr);
            *boolPtr = doubleValue != 0;
            return JIM_OK;
        }
    }
    *boolPtr = wideValue != 0;

    Jim_DecrRefCount(interp, exprResultPtr);
    return JIM_OK;
}

/* -----------------------------------------------------------------------------
 * ScanFormat String Object
 * ---------------------------------------------------------------------------*/

/* This Jim_Obj will held a parsed representation of a format string passed to
 * the Jim_ScanString command. For error diagnostics, the scanformat string has
 * to be parsed in its entirely first and then, if correct, can be used for
 * scanning. To avoid endless re-parsing, the parsed representation will be
 * stored in an internal representation and re-used for performance reason. */

/* A ScanFmtPartDescr will held the information of /one/ part of the whole
 * scanformat string. This part will later be used to extract information
 * out from the string to be parsed by Jim_ScanString */

typedef struct ScanFmtPartDescr
{
    char *arg;                  /* Specification of a CHARSET conversion */
    char *prefix;               /* Prefix to be scanned literally before conversion */
    size_t width;               /* Maximal width of input to be converted */
    int pos;                    /* -1 - no assign, 0 - natural pos, >0 - XPG3 pos */
    char type;                  /* Type of conversion (e.g. c, d, f) */
    char modifier;              /* Modify type (e.g. l - long, h - short */
} ScanFmtPartDescr;

/* The ScanFmtStringObj will hold the internal representation of a scanformat
 * string parsed and separated in part descriptions. Furthermore it contains
 * the original string representation of the scanformat string to allow for
 * fast update of the Jim_Obj's string representation part.
 *
 * As an add-on the internal object representation adds some scratch pad area
 * for usage by Jim_ScanString to avoid endless allocating and freeing of
 * memory for purpose of string scanning.
 *
 * The error member points to a static allocated string in case of a mal-
 * formed scanformat string or it contains '0' (NULL) in case of a valid
 * parse representation.
 *
 * The whole memory of the internal representation is allocated as a single
 * area of memory that will be internally separated. So freeing and duplicating
 * of such an object is cheap */

typedef struct ScanFmtStringObj
{
    jim_wide size;              /* Size of internal repr in bytes */
    char *stringRep;            /* Original string representation */
    size_t count;               /* Number of ScanFmtPartDescr contained */
    size_t convCount;           /* Number of conversions that will assign */
    size_t maxPos;              /* Max position index if XPG3 is used */
    const char *error;          /* Ptr to error text (NULL if no error */
    char *scratch;              /* Some scratch pad used by Jim_ScanString */
    ScanFmtPartDescr descr[1];  /* The vector of partial descriptions */
} ScanFmtStringObj;


static void FreeScanFmtInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupScanFmtInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static void UpdateStringOfScanFmt(Jim_Obj *objPtr);

static const Jim_ObjType scanFmtStringObjType = {
    "scanformatstring",
    FreeScanFmtInternalRep,
    DupScanFmtInternalRep,
    UpdateStringOfScanFmt,
    JIM_TYPE_NONE,
};

void FreeScanFmtInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    JIM_NOTUSED(interp);
    Jim_Free((char *)objPtr->internalRep.ptr);
    objPtr->internalRep.ptr = 0;
}

void DupScanFmtInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    size_t size = (size_t) ((ScanFmtStringObj *) srcPtr->internalRep.ptr)->size;
    ScanFmtStringObj *newVec = (ScanFmtStringObj *) Jim_Alloc(size);

    JIM_NOTUSED(interp);
    memcpy(newVec, srcPtr->internalRep.ptr, size);
    dupPtr->internalRep.ptr = newVec;
    dupPtr->typePtr = &scanFmtStringObjType;
}

static void UpdateStringOfScanFmt(Jim_Obj *objPtr)
{
    JimSetStringBytes(objPtr, ((ScanFmtStringObj *) objPtr->internalRep.ptr)->stringRep);
}

/* SetScanFmtFromAny will parse a given string and create the internal
 * representation of the format specification. In case of an error
 * the error data member of the internal representation will be set
 * to an descriptive error text and the function will be left with
 * JIM_ERR to indicate unsucessful parsing (aka. malformed scanformat
 * specification */

static int SetScanFmtFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    ScanFmtStringObj *fmtObj;
    char *buffer;
    int maxCount, i, approxSize, lastPos = -1;
    const char *fmt = objPtr->bytes;
    int maxFmtLen = objPtr->length;
    const char *fmtEnd = fmt + maxFmtLen;
    int curr;

    Jim_FreeIntRep(interp, objPtr);
    /* Count how many conversions could take place maximally */
    for (i = 0, maxCount = 0; i < maxFmtLen; ++i)
        if (fmt[i] == '%')
            ++maxCount;
    /* Calculate an approximation of the memory necessary */
    approxSize = sizeof(ScanFmtStringObj)       /* Size of the container */
        +(maxCount + 1) * sizeof(ScanFmtPartDescr)      /* Size of all partials */
        +maxFmtLen * sizeof(char) + 3 + 1       /* Scratch + "%n" + '\0' */
        + maxFmtLen * sizeof(char) + 1  /* Original stringrep */
        + maxFmtLen * sizeof(char)      /* Arg for CHARSETs */
        +(maxCount + 1) * sizeof(char)  /* '\0' for every partial */
        +1;                     /* safety byte */
    fmtObj = (ScanFmtStringObj *) Jim_Alloc(approxSize);
    memset(fmtObj, 0, approxSize);
    fmtObj->size = approxSize;
    fmtObj->maxPos = 0;
    fmtObj->scratch = (char *)&fmtObj->descr[maxCount + 1];
    fmtObj->stringRep = fmtObj->scratch + maxFmtLen + 3 + 1;
    memcpy(fmtObj->stringRep, fmt, maxFmtLen);
    buffer = fmtObj->stringRep + maxFmtLen + 1;
    objPtr->internalRep.ptr = fmtObj;
    objPtr->typePtr = &scanFmtStringObjType;
    for (i = 0, curr = 0; fmt < fmtEnd; ++fmt) {
        int width = 0, skip;
        ScanFmtPartDescr *descr = &fmtObj->descr[curr];

        fmtObj->count++;
        descr->width = 0;       /* Assume width unspecified */
        /* Overread and store any "literal" prefix */
        if (*fmt != '%' || fmt[1] == '%') {
            descr->type = 0;
            descr->prefix = &buffer[i];
            for (; fmt < fmtEnd; ++fmt) {
                if (*fmt == '%') {
                    if (fmt[1] != '%')
                        break;
                    ++fmt;
                }
                buffer[i++] = *fmt;
            }
            buffer[i++] = 0;
        }
        /* Skip the conversion introducing '%' sign */
        ++fmt;
        /* End reached due to non-conversion literal only? */
        if (fmt >= fmtEnd)
            goto done;
        descr->pos = 0;         /* Assume "natural" positioning */
        if (*fmt == '*') {
            descr->pos = -1;    /* Okay, conversion will not be assigned */
            ++fmt;
        }
        else
            fmtObj->convCount++;        /* Otherwise count as assign-conversion */
        /* Check if next token is a number (could be width or pos */
        if (sscanf(fmt, "%d%n", &width, &skip) == 1) {
            fmt += skip;
            /* Was the number a XPG3 position specifier? */
            if (descr->pos != -1 && *fmt == '$') {
                int prev;

                ++fmt;
                descr->pos = width;
                width = 0;
                /* Look if "natural" postioning and XPG3 one was mixed */
                if ((lastPos == 0 && descr->pos > 0)
                    || (lastPos > 0 && descr->pos == 0)) {
                    fmtObj->error = "cannot mix \"%\" and \"%n$\" conversion specifiers";
                    return JIM_ERR;
                }
                /* Look if this position was already used */
                for (prev = 0; prev < curr; ++prev) {
                    if (fmtObj->descr[prev].pos == -1)
                        continue;
                    if (fmtObj->descr[prev].pos == descr->pos) {
                        fmtObj->error =
                            "variable is assigned by multiple \"%n$\" conversion specifiers";
                        return JIM_ERR;
                    }
                }
                /* Try to find a width after the XPG3 specifier */
                if (sscanf(fmt, "%d%n", &width, &skip) == 1) {
                    descr->width = width;
                    fmt += skip;
                }
                if (descr->pos > 0 && (size_t) descr->pos > fmtObj->maxPos)
                    fmtObj->maxPos = descr->pos;
            }
            else {
                /* Number was not a XPG3, so it has to be a width */
                descr->width = width;
            }
        }
        /* If positioning mode was undetermined yet, fix this */
        if (lastPos == -1)
            lastPos = descr->pos;
        /* Handle CHARSET conversion type ... */
        if (*fmt == '[') {
            int swapped = 1, beg = i, end, j;

            descr->type = '[';
            descr->arg = &buffer[i];
            ++fmt;
            if (*fmt == '^')
                buffer[i++] = *fmt++;
            if (*fmt == ']')
                buffer[i++] = *fmt++;
            while (*fmt && *fmt != ']')
                buffer[i++] = *fmt++;
            if (*fmt != ']') {
                fmtObj->error = "unmatched [ in format string";
                return JIM_ERR;
            }
            end = i;
            buffer[i++] = 0;
            /* In case a range fence was given "backwards", swap it */
            while (swapped) {
                swapped = 0;
                for (j = beg + 1; j < end - 1; ++j) {
                    if (buffer[j] == '-' && buffer[j - 1] > buffer[j + 1]) {
                        char tmp = buffer[j - 1];

                        buffer[j - 1] = buffer[j + 1];
                        buffer[j + 1] = tmp;
                        swapped = 1;
                    }
                }
            }
        }
        else {
            /* Remember any valid modifier if given */
            if (strchr("hlL", *fmt) != 0)
                descr->modifier = tolower((int)*fmt++);

            descr->type = *fmt;
            if (strchr("efgcsndoxui", *fmt) == 0) {
                fmtObj->error = "bad scan conversion character";
                return JIM_ERR;
            }
            else if (*fmt == 'c' && descr->width != 0) {
                fmtObj->error = "field width may not be specified in %c " "conversion";
                return JIM_ERR;
            }
            else if (*fmt == 'u' && descr->modifier == 'l') {
                fmtObj->error = "unsigned wide not supported";
                return JIM_ERR;
            }
        }
        curr++;
    }
  done:
    return JIM_OK;
}

/* Some accessor macros to allow lowlevel access to fields of internal repr */

#define FormatGetCnvCount(_fo_) \
    ((ScanFmtStringObj*)((_fo_)->internalRep.ptr))->convCount
#define FormatGetMaxPos(_fo_) \
    ((ScanFmtStringObj*)((_fo_)->internalRep.ptr))->maxPos
#define FormatGetError(_fo_) \
    ((ScanFmtStringObj*)((_fo_)->internalRep.ptr))->error

/* JimScanAString is used to scan an unspecified string that ends with
 * next WS, or a string that is specified via a charset.
 *
 */
static Jim_Obj *JimScanAString(Jim_Interp *interp, const char *sdescr, const char *str)
{
    char *buffer = Jim_StrDup(str);
    char *p = buffer;

    while (*str) {
        int c;
        int n;

        if (!sdescr && isspace(UCHAR(*str)))
            break;              /* EOS via WS if unspecified */

        n = utf8_tounicode(str, &c);
        if (sdescr && !JimCharsetMatch(sdescr, c, JIM_CHARSET_SCAN))
            break;
        while (n--)
            *p++ = *str++;
    }
    *p = 0;
    return Jim_NewStringObjNoAlloc(interp, buffer, p - buffer);
}

/* ScanOneEntry will scan one entry out of the string passed as argument.
 * It use the sscanf() function for this task. After extracting and
 * converting of the value, the count of scanned characters will be
 * returned of -1 in case of no conversion tool place and string was
 * already scanned thru */

static int ScanOneEntry(Jim_Interp *interp, const char *str, int pos, int strLen,
    ScanFmtStringObj * fmtObj, long idx, Jim_Obj **valObjPtr)
{
    const char *tok;
    const ScanFmtPartDescr *descr = &fmtObj->descr[idx];
    size_t scanned = 0;
    size_t anchor = pos;
    int i;
    Jim_Obj *tmpObj = NULL;

    /* First pessimistically assume, we will not scan anything :-) */
    *valObjPtr = 0;
    if (descr->prefix) {
        /* There was a prefix given before the conversion, skip it and adjust
         * the string-to-be-parsed accordingly */
        for (i = 0; pos < strLen && descr->prefix[i]; ++i) {
            /* If prefix require, skip WS */
            if (isspace(UCHAR(descr->prefix[i])))
                while (pos < strLen && isspace(UCHAR(str[pos])))
                    ++pos;
            else if (descr->prefix[i] != str[pos])
                break;          /* Prefix do not match here, leave the loop */
            else
                ++pos;          /* Prefix matched so far, next round */
        }
        if (pos >= strLen) {
            return -1;          /* All of str consumed: EOF condition */
        }
        else if (descr->prefix[i] != 0)
            return 0;           /* Not whole prefix consumed, no conversion possible */
    }
    /* For all but following conversion, skip leading WS */
    if (descr->type != 'c' && descr->type != '[' && descr->type != 'n')
        while (isspace(UCHAR(str[pos])))
            ++pos;
    /* Determine how much skipped/scanned so far */
    scanned = pos - anchor;

    /* %c is a special, simple case. no width */
    if (descr->type == 'n') {
        /* Return pseudo conversion means: how much scanned so far? */
        *valObjPtr = Jim_NewIntObj(interp, anchor + scanned);
    }
    else if (pos >= strLen) {
        /* Cannot scan anything, as str is totally consumed */
        return -1;
    }
    else if (descr->type == 'c') {
            int c;
            scanned += utf8_tounicode(&str[pos], &c);
            *valObjPtr = Jim_NewIntObj(interp, c);
            return scanned;
    }
    else {
        /* Processing of conversions follows ... */
        if (descr->width > 0) {
            /* Do not try to scan as fas as possible but only the given width.
             * To ensure this, we copy the part that should be scanned. */
            size_t sLen = utf8_strlen(&str[pos], strLen - pos);
            size_t tLen = descr->width > sLen ? sLen : descr->width;

            tmpObj = Jim_NewStringObjUtf8(interp, str + pos, tLen);
            tok = tmpObj->bytes;
        }
        else {
            /* As no width was given, simply refer to the original string */
            tok = &str[pos];
        }
        switch (descr->type) {
            case 'd':
            case 'o':
            case 'x':
            case 'u':
            case 'i':{
                    char *endp; /* Position where the number finished */
                    jim_wide w;

                    int base = descr->type == 'o' ? 8
                        : descr->type == 'x' ? 16 : descr->type == 'i' ? 0 : 10;

                    /* Try to scan a number with the given base */
                    if (base == 0) {
                        w = jim_strtoull(tok, &endp);
                    }
                    else {
                        w = strtoull(tok, &endp, base);
                    }

                    if (endp != tok) {
                        /* There was some number sucessfully scanned! */
                        *valObjPtr = Jim_NewIntObj(interp, w);

                        /* Adjust the number-of-chars scanned so far */
                        scanned += endp - tok;
                    }
                    else {
                        /* Nothing was scanned. We have to determine if this
                         * happened due to e.g. prefix mismatch or input str
                         * exhausted */
                        scanned = *tok ? 0 : -1;
                    }
                    break;
                }
            case 's':
            case '[':{
                    *valObjPtr = JimScanAString(interp, descr->arg, tok);
                    scanned += Jim_Length(*valObjPtr);
                    break;
                }
            case 'e':
            case 'f':
            case 'g':{
                    char *endp;
                    double value = strtod(tok, &endp);

                    if (endp != tok) {
                        /* There was some number sucessfully scanned! */
                        *valObjPtr = Jim_NewDoubleObj(interp, value);
                        /* Adjust the number-of-chars scanned so far */
                        scanned += endp - tok;
                    }
                    else {
                        /* Nothing was scanned. We have to determine if this
                         * happened due to e.g. prefix mismatch or input str
                         * exhausted */
                        scanned = *tok ? 0 : -1;
                    }
                    break;
                }
        }
        /* If a substring was allocated (due to pre-defined width) do not
         * forget to free it */
        if (tmpObj) {
            Jim_FreeNewObj(interp, tmpObj);
        }
    }
    return scanned;
}

/* Jim_ScanString is the workhorse of string scanning. It will scan a given
 * string and returns all converted (and not ignored) values in a list back
 * to the caller. If an error occured, a NULL pointer will be returned */

Jim_Obj *Jim_ScanString(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *fmtObjPtr, int flags)
{
    size_t i, pos;
    int scanned = 1;
    const char *str = Jim_String(strObjPtr);
    int strLen = Jim_Utf8Length(interp, strObjPtr);
    Jim_Obj *resultList = 0;
    Jim_Obj **resultVec = 0;
    int resultc;
    Jim_Obj *emptyStr = 0;
    ScanFmtStringObj *fmtObj;

    /* This should never happen. The format object should already be of the correct type */
    JimPanic((fmtObjPtr->typePtr != &scanFmtStringObjType, "Jim_ScanString() for non-scan format"));

    fmtObj = (ScanFmtStringObj *) fmtObjPtr->internalRep.ptr;
    /* Check if format specification was valid */
    if (fmtObj->error != 0) {
        if (flags & JIM_ERRMSG)
            Jim_SetResultString(interp, fmtObj->error, -1);
        return 0;
    }
    /* Allocate a new "shared" empty string for all unassigned conversions */
    emptyStr = Jim_NewEmptyStringObj(interp);
    Jim_IncrRefCount(emptyStr);
    /* Create a list and fill it with empty strings up to max specified XPG3 */
    resultList = Jim_NewListObj(interp, NULL, 0);
    if (fmtObj->maxPos > 0) {
        for (i = 0; i < fmtObj->maxPos; ++i)
            Jim_ListAppendElement(interp, resultList, emptyStr);
        JimListGetElements(interp, resultList, &resultc, &resultVec);
    }
    /* Now handle every partial format description */
    for (i = 0, pos = 0; i < fmtObj->count; ++i) {
        ScanFmtPartDescr *descr = &(fmtObj->descr[i]);
        Jim_Obj *value = 0;

        /* Only last type may be "literal" w/o conversion - skip it! */
        if (descr->type == 0)
            continue;
        /* As long as any conversion could be done, we will proceed */
        if (scanned > 0)
            scanned = ScanOneEntry(interp, str, pos, strLen, fmtObj, i, &value);
        /* In case our first try results in EOF, we will leave */
        if (scanned == -1 && i == 0)
            goto eof;
        /* Advance next pos-to-be-scanned for the amount scanned already */
        pos += scanned;

        /* value == 0 means no conversion took place so take empty string */
        if (value == 0)
            value = Jim_NewEmptyStringObj(interp);
        /* If value is a non-assignable one, skip it */
        if (descr->pos == -1) {
            Jim_FreeNewObj(interp, value);
        }
        else if (descr->pos == 0)
            /* Otherwise append it to the result list if no XPG3 was given */
            Jim_ListAppendElement(interp, resultList, value);
        else if (resultVec[descr->pos - 1] == emptyStr) {
            /* But due to given XPG3, put the value into the corr. slot */
            Jim_DecrRefCount(interp, resultVec[descr->pos - 1]);
            Jim_IncrRefCount(value);
            resultVec[descr->pos - 1] = value;
        }
        else {
            /* Otherwise, the slot was already used - free obj and ERROR */
            Jim_FreeNewObj(interp, value);
            goto err;
        }
    }
    Jim_DecrRefCount(interp, emptyStr);
    return resultList;
  eof:
    Jim_DecrRefCount(interp, emptyStr);
    Jim_FreeNewObj(interp, resultList);
    return (Jim_Obj *)EOF;
  err:
    Jim_DecrRefCount(interp, emptyStr);
    Jim_FreeNewObj(interp, resultList);
    return 0;
}

/* -----------------------------------------------------------------------------
 * Pseudo Random Number Generation
 * ---------------------------------------------------------------------------*/
/* Initialize the sbox with the numbers from 0 to 255 */
static void JimPrngInit(Jim_Interp *interp)
{
#define PRNG_SEED_SIZE 256
    int i;
    unsigned int *seed;
    time_t t = time(NULL);

    interp->prngState = Jim_Alloc(sizeof(Jim_PrngState));

    seed = Jim_Alloc(PRNG_SEED_SIZE * sizeof(*seed));
    for (i = 0; i < PRNG_SEED_SIZE; i++) {
        seed[i] = (rand() ^ t ^ clock());
    }
    JimPrngSeed(interp, (unsigned char *)seed, PRNG_SEED_SIZE * sizeof(*seed));
    Jim_Free(seed);
}

/* Generates N bytes of random data */
static void JimRandomBytes(Jim_Interp *interp, void *dest, unsigned int len)
{
    Jim_PrngState *prng;
    unsigned char *destByte = (unsigned char *)dest;
    unsigned int si, sj, x;

    /* initialization, only needed the first time */
    if (interp->prngState == NULL)
        JimPrngInit(interp);
    prng = interp->prngState;
    /* generates 'len' bytes of pseudo-random numbers */
    for (x = 0; x < len; x++) {
        prng->i = (prng->i + 1) & 0xff;
        si = prng->sbox[prng->i];
        prng->j = (prng->j + si) & 0xff;
        sj = prng->sbox[prng->j];
        prng->sbox[prng->i] = sj;
        prng->sbox[prng->j] = si;
        *destByte++ = prng->sbox[(si + sj) & 0xff];
    }
}

/* Re-seed the generator with user-provided bytes */
static void JimPrngSeed(Jim_Interp *interp, unsigned char *seed, int seedLen)
{
    int i;
    Jim_PrngState *prng;

    /* initialization, only needed the first time */
    if (interp->prngState == NULL)
        JimPrngInit(interp);
    prng = interp->prngState;

    /* Set the sbox[i] with i */
    for (i = 0; i < 256; i++)
        prng->sbox[i] = i;
    /* Now use the seed to perform a random permutation of the sbox */
    for (i = 0; i < seedLen; i++) {
        unsigned char t;

        t = prng->sbox[i & 0xFF];
        prng->sbox[i & 0xFF] = prng->sbox[seed[i]];
        prng->sbox[seed[i]] = t;
    }
    prng->i = prng->j = 0;

    /* discard at least the first 256 bytes of stream.
     * borrow the seed buffer for this
     */
    for (i = 0; i < 256; i += seedLen) {
        JimRandomBytes(interp, seed, seedLen);
    }
}

/* [incr] */
static int Jim_IncrCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_wide wideValue, increment = 1;
    Jim_Obj *intObjPtr;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?increment?");
        return JIM_ERR;
    }
    if (argc == 3) {
        if (Jim_GetWide(interp, argv[2], &increment) != JIM_OK)
            return JIM_ERR;
    }
    intObjPtr = Jim_GetVariable(interp, argv[1], JIM_UNSHARED);
    if (!intObjPtr) {
        /* Set missing variable to 0 */
        wideValue = 0;
    }
    else if (Jim_GetWide(interp, intObjPtr, &wideValue) != JIM_OK) {
        return JIM_ERR;
    }
    if (!intObjPtr || Jim_IsShared(intObjPtr)) {
        intObjPtr = Jim_NewIntObj(interp, wideValue + increment);
        if (Jim_SetVariable(interp, argv[1], intObjPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, intObjPtr);
            return JIM_ERR;
        }
    }
    else {
        /* Can do it the quick way */
        Jim_InvalidateStringRep(intObjPtr);
        JimWideValue(intObjPtr) = wideValue + increment;

        /* The following step is required in order to invalidate the
         * string repr of "FOO" if the var name is on the form of "FOO(IDX)" */
        if (argv[1]->typePtr != &variableObjType) {
            /* Note that this can't fail since GetVariable already succeeded */
            Jim_SetVariable(interp, argv[1], intObjPtr);
        }
    }
    Jim_SetResult(interp, intObjPtr);
    return JIM_OK;
}


/* -----------------------------------------------------------------------------
 * Eval
 * ---------------------------------------------------------------------------*/
#define JIM_EVAL_SARGV_LEN 8    /* static arguments vector length */
#define JIM_EVAL_SINTV_LEN 8    /* static interpolation vector length */

/* Handle calls to the [unknown] command */
static int JimUnknown(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retcode;

    /* If JimUnknown() is recursively called too many times...
     * done here
     */
    if (interp->unknown_called > 50) {
        return JIM_ERR;
    }

    /* The object interp->unknown just contains
     * the "unknown" string, it is used in order to
     * avoid to lookup the unknown command every time
     * but instead to cache the result. */

    /* If the [unknown] command does not exist ... */
    if (Jim_GetCommand(interp, interp->unknown, JIM_NONE) == NULL)
        return JIM_ERR;

    interp->unknown_called++;
    /* XXX: Are we losing fileNameObj and linenr? */
    retcode = Jim_EvalObjPrefix(interp, interp->unknown, argc, argv);
    interp->unknown_called--;

    return retcode;
}

static int JimInvokeCommand(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    int retcode;
    Jim_Cmd *cmdPtr;

#if 0
    printf("invoke");
    int j;
    for (j = 0; j < objc; j++) {
        printf(" '%s'", Jim_String(objv[j]));
    }
    printf("\n");
#endif

    if (interp->framePtr->tailcallCmd) {
        /* Special tailcall command was pre-resolved */
        cmdPtr = interp->framePtr->tailcallCmd;
        interp->framePtr->tailcallCmd = NULL;
    }
    else {
        cmdPtr = Jim_GetCommand(interp, objv[0], JIM_ERRMSG);
        if (cmdPtr == NULL) {
            return JimUnknown(interp, objc, objv);
        }
        JimIncrCmdRefCount(cmdPtr);
    }

    if (interp->evalDepth == interp->maxEvalDepth) {
        Jim_SetResultString(interp, "Infinite eval recursion", -1);
        retcode = JIM_ERR;
        goto out;
    }
    interp->evalDepth++;

    /* Call it -- Make sure result is an empty object. */
    Jim_SetEmptyResult(interp);
    if (cmdPtr->isproc) {
        retcode = JimCallProcedure(interp, cmdPtr, objc, objv);
    }
    else {
        interp->cmdPrivData = cmdPtr->u.native.privData;
        retcode = cmdPtr->u.native.cmdProc(interp, objc, objv);
    }
    interp->evalDepth--;

out:
    JimDecrCmdRefCount(interp, cmdPtr);

    return retcode;
}

/* Eval the object vector 'objv' composed of 'objc' elements.
 * Every element is used as single argument.
 * Jim_EvalObj() will call this function every time its object
 * argument is of "list" type, with no string representation.
 *
 * This is possible because the string representation of a
 * list object generated by the UpdateStringOfList is made
 * in a way that ensures that every list element is a different
 * command argument. */
int Jim_EvalObjVector(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    int i, retcode;

    /* Incr refcount of arguments. */
    for (i = 0; i < objc; i++)
        Jim_IncrRefCount(objv[i]);

    retcode = JimInvokeCommand(interp, objc, objv);

    /* Decr refcount of arguments and return the retcode */
    for (i = 0; i < objc; i++)
        Jim_DecrRefCount(interp, objv[i]);

    return retcode;
}

/**
 * Invokes 'prefix' as a command with the objv array as arguments.
 */
int Jim_EvalObjPrefix(Jim_Interp *interp, Jim_Obj *prefix, int objc, Jim_Obj *const *objv)
{
    int ret;
    Jim_Obj **nargv = Jim_Alloc((objc + 1) * sizeof(*nargv));

    nargv[0] = prefix;
    memcpy(&nargv[1], &objv[0], sizeof(nargv[0]) * objc);
    ret = Jim_EvalObjVector(interp, objc + 1, nargv);
    Jim_Free(nargv);
    return ret;
}

static void JimAddErrorToStack(Jim_Interp *interp, ScriptObj *script)
{
    if (!interp->errorFlag) {
        /* This is the first error, so save the file/line information and reset the stack */
        interp->errorFlag = 1;
        Jim_IncrRefCount(script->fileNameObj);
        Jim_DecrRefCount(interp, interp->errorFileNameObj);
        interp->errorFileNameObj = script->fileNameObj;
        interp->errorLine = script->linenr;

        JimResetStackTrace(interp);
        /* Always add a level where the error first occurs */
        interp->addStackTrace++;
    }

    /* Now if this is an "interesting" level, add it to the stack trace */
    if (interp->addStackTrace > 0) {
        /* Add the stack info for the current level */

        JimAppendStackTrace(interp, Jim_String(interp->errorProc), script->fileNameObj, script->linenr);

        /* Note: if we didn't have a filename for this level,
         * don't clear the addStackTrace flag
         * so we can pick it up at the next level
         */
        if (Jim_Length(script->fileNameObj)) {
            interp->addStackTrace = 0;
        }

        Jim_DecrRefCount(interp, interp->errorProc);
        interp->errorProc = interp->emptyObj;
        Jim_IncrRefCount(interp->errorProc);
    }
}

static int JimSubstOneToken(Jim_Interp *interp, const ScriptToken *token, Jim_Obj **objPtrPtr)
{
    Jim_Obj *objPtr;

    switch (token->type) {
        case JIM_TT_STR:
        case JIM_TT_ESC:
            objPtr = token->objPtr;
            break;
        case JIM_TT_VAR:
            objPtr = Jim_GetVariable(interp, token->objPtr, JIM_ERRMSG);
            break;
        case JIM_TT_DICTSUGAR:
            objPtr = JimExpandDictSugar(interp, token->objPtr);
            break;
        case JIM_TT_EXPRSUGAR:
            objPtr = JimExpandExprSugar(interp, token->objPtr);
            break;
        case JIM_TT_CMD:
            switch (Jim_EvalObj(interp, token->objPtr)) {
                case JIM_OK:
                case JIM_RETURN:
                    objPtr = interp->result;
                    break;
                case JIM_BREAK:
                    /* Stop substituting */
                    return JIM_BREAK;
                case JIM_CONTINUE:
                    /* just skip this one */
                    return JIM_CONTINUE;
                default:
                    return JIM_ERR;
            }
            break;
        default:
            JimPanic((1,
                "default token type (%d) reached " "in Jim_SubstObj().", token->type));
            objPtr = NULL;
            break;
    }
    if (objPtr) {
        *objPtrPtr = objPtr;
        return JIM_OK;
    }
    return JIM_ERR;
}

/* Interpolate the given tokens into a unique Jim_Obj returned by reference
 * via *objPtrPtr. This function is only called by Jim_EvalObj() and Jim_SubstObj()
 * The returned object has refcount = 0.
 */
static Jim_Obj *JimInterpolateTokens(Jim_Interp *interp, const ScriptToken * token, int tokens, int flags)
{
    int totlen = 0, i;
    Jim_Obj **intv;
    Jim_Obj *sintv[JIM_EVAL_SINTV_LEN];
    Jim_Obj *objPtr;
    char *s;

    if (tokens <= JIM_EVAL_SINTV_LEN)
        intv = sintv;
    else
        intv = Jim_Alloc(sizeof(Jim_Obj *) * tokens);

    /* Compute every token forming the argument
     * in the intv objects vector. */
    for (i = 0; i < tokens; i++) {
        switch (JimSubstOneToken(interp, &token[i], &intv[i])) {
            case JIM_OK:
            case JIM_RETURN:
                break;
            case JIM_BREAK:
                if (flags & JIM_SUBST_FLAG) {
                    /* Stop here */
                    tokens = i;
                    continue;
                }
                /* XXX: Should probably set an error about break outside loop */
                /* fall through to error */
            case JIM_CONTINUE:
                if (flags & JIM_SUBST_FLAG) {
                    intv[i] = NULL;
                    continue;
                }
                /* XXX: Ditto continue outside loop */
                /* fall through to error */
            default:
                while (i--) {
                    Jim_DecrRefCount(interp, intv[i]);
                }
                if (intv != sintv) {
                    Jim_Free(intv);
                }
                return NULL;
        }
        Jim_IncrRefCount(intv[i]);
        Jim_String(intv[i]);
        totlen += intv[i]->length;
    }

    /* Fast path return for a single token */
    if (tokens == 1 && intv[0] && intv == sintv) {
        Jim_DecrRefCount(interp, intv[0]);
        return intv[0];
    }

    /* Concatenate every token in an unique
     * object. */
    objPtr = Jim_NewStringObjNoAlloc(interp, NULL, 0);

    if (tokens == 4 && token[0].type == JIM_TT_ESC && token[1].type == JIM_TT_ESC
        && token[2].type == JIM_TT_VAR) {
        /* May be able to do fast interpolated object -> dictSubst */
        objPtr->typePtr = &interpolatedObjType;
        objPtr->internalRep.dictSubstValue.varNameObjPtr = token[0].objPtr;
        objPtr->internalRep.dictSubstValue.indexObjPtr = intv[2];
        Jim_IncrRefCount(intv[2]);
    }
    else if (tokens && intv[0] && intv[0]->typePtr == &sourceObjType) {
        /* The first interpolated token is source, so preserve the source info */
        JimSetSourceInfo(interp, objPtr, intv[0]->internalRep.sourceValue.fileNameObj, intv[0]->internalRep.sourceValue.lineNumber);
    }


    s = objPtr->bytes = Jim_Alloc(totlen + 1);
    objPtr->length = totlen;
    for (i = 0; i < tokens; i++) {
        if (intv[i]) {
            memcpy(s, intv[i]->bytes, intv[i]->length);
            s += intv[i]->length;
            Jim_DecrRefCount(interp, intv[i]);
        }
    }
    objPtr->bytes[totlen] = '\0';
    /* Free the intv vector if not static. */
    if (intv != sintv) {
        Jim_Free(intv);
    }

    return objPtr;
}


/* listPtr *must* be a list.
 * The contents of the list is evaluated with the first element as the command and
 * the remaining elements as the arguments.
 */
static int JimEvalObjList(Jim_Interp *interp, Jim_Obj *listPtr)
{
    int retcode = JIM_OK;

    JimPanic((Jim_IsList(listPtr) == 0, "JimEvalObjList() invoked on non-list."));

    if (listPtr->internalRep.listValue.len) {
        Jim_IncrRefCount(listPtr);
        retcode = JimInvokeCommand(interp,
            listPtr->internalRep.listValue.len,
            listPtr->internalRep.listValue.ele);
        Jim_DecrRefCount(interp, listPtr);
    }
    return retcode;
}

int Jim_EvalObjList(Jim_Interp *interp, Jim_Obj *listPtr)
{
    SetListFromAny(interp, listPtr);
    return JimEvalObjList(interp, listPtr);
}

int Jim_EvalObj(Jim_Interp *interp, Jim_Obj *scriptObjPtr)
{
    int i;
    ScriptObj *script;
    ScriptToken *token;
    int retcode = JIM_OK;
    Jim_Obj *sargv[JIM_EVAL_SARGV_LEN], **argv = NULL;
    Jim_Obj *prevScriptObj;

    /* If the object is of type "list", with no string rep we can call
     * a specialized version of Jim_EvalObj() */
    if (Jim_IsList(scriptObjPtr) && scriptObjPtr->bytes == NULL) {
        return JimEvalObjList(interp, scriptObjPtr);
    }

    Jim_IncrRefCount(scriptObjPtr);     /* Make sure it's shared. */
    script = JimGetScript(interp, scriptObjPtr);
    if (!JimScriptValid(interp, script)) {
        Jim_DecrRefCount(interp, scriptObjPtr);
        return JIM_ERR;
    }

    /* Reset the interpreter result. This is useful to
     * return the empty result in the case of empty program. */
    Jim_SetEmptyResult(interp);

    token = script->token;

#ifdef JIM_OPTIMIZATION
    /* Check for one of the following common scripts used by for, while
     *
     *   {}
     *   incr a
     */
    if (script->len == 0) {
        Jim_DecrRefCount(interp, scriptObjPtr);
        return JIM_OK;
    }
    if (script->len == 3
        && token[1].objPtr->typePtr == &commandObjType
        && token[1].objPtr->internalRep.cmdValue.cmdPtr->isproc == 0
        && token[1].objPtr->internalRep.cmdValue.cmdPtr->u.native.cmdProc == Jim_IncrCoreCommand
        && token[2].objPtr->typePtr == &variableObjType) {

        Jim_Obj *objPtr = Jim_GetVariable(interp, token[2].objPtr, JIM_NONE);

        if (objPtr && !Jim_IsShared(objPtr) && objPtr->typePtr == &intObjType) {
            JimWideValue(objPtr)++;
            Jim_InvalidateStringRep(objPtr);
            Jim_DecrRefCount(interp, scriptObjPtr);
            Jim_SetResult(interp, objPtr);
            return JIM_OK;
        }
    }
#endif

    /* Now we have to make sure the internal repr will not be
     * freed on shimmering.
     *
     * Think for example to this:
     *
     * set x {llength $x; ... some more code ...}; eval $x
     *
     * In order to preserve the internal rep, we increment the
     * inUse field of the script internal rep structure. */
    script->inUse++;

    /* Stash the current script */
    prevScriptObj = interp->currentScriptObj;
    interp->currentScriptObj = scriptObjPtr;

    interp->errorFlag = 0;
    argv = sargv;

    /* Execute every command sequentially until the end of the script
     * or an error occurs.
     */
    for (i = 0; i < script->len && retcode == JIM_OK; ) {
        int argc;
        int j;

        /* First token of the line is always JIM_TT_LINE */
        argc = token[i].objPtr->internalRep.scriptLineValue.argc;
        script->linenr = token[i].objPtr->internalRep.scriptLineValue.line;

        /* Allocate the arguments vector if required */
        if (argc > JIM_EVAL_SARGV_LEN)
            argv = Jim_Alloc(sizeof(Jim_Obj *) * argc);

        /* Skip the JIM_TT_LINE token */
        i++;

        /* Populate the arguments objects.
         * If an error occurs, retcode will be set and
         * 'j' will be set to the number of args expanded
         */
        for (j = 0; j < argc; j++) {
            long wordtokens = 1;
            int expand = 0;
            Jim_Obj *wordObjPtr = NULL;

            if (token[i].type == JIM_TT_WORD) {
                wordtokens = JimWideValue(token[i++].objPtr);
                if (wordtokens < 0) {
                    expand = 1;
                    wordtokens = -wordtokens;
                }
            }

            if (wordtokens == 1) {
                /* Fast path if the token does not
                 * need interpolation */

                switch (token[i].type) {
                    case JIM_TT_ESC:
                    case JIM_TT_STR:
                        wordObjPtr = token[i].objPtr;
                        break;
                    case JIM_TT_VAR:
                        wordObjPtr = Jim_GetVariable(interp, token[i].objPtr, JIM_ERRMSG);
                        break;
                    case JIM_TT_EXPRSUGAR:
                        wordObjPtr = JimExpandExprSugar(interp, token[i].objPtr);
                        break;
                    case JIM_TT_DICTSUGAR:
                        wordObjPtr = JimExpandDictSugar(interp, token[i].objPtr);
                        break;
                    case JIM_TT_CMD:
                        retcode = Jim_EvalObj(interp, token[i].objPtr);
                        if (retcode == JIM_OK) {
                            wordObjPtr = Jim_GetResult(interp);
                        }
                        break;
                    default:
                        JimPanic((1, "default token type reached " "in Jim_EvalObj()."));
                }
            }
            else {
                /* For interpolation we call a helper
                 * function to do the work for us. */
                wordObjPtr = JimInterpolateTokens(interp, token + i, wordtokens, JIM_NONE);
            }

            if (!wordObjPtr) {
                if (retcode == JIM_OK) {
                    retcode = JIM_ERR;
                }
                break;
            }

            Jim_IncrRefCount(wordObjPtr);
            i += wordtokens;

            if (!expand) {
                argv[j] = wordObjPtr;
            }
            else {
                /* Need to expand wordObjPtr into multiple args from argv[j] ... */
                int len = Jim_ListLength(interp, wordObjPtr);
                int newargc = argc + len - 1;
                int k;

                if (len > 1) {
                    if (argv == sargv) {
                        if (newargc > JIM_EVAL_SARGV_LEN) {
                            argv = Jim_Alloc(sizeof(*argv) * newargc);
                            memcpy(argv, sargv, sizeof(*argv) * j);
                        }
                    }
                    else {
                        /* Need to realloc to make room for (len - 1) more entries */
                        argv = Jim_Realloc(argv, sizeof(*argv) * newargc);
                    }
                }

                /* Now copy in the expanded version */
                for (k = 0; k < len; k++) {
                    argv[j++] = wordObjPtr->internalRep.listValue.ele[k];
                    Jim_IncrRefCount(wordObjPtr->internalRep.listValue.ele[k]);
                }

                /* The original object reference is no longer needed,
                 * after the expansion it is no longer present on
                 * the argument vector, but the single elements are
                 * in its place. */
                Jim_DecrRefCount(interp, wordObjPtr);

                /* And update the indexes */
                j--;
                argc += len - 1;
            }
        }

        if (retcode == JIM_OK && argc) {
            /* Invoke the command */
            retcode = JimInvokeCommand(interp, argc, argv);
            /* Check for a signal after each command */
            if (Jim_CheckSignal(interp)) {
                retcode = JIM_SIGNAL;
            }
        }

        /* Finished with the command, so decrement ref counts of each argument */
        while (j-- > 0) {
            Jim_DecrRefCount(interp, argv[j]);
        }

        if (argv != sargv) {
            Jim_Free(argv);
            argv = sargv;
        }
    }

    /* Possibly add to the error stack trace */
    if (retcode == JIM_ERR) {
        JimAddErrorToStack(interp, script);
    }
    /* Propagate the addStackTrace value through 'return -code error' */
    else if (retcode != JIM_RETURN || interp->returnCode != JIM_ERR) {
        /* No need to add stack trace */
        interp->addStackTrace = 0;
    }

    /* Restore the current script */
    interp->currentScriptObj = prevScriptObj;

    /* Note that we don't have to decrement inUse, because the
     * following code transfers our use of the reference again to
     * the script object. */
    Jim_FreeIntRep(interp, scriptObjPtr);
    scriptObjPtr->typePtr = &scriptObjType;
    Jim_SetIntRepPtr(scriptObjPtr, script);
    Jim_DecrRefCount(interp, scriptObjPtr);

    return retcode;
}

static int JimSetProcArg(Jim_Interp *interp, Jim_Obj *argNameObj, Jim_Obj *argValObj)
{
    int retcode;
    /* If argObjPtr begins with '&', do an automatic upvar */
    const char *varname = Jim_String(argNameObj);
    if (*varname == '&') {
        /* First check that the target variable exists */
        Jim_Obj *objPtr;
        Jim_CallFrame *savedCallFrame = interp->framePtr;

        interp->framePtr = interp->framePtr->parent;
        objPtr = Jim_GetVariable(interp, argValObj, JIM_ERRMSG);
        interp->framePtr = savedCallFrame;
        if (!objPtr) {
            return JIM_ERR;
        }

        /* It exists, so perform the binding. */
        objPtr = Jim_NewStringObj(interp, varname + 1, -1);
        Jim_IncrRefCount(objPtr);
        retcode = Jim_SetVariableLink(interp, objPtr, argValObj, interp->framePtr->parent);
        Jim_DecrRefCount(interp, objPtr);
    }
    else {
        retcode = Jim_SetVariable(interp, argNameObj, argValObj);
    }
    return retcode;
}

/**
 * Sets the interp result to be an error message indicating the required proc args.
 */
static void JimSetProcWrongArgs(Jim_Interp *interp, Jim_Obj *procNameObj, Jim_Cmd *cmd)
{
    /* Create a nice error message, consistent with Tcl 8.5 */
    Jim_Obj *argmsg = Jim_NewStringObj(interp, "", 0);
    int i;

    for (i = 0; i < cmd->u.proc.argListLen; i++) {
        Jim_AppendString(interp, argmsg, " ", 1);

        if (i == cmd->u.proc.argsPos) {
            if (cmd->u.proc.arglist[i].defaultObjPtr) {
                /* Renamed args */
                Jim_AppendString(interp, argmsg, "?", 1);
                Jim_AppendObj(interp, argmsg, cmd->u.proc.arglist[i].defaultObjPtr);
                Jim_AppendString(interp, argmsg, " ...?", -1);
            }
            else {
                /* We have plain args */
                Jim_AppendString(interp, argmsg, "?arg...?", -1);
            }
        }
        else {
            if (cmd->u.proc.arglist[i].defaultObjPtr) {
                Jim_AppendString(interp, argmsg, "?", 1);
                Jim_AppendObj(interp, argmsg, cmd->u.proc.arglist[i].nameObjPtr);
                Jim_AppendString(interp, argmsg, "?", 1);
            }
            else {
                const char *arg = Jim_String(cmd->u.proc.arglist[i].nameObjPtr);
                if (*arg == '&') {
                    arg++;
                }
                Jim_AppendString(interp, argmsg, arg, -1);
            }
        }
    }
    Jim_SetResultFormatted(interp, "wrong # args: should be \"%#s%#s\"", procNameObj, argmsg);
    Jim_FreeNewObj(interp, argmsg);
}

#ifdef jim_ext_namespace
/*
 * [namespace eval]
 */
int Jim_EvalNamespace(Jim_Interp *interp, Jim_Obj *scriptObj, Jim_Obj *nsObj)
{
    Jim_CallFrame *callFramePtr;
    int retcode;

    /* Create a new callframe */
    callFramePtr = JimCreateCallFrame(interp, interp->framePtr, nsObj);
    callFramePtr->argv = &interp->emptyObj;
    callFramePtr->argc = 0;
    callFramePtr->procArgsObjPtr = NULL;
    callFramePtr->procBodyObjPtr = scriptObj;
    callFramePtr->staticVars = NULL;
    callFramePtr->fileNameObj = interp->emptyObj;
    callFramePtr->line = 0;
    Jim_IncrRefCount(scriptObj);
    interp->framePtr = callFramePtr;

    /* Check if there are too nested calls */
    if (interp->framePtr->level == interp->maxCallFrameDepth) {
        Jim_SetResultString(interp, "Too many nested calls. Infinite recursion?", -1);
        retcode = JIM_ERR;
    }
    else {
        /* Eval the body */
        retcode = Jim_EvalObj(interp, scriptObj);
    }

    /* Destroy the callframe */
    interp->framePtr = interp->framePtr->parent;
    JimFreeCallFrame(interp, callFramePtr, JIM_FCF_REUSE);

    return retcode;
}
#endif

/* Call a procedure implemented in Tcl.
 * It's possible to speed-up a lot this function, currently
 * the callframes are not cached, but allocated and
 * destroied every time. What is expecially costly is
 * to create/destroy the local vars hash table every time.
 *
 * This can be fixed just implementing callframes caching
 * in JimCreateCallFrame() and JimFreeCallFrame(). */
static int JimCallProcedure(Jim_Interp *interp, Jim_Cmd *cmd, int argc, Jim_Obj *const *argv)
{
    Jim_CallFrame *callFramePtr;
    int i, d, retcode, optargs;
    ScriptObj *script;

    /* Check arity */
    if (argc - 1 < cmd->u.proc.reqArity ||
        (cmd->u.proc.argsPos < 0 && argc - 1 > cmd->u.proc.reqArity + cmd->u.proc.optArity)) {
        JimSetProcWrongArgs(interp, argv[0], cmd);
        return JIM_ERR;
    }

    if (Jim_Length(cmd->u.proc.bodyObjPtr) == 0) {
        /* Optimise for procedure with no body - useful for optional debugging */
        return JIM_OK;
    }

    /* Check if there are too nested calls */
    if (interp->framePtr->level == interp->maxCallFrameDepth) {
        Jim_SetResultString(interp, "Too many nested calls. Infinite recursion?", -1);
        return JIM_ERR;
    }

    /* Create a new callframe */
    callFramePtr = JimCreateCallFrame(interp, interp->framePtr, cmd->u.proc.nsObj);
    callFramePtr->argv = argv;
    callFramePtr->argc = argc;
    callFramePtr->procArgsObjPtr = cmd->u.proc.argListObjPtr;
    callFramePtr->procBodyObjPtr = cmd->u.proc.bodyObjPtr;
    callFramePtr->staticVars = cmd->u.proc.staticVars;

    /* Remember where we were called from. */
    script = JimGetScript(interp, interp->currentScriptObj);
    callFramePtr->fileNameObj = script->fileNameObj;
    callFramePtr->line = script->linenr;

    Jim_IncrRefCount(cmd->u.proc.argListObjPtr);
    Jim_IncrRefCount(cmd->u.proc.bodyObjPtr);
    interp->framePtr = callFramePtr;

    /* How many optional args are available */
    optargs = (argc - 1 - cmd->u.proc.reqArity);

    /* Step 'i' along the actual args, and step 'd' along the formal args */
    i = 1;
    for (d = 0; d < cmd->u.proc.argListLen; d++) {
        Jim_Obj *nameObjPtr = cmd->u.proc.arglist[d].nameObjPtr;
        if (d == cmd->u.proc.argsPos) {
            /* assign $args */
            Jim_Obj *listObjPtr;
            int argsLen = 0;
            if (cmd->u.proc.reqArity + cmd->u.proc.optArity < argc - 1) {
                argsLen = argc - 1 - (cmd->u.proc.reqArity + cmd->u.proc.optArity);
            }
            listObjPtr = Jim_NewListObj(interp, &argv[i], argsLen);

            /* It is possible to rename args. */
            if (cmd->u.proc.arglist[d].defaultObjPtr) {
                nameObjPtr =cmd->u.proc.arglist[d].defaultObjPtr;
            }
            retcode = Jim_SetVariable(interp, nameObjPtr, listObjPtr);
            if (retcode != JIM_OK) {
                goto badargset;
            }

            i += argsLen;
            continue;
        }

        /* Optional or required? */
        if (cmd->u.proc.arglist[d].defaultObjPtr == NULL || optargs-- > 0) {
            retcode = JimSetProcArg(interp, nameObjPtr, argv[i++]);
        }
        else {
            /* Ran out, so use the default */
            retcode = Jim_SetVariable(interp, nameObjPtr, cmd->u.proc.arglist[d].defaultObjPtr);
        }
        if (retcode != JIM_OK) {
            goto badargset;
        }
    }

    /* Eval the body */
    retcode = Jim_EvalObj(interp, cmd->u.proc.bodyObjPtr);

badargset:

    /* Free the callframe */
    interp->framePtr = interp->framePtr->parent;
    JimFreeCallFrame(interp, callFramePtr, JIM_FCF_REUSE);

    /* Now chain any tailcalls in the parent frame */
    if (interp->framePtr->tailcallObj) {
        do {
            Jim_Obj *tailcallObj = interp->framePtr->tailcallObj;

            interp->framePtr->tailcallObj = NULL;

            if (retcode == JIM_EVAL) {
                retcode = Jim_EvalObjList(interp, tailcallObj);
                if (retcode == JIM_RETURN) {
                    /* If the result of the tailcall is 'return', push
                     * it up to the caller
                     */
                    interp->returnLevel++;
                }
            }
            Jim_DecrRefCount(interp, tailcallObj);
        } while (interp->framePtr->tailcallObj);

        /* If the tailcall chain finished early, may need to manually discard the command */
        if (interp->framePtr->tailcallCmd) {
            JimDecrCmdRefCount(interp, interp->framePtr->tailcallCmd);
            interp->framePtr->tailcallCmd = NULL;
        }
    }

    /* Handle the JIM_RETURN return code */
    if (retcode == JIM_RETURN) {
        if (--interp->returnLevel <= 0) {
            retcode = interp->returnCode;
            interp->returnCode = JIM_OK;
            interp->returnLevel = 0;
        }
    }
    else if (retcode == JIM_ERR) {
        interp->addStackTrace++;
        Jim_DecrRefCount(interp, interp->errorProc);
        interp->errorProc = argv[0];
        Jim_IncrRefCount(interp->errorProc);
    }

    return retcode;
}

int Jim_EvalSource(Jim_Interp *interp, const char *filename, int lineno, const char *script)
{
    int retval;
    Jim_Obj *scriptObjPtr;

    scriptObjPtr = Jim_NewStringObj(interp, script, -1);
    Jim_IncrRefCount(scriptObjPtr);

    if (filename) {
        Jim_Obj *prevScriptObj;

        JimSetSourceInfo(interp, scriptObjPtr, Jim_NewStringObj(interp, filename, -1), lineno);

        prevScriptObj = interp->currentScriptObj;
        interp->currentScriptObj = scriptObjPtr;

        retval = Jim_EvalObj(interp, scriptObjPtr);

        interp->currentScriptObj = prevScriptObj;
    }
    else {
        retval = Jim_EvalObj(interp, scriptObjPtr);
    }
    Jim_DecrRefCount(interp, scriptObjPtr);
    return retval;
}

int Jim_Eval(Jim_Interp *interp, const char *script)
{
    return Jim_EvalObj(interp, Jim_NewStringObj(interp, script, -1));
}

/* Execute script in the scope of the global level */
int Jim_EvalGlobal(Jim_Interp *interp, const char *script)
{
    int retval;
    Jim_CallFrame *savedFramePtr = interp->framePtr;

    interp->framePtr = interp->topFramePtr;
    retval = Jim_Eval(interp, script);
    interp->framePtr = savedFramePtr;

    return retval;
}

int Jim_EvalFileGlobal(Jim_Interp *interp, const char *filename)
{
    int retval;
    Jim_CallFrame *savedFramePtr = interp->framePtr;

    interp->framePtr = interp->topFramePtr;
    retval = Jim_EvalFile(interp, filename);
    interp->framePtr = savedFramePtr;

    return retval;
}

#include <sys/stat.h>

int Jim_EvalFile(Jim_Interp *interp, const char *filename)
{
    FILE *fp;
    char *buf;
    Jim_Obj *scriptObjPtr;
    Jim_Obj *prevScriptObj;
    struct stat sb;
    int retcode;
    int readlen;

    if (stat(filename, &sb) != 0 || (fp = fopen(filename, "rt")) == NULL) {
        Jim_SetResultFormatted(interp, "couldn't read file \"%s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    if (sb.st_size == 0) {
        fclose(fp);
        return JIM_OK;
    }

    buf = Jim_Alloc(sb.st_size + 1);
    readlen = fread(buf, 1, sb.st_size, fp);
    if (ferror(fp)) {
        fclose(fp);
        Jim_Free(buf);
        Jim_SetResultFormatted(interp, "failed to load file \"%s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    fclose(fp);
    buf[readlen] = 0;

    scriptObjPtr = Jim_NewStringObjNoAlloc(interp, buf, readlen);
    JimSetSourceInfo(interp, scriptObjPtr, Jim_NewStringObj(interp, filename, -1), 1);
    Jim_IncrRefCount(scriptObjPtr);

    prevScriptObj = interp->currentScriptObj;
    interp->currentScriptObj = scriptObjPtr;

    retcode = Jim_EvalObj(interp, scriptObjPtr);

    /* Handle the JIM_RETURN return code */
    if (retcode == JIM_RETURN) {
        if (--interp->returnLevel <= 0) {
            retcode = interp->returnCode;
            interp->returnCode = JIM_OK;
            interp->returnLevel = 0;
        }
    }
    if (retcode == JIM_ERR) {
        /* EvalFile changes context, so add a stack frame here */
        interp->addStackTrace++;
    }

    interp->currentScriptObj = prevScriptObj;

    Jim_DecrRefCount(interp, scriptObjPtr);

    return retcode;
}

/* -----------------------------------------------------------------------------
 * Subst
 * ---------------------------------------------------------------------------*/
static void JimParseSubst(struct JimParserCtx *pc, int flags)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;

    if (pc->len == 0) {
        pc->tend = pc->p;
        pc->tt = JIM_TT_EOL;
        pc->eof = 1;
        return;
    }
    if (*pc->p == '[' && !(flags & JIM_SUBST_NOCMD)) {
        JimParseCmd(pc);
        return;
    }
    if (*pc->p == '$' && !(flags & JIM_SUBST_NOVAR)) {
        if (JimParseVar(pc) == JIM_OK) {
            return;
        }
        /* Not a var, so treat as a string */
        pc->tstart = pc->p;
        flags |= JIM_SUBST_NOVAR;
    }
    while (pc->len) {
        if (*pc->p == '$' && !(flags & JIM_SUBST_NOVAR)) {
            break;
        }
        if (*pc->p == '[' && !(flags & JIM_SUBST_NOCMD)) {
            break;
        }
        if (*pc->p == '\\' && pc->len > 1) {
            pc->p++;
            pc->len--;
        }
        pc->p++;
        pc->len--;
    }
    pc->tend = pc->p - 1;
    pc->tt = (flags & JIM_SUBST_NOESC) ? JIM_TT_STR : JIM_TT_ESC;
}

/* The subst object type reuses most of the data structures and functions
 * of the script object. Script's data structures are a bit more complex
 * for what is needed for [subst]itution tasks, but the reuse helps to
 * deal with a single data structure at the cost of some more memory
 * usage for substitutions. */

/* This method takes the string representation of an object
 * as a Tcl string where to perform [subst]itution, and generates
 * the pre-parsed internal representation. */
static int SetSubstFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr, int flags)
{
    int scriptTextLen;
    const char *scriptText = Jim_GetString(objPtr, &scriptTextLen);
    struct JimParserCtx parser;
    struct ScriptObj *script = Jim_Alloc(sizeof(*script));
    ParseTokenList tokenlist;

    /* Initially parse the subst into tokens (in tokenlist) */
    ScriptTokenListInit(&tokenlist);

    JimParserInit(&parser, scriptText, scriptTextLen, 1);
    while (1) {
        JimParseSubst(&parser, flags);
        if (parser.eof) {
            /* Note that subst doesn't need the EOL token */
            break;
        }
        ScriptAddToken(&tokenlist, parser.tstart, parser.tend - parser.tstart + 1, parser.tt,
            parser.tline);
    }

    /* Create the "real" subst/script tokens from the initial token list */
    script->inUse = 1;
    script->substFlags = flags;
    script->fileNameObj = interp->emptyObj;
    Jim_IncrRefCount(script->fileNameObj);
    SubstObjAddTokens(interp, script, &tokenlist);

    /* No longer need the token list */
    ScriptTokenListFree(&tokenlist);

#ifdef DEBUG_SHOW_SUBST
    {
        int i;

        printf("==== Subst ====\n");
        for (i = 0; i < script->len; i++) {
            printf("[%2d] %s '%s'\n", i, jim_tt_name(script->token[i].type),
                Jim_String(script->token[i].objPtr));
        }
    }
#endif

    /* Free the old internal rep and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    Jim_SetIntRepPtr(objPtr, script);
    objPtr->typePtr = &scriptObjType;
    return JIM_OK;
}

static ScriptObj *Jim_GetSubst(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    if (objPtr->typePtr != &scriptObjType || ((ScriptObj *)Jim_GetIntRepPtr(objPtr))->substFlags != flags)
        SetSubstFromAny(interp, objPtr, flags);
    return (ScriptObj *) Jim_GetIntRepPtr(objPtr);
}

/* Performs commands,variables,blackslashes substitution,
 * storing the result object (with refcount 0) into
 * resObjPtrPtr. */
int Jim_SubstObj(Jim_Interp *interp, Jim_Obj *substObjPtr, Jim_Obj **resObjPtrPtr, int flags)
{
    ScriptObj *script = Jim_GetSubst(interp, substObjPtr, flags);

    Jim_IncrRefCount(substObjPtr);      /* Make sure it's shared. */
    /* In order to preserve the internal rep, we increment the
     * inUse field of the script internal rep structure. */
    script->inUse++;

    *resObjPtrPtr = JimInterpolateTokens(interp, script->token, script->len, flags);

    script->inUse--;
    Jim_DecrRefCount(interp, substObjPtr);
    if (*resObjPtrPtr == NULL) {
        return JIM_ERR;
    }
    return JIM_OK;
}

/* -----------------------------------------------------------------------------
 * Core commands utility functions
 * ---------------------------------------------------------------------------*/
void Jim_WrongNumArgs(Jim_Interp *interp, int argc, Jim_Obj *const *argv, const char *msg)
{
    Jim_Obj *objPtr;
    Jim_Obj *listObjPtr = Jim_NewListObj(interp, argv, argc);

    if (*msg) {
        Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, msg, -1));
    }
    Jim_IncrRefCount(listObjPtr);
    objPtr = Jim_ListJoin(interp, listObjPtr, " ", 1);
    Jim_DecrRefCount(interp, listObjPtr);

    Jim_IncrRefCount(objPtr);
    Jim_SetResultFormatted(interp, "wrong # args: should be \"%#s\"", objPtr);
    Jim_DecrRefCount(interp, objPtr);
}

/**
 * May add the key and/or value to the list.
 */
typedef void JimHashtableIteratorCallbackType(Jim_Interp *interp, Jim_Obj *listObjPtr,
    Jim_HashEntry *he, int type);

#define JimTrivialMatch(pattern)    (strpbrk((pattern), "*[?\\") == NULL)

/**
 * For each key of the hash table 'ht' (with string keys) which matches the glob pattern (all if NULL),
 * invoke the callback to add entries to a list.
 * Returns the list.
 */
static Jim_Obj *JimHashtablePatternMatch(Jim_Interp *interp, Jim_HashTable *ht, Jim_Obj *patternObjPtr,
    JimHashtableIteratorCallbackType *callback, int type)
{
    Jim_HashEntry *he;
    Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);

    /* Check for the non-pattern case. We can do this much more efficiently. */
    if (patternObjPtr && JimTrivialMatch(Jim_String(patternObjPtr))) {
        he = Jim_FindHashEntry(ht, Jim_String(patternObjPtr));
        if (he) {
            callback(interp, listObjPtr, he, type);
        }
    }
    else {
        Jim_HashTableIterator htiter;
        JimInitHashTableIterator(ht, &htiter);
        while ((he = Jim_NextHashEntry(&htiter)) != NULL) {
            if (patternObjPtr == NULL || JimGlobMatch(Jim_String(patternObjPtr), he->key, 0)) {
                callback(interp, listObjPtr, he, type);
            }
        }
    }
    return listObjPtr;
}

/* Keep these in order */
#define JIM_CMDLIST_COMMANDS 0
#define JIM_CMDLIST_PROCS 1
#define JIM_CMDLIST_CHANNELS 2

/**
 * Adds matching command names (procs, channels) to the list.
 */
static void JimCommandMatch(Jim_Interp *interp, Jim_Obj *listObjPtr,
    Jim_HashEntry *he, int type)
{
    Jim_Cmd *cmdPtr = Jim_GetHashEntryVal(he);
    Jim_Obj *objPtr;

    if (type == JIM_CMDLIST_PROCS && !cmdPtr->isproc) {
        /* not a proc */
        return;
    }

    objPtr = Jim_NewStringObj(interp, he->key, -1);
    Jim_IncrRefCount(objPtr);

    if (type != JIM_CMDLIST_CHANNELS || Jim_AioFilehandle(interp, objPtr)) {
        Jim_ListAppendElement(interp, listObjPtr, objPtr);
    }
    Jim_DecrRefCount(interp, objPtr);
}

/* type is JIM_CMDLIST_xxx */
static Jim_Obj *JimCommandsList(Jim_Interp *interp, Jim_Obj *patternObjPtr, int type)
{
    return JimHashtablePatternMatch(interp, &interp->commands, patternObjPtr, JimCommandMatch, type);
}

/* Keep these in order */
#define JIM_VARLIST_GLOBALS 0
#define JIM_VARLIST_LOCALS 1
#define JIM_VARLIST_VARS 2

#define JIM_VARLIST_VALUES 0x1000

/**
 * Adds matching variable names to the list.
 */
static void JimVariablesMatch(Jim_Interp *interp, Jim_Obj *listObjPtr,
    Jim_HashEntry *he, int type)
{
    Jim_Var *varPtr = Jim_GetHashEntryVal(he);

    if (type != JIM_VARLIST_LOCALS || varPtr->linkFramePtr == NULL) {
        Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, he->key, -1));
        if (type & JIM_VARLIST_VALUES) {
            Jim_ListAppendElement(interp, listObjPtr, varPtr->objPtr);
        }
    }
}

/* mode is JIM_VARLIST_xxx */
static Jim_Obj *JimVariablesList(Jim_Interp *interp, Jim_Obj *patternObjPtr, int mode)
{
    if (mode == JIM_VARLIST_LOCALS && interp->framePtr == interp->topFramePtr) {
        /* For [info locals], if we are at top level an emtpy list
         * is returned. I don't agree, but we aim at compatibility (SS) */
        return interp->emptyObj;
    }
    else {
        Jim_CallFrame *framePtr = (mode == JIM_VARLIST_GLOBALS) ? interp->topFramePtr : interp->framePtr;
        return JimHashtablePatternMatch(interp, &framePtr->vars, patternObjPtr, JimVariablesMatch, mode);
    }
}

static int JimInfoLevel(Jim_Interp *interp, Jim_Obj *levelObjPtr,
    Jim_Obj **objPtrPtr, int info_level_cmd)
{
    Jim_CallFrame *targetCallFrame;

    targetCallFrame = JimGetCallFrameByInteger(interp, levelObjPtr);
    if (targetCallFrame == NULL) {
        return JIM_ERR;
    }
    /* No proc call at toplevel callframe */
    if (targetCallFrame == interp->topFramePtr) {
        Jim_SetResultFormatted(interp, "bad level \"%#s\"", levelObjPtr);
        return JIM_ERR;
    }
    if (info_level_cmd) {
        *objPtrPtr = Jim_NewListObj(interp, targetCallFrame->argv, targetCallFrame->argc);
    }
    else {
        Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);

        Jim_ListAppendElement(interp, listObj, targetCallFrame->argv[0]);
        Jim_ListAppendElement(interp, listObj, targetCallFrame->fileNameObj);
        Jim_ListAppendElement(interp, listObj, Jim_NewIntObj(interp, targetCallFrame->line));
        *objPtrPtr = listObj;
    }
    return JIM_OK;
}

/* -----------------------------------------------------------------------------
 * Core commands
 * ---------------------------------------------------------------------------*/

/* fake [puts] -- not the real puts, just for debugging. */
static int Jim_PutsCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "?-nonewline? string");
        return JIM_ERR;
    }
    if (argc == 3) {
        if (!Jim_CompareStringImmediate(interp, argv[1], "-nonewline")) {
            Jim_SetResultString(interp, "The second argument must " "be -nonewline", -1);
            return JIM_ERR;
        }
        else {
            fputs(Jim_String(argv[2]), stdout);
        }
    }
    else {
        puts(Jim_String(argv[1]));
    }
    return JIM_OK;
}

/* Helper for [+] and [*] */
static int JimAddMulHelper(Jim_Interp *interp, int argc, Jim_Obj *const *argv, int op)
{
    jim_wide wideValue, res;
    double doubleValue, doubleRes;
    int i;

    res = (op == JIM_EXPROP_ADD) ? 0 : 1;

    for (i = 1; i < argc; i++) {
        if (Jim_GetWide(interp, argv[i], &wideValue) != JIM_OK)
            goto trydouble;
        if (op == JIM_EXPROP_ADD)
            res += wideValue;
        else
            res *= wideValue;
    }
    Jim_SetResultInt(interp, res);
    return JIM_OK;
  trydouble:
    doubleRes = (double)res;
    for (; i < argc; i++) {
        if (Jim_GetDouble(interp, argv[i], &doubleValue) != JIM_OK)
            return JIM_ERR;
        if (op == JIM_EXPROP_ADD)
            doubleRes += doubleValue;
        else
            doubleRes *= doubleValue;
    }
    Jim_SetResult(interp, Jim_NewDoubleObj(interp, doubleRes));
    return JIM_OK;
}

/* Helper for [-] and [/] */
static int JimSubDivHelper(Jim_Interp *interp, int argc, Jim_Obj *const *argv, int op)
{
    jim_wide wideValue, res = 0;
    double doubleValue, doubleRes = 0;
    int i = 2;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "number ?number ... number?");
        return JIM_ERR;
    }
    else if (argc == 2) {
        /* The arity = 2 case is different. For [- x] returns -x,
         * while [/ x] returns 1/x. */
        if (Jim_GetWide(interp, argv[1], &wideValue) != JIM_OK) {
            if (Jim_GetDouble(interp, argv[1], &doubleValue) != JIM_OK) {
                return JIM_ERR;
            }
            else {
                if (op == JIM_EXPROP_SUB)
                    doubleRes = -doubleValue;
                else
                    doubleRes = 1.0 / doubleValue;
                Jim_SetResult(interp, Jim_NewDoubleObj(interp, doubleRes));
                return JIM_OK;
            }
        }
        if (op == JIM_EXPROP_SUB) {
            res = -wideValue;
            Jim_SetResultInt(interp, res);
        }
        else {
            doubleRes = 1.0 / wideValue;
            Jim_SetResult(interp, Jim_NewDoubleObj(interp, doubleRes));
        }
        return JIM_OK;
    }
    else {
        if (Jim_GetWide(interp, argv[1], &res) != JIM_OK) {
            if (Jim_GetDouble(interp, argv[1], &doubleRes)
                != JIM_OK) {
                return JIM_ERR;
            }
            else {
                goto trydouble;
            }
        }
    }
    for (i = 2; i < argc; i++) {
        if (Jim_GetWide(interp, argv[i], &wideValue) != JIM_OK) {
            doubleRes = (double)res;
            goto trydouble;
        }
        if (op == JIM_EXPROP_SUB)
            res -= wideValue;
        else
            res /= wideValue;
    }
    Jim_SetResultInt(interp, res);
    return JIM_OK;
  trydouble:
    for (; i < argc; i++) {
        if (Jim_GetDouble(interp, argv[i], &doubleValue) != JIM_OK)
            return JIM_ERR;
        if (op == JIM_EXPROP_SUB)
            doubleRes -= doubleValue;
        else
            doubleRes /= doubleValue;
    }
    Jim_SetResult(interp, Jim_NewDoubleObj(interp, doubleRes));
    return JIM_OK;
}


/* [+] */
static int Jim_AddCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimAddMulHelper(interp, argc, argv, JIM_EXPROP_ADD);
}

/* [*] */
static int Jim_MulCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimAddMulHelper(interp, argc, argv, JIM_EXPROP_MUL);
}

/* [-] */
static int Jim_SubCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimSubDivHelper(interp, argc, argv, JIM_EXPROP_SUB);
}

/* [/] */
static int Jim_DivCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimSubDivHelper(interp, argc, argv, JIM_EXPROP_DIV);
}

/* [set] */
static int Jim_SetCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?newValue?");
        return JIM_ERR;
    }
    if (argc == 2) {
        Jim_Obj *objPtr;

        objPtr = Jim_GetVariable(interp, argv[1], JIM_ERRMSG);
        if (!objPtr)
            return JIM_ERR;
        Jim_SetResult(interp, objPtr);
        return JIM_OK;
    }
    /* argc == 3 case. */
    if (Jim_SetVariable(interp, argv[1], argv[2]) != JIM_OK)
        return JIM_ERR;
    Jim_SetResult(interp, argv[2]);
    return JIM_OK;
}

/* [unset]
 *
 * unset ?-nocomplain? ?--? ?varName ...?
 */
static int Jim_UnsetCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i = 1;
    int complain = 1;

    while (i < argc) {
        if (Jim_CompareStringImmediate(interp, argv[i], "--")) {
            i++;
            break;
        }
        if (Jim_CompareStringImmediate(interp, argv[i], "-nocomplain")) {
            complain = 0;
            i++;
            continue;
        }
        break;
    }

    while (i < argc) {
        if (Jim_UnsetVariable(interp, argv[i], complain ? JIM_ERRMSG : JIM_NONE) != JIM_OK
            && complain) {
            return JIM_ERR;
        }
        i++;
    }
    return JIM_OK;
}

/* [while] */
static int Jim_WhileCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "condition body");
        return JIM_ERR;
    }

    /* The general purpose implementation of while starts here */
    while (1) {
        int boolean, retval;

        if ((retval = Jim_GetBoolFromExpr(interp, argv[1], &boolean)) != JIM_OK)
            return retval;
        if (!boolean)
            break;

        if ((retval = Jim_EvalObj(interp, argv[2])) != JIM_OK) {
            switch (retval) {
                case JIM_BREAK:
                    goto out;
                    break;
                case JIM_CONTINUE:
                    continue;
                    break;
                default:
                    return retval;
            }
        }
    }
  out:
    Jim_SetEmptyResult(interp);
    return JIM_OK;
}

/* [for] */
static int Jim_ForCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retval;
    int boolean = 1;
    Jim_Obj *varNamePtr = NULL;
    Jim_Obj *stopVarNamePtr = NULL;

    if (argc != 5) {
        Jim_WrongNumArgs(interp, 1, argv, "start test next body");
        return JIM_ERR;
    }

    /* Do the initialisation */
    if ((retval = Jim_EvalObj(interp, argv[1])) != JIM_OK) {
        return retval;
    }

    /* And do the first test now. Better for optimisation
     * if we can do next/test at the bottom of the loop
     */
    retval = Jim_GetBoolFromExpr(interp, argv[2], &boolean);

    /* Ready to do the body as follows:
     * while (1) {
     *     body // check retcode
     *     next // check retcode
     *     test // check retcode/test bool
     * }
     */

#ifdef JIM_OPTIMIZATION
    /* Check if the for is on the form:
     *      for ... {$i < CONST} {incr i}
     *      for ... {$i < $j} {incr i}
     */
    if (retval == JIM_OK && boolean) {
        ScriptObj *incrScript;
        ExprByteCode *expr;
        jim_wide stop, currentVal;
        Jim_Obj *objPtr;
        int cmpOffset;

        /* Do it only if there aren't shared arguments */
        expr = JimGetExpression(interp, argv[2]);
        incrScript = JimGetScript(interp, argv[3]);

        /* Ensure proper lengths to start */
        if (incrScript == NULL || incrScript->len != 3 || !expr || expr->len != 3) {
            goto evalstart;
        }
        /* Ensure proper token types. */
        if (incrScript->token[1].type != JIM_TT_ESC ||
            expr->token[0].type != JIM_TT_VAR ||
            (expr->token[1].type != JIM_TT_EXPR_INT && expr->token[1].type != JIM_TT_VAR)) {
            goto evalstart;
        }

        if (expr->token[2].type == JIM_EXPROP_LT) {
            cmpOffset = 0;
        }
        else if (expr->token[2].type == JIM_EXPROP_LTE) {
            cmpOffset = 1;
        }
        else {
            goto evalstart;
        }

        /* Update command must be incr */
        if (!Jim_CompareStringImmediate(interp, incrScript->token[1].objPtr, "incr")) {
            goto evalstart;
        }

        /* incr, expression must be about the same variable */
        if (!Jim_StringEqObj(incrScript->token[2].objPtr, expr->token[0].objPtr)) {
            goto evalstart;
        }

        /* Get the stop condition (must be a variable or integer) */
        if (expr->token[1].type == JIM_TT_EXPR_INT) {
            if (Jim_GetWide(interp, expr->token[1].objPtr, &stop) == JIM_ERR) {
                goto evalstart;
            }
        }
        else {
            stopVarNamePtr = expr->token[1].objPtr;
            Jim_IncrRefCount(stopVarNamePtr);
            /* Keep the compiler happy */
            stop = 0;
        }

        /* Initialization */
        varNamePtr = expr->token[0].objPtr;
        Jim_IncrRefCount(varNamePtr);

        objPtr = Jim_GetVariable(interp, varNamePtr, JIM_NONE);
        if (objPtr == NULL || Jim_GetWide(interp, objPtr, &currentVal) != JIM_OK) {
            goto testcond;
        }

        /* --- OPTIMIZED FOR --- */
        while (retval == JIM_OK) {
            /* === Check condition === */
            /* Note that currentVal is already set here */

            /* Immediate or Variable? get the 'stop' value if the latter. */
            if (stopVarNamePtr) {
                objPtr = Jim_GetVariable(interp, stopVarNamePtr, JIM_NONE);
                if (objPtr == NULL || Jim_GetWide(interp, objPtr, &stop) != JIM_OK) {
                    goto testcond;
                }
            }

            if (currentVal >= stop + cmpOffset) {
                break;
            }

            /* Eval body */
            retval = Jim_EvalObj(interp, argv[4]);
            if (retval == JIM_OK || retval == JIM_CONTINUE) {
                retval = JIM_OK;

                objPtr = Jim_GetVariable(interp, varNamePtr, JIM_ERRMSG);

                /* Increment */
                if (objPtr == NULL) {
                    retval = JIM_ERR;
                    goto out;
                }
                if (!Jim_IsShared(objPtr) && objPtr->typePtr == &intObjType) {
                    currentVal = ++JimWideValue(objPtr);
                    Jim_InvalidateStringRep(objPtr);
                }
                else {
                    if (Jim_GetWide(interp, objPtr, &currentVal) != JIM_OK ||
                        Jim_SetVariable(interp, varNamePtr, Jim_NewIntObj(interp,
                                ++currentVal)) != JIM_OK) {
                        goto evalnext;
                    }
                }
            }
        }
        goto out;
    }
  evalstart:
#endif

    while (boolean && (retval == JIM_OK || retval == JIM_CONTINUE)) {
        /* Body */
        retval = Jim_EvalObj(interp, argv[4]);

        if (retval == JIM_OK || retval == JIM_CONTINUE) {
            /* increment */
          evalnext:
            retval = Jim_EvalObj(interp, argv[3]);
            if (retval == JIM_OK || retval == JIM_CONTINUE) {
                /* test */
              testcond:
                retval = Jim_GetBoolFromExpr(interp, argv[2], &boolean);
            }
        }
    }
  out:
    if (stopVarNamePtr) {
        Jim_DecrRefCount(interp, stopVarNamePtr);
    }
    if (varNamePtr) {
        Jim_DecrRefCount(interp, varNamePtr);
    }

    if (retval == JIM_CONTINUE || retval == JIM_BREAK || retval == JIM_OK) {
        Jim_SetEmptyResult(interp);
        return JIM_OK;
    }

    return retval;
}

/* [loop] */
static int Jim_LoopCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retval;
    jim_wide i;
    jim_wide limit;
    jim_wide incr = 1;
    Jim_Obj *bodyObjPtr;

    if (argc != 5 && argc != 6) {
        Jim_WrongNumArgs(interp, 1, argv, "var first limit ?incr? body");
        return JIM_ERR;
    }

    if (Jim_GetWide(interp, argv[2], &i) != JIM_OK ||
        Jim_GetWide(interp, argv[3], &limit) != JIM_OK ||
          (argc == 6 && Jim_GetWide(interp, argv[4], &incr) != JIM_OK)) {
        return JIM_ERR;
    }
    bodyObjPtr = (argc == 5) ? argv[4] : argv[5];

    retval = Jim_SetVariable(interp, argv[1], argv[2]);

    while (((i < limit && incr > 0) || (i > limit && incr < 0)) && retval == JIM_OK) {
        retval = Jim_EvalObj(interp, bodyObjPtr);
        if (retval == JIM_OK || retval == JIM_CONTINUE) {
            Jim_Obj *objPtr = Jim_GetVariable(interp, argv[1], JIM_ERRMSG);

            retval = JIM_OK;

            /* Increment */
            i += incr;

            if (objPtr && !Jim_IsShared(objPtr) && objPtr->typePtr == &intObjType) {
                if (argv[1]->typePtr != &variableObjType) {
                    if (Jim_SetVariable(interp, argv[1], objPtr) != JIM_OK) {
                        return JIM_ERR;
                    }
                }
                JimWideValue(objPtr) = i;
                Jim_InvalidateStringRep(objPtr);

                /* The following step is required in order to invalidate the
                 * string repr of "FOO" if the var name is of the form of "FOO(IDX)" */
                if (argv[1]->typePtr != &variableObjType) {
                    if (Jim_SetVariable(interp, argv[1], objPtr) != JIM_OK) {
                        retval = JIM_ERR;
                        break;
                    }
                }
            }
            else {
                objPtr = Jim_NewIntObj(interp, i);
                retval = Jim_SetVariable(interp, argv[1], objPtr);
                if (retval != JIM_OK) {
                    Jim_FreeNewObj(interp, objPtr);
                }
            }
        }
    }

    if (retval == JIM_OK || retval == JIM_CONTINUE || retval == JIM_BREAK) {
        Jim_SetEmptyResult(interp);
        return JIM_OK;
    }
    return retval;
}

/* List iterators make it easy to iterate over a list.
 * At some point iterators will be expanded to support generators.
 */
typedef struct {
    Jim_Obj *objPtr;
    int idx;
} Jim_ListIter;

/**
 * Initialise the iterator at the start of the list.
 */
static void JimListIterInit(Jim_ListIter *iter, Jim_Obj *objPtr)
{
    iter->objPtr = objPtr;
    iter->idx = 0;
}

/**
 * Returns the next object from the list, or NULL on end-of-list.
 */
static Jim_Obj *JimListIterNext(Jim_Interp *interp, Jim_ListIter *iter)
{
    if (iter->idx >= Jim_ListLength(interp, iter->objPtr)) {
        return NULL;
    }
    return iter->objPtr->internalRep.listValue.ele[iter->idx++];
}

/**
 * Returns 1 if end-of-list has been reached.
 */
static int JimListIterDone(Jim_Interp *interp, Jim_ListIter *iter)
{
    return iter->idx >= Jim_ListLength(interp, iter->objPtr);
}

/* foreach + lmap implementation. */
static int JimForeachMapHelper(Jim_Interp *interp, int argc, Jim_Obj *const *argv, int doMap)
{
    int result = JIM_OK;
    int i, numargs;
    Jim_ListIter twoiters[2];   /* Avoid allocation for a single list */
    Jim_ListIter *iters;
    Jim_Obj *script;
    Jim_Obj *resultObj;

    if (argc < 4 || argc % 2 != 0) {
        Jim_WrongNumArgs(interp, 1, argv, "varList list ?varList list ...? script");
        return JIM_ERR;
    }
    script = argv[argc - 1];    /* Last argument is a script */
    numargs = (argc - 1 - 1);    /* argc - 'foreach' - script */

    if (numargs == 2) {
        iters = twoiters;
    }
    else {
        iters = Jim_Alloc(numargs * sizeof(*iters));
    }
    for (i = 0; i < numargs; i++) {
        JimListIterInit(&iters[i], argv[i + 1]);
        if (i % 2 == 0 && JimListIterDone(interp, &iters[i])) {
            result = JIM_ERR;
        }
    }
    if (result != JIM_OK) {
        Jim_SetResultString(interp, "foreach varlist is empty", -1);
        return result;
    }

    if (doMap) {
        resultObj = Jim_NewListObj(interp, NULL, 0);
    }
    else {
        resultObj = interp->emptyObj;
    }
    Jim_IncrRefCount(resultObj);

    while (1) {
        /* Have we expired all lists? */
        for (i = 0; i < numargs; i += 2) {
            if (!JimListIterDone(interp, &iters[i + 1])) {
                break;
            }
        }
        if (i == numargs) {
            /* All done */
            break;
        }

        /* For each list */
        for (i = 0; i < numargs; i += 2) {
            Jim_Obj *varName;

            /* foreach var */
            JimListIterInit(&iters[i], argv[i + 1]);
            while ((varName = JimListIterNext(interp, &iters[i])) != NULL) {
                Jim_Obj *valObj = JimListIterNext(interp, &iters[i + 1]);
                if (!valObj) {
                    /* Ran out, so store the empty string */
                    valObj = interp->emptyObj;
                }
                /* Avoid shimmering */
                Jim_IncrRefCount(valObj);
                result = Jim_SetVariable(interp, varName, valObj);
                Jim_DecrRefCount(interp, valObj);
                if (result != JIM_OK) {
                    goto err;
                }
            }
        }
        switch (result = Jim_EvalObj(interp, script)) {
            case JIM_OK:
                if (doMap) {
                    Jim_ListAppendElement(interp, resultObj, interp->result);
                }
                break;
            case JIM_CONTINUE:
                break;
            case JIM_BREAK:
                goto out;
            default:
                goto err;
        }
    }
  out:
    result = JIM_OK;
    Jim_SetResult(interp, resultObj);
  err:
    Jim_DecrRefCount(interp, resultObj);
    if (numargs > 2) {
        Jim_Free(iters);
    }
    return result;
}

/* [foreach] */
static int Jim_ForeachCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimForeachMapHelper(interp, argc, argv, 0);
}

/* [lmap] */
static int Jim_LmapCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimForeachMapHelper(interp, argc, argv, 1);
}

/* [lassign] */
static int Jim_LassignCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int result = JIM_ERR;
    int i;
    Jim_ListIter iter;
    Jim_Obj *resultObj;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varList list ?varName ...?");
        return JIM_ERR;
    }

    JimListIterInit(&iter, argv[1]);

    for (i = 2; i < argc; i++) {
        Jim_Obj *valObj = JimListIterNext(interp, &iter);
        result = Jim_SetVariable(interp, argv[i], valObj ? valObj : interp->emptyObj);
        if (result != JIM_OK) {
            return result;
        }
    }

    resultObj = Jim_NewListObj(interp, NULL, 0);
    while (!JimListIterDone(interp, &iter)) {
        Jim_ListAppendElement(interp, resultObj, JimListIterNext(interp, &iter));
    }

    Jim_SetResult(interp, resultObj);

    return JIM_OK;
}

/* [if] */
static int Jim_IfCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int boolean, retval, current = 1, falsebody = 0;

    if (argc >= 3) {
        while (1) {
            /* Far not enough arguments given! */
            if (current >= argc)
                goto err;
            if ((retval = Jim_GetBoolFromExpr(interp, argv[current++], &boolean))
                != JIM_OK)
                return retval;
            /* There lacks something, isn't it? */
            if (current >= argc)
                goto err;
            if (Jim_CompareStringImmediate(interp, argv[current], "then"))
                current++;
            /* Tsk tsk, no then-clause? */
            if (current >= argc)
                goto err;
            if (boolean)
                return Jim_EvalObj(interp, argv[current]);
            /* Ok: no else-clause follows */
            if (++current >= argc) {
                Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
                return JIM_OK;
            }
            falsebody = current++;
            if (Jim_CompareStringImmediate(interp, argv[falsebody], "else")) {
                /* IIICKS - else-clause isn't last cmd? */
                if (current != argc - 1)
                    goto err;
                return Jim_EvalObj(interp, argv[current]);
            }
            else if (Jim_CompareStringImmediate(interp, argv[falsebody], "elseif"))
                /* Ok: elseif follows meaning all the stuff
                 * again (how boring...) */
                continue;
            /* OOPS - else-clause is not last cmd? */
            else if (falsebody != argc - 1)
                goto err;
            return Jim_EvalObj(interp, argv[falsebody]);
        }
        return JIM_OK;
    }
  err:
    Jim_WrongNumArgs(interp, 1, argv, "condition ?then? trueBody ?elseif ...? ?else? falseBody");
    return JIM_ERR;
}


/* Returns 1 if match, 0 if no match or -<error> on error (e.g. -JIM_ERR, -JIM_BREAK)*/
int Jim_CommandMatchObj(Jim_Interp *interp, Jim_Obj *commandObj, Jim_Obj *patternObj,
    Jim_Obj *stringObj, int nocase)
{
    Jim_Obj *parms[4];
    int argc = 0;
    long eq;
    int rc;

    parms[argc++] = commandObj;
    if (nocase) {
        parms[argc++] = Jim_NewStringObj(interp, "-nocase", -1);
    }
    parms[argc++] = patternObj;
    parms[argc++] = stringObj;

    rc = Jim_EvalObjVector(interp, argc, parms);

    if (rc != JIM_OK || Jim_GetLong(interp, Jim_GetResult(interp), &eq) != JIM_OK) {
        eq = -rc;
    }

    return eq;
}

enum
{ SWITCH_EXACT, SWITCH_GLOB, SWITCH_RE, SWITCH_CMD };

/* [switch] */
static int Jim_SwitchCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int matchOpt = SWITCH_EXACT, opt = 1, patCount, i;
    Jim_Obj *command = 0, *const *caseList = 0, *strObj;
    Jim_Obj *script = 0;

    if (argc < 3) {
      wrongnumargs:
        Jim_WrongNumArgs(interp, 1, argv, "?options? string "
            "pattern body ... ?default body?   or   " "{pattern body ?pattern body ...?}");
        return JIM_ERR;
    }
    for (opt = 1; opt < argc; ++opt) {
        const char *option = Jim_String(argv[opt]);

        if (*option != '-')
            break;
        else if (strncmp(option, "--", 2) == 0) {
            ++opt;
            break;
        }
        else if (strncmp(option, "-exact", 2) == 0)
            matchOpt = SWITCH_EXACT;
        else if (strncmp(option, "-glob", 2) == 0)
            matchOpt = SWITCH_GLOB;
        else if (strncmp(option, "-regexp", 2) == 0)
            matchOpt = SWITCH_RE;
        else if (strncmp(option, "-command", 2) == 0) {
            matchOpt = SWITCH_CMD;
            if ((argc - opt) < 2)
                goto wrongnumargs;
            command = argv[++opt];
        }
        else {
            Jim_SetResultFormatted(interp,
                "bad option \"%#s\": must be -exact, -glob, -regexp, -command procname or --",
                argv[opt]);
            return JIM_ERR;
        }
        if ((argc - opt) < 2)
            goto wrongnumargs;
    }
    strObj = argv[opt++];
    patCount = argc - opt;
    if (patCount == 1) {
        Jim_Obj **vector;

        JimListGetElements(interp, argv[opt], &patCount, &vector);
        caseList = vector;
    }
    else
        caseList = &argv[opt];
    if (patCount == 0 || patCount % 2 != 0)
        goto wrongnumargs;
    for (i = 0; script == 0 && i < patCount; i += 2) {
        Jim_Obj *patObj = caseList[i];

        if (!Jim_CompareStringImmediate(interp, patObj, "default")
            || i < (patCount - 2)) {
            switch (matchOpt) {
                case SWITCH_EXACT:
                    if (Jim_StringEqObj(strObj, patObj))
                        script = caseList[i + 1];
                    break;
                case SWITCH_GLOB:
                    if (Jim_StringMatchObj(interp, patObj, strObj, 0))
                        script = caseList[i + 1];
                    break;
                case SWITCH_RE:
                    command = Jim_NewStringObj(interp, "regexp", -1);
                    /* Fall thru intentionally */
                case SWITCH_CMD:{
                        int rc = Jim_CommandMatchObj(interp, command, patObj, strObj, 0);

                        /* After the execution of a command we need to
                         * make sure to reconvert the object into a list
                         * again. Only for the single-list style [switch]. */
                        if (argc - opt == 1) {
                            Jim_Obj **vector;

                            JimListGetElements(interp, argv[opt], &patCount, &vector);
                            caseList = vector;
                        }
                        /* command is here already decref'd */
                        if (rc < 0) {
                            return -rc;
                        }
                        if (rc)
                            script = caseList[i + 1];
                        break;
                    }
            }
        }
        else {
            script = caseList[i + 1];
        }
    }
    for (; i < patCount && Jim_CompareStringImmediate(interp, script, "-"); i += 2)
        script = caseList[i + 1];
    if (script && Jim_CompareStringImmediate(interp, script, "-")) {
        Jim_SetResultFormatted(interp, "no body specified for pattern \"%#s\"", caseList[i - 2]);
        return JIM_ERR;
    }
    Jim_SetEmptyResult(interp);
    if (script) {
        return Jim_EvalObj(interp, script);
    }
    return JIM_OK;
}

/* [list] */
static int Jim_ListCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *listObjPtr;

    listObjPtr = Jim_NewListObj(interp, argv + 1, argc - 1);
    Jim_SetResult(interp, listObjPtr);
    return JIM_OK;
}

/* [lindex] */
static int Jim_LindexCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr, *listObjPtr;
    int i;
    int idx;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "list ?index ...?");
        return JIM_ERR;
    }
    objPtr = argv[1];
    Jim_IncrRefCount(objPtr);
    for (i = 2; i < argc; i++) {
        listObjPtr = objPtr;
        if (Jim_GetIndex(interp, argv[i], &idx) != JIM_OK) {
            Jim_DecrRefCount(interp, listObjPtr);
            return JIM_ERR;
        }
        if (Jim_ListIndex(interp, listObjPtr, idx, &objPtr, JIM_NONE) != JIM_OK) {
            /* Returns an empty object if the index
             * is out of range. */
            Jim_DecrRefCount(interp, listObjPtr);
            Jim_SetEmptyResult(interp);
            return JIM_OK;
        }
        Jim_IncrRefCount(objPtr);
        Jim_DecrRefCount(interp, listObjPtr);
    }
    Jim_SetResult(interp, objPtr);
    Jim_DecrRefCount(interp, objPtr);
    return JIM_OK;
}

/* [llength] */
static int Jim_LlengthCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "list");
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, Jim_ListLength(interp, argv[1]));
    return JIM_OK;
}

/* [lsearch] */
static int Jim_LsearchCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    static const char * const options[] = {
        "-bool", "-not", "-nocase", "-exact", "-glob", "-regexp", "-all", "-inline", "-command",
            NULL
    };
    enum
    { OPT_BOOL, OPT_NOT, OPT_NOCASE, OPT_EXACT, OPT_GLOB, OPT_REGEXP, OPT_ALL, OPT_INLINE,
            OPT_COMMAND };
    int i;
    int opt_bool = 0;
    int opt_not = 0;
    int opt_nocase = 0;
    int opt_all = 0;
    int opt_inline = 0;
    int opt_match = OPT_EXACT;
    int listlen;
    int rc = JIM_OK;
    Jim_Obj *listObjPtr = NULL;
    Jim_Obj *commandObj = NULL;

    if (argc < 3) {
      wrongargs:
        Jim_WrongNumArgs(interp, 1, argv,
            "?-exact|-glob|-regexp|-command 'command'? ?-bool|-inline? ?-not? ?-nocase? ?-all? list value");
        return JIM_ERR;
    }

    for (i = 1; i < argc - 2; i++) {
        int option;

        if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
        switch (option) {
            case OPT_BOOL:
                opt_bool = 1;
                opt_inline = 0;
                break;
            case OPT_NOT:
                opt_not = 1;
                break;
            case OPT_NOCASE:
                opt_nocase = 1;
                break;
            case OPT_INLINE:
                opt_inline = 1;
                opt_bool = 0;
                break;
            case OPT_ALL:
                opt_all = 1;
                break;
            case OPT_COMMAND:
                if (i >= argc - 2) {
                    goto wrongargs;
                }
                commandObj = argv[++i];
                /* fallthru */
            case OPT_EXACT:
            case OPT_GLOB:
            case OPT_REGEXP:
                opt_match = option;
                break;
        }
    }

    argv += i;

    if (opt_all) {
        listObjPtr = Jim_NewListObj(interp, NULL, 0);
    }
    if (opt_match == OPT_REGEXP) {
        commandObj = Jim_NewStringObj(interp, "regexp", -1);
    }
    if (commandObj) {
        Jim_IncrRefCount(commandObj);
    }

    listlen = Jim_ListLength(interp, argv[0]);
    for (i = 0; i < listlen; i++) {
        int eq = 0;
        Jim_Obj *objPtr = Jim_ListGetIndex(interp, argv[0], i);

        switch (opt_match) {
            case OPT_EXACT:
                eq = Jim_StringCompareObj(interp, argv[1], objPtr, opt_nocase) == 0;
                break;

            case OPT_GLOB:
                eq = Jim_StringMatchObj(interp, argv[1], objPtr, opt_nocase);
                break;

            case OPT_REGEXP:
            case OPT_COMMAND:
                eq = Jim_CommandMatchObj(interp, commandObj, argv[1], objPtr, opt_nocase);
                if (eq < 0) {
                    if (listObjPtr) {
                        Jim_FreeNewObj(interp, listObjPtr);
                    }
                    rc = JIM_ERR;
                    goto done;
                }
                break;
        }

        /* If we have a non-match with opt_bool, opt_not, !opt_all, can't exit early */
        if (!eq && opt_bool && opt_not && !opt_all) {
            continue;
        }

        if ((!opt_bool && eq == !opt_not) || (opt_bool && (eq || opt_all))) {
            /* Got a match (or non-match for opt_not), or (opt_bool && opt_all) */
            Jim_Obj *resultObj;

            if (opt_bool) {
                resultObj = Jim_NewIntObj(interp, eq ^ opt_not);
            }
            else if (!opt_inline) {
                resultObj = Jim_NewIntObj(interp, i);
            }
            else {
                resultObj = objPtr;
            }

            if (opt_all) {
                Jim_ListAppendElement(interp, listObjPtr, resultObj);
            }
            else {
                Jim_SetResult(interp, resultObj);
                goto done;
            }
        }
    }

    if (opt_all) {
        Jim_SetResult(interp, listObjPtr);
    }
    else {
        /* No match */
        if (opt_bool) {
            Jim_SetResultBool(interp, opt_not);
        }
        else if (!opt_inline) {
            Jim_SetResultInt(interp, -1);
        }
    }

  done:
    if (commandObj) {
        Jim_DecrRefCount(interp, commandObj);
    }
    return rc;
}

/* [lappend] */
static int Jim_LappendCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *listObjPtr;
    int new_obj = 0;
    int i;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?value value ...?");
        return JIM_ERR;
    }
    listObjPtr = Jim_GetVariable(interp, argv[1], JIM_UNSHARED);
    if (!listObjPtr) {
        /* Create the list if it does not exist */
        listObjPtr = Jim_NewListObj(interp, NULL, 0);
        new_obj = 1;
    }
    else if (Jim_IsShared(listObjPtr)) {
        listObjPtr = Jim_DuplicateObj(interp, listObjPtr);
        new_obj = 1;
    }
    for (i = 2; i < argc; i++)
        Jim_ListAppendElement(interp, listObjPtr, argv[i]);
    if (Jim_SetVariable(interp, argv[1], listObjPtr) != JIM_OK) {
        if (new_obj)
            Jim_FreeNewObj(interp, listObjPtr);
        return JIM_ERR;
    }
    Jim_SetResult(interp, listObjPtr);
    return JIM_OK;
}

/* [linsert] */
static int Jim_LinsertCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int idx, len;
    Jim_Obj *listPtr;

    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "list index ?element ...?");
        return JIM_ERR;
    }
    listPtr = argv[1];
    if (Jim_IsShared(listPtr))
        listPtr = Jim_DuplicateObj(interp, listPtr);
    if (Jim_GetIndex(interp, argv[2], &idx) != JIM_OK)
        goto err;
    len = Jim_ListLength(interp, listPtr);
    if (idx >= len)
        idx = len;
    else if (idx < 0)
        idx = len + idx + 1;
    Jim_ListInsertElements(interp, listPtr, idx, argc - 3, &argv[3]);
    Jim_SetResult(interp, listPtr);
    return JIM_OK;
  err:
    if (listPtr != argv[1]) {
        Jim_FreeNewObj(interp, listPtr);
    }
    return JIM_ERR;
}

/* [lreplace] */
static int Jim_LreplaceCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int first, last, len, rangeLen;
    Jim_Obj *listObj;
    Jim_Obj *newListObj;

    if (argc < 4) {
        Jim_WrongNumArgs(interp, 1, argv, "list first last ?element ...?");
        return JIM_ERR;
    }
    if (Jim_GetIndex(interp, argv[2], &first) != JIM_OK ||
        Jim_GetIndex(interp, argv[3], &last) != JIM_OK) {
        return JIM_ERR;
    }

    listObj = argv[1];
    len = Jim_ListLength(interp, listObj);

    first = JimRelToAbsIndex(len, first);
    last = JimRelToAbsIndex(len, last);
    JimRelToAbsRange(len, &first, &last, &rangeLen);

    /* Now construct a new list which consists of:
     * <elements before first> <supplied elements> <elements after last>
     */

    /* Check to see if trying to replace past the end of the list */
    if (first < len) {
        /* OK. Not past the end */
    }
    else if (len == 0) {
        /* Special for empty list, adjust first to 0 */
        first = 0;
    }
    else {
        Jim_SetResultString(interp, "list doesn't contain element ", -1);
        Jim_AppendObj(interp, Jim_GetResult(interp), argv[2]);
        return JIM_ERR;
    }

    /* Add the first set of elements */
    newListObj = Jim_NewListObj(interp, listObj->internalRep.listValue.ele, first);

    /* Add supplied elements */
    ListInsertElements(newListObj, -1, argc - 4, argv + 4);

    /* Add the remaining elements */
    ListInsertElements(newListObj, -1, len - first - rangeLen, listObj->internalRep.listValue.ele + first + rangeLen);

    Jim_SetResult(interp, newListObj);
    return JIM_OK;
}

/* [lset] */
static int Jim_LsetCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "listVar ?index...? newVal");
        return JIM_ERR;
    }
    else if (argc == 3) {
        /* With no indexes, simply implements [set] */
        if (Jim_SetVariable(interp, argv[1], argv[2]) != JIM_OK)
            return JIM_ERR;
        Jim_SetResult(interp, argv[2]);
        return JIM_OK;
    }
    return Jim_ListSetIndex(interp, argv[1], argv + 2, argc - 3, argv[argc - 1]);
}

/* [lsort] */
static int Jim_LsortCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const argv[])
{
    static const char * const options[] = {
        "-ascii", "-nocase", "-increasing", "-decreasing", "-command", "-integer", "-real", "-index", "-unique", NULL
    };
    enum
    { OPT_ASCII, OPT_NOCASE, OPT_INCREASING, OPT_DECREASING, OPT_COMMAND, OPT_INTEGER, OPT_REAL, OPT_INDEX, OPT_UNIQUE };
    Jim_Obj *resObj;
    int i;
    int retCode;

    struct lsort_info info;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "?options? list");
        return JIM_ERR;
    }

    info.type = JIM_LSORT_ASCII;
    info.order = 1;
    info.indexed = 0;
    info.unique = 0;
    info.command = NULL;
    info.interp = interp;

    for (i = 1; i < (argc - 1); i++) {
        int option;

        if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ENUM_ABBREV | JIM_ERRMSG)
            != JIM_OK)
            return JIM_ERR;
        switch (option) {
            case OPT_ASCII:
                info.type = JIM_LSORT_ASCII;
                break;
            case OPT_NOCASE:
                info.type = JIM_LSORT_NOCASE;
                break;
            case OPT_INTEGER:
                info.type = JIM_LSORT_INTEGER;
                break;
            case OPT_REAL:
                info.type = JIM_LSORT_REAL;
                break;
            case OPT_INCREASING:
                info.order = 1;
                break;
            case OPT_DECREASING:
                info.order = -1;
                break;
            case OPT_UNIQUE:
                info.unique = 1;
                break;
            case OPT_COMMAND:
                if (i >= (argc - 2)) {
                    Jim_SetResultString(interp, "\"-command\" option must be followed by comparison command", -1);
                    return JIM_ERR;
                }
                info.type = JIM_LSORT_COMMAND;
                info.command = argv[i + 1];
                i++;
                break;
            case OPT_INDEX:
                if (i >= (argc - 2)) {
                    Jim_SetResultString(interp, "\"-index\" option must be followed by list index", -1);
                    return JIM_ERR;
                }
                if (Jim_GetIndex(interp, argv[i + 1], &info.index) != JIM_OK) {
                    return JIM_ERR;
                }
                info.indexed = 1;
                i++;
                break;
        }
    }
    resObj = Jim_DuplicateObj(interp, argv[argc - 1]);
    retCode = ListSortElements(interp, resObj, &info);
    if (retCode == JIM_OK) {
        Jim_SetResult(interp, resObj);
    }
    else {
        Jim_FreeNewObj(interp, resObj);
    }
    return retCode;
}

/* [append] */
static int Jim_AppendCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *stringObjPtr;
    int i;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?value ...?");
        return JIM_ERR;
    }
    if (argc == 2) {
        stringObjPtr = Jim_GetVariable(interp, argv[1], JIM_ERRMSG);
        if (!stringObjPtr)
            return JIM_ERR;
    }
    else {
        int new_obj = 0;
        stringObjPtr = Jim_GetVariable(interp, argv[1], JIM_UNSHARED);
        if (!stringObjPtr) {
            /* Create the string if it doesn't exist */
            stringObjPtr = Jim_NewEmptyStringObj(interp);
            new_obj = 1;
        }
        else if (Jim_IsShared(stringObjPtr)) {
            new_obj = 1;
            stringObjPtr = Jim_DuplicateObj(interp, stringObjPtr);
        }
        for (i = 2; i < argc; i++) {
            Jim_AppendObj(interp, stringObjPtr, argv[i]);
        }
        if (Jim_SetVariable(interp, argv[1], stringObjPtr) != JIM_OK) {
            if (new_obj) {
                Jim_FreeNewObj(interp, stringObjPtr);
            }
            return JIM_ERR;
        }
    }
    Jim_SetResult(interp, stringObjPtr);
    return JIM_OK;
}

/* [debug] */
static int Jim_DebugCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
#if defined(JIM_DEBUG_COMMAND) && !defined(JIM_BOOTSTRAP)
    static const char * const options[] = {
        "refcount", "objcount", "objects", "invstr", "scriptlen", "exprlen",
        "exprbc", "show",
        NULL
    };
    enum
    {
        OPT_REFCOUNT, OPT_OBJCOUNT, OPT_OBJECTS, OPT_INVSTR, OPT_SCRIPTLEN,
        OPT_EXPRLEN, OPT_EXPRBC, OPT_SHOW,
    };
    int option;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "subcommand ?...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], options, &option, "subcommand", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    if (option == OPT_REFCOUNT) {
        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "object");
            return JIM_ERR;
        }
        Jim_SetResultInt(interp, argv[2]->refCount);
        return JIM_OK;
    }
    else if (option == OPT_OBJCOUNT) {
        int freeobj = 0, liveobj = 0;
        char buf[256];
        Jim_Obj *objPtr;

        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        /* Count the number of free objects. */
        objPtr = interp->freeList;
        while (objPtr) {
            freeobj++;
            objPtr = objPtr->nextObjPtr;
        }
        /* Count the number of live objects. */
        objPtr = interp->liveList;
        while (objPtr) {
            liveobj++;
            objPtr = objPtr->nextObjPtr;
        }
        /* Set the result string and return. */
        sprintf(buf, "free %d used %d", freeobj, liveobj);
        Jim_SetResultString(interp, buf, -1);
        return JIM_OK;
    }
    else if (option == OPT_OBJECTS) {
        Jim_Obj *objPtr, *listObjPtr, *subListObjPtr;

        /* Count the number of live objects. */
        objPtr = interp->liveList;
        listObjPtr = Jim_NewListObj(interp, NULL, 0);
        while (objPtr) {
            char buf[128];
            const char *type = objPtr->typePtr ? objPtr->typePtr->name : "";

            subListObjPtr = Jim_NewListObj(interp, NULL, 0);
            sprintf(buf, "%p", objPtr);
            Jim_ListAppendElement(interp, subListObjPtr, Jim_NewStringObj(interp, buf, -1));
            Jim_ListAppendElement(interp, subListObjPtr, Jim_NewStringObj(interp, type, -1));
            Jim_ListAppendElement(interp, subListObjPtr, Jim_NewIntObj(interp, objPtr->refCount));
            Jim_ListAppendElement(interp, subListObjPtr, objPtr);
            Jim_ListAppendElement(interp, listObjPtr, subListObjPtr);
            objPtr = objPtr->nextObjPtr;
        }
        Jim_SetResult(interp, listObjPtr);
        return JIM_OK;
    }
    else if (option == OPT_INVSTR) {
        Jim_Obj *objPtr;

        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "object");
            return JIM_ERR;
        }
        objPtr = argv[2];
        if (objPtr->typePtr != NULL)
            Jim_InvalidateStringRep(objPtr);
        Jim_SetEmptyResult(interp);
        return JIM_OK;
    }
    else if (option == OPT_SHOW) {
        const char *s;
        int len, charlen;

        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "object");
            return JIM_ERR;
        }
        s = Jim_GetString(argv[2], &len);
#ifdef JIM_UTF8
        charlen = utf8_strlen(s, len);
#else
        charlen = len;
#endif
        printf("refcount: %d, type: %s\n", argv[2]->refCount, JimObjTypeName(argv[2]));
        printf("chars (%d): <<%s>>\n", charlen, s);
        printf("bytes (%d):", len);
        while (len--) {
            printf(" %02x", (unsigned char)*s++);
        }
        printf("\n");
        return JIM_OK;
    }
    else if (option == OPT_SCRIPTLEN) {
        ScriptObj *script;

        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "script");
            return JIM_ERR;
        }
        script = JimGetScript(interp, argv[2]);
        if (script == NULL)
            return JIM_ERR;
        Jim_SetResultInt(interp, script->len);
        return JIM_OK;
    }
    else if (option == OPT_EXPRLEN) {
        ExprByteCode *expr;

        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "expression");
            return JIM_ERR;
        }
        expr = JimGetExpression(interp, argv[2]);
        if (expr == NULL)
            return JIM_ERR;
        Jim_SetResultInt(interp, expr->len);
        return JIM_OK;
    }
    else if (option == OPT_EXPRBC) {
        Jim_Obj *objPtr;
        ExprByteCode *expr;
        int i;

        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "expression");
            return JIM_ERR;
        }
        expr = JimGetExpression(interp, argv[2]);
        if (expr == NULL)
            return JIM_ERR;
        objPtr = Jim_NewListObj(interp, NULL, 0);
        for (i = 0; i < expr->len; i++) {
            const char *type;
            const Jim_ExprOperator *op;
            Jim_Obj *obj = expr->token[i].objPtr;

            switch (expr->token[i].type) {
                case JIM_TT_EXPR_INT:
                    type = "int";
                    break;
                case JIM_TT_EXPR_DOUBLE:
                    type = "double";
                    break;
                case JIM_TT_CMD:
                    type = "command";
                    break;
                case JIM_TT_VAR:
                    type = "variable";
                    break;
                case JIM_TT_DICTSUGAR:
                    type = "dictsugar";
                    break;
                case JIM_TT_EXPRSUGAR:
                    type = "exprsugar";
                    break;
                case JIM_TT_ESC:
                    type = "subst";
                    break;
                case JIM_TT_STR:
                    type = "string";
                    break;
                default:
                    op = JimExprOperatorInfoByOpcode(expr->token[i].type);
                    if (op == NULL) {
                        type = "private";
                    }
                    else {
                        type = "operator";
                    }
                    obj = Jim_NewStringObj(interp, op ? op->name : "", -1);
                    break;
            }
            Jim_ListAppendElement(interp, objPtr, Jim_NewStringObj(interp, type, -1));
            Jim_ListAppendElement(interp, objPtr, obj);
        }
        Jim_SetResult(interp, objPtr);
        return JIM_OK;
    }
    else {
        Jim_SetResultString(interp,
            "bad option. Valid options are refcount, " "objcount, objects, invstr", -1);
        return JIM_ERR;
    }
    /* unreached */
#endif /* JIM_BOOTSTRAP */
#if !defined(JIM_DEBUG_COMMAND)
    Jim_SetResultString(interp, "unsupported", -1);
    return JIM_ERR;
#endif
}

/* [eval] */
static int Jim_EvalCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int rc;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "arg ?arg ...?");
        return JIM_ERR;
    }

    if (argc == 2) {
        rc = Jim_EvalObj(interp, argv[1]);
    }
    else {
        rc = Jim_EvalObj(interp, Jim_ConcatObj(interp, argc - 1, argv + 1));
    }

    if (rc == JIM_ERR) {
        /* eval is "interesting", so add a stack frame here */
        interp->addStackTrace++;
    }
    return rc;
}

/* [uplevel] */
static int Jim_UplevelCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc >= 2) {
        int retcode;
        Jim_CallFrame *savedCallFrame, *targetCallFrame;
        const char *str;

        /* Save the old callframe pointer */
        savedCallFrame = interp->framePtr;

        /* Lookup the target frame pointer */
        str = Jim_String(argv[1]);
        if ((str[0] >= '0' && str[0] <= '9') || str[0] == '#') {
            targetCallFrame = Jim_GetCallFrameByLevel(interp, argv[1]);
            argc--;
            argv++;
        }
        else {
            targetCallFrame = Jim_GetCallFrameByLevel(interp, NULL);
        }
        if (targetCallFrame == NULL) {
            return JIM_ERR;
        }
        if (argc < 2) {
            Jim_WrongNumArgs(interp, 1, argv - 1, "?level? command ?arg ...?");
            return JIM_ERR;
        }
        /* Eval the code in the target callframe. */
        interp->framePtr = targetCallFrame;
        if (argc == 2) {
            retcode = Jim_EvalObj(interp, argv[1]);
        }
        else {
            retcode = Jim_EvalObj(interp, Jim_ConcatObj(interp, argc - 1, argv + 1));
        }
        interp->framePtr = savedCallFrame;
        return retcode;
    }
    else {
        Jim_WrongNumArgs(interp, 1, argv, "?level? command ?arg ...?");
        return JIM_ERR;
    }
}

/* [expr] */
static int Jim_ExprCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *exprResultPtr;
    int retcode;

    if (argc == 2) {
        retcode = Jim_EvalExpression(interp, argv[1], &exprResultPtr);
    }
    else if (argc > 2) {
        Jim_Obj *objPtr;

        objPtr = Jim_ConcatObj(interp, argc - 1, argv + 1);
        Jim_IncrRefCount(objPtr);
        retcode = Jim_EvalExpression(interp, objPtr, &exprResultPtr);
        Jim_DecrRefCount(interp, objPtr);
    }
    else {
        Jim_WrongNumArgs(interp, 1, argv, "expression ?...?");
        return JIM_ERR;
    }
    if (retcode != JIM_OK)
        return retcode;
    Jim_SetResult(interp, exprResultPtr);
    Jim_DecrRefCount(interp, exprResultPtr);
    return JIM_OK;
}

/* [break] */
static int Jim_BreakCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    return JIM_BREAK;
}

/* [continue] */
static int Jim_ContinueCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    return JIM_CONTINUE;
}

/* [return] */
static int Jim_ReturnCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    Jim_Obj *stackTraceObj = NULL;
    Jim_Obj *errorCodeObj = NULL;
    int returnCode = JIM_OK;
    long level = 1;

    for (i = 1; i < argc - 1; i += 2) {
        if (Jim_CompareStringImmediate(interp, argv[i], "-code")) {
            if (Jim_GetReturnCode(interp, argv[i + 1], &returnCode) == JIM_ERR) {
                return JIM_ERR;
            }
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-errorinfo")) {
            stackTraceObj = argv[i + 1];
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-errorcode")) {
            errorCodeObj = argv[i + 1];
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-level")) {
            if (Jim_GetLong(interp, argv[i + 1], &level) != JIM_OK || level < 0) {
                Jim_SetResultFormatted(interp, "bad level \"%#s\"", argv[i + 1]);
                return JIM_ERR;
            }
        }
        else {
            break;
        }
    }

    if (i != argc - 1 && i != argc) {
        Jim_WrongNumArgs(interp, 1, argv,
            "?-code code? ?-errorinfo stacktrace? ?-level level? ?result?");
    }

    /* If a stack trace is supplied and code is error, set the stack trace */
    if (stackTraceObj && returnCode == JIM_ERR) {
        JimSetStackTrace(interp, stackTraceObj);
    }
    /* If an error code list is supplied, set the global $errorCode */
    if (errorCodeObj && returnCode == JIM_ERR) {
        Jim_SetGlobalVariableStr(interp, "errorCode", errorCodeObj);
    }
    interp->returnCode = returnCode;
    interp->returnLevel = level;

    if (i == argc - 1) {
        Jim_SetResult(interp, argv[i]);
    }
    return JIM_RETURN;
}

/* [tailcall] */
static int Jim_TailcallCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (interp->framePtr->level == 0) {
        Jim_SetResultString(interp, "tailcall can only be called from a proc or lambda", -1);
        return JIM_ERR;
    }
    else if (argc >= 2) {
        /* Need to resolve the tailcall command in the current context */
        Jim_CallFrame *cf = interp->framePtr->parent;

        Jim_Cmd *cmdPtr = Jim_GetCommand(interp, argv[1], JIM_ERRMSG);
        if (cmdPtr == NULL) {
            return JIM_ERR;
        }

        JimPanic((cf->tailcallCmd != NULL, "Already have a tailcallCmd"));

        /* And stash this pre-resolved command */
        JimIncrCmdRefCount(cmdPtr);
        cf->tailcallCmd = cmdPtr;

        /* And stash the command list */
        JimPanic((cf->tailcallObj != NULL, "Already have a tailcallobj"));

        cf->tailcallObj = Jim_NewListObj(interp, argv + 1, argc - 1);
        Jim_IncrRefCount(cf->tailcallObj);

        /* When the stack unwinds to the previous proc, the stashed command will be evaluated */
        return JIM_EVAL;
    }
    return JIM_OK;
}

static int JimAliasCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *cmdList;
    Jim_Obj *prefixListObj = Jim_CmdPrivData(interp);

    /* prefixListObj is a list to which the args need to be appended */
    cmdList = Jim_DuplicateObj(interp, prefixListObj);
    Jim_ListInsertElements(interp, cmdList, Jim_ListLength(interp, cmdList), argc - 1, argv + 1);

    return JimEvalObjList(interp, cmdList);
}

static void JimAliasCmdDelete(Jim_Interp *interp, void *privData)
{
    Jim_Obj *prefixListObj = privData;
    Jim_DecrRefCount(interp, prefixListObj);
}

static int Jim_AliasCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *prefixListObj;
    const char *newname;

    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "newname command ?args ...?");
        return JIM_ERR;
    }

    prefixListObj = Jim_NewListObj(interp, argv + 2, argc - 2);
    Jim_IncrRefCount(prefixListObj);
    newname = Jim_String(argv[1]);
    if (newname[0] == ':' && newname[1] == ':') {
        while (*++newname == ':') {
        }
    }

    Jim_SetResult(interp, argv[1]);

    return Jim_CreateCommand(interp, newname, JimAliasCmd, prefixListObj, JimAliasCmdDelete);
}

/* [proc] */
static int Jim_ProcCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Cmd *cmd;

    if (argc != 4 && argc != 5) {
        Jim_WrongNumArgs(interp, 1, argv, "name arglist ?statics? body");
        return JIM_ERR;
    }

    if (JimValidName(interp, "procedure", argv[1]) != JIM_OK) {
        return JIM_ERR;
    }

    if (argc == 4) {
        cmd = JimCreateProcedureCmd(interp, argv[2], NULL, argv[3], NULL);
    }
    else {
        cmd = JimCreateProcedureCmd(interp, argv[2], argv[3], argv[4], NULL);
    }

    if (cmd) {
        /* Add the new command */
        Jim_Obj *qualifiedCmdNameObj;
        const char *cmdname = JimQualifyName(interp, Jim_String(argv[1]), &qualifiedCmdNameObj);

        JimCreateCommand(interp, cmdname, cmd);

        /* Calculate and set the namespace for this proc */
        JimUpdateProcNamespace(interp, cmd, cmdname);

        JimFreeQualifiedName(interp, qualifiedCmdNameObj);

        /* Unlike Tcl, set the name of the proc as the result */
        Jim_SetResult(interp, argv[1]);
        return JIM_OK;
    }
    return JIM_ERR;
}

/* [local] */
static int Jim_LocalCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retcode;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "cmd ?args ...?");
        return JIM_ERR;
    }

    /* Evaluate the arguments with 'local' in force */
    interp->local++;
    retcode = Jim_EvalObjVector(interp, argc - 1, argv + 1);
    interp->local--;


    /* If OK, and the result is a proc, add it to the list of local procs */
    if (retcode == 0) {
        Jim_Obj *cmdNameObj = Jim_GetResult(interp);

        if (Jim_GetCommand(interp, cmdNameObj, JIM_ERRMSG) == NULL) {
            return JIM_ERR;
        }
        if (interp->framePtr->localCommands == NULL) {
            interp->framePtr->localCommands = Jim_Alloc(sizeof(*interp->framePtr->localCommands));
            Jim_InitStack(interp->framePtr->localCommands);
        }
        Jim_IncrRefCount(cmdNameObj);
        Jim_StackPush(interp->framePtr->localCommands, cmdNameObj);
    }

    return retcode;
}

/* [upcall] */
static int Jim_UpcallCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "cmd ?args ...?");
        return JIM_ERR;
    }
    else {
        int retcode;

        Jim_Cmd *cmdPtr = Jim_GetCommand(interp, argv[1], JIM_ERRMSG);
        if (cmdPtr == NULL || !cmdPtr->isproc || !cmdPtr->prevCmd) {
            Jim_SetResultFormatted(interp, "no previous command: \"%#s\"", argv[1]);
            return JIM_ERR;
        }
        /* OK. Mark this command as being in an upcall */
        cmdPtr->u.proc.upcall++;
        JimIncrCmdRefCount(cmdPtr);

        /* Invoke the command as normal */
        retcode = Jim_EvalObjVector(interp, argc - 1, argv + 1);

        /* No longer in an upcall */
        cmdPtr->u.proc.upcall--;
        JimDecrCmdRefCount(interp, cmdPtr);

        return retcode;
    }
}

/* [apply] */
static int Jim_ApplyCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "lambdaExpr ?arg ...?");
        return JIM_ERR;
    }
    else {
        int ret;
        Jim_Cmd *cmd;
        Jim_Obj *argListObjPtr;
        Jim_Obj *bodyObjPtr;
        Jim_Obj *nsObj = NULL;
        Jim_Obj **nargv;

        int len = Jim_ListLength(interp, argv[1]);
        if (len != 2 && len != 3) {
            Jim_SetResultFormatted(interp, "can't interpret \"%#s\" as a lambda expression", argv[1]);
            return JIM_ERR;
        }

        if (len == 3) {
#ifdef jim_ext_namespace
            /* Need to canonicalise the given namespace. */
            nsObj = JimQualifyNameObj(interp, Jim_ListGetIndex(interp, argv[1], 2));
#else
            Jim_SetResultString(interp, "namespaces not enabled", -1);
            return JIM_ERR;
#endif
        }
        argListObjPtr = Jim_ListGetIndex(interp, argv[1], 0);
        bodyObjPtr = Jim_ListGetIndex(interp, argv[1], 1);

        cmd = JimCreateProcedureCmd(interp, argListObjPtr, NULL, bodyObjPtr, nsObj);

        if (cmd) {
            /* Create a new argv array with a dummy argv[0], for error messages */
            nargv = Jim_Alloc((argc - 2 + 1) * sizeof(*nargv));
            nargv[0] = Jim_NewStringObj(interp, "apply lambdaExpr", -1);
            Jim_IncrRefCount(nargv[0]);
            memcpy(&nargv[1], argv + 2, (argc - 2) * sizeof(*nargv));
            ret = JimCallProcedure(interp, cmd, argc - 2 + 1, nargv);
            Jim_DecrRefCount(interp, nargv[0]);
            Jim_Free(nargv);

            JimDecrCmdRefCount(interp, cmd);
            return ret;
        }
        return JIM_ERR;
    }
}


/* [concat] */
static int Jim_ConcatCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_SetResult(interp, Jim_ConcatObj(interp, argc - 1, argv + 1));
    return JIM_OK;
}

/* [upvar] */
static int Jim_UpvarCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    Jim_CallFrame *targetCallFrame;

    /* Lookup the target frame pointer */
    if (argc > 3 && (argc % 2 == 0)) {
        targetCallFrame = Jim_GetCallFrameByLevel(interp, argv[1]);
        argc--;
        argv++;
    }
    else {
        targetCallFrame = Jim_GetCallFrameByLevel(interp, NULL);
    }
    if (targetCallFrame == NULL) {
        return JIM_ERR;
    }

    /* Check for arity */
    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "?level? otherVar localVar ?otherVar localVar ...?");
        return JIM_ERR;
    }

    /* Now... for every other/local couple: */
    for (i = 1; i < argc; i += 2) {
        if (Jim_SetVariableLink(interp, argv[i + 1], argv[i], targetCallFrame) != JIM_OK)
            return JIM_ERR;
    }
    return JIM_OK;
}

/* [global] */
static int Jim_GlobalCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?varName ...?");
        return JIM_ERR;
    }
    /* Link every var to the toplevel having the same name */
    if (interp->framePtr->level == 0)
        return JIM_OK;          /* global at toplevel... */
    for (i = 1; i < argc; i++) {
        /* global ::blah does nothing */
        const char *name = Jim_String(argv[i]);
        if (name[0] != ':' || name[1] != ':') {
            if (Jim_SetVariableLink(interp, argv[i], argv[i], interp->topFramePtr) != JIM_OK)
                return JIM_ERR;
        }
    }
    return JIM_OK;
}

/* does the [string map] operation. On error NULL is returned,
 * otherwise a new string object with the result, having refcount = 0,
 * is returned. */
static Jim_Obj *JimStringMap(Jim_Interp *interp, Jim_Obj *mapListObjPtr,
    Jim_Obj *objPtr, int nocase)
{
    int numMaps;
    const char *str, *noMatchStart = NULL;
    int strLen, i;
    Jim_Obj *resultObjPtr;

    numMaps = Jim_ListLength(interp, mapListObjPtr);
    if (numMaps % 2) {
        Jim_SetResultString(interp, "list must contain an even number of elements", -1);
        return NULL;
    }

    str = Jim_String(objPtr);
    strLen = Jim_Utf8Length(interp, objPtr);

    /* Map it */
    resultObjPtr = Jim_NewStringObj(interp, "", 0);
    while (strLen) {
        for (i = 0; i < numMaps; i += 2) {
            Jim_Obj *objPtr;
            const char *k;
            int kl;

            objPtr = Jim_ListGetIndex(interp, mapListObjPtr, i);
            k = Jim_String(objPtr);
            kl = Jim_Utf8Length(interp, objPtr);

            if (strLen >= kl && kl) {
                int rc;
                rc = JimStringCompareLen(str, k, kl, nocase);
                if (rc == 0) {
                    if (noMatchStart) {
                        Jim_AppendString(interp, resultObjPtr, noMatchStart, str - noMatchStart);
                        noMatchStart = NULL;
                    }
                    Jim_AppendObj(interp, resultObjPtr, Jim_ListGetIndex(interp, mapListObjPtr, i + 1));
                    str += utf8_index(str, kl);
                    strLen -= kl;
                    break;
                }
            }
        }
        if (i == numMaps) {     /* no match */
            int c;
            if (noMatchStart == NULL)
                noMatchStart = str;
            str += utf8_tounicode(str, &c);
            strLen--;
        }
    }
    if (noMatchStart) {
        Jim_AppendString(interp, resultObjPtr, noMatchStart, str - noMatchStart);
    }
    return resultObjPtr;
}

/* [string] */
static int Jim_StringCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int len;
    int opt_case = 1;
    int option;
    static const char * const options[] = {
        "bytelength", "length", "compare", "match", "equal", "is", "byterange", "range", "replace",
        "map", "repeat", "reverse", "index", "first", "last", "cat",
        "trim", "trimleft", "trimright", "tolower", "toupper", "totitle", NULL
    };
    enum
    {
        OPT_BYTELENGTH, OPT_LENGTH, OPT_COMPARE, OPT_MATCH, OPT_EQUAL, OPT_IS, OPT_BYTERANGE, OPT_RANGE, OPT_REPLACE,
        OPT_MAP, OPT_REPEAT, OPT_REVERSE, OPT_INDEX, OPT_FIRST, OPT_LAST, OPT_CAT,
        OPT_TRIM, OPT_TRIMLEFT, OPT_TRIMRIGHT, OPT_TOLOWER, OPT_TOUPPER, OPT_TOTITLE
    };
    static const char * const nocase_options[] = {
        "-nocase", NULL
    };
    static const char * const nocase_length_options[] = {
        "-nocase", "-length", NULL
    };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "option ?arguments ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], options, &option, NULL,
            JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK)
        return JIM_ERR;

    switch (option) {
        case OPT_LENGTH:
        case OPT_BYTELENGTH:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "string");
                return JIM_ERR;
            }
            if (option == OPT_LENGTH) {
                len = Jim_Utf8Length(interp, argv[2]);
            }
            else {
                len = Jim_Length(argv[2]);
            }
            Jim_SetResultInt(interp, len);
            return JIM_OK;

        case OPT_CAT:{
                Jim_Obj *objPtr;
                if (argc == 3) {
                    /* optimise the one-arg case */
                    objPtr = argv[2];
                }
                else {
                    int i;

                    objPtr = Jim_NewStringObj(interp, "", 0);

                    for (i = 2; i < argc; i++) {
                        Jim_AppendObj(interp, objPtr, argv[i]);
                    }
                }
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }

        case OPT_COMPARE:
        case OPT_EQUAL:
            {
                /* n is the number of remaining option args */
                long opt_length = -1;
                int n = argc - 4;
                int i = 2;
                while (n > 0) {
                    int subopt;
                    if (Jim_GetEnum(interp, argv[i++], nocase_length_options, &subopt, NULL,
                            JIM_ENUM_ABBREV) != JIM_OK) {
badcompareargs:
                        Jim_WrongNumArgs(interp, 2, argv, "?-nocase? ?-length int? string1 string2");
                        return JIM_ERR;
                    }
                    if (subopt == 0) {
                        /* -nocase */
                        opt_case = 0;
                        n--;
                    }
                    else {
                        /* -length */
                        if (n < 2) {
                            goto badcompareargs;
                        }
                        if (Jim_GetLong(interp, argv[i++], &opt_length) != JIM_OK) {
                            return JIM_ERR;
                        }
                        n -= 2;
                    }
                }
                if (n) {
                    goto badcompareargs;
                }
                argv += argc - 2;
                if (opt_length < 0 && option != OPT_COMPARE && opt_case) {
                    /* Fast version - [string equal], case sensitive, no length */
                    Jim_SetResultBool(interp, Jim_StringEqObj(argv[0], argv[1]));
                }
                else {
                    if (opt_length >= 0) {
                        n = JimStringCompareLen(Jim_String(argv[0]), Jim_String(argv[1]), opt_length, !opt_case);
                    }
                    else {
                        n = Jim_StringCompareObj(interp, argv[0], argv[1], !opt_case);
                    }
                    Jim_SetResultInt(interp, option == OPT_COMPARE ? n : n == 0);
                }
                return JIM_OK;
            }

        case OPT_MATCH:
            if (argc != 4 &&
                (argc != 5 ||
                    Jim_GetEnum(interp, argv[2], nocase_options, &opt_case, NULL,
                        JIM_ENUM_ABBREV) != JIM_OK)) {
                Jim_WrongNumArgs(interp, 2, argv, "?-nocase? pattern string");
                return JIM_ERR;
            }
            if (opt_case == 0) {
                argv++;
            }
            Jim_SetResultBool(interp, Jim_StringMatchObj(interp, argv[2], argv[3], !opt_case));
            return JIM_OK;

        case OPT_MAP:{
                Jim_Obj *objPtr;

                if (argc != 4 &&
                    (argc != 5 ||
                        Jim_GetEnum(interp, argv[2], nocase_options, &opt_case, NULL,
                            JIM_ENUM_ABBREV) != JIM_OK)) {
                    Jim_WrongNumArgs(interp, 2, argv, "?-nocase? mapList string");
                    return JIM_ERR;
                }

                if (opt_case == 0) {
                    argv++;
                }
                objPtr = JimStringMap(interp, argv[2], argv[3], !opt_case);
                if (objPtr == NULL) {
                    return JIM_ERR;
                }
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }

        case OPT_RANGE:
        case OPT_BYTERANGE:{
                Jim_Obj *objPtr;

                if (argc != 5) {
                    Jim_WrongNumArgs(interp, 2, argv, "string first last");
                    return JIM_ERR;
                }
                if (option == OPT_RANGE) {
                    objPtr = Jim_StringRangeObj(interp, argv[2], argv[3], argv[4]);
                }
                else
                {
                    objPtr = Jim_StringByteRangeObj(interp, argv[2], argv[3], argv[4]);
                }

                if (objPtr == NULL) {
                    return JIM_ERR;
                }
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }

        case OPT_REPLACE:{
                Jim_Obj *objPtr;

                if (argc != 5 && argc != 6) {
                    Jim_WrongNumArgs(interp, 2, argv, "string first last ?string?");
                    return JIM_ERR;
                }
                objPtr = JimStringReplaceObj(interp, argv[2], argv[3], argv[4], argc == 6 ? argv[5] : NULL);
                if (objPtr == NULL) {
                    return JIM_ERR;
                }
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }


        case OPT_REPEAT:{
                Jim_Obj *objPtr;
                jim_wide count;

                if (argc != 4) {
                    Jim_WrongNumArgs(interp, 2, argv, "string count");
                    return JIM_ERR;
                }
                if (Jim_GetWide(interp, argv[3], &count) != JIM_OK) {
                    return JIM_ERR;
                }
                objPtr = Jim_NewStringObj(interp, "", 0);
                if (count > 0) {
                    while (count--) {
                        Jim_AppendObj(interp, objPtr, argv[2]);
                    }
                }
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }

        case OPT_REVERSE:{
                char *buf, *p;
                const char *str;
                int len;
                int i;

                if (argc != 3) {
                    Jim_WrongNumArgs(interp, 2, argv, "string");
                    return JIM_ERR;
                }

                str = Jim_GetString(argv[2], &len);
                buf = Jim_Alloc(len + 1);
                p = buf + len;
                *p = 0;
                for (i = 0; i < len; ) {
                    int c;
                    int l = utf8_tounicode(str, &c);
                    memcpy(p - l, str, l);
                    p -= l;
                    i += l;
                    str += l;
                }
                Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, buf, len));
                return JIM_OK;
            }

        case OPT_INDEX:{
                int idx;
                const char *str;

                if (argc != 4) {
                    Jim_WrongNumArgs(interp, 2, argv, "string index");
                    return JIM_ERR;
                }
                if (Jim_GetIndex(interp, argv[3], &idx) != JIM_OK) {
                    return JIM_ERR;
                }
                str = Jim_String(argv[2]);
                len = Jim_Utf8Length(interp, argv[2]);
                if (idx != INT_MIN && idx != INT_MAX) {
                    idx = JimRelToAbsIndex(len, idx);
                }
                if (idx < 0 || idx >= len || str == NULL) {
                    Jim_SetResultString(interp, "", 0);
                }
                else if (len == Jim_Length(argv[2])) {
                    /* ASCII optimisation */
                    Jim_SetResultString(interp, str + idx, 1);
                }
                else {
                    int c;
                    int i = utf8_index(str, idx);
                    Jim_SetResultString(interp, str + i, utf8_tounicode(str + i, &c));
                }
                return JIM_OK;
            }

        case OPT_FIRST:
        case OPT_LAST:{
                int idx = 0, l1, l2;
                const char *s1, *s2;

                if (argc != 4 && argc != 5) {
                    Jim_WrongNumArgs(interp, 2, argv, "subString string ?index?");
                    return JIM_ERR;
                }
                s1 = Jim_String(argv[2]);
                s2 = Jim_String(argv[3]);
                l1 = Jim_Utf8Length(interp, argv[2]);
                l2 = Jim_Utf8Length(interp, argv[3]);
                if (argc == 5) {
                    if (Jim_GetIndex(interp, argv[4], &idx) != JIM_OK) {
                        return JIM_ERR;
                    }
                    idx = JimRelToAbsIndex(l2, idx);
                }
                else if (option == OPT_LAST) {
                    idx = l2;
                }
                if (option == OPT_FIRST) {
                    Jim_SetResultInt(interp, JimStringFirst(s1, l1, s2, l2, idx));
                }
                else {
#ifdef JIM_UTF8
                    Jim_SetResultInt(interp, JimStringLastUtf8(s1, l1, s2, idx));
#else
                    Jim_SetResultInt(interp, JimStringLast(s1, l1, s2, idx));
#endif
                }
                return JIM_OK;
            }

        case OPT_TRIM:
        case OPT_TRIMLEFT:
        case OPT_TRIMRIGHT:{
                Jim_Obj *trimchars;

                if (argc != 3 && argc != 4) {
                    Jim_WrongNumArgs(interp, 2, argv, "string ?trimchars?");
                    return JIM_ERR;
                }
                trimchars = (argc == 4 ? argv[3] : NULL);
                if (option == OPT_TRIM) {
                    Jim_SetResult(interp, JimStringTrim(interp, argv[2], trimchars));
                }
                else if (option == OPT_TRIMLEFT) {
                    Jim_SetResult(interp, JimStringTrimLeft(interp, argv[2], trimchars));
                }
                else if (option == OPT_TRIMRIGHT) {
                    Jim_SetResult(interp, JimStringTrimRight(interp, argv[2], trimchars));
                }
                return JIM_OK;
            }

        case OPT_TOLOWER:
        case OPT_TOUPPER:
        case OPT_TOTITLE:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "string");
                return JIM_ERR;
            }
            if (option == OPT_TOLOWER) {
                Jim_SetResult(interp, JimStringToLower(interp, argv[2]));
            }
            else if (option == OPT_TOUPPER) {
                Jim_SetResult(interp, JimStringToUpper(interp, argv[2]));
            }
            else {
                Jim_SetResult(interp, JimStringToTitle(interp, argv[2]));
            }
            return JIM_OK;

        case OPT_IS:
            if (argc == 4 || (argc == 5 && Jim_CompareStringImmediate(interp, argv[3], "-strict"))) {
                return JimStringIs(interp, argv[argc - 1], argv[2], argc == 5);
            }
            Jim_WrongNumArgs(interp, 2, argv, "class ?-strict? str");
            return JIM_ERR;
    }
    return JIM_OK;
}

/* [time] */
static int Jim_TimeCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long i, count = 1;
    jim_wide start, elapsed;
    char buf[60];
    const char *fmt = "%" JIM_WIDE_MODIFIER " microseconds per iteration";

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "script ?count?");
        return JIM_ERR;
    }
    if (argc == 3) {
        if (Jim_GetLong(interp, argv[2], &count) != JIM_OK)
            return JIM_ERR;
    }
    if (count < 0)
        return JIM_OK;
    i = count;
    start = JimClock();
    while (i-- > 0) {
        int retval;

        retval = Jim_EvalObj(interp, argv[1]);
        if (retval != JIM_OK) {
            return retval;
        }
    }
    elapsed = JimClock() - start;
    sprintf(buf, fmt, count == 0 ? 0 : elapsed / count);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

/* [exit] */
static int Jim_ExitCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long exitCode = 0;

    if (argc > 2) {
        Jim_WrongNumArgs(interp, 1, argv, "?exitCode?");
        return JIM_ERR;
    }
    if (argc == 2) {
        if (Jim_GetLong(interp, argv[1], &exitCode) != JIM_OK)
            return JIM_ERR;
    }
    interp->exitCode = exitCode;
    return JIM_EXIT;
}

/* [catch] */
static int Jim_CatchCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int exitCode = 0;
    int i;
    int sig = 0;

    /* Which return codes are ignored (passed through)? By default, only exit, eval and signal */
    jim_wide ignore_mask = (1 << JIM_EXIT) | (1 << JIM_EVAL) | (1 << JIM_SIGNAL);
    static const int max_ignore_code = sizeof(ignore_mask) * 8;

    /* Reset the error code before catch.
     * Note that this is not strictly correct.
     */
    Jim_SetGlobalVariableStr(interp, "errorCode", Jim_NewStringObj(interp, "NONE", -1));

    for (i = 1; i < argc - 1; i++) {
        const char *arg = Jim_String(argv[i]);
        jim_wide option;
        int ignore;

        /* It's a pity we can't use Jim_GetEnum here :-( */
        if (strcmp(arg, "--") == 0) {
            i++;
            break;
        }
        if (*arg != '-') {
            break;
        }

        if (strncmp(arg, "-no", 3) == 0) {
            arg += 3;
            ignore = 1;
        }
        else {
            arg++;
            ignore = 0;
        }

        if (Jim_StringToWide(arg, &option, 10) != JIM_OK) {
            option = -1;
        }
        if (option < 0) {
            option = Jim_FindByName(arg, jimReturnCodes, jimReturnCodesSize);
        }
        if (option < 0) {
            goto wrongargs;
        }

        if (ignore) {
            ignore_mask |= ((jim_wide)1 << option);
        }
        else {
            ignore_mask &= (~((jim_wide)1 << option));
        }
    }

    argc -= i;
    if (argc < 1 || argc > 3) {
      wrongargs:
        Jim_WrongNumArgs(interp, 1, argv,
            "?-?no?code ... --? script ?resultVarName? ?optionVarName?");
        return JIM_ERR;
    }
    argv += i;

    if ((ignore_mask & (1 << JIM_SIGNAL)) == 0) {
        sig++;
    }

    interp->signal_level += sig;
    if (Jim_CheckSignal(interp)) {
        /* If a signal is set, don't even try to execute the body */
        exitCode = JIM_SIGNAL;
    }
    else {
        exitCode = Jim_EvalObj(interp, argv[0]);
        /* Don't want any caught error included in a later stack trace */
        interp->errorFlag = 0;
    }
    interp->signal_level -= sig;

    /* Catch or pass through? Only the first 32/64 codes can be passed through */
    if (exitCode >= 0 && exitCode < max_ignore_code && (((unsigned jim_wide)1 << exitCode) & ignore_mask)) {
        /* Not caught, pass it up */
        return exitCode;
    }

    if (sig && exitCode == JIM_SIGNAL) {
        /* Catch the signal at this level */
        if (interp->signal_set_result) {
            interp->signal_set_result(interp, interp->sigmask);
        }
        else {
            Jim_SetResultInt(interp, interp->sigmask);
        }
        interp->sigmask = 0;
    }

    if (argc >= 2) {
        if (Jim_SetVariable(interp, argv[1], Jim_GetResult(interp)) != JIM_OK) {
            return JIM_ERR;
        }
        if (argc == 3) {
            Jim_Obj *optListObj = Jim_NewListObj(interp, NULL, 0);

            Jim_ListAppendElement(interp, optListObj, Jim_NewStringObj(interp, "-code", -1));
            Jim_ListAppendElement(interp, optListObj,
                Jim_NewIntObj(interp, exitCode == JIM_RETURN ? interp->returnCode : exitCode));
            Jim_ListAppendElement(interp, optListObj, Jim_NewStringObj(interp, "-level", -1));
            Jim_ListAppendElement(interp, optListObj, Jim_NewIntObj(interp, interp->returnLevel));
            if (exitCode == JIM_ERR) {
                Jim_Obj *errorCode;
                Jim_ListAppendElement(interp, optListObj, Jim_NewStringObj(interp, "-errorinfo",
                    -1));
                Jim_ListAppendElement(interp, optListObj, interp->stackTrace);

                errorCode = Jim_GetGlobalVariableStr(interp, "errorCode", JIM_NONE);
                if (errorCode) {
                    Jim_ListAppendElement(interp, optListObj, Jim_NewStringObj(interp, "-errorcode", -1));
                    Jim_ListAppendElement(interp, optListObj, errorCode);
                }
            }
            if (Jim_SetVariable(interp, argv[2], optListObj) != JIM_OK) {
                return JIM_ERR;
            }
        }
    }
    Jim_SetResultInt(interp, exitCode);
    return JIM_OK;
}

#ifdef JIM_REFERENCES

/* [ref] */
static int Jim_RefCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 3 && argc != 4) {
        Jim_WrongNumArgs(interp, 1, argv, "string tag ?finalizer?");
        return JIM_ERR;
    }
    if (argc == 3) {
        Jim_SetResult(interp, Jim_NewReference(interp, argv[1], argv[2], NULL));
    }
    else {
        Jim_SetResult(interp, Jim_NewReference(interp, argv[1], argv[2], argv[3]));
    }
    return JIM_OK;
}

/* [getref] */
static int Jim_GetrefCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Reference *refPtr;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "reference");
        return JIM_ERR;
    }
    if ((refPtr = Jim_GetReference(interp, argv[1])) == NULL)
        return JIM_ERR;
    Jim_SetResult(interp, refPtr->objPtr);
    return JIM_OK;
}

/* [setref] */
static int Jim_SetrefCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Reference *refPtr;

    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "reference newValue");
        return JIM_ERR;
    }
    if ((refPtr = Jim_GetReference(interp, argv[1])) == NULL)
        return JIM_ERR;
    Jim_IncrRefCount(argv[2]);
    Jim_DecrRefCount(interp, refPtr->objPtr);
    refPtr->objPtr = argv[2];
    Jim_SetResult(interp, argv[2]);
    return JIM_OK;
}

/* [collect] */
static int Jim_CollectCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, Jim_Collect(interp));

    /* Free all the freed objects. */
    while (interp->freeList) {
        Jim_Obj *nextObjPtr = interp->freeList->nextObjPtr;
        Jim_Free(interp->freeList);
        interp->freeList = nextObjPtr;
    }

    return JIM_OK;
}

/* [finalize] reference ?newValue? */
static int Jim_FinalizeCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "reference ?finalizerProc?");
        return JIM_ERR;
    }
    if (argc == 2) {
        Jim_Obj *cmdNamePtr;

        if (Jim_GetFinalizer(interp, argv[1], &cmdNamePtr) != JIM_OK)
            return JIM_ERR;
        if (cmdNamePtr != NULL) /* otherwise the null string is returned. */
            Jim_SetResult(interp, cmdNamePtr);
    }
    else {
        if (Jim_SetFinalizer(interp, argv[1], argv[2]) != JIM_OK)
            return JIM_ERR;
        Jim_SetResult(interp, argv[2]);
    }
    return JIM_OK;
}

/* [info references] */
static int JimInfoReferences(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *listObjPtr;
    Jim_HashTableIterator htiter;
    Jim_HashEntry *he;

    listObjPtr = Jim_NewListObj(interp, NULL, 0);

    JimInitHashTableIterator(&interp->references, &htiter);
    while ((he = Jim_NextHashEntry(&htiter)) != NULL) {
        char buf[JIM_REFERENCE_SPACE + 1];
        Jim_Reference *refPtr = Jim_GetHashEntryVal(he);
        const unsigned long *refId = he->key;

        JimFormatReference(buf, refPtr, *refId);
        Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, buf, -1));
    }
    Jim_SetResult(interp, listObjPtr);
    return JIM_OK;
}
#endif

/* [rename] */
static int Jim_RenameCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "oldName newName");
        return JIM_ERR;
    }

    if (JimValidName(interp, "new procedure", argv[2])) {
        return JIM_ERR;
    }

    return Jim_RenameCommand(interp, Jim_String(argv[1]), Jim_String(argv[2]));
}

#define JIM_DICTMATCH_VALUES 0x0001

typedef void JimDictMatchCallbackType(Jim_Interp *interp, Jim_Obj *listObjPtr, Jim_HashEntry *he, int type);

static void JimDictMatchKeys(Jim_Interp *interp, Jim_Obj *listObjPtr, Jim_HashEntry *he, int type)
{
    Jim_ListAppendElement(interp, listObjPtr, (Jim_Obj *)he->key);
    if (type & JIM_DICTMATCH_VALUES) {
        Jim_ListAppendElement(interp, listObjPtr, Jim_GetHashEntryVal(he));
    }
}

/**
 * Like JimHashtablePatternMatch, but for dictionaries.
 */
static Jim_Obj *JimDictPatternMatch(Jim_Interp *interp, Jim_HashTable *ht, Jim_Obj *patternObjPtr,
    JimDictMatchCallbackType *callback, int type)
{
    Jim_HashEntry *he;
    Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);

    /* Check for the non-pattern case. We can do this much more efficiently. */
    Jim_HashTableIterator htiter;
    JimInitHashTableIterator(ht, &htiter);
    while ((he = Jim_NextHashEntry(&htiter)) != NULL) {
        if (patternObjPtr == NULL || JimGlobMatch(Jim_String(patternObjPtr), Jim_String((Jim_Obj *)he->key), 0)) {
            callback(interp, listObjPtr, he, type);
        }
    }

    return listObjPtr;
}


int Jim_DictKeys(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *patternObjPtr)
{
    if (SetDictFromAny(interp, objPtr) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResult(interp, JimDictPatternMatch(interp, objPtr->internalRep.ptr, patternObjPtr, JimDictMatchKeys, 0));
    return JIM_OK;
}

int Jim_DictValues(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *patternObjPtr)
{
    if (SetDictFromAny(interp, objPtr) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResult(interp, JimDictPatternMatch(interp, objPtr->internalRep.ptr, patternObjPtr, JimDictMatchKeys, JIM_DICTMATCH_VALUES));
    return JIM_OK;
}

int Jim_DictSize(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (SetDictFromAny(interp, objPtr) != JIM_OK) {
        return -1;
    }
    return ((Jim_HashTable *)objPtr->internalRep.ptr)->used;
}

int Jim_DictInfo(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_HashTable *ht;
    unsigned int i;

    if (SetDictFromAny(interp, objPtr) != JIM_OK) {
        return JIM_ERR;
    }

    ht = (Jim_HashTable *)objPtr->internalRep.ptr;

    /* Note that this uses internal knowledge of the hash table */
    printf("%d entries in table, %d buckets\n", ht->used, ht->size);

    for (i = 0; i < ht->size; i++) {
        Jim_HashEntry *he = ht->table[i];

        if (he) {
            printf("%d: ", i);

            while (he) {
                printf(" %s", Jim_String(he->key));
                he = he->next;
            }
            printf("\n");
        }
    }
    return JIM_OK;
}

static int Jim_EvalEnsemble(Jim_Interp *interp, const char *basecmd, const char *subcmd, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *prefixObj = Jim_NewStringObj(interp, basecmd, -1);

    Jim_AppendString(interp, prefixObj, " ", 1);
    Jim_AppendString(interp, prefixObj, subcmd, -1);

    return Jim_EvalObjPrefix(interp, prefixObj, argc, argv);
}

/* [dict] */
static int Jim_DictCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    int option;
    static const char * const options[] = {
        "create", "get", "set", "unset", "exists", "keys", "size", "info",
        "merge", "with", "append", "lappend", "incr", "remove", "values", "for",
        "replace", "update", NULL
    };
    enum
    {
        OPT_CREATE, OPT_GET, OPT_SET, OPT_UNSET, OPT_EXISTS, OPT_KEYS, OPT_SIZE, OPT_INFO,
        OPT_MERGE, OPT_WITH, OPT_APPEND, OPT_LAPPEND, OPT_INCR, OPT_REMOVE, OPT_VALUES, OPT_FOR,
        OPT_REPLACE, OPT_UPDATE,
    };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "subcommand ?arguments ...?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], options, &option, "subcommand", JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    switch (option) {
        case OPT_GET:
            if (argc < 3) {
                Jim_WrongNumArgs(interp, 2, argv, "dictionary ?key ...?");
                return JIM_ERR;
            }
            if (Jim_DictKeysVector(interp, argv[2], argv + 3, argc - 3, &objPtr,
                    JIM_ERRMSG) != JIM_OK) {
                return JIM_ERR;
            }
            Jim_SetResult(interp, objPtr);
            return JIM_OK;

        case OPT_SET:
            if (argc < 5) {
                Jim_WrongNumArgs(interp, 2, argv, "varName key ?key ...? value");
                return JIM_ERR;
            }
            return Jim_SetDictKeysVector(interp, argv[2], argv + 3, argc - 4, argv[argc - 1], JIM_ERRMSG);

        case OPT_EXISTS:
            if (argc < 4) {
                Jim_WrongNumArgs(interp, 2, argv, "dictionary key ?key ...?");
                return JIM_ERR;
            }
            else {
                int rc = Jim_DictKeysVector(interp, argv[2], argv + 3, argc - 3, &objPtr, JIM_ERRMSG);
                if (rc < 0) {
                    return JIM_ERR;
                }
                Jim_SetResultBool(interp,  rc == JIM_OK);
                return JIM_OK;
            }

        case OPT_UNSET:
            if (argc < 4) {
                Jim_WrongNumArgs(interp, 2, argv, "varName key ?key ...?");
                return JIM_ERR;
            }
            if (Jim_SetDictKeysVector(interp, argv[2], argv + 3, argc - 3, NULL, 0) != JIM_OK) {
                return JIM_ERR;
            }
            return JIM_OK;

        case OPT_KEYS:
            if (argc != 3 && argc != 4) {
                Jim_WrongNumArgs(interp, 2, argv, "dictionary ?pattern?");
                return JIM_ERR;
            }
            return Jim_DictKeys(interp, argv[2], argc == 4 ? argv[3] : NULL);

        case OPT_SIZE:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "dictionary");
                return JIM_ERR;
            }
            else if (Jim_DictSize(interp, argv[2]) < 0) {
                return JIM_ERR;
            }
            Jim_SetResultInt(interp, Jim_DictSize(interp, argv[2]));
            return JIM_OK;

        case OPT_MERGE:
            if (argc == 2) {
                return JIM_OK;
            }
            if (Jim_DictSize(interp, argv[2]) < 0) {
                return JIM_ERR;
            }
            /* Handle as ensemble */
            break;

        case OPT_UPDATE:
            if (argc < 6 || argc % 2) {
                /* Better error message */
                argc = 2;
            }
            break;

        case OPT_CREATE:
            if (argc % 2) {
                Jim_WrongNumArgs(interp, 2, argv, "?key value ...?");
                return JIM_ERR;
            }
            objPtr = Jim_NewDictObj(interp, argv + 2, argc - 2);
            Jim_SetResult(interp, objPtr);
            return JIM_OK;

        case OPT_INFO:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "dictionary");
                return JIM_ERR;
            }
            return Jim_DictInfo(interp, argv[2]);
    }
    /* Handle command as an ensemble */
    return Jim_EvalEnsemble(interp, "dict", options[option], argc - 2, argv + 2);
}

/* [subst] */
static int Jim_SubstCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    static const char * const options[] = {
        "-nobackslashes", "-nocommands", "-novariables", NULL
    };
    enum
    { OPT_NOBACKSLASHES, OPT_NOCOMMANDS, OPT_NOVARIABLES };
    int i;
    int flags = JIM_SUBST_FLAG;
    Jim_Obj *objPtr;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "?options? string");
        return JIM_ERR;
    }
    for (i = 1; i < (argc - 1); i++) {
        int option;

        if (Jim_GetEnum(interp, argv[i], options, &option, NULL,
                JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }
        switch (option) {
            case OPT_NOBACKSLASHES:
                flags |= JIM_SUBST_NOESC;
                break;
            case OPT_NOCOMMANDS:
                flags |= JIM_SUBST_NOCMD;
                break;
            case OPT_NOVARIABLES:
                flags |= JIM_SUBST_NOVAR;
                break;
        }
    }
    if (Jim_SubstObj(interp, argv[argc - 1], &objPtr, flags) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

/* [info] */
static int Jim_InfoCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int cmd;
    Jim_Obj *objPtr;
    int mode = 0;

    static const char * const commands[] = {
        "body", "statics", "commands", "procs", "channels", "exists", "globals", "level", "frame", "locals",
        "vars", "version", "patchlevel", "complete", "args", "hostname",
        "script", "source", "stacktrace", "nameofexecutable", "returncodes",
        "references", "alias", NULL
    };
    enum
    { INFO_BODY, INFO_STATICS, INFO_COMMANDS, INFO_PROCS, INFO_CHANNELS, INFO_EXISTS, INFO_GLOBALS, INFO_LEVEL,
        INFO_FRAME, INFO_LOCALS, INFO_VARS, INFO_VERSION, INFO_PATCHLEVEL, INFO_COMPLETE, INFO_ARGS,
        INFO_HOSTNAME, INFO_SCRIPT, INFO_SOURCE, INFO_STACKTRACE, INFO_NAMEOFEXECUTABLE,
        INFO_RETURNCODES, INFO_REFERENCES, INFO_ALIAS,
    };

#ifdef jim_ext_namespace
    int nons = 0;

    if (argc > 2 && Jim_CompareStringImmediate(interp, argv[1], "-nons")) {
        /* This is for internal use only */
        argc--;
        argv++;
        nons = 1;
    }
#endif

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "subcommand ?args ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], commands, &cmd, "subcommand", JIM_ERRMSG | JIM_ENUM_ABBREV)
        != JIM_OK) {
        return JIM_ERR;
    }

    /* Test for the most common commands first, just in case it makes a difference */
    switch (cmd) {
        case INFO_EXISTS:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "varName");
                return JIM_ERR;
            }
            Jim_SetResultBool(interp, Jim_GetVariable(interp, argv[2], 0) != NULL);
            break;

        case INFO_ALIAS:{
            Jim_Cmd *cmdPtr;

            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "command");
                return JIM_ERR;
            }
            if ((cmdPtr = Jim_GetCommand(interp, argv[2], JIM_ERRMSG)) == NULL) {
                return JIM_ERR;
            }
            if (cmdPtr->isproc || cmdPtr->u.native.cmdProc != JimAliasCmd) {
                Jim_SetResultFormatted(interp, "command \"%#s\" is not an alias", argv[2]);
                return JIM_ERR;
            }
            Jim_SetResult(interp, (Jim_Obj *)cmdPtr->u.native.privData);
            return JIM_OK;
        }

        case INFO_CHANNELS:
            mode++;             /* JIM_CMDLIST_CHANNELS */
#ifndef jim_ext_aio
            Jim_SetResultString(interp, "aio not enabled", -1);
            return JIM_ERR;
#endif
            /* fall through */
        case INFO_PROCS:
            mode++;             /* JIM_CMDLIST_PROCS */
            /* fall through */
        case INFO_COMMANDS:
            /* mode 0 => JIM_CMDLIST_COMMANDS */
            if (argc != 2 && argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "?pattern?");
                return JIM_ERR;
            }
#ifdef jim_ext_namespace
            if (!nons) {
                if (Jim_Length(interp->framePtr->nsObj) || (argc == 3 && JimGlobMatch("::*", Jim_String(argv[2]), 0))) {
                    return Jim_EvalPrefix(interp, "namespace info", argc - 1, argv + 1);
                }
            }
#endif
            Jim_SetResult(interp, JimCommandsList(interp, (argc == 3) ? argv[2] : NULL, mode));
            break;

        case INFO_VARS:
            mode++;             /* JIM_VARLIST_VARS */
            /* fall through */
        case INFO_LOCALS:
            mode++;             /* JIM_VARLIST_LOCALS */
            /* fall through */
        case INFO_GLOBALS:
            /* mode 0 => JIM_VARLIST_GLOBALS */
            if (argc != 2 && argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "?pattern?");
                return JIM_ERR;
            }
#ifdef jim_ext_namespace
            if (!nons) {
                if (Jim_Length(interp->framePtr->nsObj) || (argc == 3 && JimGlobMatch("::*", Jim_String(argv[2]), 0))) {
                    return Jim_EvalPrefix(interp, "namespace info", argc - 1, argv + 1);
                }
            }
#endif
            Jim_SetResult(interp, JimVariablesList(interp, argc == 3 ? argv[2] : NULL, mode));
            break;

        case INFO_SCRIPT:
            if (argc != 2) {
                Jim_WrongNumArgs(interp, 2, argv, "");
                return JIM_ERR;
            }
            Jim_SetResult(interp, JimGetScript(interp, interp->currentScriptObj)->fileNameObj);
            break;

        case INFO_SOURCE:{
                jim_wide line;
                Jim_Obj *resObjPtr;
                Jim_Obj *fileNameObj;

                if (argc != 3 && argc != 5) {
                    Jim_WrongNumArgs(interp, 2, argv, "source ?filename line?");
                    return JIM_ERR;
                }
                if (argc == 5) {
                    if (Jim_GetWide(interp, argv[4], &line) != JIM_OK) {
                        return JIM_ERR;
                    }
                    resObjPtr = Jim_NewStringObj(interp, Jim_String(argv[2]), Jim_Length(argv[2]));
                    JimSetSourceInfo(interp, resObjPtr, argv[3], line);
                }
                else {
                    if (argv[2]->typePtr == &sourceObjType) {
                        fileNameObj = argv[2]->internalRep.sourceValue.fileNameObj;
                        line = argv[2]->internalRep.sourceValue.lineNumber;
                    }
                    else if (argv[2]->typePtr == &scriptObjType) {
                        ScriptObj *script = JimGetScript(interp, argv[2]);
                        fileNameObj = script->fileNameObj;
                        line = script->firstline;
                    }
                    else {
                        fileNameObj = interp->emptyObj;
                        line = 1;
                    }
                    resObjPtr = Jim_NewListObj(interp, NULL, 0);
                    Jim_ListAppendElement(interp, resObjPtr, fileNameObj);
                    Jim_ListAppendElement(interp, resObjPtr, Jim_NewIntObj(interp, line));
                }
                Jim_SetResult(interp, resObjPtr);
                break;
            }

        case INFO_STACKTRACE:
            Jim_SetResult(interp, interp->stackTrace);
            break;

        case INFO_LEVEL:
        case INFO_FRAME:
            switch (argc) {
                case 2:
                    Jim_SetResultInt(interp, interp->framePtr->level);
                    break;

                case 3:
                    if (JimInfoLevel(interp, argv[2], &objPtr, cmd == INFO_LEVEL) != JIM_OK) {
                        return JIM_ERR;
                    }
                    Jim_SetResult(interp, objPtr);
                    break;

                default:
                    Jim_WrongNumArgs(interp, 2, argv, "?levelNum?");
                    return JIM_ERR;
            }
            break;

        case INFO_BODY:
        case INFO_STATICS:
        case INFO_ARGS:{
                Jim_Cmd *cmdPtr;

                if (argc != 3) {
                    Jim_WrongNumArgs(interp, 2, argv, "procname");
                    return JIM_ERR;
                }
                if ((cmdPtr = Jim_GetCommand(interp, argv[2], JIM_ERRMSG)) == NULL) {
                    return JIM_ERR;
                }
                if (!cmdPtr->isproc) {
                    Jim_SetResultFormatted(interp, "command \"%#s\" is not a procedure", argv[2]);
                    return JIM_ERR;
                }
                switch (cmd) {
                    case INFO_BODY:
                        Jim_SetResult(interp, cmdPtr->u.proc.bodyObjPtr);
                        break;
                    case INFO_ARGS:
                        Jim_SetResult(interp, cmdPtr->u.proc.argListObjPtr);
                        break;
                    case INFO_STATICS:
                        if (cmdPtr->u.proc.staticVars) {
                            int mode = JIM_VARLIST_LOCALS | JIM_VARLIST_VALUES;
                            Jim_SetResult(interp, JimHashtablePatternMatch(interp, cmdPtr->u.proc.staticVars,
                                NULL, JimVariablesMatch, mode));
                        }
                        break;
                }
                break;
            }

        case INFO_VERSION:
        case INFO_PATCHLEVEL:{
                char buf[(JIM_INTEGER_SPACE * 2) + 1];

                sprintf(buf, "%d.%d", JIM_VERSION / 100, JIM_VERSION % 100);
                Jim_SetResultString(interp, buf, -1);
                break;
            }

        case INFO_COMPLETE:
            if (argc != 3 && argc != 4) {
                Jim_WrongNumArgs(interp, 2, argv, "script ?missing?");
                return JIM_ERR;
            }
            else {
                char missing;

                Jim_SetResultBool(interp, Jim_ScriptIsComplete(interp, argv[2], &missing));
                if (missing != ' ' && argc == 4) {
                    Jim_SetVariable(interp, argv[3], Jim_NewStringObj(interp, &missing, 1));
                }
            }
            break;

        case INFO_HOSTNAME:
            /* Redirect to os.gethostname if it exists */
            return Jim_Eval(interp, "os.gethostname");

        case INFO_NAMEOFEXECUTABLE:
            /* Redirect to Tcl proc */
            return Jim_Eval(interp, "{info nameofexecutable}");

        case INFO_RETURNCODES:
            if (argc == 2) {
                int i;
                Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);

                for (i = 0; jimReturnCodes[i]; i++) {
                    Jim_ListAppendElement(interp, listObjPtr, Jim_NewIntObj(interp, i));
                    Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp,
                            jimReturnCodes[i], -1));
                }

                Jim_SetResult(interp, listObjPtr);
            }
            else if (argc == 3) {
                long code;
                const char *name;

                if (Jim_GetLong(interp, argv[2], &code) != JIM_OK) {
                    return JIM_ERR;
                }
                name = Jim_ReturnCode(code);
                if (*name == '?') {
                    Jim_SetResultInt(interp, code);
                }
                else {
                    Jim_SetResultString(interp, name, -1);
                }
            }
            else {
                Jim_WrongNumArgs(interp, 2, argv, "?code?");
                return JIM_ERR;
            }
            break;
        case INFO_REFERENCES:
#ifdef JIM_REFERENCES
            return JimInfoReferences(interp, argc, argv);
#else
            Jim_SetResultString(interp, "not supported", -1);
            return JIM_ERR;
#endif
    }
    return JIM_OK;
}

/* [exists] */
static int Jim_ExistsCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    int result = 0;

    static const char * const options[] = {
        "-command", "-proc", "-alias", "-var", NULL
    };
    enum
    {
        OPT_COMMAND, OPT_PROC, OPT_ALIAS, OPT_VAR
    };
    int option;

    if (argc == 2) {
        option = OPT_VAR;
        objPtr = argv[1];
    }
    else if (argc == 3) {
        if (Jim_GetEnum(interp, argv[1], options, &option, NULL, JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }
        objPtr = argv[2];
    }
    else {
        Jim_WrongNumArgs(interp, 1, argv, "?option? name");
        return JIM_ERR;
    }

    if (option == OPT_VAR) {
        result = Jim_GetVariable(interp, objPtr, 0) != NULL;
    }
    else {
        /* Now different kinds of commands */
        Jim_Cmd *cmd = Jim_GetCommand(interp, objPtr, JIM_NONE);

        if (cmd) {
            switch (option) {
            case OPT_COMMAND:
                result = 1;
                break;

            case OPT_ALIAS:
                result = cmd->isproc == 0 && cmd->u.native.cmdProc == JimAliasCmd;
                break;

            case OPT_PROC:
                result = cmd->isproc;
                break;
            }
        }
    }
    Jim_SetResultBool(interp, result);
    return JIM_OK;
}

/* [split] */
static int Jim_SplitCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *str, *splitChars, *noMatchStart;
    int splitLen, strLen;
    Jim_Obj *resObjPtr;
    int c;
    int len;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "string ?splitChars?");
        return JIM_ERR;
    }

    str = Jim_GetString(argv[1], &len);
    if (len == 0) {
        return JIM_OK;
    }
    strLen = Jim_Utf8Length(interp, argv[1]);

    /* Init */
    if (argc == 2) {
        splitChars = " \n\t\r";
        splitLen = 4;
    }
    else {
        splitChars = Jim_String(argv[2]);
        splitLen = Jim_Utf8Length(interp, argv[2]);
    }

    noMatchStart = str;
    resObjPtr = Jim_NewListObj(interp, NULL, 0);

    /* Split */
    if (splitLen) {
        Jim_Obj *objPtr;
        while (strLen--) {
            const char *sc = splitChars;
            int scLen = splitLen;
            int sl = utf8_tounicode(str, &c);
            while (scLen--) {
                int pc;
                sc += utf8_tounicode(sc, &pc);
                if (c == pc) {
                    objPtr = Jim_NewStringObj(interp, noMatchStart, (str - noMatchStart));
                    Jim_ListAppendElement(interp, resObjPtr, objPtr);
                    noMatchStart = str + sl;
                    break;
                }
            }
            str += sl;
        }
        objPtr = Jim_NewStringObj(interp, noMatchStart, (str - noMatchStart));
        Jim_ListAppendElement(interp, resObjPtr, objPtr);
    }
    else {
        /* This handles the special case of splitchars eq {}
         * Optimise by sharing common (ASCII) characters
         */
        Jim_Obj **commonObj = NULL;
#define NUM_COMMON (128 - 9)
        while (strLen--) {
            int n = utf8_tounicode(str, &c);
#ifdef JIM_OPTIMIZATION
            if (c >= 9 && c < 128) {
                /* Common ASCII char. Note that 9 is the tab character */
                c -= 9;
                if (!commonObj) {
                    commonObj = Jim_Alloc(sizeof(*commonObj) * NUM_COMMON);
                    memset(commonObj, 0, sizeof(*commonObj) * NUM_COMMON);
                }
                if (!commonObj[c]) {
                    commonObj[c] = Jim_NewStringObj(interp, str, 1);
                }
                Jim_ListAppendElement(interp, resObjPtr, commonObj[c]);
                str++;
                continue;
            }
#endif
            Jim_ListAppendElement(interp, resObjPtr, Jim_NewStringObjUtf8(interp, str, 1));
            str += n;
        }
        Jim_Free(commonObj);
    }

    Jim_SetResult(interp, resObjPtr);
    return JIM_OK;
}

/* [join] */
static int Jim_JoinCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *joinStr;
    int joinStrLen;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "list ?joinString?");
        return JIM_ERR;
    }
    /* Init */
    if (argc == 2) {
        joinStr = " ";
        joinStrLen = 1;
    }
    else {
        joinStr = Jim_GetString(argv[2], &joinStrLen);
    }
    Jim_SetResult(interp, Jim_ListJoin(interp, argv[1], joinStr, joinStrLen));
    return JIM_OK;
}

/* [format] */
static int Jim_FormatCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "formatString ?arg arg ...?");
        return JIM_ERR;
    }
    objPtr = Jim_FormatString(interp, argv[1], argc - 2, argv + 2);
    if (objPtr == NULL)
        return JIM_ERR;
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

/* [scan] */
static int Jim_ScanCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *listPtr, **outVec;
    int outc, i;

    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "string format ?varName varName ...?");
        return JIM_ERR;
    }
    if (argv[2]->typePtr != &scanFmtStringObjType)
        SetScanFmtFromAny(interp, argv[2]);
    if (FormatGetError(argv[2]) != 0) {
        Jim_SetResultString(interp, FormatGetError(argv[2]), -1);
        return JIM_ERR;
    }
    if (argc > 3) {
        int maxPos = FormatGetMaxPos(argv[2]);
        int count = FormatGetCnvCount(argv[2]);

        if (maxPos > argc - 3) {
            Jim_SetResultString(interp, "\"%n$\" argument index out of range", -1);
            return JIM_ERR;
        }
        else if (count > argc - 3) {
            Jim_SetResultString(interp, "different numbers of variable names and "
                "field specifiers", -1);
            return JIM_ERR;
        }
        else if (count < argc - 3) {
            Jim_SetResultString(interp, "variable is not assigned by any "
                "conversion specifiers", -1);
            return JIM_ERR;
        }
    }
    listPtr = Jim_ScanString(interp, argv[1], argv[2], JIM_ERRMSG);
    if (listPtr == 0)
        return JIM_ERR;
    if (argc > 3) {
        int rc = JIM_OK;
        int count = 0;

        if (listPtr != 0 && listPtr != (Jim_Obj *)EOF) {
            int len = Jim_ListLength(interp, listPtr);

            if (len != 0) {
                JimListGetElements(interp, listPtr, &outc, &outVec);
                for (i = 0; i < outc; ++i) {
                    if (Jim_Length(outVec[i]) > 0) {
                        ++count;
                        if (Jim_SetVariable(interp, argv[3 + i], outVec[i]) != JIM_OK) {
                            rc = JIM_ERR;
                        }
                    }
                }
            }
            Jim_FreeNewObj(interp, listPtr);
        }
        else {
            count = -1;
        }
        if (rc == JIM_OK) {
            Jim_SetResultInt(interp, count);
        }
        return rc;
    }
    else {
        if (listPtr == (Jim_Obj *)EOF) {
            Jim_SetResult(interp, Jim_NewListObj(interp, 0, 0));
            return JIM_OK;
        }
        Jim_SetResult(interp, listPtr);
    }
    return JIM_OK;
}

/* [error] */
static int Jim_ErrorCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "message ?stacktrace?");
        return JIM_ERR;
    }
    Jim_SetResult(interp, argv[1]);
    if (argc == 3) {
        JimSetStackTrace(interp, argv[2]);
        return JIM_ERR;
    }
    interp->addStackTrace++;
    return JIM_ERR;
}

/* [lrange] */
static int Jim_LrangeCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;

    if (argc != 4) {
        Jim_WrongNumArgs(interp, 1, argv, "list first last");
        return JIM_ERR;
    }
    if ((objPtr = Jim_ListRange(interp, argv[1], argv[2], argv[3])) == NULL)
        return JIM_ERR;
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

/* [lrepeat] */
static int Jim_LrepeatCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    long count;

    if (argc < 2 || Jim_GetLong(interp, argv[1], &count) != JIM_OK || count < 0) {
        Jim_WrongNumArgs(interp, 1, argv, "count ?value ...?");
        return JIM_ERR;
    }

    if (count == 0 || argc == 2) {
        return JIM_OK;
    }

    argc -= 2;
    argv += 2;

    objPtr = Jim_NewListObj(interp, argv, argc);
    while (--count) {
        ListInsertElements(objPtr, -1, argc, argv);
    }

    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

char **Jim_GetEnviron(void)
{
#if defined(HAVE__NSGETENVIRON)
    return *_NSGetEnviron();
#else
    #if !defined(NO_ENVIRON_EXTERN)
    extern char **environ;
    #endif

    return environ;
#endif
}

void Jim_SetEnviron(char **env)
{
#if defined(HAVE__NSGETENVIRON)
    *_NSGetEnviron() = env;
#else
    #if !defined(NO_ENVIRON_EXTERN)
    extern char **environ;
    #endif

    environ = env;
#endif
}

/* [env] */
static int Jim_EnvCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *key;
    const char *val;

    if (argc == 1) {
        char **e = Jim_GetEnviron();

        int i;
        Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);

        for (i = 0; e[i]; i++) {
            const char *equals = strchr(e[i], '=');

            if (equals) {
                Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, e[i],
                        equals - e[i]));
                Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, equals + 1, -1));
            }
        }

        Jim_SetResult(interp, listObjPtr);
        return JIM_OK;
    }

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?default?");
        return JIM_ERR;
    }
    key = Jim_String(argv[1]);
    val = getenv(key);
    if (val == NULL) {
        if (argc < 3) {
            Jim_SetResultFormatted(interp, "environment variable \"%#s\" does not exist", argv[1]);
            return JIM_ERR;
        }
        val = Jim_String(argv[2]);
    }
    Jim_SetResult(interp, Jim_NewStringObj(interp, val, -1));
    return JIM_OK;
}

/* [source] */
static int Jim_SourceCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retval;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "fileName");
        return JIM_ERR;
    }
    retval = Jim_EvalFile(interp, Jim_String(argv[1]));
    if (retval == JIM_RETURN)
        return JIM_OK;
    return retval;
}

/* [lreverse] */
static int Jim_LreverseCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *revObjPtr, **ele;
    int len;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "list");
        return JIM_ERR;
    }
    JimListGetElements(interp, argv[1], &len, &ele);
    len--;
    revObjPtr = Jim_NewListObj(interp, NULL, 0);
    while (len >= 0)
        ListAppendElement(revObjPtr, ele[len--]);
    Jim_SetResult(interp, revObjPtr);
    return JIM_OK;
}

static int JimRangeLen(jim_wide start, jim_wide end, jim_wide step)
{
    jim_wide len;

    if (step == 0)
        return -1;
    if (start == end)
        return 0;
    else if (step > 0 && start > end)
        return -1;
    else if (step < 0 && end > start)
        return -1;
    len = end - start;
    if (len < 0)
        len = -len;             /* abs(len) */
    if (step < 0)
        step = -step;           /* abs(step) */
    len = 1 + ((len - 1) / step);
    /* We can truncate safely to INT_MAX, the range command
     * will always return an error for a such long range
     * because Tcl lists can't be so long. */
    if (len > INT_MAX)
        len = INT_MAX;
    return (int)((len < 0) ? -1 : len);
}

/* [range] */
static int Jim_RangeCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_wide start = 0, end, step = 1;
    int len, i;
    Jim_Obj *objPtr;

    if (argc < 2 || argc > 4) {
        Jim_WrongNumArgs(interp, 1, argv, "?start? end ?step?");
        return JIM_ERR;
    }
    if (argc == 2) {
        if (Jim_GetWide(interp, argv[1], &end) != JIM_OK)
            return JIM_ERR;
    }
    else {
        if (Jim_GetWide(interp, argv[1], &start) != JIM_OK ||
            Jim_GetWide(interp, argv[2], &end) != JIM_OK)
            return JIM_ERR;
        if (argc == 4 && Jim_GetWide(interp, argv[3], &step) != JIM_OK)
            return JIM_ERR;
    }
    if ((len = JimRangeLen(start, end, step)) == -1) {
        Jim_SetResultString(interp, "Invalid (infinite?) range specified", -1);
        return JIM_ERR;
    }
    objPtr = Jim_NewListObj(interp, NULL, 0);
    for (i = 0; i < len; i++)
        ListAppendElement(objPtr, Jim_NewIntObj(interp, start + i * step));
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

/* [rand] */
static int Jim_RandCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_wide min = 0, max = 0, len, maxMul;

    if (argc < 1 || argc > 3) {
        Jim_WrongNumArgs(interp, 1, argv, "?min? max");
        return JIM_ERR;
    }
    if (argc == 1) {
        max = JIM_WIDE_MAX;
    } else if (argc == 2) {
        if (Jim_GetWide(interp, argv[1], &max) != JIM_OK)
            return JIM_ERR;
    } else if (argc == 3) {
        if (Jim_GetWide(interp, argv[1], &min) != JIM_OK ||
            Jim_GetWide(interp, argv[2], &max) != JIM_OK)
            return JIM_ERR;
    }
    len = max-min;
    if (len < 0) {
        Jim_SetResultString(interp, "Invalid arguments (max < min)", -1);
        return JIM_ERR;
    }
    maxMul = JIM_WIDE_MAX - (len ? (JIM_WIDE_MAX%len) : 0);
    while (1) {
        jim_wide r;

        JimRandomBytes(interp, &r, sizeof(jim_wide));
        if (r < 0 || r >= maxMul) continue;
        r = (len == 0) ? 0 : r%len;
        Jim_SetResultInt(interp, min+r);
        return JIM_OK;
    }
}

static const struct {
    const char *name;
    Jim_CmdProc *cmdProc;
} Jim_CoreCommandsTable[] = {
    {"alias", Jim_AliasCoreCommand},
    {"set", Jim_SetCoreCommand},
    {"unset", Jim_UnsetCoreCommand},
    {"puts", Jim_PutsCoreCommand},
    {"+", Jim_AddCoreCommand},
    {"*", Jim_MulCoreCommand},
    {"-", Jim_SubCoreCommand},
    {"/", Jim_DivCoreCommand},
    {"incr", Jim_IncrCoreCommand},
    {"while", Jim_WhileCoreCommand},
    {"loop", Jim_LoopCoreCommand},
    {"for", Jim_ForCoreCommand},
    {"foreach", Jim_ForeachCoreCommand},
    {"lmap", Jim_LmapCoreCommand},
    {"lassign", Jim_LassignCoreCommand},
    {"if", Jim_IfCoreCommand},
    {"switch", Jim_SwitchCoreCommand},
    {"list", Jim_ListCoreCommand},
    {"lindex", Jim_LindexCoreCommand},
    {"lset", Jim_LsetCoreCommand},
    {"lsearch", Jim_LsearchCoreCommand},
    {"llength", Jim_LlengthCoreCommand},
    {"lappend", Jim_LappendCoreCommand},
    {"linsert", Jim_LinsertCoreCommand},
    {"lreplace", Jim_LreplaceCoreCommand},
    {"lsort", Jim_LsortCoreCommand},
    {"append", Jim_AppendCoreCommand},
    {"debug", Jim_DebugCoreCommand},
    {"eval", Jim_EvalCoreCommand},
    {"uplevel", Jim_UplevelCoreCommand},
    {"expr", Jim_ExprCoreCommand},
    {"break", Jim_BreakCoreCommand},
    {"continue", Jim_ContinueCoreCommand},
    {"proc", Jim_ProcCoreCommand},
    {"concat", Jim_ConcatCoreCommand},
    {"return", Jim_ReturnCoreCommand},
    {"upvar", Jim_UpvarCoreCommand},
    {"global", Jim_GlobalCoreCommand},
    {"string", Jim_StringCoreCommand},
    {"time", Jim_TimeCoreCommand},
    {"exit", Jim_ExitCoreCommand},
    {"catch", Jim_CatchCoreCommand},
#ifdef JIM_REFERENCES
    {"ref", Jim_RefCoreCommand},
    {"getref", Jim_GetrefCoreCommand},
    {"setref", Jim_SetrefCoreCommand},
    {"finalize", Jim_FinalizeCoreCommand},
    {"collect", Jim_CollectCoreCommand},
#endif
    {"rename", Jim_RenameCoreCommand},
    {"dict", Jim_DictCoreCommand},
    {"subst", Jim_SubstCoreCommand},
    {"info", Jim_InfoCoreCommand},
    {"exists", Jim_ExistsCoreCommand},
    {"split", Jim_SplitCoreCommand},
    {"join", Jim_JoinCoreCommand},
    {"format", Jim_FormatCoreCommand},
    {"scan", Jim_ScanCoreCommand},
    {"error", Jim_ErrorCoreCommand},
    {"lrange", Jim_LrangeCoreCommand},
    {"lrepeat", Jim_LrepeatCoreCommand},
    {"env", Jim_EnvCoreCommand},
    {"source", Jim_SourceCoreCommand},
    {"lreverse", Jim_LreverseCoreCommand},
    {"range", Jim_RangeCoreCommand},
    {"rand", Jim_RandCoreCommand},
    {"tailcall", Jim_TailcallCoreCommand},
    {"local", Jim_LocalCoreCommand},
    {"upcall", Jim_UpcallCoreCommand},
    {"apply", Jim_ApplyCoreCommand},
    {NULL, NULL},
};

void Jim_RegisterCoreCommands(Jim_Interp *interp)
{
    int i = 0;

    while (Jim_CoreCommandsTable[i].name != NULL) {
        Jim_CreateCommand(interp,
            Jim_CoreCommandsTable[i].name, Jim_CoreCommandsTable[i].cmdProc, NULL, NULL);
        i++;
    }
}

/* -----------------------------------------------------------------------------
 * Interactive prompt
 * ---------------------------------------------------------------------------*/
void Jim_MakeErrorMessage(Jim_Interp *interp)
{
    Jim_Obj *argv[2];

    argv[0] = Jim_NewStringObj(interp, "errorInfo", -1);
    argv[1] = interp->result;

    Jim_EvalObjVector(interp, 2, argv);
}

static void JimSetFailedEnumResult(Jim_Interp *interp, const char *arg, const char *badtype,
    const char *prefix, const char *const *tablePtr, const char *name)
{
    int count;
    char **tablePtrSorted;
    int i;

    for (count = 0; tablePtr[count]; count++) {
    }

    if (name == NULL) {
        name = "option";
    }

    Jim_SetResultFormatted(interp, "%s%s \"%s\": must be ", badtype, name, arg);
    tablePtrSorted = Jim_Alloc(sizeof(char *) * count);
    memcpy(tablePtrSorted, tablePtr, sizeof(char *) * count);
    qsort(tablePtrSorted, count, sizeof(char *), qsortCompareStringPointers);
    for (i = 0; i < count; i++) {
        if (i + 1 == count && count > 1) {
            Jim_AppendString(interp, Jim_GetResult(interp), "or ", -1);
        }
        Jim_AppendStrings(interp, Jim_GetResult(interp), prefix, tablePtrSorted[i], NULL);
        if (i + 1 != count) {
            Jim_AppendString(interp, Jim_GetResult(interp), ", ", -1);
        }
    }
    Jim_Free(tablePtrSorted);
}

int Jim_GetEnum(Jim_Interp *interp, Jim_Obj *objPtr,
    const char *const *tablePtr, int *indexPtr, const char *name, int flags)
{
    const char *bad = "bad ";
    const char *const *entryPtr = NULL;
    int i;
    int match = -1;
    int arglen;
    const char *arg = Jim_GetString(objPtr, &arglen);

    *indexPtr = -1;

    for (entryPtr = tablePtr, i = 0; *entryPtr != NULL; entryPtr++, i++) {
        if (Jim_CompareStringImmediate(interp, objPtr, *entryPtr)) {
            /* Found an exact match */
            *indexPtr = i;
            return JIM_OK;
        }
        if (flags & JIM_ENUM_ABBREV) {
            /* Accept an unambiguous abbreviation.
             * Note that '-' doesnt' consitute a valid abbreviation
             */
            if (strncmp(arg, *entryPtr, arglen) == 0) {
                if (*arg == '-' && arglen == 1) {
                    break;
                }
                if (match >= 0) {
                    bad = "ambiguous ";
                    goto ambiguous;
                }
                match = i;
            }
        }
    }

    /* If we had an unambiguous partial match */
    if (match >= 0) {
        *indexPtr = match;
        return JIM_OK;
    }

  ambiguous:
    if (flags & JIM_ERRMSG) {
        JimSetFailedEnumResult(interp, arg, bad, "", tablePtr, name);
    }
    return JIM_ERR;
}

int Jim_FindByName(const char *name, const char * const array[], size_t len)
{
    int i;

    for (i = 0; i < (int)len; i++) {
        if (array[i] && strcmp(array[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

int Jim_IsDict(Jim_Obj *objPtr)
{
    return objPtr->typePtr == &dictObjType;
}

int Jim_IsList(Jim_Obj *objPtr)
{
    return objPtr->typePtr == &listObjType;
}

/**
 * Very simple printf-like formatting, designed for error messages.
 *
 * The format may contain up to 5 '%s' or '%#s', corresponding to variable arguments.
 * The resulting string is created and set as the result.
 *
 * Each '%s' should correspond to a regular string parameter.
 * Each '%#s' should correspond to a (Jim_Obj *) parameter.
 * Any other printf specifier is not allowed (but %% is allowed for the % character).
 *
 * e.g. Jim_SetResultFormatted(interp, "Bad option \"%#s\" in proc \"%#s\"", optionObjPtr, procNamePtr);
 *
 * Note: We take advantage of the fact that printf has the same behaviour for both %s and %#s
 */
void Jim_SetResultFormatted(Jim_Interp *interp, const char *format, ...)
{
    /* Initial space needed */
    int len = strlen(format);
    int extra = 0;
    int n = 0;
    const char *params[5];
    char *buf;
    va_list args;
    int i;

    va_start(args, format);

    for (i = 0; i < len && n < 5; i++) {
        int l;

        if (strncmp(format + i, "%s", 2) == 0) {
            params[n] = va_arg(args, char *);

            l = strlen(params[n]);
        }
        else if (strncmp(format + i, "%#s", 3) == 0) {
            Jim_Obj *objPtr = va_arg(args, Jim_Obj *);

            params[n] = Jim_GetString(objPtr, &l);
        }
        else {
            if (format[i] == '%') {
                i++;
            }
            continue;
        }
        n++;
        extra += l;
    }

    len += extra;
    buf = Jim_Alloc(len + 1);
    len = snprintf(buf, len + 1, format, params[0], params[1], params[2], params[3], params[4]);

    va_end(args);

    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, buf, len));
}

/* stubs */
#ifndef jim_ext_package
int Jim_PackageProvide(Jim_Interp *interp, const char *name, const char *ver, int flags)
{
    return JIM_OK;
}
#endif
#ifndef jim_ext_aio
FILE *Jim_AioFilehandle(Jim_Interp *interp, Jim_Obj *fhObj)
{
    Jim_SetResultString(interp, "aio not enabled", -1);
    return NULL;
}
#endif


/*
 * Local Variables: ***
 * c-basic-offset: 4 ***
 * tab-width: 4 ***
 * End: ***
 */
