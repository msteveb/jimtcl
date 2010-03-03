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
 * The FreeBSD license
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
#define __JIM_CORE__
#define JIM_OPTIMIZATION /* comment to avoid optimizations and reduce size */

#if !defined(JIM_ANSIC) && !defined(NOMMU)
#define JIM_DYNLIB      /* Dynamic library support */
#endif /* JIM_ANSIC */

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#endif

#ifdef __FreeBSD__
#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define	NEED_ENVIRON_EXTERN	(1)
#endif

#ifndef JIM_ANSIC
#define JIM_DYNLIB      /* Dynamic library support for UNIX and WIN32 */
#endif /* JIM_ANSIC */

#include <stdarg.h>
#include <limits.h>

/* Include the platform dependent libraries for
 * dynamic loading of libraries. */
#ifdef JIM_DYNLIB
#include <dlfcn.h>
#endif /* JIM_DYNLIB */

#include <unistd.h>
#include <sys/time.h>

#include "jim.h"

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

/* For INFINITY, even if math functions are not enabled */
#include <math.h>

/* -----------------------------------------------------------------------------
 * Global variables
 * ---------------------------------------------------------------------------*/

/* A shared empty string for the objects string representation.
 * Jim_InvalidateStringRep knows about it and don't try to free. */
static char *JimEmptyStringRep = (char*) "";

/* -----------------------------------------------------------------------------
 * Required prototypes of not exported functions
 * ---------------------------------------------------------------------------*/
static void JimChangeCallFrameId(Jim_Interp *interp, Jim_CallFrame *cf);
static void JimFreeCallFrame(Jim_Interp *interp, Jim_CallFrame *cf, int flags);
static int ListSetIndex(Jim_Interp *interp, Jim_Obj *listPtr, int index, Jim_Obj *newObjPtr, int flags);
static Jim_Obj *Jim_ExpandDictSugar(Jim_Interp *interp, Jim_Obj *objPtr);
static void SetDictSubstFromAny(Jim_Interp *interp, Jim_Obj *objPtr);
static void JimSetFailedEnumResult(Jim_Interp *interp, const char *arg, const char *badtype, const char *prefix, const char * const *tablePtr, const char *name);

static const Jim_HashTableType JimVariablesHashTableType;

const char *tt_name(int type);

/* -----------------------------------------------------------------------------
 * Utility functions
 * ---------------------------------------------------------------------------*/

/* Glob-style pattern matching. */
static int JimStringMatch(const char *pattern, int patternLen,
        const char *string, int stringLen, int nocase)
{
    while(patternLen) {
        switch(pattern[0]) {
        case '*':
            while (pattern[1] == '*') {
                pattern++;
                patternLen--;
            }
            if (patternLen == 1)
                return 1; /* match */
            while(stringLen) {
                if (JimStringMatch(pattern+1, patternLen-1,
                            string, stringLen, nocase))
                    return 1; /* match */
                string++;
                stringLen--;
            }
            return 0; /* no match */
            break;
        case '?':
            if (stringLen == 0)
                return 0; /* no match */
            string++;
            stringLen--;
            break;
        case '[':
        {
            int not, match;

            pattern++;
            patternLen--;
            not = pattern[0] == '^';
            if (not) {
                pattern++;
                patternLen--;
            }
            match = 0;
            while(1) {
                if (pattern[0] == '\\') {
                    pattern++;
                    patternLen--;
                    if (pattern[0] == string[0])
                        match = 1;
                } else if (pattern[0] == ']') {
                    break;
                } else if (patternLen == 0) {
                    pattern--;
                    patternLen++;
                    break;
                } else if (pattern[1] == '-' && patternLen >= 3) {
                    int start = pattern[0];
                    int end = pattern[2];
                    int c = string[0];
                    if (start > end) {
                        int t = start;
                        start = end;
                        end = t;
                    }
                    if (nocase) {
                        start = tolower(start);
                        end = tolower(end);
                        c = tolower(c);
                    }
                    pattern += 2;
                    patternLen -= 2;
                    if (c >= start && c <= end)
                        match = 1;
                } else {
                    if (!nocase) {
                        if (pattern[0] == string[0])
                            match = 1;
                    } else {
                        if (tolower((int)pattern[0]) == tolower((int)string[0]))
                            match = 1;
                    }
                }
                pattern++;
                patternLen--;
            }
            if (not)
                match = !match;
            if (!match)
                return 0; /* no match */
            string++;
            stringLen--;
            break;
        }
        case '\\':
            if (patternLen >= 2) {
                pattern++;
                patternLen--;
            }
            /* fall through */
        default:
            if (!nocase) {
                if (pattern[0] != string[0])
                    return 0; /* no match */
            } else {
                if (tolower((int)pattern[0]) != tolower((int)string[0]))
                    return 0; /* no match */
            }
            string++;
            stringLen--;
            break;
        }
        pattern++;
        patternLen--;
        if (stringLen == 0) {
            while(*pattern == '*') {
                pattern++;
                patternLen--;
            }
            break;
        }
    }
    if (patternLen == 0 && stringLen == 0)
        return 1;
    return 0;
}

int JimStringCompare(const char *s1, int l1, const char *s2, int l2,
        int nocase)
{
    unsigned char *u1 = (unsigned char*) s1, *u2 = (unsigned char*) s2;
    int diff;

    if (nocase == 0) {
        while(l1 && l2) {
            diff = (int)*u1-*u2;
            if (diff) {
                goto done;
            }
            u1++; u2++; l1--; l2--;
        }
        diff = l1-l2;
    } else {
        while(l1 && l2) {
            diff = tolower((int)*u1)-tolower((int)*u2);
            if (diff) {
                goto done;
            }
            u1++; u2++; l1--; l2--;
        }
        diff = l1-l2;
    }
    if (diff == 0) {
        return 0;
    }
done:
    return diff < 0 ? -1 : 1;
}

/* Search 's1' inside 's2', starting to search from char 'index' of 's2'.
 * The index of the first occurrence of s1 in s2 is returned. 
 * If s1 is not found inside s2, -1 is returned. */
int JimStringFirst(const char *s1, int l1, const char *s2, int l2, int index)
{
    int i;

    if (!l1 || !l2 || l1 > l2) return -1;
    if (index < 0) index = 0;
    s2 += index;
    for (i = index; i <= l2-l1; i++) {
        if (memcmp(s2, s1, l1) == 0)
            return i;
        s2++;
    }
    return -1;
}

int JimStringLast(const char *s1, int l1, const char *s2, int l2)
{
    const char *p;

    if (!l1 || !l2 || l1 > l2) return -1;

    /* Now search for the needle */
    for (p = s2 + l2 - 1; p != s2 - 1; p--) {
        if (*p == *s1 && memcmp(s1, p, l1) == 0) {
            return p - s2;
        }
    }
    return -1;
}

int Jim_WideToString(char *buf, jim_wide wideValue)
{
    const char *fmt = "%" JIM_WIDE_MODIFIER;
    return sprintf(buf, fmt, wideValue);
}

int Jim_StringToWide(const char *str, jim_wide *widePtr, int base)
{
    char *endptr;

    *widePtr = strtoull(str, &endptr, base);

    if ((str[0] == '\0') || (str == endptr) )
        return JIM_ERR;
    if (endptr[0] != '\0') {
        while(*endptr) {
            if (!isspace(*endptr)) {
                return JIM_ERR;
            }
            endptr++;
        }
    }
    return JIM_OK;
}

int Jim_DoubleToString(char *buf, double doubleValue)
{
    int len;

    len = sprintf(buf, "%.12g", doubleValue);

    /* Add a final ".0" if it's a number. But not
     * for NaN or InF */
    while (*buf) {
        if (*buf == '.' || isalpha(*buf)) {
            /* inf -> Inf, nan -> Nan */
            if (*buf == 'i' || *buf == 'n') {
                *buf = toupper(*buf);
            }
            return len;
        }
        buf++;
    }

    *buf++ = '.';
    *buf++ = '0';
    *buf = '\0';

    return len + 2;
}

int Jim_StringToDouble(const char *str, double *doublePtr)
{
    char *endptr;

    *doublePtr = strtod(str, &endptr);
    if (str[0] == '\0' || endptr[0] != '\0' || (str == endptr)) {
        return JIM_ERR;
    }
    return JIM_OK;
}

static jim_wide JimPowWide(jim_wide b, jim_wide e)
{
    jim_wide i, res = 1;
    if ((b==0 && e!=0) || (e<0)) return 0;
    for(i=0; i<e; i++) {res *= b;}
    return res;
}

/* -----------------------------------------------------------------------------
 * Special functions
 * ---------------------------------------------------------------------------*/

/* Note that 'interp' may be NULL if not available in the
 * context of the panic. It's only useful to get the error
 * file descriptor, it will default to stderr otherwise. */
void Jim_Panic(Jim_Interp *interp, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    /* 
     * Send it here first.. Assuming STDIO still works
     */
    fprintf(stderr, JIM_NL "JIM INTERPRETER PANIC: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, JIM_NL JIM_NL);
    va_end(ap);

#ifdef HAVE_BACKTRACE
    {
        void *array[40];
        int size, i;
        char **strings;

        size = backtrace(array, 40);
        strings = backtrace_symbols(array, size);
        for (i = 0; i < size; i++)
            fprintf(stderr,"[backtrace] %s" JIM_NL, strings[i]);
        fprintf(stderr,"[backtrace] Include the above lines and the output" JIM_NL);
        fprintf(stderr,"[backtrace] of 'nm <executable>' in the bug report." JIM_NL);
    }
#endif

    abort();
}

/* -----------------------------------------------------------------------------
 * Memory allocation
 * ---------------------------------------------------------------------------*/

void *Jim_Alloc(int size)
{
    /* We allocate zero length arrayes, etc. to use a single orthogonal codepath */
    return malloc(size);
}

void Jim_Free(void *ptr) {
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
    char *copy = Jim_Alloc(l+1);
    
    memcpy(copy, s, l+1);
    copy[l] = 0;    /* Just to be sure, original could be substring */
    return copy;
}

/* -----------------------------------------------------------------------------
 * Time related functions
 * ---------------------------------------------------------------------------*/
/* Returns microseconds of CPU used since start. */
static jim_wide JimClock(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (jim_wide)tv.tv_sec*1000000 + tv.tv_usec;
}

/* -----------------------------------------------------------------------------
 * Hash Tables
 * ---------------------------------------------------------------------------*/

/* -------------------------- private prototypes ---------------------------- */
static int JimExpandHashTableIfNeeded(Jim_HashTable *ht);
static unsigned int JimHashTableNextPower(unsigned int size);
static int JimInsertHashEntry(Jim_HashTable *ht, const void *key);

/* -------------------------- hash functions -------------------------------- */

/* Thomas Wang's 32 bit Mix Function */
unsigned int Jim_IntHashFunction(unsigned int key)
{
    key += ~(key << 15);
    key ^=  (key >> 10);
    key +=  (key << 3);
    key ^=  (key >> 6);
    key += ~(key << 11);
    key ^=  (key >> 16);
    return key;
}

/* Generic hash function (we are using to multiply by 9 and add the byte
 * as Tcl) */
unsigned int Jim_GenHashFunction(const unsigned char *buf, int len)
{
    unsigned int h = 0;
    while(len--)
        h += (h<<3)+*buf++;
    return h;
}

/* ----------------------------- API implementation ------------------------- */
/* reset an hashtable already initialized with ht_init().
 * NOTE: This function should only called by ht_destroy(). */
static void JimResetHashTable(Jim_HashTable *ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
    ht->collisions = 0;
}

/* Initialize the hash table */
int Jim_InitHashTable(Jim_HashTable *ht, const Jim_HashTableType *type,
        void *privDataPtr)
{
    JimResetHashTable(ht);
    ht->type = type;
    ht->privdata = privDataPtr;
    return JIM_OK;
}

/* Resize the table to the minimal size that contains all the elements,
 * but with the invariant of a USER/BUCKETS ration near to <= 1 */
int Jim_ResizeHashTable(Jim_HashTable *ht)
{
    int minimal = ht->used;

    if (minimal < JIM_HT_INITIAL_SIZE)
        minimal = JIM_HT_INITIAL_SIZE;
    return Jim_ExpandHashTable(ht, minimal);
}

/* Expand or create the hashtable */
int Jim_ExpandHashTable(Jim_HashTable *ht, unsigned int size)
{
    Jim_HashTable n; /* the new hashtable */
    unsigned int realsize = JimHashTableNextPower(size), i;

    /* the size is invalid if it is smaller than the number of
     * elements already inside the hashtable */
    if (ht->used >= size)
        return JIM_ERR;

    Jim_InitHashTable(&n, ht->type, ht->privdata);
    n.size = realsize;
    n.sizemask = realsize-1;
    n.table = Jim_Alloc(realsize*sizeof(Jim_HashEntry*));

    /* Initialize all the pointers to NULL */
    memset(n.table, 0, realsize*sizeof(Jim_HashEntry*));

    /* Copy all the elements from the old to the new table:
     * note that if the old hash table is empty ht->size is zero,
     * so Jim_ExpandHashTable just creates an hash table. */
    n.used = ht->used;
    for (i = 0; i < ht->size && ht->used > 0; i++) {
        Jim_HashEntry *he, *nextHe;

        if (ht->table[i] == NULL) continue;
        
        /* For each hash entry on this slot... */
        he = ht->table[i];
        while(he) {
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
    return JIM_OK;
}

/* Add an element to the target hash table */
int Jim_AddHashEntry(Jim_HashTable *ht, const void *key, void *val)
{
    int index;
    Jim_HashEntry *entry;

    /* Get the index of the new element, or -1 if
     * the element already exists. */
    if ((index = JimInsertHashEntry(ht, key)) == -1)
        return JIM_ERR;

    /* Allocates the memory and stores key */
    entry = Jim_Alloc(sizeof(*entry));
    entry->next = ht->table[index];
    ht->table[index] = entry;

    /* Set the hash entry fields. */
    Jim_SetHashKey(ht, entry, key);
    Jim_SetHashVal(ht, entry, val);
    ht->used++;
    return JIM_OK;
}

/* Add an element, discarding the old if the key already exists */
int Jim_ReplaceHashEntry(Jim_HashTable *ht, const void *key, void *val)
{
    Jim_HashEntry *entry;

    /* Try to add the element. If the key
     * does not exists Jim_AddHashEntry will suceed. */
    if (Jim_AddHashEntry(ht, key, val) == JIM_OK)
        return JIM_OK;
    /* It already exists, get the entry */
    entry = Jim_FindHashEntry(ht, key);
    /* Free the old value and set the new one */
    Jim_FreeEntryVal(ht, entry);
    Jim_SetHashVal(ht, entry, val);
    return JIM_OK;
}

/* Search and remove an element */
int Jim_DeleteHashEntry(Jim_HashTable *ht, const void *key)
{
    unsigned int h;
    Jim_HashEntry *he, *prevHe;

    if (ht->size == 0)
        return JIM_ERR;
    h = Jim_HashKey(ht, key) & ht->sizemask;
    he = ht->table[h];

    prevHe = NULL;
    while(he) {
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
    return JIM_ERR; /* not found */
}

/* Destroy an entire hash table */
int Jim_FreeHashTable(Jim_HashTable *ht)
{
    unsigned int i;

    /* Free all the elements */
    for (i = 0; i < ht->size && ht->used > 0; i++) {
        Jim_HashEntry *he, *nextHe;

        if ((he = ht->table[i]) == NULL) continue;
        while(he) {
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
    return JIM_OK; /* never fails */
}

Jim_HashEntry *Jim_FindHashEntry(Jim_HashTable *ht, const void *key)
{
    Jim_HashEntry *he;
    unsigned int h;

    if (ht->size == 0) return NULL;
    h = Jim_HashKey(ht, key) & ht->sizemask;
    he = ht->table[h];
    while(he) {
        if (Jim_CompareHashKeys(ht, key, he->key))
            return he;
        he = he->next;
    }
    return NULL;
}

Jim_HashTableIterator *Jim_GetHashTableIterator(Jim_HashTable *ht)
{
    Jim_HashTableIterator *iter = Jim_Alloc(sizeof(*iter));

    iter->ht = ht;
    iter->index = -1;
    iter->entry = NULL;
    iter->nextEntry = NULL;
    return iter;
}

Jim_HashEntry *Jim_NextHashEntry(Jim_HashTableIterator *iter)
{
    while (1) {
        if (iter->entry == NULL) {
            iter->index++;
            if (iter->index >=
                    (signed)iter->ht->size) break;
            iter->entry = iter->ht->table[iter->index];
        } else {
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
static int JimExpandHashTableIfNeeded(Jim_HashTable *ht)
{
    /* If the hash table is empty expand it to the intial size,
     * if the table is "full" dobule its size. */
    if (ht->size == 0)
        return Jim_ExpandHashTable(ht, JIM_HT_INITIAL_SIZE);
    if (ht->size == ht->used)
        return Jim_ExpandHashTable(ht, ht->size*2);
    return JIM_OK;
}

/* Our hash table capability is a power of two */
static unsigned int JimHashTableNextPower(unsigned int size)
{
    unsigned int i = JIM_HT_INITIAL_SIZE;

    if (size >= 2147483648U)
        return 2147483648U;
    while(1) {
        if (i >= size)
            return i;
        i *= 2;
    }
}

/* Returns the index of a free slot that can be populated with
 * an hash entry for the given 'key'.
 * If the key already exists, -1 is returned. */
static int JimInsertHashEntry(Jim_HashTable *ht, const void *key)
{
    unsigned int h;
    Jim_HashEntry *he;

    /* Expand the hashtable if needed */
    if (JimExpandHashTableIfNeeded(ht) == JIM_ERR)
        return -1;
    /* Compute the key hash value */
    h = Jim_HashKey(ht, key) & ht->sizemask;
    /* Search if this slot does not already contain the given key */
    he = ht->table[h];
    while(he) {
        if (Jim_CompareHashKeys(ht, key, he->key))
            return -1;
        he = he->next;
    }
    return h;
}

/* ----------------------- StringCopy Hash Table Type ------------------------*/

static unsigned int JimStringCopyHTHashFunction(const void *key)
{
    return Jim_GenHashFunction(key, strlen(key));
}

static const void *JimStringCopyHTKeyDup(void *privdata, const void *key)
{
    int len = strlen(key);
    char *copy = Jim_Alloc(len+1);
    JIM_NOTUSED(privdata);

    memcpy(copy, key, len);
    copy[len] = '\0';
    return copy;
}

static void *JimStringKeyValCopyHTValDup(void *privdata, const void *val)
{
    int len = strlen(val);
    char *copy = Jim_Alloc(len+1);
    JIM_NOTUSED(privdata);

    memcpy(copy, val, len);
    copy[len] = '\0';
    return copy;
}

static int JimStringCopyHTKeyCompare(void *privdata, const void *key1,
        const void *key2)
{
    JIM_NOTUSED(privdata);

    return strcmp(key1, key2) == 0;
}

static void JimStringCopyHTKeyDestructor(void *privdata, const void *key)
{
    JIM_NOTUSED(privdata);

    Jim_Free((void*)key); /* ATTENTION: const cast */
}

static void JimStringKeyValCopyHTValDestructor(void *privdata, void *val)
{
    JIM_NOTUSED(privdata);

    Jim_Free((void*)val); /* ATTENTION: const cast */
}

#if 0
static Jim_HashTableType JimStringCopyHashTableType = {
    JimStringCopyHTHashFunction,        /* hash function */
    JimStringCopyHTKeyDup,              /* key dup */
    NULL,                               /* val dup */
    JimStringCopyHTKeyCompare,          /* key compare */
    JimStringCopyHTKeyDestructor,       /* key destructor */
    NULL                                /* val destructor */
};
#endif

/* This is like StringCopy but does not auto-duplicate the key.
 * It's used for intepreter's shared strings. */
static const Jim_HashTableType JimSharedStringsHashTableType = {
    JimStringCopyHTHashFunction,        /* hash function */
    NULL,                               /* key dup */
    NULL,                               /* val dup */
    JimStringCopyHTKeyCompare,          /* key compare */
    JimStringCopyHTKeyDestructor,       /* key destructor */
    NULL                                /* val destructor */
};

/* This is like StringCopy but also automatically handle dynamic
 * allocated C strings as values. */
static const Jim_HashTableType JimStringKeyValCopyHashTableType = {
    JimStringCopyHTHashFunction,        /* hash function */
    JimStringCopyHTKeyDup,              /* key dup */
    JimStringKeyValCopyHTValDup,        /* val dup */
    JimStringCopyHTKeyCompare,          /* key compare */
    JimStringCopyHTKeyDestructor,       /* key destructor */
    JimStringKeyValCopyHTValDestructor, /* val destructor */
};

typedef struct AssocDataValue {
    Jim_InterpDeleteProc *delProc;
    void *data;
} AssocDataValue;

static void JimAssocDataHashTableValueDestructor(void *privdata, void *data)
{
    AssocDataValue *assocPtr = (AssocDataValue *)data;
    if (assocPtr->delProc != NULL)
        assocPtr->delProc((Jim_Interp *)privdata, assocPtr->data);
    Jim_Free(data);
}

static const Jim_HashTableType JimAssocDataHashTableType = {
    JimStringCopyHTHashFunction,         /* hash function */
    JimStringCopyHTKeyDup,               /* key dup */
    NULL,                                /* val dup */
    JimStringCopyHTKeyCompare,           /* key compare */
    JimStringCopyHTKeyDestructor,        /* key destructor */
    JimAssocDataHashTableValueDestructor /* val destructor */
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

void Jim_StackPush(Jim_Stack *stack, void *element) {
    int neededLen = stack->len+1;
    if (neededLen > stack->maxlen) {
        stack->maxlen = neededLen < 20 ? 20 : neededLen*2;
        stack->vector = Jim_Realloc(stack->vector, sizeof(void*)*stack->maxlen);
    }
    stack->vector[stack->len] = element;
    stack->len++;
}

void *Jim_StackPop(Jim_Stack *stack)
{
    if (stack->len == 0) return NULL;
    stack->len--;
    return stack->vector[stack->len];
}

void *Jim_StackPeek(Jim_Stack *stack)
{
    if (stack->len == 0) return NULL;
    return stack->vector[stack->len-1];
}

void Jim_FreeStackElements(Jim_Stack *stack, void (*freeFunc)(void *ptr))
{
    int i;

    for (i = 0; i < stack->len; i++)
        freeFunc(stack->vector[i]);
}

/* -----------------------------------------------------------------------------
 * Parser
 * ---------------------------------------------------------------------------*/

/* Token types */
#define JIM_TT_NONE 0        /* No token returned */
#define JIM_TT_STR 1        /* simple string */
#define JIM_TT_ESC 2        /* string that needs escape chars conversion */
#define JIM_TT_VAR 3        /* var substitution */
#define JIM_TT_DICTSUGAR 4    /* Syntax sugar for [dict get], $foo(bar) */
#define JIM_TT_CMD 5        /* command substitution */
#define JIM_TT_SEP 6        /* word separator */
#define JIM_TT_EOL 7        /* line separator */
#define JIM_TT_EOF 8        /* end of script */

/* Additional token types needed for expressions */
#define JIM_TT_SUBEXPR_START 10
#define JIM_TT_SUBEXPR_END 11
#define JIM_TT_EXPR_INT 12
#define JIM_TT_EXPR_DOUBLE 13

/* Operator token types start here */
#define JIM_TT_EXPR_OP 15

/* Parser states */
#define JIM_PS_DEF 0          /* Default state */
#define JIM_PS_QUOTE 1        /* Inside "" */
#define JIM_PS_DICTSUGAR 2    /* Tokenising abc(def) into 4 separate tokens */

/* Parser context structure. The same context is used both to parse
 * Tcl scripts and lists. */
struct JimParserCtx {
    const char *prg;     /* Program text */
    const char *p;       /* Pointer to the point of the program we are parsing */
    int len;             /* Left length of 'prg' */
    int linenr;          /* Current line number */
    const char *tstart;
    const char *tend;    /* Returned token is at tstart-tend in 'prg'. */
    int tline;           /* Line number of the returned token */
    int tt;              /* Token type */
    int eof;             /* Non zero if EOF condition is true. */
    int state;           /* Parser state */
    int comment;         /* Non zero if the next chars may be a comment. */
};

#define JimParserEof(c) ((c)->eof)
#define JimParserTstart(c) ((c)->tstart)
#define JimParserTend(c) ((c)->tend)
#define JimParserTtype(c) ((c)->tt)
#define JimParserTline(c) ((c)->tline)

static int JimParseScript(struct JimParserCtx *pc);
static int JimParseSep(struct JimParserCtx *pc);
static int JimParseEol(struct JimParserCtx *pc);
static int JimParseCmd(struct JimParserCtx *pc);
static int JimParseVar(struct JimParserCtx *pc);
static int JimParseBrace(struct JimParserCtx *pc);
static int JimParseStr(struct JimParserCtx *pc);
static int JimParseComment(struct JimParserCtx *pc);
static char *JimParserGetToken(struct JimParserCtx *pc,
        int *lenPtr, int *typePtr, int *linePtr);

/* Initialize a parser context.
 * 'prg' is a pointer to the program text, linenr is the line
 * number of the first line contained in the program. */
static void JimParserInit(struct JimParserCtx *pc, const char *prg, 
        int len, int linenr)
{
    pc->prg = prg;
    pc->p = prg;
    pc->len = len;
    pc->tstart = NULL;
    pc->tend = NULL;
    pc->tline = 0;
    pc->tt = JIM_TT_NONE;
    pc->eof = 0;
    pc->state = JIM_PS_DEF;
    pc->linenr = linenr;
    pc->comment = 1;
}

static int JimParseScript(struct JimParserCtx *pc)
{
    while(1) { /* the while is used to reiterate with continue if needed */
        if (!pc->len) {
            pc->tstart = pc->p;
            pc->tend = pc->p-1;
            pc->tline = pc->linenr;
            pc->tt = JIM_TT_EOL;
            pc->eof = 1;
            return JIM_OK;
        }
        switch(*(pc->p)) {
        case '\\':
            if (*(pc->p+1) == '\n')
                return JimParseSep(pc);
            else {
                pc->comment = 0;
                return JimParseStr(pc);
            }
            break;
        case ' ':
        case '\t':
        case '\r':
            if (pc->state == JIM_PS_DEF)
                return JimParseSep(pc);
            else {
                pc->comment = 0;
                return JimParseStr(pc);
            }
            break;
        case '\n':
        case ';':
            pc->comment = 1;
            if (pc->state == JIM_PS_DEF)
                return JimParseEol(pc);
            else
                return JimParseStr(pc);
            break;
        case '[':
            pc->comment = 0;
            return JimParseCmd(pc);
            break;
        case '$':
            pc->comment = 0;
            if (JimParseVar(pc) == JIM_ERR) {
                pc->tstart = pc->tend = pc->p++; pc->len--;
                pc->tline = pc->linenr;
                pc->tt = JIM_TT_STR;
                return JIM_OK;
            } else
                return JIM_OK;
            break;
        case '#':
            if (pc->comment) {
                JimParseComment(pc);
                continue;
            } else {
                return JimParseStr(pc);
            }
        default:
            pc->comment = 0;
            return JimParseStr(pc);
            break;
        }
        return JIM_OK;
    }
}

static int JimParseSep(struct JimParserCtx *pc)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (*pc->p == ' ' || *pc->p == '\t' || *pc->p == '\r' ||
           (*pc->p == '\\' && *(pc->p+1) == '\n')) {
        if (*pc->p == '\\') {
            pc->p++; pc->len--;
            pc->linenr++;
        }
        pc->p++; pc->len--;
    }
    pc->tend = pc->p-1;
    pc->tt = JIM_TT_SEP;
    return JIM_OK;
}

static int JimParseEol(struct JimParserCtx *pc)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (*pc->p == ' ' || *pc->p == '\n' ||
           *pc->p == '\t' || *pc->p == '\r' || *pc->p == ';') {
        if (*pc->p == '\n')
            pc->linenr++;
        pc->p++; pc->len--;
    }
    pc->tend = pc->p-1;
    pc->tt = JIM_TT_EOL;
    return JIM_OK;
}

/* Todo. Don't stop if ']' appears inside {} or quoted.
 * Also should handle the case of puts [string length "]"] */
static int JimParseCmd(struct JimParserCtx *pc)
{
    int level = 1;
    int blevel = 0;

    pc->tstart = ++pc->p; pc->len--;
    pc->tline = pc->linenr;
    while (1) {
        if (pc->len == 0) {
            break;
        } else if (*pc->p == '[' && blevel == 0) {
            level++;
        } else if (*pc->p == ']' && blevel == 0) {
            level--;
            if (!level) break;
        } else if (*pc->p == '\\') {
            pc->p++; pc->len--;
            if (*pc->p == '\n')
                pc->linenr++;
        } else if (*pc->p == '{') {
            blevel++;
        } else if (*pc->p == '}') {
            if (blevel != 0)
                blevel--;
        } else if (*pc->p == '\n')
            pc->linenr++;
        pc->p++; pc->len--;
    }
    pc->tend = pc->p-1;
    pc->tt = JIM_TT_CMD;
    if (*pc->p == ']') {
        pc->p++; pc->len--;
    }
    return JIM_OK;
}

static int JimParseVar(struct JimParserCtx *pc)
{
    int brace = 0, stop = 0, ttype = JIM_TT_VAR;

    pc->tstart = ++pc->p; pc->len--; /* skip the $ */
    pc->tline = pc->linenr;
    if (*pc->p == '{') {
        pc->tstart = ++pc->p; pc->len--;
        brace = 1;
    }
    if (brace) {
        while (!stop) {
            if (*pc->p == '}' || pc->len == 0) {
                pc->tend = pc->p-1;
                stop = 1;
                if (pc->len == 0)
                    break;
            }
            else if (*pc->p == '\n')
                pc->linenr++;
            pc->p++; pc->len--;
        }
    } else {
        /* Include leading colons */
        while (*pc->p == ':') {
            pc->p++;
            pc->len--;
        }
        while (!stop) {
            if (!((*pc->p >= 'a' && *pc->p <= 'z') ||
                (*pc->p >= 'A' && *pc->p <= 'Z') ||
                (*pc->p >= '0' && *pc->p <= '9') || *pc->p == '_'))
                stop = 1;
            else {
                pc->p++; pc->len--;
            }
        }
        /* Parse [dict get] syntax sugar. */
        if (*pc->p == '(') {
            int count = 1;
            while (count && pc->len) {
                pc->p++; pc->len--;
                if (*pc->p == '\\' && pc->len >= 2) {
                    pc->p += 2; pc->len -= 2;
                }
                else if (*pc->p == '(') {
                    count++;
                }
                else if (*pc->p == ')') {
                    count--;
                }
            }
            if (*pc->p != '\0') {
                pc->p++; pc->len--;
            }
            ttype = JIM_TT_DICTSUGAR;
        }
        pc->tend = pc->p-1;
    }
    /* Check if we parsed just the '$' character.
     * That's not a variable so an error is returned
     * to tell the state machine to consider this '$' just
     * a string. */
    if (pc->tstart == pc->p) {
        pc->p--; pc->len++;
        return JIM_ERR;
    }
    pc->tt = ttype;
    return JIM_OK;
}

static int JimParseBrace(struct JimParserCtx *pc)
{
    int level = 1;

    pc->tstart = ++pc->p; pc->len--;
    pc->tline = pc->linenr;
    while (1) {
        if (*pc->p == '\\' && pc->len >= 2) {
            pc->p++; pc->len--;
            if (*pc->p == '\n')
                pc->linenr++;
        } else if (*pc->p == '{') {
            level++;
        } else if (pc->len == 0 || *pc->p == '}') {
            level--;
            if (pc->len == 0 || level == 0) {
                pc->tend = pc->p-1;
                if (pc->len != 0) {
                    pc->p++; pc->len--;
                }
                pc->tt = JIM_TT_STR;
                return JIM_OK;
            }
        } else if (*pc->p == '\n') {
            pc->linenr++;
        }
        pc->p++; pc->len--;
    }
    return JIM_OK; /* unreached */
}

static int JimParseStr(struct JimParserCtx *pc)
{
    int newword = (pc->tt == JIM_TT_SEP || pc->tt == JIM_TT_EOL ||
            pc->tt == JIM_TT_NONE || pc->tt == JIM_TT_STR);
    if (newword && *pc->p == '{') {
        return JimParseBrace(pc);
    } else if (newword && *pc->p == '"') {
        pc->state = JIM_PS_QUOTE;
        pc->p++; pc->len--;
    }
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (1) {
        if (pc->len == 0) {
            pc->tend = pc->p-1;
            pc->tt = JIM_TT_ESC;
            return JIM_OK;
        }
        switch(*pc->p) {
        case '\\':
            if (pc->state == JIM_PS_DEF &&
                *(pc->p+1) == '\n') {
                pc->tend = pc->p-1;
                pc->tt = JIM_TT_ESC;
                return JIM_OK;
            }
            if (pc->len >= 2) {
                if (*(pc->p+1) == '\n') {
                    pc->linenr++;
                }
                pc->p++; pc->len--;
            }
            break;
        case '(':
            /* If the following token is not '$' just keep going */
            if (pc->len > 1 && pc->p[1] != '$') {
                break;
            }
        case ')':
            /* Only need a separate ')' token if the previous was a var */
            if (*pc->p == '(' || pc->tt == JIM_TT_VAR) {
                if (pc->p == pc->tstart) {
                    /* At the start of the token, so just return this char */
                    pc->p++;
                    pc->len--;
                }
                pc->tend = pc->p-1;
                pc->tt = JIM_TT_ESC;
                return JIM_OK;
            }
            break;

        case '$':
        case '[':
            pc->tend = pc->p-1;
            pc->tt = JIM_TT_ESC;
            return JIM_OK;
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case ';':
            if (pc->state == JIM_PS_DEF) {
                pc->tend = pc->p-1;
                pc->tt = JIM_TT_ESC;
                return JIM_OK;
            } else if (*pc->p == '\n') {
                pc->linenr++;
            }
            break;
        case '"':
            if (pc->state == JIM_PS_QUOTE) {
                pc->tend = pc->p-1;
                pc->tt = JIM_TT_ESC;
                pc->p++; pc->len--;
                pc->state = JIM_PS_DEF;
                return JIM_OK;
            }
            break;
        }
        pc->p++; pc->len--;
    }
    return JIM_OK; /* unreached */
}

int JimParseComment(struct JimParserCtx *pc)
{
    while (*pc->p) {
        if (*pc->p == '\n') {
            pc->linenr++;
            if (*(pc->p-1) != '\\') {
                pc->p++; pc->len--;
                return JIM_OK;
            }
        }
        pc->p++; pc->len--;
    }
    return JIM_OK;
}

/* xdigitval and odigitval are helper functions for JimParserGetToken() */
static int xdigitval(int c)
{
    if (c >= '0' && c <= '9') return c-'0';
    if (c >= 'a' && c <= 'f') return c-'a'+10;
    if (c >= 'A' && c <= 'F') return c-'A'+10;
    return -1;
}

static int odigitval(int c)
{
    if (c >= '0' && c <= '7') return c-'0';
    return -1;
}

/* Perform Tcl escape substitution of 's', storing the result
 * string into 'dest'. The escaped string is guaranteed to
 * be the same length or shorted than the source string.
 * Slen is the length of the string at 's', if it's -1 the string
 * length will be calculated by the function.
 *
 * The function returns the length of the resulting string. */
static int JimEscape(char *dest, const char *s, int slen)
{
    char *p = dest;
    int i, len;
    
    if (slen == -1)
        slen = strlen(s);

    for (i = 0; i < slen; i++) {
        switch(s[i]) {
        case '\\':
            switch(s[i+1]) {
            case 'a': *p++ = 0x7; i++; break;
            case 'b': *p++ = 0x8; i++; break;
            case 'f': *p++ = 0xc; i++; break;
            case 'n': *p++ = 0xa; i++; break;
            case 'r': *p++ = 0xd; i++; break;
            case 't': *p++ = 0x9; i++; break;
            case 'v': *p++ = 0xb; i++; break;
            case '\0': *p++ = '\\'; i++; break;
            case '\n': *p++ = ' '; i++; break;
            default:
                  if (s[i+1] == 'x') {
                    int val = 0;
                    int c = xdigitval(s[i+2]);
                    if (c == -1) {
                        *p++ = 'x';
                        i++;
                        break;
                    }
                    val = c;
                    c = xdigitval(s[i+3]);
                    if (c == -1) {
                        *p++ = val;
                        i += 2;
                        break;
                    }
                    val = (val*16)+c;
                    *p++ = val;
                    i += 3;
                    break;
                  } else if (s[i+1] >= '0' && s[i+1] <= '7')
                  {
                    int val = 0;
                    int c = odigitval(s[i+1]);
                    val = c;
                    c = odigitval(s[i+2]);
                    if (c == -1) {
                        *p++ = val;
                        i ++;
                        break;
                    }
                    val = (val*8)+c;
                    c = odigitval(s[i+3]);
                    if (c == -1) {
                        *p++ = val;
                        i += 2;
                        break;
                    }
                    val = (val*8)+c;
                    *p++ = val;
                    i += 3;
                  } else {
                    *p++ = s[i+1];
                    i++;
                  }
                  break;
            }
            break;
        default:
            *p++ = s[i];
            break;
        }
    }
    len = p-dest;
    *p = '\0';
    return len;
}

/* Returns a dynamically allocated copy of the current token in the
 * parser context. The function perform conversion of escapes if
 * the token is of type JIM_TT_ESC.
 *
 * Note that after the conversion, tokens that are grouped with
 * braces in the source code, are always recognizable from the
 * identical string obtained in a different way from the type.
 *
 * For exmple the string:
 *
 * {expand}$a
 * 
 * will return as first token "expand", of type JIM_TT_STR
 *
 * While the string:
 *
 * expand$a
 *
 * will return as first token "expand", of type JIM_TT_ESC
 */
char *JimParserGetToken(struct JimParserCtx *pc,
        int *lenPtr, int *typePtr, int *linePtr)
{
    const char *start, *end;
    char *token;
    int len;

    start = JimParserTstart(pc);
    end = JimParserTend(pc);
    if (start > end) {
        if (lenPtr) *lenPtr = 0;
        if (typePtr) *typePtr = JimParserTtype(pc);
        if (linePtr) *linePtr = JimParserTline(pc);
        token = Jim_Alloc(1);
        token[0] = '\0';
        return token;
    }
    len = (end-start)+1;
    token = Jim_Alloc(len+1);
    if (JimParserTtype(pc) != JIM_TT_ESC) {
        /* No escape conversion needed? Just copy it. */
        memcpy(token, start, len);
        token[len] = '\0';
    } else {
        /* Else convert the escape chars. */
        len = JimEscape(token, start, len);
    }
    if (lenPtr) *lenPtr = len;
    if (typePtr) *typePtr = JimParserTtype(pc);
    if (linePtr) *linePtr = JimParserTline(pc);
    return token;
}

/* The following functin is not really part of the parsing engine of Jim,
 * but it somewhat related. Given an string and its length, it tries
 * to guess if the script is complete or there are instead " " or { }
 * open and not completed. This is useful for interactive shells
 * implementation and for [info complete].
 *
 * If 'stateCharPtr' != NULL, the function stores ' ' on complete script,
 * '{' on scripts incomplete missing one or more '}' to be balanced.
 * '"' on scripts incomplete missing a '"' char.
 *
 * If the script is complete, 1 is returned, otherwise 0. */
int Jim_ScriptIsComplete(const char *s, int len, char *stateCharPtr)
{
    int level = 0;
    int state = ' ';

    while(len) {
        switch (*s) {
            case '\\':
                if (len > 1)
                    s++;
                break;
            case '"':
                if (state == ' ') {
                    state = '"';
                } else if (state == '"') {
                    state = ' ';
                }
                break;
            case '{':
                if (state == '{') {
                    level++;
                } else if (state == ' ') {
                    state = '{';
                    level++;
                }
                break;
            case '}':
                if (state == '{') {
                    level--;
                    if (level == 0)
                        state = ' ';
                }
                break;
        }
        s++;
        len--;
    }
    if (stateCharPtr)
        *stateCharPtr = state;
    return state == ' ';
}

/* -----------------------------------------------------------------------------
 * Tcl Lists parsing
 * ---------------------------------------------------------------------------*/
static int JimParseListSep(struct JimParserCtx *pc);
static int JimParseListStr(struct JimParserCtx *pc);

int JimParseList(struct JimParserCtx *pc)
{
    if (pc->len == 0) {
        pc->tstart = pc->tend = pc->p;
        pc->tline = pc->linenr;
        pc->tt = JIM_TT_EOL;
        pc->eof = 1;
        return JIM_OK;
    }
    switch(*pc->p) {
    case ' ':
    case '\n':
    case '\t':
    case '\r':
        if (pc->state == JIM_PS_DEF)
            return JimParseListSep(pc);
        else
            return JimParseListStr(pc);
        break;
    default:
        return JimParseListStr(pc);
        break;
    }
    return JIM_OK;
}

int JimParseListSep(struct JimParserCtx *pc)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (*pc->p == ' ' || *pc->p == '\t' || *pc->p == '\r' || *pc->p == '\n')
    {
        pc->p++; pc->len--;
    }
    pc->tend = pc->p-1;
    pc->tt = JIM_TT_SEP;
    return JIM_OK;
}

int JimParseListStr(struct JimParserCtx *pc)
{
    int newword = (pc->tt == JIM_TT_SEP || pc->tt == JIM_TT_EOL ||
            pc->tt == JIM_TT_NONE);
    if (newword && *pc->p == '{') {
        return JimParseBrace(pc);
    } else if (newword && *pc->p == '"') {
        pc->state = JIM_PS_QUOTE;
        pc->p++; pc->len--;
    }
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (1) {
        if (pc->len == 0) {
            pc->tend = pc->p-1;
            pc->tt = JIM_TT_ESC;
            return JIM_OK;
        }
        switch(*pc->p) {
        case '\\':
            pc->p++; pc->len--;
            break;
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            if (pc->state == JIM_PS_DEF) {
                pc->tend = pc->p-1;
                pc->tt = JIM_TT_ESC;
                return JIM_OK;
            } else if (*pc->p == '\n') {
                pc->linenr++;
            }
            break;
        case '"':
            if (pc->state == JIM_PS_QUOTE) {
                pc->tend = pc->p-1;
                pc->tt = JIM_TT_ESC;
                pc->p++; pc->len--;
                pc->state = JIM_PS_DEF;
                return JIM_OK;
            }
            break;
        }
        pc->p++; pc->len--;
    }
    return JIM_OK; /* unreached */
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
    } else {
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
    if (objPtr->refCount != 0)  {
        Jim_Panic(interp,"!!!Object %p freed with bad refcount %d, type=%s", objPtr,
                objPtr->refCount, objPtr->typePtr ? objPtr->typePtr->name : "<none>");
    }

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
    /* Link the object into the free objects list */
    objPtr->prevObjPtr = NULL;
    objPtr->nextObjPtr = interp->freeList;
    if (interp->freeList)
        interp->freeList->prevObjPtr = objPtr;
    interp->freeList = objPtr;
    objPtr->refCount = -1;
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

#define Jim_SetStringRep(o, b, l) \
    do { (o)->bytes = b; (o)->length = l; } while (0)

/* Set the initial string representation for an object.
 * Does not try to free an old one. */
void Jim_InitStringRep(Jim_Obj *objPtr, const char *bytes, int length)
{
    if (length == 0) {
        objPtr->bytes = JimEmptyStringRep;
        objPtr->length = 0;
    } else {
        objPtr->bytes = Jim_Alloc(length+1);
        objPtr->length = length;
        memcpy(objPtr->bytes, bytes, length);
        objPtr->bytes[length] = '\0';
    }
}

/* Duplicate an object. The returned object has refcount = 0. */
Jim_Obj *Jim_DuplicateObj(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_Obj *dupPtr;

    dupPtr = Jim_NewObj(interp);
    if (objPtr->bytes == NULL) {
        /* Object does not have a valid string representation. */
        dupPtr->bytes = NULL;
    } else {
        Jim_InitStringRep(dupPtr, objPtr->bytes, objPtr->length);
    }
    if (objPtr->typePtr != NULL) {
        if (objPtr->typePtr->dupIntRepProc == NULL) {
            dupPtr->internalRep = objPtr->internalRep;
        } else {
            objPtr->typePtr->dupIntRepProc(interp, objPtr, dupPtr);
        }
        dupPtr->typePtr = objPtr->typePtr;
    } else {
        dupPtr->typePtr = NULL;
    }
    return dupPtr;
}

/* Return the string representation for objPtr. If the object
 * string representation is invalid, calls the method to create
 * a new one starting from the internal representation of the object. */
const char *Jim_GetString(Jim_Obj *objPtr, int *lenPtr)
{
    if (objPtr->bytes == NULL) {
        /* Invalid string repr. Generate it. */
        if (objPtr->typePtr->updateStringProc == NULL) {
            Jim_Panic(NULL,"UpdateStringProc called against '%s' type.",
                objPtr->typePtr->name);
        }
        objPtr->typePtr->updateStringProc(objPtr);
    }
    if (lenPtr)
        *lenPtr = objPtr->length;
    return objPtr->bytes;
}

/* Just returns the length of the object's string rep */
int Jim_Length(Jim_Obj *objPtr)
{
    int len;

    Jim_GetString(objPtr, &len);
    return len;
}

static void FreeDictSubstInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupDictSubstInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr,
        Jim_Obj *dupPtr);

static const Jim_ObjType dictSubstObjType = {
    "dict-substitution",
    FreeDictSubstInternalRep,
    DupDictSubstInternalRep,
    NULL,
    JIM_TYPE_NONE,
};

static void FreeInterpolatedInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_DecrRefCount(interp, (Jim_Obj *)objPtr->internalRep.twoPtrValue.ptr2);
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

void DupStringInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    JIM_NOTUSED(interp);

    /* This is a bit subtle: the only caller of this function
     * should be Jim_DuplicateObj(), that will copy the
     * string representaion. After the copy, the duplicated
     * object will not have more room in teh buffer than
     * srcPtr->length bytes. So we just set it to length. */
    dupPtr->internalRep.strValue.maxLength = srcPtr->length;
}

int SetStringFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    /* Get a fresh string representation. */
    (void) Jim_GetString(objPtr, NULL);
    /* Free any other internal representation. */
    Jim_FreeIntRep(interp, objPtr);
    /* Set it as string, i.e. just set the maxLength field. */
    objPtr->typePtr = &stringObjType;
    objPtr->internalRep.strValue.maxLength = objPtr->length;
    return JIM_OK;
}

Jim_Obj *Jim_NewStringObj(Jim_Interp *interp, const char *s, int len)
{
    Jim_Obj *objPtr = Jim_NewObj(interp);

    if (len == -1)
        len = strlen(s);
    /* Alloc/Set the string rep. */
    if (len == 0) {
        objPtr->bytes = JimEmptyStringRep;
        objPtr->length = 0;
    } else {
        objPtr->bytes = Jim_Alloc(len+1);
        objPtr->length = len;
        memcpy(objPtr->bytes, s, len);
        objPtr->bytes[len] = '\0';
    }

    /* No typePtr field for the vanilla string object. */
    objPtr->typePtr = NULL;
    return objPtr;
}

/* This version does not try to duplicate the 's' pointer, but
 * use it directly. */
Jim_Obj *Jim_NewStringObjNoAlloc(Jim_Interp *interp, char *s, int len)
{
    Jim_Obj *objPtr = Jim_NewObj(interp);

    if (len == -1)
        len = strlen(s);
    Jim_SetStringRep(objPtr, s, len);
    objPtr->typePtr = NULL;
    return objPtr;
}

/* Low-level string append. Use it only against objects
 * of type "string". */
void StringAppendString(Jim_Obj *objPtr, const char *str, int len)
{
    int needlen;

    if (len == -1)
        len = strlen(str);
    needlen = objPtr->length + len;
    if (objPtr->internalRep.strValue.maxLength < needlen ||
        objPtr->internalRep.strValue.maxLength == 0) {
        if (objPtr->bytes == JimEmptyStringRep) {
            objPtr->bytes = Jim_Alloc((needlen*2)+1);
        } else {
            objPtr->bytes = Jim_Realloc(objPtr->bytes, (needlen*2)+1);
        }
        objPtr->internalRep.strValue.maxLength = needlen*2;
    }
    memcpy(objPtr->bytes + objPtr->length, str, len);
    objPtr->bytes[objPtr->length+len] = '\0';
    objPtr->length += len;
}

/* Higher level API to append strings to objects. */
void Jim_AppendString(Jim_Interp *interp, Jim_Obj *objPtr, const char *str,
        int len)
{
    if (Jim_IsShared(objPtr))
        Jim_Panic(interp,"Jim_AppendString called with shared object");
    if (objPtr->typePtr != &stringObjType)
        SetStringFromAny(interp, objPtr);
    StringAppendString(objPtr, str, len);
}

void Jim_AppendObj(Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj *appendObjPtr)
{
    int len;
    const char *str;

    str = Jim_GetString(appendObjPtr, &len);
    Jim_AppendString(interp, objPtr, str, len);
}

void Jim_AppendStrings(Jim_Interp *interp, Jim_Obj *objPtr, ...)
{
    va_list ap;

    if (objPtr->typePtr != &stringObjType)
        SetStringFromAny(interp, objPtr);
    va_start(ap, objPtr);
    while (1) {
        char *s = va_arg(ap, char*);

        if (s == NULL) break;
        Jim_AppendString(interp, objPtr, s, -1);
    }
    va_end(ap);
}

int Jim_StringEqObj(Jim_Obj *aObjPtr, Jim_Obj *bObjPtr, int nocase)
{
    const char *aStr, *bStr;
    int aLen, bLen, i;

    if (aObjPtr == bObjPtr) return 1;
    aStr = Jim_GetString(aObjPtr, &aLen);
    bStr = Jim_GetString(bObjPtr, &bLen);
    if (aLen != bLen) return 0;
    if (nocase == 0)
        return memcmp(aStr, bStr, aLen) == 0;
    for (i = 0; i < aLen; i++) {
        if (tolower((int)aStr[i]) != tolower((int)bStr[i]))
            return 0;
    }
    return 1;
}

int Jim_StringMatchObj(Jim_Obj *patternObjPtr, Jim_Obj *objPtr,
        int nocase)
{
    const char *pattern, *string;
    int patternLen, stringLen;

    pattern = Jim_GetString(patternObjPtr, &patternLen);
    string = Jim_GetString(objPtr, &stringLen);
    return JimStringMatch(pattern, patternLen, string, stringLen, nocase);
}

int Jim_StringCompareObj(Jim_Obj *firstObjPtr,
        Jim_Obj *secondObjPtr, int nocase)
{
    const char *s1, *s2;
    int l1, l2;

    s1 = Jim_GetString(firstObjPtr, &l1);
    s2 = Jim_GetString(secondObjPtr, &l2);
    return JimStringCompare(s1, l1, s2, l2, nocase);
}

/* Convert a range, as returned by Jim_GetRange(), into
 * an absolute index into an object of the specified length.
 * This function may return negative values, or values
 * bigger or equal to the length of the list if the index
 * is out of range. */
static int JimRelToAbsIndex(int len, int index)
{
    if (index < 0)
        return len + index;
    return index;
}

/* Convert a pair of index as normalize by JimRelToAbsIndex(),
 * into a range stored in *firstPtr, *lastPtr, *rangeLenPtr, suitable
 * for implementation of commands like [string range] and [lrange].
 *
 * The resulting range is guaranteed to address valid elements of
 * the structure. */
static void JimRelToAbsRange(int len, int first, int last,
        int *firstPtr, int *lastPtr, int *rangeLenPtr)
{
    int rangeLen;

    if (first > last) {
        rangeLen = 0;
    } else {
        rangeLen = last-first+1;
        if (rangeLen) {
            if (first < 0) {
                rangeLen += first;
                first = 0;
            }
            if (last >= len) {
                rangeLen -= (last-(len-1));
                last = len-1;
            }
        }
    }
    if (rangeLen < 0) rangeLen = 0;

    *firstPtr = first;
    *lastPtr = last;
    *rangeLenPtr = rangeLen;
}

Jim_Obj *Jim_StringRangeObj(Jim_Interp *interp,
        Jim_Obj *strObjPtr, Jim_Obj *firstObjPtr, Jim_Obj *lastObjPtr)
{
    int first, last;
    const char *str;
    int len, rangeLen;

    if (Jim_GetIndex(interp, firstObjPtr, &first) != JIM_OK ||
        Jim_GetIndex(interp, lastObjPtr, &last) != JIM_OK)
        return NULL;
    str = Jim_GetString(strObjPtr, &len);
    first = JimRelToAbsIndex(len, first);
    last = JimRelToAbsIndex(len, last);
    JimRelToAbsRange(len, first, last, &first, &last, &rangeLen);
    return Jim_NewStringObj(interp, str+first, rangeLen);
}

static Jim_Obj *JimStringToLower(Jim_Interp *interp, Jim_Obj *strObjPtr)
{
    char *buf;
    int i;
    if (strObjPtr->typePtr != &stringObjType) {
        SetStringFromAny(interp, strObjPtr);
    }

    buf = Jim_Alloc(strObjPtr->length+1);

    memcpy(buf, strObjPtr->bytes, strObjPtr->length+1);
    for (i = 0; i < strObjPtr->length; i++)
        buf[i] = tolower(buf[i]);
    return Jim_NewStringObjNoAlloc(interp, buf, strObjPtr->length);
}

static Jim_Obj *JimStringToUpper(Jim_Interp *interp, Jim_Obj *strObjPtr)
{
    char *buf;
    int i;
    if (strObjPtr->typePtr != &stringObjType) {
        SetStringFromAny(interp, strObjPtr);
    }

    buf = Jim_Alloc(strObjPtr->length+1);

    memcpy(buf, strObjPtr->bytes, strObjPtr->length+1);
    for (i = 0; i < strObjPtr->length; i++)
        buf[i] = toupper(buf[i]);
    return Jim_NewStringObjNoAlloc(interp, buf, strObjPtr->length);
}

static const char *trim_left(const char *str, const char *trimchars)
{
    return str + strspn(str, trimchars);
}

static void trim_right(char *str, const char *trimchars)
{
    char *p = str + strlen(str) - 1;
    char *end = str - 1;
    int c;

    while (p != end) {
        c = *p;
        if (strchr(trimchars, c) == 0) {
            break;
        }
        p--;
    }
    p[1] = 0;
}

static const char default_trim_chars[] = " \t\n\r";

static Jim_Obj *JimStringTrim(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *trimcharsObjPtr)
{
    char *buf;
    const char *trimchars = default_trim_chars;
    if (strObjPtr->typePtr != &stringObjType) {
        SetStringFromAny(interp, strObjPtr);
    }
    if (trimcharsObjPtr) {
        trimchars = Jim_GetString(trimcharsObjPtr, NULL);
    }

    buf = Jim_Alloc(strObjPtr->length+1);
    strcpy(buf, trim_left(strObjPtr->bytes, trimchars));
    trim_right(buf, trimchars);

    return Jim_NewStringObjNoAlloc(interp, buf, -1);
}

static Jim_Obj *JimStringTrimLeft(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *trimcharsObjPtr)
{
    const char *str = Jim_GetString(strObjPtr, NULL);
    const char *trimchars = default_trim_chars;

    if (trimcharsObjPtr) {
        trimchars = Jim_GetString(trimcharsObjPtr, NULL);
    }

    return Jim_NewStringObj(interp, trim_left(str, trimchars), -1);
}

static Jim_Obj *JimStringTrimRight(Jim_Interp *interp, Jim_Obj *strObjPtr, Jim_Obj *trimcharsObjPtr)
{
    char *buf;
    const char *trimchars = default_trim_chars;

    if (trimcharsObjPtr) {
        trimchars = Jim_GetString(trimcharsObjPtr, NULL);
    }


    if (strObjPtr->typePtr != &stringObjType) {
        SetStringFromAny(interp, strObjPtr);
    }

    buf = Jim_StrDup(strObjPtr->bytes);
    trim_right(buf, trimchars);

    return Jim_NewStringObjNoAlloc(interp, buf, -1);
}

/* This is the core of the [format] command.
 * TODO: Lots of things work - via a hack
 *       However, no format item can be >= JIM_MAX_FMT 
 */
#define JIM_MAX_FMT 2048
static Jim_Obj *Jim_FormatString_Inner(Jim_Interp *interp, Jim_Obj *fmtObjPtr,
        int objc, Jim_Obj *const *objv, char *sprintf_buf)
{
    const char *fmt;
    int fmtLen;
    Jim_Obj *resObjPtr;
    

    fmt = Jim_GetString(fmtObjPtr, &fmtLen);
    resObjPtr = Jim_NewStringObj(interp, "", 0);
    while (fmtLen) {
        const char *p = fmt;
        char spec[2], c;
        jim_wide wideValue;
        double doubleValue;
        /* we cheat and use Sprintf()! */
        char fmt_str[100];
        char *cp;
        int width;
        int ljust;
        int zpad;
        int spad;
        int altfm;
        int forceplus;
        int prec;
        int inprec;
        int haveprec;
        int accum;
        int buflen = 0;

        while (*fmt != '%' && fmtLen) {
            fmt++; fmtLen--;
        }
        Jim_AppendString(interp, resObjPtr, p, fmt-p);
        if (fmtLen == 0)
            break;
        fmt++; fmtLen--; /* skip '%' */
        if (*fmt == '%') {
            /* %% -> % */
            Jim_AppendString(interp, resObjPtr, "%", 1);
            fmt++; fmtLen--;
            continue;
        }
        zpad = 0;
        spad = 0;
        width = -1;
        ljust = 0;
        altfm = 0;
        forceplus = 0;
        inprec = 0;
        haveprec = 0;
        prec = -1; /* not found yet */
    next_fmt:
        if (fmtLen <= 0) {
                break;
        }
        switch (*fmt) {
            /* terminals */
        case 'b': /* binary - not all printfs() do this */
        case 's': /* string */
        case 'i': /* integer */
        case 'd': /* decimal */
        case 'x': /* hex */
        case 'X': /* CAP hex */
        case 'c': /* char */
        case 'o': /* octal */
        case 'u': /* unsigned */
        case 'f': /* float */
                break;
                
                /* non-terminals */
        case '+':
                forceplus = 1;
                fmt++;  fmtLen--;
                goto next_fmt;
                break;
        case ' ': /* sign space */
                spad = 1;
                fmt++;  fmtLen--;
                goto next_fmt;
                break;
        case '-':
                ljust = 1;
                fmt++;  fmtLen--;
                goto next_fmt;
                break;
        case '#':
                altfm = 1;
                fmt++; fmtLen--;
                goto next_fmt;
                
        case '.':
                inprec = 1;
                fmt++; fmtLen--;
                goto next_fmt;
                break;
        case '0':
                if (!inprec) {
                    /* zero pad */
                    zpad = 1;
                    fmt++;  fmtLen--;
                    goto next_fmt;
                    break;
                }
                /* fall through */
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
                accum = 0;
                while (isdigit(*fmt) && (fmtLen > 0)) {
                        accum = (accum * 10) + (*fmt - '0');
                        fmt++;  fmtLen--;
                }
                if (inprec) {
                        haveprec = 1;
                        prec = accum;
                } else {
                        width = accum;
                }
                goto next_fmt;
        case '*':
                /* suck up the next item as an integer */
                fmt++;  fmtLen--;
                objc--;
                if (objc <= 0) {
                        goto not_enough_args;
                }
                if (Jim_GetWide(interp,objv[0],&wideValue )== JIM_ERR) {
                        Jim_FreeNewObj(interp, resObjPtr );
                        return NULL;
                }
                if (inprec) {
                        haveprec = 1;
                        prec = wideValue;
                        if (prec < 0) {
                                /* man 3 printf says */
                                /* if prec is negative, it is zero */
                                prec = 0;
                        }
                } else {
                width = wideValue;
                if (width < 0) {
                        ljust = 1;
                        width = -width;
                }
                }
                objv++;
                goto next_fmt;
                break;
        }
        
        
        if (*fmt != '%') {
            if (objc == 0) {
                    not_enough_args:
                Jim_FreeNewObj(interp, resObjPtr);
                Jim_SetResultString(interp, "not enough arguments for all format specifiers", -1);
                return NULL;
            } else {
                objc--;
            }
        }
        
        /*
         * Create the formatter
         * cause we cheat and use sprintf()
         */
        cp = fmt_str;
        *cp++ = '%';
        if (altfm) {
            *cp++ = '#';
        }
        if (forceplus) {
            *cp++ = '+';
        } else if (spad) {
            /* PLUS overrides */
            *cp++ = ' ';
        }
        if (ljust) {
            *cp++ = '-';
        }
        if (zpad ) {
            *cp++ = '0';
        }
        if (width > 0) {
            sprintf( cp, "%d", width );
            /* skip ahead */
            cp = strchr(cp,0);
        }
        /* did we find a period? */
        if (inprec) {
            /* then add it */
            *cp++ = '.';
            /* did something occur after the period? */
            if (haveprec) {
                sprintf( cp, "%d", prec );
            }
            cp = strchr(cp,0);
        }
        *cp = 0;

        /* here we do the work */
        /* actually - we make sprintf() do it for us */
        switch(*fmt) {
        case 's':
            *cp++ = 's';
            *cp   = 0;
            /* BUG: we do not handled embeded NULLs */
            buflen = snprintf( sprintf_buf, JIM_MAX_FMT, fmt_str, Jim_GetString( objv[0], NULL ));
            break;
        case 'c':
            *cp++ = 'c';
            *cp   = 0;
            if (Jim_GetWide(interp, objv[0], &wideValue) == JIM_ERR) {
                Jim_FreeNewObj(interp, resObjPtr);
                return NULL;
            }
            c = (char) wideValue;
            buflen = snprintf( sprintf_buf, JIM_MAX_FMT, fmt_str, c );
            break;
        case 'f':
        case 'F':
        case 'g':
        case 'G':
        case 'e':
        case 'E':
            *cp++ = *fmt;
            *cp   = 0;
            if (Jim_GetDouble( interp, objv[0], &doubleValue ) == JIM_ERR) {
                Jim_FreeNewObj( interp, resObjPtr );
                return NULL;
            }
            buflen = snprintf( sprintf_buf, JIM_MAX_FMT, fmt_str, doubleValue );
            break;
        case 'b':
        case 'd':
        case 'o':
        case 'i':
        case 'u':
        case 'x':
        case 'X':
            /* jim widevaluse are 64bit */
            if (sizeof(jim_wide) == sizeof(long long)) {
                *cp++ = 'l'; 
                *cp++ = 'l';
            } else {
                *cp++ = 'l';
            }
            *cp++ = *fmt;
            *cp   = 0;
            if (Jim_GetWide(interp, objv[0], &wideValue) == JIM_ERR) {
                Jim_FreeNewObj(interp, resObjPtr);
                return NULL;
            }
            buflen = snprintf(sprintf_buf, JIM_MAX_FMT, fmt_str, wideValue );
            break;
        case '%':
            sprintf_buf[0] = '%';
            sprintf_buf[1] = 0;
            objv--; /* undo the objv++ below */
            break;
        default:
            spec[0] = *fmt; spec[1] = '\0';
            Jim_SetResultFormatted(interp, "bad field specifier \"%s\"", spec);
            return NULL;
        }
        /* force terminate */
#if 0
        printf("FMT was: %s\n", fmt_str );
        printf("RES was: |%s|\n", sprintf_buf );
#endif

        Jim_AppendString( interp, resObjPtr, sprintf_buf, buflen <= JIM_MAX_FMT ? buflen : JIM_MAX_FMT);
        /* next obj */
        objv++;
        fmt++;
        fmtLen--;
    }
    return resObjPtr;
}

Jim_Obj *Jim_FormatString(Jim_Interp *interp, Jim_Obj *fmtObjPtr,
        int objc, Jim_Obj *const *objv)
{
    char *sprintf_buf=malloc(JIM_MAX_FMT);
    Jim_Obj *t=Jim_FormatString_Inner(interp, fmtObjPtr, objc, objv, sprintf_buf);
    free(sprintf_buf);
    return t; 
}

/* -----------------------------------------------------------------------------
 * Compared String Object
 * ---------------------------------------------------------------------------*/

/* This is strange object that allows to compare a C literal string
 * with a Jim object in very short time if the same comparison is done
 * multiple times. For example every time the [if] command is executed,
 * Jim has to check if a given argument is "else". This comparions if
 * the code has no errors are true most of the times, so we can cache
 * inside the object the pointer of the string of the last matching
 * comparison. Because most C compilers perform literal sharing,
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
int Jim_CompareStringImmediate(Jim_Interp *interp, Jim_Obj *objPtr,
        const char *str)
{
    if (objPtr->typePtr == &comparedStringObjType &&
        objPtr->internalRep.ptr == str)
        return 1;
    else {
        const char *objStr = Jim_GetString(objPtr, NULL);
        if (strcmp(str, objStr) != 0) return 0;
        if (objPtr->typePtr != &comparedStringObjType) {
            Jim_FreeIntRep(interp, objPtr);
            objPtr->typePtr = &comparedStringObjType;
        }
        objPtr->internalRep.ptr = (char*)str; /*ATTENTION: const cast */
        return 1;
    }
}

int qsortCompareStringPointers(const void *a, const void *b)
{
    char * const *sa = (char * const *)a;
    char * const *sb = (char * const *)b;
    return strcmp(*sa, *sb);
}


/* -----------------------------------------------------------------------------
 * Source Object
 *
 * This object is just a string from the language point of view, but
 * in the internal representation it contains the filename and line number
 * where this given token was read. This information is used by
 * Jim_EvalObj() if the object passed happens to be of type "source".
 *
 * This allows to propagate the information about line numbers and file
 * names and give error messages with absolute line numbers.
 *
 * Note that this object uses shared strings for filenames, and the
 * pointer to the filename together with the line number is taken into
 * the space for the "inline" internal representation of the Jim_Object,
 * so there is almost memory zero-overhead.
 *
 * Also the object will be converted to something else if the given
 * token it represents in the source file is not something to be
 * evaluated (not a script), and will be specialized in some other way,
 * so the time overhead is also null.
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
    Jim_ReleaseSharedString(interp,
            objPtr->internalRep.sourceValue.fileName);
}

void DupSourceInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    dupPtr->internalRep.sourceValue.fileName =
        Jim_GetSharedString(interp,
                srcPtr->internalRep.sourceValue.fileName);
    dupPtr->internalRep.sourceValue.lineNumber =
        dupPtr->internalRep.sourceValue.lineNumber;
    dupPtr->typePtr = &sourceObjType;
}

static void JimSetSourceInfo(Jim_Interp *interp, Jim_Obj *objPtr,
        const char *fileName, int lineNumber)
{
    if (Jim_IsShared(objPtr))
        Jim_Panic(interp,"JimSetSourceInfo called with shared object");
    if (objPtr->typePtr != NULL)
        Jim_Panic(interp,"JimSetSourceInfo called with typePtr != NULL");
    objPtr->internalRep.sourceValue.fileName =
        Jim_GetSharedString(interp, fileName);
    objPtr->internalRep.sourceValue.lineNumber = lineNumber;
    objPtr->typePtr = &sourceObjType;
}

/* -----------------------------------------------------------------------------
 * Script Object
 * ---------------------------------------------------------------------------*/

#define JIM_CMDSTRUCT_EXPAND -1

static void FreeScriptInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupScriptInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static int SetScriptFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

static const Jim_ObjType scriptObjType = {
    "script",
    FreeScriptInternalRep,
    DupScriptInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

/* The ScriptToken structure represents every token into a scriptObj.
 * Every token contains an associated Jim_Obj that can be specialized
 * by commands operating on it. */
typedef struct ScriptToken {
    int type;
    Jim_Obj *objPtr;
    int linenr;
} ScriptToken;

/* This is the script object internal representation. An array of
 * ScriptToken structures, with an associated command structure array.
 * The command structure is a pre-computed representation of the
 * command length and arguments structure as a simple liner array
 * of integers.
 * 
 * For example the script:
 *
 * puts hello
 * set $i $x$y [foo]BAR
 *
 * will produce a ScriptObj with the following Tokens:
 *
 * ESC puts
 * SEP
 * ESC hello
 * EOL
 * ESC set
 * EOL
 * VAR i
 * SEP
 * VAR x
 * VAR y
 * SEP
 * CMD foo
 * ESC BAR
 * EOL
 *
 * This is a description of the tokens, separators, and of lines.
 * The command structure instead represents the number of arguments
 * of every command, followed by the tokens of which every argument
 * is composed. So for the example script, the cmdstruct array will
 * contain:
 *
 * 2 1 1 4 1 1 2 2
 *
 * Because "puts hello" has two args (2), composed of single tokens (1 1)
 * While "set $i $x$y [foo]BAR" has four (4) args, the first two
 * composed of single tokens (1 1) and the last two of double tokens
 * (2 2).
 *
 * The precomputation of the command structure makes Jim_Eval() faster,
 * and simpler because there aren't dynamic lengths / allocations.
 *
 * -- {expand}/{*} handling --
 *
 * Expand is handled in a special way. When a command
 * contains at least an argument with the {expand} or {*} prefix,
 * the command structure presents a -1 before the integer
 * describing the number of arguments. This is used in order
 * to send the command exection to a different path in case
 * of {expand} and guarantee a fast path for the more common
 * case. Also, the integers describing the number of tokens
 * are expressed with negative sign, to allow for fast check
 * of what's an {expand}-prefixed argument and what not.
 *
 * For example the command:
 *
 * list {expand}{1 2}
 *
 * Will produce the following cmdstruct array:
 *
 * -1 2 1 -2
 *
 * -- the substFlags field of the structure --
 *
 * The scriptObj structure is used to represent both "script" objects
 * and "subst" objects. In the second case, the cmdStruct related
 * fields are not used at all, but there is an additional field used
 * that is 'substFlags': this represents the flags used to turn
 * the string into the internal representation used to perform the
 * substitution. If this flags are not what the application requires
 * the scriptObj is created again. For example the script:
 *
 * subst -nocommands $string
 * subst -novariables $string
 *
 * Will recreate the internal representation of the $string object
 * two times.
 */
typedef struct ScriptObj {
    int len; /* Length as number of tokens. */
    ScriptToken *token; /* Tokens array. */
    int *cmdStruct; /* commands structure */
    int csLen; /* length of the cmdStruct array. */
    int substFlags; /* flags used for the compilation of "subst" objects */
    int inUse; /* Used to share a ScriptObj. Currently
              only used by Jim_EvalObj() as protection against
              shimmering of the currently evaluated object. */
    char *fileName;
} ScriptObj;

void FreeScriptInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int i;
    struct ScriptObj *script = (void*) objPtr->internalRep.ptr;

    script->inUse--;
    if (script->inUse != 0) return;
    for (i = 0; i < script->len; i++) {
        if (script->token[i].objPtr != NULL) {
            Jim_DecrRefCount(interp, script->token[i].objPtr);
        }
    }
    Jim_Free(script->token);
    Jim_Free(script->cmdStruct);
    Jim_Free(script->fileName);
    Jim_Free(script);
}

void DupScriptInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    JIM_NOTUSED(interp);
    JIM_NOTUSED(srcPtr);

    /* Just returns an simple string. */
    dupPtr->typePtr = NULL;
}

/* A simple parser token.
 * All the simple tokens for the script point into the same script string rep.
 */
typedef struct {
    const char *token;  /* Pointer to the start of the token */
    int len;            /* Length of this token */
    int type;           /* Token type */
    int line;           /* Line number */
} ParseToken;

/* A list of parsed tokens representing a script.
 * Tokens are added to this list as the script is parsed.
 * It grows as needed.
 */
typedef struct {
    /* Start with a statically allocated list of tokens which will be expanded with realloc if needed */
    ParseToken *list;  /* Array of tokens */
    int size;       /* Current size of the list */
    int count;      /* Number of entries used */
    ParseToken static_list[20];   /* Small initial token space to avoid allocation */
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
static void ScriptAddToken(ParseTokenList *tokenlist, const char *token, int len, int type, int line)
{
    ParseToken *t;

    if (tokenlist->count == tokenlist->size) {
        /* Resize the list */
        tokenlist->size *= 2;
        if (tokenlist->list != tokenlist->static_list) {
            tokenlist->list = Jim_Realloc(tokenlist->list, tokenlist->size * sizeof(*tokenlist->list));
        }
        else {
            /* The list needs to become allocated */
            tokenlist->list = Jim_Alloc(tokenlist->size * sizeof(*tokenlist->list));
            memcpy(tokenlist->list, tokenlist->static_list, tokenlist->count * sizeof(*tokenlist->list));
        }
    }
    t = &tokenlist->list[tokenlist->count++];
    t->token = token;
    t->len = len;
    t->type = type;
    t->line = line;
}

/**
 * Takes a tokenlist and creates the allocated list of script tokens
 * in script->token, of length script->len.
 *
 * Unnecessary tokens are discarded, and some tokens may be consolidated into
 * a single token.
 *
 * Also counts the required cmdStruct length in script->csLen.
 */
static void ScriptObjAddTokens(Jim_Interp *interp, struct ScriptObj *script, ParseTokenList *tokenlist)
{
    int i;
    struct ScriptToken *token;
    int prevtype = JIM_TT_EOL;

    /* Be pessimistic. This will definitely be big enough since at least the EOF token
     * will be discarded
     */
    token = script->token = Jim_Alloc(sizeof(ScriptToken)*tokenlist->count);
    script->csLen = 0;

    for (i = 0; i < tokenlist->count; i++) {
        const ParseToken *t = &tokenlist->list[i];

        if (t->type == JIM_TT_EOF) {
            break;
        }

        switch (t->type) {
            case JIM_TT_EOL:
                /* Combine multiple EOLs to one */
                if (prevtype == JIM_TT_EOL) {
                    continue;
                }
                token->objPtr = interp->emptyObj;
                script->csLen += 2;
                break;

            case JIM_TT_SEP:
                /* Skip SEP before or after EOL */
                if (prevtype == JIM_TT_EOL || t[1].type == JIM_TT_EOL) {
                    continue;
                }
                token->objPtr = interp->emptyObj;
                script->csLen++;
                break;

            default: {
                char *str;
                int len = t->len;

                if (t->type == JIM_TT_ESC) {
                    /* Convert the escape chars. */
                    str = Jim_Alloc(len+1);
                    len = JimEscape(str, t->token, len);
                }
                else {
                    /* No escape conversion needed, so just copy it. */
                    str = Jim_StrDupLen(t->token, len);
                }

                /* Every object is initially a string, but the
                 * internal type may be specialized during execution of the
                 * script. */
                token->objPtr = Jim_NewStringObjNoAlloc(interp, str, len);

                if (script->fileName) {
                    JimSetSourceInfo(interp, token->objPtr, script->fileName, t->line);
                }
                break;
            }
        }

        token->type = t->type;
        token->linenr = t->line;

        Jim_IncrRefCount(token->objPtr);
        token++;

        prevtype = t->type;
    }

    script->len = token - script->token;
}

#ifdef JIM_OPTIMIZATION
/**
 * An optimised version of ScriptObjAddTokens() for subst objects.
 */
static void SubstObjAddTokens(Jim_Interp *interp, struct ScriptObj *script, ParseTokenList *tokenlist)
{
    int i;
    struct ScriptToken *token;

    token = script->token = Jim_Alloc(sizeof(ScriptToken)*tokenlist->count);

    for (i = 0; i < tokenlist->count; i++) {
        ParseToken *t = &tokenlist->list[i];
        int len;
        char *str;

        /* Create a token for 't' */
        token->type = t->type;
        token->linenr = t->line;

        len = t->len;

        if (t->type != JIM_TT_ESC) {
            /* No escape conversion needed, so just copy it. */
            str = Jim_StrDupLen(t->token, len);
        }
        else {
            /* Else convert the escape chars. */
            str = Jim_Alloc(len+1);
            len = JimEscape(str, t->token, len);
        }

        /* Every object is initially a string, but the
         * internal type may be specialized during execution of the
         * script. */
        token->objPtr = Jim_NewStringObjNoAlloc(interp, str, len);

        /* To add source info to SEP and EOL tokens is useless because
         * they will never by called as arguments of Jim_EvalObj(). */
        Jim_IncrRefCount(token->objPtr);
        token++;
    }

    script->len = i;
}
#endif

/* This method takes the string representation of an object
 * as a Tcl script, and generates the pre-parsed internal representation
 * of the script. */
int SetScriptFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    int scriptTextLen;
    const char *scriptText = Jim_GetString(objPtr, &scriptTextLen);
    struct JimParserCtx parser;
    struct ScriptObj *script = Jim_Alloc(sizeof(*script));
    ScriptToken *token;
    int *cs;
    int i;
    int initialLineNumber;
    ParseTokenList tokenlist;
    int line_expand;
    int arg_expand;
    int *csp;
    int args;
    int tokens;

    /* Try to get information about filename / line number */
    if (objPtr->typePtr == &sourceObjType) {
        script->fileName =
            Jim_StrDup(objPtr->internalRep.sourceValue.fileName);
        initialLineNumber = objPtr->internalRep.sourceValue.lineNumber;
    } else {
        script->fileName = NULL;
        initialLineNumber = 1;
    }

    /* Initially parse the script into tokens (in tokenlist) */
    ScriptTokenListInit(&tokenlist);

    JimParserInit(&parser, scriptText, scriptTextLen, initialLineNumber);
    while(!JimParserEof(&parser)) {
        JimParseScript(&parser);
        ScriptAddToken(&tokenlist, parser.tstart, parser.tend - parser.tstart + 1, parser.tt, parser.tline);
    }
    /* Add a final EOF token */
    ScriptAddToken(&tokenlist, scriptText + scriptTextLen, 0, JIM_TT_EOF, 0);

    /* Create the "real" script tokens from the initial token list */
    script->substFlags = 0;
    script->inUse = 1;
    ScriptObjAddTokens(interp, script, &tokenlist);

    /* No longer need the token list */
    ScriptTokenListFree(&tokenlist);

    if (!script->fileName) {
        script->fileName = Jim_StrDup("");
    }

#if 0
    printf("==== Script ====\n");
    for (i = 0; i < script->len; i++) {
        printf("[%2d] %s (%d)'%s'\n", i, tt_name(script->token[i].type), script->token[i].objPtr->length, script->token[i].objPtr->bytes);
    }
#endif

    /* Compute the command structure array
     * (see the ScriptObj struct definition for more info).
     * Note that the required size has already been calculated in script->csLen.
     */

    cs = script->cmdStruct = Jim_Alloc(sizeof(int)*(script->csLen));

    token = script->token;

    line_expand = 0;  /* expand is used on this line */
    arg_expand = 0;   /* expand is used on this argument */
    csp = cs++;       /* points to argument count */
    args = 1;         /* Number of args on this line */
    tokens = 0;       /* Number of tokens in current argument. */

    for (i = 0; i < script->len; i++) {
        ScriptToken *t = &token[i];

        if (tokens == 0 && t[0].type == JIM_TT_STR &&
            t[1].type != JIM_TT_SEP && t[1].type != JIM_TT_EOL &&
            (!strcmp(t->objPtr->bytes, "expand") || !strcmp(t->objPtr->bytes, "*"))) {

            arg_expand = line_expand = 1;
        }

        if (t->type == JIM_TT_SEP || t->type == JIM_TT_EOL) {
            /* Now add info about the number of tokens. -ve is list expansion is involved */
            *cs++ = arg_expand ? -tokens : tokens;
            arg_expand = 0;
            tokens = 0;

            if (t->type == JIM_TT_EOL) {
                /* End of line. Back patch the arg count */
                /* Negative value if there is list expansion involved. */
                if (line_expand) {
                    line_expand = 0;
                    *csp = -args;
                }
                else {
                    *csp = args;
                }
                /* And reset */
                csp = cs++;
                args = 0;
            }
            args++;
        }
        else {
            tokens++;
        }
    }
#if 0
    for (i = 0; i < script->csLen; i++) {
        printf("cs[%d]=%d\n", i, script->cmdStruct[i]);
    }
#endif

    /* Free the old internal rep and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    Jim_SetIntRepPtr(objPtr, script);
    objPtr->typePtr = &scriptObjType;

    return JIM_OK;
}

ScriptObj *Jim_GetScript(Jim_Interp *interp, Jim_Obj *objPtr)
{
    struct ScriptObj *script = Jim_GetIntRepPtr(objPtr);

    if (objPtr->typePtr != &scriptObjType || script->substFlags) {
        SetScriptFromAny(interp, objPtr);
    }
    return (ScriptObj*) Jim_GetIntRepPtr(objPtr);
}

/* -----------------------------------------------------------------------------
 * Commands
 * ---------------------------------------------------------------------------*/

/* Commands HashTable Type.
 *
 * Keys are dynamic allocated strings, Values are Jim_Cmd structures. */
static void Jim_CommandsHT_ValDestructor(void *interp, void *val)
{
    Jim_Cmd *cmdPtr = (void*) val;

    if (cmdPtr->cmdProc == NULL) {
        Jim_DecrRefCount(interp, cmdPtr->argListObjPtr);
        Jim_DecrRefCount(interp, cmdPtr->bodyObjPtr);
        if (cmdPtr->staticVars) {
            Jim_FreeHashTable(cmdPtr->staticVars);
            Jim_Free(cmdPtr->staticVars);
        }
    } else if (cmdPtr->delProc != NULL) {
            /* If it was a C coded command, call the delProc if any */
            cmdPtr->delProc(interp, cmdPtr->privData);
    }
    Jim_Free(val);
}

static const Jim_HashTableType JimCommandsHashTableType = {
    JimStringCopyHTHashFunction,        /* hash function */
    JimStringCopyHTKeyDup,        /* key dup */
    NULL,                    /* val dup */
    JimStringCopyHTKeyCompare,        /* key compare */
    JimStringCopyHTKeyDestructor,        /* key destructor */
    Jim_CommandsHT_ValDestructor        /* val destructor */
};

/* ------------------------- Commands related functions --------------------- */

int Jim_CreateCommand(Jim_Interp *interp, const char *cmdName,
        Jim_CmdProc cmdProc, void *privData, Jim_DelCmdProc delProc)
{
    Jim_HashEntry *he;
    Jim_Cmd *cmdPtr;

    he = Jim_FindHashEntry(&interp->commands, cmdName);
    if (he == NULL) { /* New command to create */
        cmdPtr = Jim_Alloc(sizeof(*cmdPtr));
        Jim_AddHashEntry(&interp->commands, cmdName, cmdPtr);
    } else {
        Jim_InterpIncrProcEpoch(interp);
        /* Free the arglist/body objects if it was a Tcl procedure */
        cmdPtr = he->val;
        if (cmdPtr->cmdProc == NULL) {
            Jim_DecrRefCount(interp, cmdPtr->argListObjPtr);
            Jim_DecrRefCount(interp, cmdPtr->bodyObjPtr);
            if (cmdPtr->staticVars) {
                Jim_FreeHashTable(cmdPtr->staticVars);
                Jim_Free(cmdPtr->staticVars);
            }
            cmdPtr->staticVars = NULL;
        } else if (cmdPtr->delProc != NULL) {
            /* If it was a C coded command, call the delProc if any */
            cmdPtr->delProc(interp, cmdPtr->privData);
        }
    }

    /* Store the new details for this proc */
    cmdPtr->delProc = delProc;
    cmdPtr->cmdProc = cmdProc;
    cmdPtr->privData = privData;

    /* There is no need to increment the 'proc epoch' because
     * creation of a new procedure can never affect existing
     * cached commands. We don't do negative caching. */
    return JIM_OK;
}

int Jim_CreateProcedure(Jim_Interp *interp, const char *cmdName,
        Jim_Obj *argListObjPtr, Jim_Obj *staticsListObjPtr, Jim_Obj *bodyObjPtr,
        int leftArity, int optionalArgs, int args, int rightArity)
{
    Jim_Cmd *cmdPtr;

    cmdPtr = Jim_Alloc(sizeof(*cmdPtr));
    cmdPtr->cmdProc = NULL; /* Not a C coded command */
    cmdPtr->argListObjPtr = argListObjPtr;
    cmdPtr->bodyObjPtr = bodyObjPtr;
    Jim_IncrRefCount(argListObjPtr);
    Jim_IncrRefCount(bodyObjPtr);
    cmdPtr->leftArity = leftArity;
    cmdPtr->optionalArgs = optionalArgs;
    cmdPtr->args = args;
    cmdPtr->rightArity = rightArity;
    cmdPtr->staticVars = NULL;
   
    /* Create the statics hash table. */
    if (staticsListObjPtr) {
        int len, i;

        len = Jim_ListLength(interp, staticsListObjPtr);
        if (len != 0) {
            cmdPtr->staticVars = Jim_Alloc(sizeof(Jim_HashTable));
            Jim_InitHashTable(cmdPtr->staticVars, &JimVariablesHashTableType,
                    interp);
            for (i = 0; i < len; i++) {
                Jim_Obj *objPtr = 0, *initObjPtr = 0, *nameObjPtr = 0;
                Jim_Var *varPtr;
                int subLen;

                Jim_ListIndex(interp, staticsListObjPtr, i, &objPtr, JIM_NONE);
                /* Check if it's composed of two elements. */
                subLen = Jim_ListLength(interp, objPtr);
                if (subLen == 1 || subLen == 2) {
                    /* Try to get the variable value from the current
                     * environment. */
                    Jim_ListIndex(interp, objPtr, 0, &nameObjPtr, JIM_NONE);
                    if (subLen == 1) {
                        initObjPtr = Jim_GetVariable(interp, nameObjPtr,
                                JIM_NONE);
                        if (initObjPtr == NULL) {
                            Jim_SetResultFormatted(interp,
                                "variable for initialization of static \"%#s\" not found in the local context",
                                nameObjPtr);
                            goto err;
                        }
                    } else {
                        Jim_ListIndex(interp, objPtr, 1, &initObjPtr, JIM_NONE);
                    }
                    varPtr = Jim_Alloc(sizeof(*varPtr));
                    varPtr->objPtr = initObjPtr;
                    Jim_IncrRefCount(initObjPtr);
                    varPtr->linkFramePtr = NULL;
                    if (Jim_AddHashEntry(cmdPtr->staticVars,
                            Jim_GetString(nameObjPtr, NULL),
                            varPtr) != JIM_OK)
                    {
                        Jim_SetResultFormatted(interp,
                            "static variable name \"%#s\" duplicated in statics list", 
                            nameObjPtr);
                        Jim_DecrRefCount(interp, initObjPtr);
                        Jim_Free(varPtr);
                        goto err;
                    }
                } else {
                    Jim_SetResultFormatted(interp, "too many fields in static specifier \"%#s\"", objPtr);
                    goto err;
                }
            }
        }
    }

    /* Add the new command */

    /* it may already exist, so we try to delete the old one */
    if (Jim_DeleteHashEntry(&interp->commands, cmdName) != JIM_ERR) {
        /* There was an old procedure with the same name, this requires
         * a 'proc epoch' update. */
        Jim_InterpIncrProcEpoch(interp);
    }
    /* If a procedure with the same name didn't existed there is no need
     * to increment the 'proc epoch' because creation of a new procedure
     * can never affect existing cached commands. We don't do
     * negative caching. */
    Jim_AddHashEntry(&interp->commands, cmdName, cmdPtr);
    return JIM_OK;

err:
    Jim_FreeHashTable(cmdPtr->staticVars);
    Jim_Free(cmdPtr->staticVars);
    Jim_DecrRefCount(interp, argListObjPtr);
    Jim_DecrRefCount(interp, bodyObjPtr);
    Jim_Free(cmdPtr);
    return JIM_ERR;
}

int Jim_DeleteCommand(Jim_Interp *interp, const char *cmdName)
{
    if (Jim_DeleteHashEntry(&interp->commands, cmdName) == JIM_ERR)
        return JIM_ERR;
    Jim_InterpIncrProcEpoch(interp);
    return JIM_OK;
}

int Jim_RenameCommand(Jim_Interp *interp, const char *oldName, 
        const char *newName)
{
    Jim_Cmd *cmdPtr;
    Jim_HashEntry *he;
    Jim_Cmd *copyCmdPtr;

    if (newName[0] == '\0') /* Delete! */
        return Jim_DeleteCommand(interp, oldName);
    /* Rename */
    he = Jim_FindHashEntry(&interp->commands, oldName);
    if (he == NULL)
        return JIM_ERR; /* Invalid command name */
    cmdPtr = he->val;
    copyCmdPtr = Jim_Alloc(sizeof(Jim_Cmd));
    *copyCmdPtr = *cmdPtr;
    /* In order to avoid that a procedure will get arglist/body/statics
     * freed by the hash table methods, fake a C-coded command
     * setting cmdPtr->cmdProc as not NULL */
    cmdPtr->cmdProc = (void*)1;
    /* Also make sure delProc is NULL. */
    cmdPtr->delProc = NULL;
    /* Destroy the old command, and make sure the new is freed
     * as well. */
    Jim_DeleteHashEntry(&interp->commands, oldName);
    Jim_DeleteHashEntry(&interp->commands, newName);
    /* Now the new command. We are sure it can't fail because
     * the target name was already freed. */
    Jim_AddHashEntry(&interp->commands, newName, copyCmdPtr);
    /* Increment the epoch */
    Jim_InterpIncrProcEpoch(interp);
    return JIM_OK;
}

/* -----------------------------------------------------------------------------
 * Command object
 * ---------------------------------------------------------------------------*/

static int SetCommandFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

static const Jim_ObjType commandObjType = {
    "command",
    NULL,
    NULL,
    NULL,
    JIM_TYPE_REFERENCES,
};

int SetCommandFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_HashEntry *he;
    const char *cmdName;

    /* Get the string representation */
    cmdName = Jim_GetString(objPtr, NULL);
    /* Lookup this name into the commands hash table */
    he = Jim_FindHashEntry(&interp->commands, cmdName);
    if (he == NULL)
        return JIM_ERR;

    /* Free the old internal repr and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &commandObjType;
    objPtr->internalRep.cmdValue.procEpoch = interp->procEpoch;
    objPtr->internalRep.cmdValue.cmdPtr = (void*)he->val;
    return JIM_OK;
}

/* This function returns the command structure for the command name
 * stored in objPtr. It tries to specialize the objPtr to contain
 * a cached info instead to perform the lookup into the hash table
 * every time. The information cached may not be uptodate, in such
 * a case the lookup is performed and the cache updated. */
Jim_Cmd *Jim_GetCommand(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    if ((objPtr->typePtr != &commandObjType ||
        objPtr->internalRep.cmdValue.procEpoch != interp->procEpoch) &&
        SetCommandFromAny(interp, objPtr) == JIM_ERR) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp, "invalid command name \"%#s\"", objPtr);
        }
        return NULL;
    }
    return objPtr->internalRep.cmdValue.cmdPtr;
}

/* -----------------------------------------------------------------------------
 * Variables
 * ---------------------------------------------------------------------------*/

/* Variables HashTable Type.
 *
 * Keys are dynamic allocated strings, Values are Jim_Var structures. */
static void JimVariablesHTValDestructor(void *interp, void *val)
{
    Jim_Var *varPtr = (void*) val;

    Jim_DecrRefCount(interp, varPtr->objPtr);
    Jim_Free(val);
}

static const Jim_HashTableType JimVariablesHashTableType = {
    JimStringCopyHTHashFunction,        /* hash function */
    JimStringCopyHTKeyDup,              /* key dup */
    NULL,                               /* val dup */
    JimStringCopyHTKeyCompare,        /* key compare */
    JimStringCopyHTKeyDestructor,     /* key destructor */
    JimVariablesHTValDestructor       /* val destructor */
};

/* -----------------------------------------------------------------------------
 * Variable object
 * ---------------------------------------------------------------------------*/

#define JIM_DICT_SUGAR 100 /* Only returned by SetVariableFromAny() */

static int SetVariableFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr);

static const Jim_ObjType variableObjType = {
    "variable",
    NULL,
    NULL,
    NULL,
    JIM_TYPE_REFERENCES,
};

/* Return true if the string "str" looks like syntax sugar for [dict]. I.e.
 * is in the form "varname(key)". */
static int Jim_NameIsDictSugar(const char *str, int len)
{
    if (len && str[len-1] == ')' && strchr(str, '(') != NULL)
        return 1;
    return 0;
}

/* This method should be called only by the variable API.
 * It returns JIM_OK on success (variable already exists),
 * JIM_ERR if it does not exists, JIM_DICT_SUGAR if it's not
 * a variable name, but syntax glue for [dict] i.e. the last
 * character is ')' */
int SetVariableFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    Jim_HashEntry *he;
    const char *varName;
    int len;
    Jim_CallFrame *framePtr = interp->framePtr;
    
    /* Check if the object is already an uptodate variable */
    if (objPtr->typePtr == &variableObjType && 
        objPtr->internalRep.varValue.callFrameId == framePtr->id) {
            return JIM_OK; /* nothing to do */
    }

    if (objPtr->typePtr == &dictSubstObjType) {
        return JIM_DICT_SUGAR;
    }

    /* Get the string representation */
    varName = Jim_GetString(objPtr, &len);

    /* Make sure it's not syntax glue to get/set dict. */
    if (Jim_NameIsDictSugar(varName, len)) {
        return JIM_DICT_SUGAR;
    }

    if (varName[0] == ':' && varName[1] == ':') {
        framePtr = interp->topFramePtr;
        he = Jim_FindHashEntry(&framePtr->vars, varName + 2);
        if (he == NULL) {
            return JIM_ERR;
        }
    }
    else {
        /* Lookup this name into the variables hash table */
        he = Jim_FindHashEntry(&framePtr->vars, varName);
        if (he == NULL) {
            /* Try with static vars. */
            if (framePtr->staticVars == NULL)
                return JIM_ERR;
            if (!(he = Jim_FindHashEntry(framePtr->staticVars, varName)))
                return JIM_ERR;
        }
    }
    /* Free the old internal repr and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &variableObjType;
    objPtr->internalRep.varValue.callFrameId = framePtr->id;
    objPtr->internalRep.varValue.varPtr = (void*)he->val;
    return JIM_OK;
}

/* -------------------- Variables related functions ------------------------- */
static int JimDictSugarSet(Jim_Interp *interp, Jim_Obj *ObjPtr,
        Jim_Obj *valObjPtr);
static Jim_Obj *JimDictSugarGet(Jim_Interp *interp, Jim_Obj *ObjPtr);

/* For now that's dummy. Variables lookup should be optimized
 * in many ways, with caching of lookups, and possibly with
 * a table of pre-allocated vars in every CallFrame for local vars.
 * All the caching should also have an 'epoch' mechanism similar
 * to the one used by Tcl for procedures lookup caching. */

int Jim_SetVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr, Jim_Obj *valObjPtr)
{
    const char *name;
    Jim_Var *var;
    int err;

    if ((err = SetVariableFromAny(interp, nameObjPtr)) != JIM_OK) {
        Jim_CallFrame *framePtr = interp->framePtr;

        /* Check for [dict] syntax sugar. */
        if (err == JIM_DICT_SUGAR)
            return JimDictSugarSet(interp, nameObjPtr, valObjPtr);
        /* New variable to create */
        name = Jim_GetString(nameObjPtr, NULL);

        var = Jim_Alloc(sizeof(*var));
        var->objPtr = valObjPtr;
        Jim_IncrRefCount(valObjPtr);
        var->linkFramePtr = NULL;
        /* Insert the new variable */
        if (name[0] == ':' && name[1] == ':') {
            /* Into to the top evel frame */
            framePtr = interp->topFramePtr;
            Jim_AddHashEntry(&framePtr->vars, name + 2, var);
        }
        else {
            Jim_AddHashEntry(&framePtr->vars, name, var);
        }
        /* Make the object int rep a variable */
        Jim_FreeIntRep(interp, nameObjPtr);
        nameObjPtr->typePtr = &variableObjType;
        nameObjPtr->internalRep.varValue.callFrameId = framePtr->id;
        nameObjPtr->internalRep.varValue.varPtr = var;
    } else {
        var = nameObjPtr->internalRep.varValue.varPtr;
        if (var->linkFramePtr == NULL) {
            Jim_IncrRefCount(valObjPtr);
            Jim_DecrRefCount(interp, var->objPtr);
            var->objPtr = valObjPtr;
        } else { /* Else handle the link */
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
    int len;

    varName = Jim_GetString(nameObjPtr, &len);

    if (Jim_FindHashEntry(&interp->framePtr->vars, varName)) {
        Jim_SetResultFormatted(interp, "variable \"%#s\" already exists", nameObjPtr);
        return JIM_ERR;
    }

    /* Check for cycles. */
    if (interp->framePtr == targetCallFrame) {
        Jim_Obj *objPtr = targetNameObjPtr;
        Jim_Var *varPtr;
        /* Cycles are only possible with 'uplevel 0' */
        while(1) {
            if (Jim_StringEqObj(objPtr, nameObjPtr, 0)) {
                Jim_SetResultString(interp,
                    "can't upvar from variable to itself", -1);
                return JIM_ERR;
            }
            if (SetVariableFromAny(interp, objPtr) != JIM_OK)
                break;
            varPtr = objPtr->internalRep.varValue.varPtr;
            if (varPtr->linkFramePtr != targetCallFrame) break;
            objPtr = varPtr->objPtr;
        }
    }
    if (Jim_NameIsDictSugar(varName, len)) {
        Jim_SetResultString(interp,
            "Dict key syntax invalid as link source", -1);
        return JIM_ERR;
    }
    /* Perform the binding */
    Jim_SetVariable(interp, nameObjPtr, targetNameObjPtr);
    /* We are now sure 'nameObjPtr' type is variableObjType */
    nameObjPtr->internalRep.varValue.varPtr->linkFramePtr = targetCallFrame;
    return JIM_OK;
}

/* Return the Jim_Obj pointer associated with a variable name,
 * or NULL if the variable was not found in the current context.
 * The same optimization discussed in the comment to the
 * 'SetVariable' function should apply here. */
Jim_Obj *Jim_GetVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr, int flags)
{
    switch (SetVariableFromAny(interp, nameObjPtr)) {
        case JIM_OK: {
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
                return objPtr;
            }
        }

        case JIM_DICT_SUGAR:
            /* [dict] syntax sugar. */
            return JimDictSugarGet(interp, nameObjPtr);

        default:
            if (flags & JIM_ERRMSG) {
                Jim_SetResultFormatted(interp, "can't read \"%#s\": no such variable", nameObjPtr);
            }
            return NULL;
    }
}

Jim_Obj *Jim_GetGlobalVariable(Jim_Interp *interp, Jim_Obj *nameObjPtr,
        int flags)
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

Jim_Obj *Jim_GetGlobalVariableStr(Jim_Interp *interp, const char *name,
        int flags)
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
    const char *name;
    Jim_Var *varPtr;
    int retval;
    
    retval = SetVariableFromAny(interp, nameObjPtr);
    if (retval == JIM_DICT_SUGAR) {
        /* [dict] syntax sugar. */
        return JimDictSugarSet(interp, nameObjPtr, NULL);
    }
    else if (retval == JIM_OK) {
        varPtr = nameObjPtr->internalRep.varValue.varPtr;

        /* If it's a link call UnsetVariable recursively */
        if (varPtr->linkFramePtr) {
            Jim_CallFrame *savedCallFrame;

            savedCallFrame = interp->framePtr;
            interp->framePtr = varPtr->linkFramePtr;
            retval = Jim_UnsetVariable(interp, varPtr->objPtr, JIM_NONE);
            interp->framePtr = savedCallFrame;
        } else {
            Jim_CallFrame *framePtr = interp->framePtr;

            name = Jim_GetString(nameObjPtr, NULL);
            if (name[0] == ':' && name[1] == ':') {
                framePtr = interp->topFramePtr;
                name += 2;
            }
            retval = Jim_DeleteHashEntry(&framePtr->vars, name);
            if (retval == JIM_OK) {
                /* Change the callframe id, invalidating var lookup caching */
                JimChangeCallFrameId(interp, framePtr);
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
 * of the variable to set, and the second with the rispective key.
 * For example "foo(bar)" will return objects with string repr. of
 * "foo" and "bar".
 *
 * The returned objects have refcount = 1. The function can't fail. */
static void JimDictSugarParseVarKey(Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj **varPtrPtr, Jim_Obj **keyPtrPtr)
{
    const char *str, *p;
    char *t;
    int len, keyLen, nameLen;
    Jim_Obj *varObjPtr, *keyObjPtr;

    str = Jim_GetString(objPtr, &len);

    p = strchr(str, '(');
    if (p == NULL) {
        Jim_Panic(interp, "JimDictSugarParseVarKey() called for non-dict-sugar (%s)", str);
    }
    p++;
    keyLen = len-((p-str)+1);
    nameLen = (p-str)-1;
    /* Create the objects with the variable name and key. */
    t = Jim_Alloc(nameLen+1);
    memcpy(t, str, nameLen);
    t[nameLen] = '\0';
    varObjPtr = Jim_NewStringObjNoAlloc(interp, t, nameLen);

    t = Jim_Alloc(keyLen+1);
    memcpy(t, p, keyLen);
    t[keyLen] = '\0';
    keyObjPtr = Jim_NewStringObjNoAlloc(interp, t, keyLen);

    Jim_IncrRefCount(varObjPtr);
    Jim_IncrRefCount(keyObjPtr);
    *varPtrPtr = varObjPtr;
    *keyPtrPtr = keyObjPtr;
}

/* Helper of Jim_SetVariable() to deal with dict-syntax variable names.
 * Also used by Jim_UnsetVariable() with valObjPtr = NULL. */
static int JimDictSugarSet(Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj *valObjPtr)
{
    int err;

    SetDictSubstFromAny(interp, objPtr);

    err = Jim_SetDictKeysVector(interp, objPtr->internalRep.dictSubstValue.varNameObjPtr,
        &objPtr->internalRep.dictSubstValue.indexObjPtr, 1, valObjPtr);

    if (err == JIM_OK) {
        /* Don't keep an extra ref to the result */
        Jim_SetEmptyResult(interp);
    }
    else {
        if (!valObjPtr) {
            /* Better error message for unset a(2) where a exists but a(2) doesn't */
            if (Jim_GetVariable(interp, objPtr->internalRep.dictSubstValue.varNameObjPtr, JIM_NONE)) {
                Jim_SetResultFormatted(interp, "can't unset \"%#s\": no such element in array", objPtr);
                return err;
            }
        }
        /* Make the error more informative and Tcl-compatible */
        Jim_SetResultFormatted(interp, "can't %s \"%#s\": variable isn't array", (valObjPtr ? "set" : "unset"), objPtr);
    }
    return err;
}

static Jim_Obj *JimDictExpandArrayVariable(Jim_Interp *interp, Jim_Obj *varObjPtr, Jim_Obj *keyObjPtr)
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
        resObjPtr = NULL;
        if (ret < 0) {
            Jim_SetResultFormatted(interp,
                "can't read \"%#s(%#s)\": variable isn't array", varObjPtr, keyObjPtr);
        }
        else {
            Jim_SetResultFormatted(interp,
                "can't read \"%#s(%#s)\": no such element in array", varObjPtr, keyObjPtr);
        }
    }

    return resObjPtr;
}

/* Helper of Jim_GetVariable() to deal with dict-syntax variable names */
static Jim_Obj *JimDictSugarGet(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_Obj *varObjPtr, *keyObjPtr, *resObjPtr;

    
    JimDictSugarParseVarKey(interp, objPtr, &varObjPtr, &keyObjPtr);

    resObjPtr = JimDictExpandArrayVariable(interp, varObjPtr, keyObjPtr);

    Jim_DecrRefCount(interp, varObjPtr);
    Jim_DecrRefCount(interp, keyObjPtr);

    return resObjPtr;
}

/* --------- $var(INDEX) substitution, using a specialized object ----------- */

void FreeDictSubstInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    Jim_DecrRefCount(interp, objPtr->internalRep.dictSubstValue.varNameObjPtr);
    Jim_DecrRefCount(interp, objPtr->internalRep.dictSubstValue.indexObjPtr);
}

void DupDictSubstInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr,
        Jim_Obj *dupPtr)
{
    JIM_NOTUSED(interp);

    dupPtr->internalRep.dictSubstValue.varNameObjPtr =
        srcPtr->internalRep.dictSubstValue.varNameObjPtr;
    dupPtr->internalRep.dictSubstValue.indexObjPtr =
        srcPtr->internalRep.dictSubstValue.indexObjPtr;
    dupPtr->typePtr = &dictSubstObjType;
}

/* Note: The object *must* be in dict-sugar format */
static void SetDictSubstFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (objPtr->typePtr != &dictSubstObjType) {
        Jim_Obj *varObjPtr, *keyObjPtr;

        if (objPtr->typePtr == &interpolatedObjType) {
            /* An interpolated object in dict-sugar form */

            const ScriptToken *token = objPtr->internalRep.twoPtrValue.ptr1;

            varObjPtr = token[0].objPtr;
            keyObjPtr = objPtr->internalRep.twoPtrValue.ptr2;

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
static Jim_Obj *Jim_ExpandDictSugar(Jim_Interp *interp, Jim_Obj *objPtr)
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
    resObjPtr = JimDictExpandArrayVariable(interp, objPtr->internalRep.dictSubstValue.varNameObjPtr, substKeyObjPtr);
    Jim_DecrRefCount(interp, substKeyObjPtr);

    return resObjPtr;
}

/* -----------------------------------------------------------------------------
 * CallFrame
 * ---------------------------------------------------------------------------*/

static Jim_CallFrame *JimCreateCallFrame(Jim_Interp *interp)
{
    Jim_CallFrame *cf;
    if (interp->freeFramesList) {
        cf = interp->freeFramesList;
        interp->freeFramesList = cf->nextFramePtr;
    } else {
        cf = Jim_Alloc(sizeof(*cf));
        cf->vars.table = NULL;
    }

    cf->id = interp->callFrameEpoch++;
    cf->parentCallFrame = NULL;
    cf->argv = NULL;
    cf->argc = 0;
    cf->procArgsObjPtr = NULL;
    cf->procBodyObjPtr = NULL;
    cf->nextFramePtr = NULL;
    cf->staticVars = NULL;
    if (cf->vars.table == NULL)
        Jim_InitHashTable(&cf->vars, &JimVariablesHashTableType, interp);
    return cf;
}

/* Used to invalidate every caching related to callframe stability. */
static void JimChangeCallFrameId(Jim_Interp *interp, Jim_CallFrame *cf)
{
    cf->id = interp->callFrameEpoch++;
}

#define JIM_FCF_NONE 0 /* no flags */
#define JIM_FCF_NOHT 1 /* don't free the hash table */
static void JimFreeCallFrame(Jim_Interp *interp, Jim_CallFrame *cf,
        int flags)
{
    if (cf->procArgsObjPtr) Jim_DecrRefCount(interp, cf->procArgsObjPtr);
    if (cf->procBodyObjPtr) Jim_DecrRefCount(interp, cf->procBodyObjPtr);
    if (!(flags & JIM_FCF_NOHT))
        Jim_FreeHashTable(&cf->vars);
    else {
        int i;
        Jim_HashEntry **table = cf->vars.table, *he;

        for (i = 0; i < JIM_HT_INITIAL_SIZE; i++) {
            he = table[i];
            while (he != NULL) {
                Jim_HashEntry *nextEntry = he->next;
                Jim_Var *varPtr = (void*) he->val;

                Jim_DecrRefCount(interp, varPtr->objPtr);
                Jim_Free(he->val);
                Jim_Free((void*)he->key); /* ATTENTION: const cast */
                Jim_Free(he);
                table[i] = NULL;
                he = nextEntry;
            }
        }
        cf->vars.used = 0;
    }
    cf->nextFramePtr = interp->freeFramesList;
    interp->freeFramesList = cf;
}

/* -----------------------------------------------------------------------------
 * References
 * ---------------------------------------------------------------------------*/
#ifdef JIM_REFERENCES

/* References HashTable Type.
 *
 * Keys are jim_wide integers, dynamically allocated for now but in the
 * future it's worth to cache this 8 bytes objects. Values are poitners
 * to Jim_References. */
static void JimReferencesHTValDestructor(void *interp, void *val)
{
    Jim_Reference *refPtr = (void*) val;

    Jim_DecrRefCount(interp, refPtr->objPtr);
    if (refPtr->finalizerCmdNamePtr != NULL) {
        Jim_DecrRefCount(interp, refPtr->finalizerCmdNamePtr);
    }
    Jim_Free(val);
}

unsigned int JimReferencesHTHashFunction(const void *key)
{
    /* Only the least significant bits are used. */
    const jim_wide *widePtr = key;
    unsigned int intValue = (unsigned int) *widePtr;
    return Jim_IntHashFunction(intValue);
}

const void *JimReferencesHTKeyDup(void *privdata, const void *key)
{
    void *copy = Jim_Alloc(sizeof(jim_wide));
    JIM_NOTUSED(privdata);

    memcpy(copy, key, sizeof(jim_wide));
    return copy;
}

int JimReferencesHTKeyCompare(void *privdata, const void *key1, 
        const void *key2)
{
    JIM_NOTUSED(privdata);

    return memcmp(key1, key2, sizeof(jim_wide)) == 0;
}

void JimReferencesHTKeyDestructor(void *privdata, const void *key)
{
    JIM_NOTUSED(privdata);

    Jim_Free((void*)key);
}

static const Jim_HashTableType JimReferencesHashTableType = {
    JimReferencesHTHashFunction,    /* hash function */
    JimReferencesHTKeyDup,          /* key dup */
    NULL,                           /* val dup */
    JimReferencesHTKeyCompare,      /* key compare */
    JimReferencesHTKeyDestructor,   /* key destructor */
    JimReferencesHTValDestructor    /* val destructor */
};

/* -----------------------------------------------------------------------------
 * Reference object type and References API
 * ---------------------------------------------------------------------------*/

/* The string representation of references has two features in order
 * to make the GC faster. The first is that every reference starts
 * with a non common character '<', in order to make the string matching
 * faster. The second is that the reference string rep is 42 characters
 * in length, this allows to avoid to check every object with a string
 * repr < 42, and usually there aren't many of these objects. */

#define JIM_REFERENCE_SPACE (35+JIM_REFERENCE_TAGLEN)

static int JimFormatReference(char *buf, Jim_Reference *refPtr, jim_wide id)
{
    const char *fmt = "<reference.<%s>.%020" JIM_WIDE_MODIFIER ">";
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

void UpdateStringOfReference(struct Jim_Obj *objPtr)
{
    int len;
    char buf[JIM_REFERENCE_SPACE+1];
    Jim_Reference *refPtr;

    refPtr = objPtr->internalRep.refValue.refPtr;
    len = JimFormatReference(buf, refPtr, objPtr->internalRep.refValue.id);
    objPtr->bytes = Jim_Alloc(len+1);
    memcpy(objPtr->bytes, buf, len+1);
    objPtr->length = len;
}

/* returns true if 'c' is a valid reference tag character.
 * i.e. inside the range [_a-zA-Z0-9] */
static int isrefchar(int c)
{
    return (c == '_' || isalnum(c));
}

int SetReferenceFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    jim_wide wideValue;
    int i, len;
    const char *str, *start, *end;
    char refId[21];
    Jim_Reference *refPtr;
    Jim_HashEntry *he;

    /* Get the string representation */
    str = Jim_GetString(objPtr, &len);
    /* Check if it looks like a reference */
    if (len < JIM_REFERENCE_SPACE) goto badformat;
    /* Trim spaces */
    start = str;
    end = str+len-1;
    while (*start == ' ') start++;
    while (*end == ' ' && end > start) end--;
    if (end-start+1 != JIM_REFERENCE_SPACE) goto badformat;
    /* <reference.<1234567>.%020> */
    if (memcmp(start, "<reference.<", 12) != 0) goto badformat;
    if (start[12+JIM_REFERENCE_TAGLEN] != '>' || end[0] != '>') goto badformat;
    /* The tag can't contain chars other than a-zA-Z0-9 + '_'. */
    for (i = 0; i < JIM_REFERENCE_TAGLEN; i++) {
        if (!isrefchar(start[12+i])) goto badformat;
    }
    /* Extract info from the refernece. */
    memcpy(refId, start+14+JIM_REFERENCE_TAGLEN, 20);
    refId[20] = '\0';
    /* Try to convert the ID into a jim_wide */
    if (Jim_StringToWide(refId, &wideValue, 10) != JIM_OK) goto badformat;
    /* Check if the reference really exists! */
    he = Jim_FindHashEntry(&interp->references, &wideValue);
    if (he == NULL) {
        Jim_SetResultFormatted(interp, "invalid reference id \"%#s\"", objPtr);
        return JIM_ERR;
    }
    refPtr = he->val;
    /* Free the old internal repr and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &referenceObjType;
    objPtr->internalRep.refValue.id = wideValue;
    objPtr->internalRep.refValue.refPtr = refPtr;
    return JIM_OK;

badformat:
    Jim_SetResultFormatted(interp, "expected reference but got \"%#s\"", objPtr);
    return JIM_ERR;
}

/* Returns a new reference pointing to objPtr, having cmdNamePtr
 * as finalizer command (or NULL if there is no finalizer).
 * The returned reference object has refcount = 0. */
Jim_Obj *Jim_NewReference(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *tagPtr,
        Jim_Obj *cmdNamePtr)
{
    struct Jim_Reference *refPtr;
    jim_wide wideValue = interp->referenceNextId;
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
    Jim_AddHashEntry(&interp->references, &wideValue, refPtr);
    refObjPtr = Jim_NewObj(interp);
    refObjPtr->typePtr = &referenceObjType;
    refObjPtr->bytes = NULL;
    refObjPtr->internalRep.refValue.id = interp->referenceNextId;
    refObjPtr->internalRep.refValue.refPtr = refPtr;
    interp->referenceNextId++;
    /* Set the tag. Trimmered at JIM_REFERENCE_TAGLEN. Everything
     * that does not pass the 'isrefchar' test is replaced with '_' */
    tag = Jim_GetString(tagPtr, &tagLen);
    if (tagLen > JIM_REFERENCE_TAGLEN)
        tagLen = JIM_REFERENCE_TAGLEN;
    for (i = 0; i < JIM_REFERENCE_TAGLEN; i++) {
        if (i < tagLen)
            refPtr->tag[i] = tag[i];
        else
            refPtr->tag[i] = '_';
    }
    refPtr->tag[JIM_REFERENCE_TAGLEN] = '\0';
    return refObjPtr;
}

Jim_Reference *Jim_GetReference(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (objPtr->typePtr != &referenceObjType &&
        SetReferenceFromAny(interp, objPtr) == JIM_ERR)
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
    JimReferencesHTHashFunction,    /* hash function */
    JimReferencesHTKeyDup,          /* key dup */
    NULL,                           /* val dup */
    JimReferencesHTKeyCompare,      /* key compare */
    JimReferencesHTKeyDestructor,   /* key destructor */
    NULL                            /* val destructor */
};

/* #define JIM_DEBUG_GC 1 */

/* Performs the garbage collection. */
int Jim_Collect(Jim_Interp *interp)
{
    Jim_HashTable marks;
    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;
    Jim_Obj *objPtr;
    int collected = 0;

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
    while(objPtr) {
        if (objPtr->typePtr == NULL ||
            objPtr->typePtr->flags & JIM_TYPE_REFERENCES) {
            const char *str, *p;
            int len;

            /* If the object is of type reference, to get the
             * Id is simple... */
            if (objPtr->typePtr == &referenceObjType) {
                Jim_AddHashEntry(&marks,
                    &objPtr->internalRep.refValue.id, NULL);
#ifdef JIM_DEBUG_GC
                printf(
                    "MARK (reference): %d refcount: %d" JIM_NL, 
                    (int) objPtr->internalRep.refValue.id,
                    objPtr->refCount);
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
            while(1) {
                int i;
                jim_wide id;
                char buf[21];

                if ((p = strstr(p, "<reference.<")) == NULL)
                    break;
                /* Check if it's a valid reference. */
                if (len-(p-str) < JIM_REFERENCE_SPACE) break;
                if (p[41] != '>' || p[19] != '>' || p[20] != '.') break;
                for (i = 21; i <= 40; i++)
                    if (!isdigit(p[i]))
                        break;
                /* Get the ID */
                memcpy(buf, p+21, 20);
                buf[20] = '\0';
                Jim_StringToWide(buf, &id, 10);

                /* Ok, a reference for the given ID
                 * was found. Mark it. */
                Jim_AddHashEntry(&marks, &id, NULL);
#ifdef JIM_DEBUG_GC
                printf("MARK: %d" JIM_NL, (int)id);
#endif
                p += JIM_REFERENCE_SPACE;
            }
        }
        objPtr = objPtr->nextObjPtr;
    }

    /* Run the references hash table to destroy every reference that
     * is not referenced outside (not present in the mark HT). */
    htiter = Jim_GetHashTableIterator(&interp->references);
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        const jim_wide *refId;
        Jim_Reference *refPtr;

        refId = he->key;
        /* Check if in the mark phase we encountered
         * this reference. */
        if (Jim_FindHashEntry(&marks, refId) == NULL) {
#ifdef JIM_DEBUG_GC
            printf("COLLECTING %d" JIM_NL, (int)*refId);
#endif
            collected++;
            /* Drop the reference, but call the
             * finalizer first if registered. */
            refPtr = he->val;
            if (refPtr->finalizerCmdNamePtr) {
                char *refstr = Jim_Alloc(JIM_REFERENCE_SPACE+1);
                Jim_Obj *objv[3], *oldResult;

                JimFormatReference(refstr, refPtr, *refId);

                objv[0] = refPtr->finalizerCmdNamePtr;
                objv[1] = Jim_NewStringObjNoAlloc(interp,
                        refstr, 32);
                objv[2] = refPtr->objPtr;
                Jim_IncrRefCount(objv[0]);
                Jim_IncrRefCount(objv[1]);
                Jim_IncrRefCount(objv[2]);

                /* Drop the reference itself */
                Jim_DeleteHashEntry(&interp->references, refId);

                /* Call the finalizer. Errors ignored. */
                oldResult = interp->result;
                Jim_IncrRefCount(oldResult);
                Jim_EvalObjVector(interp, 3, objv);
                Jim_SetResult(interp, oldResult);
                Jim_DecrRefCount(interp, oldResult);

                Jim_DecrRefCount(interp, objv[0]);
                Jim_DecrRefCount(interp, objv[1]);
                Jim_DecrRefCount(interp, objv[2]);
            } else {
                Jim_DeleteHashEntry(&interp->references, refId);
            }
        }
    }
    Jim_FreeHashTableIterator(htiter);
    Jim_FreeHashTable(&marks);
    interp->lastCollectId = interp->referenceNextId;
    interp->lastCollectTime = time(NULL);
    return collected;
}

#define JIM_COLLECT_ID_PERIOD 5000
#define JIM_COLLECT_TIME_PERIOD 300

void Jim_CollectIfNeeded(Jim_Interp *interp)
{
    jim_wide elapsedId;
    int elapsedTime;
    
    elapsedId = interp->referenceNextId - interp->lastCollectId;
    elapsedTime = time(NULL) - interp->lastCollectTime;


    if (elapsedId > JIM_COLLECT_ID_PERIOD ||
        elapsedTime > JIM_COLLECT_TIME_PERIOD) {
        Jim_Collect(interp);
    }
}
#endif

/* -----------------------------------------------------------------------------
 * Interpreter related functions
 * ---------------------------------------------------------------------------*/

Jim_Interp *Jim_CreateInterp(void)
{
    Jim_Interp *i = Jim_Alloc(sizeof(*i));

    i->errorLine = 0;
    i->errorFileName = Jim_StrDup("");
    i->addStackTrace = 0;
    i->numLevels = 0;
    i->maxNestingDepth = JIM_MAX_NESTING_DEPTH;
    i->returnCode = JIM_OK;
    i->returnLevel = 0;
    i->exitCode = 0;
    i->procEpoch = 0;
    i->callFrameEpoch = 0;
    i->liveList = i->freeList = NULL;
    i->referenceNextId = 0;
    i->lastCollectId = 0;
    i->lastCollectTime = time(NULL);
    i->freeFramesList = NULL;
    i->prngState = NULL;
    i->evalRetcodeLevel = -1;
    i->id = 0;
    i->sigmask = 0;
    i->signal_level = 0;
    i->signal_set_result = NULL;

    /* Note that we can create objects only after the
     * interpreter liveList and freeList pointers are
     * initialized to NULL. */
    Jim_InitHashTable(&i->commands, &JimCommandsHashTableType, i);
#ifdef JIM_REFERENCES
    Jim_InitHashTable(&i->references, &JimReferencesHashTableType, i);
#endif
    Jim_InitHashTable(&i->sharedStrings, &JimSharedStringsHashTableType,
            NULL);
    Jim_InitHashTable(&i->assocData, &JimAssocDataHashTableType, i);
    Jim_InitHashTable(&i->packages, &JimStringKeyValCopyHashTableType, NULL);
    i->framePtr = i->topFramePtr = JimCreateCallFrame(i);
    i->emptyObj = Jim_NewEmptyStringObj(i);
    i->trueObj = Jim_NewIntObj(i, 1);
    i->falseObj = Jim_NewIntObj(i, 0);
    i->result = i->emptyObj;
    i->stackTrace = Jim_NewListObj(i, NULL, 0);
    i->unknown = Jim_NewStringObj(i, "unknown", -1);
    i->unknown_called = 0;
    i->errorProc = i->emptyObj;
    i->currentScriptObj = Jim_NewEmptyStringObj(i);
    Jim_IncrRefCount(i->emptyObj);
    Jim_IncrRefCount(i->result);
    Jim_IncrRefCount(i->stackTrace);
    Jim_IncrRefCount(i->unknown);
    Jim_IncrRefCount(i->currentScriptObj);
    Jim_IncrRefCount(i->errorProc);
    Jim_IncrRefCount(i->trueObj);
    Jim_IncrRefCount(i->falseObj);

    /* Initialize key variables every interpreter should contain */
    Jim_SetVariableStrWithStr(i, JIM_LIBPATH, ". /lib/jim");
    Jim_SetVariableStrWithStr(i, JIM_INTERACTIVE, "0");

    return i;
}

void Jim_FreeInterp(Jim_Interp *i)
{
    Jim_CallFrame *cf = i->framePtr, *prevcf, *nextcf;
    Jim_Obj *objPtr, *nextObjPtr;

    Jim_DecrRefCount(i, i->emptyObj);
    Jim_DecrRefCount(i, i->trueObj);
    Jim_DecrRefCount(i, i->falseObj);
    Jim_DecrRefCount(i, i->result);
    Jim_DecrRefCount(i, i->stackTrace);
    Jim_DecrRefCount(i, i->errorProc);
    Jim_DecrRefCount(i, i->unknown);
    Jim_Free((void*)i->errorFileName);
    Jim_DecrRefCount(i, i->currentScriptObj);
    Jim_FreeHashTable(&i->commands);
    Jim_FreeHashTable(&i->references);
    Jim_FreeHashTable(&i->assocData);
    Jim_FreeHashTable(&i->packages);
    Jim_Free(i->prngState);
    /* Free the call frames list */
    while(cf) {
        prevcf = cf->parentCallFrame;
        JimFreeCallFrame(i, cf, JIM_FCF_NONE);
        cf = prevcf;
    }
    /* Check that the live object list is empty, otherwise
     * there is a memory leak. */
    if (i->liveList != NULL) {
        Jim_Obj *objPtr = i->liveList;
    
        printf(JIM_NL "-------------------------------------" JIM_NL);
        printf("Objects still in the free list:" JIM_NL);
        while(objPtr) {
            const char *type = objPtr->typePtr ?
                objPtr->typePtr->name : "string";
            printf("%p (%d) %-10s: '%.20s'" JIM_NL,
                    objPtr, objPtr->refCount, type,
                    objPtr->bytes ? objPtr->bytes
                    : "(null)");
            if (objPtr->typePtr == &sourceObjType) {
                printf( "FILE %s LINE %d" JIM_NL,
                objPtr->internalRep.sourceValue.fileName,
                objPtr->internalRep.sourceValue.lineNumber);
            }
            objPtr = objPtr->nextObjPtr;
        }
        printf( "-------------------------------------" JIM_NL JIM_NL);
        Jim_Panic(i,"Live list non empty freeing the interpreter! Leak?");
    }
    /* Free all the freed objects. */
    objPtr = i->freeList;
    while (objPtr) {
        nextObjPtr = objPtr->nextObjPtr;
        Jim_Free(objPtr);
        objPtr = nextObjPtr;
    }
    /* Free cached CallFrame structures */
    cf = i->freeFramesList;
    while(cf) {
        nextcf = cf->nextFramePtr;
        if (cf->vars.table != NULL)
            Jim_Free(cf->vars.table);
        Jim_Free(cf);
        cf = nextcf;
    }
    /* Free the sharedString hash table. Make sure to free it
     * after every other Jim_Object was freed. */
    Jim_FreeHashTable(&i->sharedStrings);
    /* Free the interpreter structure. */
    Jim_Free(i);
}

/* Store the call frame relative to the level represented by
 * levelObjPtr into *framePtrPtr. If levelObjPtr == NULL, the
 * level is assumed to be '1'.
 *
 * If a newLevelptr int pointer is specified, the function stores
 * the absolute level integer value of the new target callframe into
 * *newLevelPtr. (this is used to adjust interp->numLevels
 * in the implementation of [uplevel], so that [info level] will
 * return a correct information).
 *
 * This function accepts the 'level' argument in the form
 * of the commands [uplevel] and [upvar].
 *
 * For a function accepting a relative integer as level suitable
 * for implementation of [info level ?level?] check the
 * GetCallFrameByInteger() function. */
int Jim_GetCallFrameByLevel(Jim_Interp *interp, Jim_Obj *levelObjPtr,
        Jim_CallFrame **framePtrPtr, int *newLevelPtr)
{
    long level;
    const char *str;
    Jim_CallFrame *framePtr;

    if (newLevelPtr) *newLevelPtr = interp->numLevels;
    if (levelObjPtr) {
        str = Jim_GetString(levelObjPtr, NULL);
        if (str[0] == '#') {
            char *endptr;
            /* speedup for the toplevel (level #0) */
            if (str[1] == '0' && str[2] == '\0') {
                if (newLevelPtr) *newLevelPtr = 0;
                *framePtrPtr = interp->topFramePtr;
                return JIM_OK;
            }

            level = strtol(str+1, &endptr, 0);
            if (str[1] == '\0' || endptr[0] != '\0' || level < 0)
                goto badlevel;
            /* An 'absolute' level is converted into the
             * 'number of levels to go back' format. */
            level = interp->numLevels - level;
            if (level < 0) goto badlevel;
        } else {
            if (Jim_GetLong(interp, levelObjPtr, &level) != JIM_OK || level < 0)
                goto badlevel;
        }
    } else {
        str = "1"; /* Needed to format the error message. */
        level = 1;
    }
    /* Lookup */
    framePtr = interp->framePtr;
    if (newLevelPtr) *newLevelPtr = (*newLevelPtr)-level;
    while (level--) {
        framePtr = framePtr->parentCallFrame;
        if (framePtr == NULL) goto badlevel;
    }
    *framePtrPtr = framePtr;
    return JIM_OK;
badlevel:
    Jim_SetResultFormatted(interp, "bad level \"%s\"", str);
    return JIM_ERR;
}

/* Similar to Jim_GetCallFrameByLevel() but the level is specified
 * as a relative integer like in the [info level ?level?] command. */
static int JimGetCallFrameByInteger(Jim_Interp *interp, Jim_Obj *levelObjPtr,
        Jim_CallFrame **framePtrPtr)
{
    jim_wide level;
    jim_wide relLevel; /* level relative to the current one. */
    Jim_CallFrame *framePtr;

    if (Jim_GetWide(interp, levelObjPtr, &level) != JIM_OK)
        goto badlevel;
    if (level > 0) {
        /* An 'absolute' level is converted into the
         * 'number of levels to go back' format. */
        relLevel = interp->numLevels - level;
    } else {
        relLevel = -level;
    }
    /* Lookup */
    framePtr = interp->framePtr;
    while (relLevel--) {
        framePtr = framePtr->parentCallFrame;
        if (framePtr == NULL) goto badlevel;
    }
    *framePtrPtr = framePtr;
    return JIM_OK;
badlevel:
    Jim_SetResultFormatted(interp, "bad level \"%#s\"", levelObjPtr);
    return JIM_ERR;
}

static void JimSetErrorFileName(Jim_Interp *interp, const char *filename)
{
    Jim_Free((void*)interp->errorFileName);
    interp->errorFileName = Jim_StrDup(filename);
}

static void JimSetErrorLineNumber(Jim_Interp *interp, int linenr)
{
    interp->errorLine = linenr;
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
        Jim_Obj *filenameObj;
        Jim_ListIndex(interp, interp->stackTrace, len - 2, &filenameObj, JIM_NONE);

        Jim_GetString(filenameObj, &len);

        if (len == 0) {
            interp->addStackTrace = 1;
        }
    }
}

/* Returns 1 if the stack trace information was used or 0 if not */
static void JimAppendStackTrace(Jim_Interp *interp, const char *procname,
        const char *filename, int linenr)
{
#if 0
    printf("JimAppendStackTrace: %s:%d (%s)\n", filename, linenr, procname);
#endif

    /* XXX Omit "unknown" for now since it can be confusing (but it may help too!) */
    if (strcmp(procname, "unknown") == 0) {
        procname = "";
    }
    if (!*procname && !*filename) {
        /* No useful info here */
        return;
    }

    if (Jim_IsShared(interp->stackTrace)) {
        interp->stackTrace =
            Jim_DuplicateObj(interp, interp->stackTrace);
        Jim_IncrRefCount(interp->stackTrace);
    }

    /* If we have no procname but the previous element did, merge with that frame */
    if (!*procname && *filename) {
        /* Just a filename. Check the previous entry */
        int len = Jim_ListLength(interp, interp->stackTrace);

        if (len >= 3) {
            Jim_Obj *procnameObj;
            Jim_Obj *filenameObj;
            if (Jim_ListIndex(interp, interp->stackTrace, len - 3, &procnameObj, JIM_NONE) == JIM_OK && 
                Jim_ListIndex(interp, interp->stackTrace, len - 2, &filenameObj, JIM_NONE) == JIM_OK) {

                const char *prev_procname = Jim_GetString(procnameObj, NULL);
                const char *prev_filename = Jim_GetString(filenameObj, NULL);

                if (*prev_procname && !*prev_filename) {
                    ListSetIndex(interp, interp->stackTrace, len - 2, Jim_NewStringObj(interp, filename, -1), 0);
                    ListSetIndex(interp, interp->stackTrace, len - 1, Jim_NewIntObj(interp, linenr), 0);
                    return;
                }
            }
        }
    }

    Jim_ListAppendElement(interp, interp->stackTrace,
            Jim_NewStringObj(interp, procname, -1));
    Jim_ListAppendElement(interp, interp->stackTrace,
            Jim_NewStringObj(interp, filename, -1));
    Jim_ListAppendElement(interp, interp->stackTrace,
            Jim_NewIntObj(interp, linenr));
}

int Jim_SetAssocData(Jim_Interp *interp, const char *key, Jim_InterpDeleteProc *delProc, void *data)
{
    AssocDataValue *assocEntryPtr = (AssocDataValue *)Jim_Alloc(sizeof(AssocDataValue));
    assocEntryPtr->delProc = delProc;
    assocEntryPtr->data = data;
    return Jim_AddHashEntry(&interp->assocData, key, assocEntryPtr);
}

void *Jim_GetAssocData(Jim_Interp *interp, const char *key)
{
    Jim_HashEntry *entryPtr = Jim_FindHashEntry(&interp->assocData, key);
    if (entryPtr != NULL) {
        AssocDataValue *assocEntryPtr = (AssocDataValue *)entryPtr->val;
        return assocEntryPtr->data;
    }
    return NULL;
}

int Jim_DeleteAssocData(Jim_Interp *interp, const char *key)
{
    return Jim_DeleteHashEntry(&interp->assocData, key);
}

int Jim_GetExitCode(Jim_Interp *interp) {
    return interp->exitCode;
}

/* -----------------------------------------------------------------------------
 * Shared strings.
 * Every interpreter has an hash table where to put shared dynamically
 * allocate strings that are likely to be used a lot of times.
 * For example, in the 'source' object type, there is a pointer to
 * the filename associated with that object. Every script has a lot
 * of this objects with the identical file name, so it is wise to share
 * this info.
 *
 * The API is trivial: Jim_GetSharedString(interp, "foobar")
 * returns the pointer to the shared string. Every time a reference
 * to the string is no longer used, the user should call
 * Jim_ReleaseSharedString(interp, stringPointer). Once no one is using
 * a given string, it is removed from the hash table.
 * ---------------------------------------------------------------------------*/
const char *Jim_GetSharedString(Jim_Interp *interp, const char *str)
{
    Jim_HashEntry *he = Jim_FindHashEntry(&interp->sharedStrings, str);

    if (he == NULL) {
        char *strCopy = Jim_StrDup(str);

        Jim_AddHashEntry(&interp->sharedStrings, strCopy, (void*)1);
        return strCopy;
    } else {
        long refCount = (long) he->val;

        refCount++;
        he->val = (void*) refCount;
        return he->key;
    }
}

void Jim_ReleaseSharedString(Jim_Interp *interp, const char *str)
{
    long refCount;
    Jim_HashEntry *he = Jim_FindHashEntry(&interp->sharedStrings, str);

    if (he == NULL) {
        Jim_Panic(interp,"Jim_ReleaseSharedString called with "
              "unknown shared string '%s'", str);
    }
    else {
        refCount = (long) he->val;
        refCount--;
        if (refCount == 0) {
            Jim_DeleteHashEntry(&interp->sharedStrings, str);
        } else {
            he->val = (void*) refCount;
        }
    }
}

/* -----------------------------------------------------------------------------
 * Integer object
 * ---------------------------------------------------------------------------*/
#define JIM_INTEGER_SPACE 24

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


void UpdateStringOfInt(struct Jim_Obj *objPtr)
{
    int len;
    char buf[JIM_INTEGER_SPACE+1];

    len = Jim_WideToString(buf, objPtr->internalRep.wideValue);
    objPtr->bytes = Jim_Alloc(len+1);
    memcpy(objPtr->bytes, buf, len+1);
    objPtr->length = len;
}

int SetIntFromAny(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    jim_wide wideValue;

    if (objPtr->typePtr == &coercedDoubleObjType) {
        /* Simple switcheroo */
        objPtr->typePtr = &intObjType;
        return JIM_OK;
    }

    /* Get the string representation */
    const char *str = Jim_GetString(objPtr, NULL);
    /* Try to convert into a jim_wide */
    if (Jim_StringToWide(str, &wideValue, 0) != JIM_OK) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp, "expected integer but got \"%#s\"", objPtr);
        }
        return JIM_ERR;
    }
    if ((wideValue == JIM_WIDE_MIN || wideValue == JIM_WIDE_MAX) &&
        errno == ERANGE) {
        Jim_SetResultString(interp,
            "Integer value too big to be represented", -1);
        return JIM_ERR;
    }
    /* Free the old internal repr and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &intObjType;
    objPtr->internalRep.wideValue = wideValue;
    return JIM_OK;
}

static int Jim_IsWide(Jim_Obj *objPtr)
{
    return objPtr->typePtr == &intObjType;
}

int Jim_GetWide(Jim_Interp *interp, Jim_Obj *objPtr, jim_wide *widePtr)
{
    if (objPtr->typePtr != &intObjType &&
        SetIntFromAny(interp, objPtr, JIM_ERRMSG) == JIM_ERR)
        return JIM_ERR;
    *widePtr = objPtr->internalRep.wideValue;
    return JIM_OK;
}

/* Get a wide but does not set an error if the format is bad. */
static int JimGetWideNoErr(Jim_Interp *interp, Jim_Obj *objPtr,
        jim_wide *widePtr)
{
    if (objPtr->typePtr != &intObjType &&
        SetIntFromAny(interp, objPtr, JIM_NONE) == JIM_ERR)
        return JIM_ERR;
    *widePtr = objPtr->internalRep.wideValue;
    return JIM_OK;
}

int Jim_GetLong(Jim_Interp *interp, Jim_Obj *objPtr, long *longPtr)
{
    jim_wide wideValue;
    int retval;

    retval = Jim_GetWide(interp, objPtr, &wideValue);
    if (retval == JIM_OK) {
        *longPtr = (long) wideValue;
        return JIM_OK;
    }
    return JIM_ERR;
}

void Jim_SetWide(Jim_Interp *interp, Jim_Obj *objPtr, jim_wide wideValue)
{
    if (Jim_IsShared(objPtr))
        Jim_Panic(interp,"Jim_SetWide called with shared object");
    if (objPtr->typePtr != &intObjType) {
        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &intObjType;
    }
    Jim_InvalidateStringRep(objPtr);
    objPtr->internalRep.wideValue = wideValue;
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

void UpdateStringOfDouble(struct Jim_Obj *objPtr)
{
    int len;
    char buf[JIM_DOUBLE_SPACE+1];

    len = Jim_DoubleToString(buf, objPtr->internalRep.doubleValue);
    objPtr->bytes = Jim_Alloc(len+1);
    memcpy(objPtr->bytes, buf, len+1);
    objPtr->length = len;
}

int SetDoubleFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    double doubleValue;
    jim_wide wideValue;
    const char *str;

    /* Preserve the string representation.
     * Needed so we can convert back to int without loss
     */
    str = Jim_GetString(objPtr, NULL);

    /* Assume a 53 bit mantissa */
#define MIN_INT_IN_DOUBLE -(1LL << 53)
#define MAX_INT_IN_DOUBLE -(MIN_INT_IN_DOUBLE + 1)

    if (objPtr->typePtr == &intObjType
        && objPtr->internalRep.wideValue >= MIN_INT_IN_DOUBLE
        && objPtr->internalRep.wideValue <= MAX_INT_IN_DOUBLE
        )
        {

        /* Direct conversion to coerced double */
        objPtr->typePtr = &coercedDoubleObjType;
        return JIM_OK;
    }
    else if (Jim_StringToWide(str, &wideValue, 10) == JIM_OK) {
        /* Managed to convert to an int, so we can use this as a cooerced double */
        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &coercedDoubleObjType;
        objPtr->internalRep.wideValue = wideValue;
        return JIM_OK;
    }
    else {
        /* Try to convert into a double */
        if (Jim_StringToDouble(str, &doubleValue) != JIM_OK) {
            Jim_SetResultFormatted(interp, "expected number but got \"%#s\"", objPtr);
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
        *doublePtr = objPtr->internalRep.wideValue;
        return JIM_OK;
    }
    if (objPtr->typePtr != &doubleObjType &&
        SetDoubleFromAny(interp, objPtr) == JIM_ERR)
        return JIM_ERR;

    if (objPtr->typePtr == &coercedDoubleObjType) {
        *doublePtr = objPtr->internalRep.wideValue;
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
        Jim_Alloc(sizeof(Jim_Obj*)*srcPtr->internalRep.listValue.maxLen);
    memcpy(dupPtr->internalRep.listValue.ele, srcPtr->internalRep.listValue.ele,
            sizeof(Jim_Obj*)*srcPtr->internalRep.listValue.len);
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
static int ListElementQuotingType(const char *s, int len)
{
    int i, level, trySimple = 1;

    /* Try with the SIMPLE case */
    if (len == 0) return JIM_ELESTR_BRACE;
    if (s[0] == '#') return JIM_ELESTR_BRACE;
    if (s[0] == '"' || s[0] == '{') {
        trySimple = 0;
        goto testbrace;
    }
    for (i = 0; i < len; i++) {
        switch(s[i]) {
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
        case '{':
        case '}':
            goto testbrace;
        }
    }
    return JIM_ELESTR_SIMPLE;

testbrace:
    /* Test if it's possible to do with braces */
    if (s[len-1] == '\\' ||
        s[len-1] == ']') return JIM_ELESTR_QUOTE;
    level = 0;
    for (i = 0; i < len; i++) {
        switch(s[i]) {
        case '{': level++; break;
        case '}': level--;
              if (level < 0) return JIM_ELESTR_QUOTE;
              break;
        case '\\':
              if (s[i+1] == '\n')
                  return JIM_ELESTR_QUOTE;
              else
                  if (s[i+1] != '\0') i++;
              break;
        }
    }
    if (level == 0) {
        if (!trySimple) return JIM_ELESTR_BRACE;
        for (i = 0; i < len; i++) {
            switch(s[i]) {
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

/* Returns the malloc-ed representation of a string
 * using backslash to quote special chars. */
char *BackslashQuoteString(const char *s, int len, int *qlenPtr)
{
    char *q = Jim_Alloc(len*2+1), *p;

    p = q;
    while(*s) {
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
        case '\n': *p++ = '\\'; *p++ = 'n'; s++; break;
        case '\r': *p++ = '\\'; *p++ = 'r'; s++; break;
        case '\t': *p++ = '\\'; *p++ = 't'; s++; break;
        case '\f': *p++ = '\\'; *p++ = 'f'; s++; break;
        case '\v': *p++ = '\\'; *p++ = 'v'; s++; break;
        default:
            *p++ = *s++;
            break;
        }
    }
    *p = '\0';
    *qlenPtr = p-q;
    return q;
}

void UpdateStringOfList(struct Jim_Obj *objPtr)
{
    int i, bufLen, realLength;
    const char *strRep;
    char *p;
    int *quotingType;
    Jim_Obj **ele = objPtr->internalRep.listValue.ele;

    /* (Over) Estimate the space needed. */
    quotingType = Jim_Alloc(sizeof(int)*objPtr->internalRep.listValue.len+1);
    bufLen = 0;
    for (i = 0; i < objPtr->internalRep.listValue.len; i++) {
        int len;

        strRep = Jim_GetString(ele[i], &len);
        quotingType[i] = ListElementQuotingType(strRep, len);
        switch (quotingType[i]) {
        case JIM_ELESTR_SIMPLE: bufLen += len; break;
        case JIM_ELESTR_BRACE: bufLen += len+2; break;
        case JIM_ELESTR_QUOTE: bufLen += len*2; break;
        }
        bufLen++; /* elements separator. */
    }
    bufLen++;

    /* Generate the string rep. */
    p = objPtr->bytes = Jim_Alloc(bufLen+1);
    realLength = 0;
    for (i = 0; i < objPtr->internalRep.listValue.len; i++) {
        int len, qlen;
        const char *strRep = Jim_GetString(ele[i], &len);
        char *q;

        switch(quotingType[i]) {
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
            realLength += len+2;
            break;
        case JIM_ELESTR_QUOTE:
            q = BackslashQuoteString(strRep, len, &qlen);
            memcpy(p, q, qlen);
            Jim_Free(q);
            p += qlen;
            realLength += qlen;
            break;
        }
        /* Add a separating space */
        if (i+1 != objPtr->internalRep.listValue.len) {
            *p++ = ' ';
            realLength ++;
        }
    }
    *p = '\0'; /* nul term. */
    objPtr->length = realLength;
    Jim_Free(quotingType);
}

int SetListFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    struct JimParserCtx parser;
    const char *str;
    int strLen;

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
    JimParserInit(&parser, str, strLen, 1);
    while(!JimParserEof(&parser)) {
        char *token;
        int tokenLen, type;
        Jim_Obj *elementPtr;

        JimParseList(&parser);
        if (JimParserTtype(&parser) != JIM_TT_STR &&
            JimParserTtype(&parser) != JIM_TT_ESC)
            continue;
        token = JimParserGetToken(&parser, &tokenLen, &type, NULL);
        elementPtr = Jim_NewStringObjNoAlloc(interp, token, tokenLen);
        ListAppendElement(objPtr, elementPtr);
    }
    return JIM_OK;
}

Jim_Obj *Jim_NewListObj(Jim_Interp *interp, Jim_Obj *const *elements, 
        int len)
{
    Jim_Obj *objPtr;
    int i;

    objPtr = Jim_NewObj(interp);
    objPtr->typePtr = &listObjType;
    objPtr->bytes = NULL;
    objPtr->internalRep.listValue.ele = NULL;
    objPtr->internalRep.listValue.len = 0;
    objPtr->internalRep.listValue.maxLen = 0;
    for (i = 0; i < len; i++) {
        ListAppendElement(objPtr, elements[i]);
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
enum {JIM_LSORT_ASCII, JIM_LSORT_NOCASE, JIM_LSORT_INTEGER, JIM_LSORT_COMMAND};

/* Why doesn't qsort allow a user arg!!! */
static Jim_Obj *sort_command = 0;
static int sort_result = JIM_OK;
static Jim_Interp *sort_interp = 0;
static int sort_order;

/* Sort the internal rep of a list. */
static int ListSortString(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    return Jim_StringCompareObj(*lhsObj, *rhsObj, 0) * sort_order;
}

static int ListSortStringNoCase(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    return Jim_StringCompareObj(*lhsObj, *rhsObj, 1) * sort_order;
}

static int ListSortInteger(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    jim_wide lhs = 0, rhs = 0;

    /* REVISIT: If these are not valid integers, bogus results ...*/
    Jim_GetWide(sort_interp, *lhsObj, &lhs);
    Jim_GetWide(sort_interp, *rhsObj, &rhs);

    return JimSign(lhs - rhs) * sort_order;
}

static int ListSortCommand(Jim_Obj **lhsObj, Jim_Obj **rhsObj)
{
    Jim_Obj *compare_script;
    
    jim_wide ret = 0;

    /* We have already had an error, so just compare pointers */
    if (sort_result != JIM_OK) {
        return JimSign(lhsObj - rhsObj);
    }

    /* This must be a valid list */
    compare_script = Jim_DuplicateObj(sort_interp, sort_command);
    Jim_ListAppendElement(sort_interp, compare_script, *lhsObj);
    Jim_ListAppendElement(sort_interp, compare_script, *rhsObj);

    sort_result = Jim_EvalObj(sort_interp, compare_script);

    if (sort_result != JIM_OK) {
        /* We have an error, so just compare pointers */
        return JimSign(lhsObj - rhsObj);
    }

    Jim_GetWide(sort_interp, Jim_GetResult(sort_interp), &ret);
    return JimSign(ret) * sort_order;
}

/* Sort a list *in place*. MUST be called with non-shared objects. */
static int ListSortElements(Jim_Interp *interp, Jim_Obj *listObjPtr, int type, int order, Jim_Obj *command)
{
    typedef int (qsort_comparator)(const void *, const void *);
    int (*fn)(Jim_Obj**, Jim_Obj**);
    Jim_Obj **vector;
    int len;

    if (Jim_IsShared(listObjPtr))
        Jim_Panic(interp,"Jim_ListSortElements called with shared object");
    if (!Jim_IsList(listObjPtr))
        SetListFromAny(interp, listObjPtr);

    sort_order = order;
    sort_command = command;
    sort_interp = interp;
    sort_result = JIM_OK;

    vector = listObjPtr->internalRep.listValue.ele;
    len = listObjPtr->internalRep.listValue.len;
    switch (type) {
        case JIM_LSORT_ASCII: fn = ListSortString;  break;
        case JIM_LSORT_NOCASE: fn = ListSortStringNoCase;  break;
        case JIM_LSORT_INTEGER: fn = ListSortInteger;  break;
        case JIM_LSORT_COMMAND: fn = ListSortCommand;  break;
        default:
            fn = NULL; /* avoid warning */
            Jim_Panic(interp,"ListSort called with invalid sort type");
    }
    qsort(vector, len, sizeof(Jim_Obj *), (qsort_comparator *)fn);
    Jim_InvalidateStringRep(listObjPtr);

    return sort_result;
}

/* This is the low-level function to append an element to a list.
 * The higher-level Jim_ListAppendElement() performs shared object
 * check and invalidate the string repr. This version is used
 * in the internals of the List Object and is not exported.
 *
 * NOTE: this function can be called only against objects
 * with internal type of List. */
void ListAppendElement(Jim_Obj *listPtr, Jim_Obj *objPtr)
{
    int requiredLen = listPtr->internalRep.listValue.len + 1;

    if (requiredLen > listPtr->internalRep.listValue.maxLen) {
        int maxLen = requiredLen * 2;

        listPtr->internalRep.listValue.ele =
            Jim_Realloc(listPtr->internalRep.listValue.ele,
                    sizeof(Jim_Obj*)*maxLen);
        listPtr->internalRep.listValue.maxLen = maxLen;
    }
    listPtr->internalRep.listValue.ele[listPtr->internalRep.listValue.len] =
        objPtr;
    listPtr->internalRep.listValue.len ++;
    Jim_IncrRefCount(objPtr);
}

/* This is the low-level function to insert elements into a list.
 * The higher-level Jim_ListInsertElements() performs shared object
 * check and invalidate the string repr. This version is used
 * in the internals of the List Object and is not exported.
 *
 * NOTE: this function can be called only against objects
 * with internal type of List. */
void ListInsertElements(Jim_Obj *listPtr, int index, int elemc,
        Jim_Obj *const *elemVec)
{
    int currentLen = listPtr->internalRep.listValue.len;
    int requiredLen = currentLen + elemc;
    int i;
    Jim_Obj **point;

    if (requiredLen > listPtr->internalRep.listValue.maxLen) {
        int maxLen = requiredLen * 2;

        listPtr->internalRep.listValue.ele =
            Jim_Realloc(listPtr->internalRep.listValue.ele,
                    sizeof(Jim_Obj*)*maxLen);
        listPtr->internalRep.listValue.maxLen = maxLen;
    }
    point = listPtr->internalRep.listValue.ele + index;
    memmove(point+elemc, point, (currentLen-index) * sizeof(Jim_Obj*));
    for (i=0; i < elemc; ++i) {
        point[i] = elemVec[i];
        Jim_IncrRefCount(point[i]);
    }
    listPtr->internalRep.listValue.len += elemc;
}

/* Appends every element of appendListPtr into listPtr.
 * Both have to be of the list type. */
void ListAppendList(Jim_Obj *listPtr, Jim_Obj *appendListPtr)
{
    int i, oldLen = listPtr->internalRep.listValue.len;
    int appendLen = appendListPtr->internalRep.listValue.len;
    int requiredLen = oldLen + appendLen;

    if (requiredLen > listPtr->internalRep.listValue.maxLen) {
        int maxLen = requiredLen * 2;

        listPtr->internalRep.listValue.ele =
            Jim_Realloc(listPtr->internalRep.listValue.ele,
                    sizeof(Jim_Obj*)*maxLen);
        listPtr->internalRep.listValue.maxLen = maxLen;
    }
    for (i = 0; i < appendLen; i++) {
        Jim_Obj *objPtr = appendListPtr->internalRep.listValue.ele[i];
        listPtr->internalRep.listValue.ele[oldLen+i] = objPtr;
        Jim_IncrRefCount(objPtr);
    }
    listPtr->internalRep.listValue.len += appendLen;
}

void Jim_ListAppendElement(Jim_Interp *interp, Jim_Obj *listPtr, Jim_Obj *objPtr)
{
    if (Jim_IsShared(listPtr))
        Jim_Panic(interp,"Jim_ListAppendElement called with shared object");
    if (!Jim_IsList(listPtr))
        SetListFromAny(interp, listPtr);
    Jim_InvalidateStringRep(listPtr);
    ListAppendElement(listPtr, objPtr);
}

void Jim_ListAppendList(Jim_Interp *interp, Jim_Obj *listPtr, Jim_Obj *appendListPtr)
{
    if (Jim_IsShared(listPtr))
        Jim_Panic(interp,"Jim_ListAppendList called with shared object");
    if (!Jim_IsList(listPtr))
        SetListFromAny(interp, listPtr);
    Jim_InvalidateStringRep(listPtr);
    ListAppendList(listPtr, appendListPtr);
}

int Jim_ListLength(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (!Jim_IsList(objPtr))
        SetListFromAny(interp, objPtr);
    return objPtr->internalRep.listValue.len;
}

void Jim_ListInsertElements(Jim_Interp *interp, Jim_Obj *listPtr, int index,
        int objc, Jim_Obj *const *objVec)
{
    if (Jim_IsShared(listPtr))
        Jim_Panic(interp,"Jim_ListInsertElement called with shared object");
    if (!Jim_IsList(listPtr))
        SetListFromAny(interp, listPtr);
    if (index >= 0 && index > listPtr->internalRep.listValue.len)
        index = listPtr->internalRep.listValue.len;
    else if (index < 0 ) 
        index = 0;
    Jim_InvalidateStringRep(listPtr);
    ListInsertElements(listPtr, index, objc, objVec);
}

int Jim_ListIndex(Jim_Interp *interp, Jim_Obj *listPtr, int index,
        Jim_Obj **objPtrPtr, int flags)
{
    if (!Jim_IsList(listPtr))
        SetListFromAny(interp, listPtr);
    if ((index >= 0 && index >= listPtr->internalRep.listValue.len) ||
        (index < 0 && (-index-1) >= listPtr->internalRep.listValue.len)) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultString(interp,
                "list index out of range", -1);
        }
        *objPtrPtr = NULL;
        return JIM_ERR;
    }
    if (index < 0)
        index = listPtr->internalRep.listValue.len+index;
    *objPtrPtr = listPtr->internalRep.listValue.ele[index];
    return JIM_OK;
}

static int ListSetIndex(Jim_Interp *interp, Jim_Obj *listPtr, int index,
        Jim_Obj *newObjPtr, int flags)
{
    if (!Jim_IsList(listPtr))
        SetListFromAny(interp, listPtr);
    if ((index >= 0 && index >= listPtr->internalRep.listValue.len) ||
        (index < 0 && (-index-1) >= listPtr->internalRep.listValue.len)) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultString(interp,
                "list index out of range", -1);
        }
        return JIM_ERR;
    }
    if (index < 0)
        index = listPtr->internalRep.listValue.len+index;
    Jim_DecrRefCount(interp, listPtr->internalRep.listValue.ele[index]);
    listPtr->internalRep.listValue.ele[index] = newObjPtr;
    Jim_IncrRefCount(newObjPtr);
    return JIM_OK;
}

/* Modify the list stored into the variable named 'varNamePtr'
 * setting the element specified by the 'indexc' indexes objects in 'indexv',
 * with the new element 'newObjptr'. */
int Jim_SetListIndex(Jim_Interp *interp, Jim_Obj *varNamePtr,
        Jim_Obj *const *indexv, int indexc, Jim_Obj *newObjPtr)
{
    Jim_Obj *varObjPtr, *objPtr, *listObjPtr;
    int shared, i, index;

    varObjPtr = objPtr = Jim_GetVariable(interp, varNamePtr, JIM_ERRMSG);
    if (objPtr == NULL)
        return JIM_ERR;
    if ((shared = Jim_IsShared(objPtr)))
        varObjPtr = objPtr = Jim_DuplicateObj(interp, objPtr);
    for (i = 0; i < indexc-1; i++) {
        listObjPtr = objPtr;
        if (Jim_GetIndex(interp, indexv[i], &index) != JIM_OK)
            goto err;
        if (Jim_ListIndex(interp, listObjPtr, index, &objPtr,
                    JIM_ERRMSG) != JIM_OK) {
            goto err;
        }
        if (Jim_IsShared(objPtr)) {
            objPtr = Jim_DuplicateObj(interp, objPtr);
            ListSetIndex(interp, listObjPtr, index, objPtr, JIM_NONE);
        }
        Jim_InvalidateStringRep(listObjPtr);
    }
    if (Jim_GetIndex(interp, indexv[indexc-1], &index) != JIM_OK)
        goto err;
    if (ListSetIndex(interp, objPtr, index, newObjPtr, JIM_ERRMSG) == JIM_ERR)
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
            Jim_ListAppendList(interp, objPtr, objv[i]);
        return objPtr;
    } else {
        /* Else... we have to glue strings together */
        int len = 0, objLen;
        char *bytes, *p;

        /* Compute the length */
        for (i = 0; i < objc; i++) {
            Jim_GetString(objv[i], &objLen);
            len += objLen;
        }
        if (objc) len += objc-1;
        /* Create the string rep, and a stinrg object holding it. */
        p = bytes = Jim_Alloc(len+1);
        for (i = 0; i < objc; i++) {
            const char *s = Jim_GetString(objv[i], &objLen);
            while (objLen && (*s == ' ' || *s == '\t' || *s == '\n'))
            {
                s++; objLen--; len--;
            }
            while (objLen && (s[objLen-1] == ' ' ||
                s[objLen-1] == '\n' || s[objLen-1] == '\t')) {
                objLen--; len--;
            }
            memcpy(p, s, objLen);
            p += objLen;
            if (objLen && i+1 != objc) {
                *p++ = ' ';
            } else if (i+1 != objc) {
                /* Drop the space calcuated for this
                 * element that is instead null. */
                len--;
            }
        }
        *p = '\0';
        return Jim_NewStringObjNoAlloc(interp, bytes, len);
    }
}

/* Returns a list composed of the elements in the specified range.
 * first and start are directly accepted as Jim_Objects and
 * processed for the end?-index? case. */
Jim_Obj *Jim_ListRange(Jim_Interp *interp, Jim_Obj *listObjPtr, Jim_Obj *firstObjPtr, Jim_Obj *lastObjPtr)
{
    int first, last;
    int len, rangeLen;

    if (Jim_GetIndex(interp, firstObjPtr, &first) != JIM_OK ||
        Jim_GetIndex(interp, lastObjPtr, &last) != JIM_OK)
        return NULL;
    len = Jim_ListLength(interp, listObjPtr); /* will convert into list */
    first = JimRelToAbsIndex(len, first);
    last = JimRelToAbsIndex(len, last);
    JimRelToAbsRange(len, first, last, &first, &last, &rangeLen);
    return Jim_NewListObj(interp,
            listObjPtr->internalRep.listValue.ele+first, rangeLen);
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

unsigned int JimObjectHTHashFunction(const void *key)
{
    const char *str;
    Jim_Obj *objPtr = (Jim_Obj*) key;
    int len, h;

    str = Jim_GetString(objPtr, &len);
    h = Jim_GenHashFunction((unsigned char*)str, len);
    return h;
}

int JimObjectHTKeyCompare(void *privdata, const void *key1, const void *key2)
{
    JIM_NOTUSED(privdata);

    return Jim_StringEqObj((Jim_Obj*)key1, (Jim_Obj*)key2, 0);
}

static void JimObjectHTKeyValDestructor(void *interp, void *val)
{
    Jim_Obj *objPtr = val;

    Jim_DecrRefCount(interp, objPtr);
}

static const Jim_HashTableType JimDictHashTableType = {
    JimObjectHTHashFunction,            /* hash function */
    NULL,                               /* key dup */
    NULL,                               /* val dup */
    JimObjectHTKeyCompare,              /* key compare */
    (void(*)(void*, const void*))       /* ATTENTION: const cast */
        JimObjectHTKeyValDestructor,    /* key destructor */
    JimObjectHTKeyValDestructor         /* val destructor */
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
    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;

    /* Create a new hash table */
    ht = srcPtr->internalRep.ptr;
    dupHt = Jim_Alloc(sizeof(*dupHt));
    Jim_InitHashTable(dupHt, &JimDictHashTableType, interp);
    if (ht->size != 0)
        Jim_ExpandHashTable(dupHt, ht->size);
    /* Copy every element from the source to the dup hash table */
    htiter = Jim_GetHashTableIterator(ht);
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        const Jim_Obj *keyObjPtr = he->key;
        Jim_Obj *valObjPtr = he->val;

        Jim_IncrRefCount((Jim_Obj*)keyObjPtr);  /* ATTENTION: const cast */
        Jim_IncrRefCount(valObjPtr);
        Jim_AddHashEntry(dupHt, keyObjPtr, valObjPtr);
    }
    Jim_FreeHashTableIterator(htiter);

    dupPtr->internalRep.ptr = dupHt;
    dupPtr->typePtr = &dictObjType;
}

void UpdateStringOfDict(struct Jim_Obj *objPtr)
{
    int i, bufLen, realLength;
    const char *strRep;
    char *p;
    int *quotingType, objc;
    Jim_HashTable *ht;
    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;
    Jim_Obj **objv;

    /* Trun the hash table into a flat vector of Jim_Objects. */
    ht = objPtr->internalRep.ptr;
    objc = ht->used*2;
    objv = Jim_Alloc(objc*sizeof(Jim_Obj*));
    htiter = Jim_GetHashTableIterator(ht);
    i = 0;
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        objv[i++] = (Jim_Obj*)he->key;  /* ATTENTION: const cast */
        objv[i++] = he->val;
    }
    Jim_FreeHashTableIterator(htiter);
    /* (Over) Estimate the space needed. */
    quotingType = Jim_Alloc(sizeof(int)*objc);
    bufLen = 0;
    for (i = 0; i < objc; i++) {
        int len;

        strRep = Jim_GetString(objv[i], &len);
        quotingType[i] = ListElementQuotingType(strRep, len);
        switch (quotingType[i]) {
        case JIM_ELESTR_SIMPLE: bufLen += len; break;
        case JIM_ELESTR_BRACE: bufLen += len+2; break;
        case JIM_ELESTR_QUOTE: bufLen += len*2; break;
        }
        bufLen++; /* elements separator. */
    }
    bufLen++;

    /* Generate the string rep. */
    p = objPtr->bytes = Jim_Alloc(bufLen+1);
    realLength = 0;
    for (i = 0; i < objc; i++) {
        int len, qlen;
        const char *strRep = Jim_GetString(objv[i], &len);
        char *q;

        switch(quotingType[i]) {
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
            realLength += len+2;
            break;
        case JIM_ELESTR_QUOTE:
            q = BackslashQuoteString(strRep, len, &qlen);
            memcpy(p, q, qlen);
            Jim_Free(q);
            p += qlen;
            realLength += qlen;
            break;
        }
        /* Add a separating space */
        if (i+1 != objc) {
            *p++ = ' ';
            realLength ++;
        }
    }
    *p = '\0'; /* nul term. */
    objPtr->length = realLength;
    Jim_Free(quotingType);
    Jim_Free(objv);
}

#ifdef JIM_OPTIMIZATION
static int SetDictFromList(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    Jim_HashTable *ht;
    int i;
    int listlen;

    listlen = Jim_ListLength(interp, objPtr);
    if (listlen % 2) {
        return JIM_ERR;
    }

    /* Now we can't fail */
    ht = Jim_Alloc(sizeof(*ht));
    Jim_InitHashTable(ht, &JimDictHashTableType, interp);

    for (i = 0; i < listlen; i += 2) {
        Jim_Obj *keyObjPtr;
        Jim_Obj *valObjPtr;
        Jim_ListIndex(interp, objPtr, i, &keyObjPtr, JIM_NONE);
        Jim_ListIndex(interp, objPtr, i + 1, &valObjPtr, JIM_NONE);

        Jim_IncrRefCount(keyObjPtr);
        Jim_IncrRefCount(valObjPtr);

        if (Jim_AddHashEntry(ht, keyObjPtr, valObjPtr) != JIM_OK) {
            Jim_HashEntry *he;
            he = Jim_FindHashEntry(ht, keyObjPtr);
            Jim_DecrRefCount(interp, keyObjPtr);
            /* ATTENTION: const cast */
            Jim_DecrRefCount(interp, (Jim_Obj*)he->val);
            he->val = valObjPtr;
        }
    }

    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &dictObjType;
    objPtr->internalRep.ptr = ht;

    return JIM_OK;
}
#endif

static int SetDictFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    struct JimParserCtx parser;
    Jim_HashTable *ht;
    Jim_Obj *objv[2];
    const char *str;
    int i, strLen;

    /* Get the string representation. Do this first so we don't
     * change order in case of fast conversion to dict
     */
    str = Jim_GetString(objPtr, &strLen);

#ifdef JIM_OPTIMIZATION
    /* If the object is of type "list" with a string rep, we can use
     * a specialized version.
     */
    if (Jim_IsList(objPtr)) {
        if (SetDictFromList(interp, objPtr) != JIM_OK) {
            goto badlist;
        }
        return JIM_OK;
    }
#endif

    /* Free the old internal repr just now and initialize the
     * new one just now. The string->list conversion can't fail. */
    Jim_FreeIntRep(interp, objPtr);
    ht = Jim_Alloc(sizeof(*ht));
    Jim_InitHashTable(ht, &JimDictHashTableType, interp);
    objPtr->typePtr = &dictObjType;
    objPtr->internalRep.ptr = ht;

    /* Convert into a dict */
    JimParserInit(&parser, str, strLen, 1);
    i = 0;
    while(!JimParserEof(&parser)) {
        char *token;
        int tokenLen, type;

        JimParseList(&parser);
        if (JimParserTtype(&parser) != JIM_TT_STR &&
            JimParserTtype(&parser) != JIM_TT_ESC)
            continue;
        token = JimParserGetToken(&parser, &tokenLen, &type, NULL);
        objv[i++] = Jim_NewStringObjNoAlloc(interp, token, tokenLen);
        if (i == 2) {
            i = 0;
            Jim_IncrRefCount(objv[0]);
            Jim_IncrRefCount(objv[1]);
            if (Jim_AddHashEntry(ht, objv[0], objv[1]) != JIM_OK) {
                Jim_HashEntry *he;
                he = Jim_FindHashEntry(ht, objv[0]);
                Jim_DecrRefCount(interp, objv[0]);
                /* ATTENTION: const cast */
                Jim_DecrRefCount(interp, (Jim_Obj*)he->val);
                he->val = objv[1];
            }
        }
    }
    if (i) {
        Jim_FreeNewObj(interp, objv[0]);
        objPtr->typePtr = NULL;
        Jim_FreeHashTable(ht);
        Jim_Free(ht);
#ifdef JIM_OPTIMIZATION
badlist:
#endif
        Jim_SetResultString(interp, "invalid dictionary value: must be a list with an even number of elements", -1);
        return JIM_ERR;
    }
    return JIM_OK;
}

/* Dict object API */

/* Add an element to a dict. objPtr must be of the "dict" type.
 * The higer-level exported function is Jim_DictAddElement().
 * If an element with the specified key already exists, the value
 * associated is replaced with the new one.
 *
 * if valueObjPtr == NULL, the key is instead removed if it exists. */
static int DictAddElement(Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj *keyObjPtr, Jim_Obj *valueObjPtr)
{
    Jim_HashTable *ht = objPtr->internalRep.ptr;

    if (valueObjPtr == NULL) { /* unset */
        return Jim_DeleteHashEntry(ht, keyObjPtr);
    }
    Jim_IncrRefCount(keyObjPtr);
    Jim_IncrRefCount(valueObjPtr);
    if (Jim_AddHashEntry(ht, keyObjPtr, valueObjPtr) != JIM_OK) {
        Jim_HashEntry *he = Jim_FindHashEntry(ht, keyObjPtr);
        Jim_DecrRefCount(interp, keyObjPtr);
        /* ATTENTION: const cast */
        Jim_DecrRefCount(interp, (Jim_Obj*)he->val);
        he->val = valueObjPtr;
    }
    return JIM_OK;
}

/* Add an element, higher-level interface for DictAddElement().
 * If valueObjPtr == NULL, the key is removed if it exists. */
int Jim_DictAddElement(Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj *keyObjPtr, Jim_Obj *valueObjPtr)
{
    int retcode;
    if (Jim_IsShared(objPtr))
        Jim_Panic(interp,"Jim_DictAddElement called with shared object");
    if (objPtr->typePtr != &dictObjType) {
        if (SetDictFromAny(interp, objPtr) != JIM_OK)
            return JIM_ERR;
    }
    retcode = DictAddElement(interp, objPtr, keyObjPtr, valueObjPtr);
    Jim_InvalidateStringRep(objPtr);
    return retcode;
}

Jim_Obj *Jim_NewDictObj(Jim_Interp *interp, Jim_Obj *const *elements, int len)
{
    Jim_Obj *objPtr;
    int i;

    if (len % 2)
        Jim_Panic(interp,"Jim_NewDicObj() 'len' argument must be even");

    objPtr = Jim_NewObj(interp);
    objPtr->typePtr = &dictObjType;
    objPtr->bytes = NULL;
    objPtr->internalRep.ptr = Jim_Alloc(sizeof(Jim_HashTable));
    Jim_InitHashTable(objPtr->internalRep.ptr, &JimDictHashTableType, interp);
    for (i = 0; i < len; i += 2)
        DictAddElement(interp, objPtr, elements[i], elements[i+1]);
    return objPtr;
}

/* Return the value associated to the specified dict key
 * Note: Returns JIM_OK if OK, JIM_ERR if entry not found or -1 if can't create dict value
 */
int Jim_DictKey(Jim_Interp *interp, Jim_Obj *dictPtr, Jim_Obj *keyPtr,
        Jim_Obj **objPtrPtr, int flags)
{
    Jim_HashEntry *he;
    Jim_HashTable *ht;

    if (dictPtr->typePtr != &dictObjType) {
        if (SetDictFromAny(interp, dictPtr) != JIM_OK)
            return -1;
    }
    ht = dictPtr->internalRep.ptr;
    if ((he = Jim_FindHashEntry(ht, keyPtr)) == NULL) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp, "key \"%#s\" not found in dictionary", keyPtr);
        }
        return JIM_ERR;
    }
    *objPtrPtr = he->val;
    return JIM_OK;
}

/* Return an allocated array of key/value pairs for the dictionary. Stores the length in *len */
int Jim_DictPairs(Jim_Interp *interp, Jim_Obj *dictPtr, Jim_Obj ***objPtrPtr, int *len)
{
    Jim_HashTable *ht;
    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;
    Jim_Obj **objv;
    int i;

    if (dictPtr->typePtr != &dictObjType) {
        if (SetDictFromAny(interp, dictPtr) != JIM_OK)
            return JIM_ERR;
    }
    ht = dictPtr->internalRep.ptr;

    /* Turn the hash table into a flat vector of Jim_Objects. */
    objv = Jim_Alloc((ht->used * 2) * sizeof(Jim_Obj*));
    htiter = Jim_GetHashTableIterator(ht);
    i = 0;
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        objv[i++] = (Jim_Obj*)he->key;  /* ATTENTION: const cast */
        objv[i++] = he->val;
    }
    *len = i;
    Jim_FreeHashTableIterator(htiter);
    *objPtrPtr = objv;
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
        if (Jim_DictKey(interp, dictPtr, keyv[i], &objPtr, flags)
                != JIM_OK)
            return JIM_ERR;
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
 * from the dictionary. */
int Jim_SetDictKeysVector(Jim_Interp *interp, Jim_Obj *varNamePtr,
        Jim_Obj *const *keyv, int keyc, Jim_Obj *newObjPtr)
{
    Jim_Obj *varObjPtr, *objPtr, *dictObjPtr;
    int shared, i;

    varObjPtr = objPtr = Jim_GetVariable(interp, varNamePtr, newObjPtr == NULL ? JIM_ERRMSG : JIM_NONE);
    if (objPtr == NULL) {
        if (newObjPtr == NULL) /* Cannot remove a key from non existing var */
            return JIM_ERR;
        varObjPtr = objPtr = Jim_NewDictObj(interp, NULL, 0);
        if (Jim_SetVariable(interp, varNamePtr, objPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, varObjPtr);
            return JIM_ERR;
        }
    }
    if ((shared = Jim_IsShared(objPtr)))
        varObjPtr = objPtr = Jim_DuplicateObj(interp, objPtr);
    for (i = 0; i < keyc-1; i++) {
        dictObjPtr = objPtr;

        /* Check if it's a valid dictionary */
        if (dictObjPtr->typePtr != &dictObjType) {
            if (SetDictFromAny(interp, dictObjPtr) != JIM_OK)
                goto err;
        }
        /* Check if the given key exists. */
        Jim_InvalidateStringRep(dictObjPtr);
        if (Jim_DictKey(interp, dictObjPtr, keyv[i], &objPtr,
            newObjPtr ? JIM_NONE : JIM_ERRMSG) == JIM_OK)
        {
            /* This key exists at the current level.
             * Make sure it's not shared!. */
            if (Jim_IsShared(objPtr)) {
                objPtr = Jim_DuplicateObj(interp, objPtr);
                DictAddElement(interp, dictObjPtr, keyv[i], objPtr);
            }
        } else {
            /* Key not found. If it's an [unset] operation
             * this is an error. Only the last key may not
             * exist. */
            if (newObjPtr == NULL)
                goto err;
            /* Otherwise set an empty dictionary
             * as key's value. */
            objPtr = Jim_NewDictObj(interp, NULL, 0);
            DictAddElement(interp, dictObjPtr, keyv[i], objPtr);
        }
    }
    if (Jim_DictAddElement(interp, objPtr, keyv[keyc-1], newObjPtr) != JIM_OK) {
        goto err;
    }
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

void UpdateStringOfIndex(struct Jim_Obj *objPtr)
{
    int len;
    char buf[JIM_INTEGER_SPACE+1];

    if (objPtr->internalRep.indexValue >= 0)
        len = sprintf(buf, "%d", objPtr->internalRep.indexValue);
    else if (objPtr->internalRep.indexValue == -1)
        len = sprintf(buf, "end");
    else {
        len = sprintf(buf, "end%d", objPtr->internalRep.indexValue+1);
    }
    objPtr->bytes = Jim_Alloc(len+1);
    memcpy(objPtr->bytes, buf, len+1);
    objPtr->length = len;
}

int SetIndexFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int index, end = 0;
    const char *str;
    char *endptr;

    /* Get the string representation */
    str = Jim_GetString(objPtr, NULL);

    /* Try to convert into an index */
    if (strncmp(str, "end", 3) == 0) {
        end = 1;
        str += 3;
        index = 0;
    }
    else {
        index = strtol(str, &endptr, 10);

        if (endptr == str) {
            goto badindex;
        }
        str = endptr;
    }

    /* Now str may include or +<num> or -<num> */
    if (*str == '+' || *str == '-') {
        int sign = (*str == '+' ? 1 : -1);
        index += sign * strtol(++str, &endptr, 10);
        if (str == endptr || *endptr) {
            goto badindex;
        }
        str = endptr;
    }
    /* The only thing left should be spaces */
    while (isspace(*str)) {
        str++;
    }
    if (*str) {
        goto badindex;
    }
    if (end) {
        if (index > 0) {
            index = INT_MAX;
        }
        else {
            /* end-1 is repesented as -2 */
            index--;
        }
    }
    else if (index < 0) {
        index = -INT_MAX;
    }

    /* Free the old internal repr and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &indexObjType;
    objPtr->internalRep.indexValue = index;
    return JIM_OK;

badindex:
    Jim_SetResultFormatted(interp, "bad index \"%#s\": must be integer?[+-]integer? or end?[+-]integer?", objPtr);
    return JIM_ERR;
}

int Jim_GetIndex(Jim_Interp *interp, Jim_Obj *objPtr, int *indexPtr)
{
    /* Avoid shimmering if the object is an integer. */
    if (objPtr->typePtr == &intObjType) {
        jim_wide val = objPtr->internalRep.wideValue;
        if (!(val < LONG_MIN) && !(val > LONG_MAX)) {
            *indexPtr = (val < 0) ? -INT_MAX : (long)val;;
            return JIM_OK;
        }
    }
    if (objPtr->typePtr != &indexObjType &&
        SetIndexFromAny(interp, objPtr) == JIM_ERR)
        return JIM_ERR;
    *indexPtr = objPtr->internalRep.indexValue;
    return JIM_OK;
}

/* -----------------------------------------------------------------------------
 * Return Code Object.
 * ---------------------------------------------------------------------------*/

/* NOTE: These must be kept in the same order as JIM_OK, JIM_ERR, ... */
static const char *jimReturnCodes[] = {
    [JIM_OK] = "ok",
    [JIM_ERR] = "error",
    [JIM_RETURN] = "return",
    [JIM_BREAK] = "break",
    [JIM_CONTINUE] = "continue",
    [JIM_SIGNAL] = "signal",
    [JIM_EXIT] = "exit",
    [JIM_EVAL] = "eval",
    NULL
};

#define jimReturnCodesSize (sizeof(jimReturnCodes)/sizeof(*jimReturnCodes))

static int SetReturnCodeFromAny(Jim_Interp *interp, Jim_Obj *objPtr);

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

int SetReturnCodeFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int returnCode;
    jim_wide wideValue;

    /* Try to convert into an integer */
    if (JimGetWideNoErr(interp, objPtr, &wideValue) != JIM_ERR)
        returnCode = (int) wideValue;
    else if (Jim_GetEnum(interp, objPtr, jimReturnCodes, &returnCode, NULL, JIM_NONE) != JIM_OK) {
        Jim_SetResultFormatted(interp, "expected return code but got \"%#s\"", objPtr);
        return JIM_ERR;
    }
    /* Free the old internal repr and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &returnCodeObjType;
    objPtr->internalRep.returnCode = returnCode;
    return JIM_OK;
}

int Jim_GetReturnCode(Jim_Interp *interp, Jim_Obj *objPtr, int *intPtr)
{
    if (objPtr->typePtr != &returnCodeObjType &&
        SetReturnCodeFromAny(interp, objPtr) == JIM_ERR)
        return JIM_ERR;
    *intPtr = objPtr->internalRep.returnCode;
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
enum {
    /* Continues on from the JIM_TT_ space */
    /* Operations */
    JIM_EXPROP_MUL = JIM_TT_EXPR_OP, /* 15 */
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
    JIM_EXPROP_BITAND, /* 30 */
    JIM_EXPROP_BITXOR,
    JIM_EXPROP_BITOR,

    /* Note must keep these together */
    JIM_EXPROP_LOGICAND, /* 33 */
    JIM_EXPROP_LOGICAND_LEFT,
    JIM_EXPROP_LOGICAND_RIGHT,

    /* and these */
    JIM_EXPROP_LOGICOR, /* 36 */
    JIM_EXPROP_LOGICOR_LEFT,
    JIM_EXPROP_LOGICOR_RIGHT,

    /* and these */
    /* Ternary operators */
    JIM_EXPROP_TERNARY, /* 39 */
    JIM_EXPROP_TERNARY_LEFT,
    JIM_EXPROP_TERNARY_RIGHT,

    /* and these */
    JIM_EXPROP_COLON, /* 42 */
    JIM_EXPROP_COLON_LEFT,
    JIM_EXPROP_COLON_RIGHT,

    JIM_EXPROP_POW, /* 45 */

/* Binary operators (strings) */
    JIM_EXPROP_STREQ,
    JIM_EXPROP_STRNE,
    JIM_EXPROP_STRIN,
    JIM_EXPROP_STRNI,

/* Unary operators (numbers) */
    JIM_EXPROP_NOT,
    JIM_EXPROP_BITNOT,
    JIM_EXPROP_UNARYMINUS,
    JIM_EXPROP_UNARYPLUS,

    /* Functions */
    JIM_EXPROP_FUNC_FIRST,
    JIM_EXPROP_FUNC_INT = JIM_EXPROP_FUNC_FIRST,
    JIM_EXPROP_FUNC_ABS,
    JIM_EXPROP_FUNC_DOUBLE,
    JIM_EXPROP_FUNC_ROUND,

#ifdef JIM_MATH_FUNCTIONS
    /* math functions from libm */
    JIM_EXPROP_FUNC_SIN,
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
#endif
};

struct JimExprState {
    Jim_Obj **stack;
    int stacklen;
    int opcode;
    int skip;
};

/* Operators table */
typedef struct Jim_ExprOperator {
    const char *name;
    int precedence;
    int arity;
    int (*funcop)(Jim_Interp *interp, struct JimExprState *e);
    int lazy; 
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
    int intresult = 0;
    int rc = JIM_OK;
    Jim_Obj *A = ExprPop(e);
    double dA, dC = 0;
    jim_wide wA, wC = 0;
    
    if ((A->typePtr != &doubleObjType || A->bytes) && JimGetWideNoErr(interp, A, &wA) == JIM_OK) {
        intresult = 1;

        switch (e->opcode) {
            case JIM_EXPROP_FUNC_INT: wC = wA; break;
            case JIM_EXPROP_FUNC_ROUND: wC = wA; break;
            case JIM_EXPROP_FUNC_DOUBLE: dC = wA; intresult = 0; break;
            case JIM_EXPROP_FUNC_ABS: wC = wA >= 0 ? wA : -wA; break;
            case JIM_EXPROP_UNARYMINUS: wC = -wA; break;
            case JIM_EXPROP_UNARYPLUS: wC = wA; break;
            case JIM_EXPROP_NOT: wC = !wA; break;
            default: abort();
        }
    }
    else if ((rc = Jim_GetDouble(interp, A, &dA)) == JIM_OK) {
        switch (e->opcode) {
            case JIM_EXPROP_FUNC_INT: wC = dA; intresult = 1; break;
            case JIM_EXPROP_FUNC_ROUND: wC = dA < 0 ? (dA - 0.5) : (dA + 0.5); intresult = 1; break;
            case JIM_EXPROP_FUNC_DOUBLE: dC = dA; break;
            case JIM_EXPROP_FUNC_ABS: dC = dA >= 0 ? dA : -dA; break;
            case JIM_EXPROP_UNARYMINUS: dC = -dA; break;
            case JIM_EXPROP_UNARYPLUS: dC = dA; break;
            case JIM_EXPROP_NOT: wC = !dA; intresult = 1; break;
            default: abort();
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

static int JimExprOpIntUnary(Jim_Interp *interp, struct JimExprState *e)
{
    Jim_Obj *A = ExprPop(e);
    jim_wide wA;
    int rc = JIM_ERR;

    
    if (Jim_GetWide(interp, A, &wA) == JIM_OK) {
        jim_wide wC;

        switch (e->opcode) {
            case JIM_EXPROP_BITNOT: wC = ~wA; break;
            default: abort();
        }
        ExprPush(e, Jim_NewIntObj(interp, wC));
        rc = JIM_OK;
    }

    Jim_DecrRefCount(interp, A);

    return rc;
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
            case JIM_EXPROP_FUNC_SIN: dC = sin(dA); break;
            case JIM_EXPROP_FUNC_COS: dC = cos(dA); break;
            case JIM_EXPROP_FUNC_TAN: dC = tan(dA); break;
            case JIM_EXPROP_FUNC_ASIN: dC=asin(dA); break;
            case JIM_EXPROP_FUNC_ACOS: dC=acos(dA); break;
            case JIM_EXPROP_FUNC_ATAN: dC=atan(dA); break;
            case JIM_EXPROP_FUNC_SINH: dC=sinh(dA); break;
            case JIM_EXPROP_FUNC_COSH: dC=cosh(dA); break;
            case JIM_EXPROP_FUNC_TANH: dC=tanh(dA); break;
            case JIM_EXPROP_FUNC_CEIL: dC=ceil(dA); break;
            case JIM_EXPROP_FUNC_FLOOR: dC=floor(dA); break;
            case JIM_EXPROP_FUNC_EXP: dC=exp(dA); break;
            case JIM_EXPROP_FUNC_LOG: dC=log(dA); break;
            case JIM_EXPROP_FUNC_LOG10: dC=log10(dA); break;
            case JIM_EXPROP_FUNC_SQRT: dC=sqrt(dA); break;
            default: abort();
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
            case JIM_EXPROP_LSHIFT: wC = wA<<wB; break;
            case JIM_EXPROP_RSHIFT: wC = wA>>wB; break;
            case JIM_EXPROP_BITAND: wC = wA&wB; break;
            case JIM_EXPROP_BITXOR: wC = wA^wB; break;
            case JIM_EXPROP_BITOR: wC = wA|wB; break;
            case JIM_EXPROP_POW: wC = JimPowWide(wA,wB); break;
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
                    wC  = wA % wB;
                    if (wC < 0) {
                        wC += wB;
                    }
                    if (negative) {
                        wC = -wC;
                    }
                }
                break;
            case JIM_EXPROP_ROTL: {
                /* uint32_t would be better. But not everyone has inttypes.h?*/
                unsigned long uA = (unsigned long)wA;
                const unsigned int S = sizeof(unsigned long) * 8;
                wC = (unsigned long)((uA<<wB)|(uA>>(S-wB)));
                break;
            }
            case JIM_EXPROP_ROTR: {
                unsigned long uA = (unsigned long)wA;
                const unsigned int S = sizeof(unsigned long) * 8;
                wC = (unsigned long)((uA>>wB)|(uA<<(S-wB)));
                break;
            }
            default: abort();
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
    int intresult = 0;
    int rc = JIM_OK;
    double dA, dB, dC = 0;
    jim_wide wA, wB, wC = 0;

    Jim_Obj *B =  ExprPop(e);
    Jim_Obj *A =  ExprPop(e);
    
    if ((A->typePtr != &doubleObjType || A->bytes) &&
        (B->typePtr != &doubleObjType || B->bytes) &&
        JimGetWideNoErr(interp, A, &wA) == JIM_OK &&
        JimGetWideNoErr(interp, B, &wB) == JIM_OK) {

        /* Both are ints */

        intresult = 1;

        switch (e->opcode) {
            case JIM_EXPROP_POW: wC = JimPowWide(wA,wB); break;
            case JIM_EXPROP_ADD: wC = wA+wB; break;
            case JIM_EXPROP_SUB: wC = wA-wB; break;
            case JIM_EXPROP_MUL: wC = wA*wB; break;
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
            case JIM_EXPROP_LT: wC = wA<wB; break;
            case JIM_EXPROP_GT: wC = wA>wB; break;
            case JIM_EXPROP_LTE: wC = wA<=wB; break;
            case JIM_EXPROP_GTE: wC = wA>=wB; break;
            case JIM_EXPROP_NUMEQ: wC = wA==wB; break;
            case JIM_EXPROP_NUMNE: wC = wA!=wB; break;
            default: abort();
        }
    }
    else if (Jim_GetDouble(interp, A, &dA) == JIM_OK && Jim_GetDouble(interp, B, &dB) == JIM_OK) {
        switch (e->opcode) {
            case JIM_EXPROP_POW:
#ifdef JIM_MATH_FUNCTIONS
                dC = pow(dA,dB);
#else
                rc = JIM_ERR;
#endif
                break;
            case JIM_EXPROP_ADD: dC = dA+dB; break;
            case JIM_EXPROP_SUB: dC = dA-dB; break;
            case JIM_EXPROP_MUL: dC = dA*dB; break;
            case JIM_EXPROP_DIV:
                if (dB == 0) {
#ifdef INFINITY
                    dC = dA < 0 ? -INFINITY : INFINITY;
#else
                    dC = (dA < 0 ? -1.0 : 1.0) * strtod("Inf", NULL);
#endif
                }
                else {
                    dC = dA/dB;
                }
                break;
            case JIM_EXPROP_LT: wC = dA<dB; intresult = 1; break;
            case JIM_EXPROP_GT: wC = dA>dB; intresult = 1; break;
            case JIM_EXPROP_LTE: wC = dA<=dB; intresult = 1; break;
            case JIM_EXPROP_GTE: wC = dA>=dB; intresult = 1; break;
            case JIM_EXPROP_NUMEQ: wC = dA==dB; intresult = 1; break;
            case JIM_EXPROP_NUMNE: wC = dA!=dB; intresult = 1; break;
            default: abort();
        }
    }
    else {
        /* Handle the string case */
        int Alen, Blen;

        const char *sA = Jim_GetString(A, &Alen);
        const char *sB = Jim_GetString(B, &Blen);

        intresult = 1;

        switch(e->opcode) {
            case JIM_EXPROP_LT:
                wC = JimStringCompare(sA, Alen, sB, Blen, 0) < 0; break;
            case JIM_EXPROP_GT:
                wC = JimStringCompare(sA, Alen, sB, Blen, 0) > 0; break;
            case JIM_EXPROP_LTE:
                wC = JimStringCompare(sA, Alen, sB, Blen, 0) <= 0; break;
            case JIM_EXPROP_GTE:
                wC = JimStringCompare(sA, Alen, sB, Blen, 0) >= 0; break;
            case JIM_EXPROP_NUMEQ:
                wC = (Alen == Blen && memcmp(sA, sB, Alen) == 0); break;
            case JIM_EXPROP_NUMNE:
                wC = (Alen != Blen || memcmp(sA, sB, Alen) != 0); break;
            default:
                rc = JIM_ERR; break;
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
        Jim_Obj *objPtr;
        Jim_ListIndex(interp, listObjPtr, i, &objPtr, JIM_NONE);

        if (Jim_StringEqObj(objPtr, valObj, 0)) {
            return 1;
        }
    }
    return 0;
}

static int JimExprOpStrBin(Jim_Interp *interp, struct JimExprState *e)
{
    Jim_Obj *B = ExprPop(e);
    Jim_Obj *A = ExprPop(e);
    
    int Alen, Blen;
    jim_wide wC;

    /* XXX: Not needed for IN, NI */
    const char *sA = Jim_GetString(A, &Alen);
    const char *sB = Jim_GetString(B, &Blen);

    switch(e->opcode) {
        case JIM_EXPROP_STREQ:
            wC = (Alen == Blen && memcmp(sA, sB, Alen) == 0); break;
        case JIM_EXPROP_STRNE:
            wC = (Alen != Blen || memcmp(sA, sB, Alen) != 0); break;
        case JIM_EXPROP_STRIN:
            wC = JimSearchList(interp, B, A); break;
        case JIM_EXPROP_STRNI:
            wC = !JimSearchList(interp, B, A); break;
        default: abort();
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
            e->skip = skip->internalRep.wideValue;
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
            e->skip = skip->internalRep.wideValue;
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

static int JimExprOpColon(Jim_Interp *interp, struct JimExprState *e)
{
    int rc = JIM_OK;
#if 0
    Jim_Obj *C = ExprPop(e);
    Jim_Obj *B = ExprPop(e);
    Jim_Obj *A = ExprPop(e);

    switch (ExprBool(interp, A)) {
        case 0:
            ExprPush(e, C);
            break;

        case 1:
            ExprPush(e, B);
            break;

        case -1:
            /* Invalid */
            rc = JIM_ERR;
            break;
    }
    Jim_DecrRefCount(interp, A);
    Jim_DecrRefCount(interp, B);
    Jim_DecrRefCount(interp, C);

#endif
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
            e->skip = skip->internalRep.wideValue;
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
        e->skip = skip->internalRep.wideValue;
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

enum {
    LAZY_NONE,
    LAZY_OP,
    LAZY_LEFT,
    LAZY_RIGHT
};

/* name - precedence - arity - opcode */
static const struct Jim_ExprOperator Jim_ExprOperators[] = {
    [JIM_EXPROP_FUNC_INT] =       {"int", 400, 1, JimExprOpNumUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_DOUBLE] =    {"double", 400, 1, JimExprOpNumUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_ABS] =       {"abs", 400, 1, JimExprOpNumUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_ROUND] =     {"round", 400, 1, JimExprOpNumUnary, LAZY_NONE },

#ifdef JIM_MATH_FUNCTIONS
    [JIM_EXPROP_FUNC_SIN] =       {"sin", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_COS] =       {"cos", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_TAN] =       {"tan", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_ASIN] =      {"asin", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_ACOS] =      {"acos", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_ATAN] =      {"atan", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_SINH] =      {"sinh", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_COSH] =      {"cosh", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_TANH] =      {"tanh", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_CEIL] =      {"ceil", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_FLOOR] =     {"floor", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_EXP] =       {"exp", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_LOG] =       {"log", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_LOG10] =     {"log10", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
    [JIM_EXPROP_FUNC_SQRT] =      {"sqrt", 400, 1, JimExprOpDoubleUnary, LAZY_NONE },
#endif

    [JIM_EXPROP_NOT] =            {"!", 300, 1, JimExprOpNumUnary, LAZY_NONE },
    [JIM_EXPROP_BITNOT] =         {"~", 300, 1, JimExprOpIntUnary, LAZY_NONE },
    [JIM_EXPROP_UNARYMINUS] =     {NULL, 300, 1, JimExprOpNumUnary, LAZY_NONE },
    [JIM_EXPROP_UNARYPLUS] =      {NULL, 300, 1, JimExprOpNumUnary, LAZY_NONE }, 

    [JIM_EXPROP_POW] =            {"**", 250, 2, JimExprOpBin, LAZY_NONE },

    [JIM_EXPROP_MUL] =            {"*", 200, 2, JimExprOpBin, LAZY_NONE },
    [JIM_EXPROP_DIV] =            {"/", 200, 2, JimExprOpBin, LAZY_NONE },
    [JIM_EXPROP_MOD] =            {"%", 200, 2, JimExprOpIntBin, LAZY_NONE },

    [JIM_EXPROP_SUB] =            {"-", 100, 2, JimExprOpBin, LAZY_NONE },
    [JIM_EXPROP_ADD] =            {"+", 100, 2, JimExprOpBin, LAZY_NONE },

    [JIM_EXPROP_ROTL] =           {"<<<", 90, 2, JimExprOpIntBin, LAZY_NONE },
    [JIM_EXPROP_ROTR] =           {">>>", 90, 2, JimExprOpIntBin, LAZY_NONE },
    [JIM_EXPROP_LSHIFT] =         {"<<", 90, 2, JimExprOpIntBin, LAZY_NONE },
    [JIM_EXPROP_RSHIFT] =         {">>", 90, 2, JimExprOpIntBin, LAZY_NONE },

    [JIM_EXPROP_LT] =             {"<",  80, 2, JimExprOpBin, LAZY_NONE },
    [JIM_EXPROP_GT] =             {">",  80, 2, JimExprOpBin, LAZY_NONE },
    [JIM_EXPROP_LTE] =            {"<=", 80, 2, JimExprOpBin, LAZY_NONE },
    [JIM_EXPROP_GTE] =            {">=", 80, 2, JimExprOpBin, LAZY_NONE },

    [JIM_EXPROP_NUMEQ] =          {"==", 70, 2, JimExprOpBin, LAZY_NONE },
    [JIM_EXPROP_NUMNE] =          {"!=", 70, 2, JimExprOpBin, LAZY_NONE },

    [JIM_EXPROP_STREQ] =          {"eq", 60, 2, JimExprOpStrBin, LAZY_NONE },
    [JIM_EXPROP_STRNE] =          {"ne", 60, 2, JimExprOpStrBin, LAZY_NONE },

    [JIM_EXPROP_STRIN] =          {"in", 55, 2, JimExprOpStrBin, LAZY_NONE },
    [JIM_EXPROP_STRNI] =          {"ni", 55, 2, JimExprOpStrBin, LAZY_NONE },

    [JIM_EXPROP_BITAND] =         {"&", 50, 2, JimExprOpIntBin, LAZY_NONE },
    [JIM_EXPROP_BITXOR] =         {"^", 49, 2, JimExprOpIntBin, LAZY_NONE },
    [JIM_EXPROP_BITOR] =          {"|", 48, 2, JimExprOpIntBin, LAZY_NONE },

    [JIM_EXPROP_LOGICAND] =       {"&&", 10, 2, NULL, LAZY_OP },
    [JIM_EXPROP_LOGICOR] =        {"||", 9, 2, NULL, LAZY_OP },

    [JIM_EXPROP_TERNARY] =        {"?", 5, 2, JimExprOpNull, LAZY_OP },
    [JIM_EXPROP_COLON] =          {":", 5, 2, JimExprOpColon, LAZY_OP },

    /* private operators */
    [JIM_EXPROP_TERNARY_LEFT] =   {NULL, 5, 2, JimExprOpTernaryLeft, LAZY_LEFT },
    [JIM_EXPROP_TERNARY_RIGHT] =  {NULL, 5, 2, JimExprOpNull, LAZY_RIGHT },
    [JIM_EXPROP_COLON_LEFT] =     {NULL, 5, 2, JimExprOpColonLeft, LAZY_LEFT },
    [JIM_EXPROP_COLON_RIGHT] =    {NULL, 5, 2, JimExprOpNull, LAZY_RIGHT },
    [JIM_EXPROP_LOGICAND_LEFT] =  {NULL, 10, 2, JimExprOpAndLeft, LAZY_LEFT },
    [JIM_EXPROP_LOGICAND_RIGHT] = {NULL, 10, 2, JimExprOpAndOrRight, LAZY_RIGHT },
    [JIM_EXPROP_LOGICOR_LEFT] =   {NULL, 9, 2, JimExprOpOrLeft, LAZY_LEFT },
    [JIM_EXPROP_LOGICOR_RIGHT] =  {NULL, 9, 2, JimExprOpAndOrRight, LAZY_RIGHT },
};

#define JIM_EXPR_OPERATORS_NUM \
    (sizeof(Jim_ExprOperators)/sizeof(struct Jim_ExprOperator))

int JimParseExpression(struct JimParserCtx *pc)
{
    /* Discard spaces and quoted newline */
    while (isspace(*pc->p) || (*(pc->p) == '\\' && *(pc->p+1) == '\n')) {
        pc->p++; pc->len--;
    }

    if (pc->len == 0) {
        pc->tstart = pc->tend = pc->p;
        pc->tline = pc->linenr;
        pc->tt = JIM_TT_EOL;
        pc->eof = 1;
        return JIM_OK;
    }
    switch(*(pc->p)) {
    case '(':
        pc->tstart = pc->tend = pc->p;
        pc->tline = pc->linenr;
        pc->tt = JIM_TT_SUBEXPR_START;
        pc->p++; pc->len--;
        break;
    case ')':
        pc->tstart = pc->tend = pc->p;
        pc->tline = pc->linenr;
        pc->tt = JIM_TT_SUBEXPR_END;
        pc->p++; pc->len--;
        break;
    case '[':
        return JimParseCmd(pc);
        break;
    case '$':
        if (JimParseVar(pc) == JIM_ERR)
            return JimParseExprOperator(pc);
        else
            return JIM_OK;
        break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': case '.':
        return JimParseExprNumber(pc);
        break;
    case '"':
    case '{':
        /* Here it's possible to reuse the List String parsing. */
        pc->tt = JIM_TT_NONE; /* Make sure it's sensed as a new word. */
        return JimParseListStr(pc);
        break;
    case 'N': case 'I':
    case 'n': case 'i':
        if (JimParseExprIrrational(pc) == JIM_ERR)
            return JimParseExprOperator(pc);
        break;
    default:
        return JimParseExprOperator(pc);
        break;
    }
    return JIM_OK;
}

int JimParseExprNumber(struct JimParserCtx *pc)
{
    int allowdot = 1;
    int allowhex = 0;

    /* Assume an integer for now */
    pc->tt = JIM_TT_EXPR_INT;
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (  isdigit(*pc->p) 
          || (allowhex && isxdigit(*pc->p) )
          || (allowdot && *pc->p == '.') 
          || (pc->p-pc->tstart == 1 && *pc->tstart == '0' &&
              (*pc->p == 'x' || *pc->p == 'X'))
          )
    {
        if ((*pc->p == 'x') || (*pc->p == 'X')) {
            allowhex = 1;
            allowdot = 0;
        }
        if (*pc->p == '.') {
            allowdot = 0;
            pc->tt = JIM_TT_EXPR_DOUBLE;
        }
        pc->p++; pc->len--;
        if (!allowhex && (*pc->p == 'e' || *pc->p == 'E') && (pc->p[1] == '-' || pc->p[1] == '+' || isdigit(pc->p[1]))) {
            pc->p += 2; pc->len -= 2;
            pc->tt = JIM_TT_EXPR_DOUBLE;
        }
    }
    pc->tend = pc->p-1;
    return JIM_OK;
}

int JimParseExprIrrational(struct JimParserCtx *pc)
{
    const char *Tokens[] = {"NaN", "nan", "NAN", "Inf", "inf", "INF", NULL};
    const char **token;
    for (token = Tokens; *token != NULL; token++) {
        int len = strlen(*token);
        if (strncmp(*token, pc->p, len) == 0) {
            pc->tstart = pc->p;
            pc->tend = pc->p + len - 1;
            pc->p += len; pc->len -= len;
            pc->tline = pc->linenr;
            pc->tt = JIM_TT_EXPR_DOUBLE;
            return JIM_OK;
        }
    }
    return JIM_ERR;
}

int JimParseExprOperator(struct JimParserCtx *pc)
{
    int i;
    int bestIdx = -1, bestLen = 0;

    /* Try to get the longest match. */
    for (i = JIM_TT_EXPR_OP; i < (signed)JIM_EXPR_OPERATORS_NUM; i++) {
        const char *opname;
        int oplen;

        opname = Jim_ExprOperators[i].name;
        if (opname == NULL) {
            continue;
        }
        oplen = strlen(opname);

        if (strncmp(opname, pc->p, oplen) == 0 && oplen > bestLen) {
            bestIdx = i;
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
        while (len && isspace(*p)) {
            len--;
            p++;
        }
        if (*p != '(') {
            return JIM_ERR;
        }
    }
    pc->tstart = pc->p;
    pc->tend = pc->p + bestLen - 1;
    pc->p += bestLen; pc->len -= bestLen;
    pc->tline = pc->linenr;

    pc->tt = bestIdx;
    return JIM_OK;
}

static const struct Jim_ExprOperator *JimExprOperatorInfoByOpcode(int opcode)
{
    return &Jim_ExprOperators[opcode];
}

/* debugging */
const char *tt_name(int type)
{
    static const char *tt_names[] = { "NIL", "STR", "ESC", "VAR", "ARY", "CMD", "SEP", "EOL", "EOF", "???", "(((", ")))", "INT", "DBL", "???" };
    if (type < JIM_TT_EXPR_OP) {
        return tt_names[type];
    }
    else {
        const struct Jim_ExprOperator *op = JimExprOperatorInfoByOpcode(type);
        static char buf[20];

        if (op && op->name) {
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

static Jim_ObjType exprObjType = {
    "expression",
    FreeExprInternalRep,
    DupExprInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

/* Expr bytecode structure */
typedef struct ExprByteCode {
    int len;                /* Length as number of tokens. */
    ScriptToken *token;     /* Tokens array. */
    int inUse;              /* Used for sharing. */
} ExprByteCode;

static void ExprFreeByteCode(Jim_Interp *interp, ExprByteCode *expr)
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
    ExprByteCode *expr = (void*) objPtr->internalRep.ptr;

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
static int ExprCheckCorrectness(ExprByteCode *expr)
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
        if (op) {
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
static void ExprAddLazyOperator(Jim_Interp *interp, ExprByteCode *expr, ParseToken *t)
{
    int i;

    int leftindex, arity, offset;

    /* Search for the end of the first operator */
    leftindex = expr->len-1;
    arity = 1;
    while (arity) {
        ScriptToken *tt = &expr->token[leftindex];
        if (tt->type >= JIM_TT_EXPR_OP) {
            arity += JimExprOperatorInfoByOpcode(tt->type)->arity;
        }
        arity--;
        leftindex--;
    }
    leftindex++;

    /* Move them up */
    memmove(&expr->token[leftindex+2], &expr->token[leftindex], sizeof(*expr->token)*(expr->len-leftindex));
    expr->len += 2;
    offset = (expr->len-leftindex)-1;

    /* Now we rely on the fact the the left and right version have opcodes
     * 1 and 2 after the main opcode respectively
     */
    expr->token[leftindex+1].type = t->type + 1;
    expr->token[leftindex+1].objPtr = interp->emptyObj;

    expr->token[leftindex].type = JIM_TT_EXPR_INT;
    expr->token[leftindex].objPtr = Jim_NewIntObj(interp, offset);

    /* Now add the 'R' operator */
    expr->token[expr->len].objPtr = interp->emptyObj;
    expr->token[expr->len].type = t->type + 2;
    expr->len++;

    /* Do we need to adjust the skip count for any &L, |L, ?L or :L in the left operand? */
    for (i = leftindex - 1; i > 0; i--) {
        if (JimExprOperatorInfoByOpcode(expr->token[i].type)->lazy == LAZY_LEFT) {
            if (expr->token[i - 1].objPtr->internalRep.wideValue + i - 1 >= leftindex) {
                expr->token[i - 1].objPtr->internalRep.wideValue += 2;
            }
        }
    }
}

static void ExprAddOperator(Jim_Interp *interp, ExprByteCode *expr, ParseToken *t)
{
    struct ScriptToken *token = &expr->token[expr->len];

    if (JimExprOperatorInfoByOpcode(t->type)->lazy == LAZY_OP) {
        ExprAddLazyOperator(interp, expr, t);
    }
    else {
        token->objPtr = interp->emptyObj;
        token->type = t->type;
        expr->len++;
    }
}

static ExprByteCode *ExprCreateByteCode(Jim_Interp *interp, const ParseTokenList *tokenlist)
{
    Jim_Stack stack;
    ExprByteCode *expr;
    int ok = 1;
    int i;
    int prevtt = JIM_TT_NONE;
    /* -1 for EOL */
    int count = tokenlist->count - 1;

    expr = Jim_Alloc(sizeof(*expr));
    expr->inUse = 1;
    expr->len = 0;

    Jim_InitStack(&stack);

    /* Need extra bytecodes for lazy operators */
    for (i = 0; i < tokenlist->count; i++) {
        ParseToken *t = &tokenlist->list[i];
        if (JimExprOperatorInfoByOpcode(t->type)->lazy == LAZY_OP) {
            count += 2;
        }
    }

    expr->token = Jim_Alloc(sizeof(ScriptToken)*count);

    for (i = 0; i < tokenlist->count && ok; i++) {
        ParseToken *t = &tokenlist->list[i];
        /* Next token will be stored here */
        struct ScriptToken *token = &expr->token[expr->len];

        if (t->type == JIM_TT_EOL) {
            break;
        }

        switch(t->type) {
            case JIM_TT_STR:
            case JIM_TT_ESC:
            case JIM_TT_VAR:
            case JIM_TT_DICTSUGAR:
            case JIM_TT_CMD:
                token->objPtr = Jim_NewStringObj(interp, t->token, t->len);
                token->type = t->type;
                expr->len++;
                break;

            case JIM_TT_EXPR_INT:
                token->objPtr = Jim_NewIntObj(interp, strtoull(t->token, NULL, 0));
                token->type = t->type;
                expr->len++;
                break;

            case JIM_TT_EXPR_DOUBLE:
                token->objPtr = Jim_NewDoubleObj(interp, strtod(t->token, NULL));
                token->type = t->type;
                expr->len++;
                break;

            case JIM_TT_SUBEXPR_START:
                Jim_StackPush(&stack, t);
                prevtt = JIM_TT_NONE;
                continue;

            case JIM_TT_SUBEXPR_END:
                ok = 0;
                while (Jim_StackLen(&stack)) {
                    ParseToken *tt = Jim_StackPop(&stack);

                    if (tt->type == JIM_TT_SUBEXPR_START) {
                        ok = 1;
                        break;
                    }

                    ExprAddOperator(interp, expr, tt);
                }
                if (!ok) {
                    Jim_SetResultString(interp, "Unexpected close parenthesis", -1);
                    goto err;
                }
                break;


            default: {
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
                    const struct Jim_ExprOperator *tt_op = JimExprOperatorInfoByOpcode(tt->type);

                    /* XXX: Should handle right-to-left associativity of ?: operator */

                    if (op->arity != 1 && tt_op->precedence >= op->precedence) {
                        ExprAddOperator(interp, expr, tt);
                        Jim_StackPop(&stack);
                    } else {
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
        ExprAddOperator(interp, expr, tt);
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
int SetExprFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    int exprTextLen;
    const char *exprText;
    struct JimParserCtx parser;
    struct ExprByteCode *expr;
    ParseTokenList tokenlist;
    int rc = JIM_ERR;

    exprText = Jim_GetString(objPtr, &exprTextLen);

    /* Initially tokenise the expression into tokenlist */
    ScriptTokenListInit(&tokenlist);

    JimParserInit(&parser, exprText, exprTextLen, 0);
    while (!JimParserEof(&parser)) {
        if (JimParseExpression(&parser) != JIM_OK) {
            ScriptTokenListFree(&tokenlist);
invalidexpr:
            Jim_SetResultFormatted(interp, "syntax error in expression: \"%#s\"", objPtr);
            expr = NULL;
            goto err;
        }

        ScriptAddToken(&tokenlist, parser.tstart, parser.tend - parser.tstart + 1, parser.tt, parser.tline);
    }

    /* Now create the expression bytecode from the tokenlist */
    expr = ExprCreateByteCode(interp, &tokenlist);

    /* No longer need the token list */
    ScriptTokenListFree(&tokenlist);

    if (!expr) {
        goto err;
    }

#if 0
    int i;
    printf("==== Expr ====\n");
    for (i = 0; i < expr->len; i++) {
        ScriptToken *t = &expr->token[i];
        
        printf("[%2d] %s '%s'\n", i, tt_name(t->type), Jim_GetString(t->objPtr, NULL));
    }
#endif

    /* Check program correctness. */
    if (ExprCheckCorrectness(expr) != JIM_OK) {
        ExprFreeByteCode(interp, expr);
        goto invalidexpr;
    }

    rc = JIM_OK;

#if 0
    printf("==== Expr ====\n");
    for (i = 0; i < expr->len; i++) {
        ScriptToken *t = &expr->token[i];
        
        printf("[%2d] %s '%s'\n", i, tt_name(t->type), Jim_GetString(t->objPtr, NULL));
    }
#endif

err:
    /* Free the old internal rep and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    Jim_SetIntRepPtr(objPtr, expr);
    objPtr->typePtr = &exprObjType;
    return rc;
}

static ExprByteCode *Jim_GetExpression(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (objPtr->typePtr != &exprObjType) {
        if (SetExprFromAny(interp, objPtr) != JIM_OK) {
            return NULL;
        }
    }
    return (ExprByteCode*) Jim_GetIntRepPtr(objPtr);
}

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

int Jim_EvalExpression(Jim_Interp *interp, Jim_Obj *exprObjPtr,
        Jim_Obj **exprResultPtrPtr)
{
    ExprByteCode *expr;
    Jim_Obj *staticStack[JIM_EE_STATICSTACK_LEN];
    int i;
    int retcode = JIM_OK;
    struct JimExprState e;

    expr = Jim_GetExpression(interp, exprObjPtr);
    if (!expr) {
        return JIM_ERR; /* error in expression. */
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
        
        switch(expr->len) {
        case 1:
            if (expr->token[0].type == JIM_TT_EXPR_INT) {
                *exprResultPtrPtr = expr->token[0].objPtr;
                Jim_IncrRefCount(*exprResultPtrPtr);
                return JIM_OK;
            }
            if (expr->token[0].type == JIM_TT_VAR) {
                objPtr = Jim_GetVariable(interp, expr->token[0].objPtr, JIM_ERRMSG);
                if (objPtr) {
                    *exprResultPtrPtr = objPtr;
                    Jim_IncrRefCount(*exprResultPtrPtr);
                    return JIM_OK;
                }
            }
            break;

        case 2:
            if (expr->token[1].type == JIM_EXPROP_NOT && expr->token[0].type == JIM_TT_VAR) {
                jim_wide wideValue;
                objPtr = Jim_GetVariable(interp, expr->token[0].objPtr, JIM_NONE);
                if (objPtr && Jim_IsWide(objPtr) && Jim_GetWide(interp, objPtr, &wideValue) == JIM_OK) {
                    *exprResultPtrPtr = wideValue ? interp->falseObj : interp->trueObj;
                    Jim_IncrRefCount(*exprResultPtrPtr);
                    return JIM_OK;
                }
            }
            break;

        case 3:
            if (expr->token[0].type == JIM_TT_VAR && (expr->token[1].type == JIM_TT_EXPR_INT || expr->token[1].type == JIM_TT_VAR)) {
                switch(expr->token[2].type) {
                    case JIM_EXPROP_LT:
                    case JIM_EXPROP_LTE:
                    case JIM_EXPROP_GT:
                    case JIM_EXPROP_GTE:
                    case JIM_EXPROP_NUMEQ:
                    case JIM_EXPROP_NUMNE: {
                        /* optimise ok */
                        jim_wide wideValueA;
                        jim_wide wideValueB;

                        objPtr = Jim_GetVariable(interp, expr->token[0].objPtr, JIM_NONE);
                        if (objPtr && Jim_IsWide(objPtr) && Jim_GetWide(interp, objPtr, &wideValueA) == JIM_OK) {
                            if (expr->token[1].type == JIM_TT_VAR) {
                                objPtr = Jim_GetVariable(interp, expr->token[1].objPtr, JIM_NONE);
                            }
                            else {
                                objPtr = expr->token[1].objPtr;
                            }
                            if (objPtr && Jim_IsWide(objPtr) && Jim_GetWide(interp, objPtr, &wideValueB) == JIM_OK) {
                                int cmpRes;
                                switch (expr->token[2].type) {
                                case JIM_EXPROP_LT:
                                    cmpRes = wideValueA < wideValueB; break;
                                case JIM_EXPROP_LTE:
                                    cmpRes = wideValueA <= wideValueB; break;
                                case JIM_EXPROP_GT:
                                    cmpRes = wideValueA > wideValueB; break;
                                case JIM_EXPROP_GTE:
                                    cmpRes = wideValueA >= wideValueB; break;
                                case JIM_EXPROP_NUMEQ:
                                    cmpRes = wideValueA == wideValueB; break;
                                case JIM_EXPROP_NUMNE:
                                    cmpRes = wideValueA != wideValueB; break;
                                default: /*notreached */
                                    cmpRes = 0;
                                }
                                *exprResultPtrPtr = cmpRes ? interp->trueObj : interp->falseObj;
                                Jim_IncrRefCount(*exprResultPtrPtr);
                                return JIM_OK;
                            }
                        }
                    }
                }
            }
            break;
        }
    }
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
        e.stack = Jim_Alloc(sizeof(Jim_Obj*)*expr->len);
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
                objPtr = Jim_ExpandDictSugar(interp, expr->token[i].objPtr);
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

            default: {
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
        if (Jim_GetDouble(interp, exprResultPtr, &doubleValue) != JIM_OK)
        {
            Jim_DecrRefCount(interp, exprResultPtr);
            return JIM_ERR;
        } else {
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
 
typedef struct ScanFmtPartDescr {
    char type;         /* Type of conversion (e.g. c, d, f) */
    char modifier;     /* Modify type (e.g. l - long, h - short */
    size_t  width;     /* Maximal width of input to be converted */
    int  pos;          /* -1 - no assign, 0 - natural pos, >0 - XPG3 pos */ 
    char *arg;         /* Specification of a CHARSET conversion */
    char *prefix;      /* Prefix to be scanned literally before conversion */
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

typedef struct ScanFmtStringObj {
    jim_wide        size;         /* Size of internal repr in bytes */
    char            *stringRep;   /* Original string representation */
    size_t          count;        /* Number of ScanFmtPartDescr contained */
    size_t          convCount;    /* Number of conversions that will assign */
    size_t          maxPos;       /* Max position index if XPG3 is used */
    const char      *error;       /* Ptr to error text (NULL if no error */
    char            *scratch;     /* Some scratch pad used by Jim_ScanString */
    ScanFmtPartDescr descr[1];    /* The vector of partial descriptions */
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
    Jim_Free((char*)objPtr->internalRep.ptr);
    objPtr->internalRep.ptr = 0;
}

void DupScanFmtInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    size_t size = (size_t)((ScanFmtStringObj*)srcPtr->internalRep.ptr)->size;
    ScanFmtStringObj *newVec = (ScanFmtStringObj*)Jim_Alloc(size);

    JIM_NOTUSED(interp);
    memcpy(newVec, srcPtr->internalRep.ptr, size);
    dupPtr->internalRep.ptr = newVec;
    dupPtr->typePtr = &scanFmtStringObjType;
}

void UpdateStringOfScanFmt(Jim_Obj *objPtr)
{
    char *bytes = ((ScanFmtStringObj*)objPtr->internalRep.ptr)->stringRep;

    objPtr->bytes = Jim_StrDup(bytes);
    objPtr->length = strlen(bytes);
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
    for (i=0, maxCount=0; i < maxFmtLen; ++i)
        if (fmt[i] == '%')
            ++maxCount;
    /* Calculate an approximation of the memory necessary */
    approxSize = sizeof(ScanFmtStringObj)           /* Size of the container */
        + (maxCount + 1) * sizeof(ScanFmtPartDescr) /* Size of all partials */
        + maxFmtLen * sizeof(char) + 3 + 1          /* Scratch + "%n" + '\0' */
        + maxFmtLen * sizeof(char) + 1              /* Original stringrep */
        + maxFmtLen * sizeof(char)                  /* Arg for CHARSETs */
        + (maxCount +1) * sizeof(char)              /* '\0' for every partial */
        + 1;                                        /* safety byte */
    fmtObj = (ScanFmtStringObj*)Jim_Alloc(approxSize);
    memset(fmtObj, 0, approxSize);
    fmtObj->size = approxSize;
    fmtObj->maxPos = 0;
    fmtObj->scratch = (char*)&fmtObj->descr[maxCount+1];
    fmtObj->stringRep = fmtObj->scratch + maxFmtLen + 3 + 1;
    memcpy(fmtObj->stringRep, fmt, maxFmtLen);
    buffer = fmtObj->stringRep + maxFmtLen + 1;
    objPtr->internalRep.ptr = fmtObj;
    objPtr->typePtr = &scanFmtStringObjType;
    for (i=0, curr=0; fmt < fmtEnd; ++fmt) {
        int width=0, skip;
        ScanFmtPartDescr *descr = &fmtObj->descr[curr];
        fmtObj->count++;
        descr->width = 0;                   /* Assume width unspecified */ 
        /* Overread and store any "literal" prefix */
        if (*fmt != '%' || fmt[1] == '%') {
            descr->type = 0;
            descr->prefix = &buffer[i];
            for (; fmt < fmtEnd; ++fmt) {
                if (*fmt == '%') {
                    if (fmt[1] != '%') break;
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
        descr->pos = 0;                     /* Assume "natural" positioning */
        if (*fmt == '*') {
            descr->pos = -1;       /* Okay, conversion will not be assigned */
            ++fmt;
        } else
            fmtObj->convCount++;    /* Otherwise count as assign-conversion */
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
                for (prev=0; prev < curr; ++prev) {
                    if (fmtObj->descr[prev].pos == -1) continue;
                    if (fmtObj->descr[prev].pos == descr->pos) {
                        fmtObj->error = "variable is assigned by multiple \"%n$\" conversion specifiers";
                        return JIM_ERR;
                    }
                }
                /* Try to find a width after the XPG3 specifier */
                if (sscanf(fmt, "%d%n", &width, &skip) == 1) {
                    descr->width = width;
                    fmt += skip;
                }
                if (descr->pos > 0 && (size_t)descr->pos > fmtObj->maxPos)
                    fmtObj->maxPos = descr->pos;
            } else {
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
            if (*fmt == '^') buffer[i++] = *fmt++;
            if (*fmt == ']') buffer[i++] = *fmt++;
            while (*fmt && *fmt != ']') buffer[i++] = *fmt++;
            if (*fmt != ']') {
                fmtObj->error = "unmatched [ in format string";
                return JIM_ERR;
            } 
            end = i;
            buffer[i++] = 0;
            /* In case a range fence was given "backwards", swap it */
            while (swapped) {
                swapped = 0;
                for (j=beg+1; j < end-1; ++j) {
                    if (buffer[j] == '-' && buffer[j-1] > buffer[j+1]) {
                        char tmp = buffer[j-1];
                        buffer[j-1] = buffer[j+1];
                        buffer[j+1] = tmp;
                        swapped = 1;
                    }
                }
            }
        } else {
            /* Remember any valid modifier if given */
            if (strchr("hlL", *fmt) != 0)
                descr->modifier = tolower((int)*fmt++);
            
            descr->type = *fmt;
            if (strchr("efgcsndoxui", *fmt) == 0) {
                fmtObj->error = "bad scan conversion character";
                return JIM_ERR;
            } else if (*fmt == 'c' && descr->width != 0) {
                fmtObj->error = "field width may not be specified in %c "
                    "conversion";
                return JIM_ERR;
            } else if (*fmt == 'u' && descr->modifier == 'l') {
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

/* Some Bit testing/setting/cleaning routines. For now only used in handling
 * charsets ([a-z123]) within scanning. Later on perhaps a base for a 
 * bitvector implementation in Jim? */ 

static int JimTestBit(const char *bitvec, char ch)
{
    div_t pos = div(ch-1, 8);
    return bitvec[pos.quot] & (1 << pos.rem);
}

static void JimSetBit(char *bitvec, char ch)
{
    div_t pos = div(ch-1, 8);
    bitvec[pos.quot] |= (1 << pos.rem);
}

#if 0 /* currently not used */
static void JimClearBit(char *bitvec, char ch)
{
    div_t pos = div(ch-1, 8);
    bitvec[pos.quot] &= ~(1 << pos.rem);
}
#endif

/* JimScanAString is used to scan an unspecified string that ends with
 * next WS, or a string that is specified via a charset. The charset
 * is currently implemented in a way to only allow for usage with
 * ASCII. Whenever we will switch to UNICODE, another idea has to
 * be born :-/
 *
 * FIXME: Works only with ASCII */

static Jim_Obj *
JimScanAString(Jim_Interp *interp, const char *sdescr, const char *str)
{
    size_t i;
    Jim_Obj *result;
    char charset[256/8+1];  /* A Charset may contain max 256 chars */
    char *buffer = Jim_Alloc(strlen(str)+1), *anchor = buffer;

    /* First init charset to nothing or all, depending if a specified
     * or an unspecified string has to be parsed */
    memset(charset, (sdescr ? 0 : 255), sizeof(charset));
    if (sdescr) {
        /* There was a set description given, that means we are parsing
         * a specified string. So we have to build a corresponding 
         * charset reflecting the description */
        int notFlag = 0;
        /* Should the set be negated at the end? */
        if (*sdescr == '^') {
            notFlag = 1;
            ++sdescr;
        }
        /* Here '-' is meant literally and not to define a range */
        if (*sdescr == '-') {
            JimSetBit(charset, '-');
            ++sdescr;
        }
        while (*sdescr) {
            if (sdescr[1] == '-' && sdescr[2] != 0) {
                /* Handle range definitions */
                int i;
                for (i=sdescr[0]; i <= sdescr[2]; ++i)
                    JimSetBit(charset, (char)i);
                sdescr += 3;
            } else {
                /* Handle verbatim character definitions */
                JimSetBit(charset, *sdescr++);
            }
        }
        /* Negate the charset if there was a NOT given */
        for (i=0; notFlag && i < sizeof(charset); ++i)
            charset[i] = ~charset[i];
    } 
    /* And after all the mess above, the real work begin ... */
    while (str && *str) {
        if (!sdescr && isspace(*str))
            break; /* EOS via WS if unspecified */
        if (JimTestBit(charset, *str)) *buffer++ = *str++;
        else break;             /* EOS via mismatch if specified scanning */
    }
    *buffer = 0;                /* Close the string properly ... */
    result = Jim_NewStringObj(interp, anchor, -1);
    Jim_Free(anchor);           /* ... and free it afer usage */
    return result;
}

/* ScanOneEntry will scan one entry out of the string passed as argument.
 * It use the sscanf() function for this task. After extracting and
 * converting of the value, the count of scanned characters will be
 * returned of -1 in case of no conversion tool place and string was
 * already scanned thru */

static int ScanOneEntry(Jim_Interp *interp, const char *str, long pos,
        ScanFmtStringObj *fmtObj, long index, Jim_Obj **valObjPtr)
{
    const char *tok;
    const ScanFmtPartDescr *descr = &fmtObj->descr[index];
    size_t sLen = strlen(&str[pos]), scanned = 0;
    size_t anchor = pos;
    int i;

    /* First pessimistically assume, we will not scan anything :-) */
    *valObjPtr = 0;
    if (descr->prefix) {
        /* There was a prefix given before the conversion, skip it and adjust
         * the string-to-be-parsed accordingly */
        for (i=0; str[pos] && descr->prefix[i]; ++i) {
            /* If prefix require, skip WS */
            if (isspace(descr->prefix[i]))
                while (str[pos] && isspace(str[pos])) ++pos;
            else if (descr->prefix[i] != str[pos]) 
                break;  /* Prefix do not match here, leave the loop */
            else
                ++pos;  /* Prefix matched so far, next round */
        }
        if (str[pos] == 0)
            return -1;  /* All of str consumed: EOF condition */
        else if (descr->prefix[i] != 0)
            return 0;   /* Not whole prefix consumed, no conversion possible */
    }
    /* For all but following conversion, skip leading WS */
    if (descr->type != 'c' && descr->type != '[' && descr->type != 'n')
        while (isspace(str[pos])) ++pos;
    /* Determine how much skipped/scanned so far */
    scanned = pos - anchor;
    if (descr->type == 'n') {
        /* Return pseudo conversion means: how much scanned so far? */
        *valObjPtr = Jim_NewIntObj(interp, anchor + scanned);
    } else if (str[pos] == 0) {
        /* Cannot scan anything, as str is totally consumed */
        return -1;
    } else {
        /* Processing of conversions follows ... */
        if (descr->width > 0) {
            /* Do not try to scan as fas as possible but only the given width.
             * To ensure this, we copy the part that should be scanned. */
            size_t tLen = descr->width > sLen ? sLen : descr->width;
            tok = Jim_StrDupLen(&str[pos], tLen);
        } else {
            /* As no width was given, simply refer to the original string */
            tok = &str[pos];
        }
        switch (descr->type) {
            case 'c':
                *valObjPtr = Jim_NewIntObj(interp, *tok);
                scanned += 1;
                break;
            case 'd': case 'o': case 'x': case 'u': case 'i': {
                char *endp;  /* Position where the number finished */
                jim_wide w;

                int base = descr->type == 'o' ? 8
                    : descr->type == 'x' ? 16
                    : descr->type == 'i' ? 0
                    : 10;
                    
                /* Try to scan a number with the given base */
                w = strtoull(tok, &endp, base);
                if (endp == tok && base == 0) {
                    /* If scanning failed, and base was undetermined, simply
                     * put it to 10 and try once more. This should catch the
                     * case where %i begin to parse a number prefix (e.g. 
                     * '0x' but no further digits follows. This will be
                     * handled as a ZERO followed by a char 'x' by Tcl */
                    w = strtoull(tok, &endp, 10);
                }

                if (endp != tok) {
                    /* There was some number sucessfully scanned! */
                    *valObjPtr = Jim_NewIntObj(interp, w);

                    /* Adjust the number-of-chars scanned so far */
                    scanned += endp - tok;
                } else {
                    /* Nothing was scanned. We have to determine if this
                     * happened due to e.g. prefix mismatch or input str
                     * exhausted */
                    scanned = *tok ? 0 : -1;
                }
                break;
            }
            case 's': case '[': {
                *valObjPtr = JimScanAString(interp, descr->arg, tok);
                scanned += Jim_Length(*valObjPtr);
                break;
            }
            case 'e': case 'f': case 'g': {
                char *endp;
                double value = strtod(tok, &endp);

                if (endp != tok) {
                    /* There was some number sucessfully scanned! */
                    *valObjPtr = Jim_NewDoubleObj(interp, value);
                    /* Adjust the number-of-chars scanned so far */
                    scanned += endp - tok;
                } else {
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
        if (tok != &str[pos])
            Jim_Free((char*)tok);
    }
    return scanned;
}

/* Jim_ScanString is the workhorse of string scanning. It will scan a given
 * string and returns all converted (and not ignored) values in a list back
 * to the caller. If an error occured, a NULL pointer will be returned */

Jim_Obj *Jim_ScanString(Jim_Interp *interp, Jim_Obj *strObjPtr,
        Jim_Obj *fmtObjPtr, int flags)
{
    size_t i, pos;
    int scanned = 1;
    const char *str = Jim_GetString(strObjPtr, 0);
    Jim_Obj *resultList = 0;
    Jim_Obj **resultVec = 0;
    int resultc;
    Jim_Obj *emptyStr = 0;
    ScanFmtStringObj *fmtObj;

    /* This should never happen. The format object should already be of the correct type */
    if (fmtObjPtr->typePtr != &scanFmtStringObjType) {
        Jim_Panic(interp, "Jim_ScanString() for non-scan format");
    }
    fmtObj = (ScanFmtStringObj*)fmtObjPtr->internalRep.ptr;
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
    resultList = Jim_NewListObj(interp, 0, 0);
    if (fmtObj->maxPos > 0) {
        for (i=0; i < fmtObj->maxPos; ++i)
            Jim_ListAppendElement(interp, resultList, emptyStr);
        JimListGetElements(interp, resultList, &resultc, &resultVec);
    }
    /* Now handle every partial format description */
    for (i=0, pos=0; i < fmtObj->count; ++i) {
        ScanFmtPartDescr *descr = &(fmtObj->descr[i]);
        Jim_Obj *value = 0;
        /* Only last type may be "literal" w/o conversion - skip it! */
        if (descr->type == 0) continue;
        /* As long as any conversion could be done, we will proceed */
        if (scanned > 0)
            scanned = ScanOneEntry(interp, str, pos, fmtObj, i, &value);
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
        } else if (descr->pos == 0)
            /* Otherwise append it to the result list if no XPG3 was given */
            Jim_ListAppendElement(interp, resultList, value);
        else if (resultVec[descr->pos-1] == emptyStr) {
            /* But due to given XPG3, put the value into the corr. slot */
            Jim_DecrRefCount(interp, resultVec[descr->pos-1]);
            Jim_IncrRefCount(value);
            resultVec[descr->pos-1] = value;
        } else {
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
    return (Jim_Obj*)EOF;
err:
    Jim_DecrRefCount(interp, emptyStr);
    Jim_FreeNewObj(interp, resultList);
    return 0;
}

/* -----------------------------------------------------------------------------
 * Pseudo Random Number Generation
 * ---------------------------------------------------------------------------*/
static void JimPrngSeed(Jim_Interp *interp, const unsigned char *seed,
        int seedLen);

/* Initialize the sbox with the numbers from 0 to 255 */
static void JimPrngInit(Jim_Interp *interp)
{
    int i;
    unsigned int seed[256];

    interp->prngState = Jim_Alloc(sizeof(Jim_PrngState));
    for (i = 0; i < 256; i++)
        seed[i] = (rand() ^ time(NULL) ^ clock());
    JimPrngSeed(interp, (unsigned char*) seed, sizeof(int)*256);
}

/* Generates N bytes of random data */
static void JimRandomBytes(Jim_Interp *interp, void *dest, unsigned int len)
{
    Jim_PrngState *prng;
    unsigned char *destByte = (unsigned char*) dest;
    unsigned int si, sj, x;

    /* initialization, only needed the first time */
    if (interp->prngState == NULL)
        JimPrngInit(interp);
    prng = interp->prngState;
    /* generates 'len' bytes of pseudo-random numbers */
    for (x = 0; x < len; x++) {
        prng->i = (prng->i+1) & 0xff;
        si = prng->sbox[prng->i];
        prng->j = (prng->j + si) & 0xff;
        sj = prng->sbox[prng->j];
        prng->sbox[prng->i] = sj;
        prng->sbox[prng->j] = si;
        *destByte++ = prng->sbox[(si+sj)&0xff];
    }
}

/* Re-seed the generator with user-provided bytes */
static void JimPrngSeed(Jim_Interp *interp, const unsigned char *seed,
        int seedLen)
{
    int i;
    unsigned char buf[256];
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

        t = prng->sbox[i&0xFF];
        prng->sbox[i&0xFF] = prng->sbox[seed[i]];
        prng->sbox[seed[i]] = t;
    }
    prng->i = prng->j = 0;
    /* discard the first 256 bytes of stream. */
    JimRandomBytes(interp, buf, 256);
}

/* -----------------------------------------------------------------------------
 * Eval
 * ---------------------------------------------------------------------------*/
#define JIM_EVAL_SARGV_LEN 8 /* static arguments vector length */
#define JIM_EVAL_SINTV_LEN 8 /* static interpolation vector length */

static int JimCallProcedure(Jim_Interp *interp, Jim_Cmd *cmd, int argc,
        Jim_Obj *const *argv);

/* Handle calls to the [unknown] command */
static int JimUnknown(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj **v, *sv[JIM_EVAL_SARGV_LEN];
    int retCode;

    /* If JimUnknown() is recursively called too many times...
     * done here
     */
    if (interp->unknown_called > 50) {
        return JIM_ERR;
    }

    /* If the [unknown] command does not exists returns
     * just now */
    if (Jim_GetCommand(interp, interp->unknown, JIM_NONE) == NULL)
        return JIM_ERR;

    /* The object interp->unknown just contains
     * the "unknown" string, it is used in order to
     * avoid to lookup the unknown command every time
     * but instread to cache the result. */
    if (argc+1 <= JIM_EVAL_SARGV_LEN)
        v = sv;
    else
        v = Jim_Alloc(sizeof(Jim_Obj*)*(argc+1));
    /* Make a copy of the arguments vector, but shifted on
     * the right of one position. The command name of the
     * command will be instead the first argument of the
     * [unknonw] call. */
    memcpy(v+1, argv, sizeof(Jim_Obj*)*argc);
    v[0] = interp->unknown;
    /* Call it */
    interp->unknown_called++;
    retCode = Jim_EvalObjVector(interp, argc+1, v);
    interp->unknown_called--;

    /* Clean up */
    if (v != sv)
        Jim_Free(v);
    return retCode;
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
    Jim_Cmd *cmdPtr;

    /* Incr refcount of arguments. */
    for (i = 0; i < objc; i++)
        Jim_IncrRefCount(objv[i]);
    /* Command lookup */
    cmdPtr = Jim_GetCommand(interp, objv[0], JIM_ERRMSG);
    if (cmdPtr == NULL) {
        retcode = JimUnknown(interp, objc, objv);
    } else {
        /* Call it -- Make sure result is an empty object. */
        Jim_SetEmptyResult(interp);
        if (cmdPtr->cmdProc) {
            interp->cmdPrivData = cmdPtr->privData;
            retcode = cmdPtr->cmdProc(interp, objc, objv);
        } else {
            retcode = JimCallProcedure(interp, cmdPtr, objc, objv);
        }
    }
    /* Decr refcount of arguments and return the retcode */
    for (i = 0; i < objc; i++)
        Jim_DecrRefCount(interp, objv[i]);

    return retcode;
}

/* Interpolate the given tokens into a unique Jim_Obj returned by reference
 * via *objPtrPtr. This function is only called by Jim_EvalObj().
 * The returned object has refcount = 0. */
int Jim_InterpolateTokens(Jim_Interp *interp, ScriptToken *token,
        int tokens, Jim_Obj **objPtrPtr)
{
    int totlen = 0, i, retcode;
    Jim_Obj **intv;
    Jim_Obj *sintv[JIM_EVAL_SINTV_LEN];
    Jim_Obj *objPtr;
    char *s;

    if (tokens <= JIM_EVAL_SINTV_LEN)
        intv = sintv;
    else
        intv = Jim_Alloc(sizeof(Jim_Obj*)*
                tokens);
    /* Compute every token forming the argument
     * in the intv objects vector. */
    for (i = 0; i < tokens; i++) {
        switch(token[i].type) {
        case JIM_TT_ESC:
        case JIM_TT_STR:
            intv[i] = token[i].objPtr;
            break;
        case JIM_TT_VAR:
            intv[i] = Jim_GetVariable(interp, token[i].objPtr, JIM_ERRMSG);
            if (!intv[i]) {
                retcode = JIM_ERR;
                goto err;
            }
            break;
        case JIM_TT_DICTSUGAR:
            intv[i] = Jim_ExpandDictSugar(interp, token[i].objPtr);
            if (!intv[i]) {
                retcode = JIM_ERR;
                goto err;
            }
            break;
        case JIM_TT_CMD:
            retcode = Jim_EvalObj(interp, token[i].objPtr);
            if (retcode != JIM_OK)
                goto err;
            intv[i] = Jim_GetResult(interp);
            break;
        default:
            Jim_Panic(interp,
              "default token type reached "
              "in Jim_InterpolateTokens().");
            break;
        }
        Jim_IncrRefCount(intv[i]);
        /* Make sure there is a valid
         * string rep, and add the string
         * length to the total legnth. */
        Jim_GetString(intv[i], NULL);
        totlen += intv[i]->length;
    }
    /* Concatenate every token in an unique
     * object. */
    objPtr = Jim_NewStringObjNoAlloc(interp,
            NULL, 0);

    if (tokens == 4 && token[0].type == JIM_TT_ESC && token[1].type == JIM_TT_ESC && token[2].type == JIM_TT_VAR) {
        /* May be able to do fast interpolated object -> dictSubst */
        objPtr->typePtr = &interpolatedObjType;
        objPtr->internalRep.twoPtrValue.ptr1 = token;
        objPtr->internalRep.twoPtrValue.ptr2 = intv[2];
        Jim_IncrRefCount(intv[2]);
    }

    s = objPtr->bytes = Jim_Alloc(totlen+1);
    objPtr->length = totlen;
    for (i = 0; i < tokens; i++) {
        memcpy(s, intv[i]->bytes, intv[i]->length);
        s += intv[i]->length;
        Jim_DecrRefCount(interp, intv[i]);
    }
    objPtr->bytes[totlen] = '\0';
    /* Free the intv vector if not static. */
    if (tokens > JIM_EVAL_SINTV_LEN)
        Jim_Free(intv);

    *objPtrPtr = objPtr;
    return JIM_OK;
err:
    i--;
    for (; i >= 0; i--)
        Jim_DecrRefCount(interp, intv[i]);
    if (tokens > JIM_EVAL_SINTV_LEN)
        Jim_Free(intv);
    return retcode;
}

/* Helper of Jim_EvalObj() to perform argument expansion.
 * Basically this function append an argument to 'argv'
 * (and increments argc by reference accordingly), performing
 * expansion of the list object if 'expand' is non-zero, or
 * just adding objPtr to argv if 'expand' is zero. */
void Jim_ExpandArgument(Jim_Interp *interp, Jim_Obj ***argv,
        int *argcPtr, int expand, Jim_Obj *objPtr)
{
    if (!expand) {
        (*argv) = Jim_Realloc(*argv, sizeof(Jim_Obj*)*((*argcPtr)+1));
        /* refcount of objPtr not incremented because
         * we are actually transfering a reference from
         * the old 'argv' to the expanded one. */
        (*argv)[*argcPtr] = objPtr;
        (*argcPtr)++;
    } else {
        int len, i;

        len = Jim_ListLength(interp, objPtr);
        (*argv) = Jim_Realloc(*argv, sizeof(Jim_Obj*)*((*argcPtr)+len));
        for (i = 0; i < len; i++) {
            (*argv)[*argcPtr] = objPtr->internalRep.listValue.ele[i];
            Jim_IncrRefCount(objPtr->internalRep.listValue.ele[i]);
            (*argcPtr)++;
        }
        /* The original object reference is no longer needed,
         * after the expansion it is no longer present on
         * the argument vector, but the single elements are
         * in its place. */
        Jim_DecrRefCount(interp, objPtr);
    }
}

static void JimAddErrorToStack(Jim_Interp *interp, int retcode, const char *filename, int line)
{
    int rc = retcode;

#if 0
    /* XXX: Don't create a stack frame for 'return -code error' */

    /* Pick up 'return -code error' too */
    if (retcode == JIM_RETURN) {
        rc = interp->returnCode;
    }
#endif
#if 0
    printf("JimAddErrorToStack: retcode=%s, %s:%d, ast=%d, errorFlag=%d\n",
        Jim_ReturnCode(retcode), filename, line, interp->addStackTrace, interp->errorFlag);
#endif

    if (rc == JIM_ERR && !interp->errorFlag) {
        /* This is the first error, so save the file/line information and reset the stack */
        interp->errorFlag = 1;
        JimSetErrorFileName(interp, filename);
        JimSetErrorLineNumber(interp, line);

        JimResetStackTrace(interp);
        /* Always add a level where the error first occurs */
        interp->addStackTrace++;
    }

    /* Now if this is an "interesting" level, add it to the stack trace */
    if (rc == JIM_ERR && interp->addStackTrace > 0) {
        /* Add the stack info for the current level */

        JimAppendStackTrace(interp, Jim_GetString(interp->errorProc, NULL), filename, line);

        /* Note: if we didn't have a filename for this level,
         * don't clear the addStackTrace flag
         * so we can pick it up at the next level
         */
        if (*filename) {
            interp->addStackTrace = 0;
        }

        Jim_DecrRefCount(interp, interp->errorProc);
        interp->errorProc = interp->emptyObj;
        Jim_IncrRefCount(interp->errorProc);
    }
    else if (rc == JIM_RETURN && interp->returnCode == JIM_ERR) {
        /* Propagate the addStackTrace value through 'return -code error' */
    }
    else {
        interp->addStackTrace = 0;
    }
}

int Jim_EvalObj(Jim_Interp *interp, Jim_Obj *scriptObjPtr)
{
    int i, j = 0, len;
    ScriptObj *script;
    ScriptToken *token;
    ScriptToken *cmdtoken = NULL;
    int *cs; /* command structure array */
    int retcode = JIM_OK;
    Jim_Obj *sargv[JIM_EVAL_SARGV_LEN], **argv = NULL, *tmpObjPtr;

    interp->errorFlag = 0;

    /* If the object is of type "list" and there is no
     * string representation for this object, we can call
     * a specialized version of Jim_EvalObj() */
    if (Jim_IsList(scriptObjPtr) && scriptObjPtr->internalRep.listValue.len) {
        Jim_IncrRefCount(scriptObjPtr);
        retcode = Jim_EvalObjVector(interp,
                scriptObjPtr->internalRep.listValue.len,
                scriptObjPtr->internalRep.listValue.ele);
        Jim_DecrRefCount(interp, scriptObjPtr);
        return retcode;
    }

    Jim_IncrRefCount(scriptObjPtr); /* Make sure it's shared. */
    script = Jim_GetScript(interp, scriptObjPtr);

    /* Reset the interpreter result. This is useful to
     * return the emtpy result in the case of empty program. */
    Jim_SetEmptyResult(interp);

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
     if (script->len == 4 && script->token[0].type == JIM_TT_ESC && script->token[2].type == JIM_TT_ESC && script->token[2].objPtr->typePtr == &variableObjType) {
        if (Jim_CompareStringImmediate(interp, script->token[0].objPtr, "incr")) {
            Jim_Obj *objPtr = Jim_GetVariable(interp, script->token[2].objPtr, JIM_NONE);
            if (objPtr && !Jim_IsShared(objPtr) && objPtr->typePtr == &intObjType) {
                objPtr->internalRep.wideValue++;
                Jim_InvalidateStringRep(objPtr);
                Jim_DecrRefCount(interp, scriptObjPtr);
                Jim_SetResult(interp, objPtr);
                return JIM_OK;
            }
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

    token = script->token;
    len = script->len;
    cs = script->cmdStruct;
    i = 0; /* 'i' is the current token index. */

    /* Execute every command sequentially, returns on
     * error (i.e. if a command does not return JIM_OK) */
    while (i < len) {
        int expand = 0;
        int argc = *cs++; /* Get the number of arguments */
        Jim_Cmd *cmd;

        /* Set the expand flag if needed. */
        if (argc < 0) {
            expand++;
            argc = -argc;
        }
        /* Allocate the arguments vector */
        if (argc <= JIM_EVAL_SARGV_LEN)
            argv = sargv;
        else
            argv = Jim_Alloc(sizeof(Jim_Obj*)*argc);

        /* This is the command token. Remember it in the case of error */
        cmdtoken = &token[i];

        /* Populate the arguments objects. */
        for (j = 0; j < argc; j++) {
            int tokens = *cs++;

            /* tokens is negative if expansion is needed.
             * for this argument. */
            if (tokens < 0) {
                tokens = (-tokens)-1;
                i++;
            }
            if (tokens == 1) {
                /* Fast path if the token does not
                 * need interpolation */
                switch(token[i].type) {
                case JIM_TT_ESC:
                case JIM_TT_STR:
                    argv[j] = token[i].objPtr;
                    break;
                case JIM_TT_VAR:
                    tmpObjPtr = Jim_GetVariable(interp, token[i].objPtr,
                            JIM_ERRMSG);
                    if (!tmpObjPtr) {
                        retcode = JIM_ERR;
                        goto err;
                    }
                    argv[j] = tmpObjPtr;
                    break;
                case JIM_TT_DICTSUGAR:
                    tmpObjPtr = Jim_ExpandDictSugar(interp, token[i].objPtr);
                    if (!tmpObjPtr) {
                        retcode = JIM_ERR;
                        goto err;
                    }
                    argv[j] = tmpObjPtr;
                    break;
                case JIM_TT_CMD:
                    retcode = Jim_EvalObj(interp, token[i].objPtr);
                    if (retcode != JIM_OK) {
                        goto err;
                    }
                    argv[j] = Jim_GetResult(interp);
                    break;
                default:
                    Jim_Panic(interp,
                      "default token type reached "
                      "in Jim_EvalObj().");
                    break;
                }
                Jim_IncrRefCount(argv[j]);
                i += 2;
            } else {
                /* For interpolation we call a helper
                 * function to do the work for us. */
                if ((retcode = Jim_InterpolateTokens(interp,
                        token+i, tokens, &tmpObjPtr)) != JIM_OK)
                {
                    goto err;
                }
                argv[j] = tmpObjPtr;
                Jim_IncrRefCount(argv[j]);
                i += tokens+1;
            }
        }
        /* Handle {expand} expansion */
        if (expand) {
            int *ecs = cs - argc;
            int eargc = 0;
            Jim_Obj **eargv = NULL;

            for (j = 0; j < argc; j++) {
                Jim_ExpandArgument( interp, &eargv, &eargc,
                        ecs[j] < 0, argv[j]);
            }
            if (argv != sargv)
                Jim_Free(argv);
            argc = eargc;
            argv = eargv;
            j = argc;
            if (argc == 0) {
                /* Nothing to do with zero args. */
                Jim_Free(eargv);
                continue;
            }
        }
        /* Lookup the command to call */
        cmd = Jim_GetCommand(interp, argv[0], JIM_ERRMSG);
        if (cmd != NULL) {
            /* Call it -- Make sure result is an empty object. */
            Jim_SetEmptyResult(interp);
            if (cmd->cmdProc) {
                interp->cmdPrivData = cmd->privData;
                retcode = cmd->cmdProc(interp, argc, argv);
            } else {
                retcode = JimCallProcedure(interp, cmd, argc, argv);
            }
        } else {
            /* Call [unknown] */
            retcode = JimUnknown(interp, argc, argv);
        }
        if (interp->signal_level && interp->sigmask) {
            /* Check for a signal after each command */
            retcode = JIM_SIGNAL;
        }
        if (retcode != JIM_OK) {
            goto err;
        }
        /* Decrement the arguments count */
        for (j = 0; j < argc; j++) {
            Jim_DecrRefCount(interp, argv[j]);
        }

        if (argv != sargv) {
            Jim_Free(argv);
            argv = NULL;
        }
    }
    /* Note that we don't have to decrement inUse, because the
     * following code transfers our use of the reference again to
     * the script object. */
    j = 0; /* on normal termination, the argv array is already
          Jim_DecrRefCount-ed. */
err:
    JimAddErrorToStack(interp, retcode, script->fileName, cmdtoken ? cmdtoken->linenr : 0);
    Jim_FreeIntRep(interp, scriptObjPtr);
    scriptObjPtr->typePtr = &scriptObjType;
    Jim_SetIntRepPtr(scriptObjPtr, script);
    Jim_DecrRefCount(interp, scriptObjPtr);
    for (i = 0; i < j; i++) {
        Jim_DecrRefCount(interp, argv[i]);
    }
    if (argv != sargv)
        Jim_Free(argv);
    return retcode;
}

/* Call a procedure implemented in Tcl.
 * It's possible to speed-up a lot this function, currently
 * the callframes are not cached, but allocated and
 * destroied every time. What is expecially costly is
 * to create/destroy the local vars hash table every time.
 *
 * This can be fixed just implementing callframes caching
 * in JimCreateCallFrame() and JimFreeCallFrame(). */
int JimCallProcedure(Jim_Interp *interp, Jim_Cmd *cmd, int argc,
        Jim_Obj *const *argv)
{
    int i, d, retcode;
    Jim_CallFrame *callFramePtr;
    Jim_Obj *argObjPtr;
    Jim_Obj *procname = argv[0];

    /* Check arity */
    if (argc - 1 < cmd->leftArity + cmd->rightArity ||
        (!cmd->args && argc - 1 > cmd->leftArity + cmd->rightArity + cmd->optionalArgs)) {
        /* Create a nice error message, consistent with Tcl 8.5 */
        Jim_Obj *argmsg = Jim_NewStringObj(interp, "", 0);
        int arglen = Jim_ListLength(interp, cmd->argListObjPtr);
        for (i = 0; i < arglen; i++) {
            Jim_Obj *argObjPtr;
            Jim_ListIndex(interp, cmd->argListObjPtr, i, &argObjPtr, JIM_NONE);

            Jim_AppendString(interp, argmsg, " ", 1);

            if (i < cmd->leftArity || i >= arglen - cmd->rightArity) {
                Jim_AppendObj(interp, argmsg, argObjPtr);
            }
            else if (i == arglen - cmd->rightArity - cmd->args) {
                Jim_AppendString(interp, argmsg, "?argument ...?", -1);
            }
            else {
                Jim_Obj *objPtr;
                Jim_AppendString(interp, argmsg, "?", 1);
                Jim_ListIndex(interp, argObjPtr, 0, &objPtr, JIM_NONE);
                Jim_AppendObj(interp, argmsg, objPtr);
                Jim_AppendString(interp, argmsg, "?", 1);
            }
        }
        Jim_SetResultFormatted(interp, "wrong # args: should be \"%#s%#s\"", procname, argmsg);
        Jim_FreeNewObj(interp, argmsg);
        return JIM_ERR;
    }

    /* Check if there are too nested calls */
    if (interp->numLevels == interp->maxNestingDepth) {
        Jim_SetResultString(interp,
            "Too many nested calls. Infinite recursion?", -1);
        return JIM_ERR;
    }

    /* Create a new callframe */
    callFramePtr = JimCreateCallFrame(interp);
    callFramePtr->parentCallFrame = interp->framePtr;
    callFramePtr->argv = argv;
    callFramePtr->argc = argc;
    callFramePtr->procArgsObjPtr = cmd->argListObjPtr;
    callFramePtr->procBodyObjPtr = cmd->bodyObjPtr;
    callFramePtr->staticVars = cmd->staticVars;
    Jim_IncrRefCount(cmd->argListObjPtr);
    Jim_IncrRefCount(cmd->bodyObjPtr);
    interp->framePtr = callFramePtr;
    interp->numLevels ++;

    /* Simplify arg counting */
    argv++;
    argc--;

    /* Set arguments */

    /* Assign in this order:
     * leftArity required args.
     * rightArity required args (but actually do it last for simplicity)
     * optionalArgs optional args
     * remaining args into 'args' if 'args'
     */

    /* Note that 'd' steps along the arg list, whilst argc/argv follow the supplied args */

    /* leftArity required args */
    for (d = 0; d < cmd->leftArity; d++) {
        Jim_ListIndex(interp, cmd->argListObjPtr, d, &argObjPtr, JIM_NONE);
        Jim_SetVariable(interp, argObjPtr, *argv++);
        argc--;
    }
    
    /* Shorten our idea of the number of supplied args */
    argc -= cmd->rightArity;

    /* optionalArgs optional args */
    for (i = 0; i < cmd->optionalArgs; i++) {
        Jim_Obj *nameObjPtr;
        Jim_Obj *valueObjPtr;

        Jim_ListIndex(interp, cmd->argListObjPtr, d++, &argObjPtr, JIM_NONE);

        /* The name is the first element of the list */
        Jim_ListIndex(interp, argObjPtr, 0, &nameObjPtr, JIM_NONE);
        if (argc) {
            valueObjPtr = *argv++;
            argc--;
        }
        else {
            /* No more values, so use default */
            /* The value is the second element of the list */
            Jim_ListIndex(interp, argObjPtr, 1, &valueObjPtr, JIM_NONE);
        }
        Jim_SetVariable(interp, nameObjPtr, valueObjPtr);
    }

    /* Any remaining args go to 'args' */
    if (cmd->args) {
        Jim_Obj *listObjPtr = Jim_NewListObj(interp, argv, argc);
        /* Use the 'args' name from the procedure args */
        Jim_ListIndex(interp, cmd->argListObjPtr, d, &argObjPtr, JIM_NONE);
        Jim_SetVariable(interp, argObjPtr, listObjPtr);
        argv += argc;
        d++;
    }

    /* rightArity required args */
    for (i = 0; i < cmd->rightArity; i++) {
        Jim_ListIndex(interp, cmd->argListObjPtr, d++, &argObjPtr, JIM_NONE);
        Jim_SetVariable(interp, argObjPtr, *argv++);
    }

    /* Eval the body */
    retcode = Jim_EvalObj(interp, cmd->bodyObjPtr);

    /* Destroy the callframe */
    interp->numLevels --;
    interp->framePtr = interp->framePtr->parentCallFrame;
    if (callFramePtr->vars.size != JIM_HT_INITIAL_SIZE) {
        JimFreeCallFrame(interp, callFramePtr, JIM_FCF_NONE);
    } else {
        JimFreeCallFrame(interp, callFramePtr, JIM_FCF_NOHT);
    }
    /* Handle the JIM_EVAL return code */
    if (retcode == JIM_EVAL && interp->evalRetcodeLevel != interp->numLevels) {
        int savedLevel = interp->evalRetcodeLevel;

        interp->evalRetcodeLevel = interp->numLevels;
        while (retcode == JIM_EVAL) {
            Jim_Obj *resultScriptObjPtr = Jim_GetResult(interp);
            Jim_IncrRefCount(resultScriptObjPtr);
            retcode = Jim_EvalObj(interp, resultScriptObjPtr);
            Jim_DecrRefCount(interp, resultScriptObjPtr);
        }
        interp->evalRetcodeLevel = savedLevel;
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
        interp->errorProc = procname;
        Jim_IncrRefCount(interp->errorProc);
    }
    return retcode;
}

int Jim_Eval_Named(Jim_Interp *interp, const char *script, const char *filename, int lineno)
{
    int retval;
    Jim_Obj *scriptObjPtr;

    scriptObjPtr = Jim_NewStringObj(interp, script, -1);
    Jim_IncrRefCount(scriptObjPtr);


    if (filename) {
        Jim_Obj *prevScriptObj;

        JimSetSourceInfo(interp, scriptObjPtr, filename, lineno);

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
    return Jim_Eval_Named( interp, script, NULL, 0 );
}

/* Execute script in the scope of the global level */
int Jim_EvalGlobal(Jim_Interp *interp, const char *script)
{
    Jim_CallFrame *savedFramePtr;
    int retval;

    savedFramePtr = interp->framePtr;
    interp->framePtr = interp->topFramePtr;
    retval = Jim_Eval(interp, script);
    interp->framePtr = savedFramePtr;
    return retval;
}

int Jim_EvalObjBackground(Jim_Interp *interp, Jim_Obj *scriptObjPtr)
{
    Jim_CallFrame *savedFramePtr;
    int retval;

    savedFramePtr = interp->framePtr;
    interp->framePtr = interp->topFramePtr;
    retval = Jim_EvalObj(interp, scriptObjPtr);
    interp->framePtr = savedFramePtr;
    /* Try to report the error (if any) via the bgerror proc */
    if (retval != JIM_OK) {
        Jim_Obj *objv[2];

        objv[0] = Jim_NewStringObj(interp, "bgerror", -1);
        objv[1] = Jim_GetResult(interp);
        Jim_IncrRefCount(objv[0]);
        Jim_IncrRefCount(objv[1]);
        if (Jim_EvalObjVector(interp, 2, objv) != JIM_OK) {
            /* Report the error to stderr. */
            fprintf(stderr, "Background error:" JIM_NL);
            Jim_PrintErrorMessage(interp);
        }
        Jim_DecrRefCount(interp, objv[0]);
        Jim_DecrRefCount(interp, objv[1]);
    }
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

    if (stat(filename, &sb) != 0 || (fp = fopen(filename, "r")) == NULL) {
        Jim_SetResultFormatted(interp, "couldn't read file \"%s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    if (sb.st_size == 0) {
        fclose(fp);
        return JIM_OK;
    }

    buf = Jim_Alloc(sb.st_size + 1);
    readlen = fread(buf, sb.st_size, 1, fp);
    fclose(fp);
    if (readlen != 1) {
        Jim_Free(buf);
        return JIM_ERR;
    }
    buf[sb.st_size] = 0;

    scriptObjPtr = Jim_NewStringObjNoAlloc(interp, buf, sb.st_size);
    JimSetSourceInfo(interp, scriptObjPtr, filename, 1);
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
static int JimParseSubstStr(struct JimParserCtx *pc)
{
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (*pc->p && *pc->p != '$' && *pc->p != '[') {
        if (*pc->p == '\\' && pc->len > 1) {
            pc->p++; pc->len--;
        }
        pc->p++; pc->len--;
    }
    pc->tend = pc->p-1;
    pc->tt = JIM_TT_ESC;
    return JIM_OK;
}

static int JimParseSubst(struct JimParserCtx *pc, int flags)
{
    int retval;

    if (pc->len == 0) {
        pc->tstart = pc->tend = pc->p;
        pc->tline = pc->linenr;
        pc->tt = JIM_TT_EOL;
        pc->eof = 1;
        return JIM_OK;
    }
    switch(*pc->p) {
    case '[':
        retval = JimParseCmd(pc);
        if (flags & JIM_SUBST_NOCMD) {
            pc->tstart--;
            pc->tend++;
            pc->tt = (flags & JIM_SUBST_NOESC) ?
                JIM_TT_STR : JIM_TT_ESC;
        }
        return retval;
        break;
    case '$':
        if (JimParseVar(pc) == JIM_ERR) {
            pc->tstart = pc->tend = pc->p++; pc->len--;
            pc->tline = pc->linenr;
            pc->tt = JIM_TT_STR;
        } else {
            if (flags & JIM_SUBST_NOVAR) {
                pc->tstart--;
                if (flags & JIM_SUBST_NOESC)
                    pc->tt = JIM_TT_STR;
                else
                    pc->tt = JIM_TT_ESC;
                if (*pc->tstart == '{') {
                    pc->tstart--;
                    if (*(pc->tend+1))
                        pc->tend++;
                }
            }
        }
        break;
    default:
        retval = JimParseSubstStr(pc);
        if (flags & JIM_SUBST_NOESC)
            pc->tt = JIM_TT_STR;
        return retval;
        break;
    }
    return JIM_OK;
}

/* The subst object type reuses most of the data structures and functions
 * of the script object. Script's data structures are a bit more complex
 * for what is needed for [subst]itution tasks, but the reuse helps to
 * deal with a single data structure at the cost of some more memory
 * usage for substitutions. */
static const Jim_ObjType substObjType = {
    "subst",
    FreeScriptInternalRep,
    DupScriptInternalRep,
    NULL,
    JIM_TYPE_REFERENCES,
};

/* This method takes the string representation of an object
 * as a Tcl string where to perform [subst]itution, and generates
 * the pre-parsed internal representation. */
int SetSubstFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr, int flags)
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
        if (JimParserEof(&parser)) {
            /* Note that subst doesn't need the EOL token */
            break;
        }
        ScriptAddToken(&tokenlist, parser.tstart, parser.tend - parser.tstart + 1, parser.tt, parser.tline);
    }

    /* Create the "real" subst/script tokens from the initial token list */
    script->cmdStruct = NULL;
    script->csLen = 0;
    script->inUse = 1;
    script->substFlags = flags;
    script->fileName = NULL;
#ifdef JIM_OPTIMIZATION
    SubstObjAddTokens(interp, script, &tokenlist);
#else
    ScriptObjAddTokens(interp, script, &tokenlist);
#endif

    /* No longer need the token list */
    ScriptTokenListFree(&tokenlist);

#if 0
    int i;
    printf("==== Subst ====\n");
    for (i = 0; i < script->len; i++) {
        printf("[%2d] %s (%d)'%s'\n", i, tt_name(script->token[i].type), script->token[i].objPtr->length, script->token[i].objPtr->bytes);
    }
#endif

    /* Free the old internal rep and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    Jim_SetIntRepPtr(objPtr, script);
    objPtr->typePtr = &scriptObjType;
    return JIM_OK;
}

ScriptObj *Jim_GetSubst(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    struct ScriptObj *script = Jim_GetIntRepPtr(objPtr);

    if (objPtr->typePtr != &substObjType || script->substFlags != flags)
        SetSubstFromAny(interp, objPtr, flags);
    return (ScriptObj*) Jim_GetIntRepPtr(objPtr);
}

/* Performs commands,variables,blackslashes substitution,
 * storing the result object (with refcount 0) into
 * resObjPtrPtr. */
int Jim_SubstObj(Jim_Interp *interp, Jim_Obj *substObjPtr,
        Jim_Obj **resObjPtrPtr, int flags)
{
    ScriptObj *script;
    ScriptToken *token;
    int i, len, retcode = JIM_OK;
    int rc;
    Jim_Obj *resObjPtr, *savedResultObjPtr;

    script = Jim_GetSubst(interp, substObjPtr, flags);
#ifdef JIM_OPTIMIZATION
    /* Fast path for a very common case with array-alike syntax,
     * that's: $foo($bar) */
    if (script->len == 1 && script->token[0].type == JIM_TT_VAR) {
        Jim_Obj *varObjPtr = script->token[0].objPtr;

        Jim_IncrRefCount(varObjPtr);
        resObjPtr = Jim_GetVariable(interp, varObjPtr, JIM_ERRMSG);
        if (resObjPtr == NULL) {
            Jim_DecrRefCount(interp, varObjPtr);
            return JIM_ERR;
        }
        Jim_DecrRefCount(interp, varObjPtr);
        *resObjPtrPtr = resObjPtr;
        return JIM_OK;
    }
#endif

    Jim_IncrRefCount(substObjPtr); /* Make sure it's shared. */
    /* In order to preserve the internal rep, we increment the
     * inUse field of the script internal rep structure. */
    script->inUse++;

    token = script->token;
    len = script->len;

    /* Save the interp old result, to set it again before
     * to return. */
    savedResultObjPtr = interp->result;
    Jim_IncrRefCount(savedResultObjPtr);
    
    /* Perform the substitution. Starts with an empty object
     * and adds every token (performing the appropriate
     * var/command/escape substitution). */
    resObjPtr = Jim_NewStringObj(interp, "", 0);
    for (i = 0; i < len; i++) {
        Jim_Obj *objPtr;

        switch(token[i].type) {
        case JIM_TT_STR:
        case JIM_TT_ESC:
            Jim_AppendObj(interp, resObjPtr, token[i].objPtr);
            break;
        case JIM_TT_VAR:
        case JIM_TT_DICTSUGAR:
            if (token[i].type == JIM_TT_VAR) {
                objPtr = Jim_GetVariable(interp, token[i].objPtr, JIM_ERRMSG);
            }
            else {
                objPtr = Jim_ExpandDictSugar(interp, token[i].objPtr);
            }
            if (objPtr == NULL) goto err;
            Jim_IncrRefCount(objPtr);
            Jim_AppendObj(interp, resObjPtr, objPtr);
            Jim_DecrRefCount(interp, objPtr);
            break;
        case JIM_TT_CMD:
            rc = Jim_EvalObj(interp, token[i].objPtr);
            if (rc == JIM_BREAK) {
                /* Stop substituting */
                goto ok;
            }
            else if (rc == JIM_CONTINUE) {
                /* just skip this one */
            }
            else if (rc == JIM_OK || rc == JIM_RETURN) {
                Jim_AppendObj(interp, resObjPtr, interp->result);
            }
            else {
                goto err;
            }
            break;
        default:
            Jim_Panic(interp,
              "default token type (%d) reached "
              "in Jim_SubstObj().", token[i].type);
            break;
        }
    }
ok:
    if (retcode == JIM_OK)
        Jim_SetResult(interp, savedResultObjPtr);
    Jim_DecrRefCount(interp, savedResultObjPtr);
    /* Note that we don't have to decrement inUse, because the
     * following code transfers our use of the reference again to
     * the script object. */
    Jim_FreeIntRep(interp, substObjPtr);
    substObjPtr->typePtr = &scriptObjType;
    Jim_SetIntRepPtr(substObjPtr, script);
    Jim_DecrRefCount(interp, substObjPtr);
    *resObjPtrPtr = resObjPtr;
    return retcode;
err:
    Jim_FreeNewObj(interp, resObjPtr);
    retcode = JIM_ERR;
    goto ok;
}

/* -----------------------------------------------------------------------------
 * Core commands utility functions
 * ---------------------------------------------------------------------------*/
void Jim_WrongNumArgs(Jim_Interp *interp, int argc, Jim_Obj *const *argv, 
        const char *msg)
{
    int i;
    Jim_Obj *objPtr = Jim_NewEmptyStringObj(interp);

    Jim_AppendString(interp, objPtr, "wrong # args: should be \"", -1);
    for (i = 0; i < argc; i++) {
        Jim_AppendObj(interp, objPtr, argv[i]);
        if (!(i+1 == argc && msg[0] == '\0'))
            Jim_AppendString(interp, objPtr, " ", 1);
    }
    Jim_AppendString(interp, objPtr, msg, -1);
    Jim_AppendString(interp, objPtr, "\"", 1);
    Jim_SetResult(interp, objPtr);
}

static Jim_Obj *JimCommandsList(Jim_Interp *interp, Jim_Obj *patternObjPtr, int procs_only)
{
    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;
    Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);
    const char *pattern;
    int patternLen = 0;
    
    pattern = patternObjPtr ? Jim_GetString(patternObjPtr, &patternLen) : NULL;
    htiter = Jim_GetHashTableIterator(&interp->commands);
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        Jim_Cmd *cmdPtr = he->val;
        if (procs_only && cmdPtr->cmdProc != NULL) {
            continue;
        }
        if (pattern && !JimStringMatch(pattern, patternLen, he->key, 
                    strlen((const char*)he->key), 0))
            continue;
        Jim_ListAppendElement(interp, listObjPtr,
                Jim_NewStringObj(interp, he->key, -1));
    }
    Jim_FreeHashTableIterator(htiter);
    return listObjPtr;
}

/* Keep this in order */
#define JIM_VARLIST_GLOBALS 0
#define JIM_VARLIST_LOCALS 1
#define JIM_VARLIST_VARS 2

static Jim_Obj *JimVariablesList(Jim_Interp *interp, Jim_Obj *patternObjPtr,
        int mode)
{
    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;
    Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);
    const char *pattern;
    int patternLen = 0;
    
    pattern = patternObjPtr ? Jim_GetString(patternObjPtr, &patternLen) : NULL;
    if (mode == JIM_VARLIST_GLOBALS) {
        htiter = Jim_GetHashTableIterator(&interp->topFramePtr->vars);
    } else {
        /* For [info locals], if we are at top level an emtpy list
         * is returned. I don't agree, but we aim at compatibility (SS) */
        if (mode == JIM_VARLIST_LOCALS &&
            interp->framePtr == interp->topFramePtr)
            return listObjPtr;
        htiter = Jim_GetHashTableIterator(&interp->framePtr->vars);
    }
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        Jim_Var *varPtr = (Jim_Var*) he->val;
        if (mode == JIM_VARLIST_LOCALS) {
            if (varPtr->linkFramePtr != NULL)
                continue;
        }
        if (pattern && !JimStringMatch(pattern, patternLen, he->key, 
                    strlen((const char*)he->key), 0))
            continue;
        Jim_ListAppendElement(interp, listObjPtr,
                Jim_NewStringObj(interp, he->key, -1));
    }
    Jim_FreeHashTableIterator(htiter);
    return listObjPtr;
}

static int JimInfoLevel(Jim_Interp *interp, Jim_Obj *levelObjPtr,
        Jim_Obj **objPtrPtr)
{
    Jim_CallFrame *targetCallFrame;

    if (JimGetCallFrameByInteger(interp, levelObjPtr, &targetCallFrame)
            != JIM_OK)
        return JIM_ERR;
    /* No proc call at toplevel callframe */
    if (targetCallFrame == interp->topFramePtr) {
        Jim_SetResultFormatted(interp, "bad level \"%#s\"", levelObjPtr);
        return JIM_ERR;
    }
    *objPtrPtr = Jim_NewListObj(interp,
            targetCallFrame->argv,
            targetCallFrame->argc);
    return JIM_OK;
}

/* -----------------------------------------------------------------------------
 * Core commands
 * ---------------------------------------------------------------------------*/

/* fake [puts] -- not the real puts, just for debugging. */
static int Jim_PutsCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    const char *str;
    int nonewline = 0;
    
    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "-nonewline string");
        return JIM_ERR;
    }
    if (argc == 3) {
        if (!Jim_CompareStringImmediate(interp, argv[1], "-nonewline"))
        {
            Jim_SetResultString(interp, "The second argument must "
                    "be -nonewline", -1);
            return JIM_OK;
        } else {
            nonewline = 1;
            argv++;
        }
    }
    str = Jim_GetString(argv[1], 0);
    printf("%s%s", str, nonewline ? "" : "\n");
    return JIM_OK;
}

/* Helper for [+] and [*] */
static int Jim_AddMulHelper(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv, int op)
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
    doubleRes = (double) res;
    for (;i < argc; i++) {
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
static int Jim_SubDivHelper(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv, int op)
{
    jim_wide wideValue, res = 0;
    double doubleValue, doubleRes = 0;
    int i = 2;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "number ?number ... number?");
        return JIM_ERR;
    } else if (argc == 2) {
        /* The arity = 2 case is different. For [- x] returns -x,
         * while [/ x] returns 1/x. */
        if (Jim_GetWide(interp, argv[1], &wideValue) != JIM_OK) {
            if (Jim_GetDouble(interp, argv[1], &doubleValue) !=
                    JIM_OK)
            {
                return JIM_ERR;
            } else {
                if (op == JIM_EXPROP_SUB)
                    doubleRes = -doubleValue;
                else
                    doubleRes = 1.0/doubleValue;
                Jim_SetResult(interp, Jim_NewDoubleObj(interp,
                            doubleRes));
                return JIM_OK;
            }
        }
        if (op == JIM_EXPROP_SUB) {
            res = -wideValue;
            Jim_SetResultInt(interp, res);
        } else {
            doubleRes = 1.0/wideValue;
            Jim_SetResult(interp, Jim_NewDoubleObj(interp,
                        doubleRes));
        }
        return JIM_OK;
    } else {
        if (Jim_GetWide(interp, argv[1], &res) != JIM_OK) {
            if (Jim_GetDouble(interp, argv[1], &doubleRes)
                    != JIM_OK) {
                return JIM_ERR;
            } else {
                goto trydouble;
            }
        }
    }
    for (i = 2; i < argc; i++) {
        if (Jim_GetWide(interp, argv[i], &wideValue) != JIM_OK) {
            doubleRes = (double) res;
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
    for (;i < argc; i++) {
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
static int Jim_AddCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    return Jim_AddMulHelper(interp, argc, argv, JIM_EXPROP_ADD);
}

/* [*] */
static int Jim_MulCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    return Jim_AddMulHelper(interp, argc, argv, JIM_EXPROP_MUL);
}

/* [-] */
static int Jim_SubCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    return Jim_SubDivHelper(interp, argc, argv, JIM_EXPROP_SUB);
}

/* [/] */
static int Jim_DivCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    return Jim_SubDivHelper(interp, argc, argv, JIM_EXPROP_DIV);
}

/* [set] */
static int Jim_SetCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
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
static int Jim_UnsetCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
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
        if (Jim_UnsetVariable(interp, argv[i], complain ? JIM_ERRMSG : JIM_NONE) != JIM_OK && complain) {
            return JIM_ERR;
        }
        i++;
    }
    return JIM_OK;
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
    intObjPtr = Jim_GetVariable(interp, argv[1], JIM_NONE);
    if (!intObjPtr) {
        /* Set missing variable to 0 */
        wideValue = 0;
    }
    else if (Jim_GetWide(interp, intObjPtr, &wideValue) != JIM_OK) {
        return JIM_ERR;
    }
    if (!intObjPtr || Jim_IsShared(intObjPtr)) {
        intObjPtr = Jim_NewIntObj(interp, wideValue+increment);
        if (Jim_SetVariable(interp, argv[1], intObjPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, intObjPtr);
            return JIM_ERR;
        }
    } else {
        Jim_SetWide(interp, intObjPtr, wideValue+increment);
        /* The following step is required in order to invalidate the
         * string repr of "FOO" if the var name is on the form of "FOO(IDX)" */
        if (argv[1]->typePtr != &variableObjType) {
            if (Jim_SetVariable(interp, argv[1], intObjPtr) != JIM_OK) {
                return JIM_ERR;
            }
        }
    }
    Jim_SetResult(interp, intObjPtr);
    return JIM_OK;
}

/* [while] */
static int Jim_WhileCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "condition body");
        return JIM_ERR;
    }

    /* The general purpose implementation of while starts here */
    while (1) {
        int boolean, retval;

        if ((retval = Jim_GetBoolFromExpr(interp, argv[1],
                        &boolean)) != JIM_OK)
            return retval;
        if (!boolean) break;

        if ((retval = Jim_EvalObj(interp, argv[2])) != JIM_OK) {
            switch(retval) {
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
static int Jim_ForCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
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
        unsigned jim_wide procEpoch;
        Jim_Obj *objPtr;
        int cmpOffset;

        /* Do it only if there aren't shared arguments */
        expr = Jim_GetExpression(interp, argv[2]);
        incrScript = Jim_GetScript(interp, argv[3]);

        /* Ensure proper lengths to start */
        if (incrScript->len != 4 || !expr || expr->len != 3) {
            goto evalstart;
        }
        /* Ensure proper token types. */
        if (incrScript->token[2].type != JIM_TT_ESC ||
            expr->token[0].type != JIM_TT_VAR ||
            (expr->token[1].type != JIM_TT_EXPR_INT &&
             expr->token[1].type != JIM_TT_VAR)) {
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
        if (!Jim_CompareStringImmediate(interp, incrScript->token[0].objPtr, "incr")) {
            goto evalstart;
        }

        /* incr, expression must be about the same variable */
        if (!Jim_StringEqObj(incrScript->token[2].objPtr, expr->token[0].objPtr, 0)) {
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
        procEpoch = interp->procEpoch;
        varNamePtr = expr->token[0].objPtr;
        Jim_IncrRefCount(varNamePtr);

        objPtr = Jim_GetVariable(interp, varNamePtr, JIM_NONE);
        if (objPtr == NULL || Jim_GetWide(interp, objPtr, &currentVal) != JIM_OK)
        {
            goto testcond;
        }

        /* --- OPTIMIZED FOR --- */
        while (retval == JIM_OK) {
            /* === Check condition === */
            /* Note that currentVal is already set here */

            /* Immediate or Variable? get the 'stop' value if the latter. */
            if (stopVarNamePtr) {
                objPtr = Jim_GetVariable(interp, stopVarNamePtr, JIM_NONE);
                if (objPtr == NULL || Jim_GetWide(interp, objPtr, &stop) != JIM_OK)
                {
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
                /* If there was a change in procedures/command continue
                 * with the usual [for] command implementation */
                if (procEpoch != interp->procEpoch) {
                    goto evalnext;
                }

                objPtr = Jim_GetVariable(interp, varNamePtr, JIM_NONE);

                /* Increment */
                if (!Jim_IsShared(objPtr) && objPtr->typePtr == &intObjType) {
                    currentVal = ++objPtr->internalRep.wideValue;
                    Jim_InvalidateStringRep(objPtr);
                } else {
                    if (Jim_GetWide(interp, objPtr, &currentVal) != JIM_OK ||
                        Jim_SetVariable(interp, varNamePtr, Jim_NewIntObj(interp, ++currentVal)) != JIM_OK) {
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

/* foreach + lmap implementation. */
static int JimForeachMapHelper(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv, int doMap)
{
    int result = JIM_ERR, i, nbrOfLists, *listsIdx, *listsEnd;
    int nbrOfLoops = 0;
    Jim_Obj *emptyStr, *script, *mapRes = NULL;

    if (argc < 4 || argc % 2 != 0) {
        Jim_WrongNumArgs(interp, 1, argv, "varList list ?varList list ...? script");
        return JIM_ERR;
    }
    if (doMap) {
        mapRes = Jim_NewListObj(interp, NULL, 0);
        Jim_IncrRefCount(mapRes);
    }
    emptyStr = Jim_NewEmptyStringObj(interp);
    Jim_IncrRefCount(emptyStr);
    script = argv[argc-1];            /* Last argument is a script */
    nbrOfLists = (argc - 1 - 1) / 2;  /* argc - 'foreach' - script */
    listsIdx = (int*)Jim_Alloc(nbrOfLists * sizeof(int));
    listsEnd = (int*)Jim_Alloc(nbrOfLists*2 * sizeof(int));
    /* Initialize iterators and remember max nbr elements each list */
    memset(listsIdx, 0, nbrOfLists * sizeof(int));
    /* Remember lengths of all lists and calculate how much rounds to loop */
    for (i=0; i < nbrOfLists*2; i += 2) {
        div_t cnt;
        int count;
        listsEnd[i] = Jim_ListLength(interp, argv[i+1]);
        listsEnd[i + 1] = Jim_ListLength(interp, argv[i+2]);
        if (listsEnd[i] == 0) {
            Jim_SetResultString(interp, "foreach varlist is empty", -1);
            goto err;
        }
        cnt = div(listsEnd[i+1], listsEnd[i]);
        count = cnt.quot + (cnt.rem ? 1 : 0);
        if (count > nbrOfLoops)
            nbrOfLoops = count;
    }
    for (; nbrOfLoops-- > 0; ) {
        for (i=0; i < nbrOfLists; ++i) {
            int varIdx = 0, var = i * 2;
            while (varIdx < listsEnd[var]) {
                Jim_Obj *varName, *ele;
                int lst = i * 2 + 1;
                if (Jim_ListIndex(interp, argv[var+1], varIdx, &varName, JIM_ERRMSG)
                        != JIM_OK)
                        goto err;
                if (listsIdx[i] < listsEnd[lst]) {
                    if (Jim_ListIndex(interp, argv[lst+1], listsIdx[i], &ele, JIM_ERRMSG)
                        != JIM_OK)
                        goto err;
                    /* Avoid shimmering */
                    Jim_IncrRefCount(ele);
                    result = Jim_SetVariable(interp, varName, ele);
                    Jim_DecrRefCount(interp, ele);
                    if (result == JIM_OK) {
                        ++listsIdx[i];  /* Remember next iterator of current list */ 
                        ++varIdx;  /* Next variable */
                        continue;
                    }
                }
                else if (Jim_SetVariable(interp, varName, emptyStr) == JIM_OK) {
                    ++varIdx;  /* Next variable */
                    continue;
                }
                goto err;
            }
        }
        switch (result = Jim_EvalObj(interp, script)) {
            case JIM_OK:
                if (doMap)
                    Jim_ListAppendElement(interp, mapRes, interp->result);
                break;
            case JIM_CONTINUE:
                break;
            case JIM_BREAK:
                goto out;
                break;
            default:
                goto err;
        }
    }
out:
    result = JIM_OK;
    if (doMap)
        Jim_SetResult(interp, mapRes);
    else
        Jim_SetEmptyResult(interp);
err:
    if (doMap)
        Jim_DecrRefCount(interp, mapRes);
    Jim_DecrRefCount(interp, emptyStr);
    Jim_Free(listsIdx);
    Jim_Free(listsEnd);
    return result;
}

/* [foreach] */
static int Jim_ForeachCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    return JimForeachMapHelper(interp, argc, argv, 0);
}

/* [lmap] */
static int Jim_LmapCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    return JimForeachMapHelper(interp, argc, argv, 1);
}

/* [if] */
static int Jim_IfCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    int boolean, retval, current = 1, falsebody = 0;
    if (argc >= 3) {
        while (1) {
            /* Far not enough arguments given! */
            if (current >= argc) goto err;
            if ((retval = Jim_GetBoolFromExpr(interp,
                        argv[current++], &boolean))
                    != JIM_OK)
                return retval;
            /* There lacks something, isn't it? */
            if (current >= argc) goto err;
            if (Jim_CompareStringImmediate(interp, argv[current],
                        "then")) current++;
            /* Tsk tsk, no then-clause? */
            if (current >= argc) goto err;
            if (boolean)
                return Jim_EvalObj(interp, argv[current]);
             /* Ok: no else-clause follows */
            if (++current >= argc) {
                Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
                return JIM_OK;
            }
            falsebody = current++;
            if (Jim_CompareStringImmediate(interp, argv[falsebody],
                        "else")) {
                /* IIICKS - else-clause isn't last cmd? */
                if (current != argc-1) goto err;
                return Jim_EvalObj(interp, argv[current]);
            } else if (Jim_CompareStringImmediate(interp,
                        argv[falsebody], "elseif"))
                /* Ok: elseif follows meaning all the stuff
                 * again (how boring...) */
                continue;
            /* OOPS - else-clause is not last cmd?*/
            else if (falsebody != argc-1)
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
int Jim_CommandMatchObj(Jim_Interp *interp, Jim_Obj *commandObj, Jim_Obj *patternObj, Jim_Obj *stringObj, int nocase)
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

enum {SWITCH_EXACT, SWITCH_GLOB, SWITCH_RE, SWITCH_CMD };

/* [switch] */
static int Jim_SwitchCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    int matchOpt = SWITCH_EXACT, opt=1, patCount, i;
    Jim_Obj *command = 0, *const *caseList = 0, *strObj;
    Jim_Obj *script = 0;
    if (argc < 3) {
wrongnumargs:
        Jim_WrongNumArgs(interp, 1, argv, "?options? string "
            "pattern body ... ?default body?   or   "
            "{pattern body ?pattern body ...?}");
        return JIM_ERR;
    }
    for (opt=1; opt < argc; ++opt) {
        const char *option = Jim_GetString(argv[opt], 0);
        if (*option != '-') break;
        else if (strncmp(option, "--", 2) == 0) { ++opt; break; }
        else if (strncmp(option, "-exact", 2) == 0) matchOpt = SWITCH_EXACT;
        else if (strncmp(option, "-glob", 2) == 0) matchOpt = SWITCH_GLOB;
        else if (strncmp(option, "-regexp", 2) == 0) matchOpt = SWITCH_RE;
        else if (strncmp(option, "-command", 2) == 0) { matchOpt = SWITCH_CMD;
            if ((argc - opt) < 2) goto wrongnumargs;
            command = argv[++opt]; 
        } else {
            Jim_SetResultFormatted(interp, "bad option \"%#s\": must be -exact, -glob, -regexp, -command procname or --", argv[opt]);
            return JIM_ERR;
        }
        if ((argc - opt) < 2) goto wrongnumargs;
    }
    strObj = argv[opt++];
    patCount = argc - opt;
    if (patCount == 1) {
        Jim_Obj **vector;
        JimListGetElements(interp, argv[opt], &patCount, &vector);
        caseList = vector;
    } else
        caseList = &argv[opt];
    if (patCount == 0 || patCount % 2 != 0) goto wrongnumargs;
    for (i=0; script == 0 && i < patCount; i += 2) {
        Jim_Obj *patObj = caseList[i];
        if (!Jim_CompareStringImmediate(interp, patObj, "default")
            || i < (patCount-2)) {
            switch (matchOpt) {
                case SWITCH_EXACT:
                    if (Jim_StringEqObj(strObj, patObj, 0))
                        script = caseList[i+1];
                    break;
                case SWITCH_GLOB:
                    if (Jim_StringMatchObj(patObj, strObj, 0))
                        script = caseList[i+1];
                    break;
                case SWITCH_RE:
                    command = Jim_NewStringObj(interp, "regexp", -1);
                    /* Fall thru intentionally */
                case SWITCH_CMD: {
                    int rc = Jim_CommandMatchObj(interp, command, patObj, strObj, 0);
                    /* After the execution of a command we need to
                     * make sure to reconvert the object into a list
                     * again. Only for the single-list style [switch]. */
                    if (argc-opt == 1) {
                        Jim_Obj **vector;
                        JimListGetElements(interp, argv[opt], &patCount,
                                &vector);
                        caseList = vector;
                    }
                    /* command is here already decref'd */
                    if (rc < 0) {
                        return -rc;
                    }
                    if (rc)
                        script = caseList[i+1];
                    break;
                }
            }
        } else {
          script = caseList[i+1];
        }
    }
    for(; i < patCount && Jim_CompareStringImmediate(interp, script, "-");
        i += 2)
        script = caseList[i+1];
    if (script && Jim_CompareStringImmediate(interp, script, "-")) {
        Jim_SetResultFormatted(interp, "no body specified for pattern \"%#s\"", caseList[i-2]);
        return JIM_ERR;
    }
    Jim_SetEmptyResult(interp);
    if (script) {
        return Jim_EvalObj(interp, script);
    }
    return JIM_OK;
}

/* [list] */
static int Jim_ListCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    Jim_Obj *listObjPtr;

    listObjPtr = Jim_NewListObj(interp, argv+1, argc-1);
    Jim_SetResult(interp, listObjPtr);
    return JIM_OK;
}

/* [lindex] */
static int Jim_LindexCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    Jim_Obj *objPtr, *listObjPtr;
    int i;
    int index;

    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "list index ?...?");
        return JIM_ERR;
    }
    objPtr = argv[1];
    Jim_IncrRefCount(objPtr);
    for (i = 2; i < argc; i++) {
        listObjPtr = objPtr;
        if (Jim_GetIndex(interp, argv[i], &index) != JIM_OK) {
            Jim_DecrRefCount(interp, listObjPtr);
            return JIM_ERR;
        }
        if (Jim_ListIndex(interp, listObjPtr, index, &objPtr,
                    JIM_NONE) != JIM_OK) {
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
static int Jim_LlengthCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "list");
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, Jim_ListLength(interp, argv[1]));
    return JIM_OK;
}

/* [lsearch] */
static int Jim_LsearchCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    static const char *options[] = {
        "-bool", "-not", "-nocase", "-exact", "-glob", "-regexp", "-all", "-inline", "-command", NULL
    };
    enum {OPT_BOOL, OPT_NOT, OPT_NOCASE, OPT_EXACT, OPT_GLOB, OPT_REGEXP, OPT_ALL, OPT_INLINE, OPT_COMMAND };
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
        Jim_WrongNumArgs(interp, 1, argv, "?-exact|-glob|-regexp|-command 'command'? ?-bool|-inline? ?-not? ?-nocase? ?-all? list value");
        return JIM_ERR;
    }

    for (i = 1; i < argc - 2; i++) {
        int option;

        if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
        switch(option) {
            case OPT_BOOL: opt_bool = 1; opt_inline = 0; break;
            case OPT_NOT: opt_not = 1; break;
            case OPT_NOCASE: opt_nocase = 1; break;
            case OPT_INLINE: opt_inline = 1; opt_bool = 0; break;
            case OPT_ALL: opt_all = 1; break;
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
        Jim_Obj *objPtr;
        Jim_ListIndex(interp, argv[0], i, &objPtr, JIM_NONE);
        int eq = 0;
        switch (opt_match) {
            case OPT_EXACT:
                eq = Jim_StringEqObj(objPtr, argv[1], opt_nocase);
                break;

            case OPT_GLOB:
                eq = Jim_StringMatchObj(argv[1], objPtr, opt_nocase);
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
static int Jim_LappendCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    Jim_Obj *listObjPtr;
    int shared, i;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?value value ...?");
        return JIM_ERR;
    }
    listObjPtr = Jim_GetVariable(interp, argv[1], JIM_NONE);
    if (!listObjPtr) {
        /* Create the list if it does not exists */
        listObjPtr = Jim_NewListObj(interp, NULL, 0);
        if (Jim_SetVariable(interp, argv[1], listObjPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, listObjPtr);
            return JIM_ERR;
        }
    }
    shared = Jim_IsShared(listObjPtr);
    if (shared)
        listObjPtr = Jim_DuplicateObj(interp, listObjPtr);
    for (i = 2; i < argc; i++)
        Jim_ListAppendElement(interp, listObjPtr, argv[i]);
    if (Jim_SetVariable(interp, argv[1], listObjPtr) != JIM_OK) {
        if (shared)
            Jim_FreeNewObj(interp, listObjPtr);
        return JIM_ERR;
    }
    Jim_SetResult(interp, listObjPtr);
    return JIM_OK;
}

/* [linsert] */
static int Jim_LinsertCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    int index, len;
    Jim_Obj *listPtr;

    if (argc < 4) {
        Jim_WrongNumArgs(interp, 1, argv, "list index element "
            "?element ...?");
        return JIM_ERR;
    }
    listPtr = argv[1];
    if (Jim_IsShared(listPtr))
        listPtr = Jim_DuplicateObj(interp, listPtr);
    if (Jim_GetIndex(interp, argv[2], &index) != JIM_OK)
        goto err;
    len = Jim_ListLength(interp, listPtr);
    if (index >= len)
        index = len;
    else if (index < 0)
        index = len + index + 1;
    Jim_ListInsertElements(interp, listPtr, index, argc-3, &argv[3]);
    Jim_SetResult(interp, listPtr);
    return JIM_OK;
err:
    if (listPtr != argv[1]) {
        Jim_FreeNewObj(interp, listPtr);
    }
    return JIM_ERR;
}

/* [lreplace] */
static int Jim_LreplaceCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    int first, last, len, rangeLen;
    Jim_Obj *listObj;
    Jim_Obj *newListObj;
    int i;
    int shared;

    if (argc < 4) {
        Jim_WrongNumArgs(interp, 1, argv, "list first last ?element element ...?");
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
    JimRelToAbsRange(len, first, last, &first, &last, &rangeLen);

    /* Now construct a new list which consists of:
     * <elements before first> <supplied elements> <elements after last>
     */

    /* Check to see if trying to replace past the end of the list */
    if (first < len ) {
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

    newListObj = Jim_NewListObj(interp, NULL, 0);

    shared = Jim_IsShared(listObj);
    if (shared) {
        listObj = Jim_DuplicateObj(interp, listObj);
    }

    /* Add the first set of elements */
    for (i = 0; i < first; i++) {
        Jim_ListAppendElement(interp, newListObj, listObj->internalRep.listValue.ele[i]);
    }

    /* Add supplied elements */
    for (i = 4; i < argc; i++) {
        Jim_ListAppendElement(interp, newListObj, argv[i]);
    }

    /* Add the remaining elements */
    for (i = first + rangeLen; i < len; i++) {
        Jim_ListAppendElement(interp, newListObj, listObj->internalRep.listValue.ele[i]);
    }
    Jim_SetResult(interp, newListObj);
    if (shared) {
        Jim_FreeNewObj(interp, listObj);
    }
    return JIM_OK;
}

/* [lset] */
static int Jim_LsetCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "listVar ?index...? newVal");
        return JIM_ERR;
    } else if (argc == 3) {
        if (Jim_SetVariable(interp, argv[1], argv[2]) != JIM_OK)
            return JIM_ERR;
        Jim_SetResult(interp, argv[2]);
        return JIM_OK;
    }
    if (Jim_SetListIndex(interp, argv[1], argv+2, argc-3, argv[argc-1])
            == JIM_ERR) return JIM_ERR;
    return JIM_OK;
}

/* [lsort] */
static int Jim_LsortCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const argv[])
{
    const char *options[] = {
        "-ascii", "-nocase", "-increasing", "-decreasing", "-command", "-integer", NULL
    };
    enum {OPT_ASCII, OPT_NOCASE, OPT_INCREASING, OPT_DECREASING, OPT_COMMAND, OPT_INTEGER};
    Jim_Obj *resObj;
    int i, lsortType = JIM_LSORT_ASCII; /* default sort type */
    int lsort_order = 1;
    Jim_Obj *lsort_command = NULL;
    int retCode;

    if (argc < 2) {
wrongargs:
        Jim_WrongNumArgs(interp, 1, argv, "?options? list");
        return JIM_ERR;
    }
    for (i = 1; i < (argc-1); i++) {
        int option;

        if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ERRMSG)
                != JIM_OK)
            return JIM_ERR;
        switch(option) {
        case OPT_ASCII: lsortType = JIM_LSORT_ASCII; break;
        case OPT_NOCASE: lsortType = JIM_LSORT_NOCASE; break;
        case OPT_INTEGER: lsortType = JIM_LSORT_INTEGER; break;
        case OPT_INCREASING: lsort_order = 1; break;
        case OPT_DECREASING: lsort_order = -1; break;
        case OPT_COMMAND:
            if (i >= (argc - 2)) {
                goto wrongargs;
            }
            lsortType = JIM_LSORT_COMMAND; lsort_command = argv[i + 1]; i++; break;
        }
    }
    resObj = Jim_DuplicateObj(interp, argv[argc-1]);
    retCode = ListSortElements(interp, resObj, lsortType, lsort_order, lsort_command);
    if (retCode == JIM_OK) {
        Jim_SetResult(interp, resObj);
    }
    else {
        Jim_FreeNewObj(interp, resObj);
    }
    return retCode;
}

/* [append] */
static int Jim_AppendCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    Jim_Obj *stringObjPtr;
    int shared, i;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?value value ...?");
        return JIM_ERR;
    }
    if (argc == 2) {
        stringObjPtr = Jim_GetVariable(interp, argv[1], JIM_ERRMSG);
        if (!stringObjPtr) return JIM_ERR;
    } else {
        stringObjPtr = Jim_GetVariable(interp, argv[1], JIM_NONE);
        if (!stringObjPtr) {
            /* Create the string if it does not exists */
            stringObjPtr = Jim_NewEmptyStringObj(interp);
            if (Jim_SetVariable(interp, argv[1], stringObjPtr)
                    != JIM_OK) {
                Jim_FreeNewObj(interp, stringObjPtr);
                return JIM_ERR;
            }
        }
    }
    shared = Jim_IsShared(stringObjPtr);
    if (shared)
        stringObjPtr = Jim_DuplicateObj(interp, stringObjPtr);
    for (i = 2; i < argc; i++)
        Jim_AppendObj(interp, stringObjPtr, argv[i]);
    if (Jim_SetVariable(interp, argv[1], stringObjPtr) != JIM_OK) {
        if (shared)
            Jim_FreeNewObj(interp, stringObjPtr);
        return JIM_ERR;
    }
    Jim_SetResult(interp, stringObjPtr);
    return JIM_OK;
}

/* [debug] */
static int Jim_DebugCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    const char *options[] = {
        "refcount", "objcount", "objects", "invstr", "scriptlen", "exprlen",
        "exprbc",
        NULL
    };
    enum {
        OPT_REFCOUNT, OPT_OBJCOUNT, OPT_OBJECTS, OPT_INVSTR, OPT_SCRIPTLEN,
        OPT_EXPRLEN, OPT_EXPRBC
    };
    int option;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "subcommand ?...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], options, &option, "subcommand",
                JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    if (option == OPT_REFCOUNT) {
        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "object");
            return JIM_ERR;
        }
        Jim_SetResultInt(interp, argv[2]->refCount);
        return JIM_OK;
    } else if (option == OPT_OBJCOUNT) {
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
    } else if (option == OPT_OBJECTS) {
        Jim_Obj *objPtr, *listObjPtr, *subListObjPtr;
        /* Count the number of live objects. */
        objPtr = interp->liveList;
        listObjPtr = Jim_NewListObj(interp, NULL, 0);
        while (objPtr) {
            char buf[128];
            const char *type = objPtr->typePtr ?
                objPtr->typePtr->name : "";
            subListObjPtr = Jim_NewListObj(interp, NULL, 0);
            sprintf(buf, "%p", objPtr);
            Jim_ListAppendElement(interp, subListObjPtr,
                Jim_NewStringObj(interp, buf, -1));
            Jim_ListAppendElement(interp, subListObjPtr,
                Jim_NewStringObj(interp, type, -1));
            Jim_ListAppendElement(interp, subListObjPtr,
                Jim_NewIntObj(interp, objPtr->refCount));
            Jim_ListAppendElement(interp, subListObjPtr, objPtr);
            Jim_ListAppendElement(interp, listObjPtr, subListObjPtr);
            objPtr = objPtr->nextObjPtr;
        }
        Jim_SetResult(interp, listObjPtr);
        return JIM_OK;
    } else if (option == OPT_INVSTR) {
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
    } else if (option == OPT_SCRIPTLEN) {
        ScriptObj *script;
        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "script");
            return JIM_ERR;
        }
        script = Jim_GetScript(interp, argv[2]);
        Jim_SetResultInt(interp, script->len);
        return JIM_OK;
    } else if (option == OPT_EXPRLEN) {
        ExprByteCode *expr;
        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "expression");
            return JIM_ERR;
        }
        expr = Jim_GetExpression(interp, argv[2]);
        if (expr == NULL)
            return JIM_ERR;
        Jim_SetResultInt(interp, expr->len);
        return JIM_OK;
    } else if (option == OPT_EXPRBC) {
        Jim_Obj *objPtr;
        ExprByteCode *expr;
        int i;

        if (argc != 3) {
            Jim_WrongNumArgs(interp, 2, argv, "expression");
            return JIM_ERR;
        }
        expr = Jim_GetExpression(interp, argv[2]);
        if (expr == NULL)
            return JIM_ERR;
        objPtr = Jim_NewListObj(interp, NULL, 0);
        for (i = 0; i < expr->len; i++) {
            const char *type;
            const Jim_ExprOperator *op;
            Jim_Obj *obj = expr->token[i].objPtr;

            switch (expr->token[i].type) {
            case JIM_TT_EXPR_INT: type = "int"; break;
            case JIM_TT_EXPR_DOUBLE: type = "double"; break;
            case JIM_TT_CMD: type = "command"; break;
            case JIM_TT_VAR: type = "variable"; break;
            case JIM_TT_DICTSUGAR: type = "dictsugar"; break;
            case JIM_TT_ESC: type = "subst"; break;
            case JIM_TT_STR: type = "string"; break;
            default:
                op = JimExprOperatorInfoByOpcode(expr->token[i].type);
                if (op == NULL) {
                    type = "private";
                } else {
                    type = "operator";
                }
                obj = Jim_NewStringObj(interp, op ? op->name : "", -1);
                break;
            }
            Jim_ListAppendElement(interp, objPtr,
                    Jim_NewStringObj(interp, type, -1));
            Jim_ListAppendElement(interp, objPtr, obj);
        }
        Jim_SetResult(interp, objPtr);
        return JIM_OK;
    } else {
        Jim_SetResultString(interp,
            "bad option. Valid options are refcount, "
            "objcount, objects, invstr", -1);
        return JIM_ERR;
    }
    return JIM_OK; /* unreached */
}

/* [eval] */
static int Jim_EvalCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    int rc;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "script ?...?");
        return JIM_ERR;
    }
    if (argc == 2) {
        rc = Jim_EvalObj(interp, argv[1]);
    }
    else {
        rc = Jim_EvalObj(interp, Jim_ConcatObj(interp, argc-1, argv+1));
    }
    if (rc == JIM_ERR) {
        /* eval is "interesting", so add a stack frame here */
        interp->addStackTrace++;
    }
    return rc;
}

/* [uplevel] */
static int Jim_UplevelCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    if (argc >= 2) {
        int retcode, newLevel, oldLevel;
        Jim_CallFrame *savedCallFrame, *targetCallFrame;
        Jim_Obj *objPtr;
        const char *str;

        /* Save the old callframe pointer */
        savedCallFrame = interp->framePtr;

        /* Lookup the target frame pointer */
        str = Jim_GetString(argv[1], NULL);
        if ((str[0] >= '0' && str[0] <= '9') || str[0] == '#')
        {
            if (Jim_GetCallFrameByLevel(interp, argv[1],
                        &targetCallFrame,
                        &newLevel) != JIM_OK)
                return JIM_ERR;
            argc--;
            argv++;
        } else {
            if (Jim_GetCallFrameByLevel(interp, NULL,
                        &targetCallFrame,
                        &newLevel) != JIM_OK)
                return JIM_ERR;
        }
        if (argc < 2) {
            argv--;
            Jim_WrongNumArgs(interp, 1, argv,
                    "?level? command ?arg ...?");
            return JIM_ERR;
        }
        /* Eval the code in the target callframe. */
        interp->framePtr = targetCallFrame;
        oldLevel = interp->numLevels;
        interp->numLevels = newLevel;
        if (argc == 2) {
            retcode = Jim_EvalObj(interp, argv[1]);
        } else {
            objPtr = Jim_ConcatObj(interp, argc-1, argv+1);
            Jim_IncrRefCount(objPtr);
            retcode = Jim_EvalObj(interp, objPtr);
            Jim_DecrRefCount(interp, objPtr);
        }
        interp->numLevels = oldLevel;
        interp->framePtr = savedCallFrame;
        return retcode;
    } else {
        Jim_WrongNumArgs(interp, 1, argv, "?level? command ?arg ...?");
        return JIM_ERR;
    }
}

/* [expr] */
static int Jim_ExprCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    Jim_Obj *exprResultPtr;
    int retcode;

    if (argc == 2) {
        retcode = Jim_EvalExpression(interp, argv[1], &exprResultPtr);
    } else if (argc > 2) {
        Jim_Obj *objPtr;

        objPtr = Jim_ConcatObj(interp, argc-1, argv+1);
        Jim_IncrRefCount(objPtr);
        retcode = Jim_EvalExpression(interp, objPtr, &exprResultPtr);
        Jim_DecrRefCount(interp, objPtr);
    } else {
        Jim_WrongNumArgs(interp, 1, argv, "expression ?...?");
        return JIM_ERR;
    }
    if (retcode != JIM_OK) return retcode;
    Jim_SetResult(interp, exprResultPtr);
    Jim_DecrRefCount(interp, exprResultPtr);
    return JIM_OK;
}

/* [break] */
static int Jim_BreakCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    return JIM_BREAK;
}

/* [continue] */
static int Jim_ContinueCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    return JIM_CONTINUE;
}

/* [return] */
static int Jim_ReturnCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    int i;
    Jim_Obj *stackTraceObj = NULL;
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
        Jim_WrongNumArgs(interp, 1, argv, "?-code code? ?-errorinfo stacktrace? ?-level level? ?result?");
    }

    /* If a stack trace is supplied and code is error, set the stack trace */
    if (stackTraceObj && returnCode == JIM_ERR) {
        JimSetStackTrace(interp, stackTraceObj);
    }
    interp->returnCode = returnCode;
    interp->returnLevel = level;

    if (i == argc - 1) {
        Jim_SetResult(interp, argv[i]);
    }
    return JIM_RETURN;
}

/* [tailcall] */
static int Jim_TailcallCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;

    objPtr = Jim_NewListObj(interp, argv+1, argc-1);
    Jim_SetResult(interp, objPtr);
    return JIM_EVAL;
}

/* [proc] */
static int Jim_ProcCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    int argListLen;
    int leftArity, rightArity;
    int i;
    int optionalArgs = 0;
    int args = 0;

    if (argc != 4 && argc != 5) {
        Jim_WrongNumArgs(interp, 1, argv, "name arglist ?statics? body");
        return JIM_ERR;
    }
    argListLen = Jim_ListLength(interp, argv[2]);
    leftArity = 0;
    rightArity = 0;

    /* Examine the argument list for default parameters and 'args' */
    for (i = 0; i < argListLen; i++) {
        Jim_Obj *argPtr;
        int len;

        Jim_ListIndex(interp, argv[2], i, &argPtr, JIM_NONE);
        if (Jim_CompareStringImmediate(interp, argPtr, "args")) {
            if (args) {
                Jim_SetResultString(interp, "procedure has 'args' specified more than once", -1);
                return JIM_ERR;
            }
            if (rightArity) {
                Jim_SetResultString(interp, "procedure has 'args' in invalid position", -1);
                return JIM_ERR;
            }
            args = 1;
            continue;
        }

        /* Does this parameter have a default? */
        Jim_GetString(argPtr, NULL);
        len = Jim_ListLength(interp, argPtr);
        if (len == 0) {
            Jim_SetResultString(interp, "procedure has argument with no name", -1);
            return JIM_ERR;
        }
        if (len > 2) {
            Jim_SetResultString(interp, "procedure has argument with too many fields", -1);
            return JIM_ERR;
        }
        if (len == 1) {
            /* A required arg. Is it part of leftArity or rightArity? */
            if (optionalArgs || args) {
                rightArity++;
            }
            else {
                leftArity++;
            }
        }
        else {
            /* Optional arg. Can't be after rightArity */
            if (rightArity || args) {
                Jim_SetResultString(interp, "procedure has optional arg in invalid position", -1);
                return JIM_ERR;
            }
            optionalArgs++;
        }
    }

    if (argc == 4) {
        return Jim_CreateProcedure(interp, Jim_GetString(argv[1], NULL),
                argv[2], NULL, argv[3], leftArity, optionalArgs, args, rightArity);
    } else {
        return Jim_CreateProcedure(interp, Jim_GetString(argv[1], NULL),
                argv[2], argv[3], argv[4], leftArity, optionalArgs, args, rightArity);
    }
}

/* [concat] */
static int Jim_ConcatCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    Jim_SetResult(interp, Jim_ConcatObj(interp, argc-1, argv+1));
    return JIM_OK;
}

/* [upvar] */
static int Jim_UpvarCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    int i;
    Jim_CallFrame *targetCallFrame;

    /* Lookup the target frame pointer */
    if (argc > 3 && (argc % 2 == 0)) {
        if (Jim_GetCallFrameByLevel(interp, argv[1], &targetCallFrame, NULL) != JIM_OK) {
            return JIM_ERR;
        }
        argc--;
        argv++;
    } else if (Jim_GetCallFrameByLevel(interp, NULL, &targetCallFrame, NULL) != JIM_OK) {
        return JIM_ERR;
    }

    /* Check for arity */
    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "?level? otherVar localVar ?otherVar localVar ...?");
        return JIM_ERR;
    }

    /* Now... for every other/local couple: */
    for (i = 1; i < argc; i += 2) {
        if (Jim_SetVariableLink(interp, argv[i+1], argv[i],
                targetCallFrame) != JIM_OK) return JIM_ERR;
    }
    return JIM_OK;
}

/* [global] */
static int Jim_GlobalCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    int i;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?varName ...?");
        return JIM_ERR;
    }
    /* Link every var to the toplevel having the same name */
    if (interp->numLevels == 0) return JIM_OK; /* global at toplevel... */
    for (i = 1; i < argc; i++) {
        if (Jim_SetVariableLink(interp, argv[i], argv[i],
                interp->topFramePtr) != JIM_OK) return JIM_ERR;
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
    const char **key, *str, *noMatchStart = NULL;
    Jim_Obj **value;
    int *keyLen, strLen, i;
    Jim_Obj *resultObjPtr;
    
    numMaps = Jim_ListLength(interp, mapListObjPtr);
    if (numMaps % 2) {
        Jim_SetResultString(interp,
                "list must contain an even number of elements", -1);
        return NULL;
    }
    /* Initialization */
    numMaps /= 2;
    key = Jim_Alloc(sizeof(char*)*numMaps);
    keyLen = Jim_Alloc(sizeof(int)*numMaps);
    value = Jim_Alloc(sizeof(Jim_Obj*)*numMaps);
    resultObjPtr = Jim_NewStringObj(interp, "", 0);
    for (i = 0; i < numMaps; i++) {
        Jim_Obj *eleObjPtr = 0;

        Jim_ListIndex(interp, mapListObjPtr, i*2, &eleObjPtr, JIM_NONE);
        key[i] = Jim_GetString(eleObjPtr, &keyLen[i]);
        Jim_ListIndex(interp, mapListObjPtr, i*2+1, &eleObjPtr, JIM_NONE);
        value[i] = eleObjPtr;
    }
    str = Jim_GetString(objPtr, &strLen);
    /* Map it */
    while(strLen) {
        for (i = 0; i < numMaps; i++) {
            if (strLen >= keyLen[i] && keyLen[i]) {
                if (!JimStringCompare(str, keyLen[i], key[i], keyLen[i],
                            nocase))
                {
                    if (noMatchStart) {
                        Jim_AppendString(interp, resultObjPtr,
                                noMatchStart, str-noMatchStart);
                        noMatchStart = NULL;
                    }
                    Jim_AppendObj(interp, resultObjPtr, value[i]);
                    str += keyLen[i];
                    strLen -= keyLen[i];
                    break;
                }
            }
        }
        if (i == numMaps) { /* no match */
            if (noMatchStart == NULL)
                noMatchStart = str;
            str ++;
            strLen --;
        }
    }
    if (noMatchStart) {
        Jim_AppendString(interp, resultObjPtr,
            noMatchStart, str-noMatchStart);
    }
    Jim_Free((void*)key);
    Jim_Free(keyLen);
    Jim_Free(value);
    return resultObjPtr;
}

/* [string] */
static int Jim_StringCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    int len;
    int opt_case = 1;
    int option;
    static const char *options[] = {
        "length", "compare", "match", "equal", "range", "map",
        "repeat", "reverse", "index", "first", "last",
        "trim", "trimleft", "trimright", "tolower", "toupper", NULL
    };
    enum {
        OPT_LENGTH, OPT_COMPARE, OPT_MATCH, OPT_EQUAL, OPT_RANGE, OPT_MAP,
        OPT_REPEAT, OPT_REVERSE, OPT_INDEX, OPT_FIRST, OPT_LAST,
        OPT_TRIM, OPT_TRIMLEFT, OPT_TRIMRIGHT, OPT_TOLOWER, OPT_TOUPPER
    };
    static const char *nocase_options[] = {
        "-nocase", NULL
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
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "string");
                return JIM_ERR;
            }
            Jim_GetString(argv[2], &len);
            Jim_SetResultInt(interp, len);
            return JIM_OK;

        case OPT_COMPARE:
        case OPT_EQUAL:
            if (argc != 4 &&
                (argc != 5 ||
                Jim_GetEnum(interp, argv[2], nocase_options, &opt_case, NULL, JIM_ENUM_ABBREV) != JIM_OK)) {
                Jim_WrongNumArgs(interp, 2, argv, "?-nocase? string1 string2");
                return JIM_ERR;
            }
            if (opt_case == 0) {
                argv++;
            }
            if (option == OPT_COMPARE) {
                Jim_SetResultInt(interp, Jim_StringCompareObj(argv[2], argv[3], !opt_case));
            }
            else {
                Jim_SetResultBool(interp, Jim_StringEqObj(argv[2], argv[3], !opt_case));
            }
            return JIM_OK;

        case OPT_MATCH:
            if (argc != 4 &&
                (argc != 5 ||
                Jim_GetEnum(interp, argv[2], nocase_options, &opt_case, NULL, JIM_ENUM_ABBREV) != JIM_OK)) {
                Jim_WrongNumArgs(interp, 2, argv, "?-nocase? pattern string");
                return JIM_ERR;
            }
            if (opt_case == 0) {
                argv++;
            }
            Jim_SetResultBool(interp, Jim_StringMatchObj(argv[2], argv[3], !opt_case));
            return JIM_OK;

        case OPT_MAP: {
            Jim_Obj *objPtr;

            if (argc != 4 &&
                (argc != 5 ||
                Jim_GetEnum(interp, argv[2], nocase_options, &opt_case, NULL, JIM_ENUM_ABBREV) != JIM_OK)) {
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

        case OPT_RANGE: {
            Jim_Obj *objPtr;

            if (argc != 5) {
                Jim_WrongNumArgs(interp, 2, argv, "string first last");
                return JIM_ERR;
            }
            objPtr = Jim_StringRangeObj(interp, argv[2], argv[3], argv[4]);
            if (objPtr == NULL) {
                return JIM_ERR;
            }
            Jim_SetResult(interp, objPtr);
            return JIM_OK;
        }

        case OPT_REPEAT: {
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

        case OPT_REVERSE: {
            char *buf;
            const char *str;
            int i;

            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "string");
                return JIM_ERR;
            }
            str = Jim_GetString(argv[2], &len);
            buf = Jim_Alloc(len + 1);
            for (i = 0; i < len; i++) {
                buf[i] = str[len - i - 1];
            }
            buf[i] = 0;
            Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, buf, len));
            return JIM_OK;
        }

        case OPT_INDEX: {
            int index, len;
            const char *str;

            if (argc != 4) {
                Jim_WrongNumArgs(interp, 2, argv, "string index");
                return JIM_ERR;
            }
            if (Jim_GetIndex(interp, argv[3], &index) != JIM_OK) {
                return JIM_ERR;
            }
            str = Jim_GetString(argv[2], &len);
            if (index != INT_MIN && index != INT_MAX) {
                index = JimRelToAbsIndex(len, index);
            }
            if (index < 0 || index >= len) {
                Jim_SetResultString(interp, "", 0);
            } else {
                Jim_SetResultString(interp, str + index, 1);
            }
            return JIM_OK;
        }

        case OPT_FIRST:
        case OPT_LAST: {
            int index = 0, l1, l2;
            const char *s1, *s2;

            if (argc != 4 && argc != 5) {
                Jim_WrongNumArgs(interp, 2, argv, "subString string ?index?");
                return JIM_ERR;
            }
            s1 = Jim_GetString(argv[2], &l1);
            s2 = Jim_GetString(argv[3], &l2);
            if (argc == 5) {
                if (Jim_GetIndex(interp, argv[4], &index) != JIM_OK) {
                    return JIM_ERR;
                }
                index = JimRelToAbsIndex(l2, index);
            }
            else if (option == OPT_LAST) {
                index = l2;
            }
            if (option == OPT_FIRST) {
                Jim_SetResultInt(interp, JimStringFirst(s1, l1, s2, l2, index));
            }
            else {
                Jim_SetResultInt(interp, JimStringLast(s1, l1, s2, index));
            }
            return JIM_OK;
        }

        case OPT_TRIM:
        case OPT_TRIMLEFT:
        case OPT_TRIMRIGHT: {
            Jim_Obj *trimchars;

            if (argc != 3 && argc != 4) {
                Jim_WrongNumArgs(interp, 2, argv, "string ?trimchars?");
                return JIM_ERR;
            }
            trimchars = (argc == 4 ? argv[3] : NULL);
            if (option == OPT_TRIM) {
                Jim_SetResult(interp, JimStringTrim(interp, argv[2], trimchars));
            } else if (option == OPT_TRIMLEFT) {
                Jim_SetResult(interp, JimStringTrimLeft(interp, argv[2], trimchars));
            } else if (option == OPT_TRIMRIGHT) {
                Jim_SetResult(interp, JimStringTrimRight(interp, argv[2], trimchars));
            }
            return JIM_OK;
        }

        case OPT_TOLOWER:
        case OPT_TOUPPER:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "string");
                return JIM_ERR;
            }
            if (option == OPT_TOLOWER) {
                Jim_SetResult(interp, JimStringToLower(interp, argv[2]));
            }
            else {
                Jim_SetResult(interp, JimStringToUpper(interp, argv[2]));
            }
            return JIM_OK;
    }
    return JIM_OK;
}

/* [time] */
static int Jim_TimeCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    long i, count = 1;
    jim_wide start, elapsed;
    char buf [256];
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
    sprintf(buf, fmt, elapsed/count);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

/* [exit] */
static int Jim_ExitCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
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
static int Jim_CatchCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    int exitCode = 0;
    int i;
    int sig = 0;

    /* Which return codes are caught? These are the defaults */
    jim_wide mask = (1 << JIM_OK | 1 << JIM_ERR | 1 << JIM_BREAK | 1 << JIM_CONTINUE | 1 << JIM_RETURN);

    for (i = 1; i < argc - 1; i++) {
        const char *arg = Jim_GetString(argv[i], NULL);
        jim_wide option;
        int add;

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
            add = 0;
        }
        else {
            arg++;
            add = 1;
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

        if (add) {
            mask |= (1 << option);
        }
        else {
            mask &= ~(1 << option);
        }
    }

    argc -= i;
    if (argc < 1 || argc > 3) {
wrongargs:
        Jim_WrongNumArgs(interp, 1, argv, "?-?no?code ... --? script ?resultVarName? ?optionVarName?");
        return JIM_ERR;
    }
    argv += i;

    if (mask & (1 << JIM_SIGNAL)) {
        sig++;
    }

    interp->signal_level += sig;
    if (interp->signal_level && interp->sigmask) {
        /* If a signal is set, don't even try to execute the body */
        exitCode = JIM_SIGNAL;
    }
    else {
        exitCode = Jim_EvalObj(interp, argv[0]);
    }
    interp->signal_level -= sig;

    /* Catch or pass through? Only the first 64 codes can be passed through */
    if (exitCode >= 0 && exitCode < (int)sizeof(mask) && ((1 << exitCode) & mask) == 0) {
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
                Jim_ListAppendElement(interp, optListObj, Jim_NewStringObj(interp, "-errorinfo", -1));
                Jim_ListAppendElement(interp, optListObj, interp->stackTrace);
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
static int Jim_RefCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    if (argc != 3 && argc != 4) {
        Jim_WrongNumArgs(interp, 1, argv, "string tag ?finalizer?");
        return JIM_ERR;
    }
    if (argc == 3) {
        Jim_SetResult(interp, Jim_NewReference(interp, argv[1], argv[2], NULL));
    } else {
        Jim_SetResult(interp, Jim_NewReference(interp, argv[1], argv[2],
                    argv[3]));
    }
    return JIM_OK;
}

/* [getref] */
static int Jim_GetrefCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
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
static int Jim_SetrefCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
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
static int Jim_CollectCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, Jim_Collect(interp));
    return JIM_OK;
}

/* [finalize] reference ?newValue? */
static int Jim_FinalizeCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
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
    } else {
        if (Jim_SetFinalizer(interp, argv[1], argv[2]) != JIM_OK)
            return JIM_ERR;
        Jim_SetResult(interp, argv[2]);
    }
    return JIM_OK;
}
/* TODO */
/* [info references] (list of all the references/finalizers) */
#endif

/* [rename] */
static int Jim_RenameCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    const char *oldName, *newName;

    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "oldName newName");
        return JIM_ERR;
    }
    oldName = Jim_GetString(argv[1], NULL);
    newName = Jim_GetString(argv[2], NULL);
    if (Jim_RenameCommand(interp, oldName, newName) != JIM_OK) {
        Jim_SetResultFormatted(interp, "can't rename \"%#s\": command doesn't exist", argv[1]);
        return JIM_ERR;
    }
    return JIM_OK;
}

/* [dict] */
static int Jim_DictCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    int option;
    const char *options[] = {
        "create", "get", "set", "unset", "exists", NULL
    };
    enum {
        OPT_CREATE, OPT_GET, OPT_SET, OPT_UNSET, OPT_EXIST
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
            if (Jim_DictKeysVector(interp, argv[2], argv+3, argc-3, &objPtr, JIM_ERRMSG) != JIM_OK) {
                return JIM_ERR;
            }
            Jim_SetResult(interp, objPtr);
            return JIM_OK;

        case OPT_SET:
            if (argc < 5) {
                Jim_WrongNumArgs(interp, 2, argv, "varName key ?key ...? value");
                return JIM_ERR;
            }
            return Jim_SetDictKeysVector(interp, argv[2], argv+3, argc-4, argv[argc-1]);

        case OPT_EXIST:
            Jim_SetResultBool(interp, Jim_DictKeysVector(interp, argv[2], argv+3, argc-3, &objPtr, JIM_ERRMSG) == JIM_OK);
            return JIM_OK;

        case OPT_UNSET:
            if (argc < 4) {
                Jim_WrongNumArgs(interp, 2, argv, "varName key ?key ...?");
                return JIM_ERR;
            }
            return Jim_SetDictKeysVector(interp, argv[2], argv+3, argc-3, NULL);

        case OPT_CREATE:
            if (argc % 2) {
                Jim_WrongNumArgs(interp, 2, argv, "?key value ...?");
                return JIM_ERR;
            }
            objPtr = Jim_NewDictObj(interp, argv+2, argc-2);
            Jim_SetResult(interp, objPtr);
            return JIM_OK;

        default:
            abort();
    }
}

/* [subst] */
static int Jim_SubstCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    const char *options[] = {
        "-nobackslashes", "-nocommands", "-novariables", NULL
    };
    enum {OPT_NOBACKSLASHES, OPT_NOCOMMANDS, OPT_NOVARIABLES};
    int i;
    int flags = JIM_SUBST_FLAG;
    Jim_Obj *objPtr;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "?options? string");
        return JIM_ERR;
    }
    for (i = 1; i < (argc-1); i++) {
        int option;

        if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
            return JIM_ERR;
        }
        switch(option) {
        case OPT_NOBACKSLASHES: flags |= JIM_SUBST_NOESC; break;
        case OPT_NOCOMMANDS: flags |= JIM_SUBST_NOCMD; break;
        case OPT_NOVARIABLES: flags |= JIM_SUBST_NOVAR; break;
        }
    }
    if (Jim_SubstObj(interp, argv[argc-1], &objPtr, flags) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

/* [info] */
static int Jim_InfoCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    int cmd;
    Jim_Obj *objPtr;
    int mode = 0;

    static const char *commands[] = {
        "body", "commands", "procs", "exists", "globals", "level", "locals",
        "vars", "version", "patchlevel", "complete", "args", "hostname",
        "script", "source", "stacktrace", "nameofexecutable", "returncodes", NULL
    };
    enum {INFO_BODY, INFO_COMMANDS, INFO_PROCS, INFO_EXISTS, INFO_GLOBALS, INFO_LEVEL,
          INFO_LOCALS, INFO_VARS, INFO_VERSION, INFO_PATCHLEVEL, INFO_COMPLETE, INFO_ARGS,
          INFO_HOSTNAME, INFO_SCRIPT, INFO_SOURCE, INFO_STACKTRACE, INFO_NAMEOFEXECUTABLE,
          INFO_RETURNCODES };
    
    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "subcommand ?args ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], commands, &cmd, "subcommand", JIM_ERRMSG | JIM_ENUM_ABBREV)
        != JIM_OK) {
        return JIM_ERR;
    }

    /* Test for the the most common commands first, just in case it makes a difference */
    switch (cmd) {
        case INFO_EXISTS: {
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "varName");
                return JIM_ERR;
            }
            Jim_SetResultBool(interp, Jim_GetVariable(interp, argv[2], 0) != NULL);
            break;
        }

        case INFO_COMMANDS:
        case INFO_PROCS:
            if (argc != 2 && argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "?pattern?");
                return JIM_ERR;
            }
            Jim_SetResult(interp, JimCommandsList(interp, (argc == 3) ? argv[2] : NULL, (cmd == INFO_PROCS)));
            break;

        case INFO_VARS:
            mode++; /* JIM_VARLIST_VARS */
        case INFO_LOCALS:
            mode++; /* JIM_VARLIST_LOCALS */
        case INFO_GLOBALS:
            /* mode 0 => JIM_VARLIST_GLOBALS */
            if (argc != 2 && argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "?pattern?");
                return JIM_ERR;
            }
            Jim_SetResult(interp,JimVariablesList(interp, argc == 3 ? argv[2] : NULL, mode));
            break;

        case INFO_SCRIPT:
            if (argc != 2) {
                Jim_WrongNumArgs(interp, 2, argv, "");
                return JIM_ERR;
            }
            Jim_SetResultString(interp, Jim_GetScript(interp, interp->currentScriptObj)->fileName, -1);
            break;

        case INFO_SOURCE: {
            const char *filename = "";
            int line = 0;
            Jim_Obj *resObjPtr;

            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "source");
                return JIM_ERR;
            }
            if (argv[2]->typePtr == &sourceObjType) {
                filename = argv[2]->internalRep.sourceValue.fileName;
                line = argv[2]->internalRep.sourceValue.lineNumber;
            }
            else if (argv[2]->typePtr == &scriptObjType) {
                ScriptObj *script = Jim_GetScript(interp, argv[2]);
                filename = script->fileName;
                if (script->token) {
                    line = script->token->linenr;
                }
            }
            resObjPtr = Jim_NewListObj(interp, NULL, 0);
            Jim_ListAppendElement(interp, resObjPtr, Jim_NewStringObj(interp, filename, -1));
            Jim_ListAppendElement(interp, resObjPtr, Jim_NewIntObj(interp, line));
            Jim_SetResult(interp, resObjPtr);
            break;
        }

        case INFO_STACKTRACE:
            Jim_SetResult(interp, interp->stackTrace);
            break;

        case INFO_LEVEL:
            switch (argc) {
                case 2:
                    Jim_SetResultInt(interp, interp->numLevels);
                    break;

                case 3:
                    if (JimInfoLevel(interp, argv[2], &objPtr) != JIM_OK) {
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
        case INFO_ARGS: {
            Jim_Cmd *cmdPtr;

            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "procname");
                return JIM_ERR;
            }
            if ((cmdPtr = Jim_GetCommand(interp, argv[2], JIM_ERRMSG)) == NULL) {
                return JIM_ERR;
            }
            if (cmdPtr->cmdProc != NULL) {
                Jim_SetResultFormatted(interp, "command \"%#s\" is not a procedure", argv[2]);
                return JIM_ERR;
            }
            Jim_SetResult(interp, cmd == INFO_BODY ? cmdPtr->bodyObjPtr : cmdPtr->argListObjPtr);
            break;
        }

        case INFO_VERSION:
        case INFO_PATCHLEVEL: {
            char buf[(JIM_INTEGER_SPACE * 2) + 1];
            sprintf(buf, "%d.%d", 
                    JIM_VERSION / 100, JIM_VERSION % 100);
            Jim_SetResultString(interp, buf, -1);
            break;
        }

        case INFO_COMPLETE:
            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "script");
                return JIM_ERR;
            }
            else {
                int len;
                const char *s = Jim_GetString(argv[2], &len);

                Jim_SetResultBool(interp, Jim_ScriptIsComplete(s, len, NULL));
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
                    Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, jimReturnCodes[i], -1));
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
    }
    return JIM_OK;
}

/* [split] */
static int Jim_SplitCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    const char *str, *splitChars, *noMatchStart;
    int splitLen, strLen, i;
    Jim_Obj *resObjPtr;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "string ?splitChars?");
        return JIM_ERR;
    }
    /* Init */
    if (argc == 2) {
        splitChars = " \n\t\r";
        splitLen = 4;
    } else {
        splitChars = Jim_GetString(argv[2], &splitLen);
    }
    str = Jim_GetString(argv[1], &strLen);
    if (!strLen) return JIM_OK;
    noMatchStart = str;
    resObjPtr = Jim_NewListObj(interp, NULL, 0);
    /* Split */
    if (splitLen) {
        while (strLen) {
            for (i = 0; i < splitLen; i++) {
                if (*str == splitChars[i]) {
                    Jim_Obj *objPtr;

                    objPtr = Jim_NewStringObj(interp, noMatchStart,
                            (str-noMatchStart));
                    Jim_ListAppendElement(interp, resObjPtr, objPtr);
                    noMatchStart = str+1;
                    break;
                }
            }
            str ++;
            strLen --;
        }
        Jim_ListAppendElement(interp, resObjPtr,
                Jim_NewStringObj(interp, noMatchStart, (str-noMatchStart)));
    } else {
        /* This handles the special case of splitchars eq {}. This
         * is trivial but we want to perform object sharing as Tcl does. */
        Jim_Obj *objCache[256];
        const unsigned char *u = (unsigned char*) str;
        memset(objCache, 0, sizeof(objCache));
        for (i = 0; i < strLen; i++) {
            int c = u[i];
            
            if (objCache[c] == NULL)
                objCache[c] = Jim_NewStringObj(interp, (char*)u+i, 1);
            Jim_ListAppendElement(interp, resObjPtr, objCache[c]);
        }
    }
    Jim_SetResult(interp, resObjPtr);
    return JIM_OK;
}

/* [join] */
static int Jim_JoinCoreCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    const char *joinStr;
    int joinStrLen, i, listLen;
    Jim_Obj *resObjPtr;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "list ?joinString?");
        return JIM_ERR;
    }
    /* Init */
    if (argc == 2) {
        joinStr = " ";
        joinStrLen = 1;
    } else {
        joinStr = Jim_GetString(argv[2], &joinStrLen);
    }
    listLen = Jim_ListLength(interp, argv[1]);
    resObjPtr = Jim_NewStringObj(interp, NULL, 0);
    /* Split */
    for (i = 0; i < listLen; i++) {
        Jim_Obj *objPtr = 0;

        Jim_ListIndex(interp, argv[1], i, &objPtr, JIM_NONE);
        Jim_AppendObj(interp, resObjPtr, objPtr);
        if (i+1 != listLen) {
            Jim_AppendString(interp, resObjPtr, joinStr, joinStrLen);
        }
    }
    Jim_SetResult(interp, resObjPtr);
    return JIM_OK;
}

/* [format] */
static int Jim_FormatCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "formatString ?arg arg ...?");
        return JIM_ERR;
    }
    objPtr = Jim_FormatString(interp, argv[1], argc-2, argv+2);
    if (objPtr == NULL)
        return JIM_ERR;
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

/* [scan] */
static int Jim_ScanCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    Jim_Obj *listPtr, **outVec;
    int outc, i, count = 0;

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
        if (maxPos > argc-3) {
            Jim_SetResultString(interp, "\"%n$\" argument index out of range", -1);
            return JIM_ERR;
        } else if (count > argc-3) {
            Jim_SetResultString(interp, "different numbers of variable names and "
                "field specifiers", -1);
            return JIM_ERR;
        } else if (count < argc-3) {
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

        count = 0;

        if (listPtr != 0 && listPtr != (Jim_Obj*)EOF) {
            int len = Jim_ListLength(interp, listPtr);

            if (len != 0) {
                JimListGetElements(interp, listPtr, &outc, &outVec);
                for (i = 0; i < outc; ++i) {
                    if (Jim_Length(outVec[i]) > 0) {
                        ++count;
                        if (Jim_SetVariable(interp, argv[3+i], outVec[i]) != JIM_OK) {
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
    } else {
        if (listPtr == (Jim_Obj*)EOF) {
            Jim_SetResult(interp, Jim_NewListObj(interp, 0, 0));
            return JIM_OK;
        }
        Jim_SetResult(interp, listPtr);
    }
    return JIM_OK;
}

/* [error] */
static int Jim_ErrorCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
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
static int Jim_LrangeCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
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
static int Jim_LrepeatCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    long count;

    if (argc < 3 || Jim_GetLong(interp, argv[1], &count) != JIM_OK || count <= 0) {
        Jim_WrongNumArgs(interp, 1, argv, "positiveCount value ?value ...?");
        return JIM_ERR;
    }

    argc -= 2;
    argv += 2;

    objPtr = Jim_NewListObj(interp, argv, argc);
    while (--count) {
        int i;
        for (i = 0; i < argc; i++) {
            ListAppendElement(objPtr, argv[i]);
        }
    }

    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

/* [env] */
static int Jim_EnvCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    const char *key;
    const char *val;

    if (argc == 1) {
        extern char **environ;

        int i;
        Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);

        for (i = 0; environ[i]; i++) {
            const char *equals = strchr(environ[i], '=');
            if (equals) {
                Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, environ[i], equals - environ[i]));
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
    key = Jim_GetString(argv[1], NULL);
    val = getenv(key);
    if (val == NULL) {
        if (argc < 3) {
            Jim_SetResultFormatted(interp, "environment variable \"%#s\" does not exist", argv[1]);
            return JIM_ERR;
        }
        val = Jim_GetString(argv[2], NULL);
    }
    Jim_SetResult(interp, Jim_NewStringObj(interp, val, -1));
    return JIM_OK;
}

/* [source] */
static int Jim_SourceCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
{
    int retval;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "fileName");
        return JIM_ERR;
    }
    retval = Jim_EvalFile(interp, Jim_GetString(argv[1], NULL));
    if (retval == JIM_RETURN)
        return JIM_OK;
    return retval;
}

/* [lreverse] */
static int Jim_LreverseCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
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

    if (step == 0) return -1;
    if (start == end) return 0;
    else if (step > 0 && start > end) return -1;
    else if (step < 0 && end > start) return -1;
    len = end-start;
    if (len < 0) len = -len; /* abs(len) */
    if (step < 0) step = -step; /* abs(step) */
    len = 1 + ((len-1)/step);
    /* We can truncate safely to INT_MAX, the range command
     * will always return an error for a such long range
     * because Tcl lists can't be so long. */
    if (len > INT_MAX) len = INT_MAX;
    return (int)((len < 0) ? -1 : len);
}

/* [range] */
static int Jim_RangeCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
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
    } else {
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
        ListAppendElement(objPtr, Jim_NewIntObj(interp, start+i*step));
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

/* [rand] */
static int Jim_RandCoreCommand(Jim_Interp *interp, int argc,
        Jim_Obj *const *argv)
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
    Jim_CmdProc cmdProc;
} Jim_CoreCommandsTable[] = {
    {"set", Jim_SetCoreCommand},
    {"unset", Jim_UnsetCoreCommand},
    {"puts", Jim_PutsCoreCommand},
    {"+", Jim_AddCoreCommand},
    {"*", Jim_MulCoreCommand},
    {"-", Jim_SubCoreCommand},
    {"/", Jim_DivCoreCommand},
    {"incr", Jim_IncrCoreCommand},
    {"while", Jim_WhileCoreCommand},
    {"for", Jim_ForCoreCommand},
    {"foreach", Jim_ForeachCoreCommand},
    {"lmap", Jim_LmapCoreCommand},
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
    {NULL, NULL},
};

/* Some Jim core command is actually a procedure written in Jim itself. */
static void Jim_RegisterCoreProcedures(Jim_Interp *interp)
{
#ifdef JIM_REFERENCES
    Jim_Eval(interp, 
"proc lambda {arglist args} {\n"
"    set name [ref {} function lambdaFinalizer]\n"
"    uplevel 1 [list proc $name $arglist {expand}$args]\n"
"    return $name\n"
"}\n"
"proc lambdaFinalizer {name val} {\n"
"    rename $name {}\n"
"}\n"
    );
#endif
}

void Jim_RegisterCoreCommands(Jim_Interp *interp)
{
    int i = 0;

    while(Jim_CoreCommandsTable[i].name != NULL) {
        Jim_CreateCommand(interp, 
                Jim_CoreCommandsTable[i].name,
                Jim_CoreCommandsTable[i].cmdProc,
                NULL, NULL);
        i++;
    }
    Jim_RegisterCoreProcedures(interp);
}

/* -----------------------------------------------------------------------------
 * Interactive prompt
 * ---------------------------------------------------------------------------*/
void Jim_PrintErrorMessage(Jim_Interp *interp)
{
    int len, i;

    if (*interp->errorFileName) {
        fprintf(stderr, "%s:%d: Runtime Error: ",
                                    interp->errorFileName, interp->errorLine);
    }
    fprintf(stderr, "%s" JIM_NL,
            Jim_GetString(interp->result, NULL));
    len = Jim_ListLength(interp, interp->stackTrace);
    for (i = len-3; i >= 0; i-= 3) {
        Jim_Obj *objPtr = 0;
        const char *proc, *file, *line;

        Jim_ListIndex(interp, interp->stackTrace, i, &objPtr, JIM_NONE);
        proc = Jim_GetString(objPtr, NULL);
        Jim_ListIndex(interp, interp->stackTrace, i+1, &objPtr,
                JIM_NONE);
        file = Jim_GetString(objPtr, NULL);
        Jim_ListIndex(interp, interp->stackTrace, i+2, &objPtr,
                JIM_NONE);
        line = Jim_GetString(objPtr, NULL);
        if (*proc) {
            fprintf(stderr,
                    "in procedure '%s' ", proc);
            if (*file) {
                fprintf(stderr, "called ");
            }
        }
        if (*file) {
            fprintf(stderr,
                    "at file \"%s\", line %s",
                    file, line);
        }
        if (*file || *proc) {
            fprintf(stderr, JIM_NL);
        }
    }
}

static void JimSetFailedEnumResult(Jim_Interp *interp, const char *arg, const char *badtype, const char *prefix, const char * const *tablePtr, const char *name)
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
    tablePtrSorted = Jim_Alloc(sizeof(char*)*count);
    memcpy(tablePtrSorted, tablePtr, sizeof(char*)*count);
    qsort(tablePtrSorted, count, sizeof(char*), qsortCompareStringPointers);
    for (i = 0; i < count; i++) {
        if (i+1 == count && count > 1) {
            Jim_AppendString(interp, Jim_GetResult(interp), "or ", -1);
        }
        Jim_AppendStrings(interp, Jim_GetResult(interp), prefix, tablePtrSorted[i], NULL);
        if (i+1 != count) {
            Jim_AppendString(interp, Jim_GetResult(interp), ", ", -1);
        }
    }
    Jim_Free(tablePtrSorted);
}

int Jim_GetEnum(Jim_Interp *interp, Jim_Obj *objPtr, 
        const char * const *tablePtr, int *indexPtr, const char *name, int flags)
{
    const char *bad = "bad ";
    const char * const *entryPtr = NULL;
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

int Jim_FindByName(const char *name, const char *array[], size_t len)
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
            params[n] = va_arg(args, char*);
            l = strlen(params[n]);
        }
        else if (strncmp(format + i, "%#s", 3) == 0) {
            Jim_Obj *objPtr = va_arg(args, Jim_Obj*);
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

    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, buf, len));
}

/*
 * Local Variables: ***
 * c-basic-offset: 4 ***
 * tab-width: 4 ***
 * End: ***
 */
