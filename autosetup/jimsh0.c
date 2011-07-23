/* This is single source file, bootstrap version of Jim Tcl. See http://jim.berlios.de/ */
#define _GNU_SOURCE
#define JIM_TCL_COMPAT
#define JIM_REFERENCES
#define JIM_ANSIC
#define JIM_REGEXP
#define HAVE_NO_AUTOCONF
#define _JIMAUTOCONF_H
#define TCL_LIBRARY "."
#define jim_ext_bootstrap
#define jim_ext_aio
#define jim_ext_readdir
#define jim_ext_glob
#define jim_ext_regexp
#define jim_ext_file
#define jim_ext_exec
#define jim_ext_clock
#define jim_ext_array
#define jim_ext_stdlib
#define jim_ext_tclcompat
#if defined(__MINGW32__)
#define TCL_PLATFORM_OS "mingw"
#define TCL_PLATFORM_PLATFORM "windows"
#define TCL_PLATFORM_PATH_SEPARATOR ";"
#define HAVE_MKDIR_ONE_ARG
#define HAVE_SYSTEM
#else
#define TCL_PLATFORM_OS "unknown"
#define TCL_PLATFORM_PLATFORM "unix"
#define TCL_PLATFORM_PATH_SEPARATOR ":"
#define HAVE_VFORK
#define HAVE_WAITPID
#endif
#ifndef UTF8_UTIL_H
#define UTF8_UTIL_H
/**
 * UTF-8 utility functions
 *
 * (c) 2010 Steve Bennett <steveb@workware.net.au>
 *
 * See LICENCE for licence details.
 */

/**
 * Converts the given unicode codepoint (0 - 0xffff) to utf-8
 * and stores the result at 'p'.
 * 
 * Returns the number of utf-8 characters (1-3).
 */
int utf8_fromunicode(char *p, unsigned short uc);

#ifndef JIM_UTF8
#include <ctype.h>

/* No utf-8 support. 1 byte = 1 char */
#define utf8_strlen(S, B) (B) < 0 ? strlen(S) : (B)
#define utf8_tounicode(S, CP) (*(CP) = *(S), 1)
#define utf8_upper(C) toupper(C)
#define utf8_lower(C) tolower(C)
#define utf8_index(C, I) (I)
#define utf8_charlen(C) 1
#define utf8_prev_len(S, L) 1

#else
/**
 * Returns the length of the utf-8 sequence starting with 'c'.
 * 
 * Returns 1-4, or -1 if this is not a valid start byte.
 *
 * Note that charlen=4 is not supported by the rest of the API.
 */
int utf8_charlen(int c);

/**
 * Returns the number of characters in the utf-8 
 * string of the given byte length.
 *
 * Any bytes which are not part of an valid utf-8
 * sequence are treated as individual characters.
 *
 * The string *must* be null terminated.
 *
 * Does not support unicode code points > \uffff
 */
int utf8_strlen(const char *str, int bytelen);

/**
 * Returns the byte index of the given character in the utf-8 string.
 * 
 * The string *must* be null terminated.
 *
 * This will return the byte length of a utf-8 string
 * if given the char length.
 */
int utf8_index(const char *str, int charindex);

/**
 * Returns the unicode codepoint corresponding to the
 * utf-8 sequence 'str'.
 * 
 * Stores the result in *uc and returns the number of bytes
 * consumed.
 *
 * If 'str' is null terminated, then an invalid utf-8 sequence
 * at the end of the string will be returned as individual bytes.
 *
 * If it is not null terminated, the length *must* be checked first.
 *
 * Does not support unicode code points > \uffff
 */
int utf8_tounicode(const char *str, int *uc);

/**
 * Returns the number of bytes before 'str' that the previous
 * utf-8 character sequence starts (which may be the middle of a sequence).
 *
 * Looks back at most 'len' bytes backwards, which must be > 0.
 * If no start char is found, returns -len
 */
int utf8_prev_len(const char *str, int len);

/**
 * Returns the upper-case variant of the given unicode codepoint.
 *
 * Does not support unicode code points > \uffff
 */
int utf8_upper(int uc);

/**
 * Returns the lower-case variant of the given unicode codepoint.
 *
 * NOTE: Use utf8_upper() in preference for case-insensitive matching.
 *
 * Does not support unicode code points > \uffff
 */
int utf8_lower(int uc);

#endif

#endif
/* Jim - A small embeddable Tcl interpreter
 *
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright 2005 Clemens Hintze <c.hintze@gmx.net>
 * Copyright 2005 patthoyts - Pat Thoyts <patthoyts@users.sf.net> 
 * Copyright 2008 oharboe - �yvind Harboe - oyvind.harboe@zylin.com
 * Copyright 2008 Andrew Lunn <andrew@lunn.ch>
 * Copyright 2008 Duane Ellis <openocd@duaneellis.com>
 * Copyright 2008 Uwe Klein <uklein@klein-messgeraete.de>
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
 *--- Inline Header File Documentation --- 
 *    [By Duane Ellis, openocd@duaneellis.com, 8/18/8]
 *
 * Belief is "Jim" would greatly benifit if Jim Internals where
 * documented in some way - form whatever, and perhaps - the package:
 * 'doxygen' is the correct approach to do that.
 *
 *   Details, see: http://www.stack.nl/~dimitri/doxygen/
 *
 * To that end please follow these guide lines:
 *
 *    (A) Document the PUBLIC api in the .H file.
 *
 *    (B) Document JIM Internals, in the .C file.
 *
 *    (C) Remember JIM is embedded in other packages, to that end do
 *    not assume that your way of documenting is the right way, Jim's
 *    public documentation should be agnostic, such that it is some
 *    what agreeable with the "package" that is embedding JIM inside
 *    of it's own doxygen documentation.
 *
 *    (D) Use minimal Doxygen tags.
 *
 * This will be an "ongoing work in progress" for some time.
 **/

#ifndef __JIM__H
#define __JIM__H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <limits.h>
#include <stdio.h>  /* for the FILE typedef definition */
#include <stdlib.h> /* In order to export the Jim_Free() macro */
#include <stdarg.h> /* In order to get type va_list */

/* -----------------------------------------------------------------------------
 * System configuration
 * autoconf (configure) will set these
 * ---------------------------------------------------------------------------*/

#ifndef HAVE_NO_AUTOCONF
#endif

/* -----------------------------------------------------------------------------
 * Compiler specific fixes.
 * ---------------------------------------------------------------------------*/

/* Long Long type and related issues */
#ifndef jim_wide
#  ifdef HAVE_LONG_LONG
#    define jim_wide long long
#    ifndef LLONG_MAX
#      define LLONG_MAX    9223372036854775807LL
#    endif
#    ifndef LLONG_MIN
#      define LLONG_MIN    (-LLONG_MAX - 1LL)
#    endif
#    define JIM_WIDE_MIN LLONG_MIN
#    define JIM_WIDE_MAX LLONG_MAX
#  else
#    define jim_wide long
#    define JIM_WIDE_MIN LONG_MIN
#    define JIM_WIDE_MAX LONG_MAX
#  endif

/* -----------------------------------------------------------------------------
 * LIBC specific fixes
 * ---------------------------------------------------------------------------*/

#  ifdef HAVE_LONG_LONG
#    define JIM_WIDE_MODIFIER "lld"
#  else
#    define JIM_WIDE_MODIFIER "ld"
#    define strtoull strtoul
#  endif
#endif

#define UCHAR(c) ((unsigned char)(c))

/* -----------------------------------------------------------------------------
 * Exported defines
 * ---------------------------------------------------------------------------*/

/* Jim version numbering: every version of jim is marked with a
 * successive integer number. This is version 0. The first
 * stable version will be 1, then 2, 3, and so on. */
#define JIM_VERSION 71

#define JIM_OK 0
#define JIM_ERR 1
#define JIM_RETURN 2
#define JIM_BREAK 3
#define JIM_CONTINUE 4
#define JIM_SIGNAL 5
#define JIM_EXIT 6
/* The following are internal codes and should never been seen/used */
#define JIM_EVAL 7

#define JIM_MAX_NESTING_DEPTH 1000 /* default max nesting depth */

/* Some function get an integer argument with flags to change
 * the behaviour. */
#define JIM_NONE 0    /* no flags set */
#define JIM_ERRMSG 1    /* set an error message in the interpreter. */

#define JIM_UNSHARED 4 /* Flag to Jim_GetVariable() */

/* Flags for Jim_SubstObj() */
#define JIM_SUBST_NOVAR 1 /* don't perform variables substitutions */
#define JIM_SUBST_NOCMD 2 /* don't perform command substitutions */
#define JIM_SUBST_NOESC 4 /* don't perform escapes substitutions */
#define JIM_SUBST_FLAG 128 /* flag to indicate that this is a real substition object */

/* Unused arguments generate annoying warnings... */
#define JIM_NOTUSED(V) ((void) V)

/* Flags for Jim_GetEnum() */
#define JIM_ENUM_ABBREV 2    /* Allow unambiguous abbreviation */

/* Flags used by API calls getting a 'nocase' argument. */
#define JIM_CASESENS    0   /* case sensitive */
#define JIM_NOCASE      1   /* no case */

/* Filesystem related */
#define JIM_PATH_LEN 1024

/* Newline, some embedded system may need -DJIM_CRLF */
#ifdef JIM_CRLF
#define JIM_NL "\r\n"
#else
#define JIM_NL "\n"
#endif

#define JIM_LIBPATH "auto_path"
#define JIM_INTERACTIVE "tcl_interactive"

/* -----------------------------------------------------------------------------
 * Stack
 * ---------------------------------------------------------------------------*/

typedef struct Jim_Stack {
    int len;
    int maxlen;
    void **vector;
} Jim_Stack;

/* -----------------------------------------------------------------------------
 * Hash table
 * ---------------------------------------------------------------------------*/

typedef struct Jim_HashEntry {
    const void *key;
    union {
        void *val;
        int intval;
    } u;
    struct Jim_HashEntry *next;
} Jim_HashEntry;

typedef struct Jim_HashTableType {
    unsigned int (*hashFunction)(const void *key);
    const void *(*keyDup)(void *privdata, const void *key);
    void *(*valDup)(void *privdata, const void *obj);
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
    void (*keyDestructor)(void *privdata, const void *key);
    void (*valDestructor)(void *privdata, void *obj);
} Jim_HashTableType;

typedef struct Jim_HashTable {
    Jim_HashEntry **table;
    const Jim_HashTableType *type;
    unsigned int size;
    unsigned int sizemask;
    unsigned int used;
    unsigned int collisions;
    void *privdata;
} Jim_HashTable;

typedef struct Jim_HashTableIterator {
    Jim_HashTable *ht;
    int index;
    Jim_HashEntry *entry, *nextEntry;
} Jim_HashTableIterator;

/* This is the initial size of every hash table */
#define JIM_HT_INITIAL_SIZE     16

/* ------------------------------- Macros ------------------------------------*/
#define Jim_FreeEntryVal(ht, entry) \
    if ((ht)->type->valDestructor) \
        (ht)->type->valDestructor((ht)->privdata, (entry)->u.val)

#define Jim_SetHashVal(ht, entry, _val_) do { \
    if ((ht)->type->valDup) \
        entry->u.val = (ht)->type->valDup((ht)->privdata, _val_); \
    else \
        entry->u.val = (_val_); \
} while(0)

#define Jim_FreeEntryKey(ht, entry) \
    if ((ht)->type->keyDestructor) \
        (ht)->type->keyDestructor((ht)->privdata, (entry)->key)

#define Jim_SetHashKey(ht, entry, _key_) do { \
    if ((ht)->type->keyDup) \
        entry->key = (ht)->type->keyDup((ht)->privdata, _key_); \
    else \
        entry->key = (_key_); \
} while(0)

#define Jim_CompareHashKeys(ht, key1, key2) \
    (((ht)->type->keyCompare) ? \
        (ht)->type->keyCompare((ht)->privdata, key1, key2) : \
        (key1) == (key2))

#define Jim_HashKey(ht, key) (ht)->type->hashFunction(key)

#define Jim_GetHashEntryKey(he) ((he)->key)
#define Jim_GetHashEntryVal(he) ((he)->val)
#define Jim_GetHashTableCollisions(ht) ((ht)->collisions)
#define Jim_GetHashTableSize(ht) ((ht)->size)
#define Jim_GetHashTableUsed(ht) ((ht)->used)

/* -----------------------------------------------------------------------------
 * Jim_Obj structure
 * ---------------------------------------------------------------------------*/

/* -----------------------------------------------------------------------------
 * Jim object. This is mostly the same as Tcl_Obj itself,
 * with the addition of the 'prev' and 'next' pointers.
 * In Jim all the objects are stored into a linked list for GC purposes,
 * so that it's possible to access every object living in a given interpreter
 * sequentially. When an object is freed, it's moved into a different
 * linked list, used as object pool.
 *
 * The refcount of a freed object is always -1.
 * ---------------------------------------------------------------------------*/
typedef struct Jim_Obj {
    int refCount; /* reference count */
    char *bytes; /* string representation buffer. NULL = no string repr. */
    int length; /* number of bytes in 'bytes', not including the numterm. */
    const struct Jim_ObjType *typePtr; /* object type. */
    /* Internal representation union */
    union {
        /* integer number type */
        jim_wide wideValue;
        /* hashed object type value */
        int hashValue;
        /* index type */
        int indexValue;
        /* return code type */
        int returnCode;
        /* double number type */
        double doubleValue;
        /* Generic pointer */
        void *ptr;
        /* Generic two pointers value */
        struct {
            void *ptr1;
            void *ptr2;
        } twoPtrValue;
        /* Variable object */
        struct {
            unsigned jim_wide callFrameId;
            struct Jim_Var *varPtr;
        } varValue;
        /* Command object */
        struct {
            unsigned jim_wide procEpoch;
            struct Jim_Cmd *cmdPtr;
        } cmdValue;
        /* List object */
        struct {
            struct Jim_Obj **ele;    /* Elements vector */
            int len;        /* Length */
            int maxLen;        /* Allocated 'ele' length */
        } listValue;
        /* String type */
        struct {
            int maxLength;
            int charLength;     /* utf-8 char length. -1 if unknown */
        } strValue;
        /* Reference type */
        struct {
            jim_wide id;
            struct Jim_Reference *refPtr;
        } refValue;
        /* Source type */
        struct {
            const char *fileName;
            int lineNumber;
        } sourceValue;
        /* Dict substitution type */
        struct {
            struct Jim_Obj *varNameObjPtr;
            struct Jim_Obj *indexObjPtr;
        } dictSubstValue;
        /* tagged binary type */
        struct {
            unsigned char *data;
            size_t         len;
        } binaryValue;
        /* Regular expression pattern */
        struct {
            unsigned flags;
            void *compre;       /* really an allocated (regex_t *) */
        } regexpValue;
        struct {
            int line;
            int argc;
        } scriptLineValue;
    } internalRep;
    /* This are 8 or 16 bytes more for every object
     * but this is required for efficient garbage collection
     * of Jim references. */
    struct Jim_Obj *prevObjPtr; /* pointer to the prev object. */
    struct Jim_Obj *nextObjPtr; /* pointer to the next object. */
} Jim_Obj;

/* Jim_Obj related macros */
#define Jim_IncrRefCount(objPtr) \
    ++(objPtr)->refCount
#define Jim_DecrRefCount(interp, objPtr) \
    if (--(objPtr)->refCount <= 0) Jim_FreeObj(interp, objPtr)
#define Jim_IsShared(objPtr) \
    ((objPtr)->refCount > 1)

/* This macro is used when we allocate a new object using
 * Jim_New...Obj(), but for some error we need to destroy it.
 * Instead to use Jim_IncrRefCount() + Jim_DecrRefCount() we
 * can just call Jim_FreeNewObj. To call Jim_Free directly
 * seems too raw, the object handling may change and we want
 * that Jim_FreeNewObj() can be called only against objects
 * that are belived to have refcount == 0. */
#define Jim_FreeNewObj Jim_FreeObj

/* Free the internal representation of the object. */
#define Jim_FreeIntRep(i,o) \
    if ((o)->typePtr && (o)->typePtr->freeIntRepProc) \
        (o)->typePtr->freeIntRepProc(i, o)

/* Get the internal representation pointer */
#define Jim_GetIntRepPtr(o) (o)->internalRep.ptr

/* Set the internal representation pointer */
#define Jim_SetIntRepPtr(o, p) \
    (o)->internalRep.ptr = (p)

/* The object type structure.
 * There are four methods.
 *
 * - FreeIntRep is used to free the internal representation of the object.
 *   Can be NULL if there is nothing to free.
 * - DupIntRep is used to duplicate the internal representation of the object.
 *   If NULL, when an object is duplicated, the internalRep union is
 *   directly copied from an object to another.
 *   Note that it's up to the caller to free the old internal repr of the
 *   object before to call the Dup method.
 * - UpdateString is used to create the string from the internal repr.
 * - setFromAny is used to convert the current object into one of this type.
 */

struct Jim_Interp;

typedef void (Jim_FreeInternalRepProc)(struct Jim_Interp *interp,
        struct Jim_Obj *objPtr);
typedef void (Jim_DupInternalRepProc)(struct Jim_Interp *interp,
        struct Jim_Obj *srcPtr, Jim_Obj *dupPtr);
typedef void (Jim_UpdateStringProc)(struct Jim_Obj *objPtr);
    
typedef struct Jim_ObjType {
    const char *name; /* The name of the type. */
    Jim_FreeInternalRepProc *freeIntRepProc;
    Jim_DupInternalRepProc *dupIntRepProc;
    Jim_UpdateStringProc *updateStringProc;
    int flags;
} Jim_ObjType;

/* Jim_ObjType flags */
#define JIM_TYPE_NONE 0        /* No flags */
#define JIM_TYPE_REFERENCES 1    /* The object may contain referneces. */

/* Starting from 1 << 20 flags are reserved for private uses of
 * different calls. This way the same 'flags' argument may be used
 * to pass both global flags and private flags. */
#define JIM_PRIV_FLAG_SHIFT 20

/* -----------------------------------------------------------------------------
 * Call frame, vars, commands structures
 * ---------------------------------------------------------------------------*/

/* Call frame */
typedef struct Jim_CallFrame {
    unsigned jim_wide id; /* Call Frame ID. Used for caching. */
    int level; /* Level of this call frame. 0 = global */
    struct Jim_HashTable vars; /* Where local vars are stored */
    struct Jim_HashTable *staticVars; /* pointer to procedure static vars */
    struct Jim_CallFrame *parentCallFrame;
    Jim_Obj *const *argv; /* object vector of the current procedure call. */
    int argc; /* number of args of the current procedure call. */
    Jim_Obj *procArgsObjPtr; /* arglist object of the running procedure */
    Jim_Obj *procBodyObjPtr; /* body object of the running procedure */
    struct Jim_CallFrame *nextFramePtr;
    const char *filename;       /* file and line of caller of this proc (if available) */
    int line;
} Jim_CallFrame;

/* The var structure. It just holds the pointer of the referenced
 * object. If linkFramePtr is not NULL the variable is a link
 * to a variable of name store on objPtr living on the given callframe
 * (this happens when the [global] or [upvar] command is used).
 * The interp in order to always know how to free the Jim_Obj associated
 * with a given variable because In Jim objects memory managment is
 * bound to interpreters. */
typedef struct Jim_Var {
    Jim_Obj *objPtr;
    struct Jim_CallFrame *linkFramePtr;
} Jim_Var;

/* The cmd structure. */
typedef int (*Jim_CmdProc)(struct Jim_Interp *interp, int argc,
    Jim_Obj *const *argv);
typedef void (*Jim_DelCmdProc)(struct Jim_Interp *interp, void *privData);



/* A command is implemented in C if funcPtr is != NULL, otherwise
 * it's a Tcl procedure with the arglist and body represented by the
 * two objects referenced by arglistObjPtr and bodyoObjPtr. */
typedef struct Jim_Cmd {
    int inUse;           /* Reference count */
    int isproc;          /* Is this a procedure? */
    union {
        struct {
            /* native (C) command */
            Jim_CmdProc cmdProc; /* The command implementation */
            Jim_DelCmdProc delProc; /* Called when the command is deleted if != NULL */
            void *privData; /* command-private data available via Jim_CmdPrivData() */
        } native;
        struct {
            /* Tcl procedure */
            Jim_Obj *argListObjPtr;
            Jim_Obj *bodyObjPtr;
            Jim_HashTable *staticVars;  /* Static vars hash table. NULL if no statics. */
            struct Jim_Cmd *prevCmd;    /* Previous command defn if proc created 'local' */
            int argListLen;             /* Length of argListObjPtr */
            int reqArity;               /* Number of required parameters */
            int optArity;               /* Number of optional parameters */
            int argsPos;                /* Position of 'args', if specified, or -1 */
            int upcall;                 /* True if proc is currently in upcall */
            struct Jim_ProcArg {
                Jim_Obj *nameObjPtr;    /* Name of this arg */
                Jim_Obj *defaultObjPtr; /* Default value, (or rename for $args) */
            } *arglist;
        } proc;
    } u;
} Jim_Cmd;

/* Pseudo Random Number Generator State structure */
typedef struct Jim_PrngState {
    unsigned char sbox[256];
    unsigned int i, j;
} Jim_PrngState;

/* -----------------------------------------------------------------------------
 * Jim interpreter structure.
 * Fields similar to the real Tcl interpreter structure have the same names.
 * ---------------------------------------------------------------------------*/
typedef struct Jim_Interp {
    Jim_Obj *result; /* object returned by the last command called. */
    int errorLine; /* Error line where an error occurred. */
    char *errorFileName; /* Error file where an error occurred. */
    int addStackTrace; /* > 0 If a level should be added to the stack trace */
    int maxNestingDepth; /* Used for infinite loop detection. */
    int returnCode; /* Completion code to return on JIM_RETURN. */
    int returnLevel; /* Current level of 'return -level' */
    int exitCode; /* Code to return to the OS on JIM_EXIT. */
    long id; /* Hold unique id for various purposes */
    int signal_level; /* A nesting level of catch -signal */
    jim_wide sigmask;  /* Bit mask of caught signals, or 0 if none */
    int (*signal_set_result)(struct Jim_Interp *interp, jim_wide sigmask); /* Set a result for the sigmask */
    Jim_CallFrame *framePtr; /* Pointer to the current call frame */
    Jim_CallFrame *topFramePtr; /* toplevel/global frame pointer. */
    struct Jim_HashTable commands; /* Commands hash table */
    unsigned jim_wide procEpoch; /* Incremented every time the result
                of procedures names lookup caching
                may no longer be valid. */
    unsigned jim_wide callFrameEpoch; /* Incremented every time a new
                callframe is created. This id is used for the
                'ID' field contained in the Jim_CallFrame
                structure. */
    int local; /* If 'local' is in effect, newly defined procs keep a reference to the old defn */ 
    Jim_Obj *liveList; /* Linked list of all the live objects. */
    Jim_Obj *freeList; /* Linked list of all the unused objects. */
    Jim_Obj *currentScriptObj; /* Script currently in execution. */
    Jim_Obj *emptyObj; /* Shared empty string object. */
    Jim_Obj *trueObj; /* Shared true int object. */
    Jim_Obj *falseObj; /* Shared false int object. */
    unsigned jim_wide referenceNextId; /* Next id for reference. */
    struct Jim_HashTable references; /* References hash table. */
    jim_wide lastCollectId; /* reference max Id of the last GC
                execution. It's set to -1 while the collection
                is running as sentinel to avoid to recursive
                calls via the [collect] command inside
                finalizers. */
    time_t lastCollectTime; /* unix time of the last GC execution */
    struct Jim_HashTable sharedStrings; /* Shared Strings hash table */
    Jim_Obj *stackTrace; /* Stack trace object. */
    Jim_Obj *errorProc; /* Name of last procedure which returned an error */
    Jim_Obj *unknown; /* Unknown command cache */
    int unknown_called; /* The unknown command has been invoked */
    int errorFlag; /* Set if an error occurred during execution. */
    void *cmdPrivData; /* Used to pass the private data pointer to
                  a command. It is set to what the user specified
                  via Jim_CreateCommand(). */

    struct Jim_CallFrame *freeFramesList; /* list of CallFrame structures. */
    struct Jim_HashTable assocData; /* per-interp storage for use by packages */
    Jim_PrngState *prngState; /* per interpreter Random Number Gen. state. */
    struct Jim_HashTable packages; /* Provided packages hash table */
    Jim_Stack *localProcs; /* procs to be destroyed on end of evaluation */
    Jim_Stack *loadHandles; /* handles of loaded modules [load] */
} Jim_Interp;

/* Currently provided as macro that performs the increment.
 * At some point may be a real function doing more work.
 * The proc epoch is used in order to know when a command lookup
 * cached can no longer considered valid. */
#define Jim_InterpIncrProcEpoch(i) (i)->procEpoch++
#define Jim_SetResultString(i,s,l) Jim_SetResult(i, Jim_NewStringObj(i,s,l))
#define Jim_SetResultInt(i,intval) Jim_SetResult(i, Jim_NewIntObj(i,intval))
/* Note: Using trueObj and falseObj here makes some things slower...*/
#define Jim_SetResultBool(i,b) Jim_SetResultInt(i, b)
#define Jim_SetEmptyResult(i) Jim_SetResult(i, (i)->emptyObj)
#define Jim_GetResult(i) ((i)->result)
#define Jim_CmdPrivData(i) ((i)->cmdPrivData)
#define Jim_String(o) Jim_GetString((o), NULL)

/* Note that 'o' is expanded only one time inside this macro,
 * so it's safe to use side effects. */
#define Jim_SetResult(i,o) do {     \
    Jim_Obj *_resultObjPtr_ = (o);    \
    Jim_IncrRefCount(_resultObjPtr_); \
    Jim_DecrRefCount(i,(i)->result);  \
    (i)->result = _resultObjPtr_;     \
} while(0)

/* Use this for filehandles, etc. which need a unique id */
#define Jim_GetId(i) (++(i)->id)

/* Reference structure. The interpreter pointer is held within privdata member in HashTable */
#define JIM_REFERENCE_TAGLEN 7 /* The tag is fixed-length, because the reference
                                  string representation must be fixed length. */
typedef struct Jim_Reference {
    Jim_Obj *objPtr;
    Jim_Obj *finalizerCmdNamePtr;
    char tag[JIM_REFERENCE_TAGLEN+1];
} Jim_Reference;

/* -----------------------------------------------------------------------------
 * Exported API prototypes.
 * ---------------------------------------------------------------------------*/

/* Macros that are common for extensions and core. */
#define Jim_NewEmptyStringObj(i) Jim_NewStringObj(i, "", 0)

/* The core includes real prototypes, extensions instead
 * include a global function pointer for every function exported.
 * Once the extension calls Jim_InitExtension(), the global
 * functon pointers are set to the value of the STUB table
 * contained in the Jim_Interp structure.
 *
 * This makes Jim able to load extensions even if it is statically
 * linked itself, and to load extensions compiled with different
 * versions of Jim (as long as the API is still compatible.) */

/* Macros are common for core and extensions */
#define Jim_FreeHashTableIterator(iter) Jim_Free(iter)

#define JIM_EXPORT

/* Memory allocation */
JIM_EXPORT void *Jim_Alloc (int size);
JIM_EXPORT void *Jim_Realloc(void *ptr, int size);
JIM_EXPORT void Jim_Free (void *ptr);
JIM_EXPORT char * Jim_StrDup (const char *s);
JIM_EXPORT char *Jim_StrDupLen(const char *s, int l);

/* environment */
JIM_EXPORT char **Jim_GetEnviron(void);
JIM_EXPORT void Jim_SetEnviron(char **env);

/* evaluation */
JIM_EXPORT int Jim_Eval(Jim_Interp *interp, const char *script);
/* in C code, you can do this and get better error messages */
/*   Jim_Eval_Named( interp, "some tcl commands", __FILE__, __LINE__ ); */
JIM_EXPORT int Jim_Eval_Named(Jim_Interp *interp, const char *script,const char *filename, int lineno);
JIM_EXPORT int Jim_EvalGlobal(Jim_Interp *interp, const char *script);
JIM_EXPORT int Jim_EvalFile(Jim_Interp *interp, const char *filename);
JIM_EXPORT int Jim_EvalFileGlobal(Jim_Interp *interp, const char *filename);
JIM_EXPORT int Jim_EvalObj (Jim_Interp *interp, Jim_Obj *scriptObjPtr);
JIM_EXPORT int Jim_EvalObjVector (Jim_Interp *interp, int objc,
        Jim_Obj *const *objv);
JIM_EXPORT int Jim_EvalObjPrefix(Jim_Interp *interp, const char *prefix,
        int objc, Jim_Obj *const *objv);
JIM_EXPORT int Jim_SubstObj (Jim_Interp *interp, Jim_Obj *substObjPtr,
        Jim_Obj **resObjPtrPtr, int flags);

/* stack */
JIM_EXPORT void Jim_InitStack(Jim_Stack *stack);
JIM_EXPORT void Jim_FreeStack(Jim_Stack *stack);
JIM_EXPORT int Jim_StackLen(Jim_Stack *stack);
JIM_EXPORT void Jim_StackPush(Jim_Stack *stack, void *element);
JIM_EXPORT void * Jim_StackPop(Jim_Stack *stack);
JIM_EXPORT void * Jim_StackPeek(Jim_Stack *stack);
JIM_EXPORT void Jim_FreeStackElements(Jim_Stack *stack, void (*freeFunc)(void *ptr));

/* hash table */
JIM_EXPORT int Jim_InitHashTable (Jim_HashTable *ht,
        const Jim_HashTableType *type, void *privdata);
JIM_EXPORT int Jim_ExpandHashTable (Jim_HashTable *ht,
        unsigned int size);
JIM_EXPORT int Jim_AddHashEntry (Jim_HashTable *ht, const void *key,
        void *val);
JIM_EXPORT int Jim_ReplaceHashEntry (Jim_HashTable *ht,
        const void *key, void *val);
JIM_EXPORT int Jim_DeleteHashEntry (Jim_HashTable *ht,
        const void *key);
JIM_EXPORT int Jim_FreeHashTable (Jim_HashTable *ht);
JIM_EXPORT Jim_HashEntry * Jim_FindHashEntry (Jim_HashTable *ht,
        const void *key);
JIM_EXPORT int Jim_ResizeHashTable (Jim_HashTable *ht);
JIM_EXPORT Jim_HashTableIterator *Jim_GetHashTableIterator
        (Jim_HashTable *ht);
JIM_EXPORT Jim_HashEntry * Jim_NextHashEntry
        (Jim_HashTableIterator *iter);

/* objects */
JIM_EXPORT Jim_Obj * Jim_NewObj (Jim_Interp *interp);
JIM_EXPORT void Jim_FreeObj (Jim_Interp *interp, Jim_Obj *objPtr);
JIM_EXPORT void Jim_InvalidateStringRep (Jim_Obj *objPtr);
JIM_EXPORT void Jim_InitStringRep (Jim_Obj *objPtr, const char *bytes,
        int length);
JIM_EXPORT Jim_Obj * Jim_DuplicateObj (Jim_Interp *interp,
        Jim_Obj *objPtr);
JIM_EXPORT const char * Jim_GetString(Jim_Obj *objPtr,
        int *lenPtr);
JIM_EXPORT int Jim_Length(Jim_Obj *objPtr);

/* string object */
JIM_EXPORT Jim_Obj * Jim_NewStringObj (Jim_Interp *interp,
        const char *s, int len);
JIM_EXPORT Jim_Obj *Jim_NewStringObjUtf8(Jim_Interp *interp,
        const char *s, int charlen);
JIM_EXPORT Jim_Obj * Jim_NewStringObjNoAlloc (Jim_Interp *interp,
        char *s, int len);
JIM_EXPORT void Jim_AppendString (Jim_Interp *interp, Jim_Obj *objPtr,
        const char *str, int len);
JIM_EXPORT void Jim_AppendObj (Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj *appendObjPtr);
JIM_EXPORT void Jim_AppendStrings (Jim_Interp *interp,
        Jim_Obj *objPtr, ...);
JIM_EXPORT int Jim_StringEqObj(Jim_Obj *aObjPtr, Jim_Obj *bObjPtr);
JIM_EXPORT int Jim_StringMatchObj (Jim_Interp *interp, Jim_Obj *patternObjPtr,
        Jim_Obj *objPtr, int nocase);
JIM_EXPORT Jim_Obj * Jim_StringRangeObj (Jim_Interp *interp,
        Jim_Obj *strObjPtr, Jim_Obj *firstObjPtr,
        Jim_Obj *lastObjPtr);
JIM_EXPORT Jim_Obj * Jim_FormatString (Jim_Interp *interp,
        Jim_Obj *fmtObjPtr, int objc, Jim_Obj *const *objv);
JIM_EXPORT Jim_Obj * Jim_ScanString (Jim_Interp *interp, Jim_Obj *strObjPtr,
        Jim_Obj *fmtObjPtr, int flags);
JIM_EXPORT int Jim_CompareStringImmediate (Jim_Interp *interp,
        Jim_Obj *objPtr, const char *str);
JIM_EXPORT int Jim_StringCompareObj(Jim_Interp *interp, Jim_Obj *firstObjPtr,
        Jim_Obj *secondObjPtr, int nocase);
JIM_EXPORT int Jim_Utf8Length(Jim_Interp *interp, Jim_Obj *objPtr);

/* reference object */
JIM_EXPORT Jim_Obj * Jim_NewReference (Jim_Interp *interp,
        Jim_Obj *objPtr, Jim_Obj *tagPtr, Jim_Obj *cmdNamePtr);
JIM_EXPORT Jim_Reference * Jim_GetReference (Jim_Interp *interp,
        Jim_Obj *objPtr);
JIM_EXPORT int Jim_SetFinalizer (Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *cmdNamePtr);
JIM_EXPORT int Jim_GetFinalizer (Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj **cmdNamePtrPtr);

/* interpreter */
JIM_EXPORT Jim_Interp * Jim_CreateInterp (void);
JIM_EXPORT void Jim_FreeInterp (Jim_Interp *i);
JIM_EXPORT int Jim_GetExitCode (Jim_Interp *interp);
JIM_EXPORT const char *Jim_ReturnCode(int code);
JIM_EXPORT void Jim_SetResultFormatted(Jim_Interp *interp, const char *format, ...);

/* commands */
JIM_EXPORT void Jim_RegisterCoreCommands (Jim_Interp *interp);
JIM_EXPORT int Jim_CreateCommand (Jim_Interp *interp, 
        const char *cmdName, Jim_CmdProc cmdProc, void *privData,
         Jim_DelCmdProc delProc);
JIM_EXPORT int Jim_DeleteCommand (Jim_Interp *interp,
        const char *cmdName);
JIM_EXPORT int Jim_RenameCommand (Jim_Interp *interp, 
        const char *oldName, const char *newName);
JIM_EXPORT Jim_Cmd * Jim_GetCommand (Jim_Interp *interp,
        Jim_Obj *objPtr, int flags);
JIM_EXPORT int Jim_SetVariable (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, Jim_Obj *valObjPtr);
JIM_EXPORT int Jim_SetVariableStr (Jim_Interp *interp,
        const char *name, Jim_Obj *objPtr);
JIM_EXPORT int Jim_SetGlobalVariableStr (Jim_Interp *interp,
        const char *name, Jim_Obj *objPtr);
JIM_EXPORT int Jim_SetVariableStrWithStr (Jim_Interp *interp,
        const char *name, const char *val);
JIM_EXPORT int Jim_SetVariableLink (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, Jim_Obj *targetNameObjPtr,
        Jim_CallFrame *targetCallFrame);
JIM_EXPORT Jim_Obj * Jim_GetVariable (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, int flags);
JIM_EXPORT Jim_Obj * Jim_GetGlobalVariable (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, int flags);
JIM_EXPORT Jim_Obj * Jim_GetVariableStr (Jim_Interp *interp,
        const char *name, int flags);
JIM_EXPORT Jim_Obj * Jim_GetGlobalVariableStr (Jim_Interp *interp,
        const char *name, int flags);
JIM_EXPORT int Jim_UnsetVariable (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, int flags);

/* call frame */
JIM_EXPORT Jim_CallFrame *Jim_GetCallFrameByLevel(Jim_Interp *interp,
        Jim_Obj *levelObjPtr);

/* garbage collection */
JIM_EXPORT int Jim_Collect (Jim_Interp *interp);
JIM_EXPORT void Jim_CollectIfNeeded (Jim_Interp *interp);

/* index object */
JIM_EXPORT int Jim_GetIndex (Jim_Interp *interp, Jim_Obj *objPtr,
        int *indexPtr);

/* list object */
JIM_EXPORT Jim_Obj * Jim_NewListObj (Jim_Interp *interp,
        Jim_Obj *const *elements, int len);
JIM_EXPORT void Jim_ListInsertElements (Jim_Interp *interp,
        Jim_Obj *listPtr, int listindex, int objc, Jim_Obj *const *objVec);
JIM_EXPORT void Jim_ListAppendElement (Jim_Interp *interp,
        Jim_Obj *listPtr, Jim_Obj *objPtr);
JIM_EXPORT void Jim_ListAppendList (Jim_Interp *interp,
        Jim_Obj *listPtr, Jim_Obj *appendListPtr);
JIM_EXPORT int Jim_ListLength (Jim_Interp *interp, Jim_Obj *objPtr);
JIM_EXPORT int Jim_ListIndex (Jim_Interp *interp, Jim_Obj *listPrt,
        int listindex, Jim_Obj **objPtrPtr, int seterr);
JIM_EXPORT int Jim_SetListIndex (Jim_Interp *interp,
        Jim_Obj *varNamePtr, Jim_Obj *const *indexv, int indexc,
        Jim_Obj *newObjPtr);
JIM_EXPORT Jim_Obj * Jim_ConcatObj (Jim_Interp *interp, int objc,
        Jim_Obj *const *objv);

/* dict object */
JIM_EXPORT Jim_Obj * Jim_NewDictObj (Jim_Interp *interp,
        Jim_Obj *const *elements, int len);
JIM_EXPORT int Jim_DictKey (Jim_Interp *interp, Jim_Obj *dictPtr,
        Jim_Obj *keyPtr, Jim_Obj **objPtrPtr, int flags);
JIM_EXPORT int Jim_DictKeysVector (Jim_Interp *interp,
        Jim_Obj *dictPtr, Jim_Obj *const *keyv, int keyc,
        Jim_Obj **objPtrPtr, int flags);
JIM_EXPORT int Jim_SetDictKeysVector (Jim_Interp *interp,
        Jim_Obj *varNamePtr, Jim_Obj *const *keyv, int keyc,
        Jim_Obj *newObjPtr);
JIM_EXPORT int Jim_DictPairs(Jim_Interp *interp,
        Jim_Obj *dictPtr, Jim_Obj ***objPtrPtr, int *len);
JIM_EXPORT int Jim_DictAddElement(Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj *keyObjPtr, Jim_Obj *valueObjPtr);
JIM_EXPORT int Jim_DictKeys(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *patternObj);
JIM_EXPORT int Jim_DictSize(Jim_Interp *interp, Jim_Obj *objPtr);

/* return code object */
JIM_EXPORT int Jim_GetReturnCode (Jim_Interp *interp, Jim_Obj *objPtr,
        int *intPtr);

/* expression object */
JIM_EXPORT int Jim_EvalExpression (Jim_Interp *interp,
        Jim_Obj *exprObjPtr, Jim_Obj **exprResultPtrPtr);
JIM_EXPORT int Jim_GetBoolFromExpr (Jim_Interp *interp,
        Jim_Obj *exprObjPtr, int *boolPtr);

/* integer object */
JIM_EXPORT int Jim_GetWide (Jim_Interp *interp, Jim_Obj *objPtr,
        jim_wide *widePtr);
JIM_EXPORT int Jim_GetLong (Jim_Interp *interp, Jim_Obj *objPtr,
        long *longPtr);
#define Jim_NewWideObj  Jim_NewIntObj
JIM_EXPORT Jim_Obj * Jim_NewIntObj (Jim_Interp *interp,
        jim_wide wideValue);

/* double object */
JIM_EXPORT int Jim_GetDouble(Jim_Interp *interp, Jim_Obj *objPtr,
        double *doublePtr);
JIM_EXPORT void Jim_SetDouble(Jim_Interp *interp, Jim_Obj *objPtr,
        double doubleValue);
JIM_EXPORT Jim_Obj * Jim_NewDoubleObj(Jim_Interp *interp, double doubleValue);

/* shared strings */
JIM_EXPORT const char * Jim_GetSharedString (Jim_Interp *interp, 
        const char *str);
JIM_EXPORT void Jim_ReleaseSharedString (Jim_Interp *interp,
        const char *str);

/* commands utilities */
JIM_EXPORT void Jim_WrongNumArgs (Jim_Interp *interp, int argc,
        Jim_Obj *const *argv, const char *msg);
JIM_EXPORT int Jim_GetEnum (Jim_Interp *interp, Jim_Obj *objPtr,
        const char * const *tablePtr, int *indexPtr, const char *name, int flags);
JIM_EXPORT int Jim_ScriptIsComplete (const char *s, int len,
        char *stateCharPtr);
/**
 * Find a matching name in the array of the given length.
 * 
 * NULL entries are ignored.
 *
 * Returns the matching index if found, or -1 if not.
 */
JIM_EXPORT int Jim_FindByName(const char *name, const char * const array[], size_t len);

/* package utilities */
typedef void (Jim_InterpDeleteProc)(Jim_Interp *interp, void *data);
JIM_EXPORT void * Jim_GetAssocData(Jim_Interp *interp, const char *key);
JIM_EXPORT int Jim_SetAssocData(Jim_Interp *interp, const char *key,
        Jim_InterpDeleteProc *delProc, void *data);
JIM_EXPORT int Jim_DeleteAssocData(Jim_Interp *interp, const char *key);

/* Packages C API */
/* jim-package.c */
JIM_EXPORT int Jim_PackageProvide (Jim_Interp *interp,
        const char *name, const char *ver, int flags);
JIM_EXPORT int Jim_PackageRequire (Jim_Interp *interp,
        const char *name, int flags);

/* error messages */
JIM_EXPORT void Jim_MakeErrorMessage (Jim_Interp *interp);

/* interactive mode */
JIM_EXPORT int Jim_InteractivePrompt (Jim_Interp *interp);

/* Misc */
JIM_EXPORT int Jim_InitStaticExtensions(Jim_Interp *interp);
JIM_EXPORT int Jim_StringToWide(const char *str, jim_wide *widePtr, int base);

/* jim-load.c */
JIM_EXPORT int Jim_LoadLibrary(Jim_Interp *interp, const char *pathName);
JIM_EXPORT void Jim_FreeLoadHandles(Jim_Interp *interp);

/* jim-aio.c */
JIM_EXPORT FILE *Jim_AioFilehandle(Jim_Interp *interp, Jim_Obj *command);


/* type inspection - avoid where possible */
JIM_EXPORT int Jim_IsDict(Jim_Obj *objPtr);
JIM_EXPORT int Jim_IsList(Jim_Obj *objPtr);

#ifdef __cplusplus
}
#endif

#endif /* __JIM__H */

/*
 * Local Variables: ***
 * c-basic-offset: 4 ***
 * tab-width: 4 ***
 * End: ***
 */
/* Provides a common approach to implementing Tcl commands
 * which implement subcommands
 */
#ifndef JIM_SUBCMD_H
#define JIM_SUBCMD_H


#define JIM_MODFLAG_HIDDEN   0x0001		/* Don't show the subcommand in usage or commands */
#define JIM_MODFLAG_FULLARGV 0x0002		/* Subcmd proc gets called with full argv */

/* Custom flags start at 0x0100 */

/**
 * Returns JIM_OK if OK, JIM_ERR (etc.) on error, break, continue, etc.
 * Returns -1 if invalid args.
 */
typedef int tclmod_cmd_function(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

typedef struct {
	const char *cmd;				/* Name of the (sub)command */
	const char *args;				/* Textual description of allowed args */
	tclmod_cmd_function *function;	/* Function implementing the subcommand */
	short minargs;					/* Minimum required arguments */
	short maxargs;					/* Maximum allowed arguments or -1 if no limit */
	unsigned flags;					/* JIM_MODFLAG_... plus custom flags */
	const char *description;		/* Description of the subcommand */
} jim_subcmd_type;

/**
 * Looks up the appropriate subcommand in the given command table and return
 * the command function which implements the subcommand.
 * NULL will be returned and an appropriate error will be set if the subcommand or 
 * arguments are invalid.
 *
 * Typical usage is:
 *  {
 *    const jim_subcmd_type *ct = Jim_ParseSubCmd(interp, command_table, argc, argv);
 *
 *    return Jim_CallSubCmd(interp, ct, argc, argv);
 *  }
 *
 */
const jim_subcmd_type *
Jim_ParseSubCmd(Jim_Interp *interp, const jim_subcmd_type *command_table, int argc, Jim_Obj *const *argv);

/**
 * Parses the args against the given command table and executes the subcommand if found
 * or sets an appropriate error if the subcommand or arguments is invalid.
 *
 * Can be used directly with Jim_CreateCommand() where the ClientData is the command table.
 *
 * e.g. Jim_CreateCommand(interp, "mycmd", Jim_SubCmdProc, command_table, NULL);
 */
int Jim_SubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

/**
 * Invokes the given subcmd with the given args as returned
 * by Jim_ParseSubCmd()
 * 
 * If ct is NULL, returns JIM_ERR, leaving any message.
 * Otherwise invokes ct->function
 *
 * If ct->function returns -1, sets an error message and returns JIM_ERR.
 * Otherwise returns the result of ct->function.
 */
int Jim_CallSubCmd(Jim_Interp *interp, const jim_subcmd_type *ct, int argc, Jim_Obj *const *argv);

/**
 * Standard processing for a command.
 * 
 * This does the '-help' and '-usage' check and the number of args checks.
 * for a top level command against a single 'jim_subcmd_type' structure.
 *
 * Additionally, if command_table->function is set, it should point to a sub command table
 * and '-subhelp ?subcmd?', '-subusage' and '-subcommands' are then also recognised.
 * 
 * Returns 0 if user requested usage, -1 on arg error, 1 if OK to process.
 */
int
Jim_CheckCmdUsage(Jim_Interp *interp, const jim_subcmd_type *command_table, int argc, Jim_Obj *const *argv);

#endif
#ifndef JIMREGEXP_H
#define JIMREGEXP_H

#ifndef _JIMAUTOCONF_H
#error Need jimautoconf.h
#endif

#if defined(HAVE_REGCOMP) && !defined(JIM_REGEXP)
/* Use POSIX regex */
#include <regex.h>

#else

#include <stdlib.h>

/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 *
 * 11/04/02 (seiwald) - const-ing for string literals
 */

typedef struct {
	int rm_so;
	int rm_eo;
} regmatch_t;

/*
 * The "internal use only" fields in regexp.h are present to pass info from
 * compile to execute that permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 * regstart	char that must begin a match; '\0' if none obvious
 * reganch	is the match anchored (at beginning-of-line only)?
 * regmust	string (pointer into program) that match must include, or NULL
 * regmlen	length of regmust string
 *
 * Regstart and reganch permit very fast decisions on suitable starting points
 * for a match, cutting down the work a lot.  Regmust permits fast rejection
 * of lines that cannot possibly match.  The regmust tests are costly enough
 * that regcomp() supplies a regmust only if the r.e. contains something
 * potentially expensive (at present, the only such thing detected is * or +
 * at the start of the r.e., which can involve a lot of backup).  Regmlen is
 * supplied because the test in regexec() needs it and regcomp() is computing
 * it anyway.
 */

typedef struct regexp {
	/* -- public -- */
	int re_nsub;		/* number of parenthesized subexpressions */

	/* -- private -- */
	int cflags;			/* Flags used when compiling */
	int err;			/* Any error which occurred during compile */
	int regstart;		/* Internal use only. */
	int reganch;		/* Internal use only. */
	int regmust;		/* Internal use only. */
	int regmlen;		/* Internal use only. */
	int *program;		/* Allocated */

	/* working state - compile */
	const char *regparse;		/* Input-scan pointer. */
	int p;				/* Current output pos in program */
	int proglen;		/* Allocated program size */

	/* working state - exec */
	int eflags;				/* Flags used when executing */
	const char *start;		/* Initial string pointer. */
	const char *reginput;	/* Current input pointer. */
	const char *regbol;		/* Beginning of input, for ^ check. */

	/* Input to regexec() */
	regmatch_t *pmatch;		/* submatches will be stored here */
	int nmatch;				/* size of pmatch[] */
} regexp;

typedef regexp regex_t;

#define REG_EXTENDED 0
#define REG_NEWLINE 1
#define REG_ICASE 2

#define REG_NOTBOL 16

enum {
	REG_NOERROR,      /* Success.  */
	REG_NOMATCH,      /* Didn't find a match (for regexec).  */
	REG_BADPAT,		  /* >= REG_BADPAT is an error */
	REG_ERR_NULL_ARGUMENT,
	REG_ERR_UNKNOWN,
	REG_ERR_TOO_BIG,
	REG_ERR_NOMEM,
	REG_ERR_TOO_MANY_PAREN,
	REG_ERR_UNMATCHED_PAREN,
	REG_ERR_UNMATCHED_BRACES,
	REG_ERR_BAD_COUNT,
	REG_ERR_JUNK_ON_END,
	REG_ERR_OPERAND_COULD_BE_EMPTY,
	REG_ERR_NESTED_COUNT,
	REG_ERR_INTERNAL,
	REG_ERR_COUNT_FOLLOWS_NOTHING,
	REG_ERR_TRAILING_BACKSLASH,
	REG_ERR_CORRUPTED,
	REG_ERR_NULL_CHAR,
	REG_ERR_NUM
};

int regcomp(regex_t *preg, const char *regex, int cflags);
int regexec(regex_t  *preg,  const  char *string, size_t nmatch, regmatch_t pmatch[], int eflags);
size_t regerror(int errcode, const regex_t *preg, char *errbuf,  size_t errbuf_size);
void regfree(regex_t *preg);

#endif

#endif
int Jim_bootstrapInit(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "bootstrap", "1.0", JIM_ERRMSG))
		return JIM_ERR;

	return Jim_Eval_Named(interp, 
"\n"
"\n"
"proc package {args} {}\n"
,"bootstrap.tcl", 1);
}
int Jim_initjimshInit(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "initjimsh", "1.0", JIM_ERRMSG))
		return JIM_ERR;

	return Jim_Eval_Named(interp, 
"\n"
"\n"
"\n"
"proc _jimsh_init {} {\n"
"	rename _jimsh_init {}\n"
"\n"
"\n"
"	lappend p {*}[split [env JIMLIB {}] $::tcl_platform(pathSeparator)]\n"
"	lappend p {*}$::auto_path\n"
"	lappend p [file dirname [info nameofexecutable]]\n"
"	set ::auto_path $p\n"
"\n"
"	if {$::tcl_interactive && [env HOME {}] ne \"\"} {\n"
"		foreach src {.jimrc jimrc.tcl} {\n"
"			if {[file exists [env HOME]/$src]} {\n"
"				uplevel #0 source [env HOME]/$src\n"
"				break\n"
"			}\n"
"		}\n"
"	}\n"
"}\n"
"\n"
"if {$tcl_platform(platform) eq \"windows\"} {\n"
"	set jim_argv0 [string map {\\\\ /} $jim_argv0]\n"
"}\n"
"\n"
"_jimsh_init\n"
,"initjimsh.tcl", 1);
}
int Jim_globInit(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "glob", "1.0", JIM_ERRMSG))
		return JIM_ERR;

	return Jim_Eval_Named(interp, 
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"package require readdir\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"proc glob {args} {\n"
"\n"
"\n"
"\n"
"\n"
"	local proc glob.readdir_pattern {dir pattern} {\n"
"		set result {}\n"
"\n"
"\n"
"		if {$pattern in {. ..}} {\n"
"			return $pattern\n"
"		}\n"
"\n"
"\n"
"		if {[string match {*[*?]*} $pattern]} {\n"
"\n"
"			set files [readdir -nocomplain $dir]\n"
"		} elseif {[file isdir $dir] && [file exists $dir/$pattern]} {\n"
"			set files [list $pattern]\n"
"		} else {\n"
"			set files \"\"\n"
"		}\n"
"\n"
"		foreach name $files {\n"
"			if {[string match $pattern $name]} {\n"
"\n"
"				if {[string index $name 0] eq \".\" && [string index $pattern 0] ne \".\"} {\n"
"					continue\n"
"				}\n"
"				lappend result $name\n"
"			}\n"
"		}\n"
"\n"
"		return $result\n"
"	}\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"	proc glob.expandbraces {pattern} {\n"
"\n"
"\n"
"		if {[set fb [string first \"\\{\" $pattern]] < 0} {\n"
"			return $pattern\n"
"		}\n"
"		if {[set nb [string first \"\\}\" $pattern $fb]] < 0} {\n"
"			return $pattern\n"
"		}\n"
"		set before [string range $pattern 0 $fb-1]\n"
"		set braced [string range $pattern $fb+1 $nb-1]\n"
"		set after [string range $pattern $nb+1 end]\n"
"\n"
"		lmap part [split $braced ,] {\n"
"			set pat $before$part$after\n"
"		}\n"
"	}\n"
"\n"
"\n"
"	proc glob.glob {pattern} {\n"
"		set dir [file dirname $pattern]\n"
"		if {$dir eq $pattern} {\n"
"\n"
"			return [list $dir]\n"
"		}\n"
"\n"
"\n"
"		set dirlist [glob.glob $dir]\n"
"		set pattern [file tail $pattern]\n"
"\n"
"\n"
"		set result {}\n"
"		foreach dir $dirlist {\n"
"			set globdir $dir\n"
"			if {[string match \"*/\" $dir]} {\n"
"				set sep \"\"\n"
"			} elseif {$dir eq \".\"} {\n"
"				set globdir \"\"\n"
"				set sep \"\"\n"
"			} else {\n"
"				set sep /\n"
"			}\n"
"			foreach pat [glob.expandbraces $pattern] {\n"
"				foreach name [glob.readdir_pattern $dir $pat] {\n"
"					lappend result $globdir$sep$name\n"
"				}\n"
"			}\n"
"		}\n"
"		return $result\n"
"	}\n"
"\n"
"\n"
"	set nocomplain 0\n"
"\n"
"	if {[lindex $args 0] eq \"-nocomplain\"} {\n"
"		set nocomplain 1\n"
"		set args [lrange $args 1 end]\n"
"	}\n"
"\n"
"	set result {}\n"
"	foreach pattern $args {\n"
"		lappend result {*}[glob.glob $pattern]\n"
"	}\n"
"\n"
"	if {$nocomplain == 0 && [llength $result] == 0} {\n"
"		return -code error \"no files matched glob patterns\"\n"
"	}\n"
"\n"
"	return $result\n"
"}\n"
,"glob.tcl", 1);
}
int Jim_stdlibInit(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "stdlib", "1.0", JIM_ERRMSG))
		return JIM_ERR;

	return Jim_Eval_Named(interp, 
"\n"
"\n"
"\n"
"proc alias {name args} {\n"
"	set prefix $args\n"
"	proc $name args prefix {\n"
"		tailcall {*}$prefix {*}$args\n"
"	}\n"
"}\n"
"\n"
"\n"
"proc lambda {arglist args} {\n"
"	set name [ref {} function lambda.finalizer]\n"
"	tailcall proc $name $arglist {*}$args\n"
"}\n"
"\n"
"proc lambda.finalizer {name val} {\n"
"	rename $name {}\n"
"}\n"
"\n"
"\n"
"proc curry {args} {\n"
"	set prefix $args\n"
"	lambda args prefix {\n"
"		tailcall {*}$prefix {*}$args\n"
"	}\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"proc function {value} {\n"
"	return $value\n"
"}\n"
"\n"
"\n"
"proc lassign {list args} {\n"
"\n"
"	lappend list {}\n"
"	uplevel 1 [list foreach $args $list break]\n"
"	lrange $list [llength $args] end-1\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"proc stacktrace {} {\n"
"	set trace {}\n"
"	foreach level [range 1 [info level]] {\n"
"		lassign [info frame -$level] p f l\n"
"		lappend trace $p $f $l\n"
"	}\n"
"	return $trace\n"
"}\n"
"\n"
"\n"
"proc stackdump {stacktrace} {\n"
"	set result {}\n"
"	set count 0\n"
"	foreach {l f p} [lreverse $stacktrace] {\n"
"		if {$count} {\n"
"			append result \\n\n"
"		}\n"
"		incr count\n"
"		if {$p ne \"\"} {\n"
"			append result \"in procedure '$p' \"\n"
"			if {$f ne \"\"} {\n"
"				append result \"called \"\n"
"			}\n"
"		}\n"
"		if {$f ne \"\"} {\n"
"			append result \"at file \\\"$f\\\", line $l\"\n"
"		}\n"
"	}\n"
"	return $result\n"
"}\n"
"\n"
"\n"
"\n"
"proc errorInfo {msg {stacktrace \"\"}} {\n"
"	if {$stacktrace eq \"\"} {\n"
"		set stacktrace [info stacktrace]\n"
"	}\n"
"	lassign $stacktrace p f l\n"
"	if {$f ne \"\"} {\n"
"		set result \"Runtime Error: $f:$l: \"\n"
"	}\n"
"	append result \"$msg\\n\"\n"
"	append result [stackdump $stacktrace]\n"
"\n"
"\n"
"	string trim $result\n"
"}\n"
"\n"
"\n"
"\n"
"proc {info nameofexecutable} {} {\n"
"	if {[info exists ::jim_argv0]} {\n"
"		if {[string match \"*/*\" $::jim_argv0]} {\n"
"			return [file join [pwd] $::jim_argv0]\n"
"		}\n"
"		foreach path [split [env PATH \"\"] $::tcl_platform(pathSeparator)] {\n"
"			set exec [file join [pwd] $path $::jim_argv0]\n"
"			if {[file executable $exec]} {\n"
"				return $exec\n"
"			}\n"
"		}\n"
"	}\n"
"	return \"\"\n"
"}\n"
"\n"
"\n"
"proc {dict with} {dictVar args script} {\n"
"	upvar $dictVar dict\n"
"	set keys {}\n"
"	foreach {n v} [dict get $dict {*}$args] {\n"
"		upvar $n var_$n\n"
"		set var_$n $v\n"
"		lappend keys $n\n"
"	}\n"
"	catch {uplevel 1 $script} msg opts\n"
"	if {[info exists dict] && [dict exists $dict {*}$args]} {\n"
"		foreach n $keys {\n"
"			if {[info exists var_$n]} {\n"
"				dict set dict {*}$args $n [set var_$n]\n"
"			} else {\n"
"				dict unset dict {*}$args $n\n"
"			}\n"
"		}\n"
"	}\n"
"	return {*}$opts $msg\n"
"}\n"
"\n"
"\n"
"\n"
"proc {dict merge} {dict args} {\n"
"	foreach d $args {\n"
"\n"
"		dict size $d\n"
"		foreach {k v} $d {\n"
"			dict set dict $k $v\n"
"		}\n"
"	}\n"
"	return $dict\n"
"}\n"
,"stdlib.tcl", 1);
}
int Jim_tclcompatInit(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "tclcompat", "1.0", JIM_ERRMSG))
		return JIM_ERR;

	return Jim_Eval_Named(interp, 
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"set env [env]\n"
"\n"
"if {[info commands stdout] ne \"\"} {\n"
"\n"
"	foreach p {gets flush close eof seek tell} {\n"
"		proc $p {chan args} {p} {\n"
"			tailcall $chan $p {*}$args\n"
"		}\n"
"	}\n"
"	unset p\n"
"\n"
"\n"
"\n"
"	proc puts {{-nonewline {}} {chan stdout} msg} {\n"
"		if {${-nonewline} ni {-nonewline {}}} {\n"
"			tailcall ${-nonewline} puts $msg\n"
"		}\n"
"		tailcall $chan puts {*}${-nonewline} $msg\n"
"	}\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"	proc read {{-nonewline {}} chan} {\n"
"		if {${-nonewline} ni {-nonewline {}}} {\n"
"			tailcall ${-nonewline} read {*}${chan}\n"
"		}\n"
"		tailcall $chan read {*}${-nonewline}\n"
"	}\n"
"\n"
"	proc fconfigure {f args} {\n"
"		foreach {n v} $args {\n"
"			switch -glob -- $n {\n"
"				-bl* {\n"
"					$f ndelay $v\n"
"				}\n"
"				-bu* {\n"
"					$f buffering $v\n"
"				}\n"
"				-tr* {\n"
"\n"
"				}\n"
"				default {\n"
"					return -code error \"fconfigure: unknown option $n\"\n"
"				}\n"
"			}\n"
"		}\n"
"	}\n"
"}\n"
"\n"
"\n"
"proc case {var args} {\n"
"\n"
"	if {[lindex $args 0] eq \"in\"} {\n"
"		set args [lrange $args 1 end]\n"
"	}\n"
"\n"
"\n"
"	if {[llength $args] == 1} {\n"
"		set args [lindex $args 0]\n"
"	}\n"
"\n"
"\n"
"	if {[llength $args] % 2 != 0} {\n"
"		return -code error \"extra case pattern with no body\"\n"
"	}\n"
"\n"
"\n"
"	local proc case.checker {value pattern} {\n"
"		string match $pattern $value\n"
"	}\n"
"\n"
"	foreach {value action} $args {\n"
"		if {$value eq \"default\"} {\n"
"			set do_action $action\n"
"			continue\n"
"		} elseif {[lsearch -bool -command case.checker $value $var]} {\n"
"			set do_action $action\n"
"			break\n"
"		}\n"
"	}\n"
"\n"
"	if {[info exists do_action]} {\n"
"		set rc [catch [list uplevel 1 $do_action] result opts]\n"
"		if {$rc} {\n"
"			incr opts(-level)\n"
"		}\n"
"		return {*}$opts $result\n"
"	}\n"
"}\n"
"\n"
"\n"
"proc fileevent {args} {\n"
"	tailcall {*}$args\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"proc parray {arrayname {pattern *} {puts puts}} {\n"
"	upvar $arrayname a\n"
"\n"
"	set max 0\n"
"	foreach name [array names a $pattern]] {\n"
"		if {[string length $name] > $max} {\n"
"			set max [string length $name]\n"
"		}\n"
"	}\n"
"	incr max [string length $arrayname]\n"
"	incr max 2\n"
"	foreach name [lsort [array names a $pattern]] {\n"
"		$puts [format \"%-${max}s = %s\" $arrayname\\($name\\) $a($name)]\n"
"	}\n"
"}\n"
"\n"
"\n"
"proc {file copy} {{force {}} source target} {\n"
"	try {\n"
"		if {$force ni {{} -force}} {\n"
"			error \"bad option \\\"$force\\\": should be -force\"\n"
"		}\n"
"\n"
"		set in [open $source]\n"
"\n"
"		if {$force eq \"\" && [file exists $target]} {\n"
"			$in close\n"
"			error \"error copying \\\"$source\\\" to \\\"$target\\\": file already exists\"\n"
"		}\n"
"		set out [open $target w]\n"
"		$in copyto $out\n"
"		$out close\n"
"	} on error {msg opts} {\n"
"		incr opts(-level)\n"
"		return {*}$opts $msg\n"
"	} finally {\n"
"		catch {$in close}\n"
"	}\n"
"}\n"
"\n"
"\n"
"\n"
"proc popen {cmd {mode r}} {\n"
"	lassign [socket pipe] r w\n"
"	try {\n"
"		if {[string match \"w*\" $mode]} {\n"
"			lappend cmd <@$r &\n"
"			set pids [exec {*}$cmd]\n"
"			$r close\n"
"			set f $w\n"
"		} else {\n"
"			lappend cmd >@$w &\n"
"			set pids [exec {*}$cmd]\n"
"			$w close\n"
"			set f $r\n"
"		}\n"
"		lambda {cmd args} {f pids} {\n"
"			if {$cmd eq \"pid\"} {\n"
"				return $pids\n"
"			}\n"
"			if {$cmd eq \"close\"} {\n"
"				$f close\n"
"\n"
"				foreach p $pids { os.wait $p }\n"
"				return\n"
"			}\n"
"			tailcall $f $cmd {*}$args\n"
"		}\n"
"	} on error {error opts} {\n"
"		$r close\n"
"		$w close\n"
"		error $error\n"
"	}\n"
"}\n"
"\n"
"\n"
"local proc pid {{chan {}}} {\n"
"	if {$chan eq \"\"} {\n"
"		tailcall upcall pid\n"
"	}\n"
"	if {[catch {$chan tell}]} {\n"
"		return -code error \"can not find channel named \\\"$chan\\\"\"\n"
"	}\n"
"	if {[catch {$chan pid} pids]} {\n"
"		return \"\"\n"
"	}\n"
"	return $pids\n"
"}\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"\n"
"proc try {args} {\n"
"	set catchopts {}\n"
"	while {[string match -* [lindex $args 0]]} {\n"
"		set args [lassign $args opt]\n"
"		if {$opt eq \"--\"} {\n"
"			break\n"
"		}\n"
"		lappend catchopts $opt\n"
"	}\n"
"	if {[llength $args] == 0} {\n"
"		return -code error {wrong # args: should be \"try ?options? script ?argument ...?\"}\n"
"	}\n"
"	set args [lassign $args script]\n"
"	set code [catch -eval {*}$catchopts [list uplevel 1 $script] msg opts]\n"
"\n"
"	set handled 0\n"
"\n"
"	foreach {on codes vars script} $args {\n"
"		switch -- $on \\\n"
"			on {\n"
"				if {!$handled && ($codes eq \"*\" || [info returncode $code] in $codes)} {\n"
"					lassign $vars msgvar optsvar\n"
"					if {$msgvar ne \"\"} {\n"
"						upvar $msgvar hmsg\n"
"						set hmsg $msg\n"
"					}\n"
"					if {$optsvar ne \"\"} {\n"
"						upvar $optsvar hopts\n"
"						set hopts $opts\n"
"					}\n"
"\n"
"					set code [catch [list uplevel 1 $script] msg opts]\n"
"					incr handled\n"
"				}\n"
"			} \\\n"
"			finally {\n"
"				set finalcode [catch [list uplevel 1 $codes] finalmsg finalopts]\n"
"				if {$finalcode} {\n"
"\n"
"					set code $finalcode\n"
"					set msg $finalmsg\n"
"					set opts $finalopts\n"
"				}\n"
"				break\n"
"			} \\\n"
"			default {\n"
"				return -code error \"try: expected 'on' or 'finally', got '$on'\"\n"
"			}\n"
"	}\n"
"\n"
"	if {$code} {\n"
"		incr opts(-level)\n"
"		return {*}$opts $msg\n"
"	}\n"
"	return $msg\n"
"}\n"
"\n"
"\n"
"\n"
"proc throw {code {msg \"\"}} {\n"
"	return -code $code $msg\n"
"}\n"
"\n"
"\n"
"proc {file delete force} {path} {\n"
"	foreach e [readdir $path] {\n"
"		file delete -force $path/$e\n"
"	}\n"
"	file delete $path\n"
"}\n"
,"tclcompat.tcl", 1);
}

/* Jim - A small embeddable Tcl interpreter
 *
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright 2005 Clemens Hintze <c.hintze@gmx.net>
 * Copyright 2005 patthoyts - Pat Thoyts <patthoyts@users.sf.net> 
 * Copyright 2008 oharboe - Øyvind Harboe - oyvind.harboe@zylin.com
 * Copyright 2008 Andrew Lunn <andrew@lunn.ch>
 * Copyright 2008 Duane Ellis <openocd@duaneellis.com>
 * Copyright 2008 Uwe Klein <uklein@klein-messgeraete.de>
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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>


#if defined(HAVE_SYS_SOCKET_H) && defined(HAVE_SELECT) && defined(HAVE_NETINET_IN_H) && defined(HAVE_NETDB_H) && defined(HAVE_ARPA_INET_H)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#else
#define JIM_ANSIC
#endif


#define AIO_CMD_LEN 32      /* e.g. aio.handleXXXXXX */
#define AIO_BUF_LEN 256     /* Can keep this small and rely on stdio buffering */

#define AIO_KEEPOPEN 1

#if defined(JIM_IPV6)
#define IPV6 1
#else
#define IPV6 0
#ifndef PF_INET6
#define PF_INET6 0
#endif
#endif

#ifndef JIM_ANSIC
union sockaddr_any {
    struct sockaddr sa;
    struct sockaddr_in sin;
#if IPV6
    struct sockaddr_in6 sin6;
#endif
};

#ifndef HAVE_INET_NTOP
const char *inet_ntop(int af, const void *src, char *dst, int size)
{
    if (af != PF_INET) {
        return NULL;
    }
    snprintf(dst, size, "%s", inet_ntoa(((struct sockaddr_in *)src)->sin_addr));
    return dst;
}
#endif
#endif

typedef struct AioFile
{
    FILE *fp;
    Jim_Obj *filename;
    int type;
    int OpenFlags;              /* AIO_KEEPOPEN? keep FILE* */
    int fd;
#ifdef O_NDELAY
    int flags;
#endif
    Jim_Obj *rEvent;
    Jim_Obj *wEvent;
    Jim_Obj *eEvent;
#ifndef JIM_ANSIC
    int addr_family;
#endif
} AioFile;

static int JimAioSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

#ifndef JIM_ANSIC
static int JimParseIPv6Address(Jim_Interp *interp, const char *hostport, union sockaddr_any *sa, int *salen)
{
#if IPV6
    /*
     * An IPv6 addr/port looks like:
     *   [::1]
     *   [::1]:2000
     *   [fe80::223:6cff:fe95:bdc0%en1]:2000
     *   [::]:2000
     *   2000
     *
     *   Note that the "any" address is ::, which is the same as when no address is specified.
     */
     char *sthost = NULL;
     const char *stport;
     int ret = JIM_OK;
     struct addrinfo req;
     struct addrinfo *ai;

    stport = strrchr(hostport, ':');
    if (!stport) {
        /* No : so, the whole thing is the port */
        stport = hostport;
        hostport = "::";
        sthost = Jim_StrDup(hostport);
    }
    else {
        stport++;
    }

    if (*hostport == '[') {
        /* This is a numeric ipv6 address */
        char *pt = strchr(++hostport, ']');
        if (pt) {
            sthost = Jim_StrDupLen(hostport, pt - hostport);
        }
    }

    if (!sthost) {
        sthost = Jim_StrDupLen(hostport, stport - hostport - 1);
    }

    memset(&req, '\0', sizeof(req));
    req.ai_family = PF_INET6;
    
    if (getaddrinfo(sthost, NULL, &req, &ai)) {
        Jim_SetResultFormatted(interp, "Not a valid address: %s", hostport);
        ret = JIM_ERR;
    }
    else {
        memcpy(&sa->sin, ai->ai_addr, ai->ai_addrlen);
        *salen = ai->ai_addrlen;

        sa->sin.sin_port = htons(atoi(stport));

        freeaddrinfo(ai);
    }
    Jim_Free(sthost);

    return ret;
#else
    Jim_SetResultString(interp, "ipv6 not supported", -1);
    return JIM_ERR;
#endif
}

static int JimParseIpAddress(Jim_Interp *interp, const char *hostport, union sockaddr_any *sa, int *salen)
{
    /* An IPv4 addr/port looks like:
     *   192.168.1.5
     *   192.168.1.5:2000
     *   2000
     *
     * If the address is missing, INADDR_ANY is used.
     * If the port is missing, 0 is used (only useful for server sockets).
     */
    char *sthost = NULL;
    const char *stport;
    int ret = JIM_OK;

    stport = strrchr(hostport, ':');
    if (!stport) {
        /* No : so, the whole thing is the port */
        stport = hostport;
        sthost = Jim_StrDup("0.0.0.0");
    }
    else {
        sthost = Jim_StrDupLen(hostport, stport - hostport);
        stport++;
    }

    {
#ifdef HAVE_GETADDRINFO
        struct addrinfo req;
        struct addrinfo *ai;
        memset(&req, '\0', sizeof(req));
        req.ai_family = PF_INET;
        
        if (getaddrinfo(sthost, NULL, &req, &ai)) {
            ret = JIM_ERR;
        }
        else {
            memcpy(&sa->sin, ai->ai_addr, ai->ai_addrlen);
            *salen = ai->ai_addrlen;
            freeaddrinfo(ai);
        }
#else
        struct hostent *he;

        ret = JIM_ERR;

        if ((he = gethostbyname(sthost)) != NULL) {
            if (he->h_length == sizeof(sa->sin.sin_addr)) {
                *salen = sizeof(sa->sin);
                sa->sin.sin_family= he->h_addrtype;
                memcpy(&sa->sin.sin_addr, he->h_addr, he->h_length); /* set address */
                ret = JIM_OK;
            }
        }
#endif

        sa->sin.sin_port = htons(atoi(stport));
    }
    Jim_Free(sthost);

    if (ret != JIM_OK) {
        Jim_SetResultFormatted(interp, "Not a valid address: %s", hostport);
    }

    return ret;
}

#ifdef HAVE_SYS_UN_H
static int JimParseDomainAddress(Jim_Interp *interp, const char *path, struct sockaddr_un *sa)
{
    sa->sun_family = PF_UNIX;
    snprintf(sa->sun_path, sizeof(sa->sun_path), "%s", path);

    return JIM_OK;
}
#endif
#endif

static void JimAioSetError(Jim_Interp *interp, Jim_Obj *name)
{
    if (name) {
        Jim_SetResultFormatted(interp, "%#s: %s", name, strerror(errno));
    }
    else {
        Jim_SetResultString(interp, strerror(errno), -1);
    }
}

static void JimAioDelProc(Jim_Interp *interp, void *privData)
{
    AioFile *af = privData;

    JIM_NOTUSED(interp);

    Jim_DecrRefCount(interp, af->filename);

    if (!(af->OpenFlags & AIO_KEEPOPEN)) {
        fclose(af->fp);
    }
#ifdef jim_ext_eventloop
    /* remove existing EventHandlers */
    if (af->rEvent) {
        Jim_DeleteFileHandler(interp, af->fp);
    }
    if (af->wEvent) {
        Jim_DeleteFileHandler(interp, af->fp);
    }
    if (af->eEvent) {
        Jim_DeleteFileHandler(interp, af->fp);
    }
#endif
    Jim_Free(af);
}

static int aio_cmd_read(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char buf[AIO_BUF_LEN];
    Jim_Obj *objPtr;
    int nonewline = 0;
    int neededLen = -1;         /* -1 is "read as much as possible" */

    if (argc && Jim_CompareStringImmediate(interp, argv[0], "-nonewline")) {
        nonewline = 1;
        argv++;
        argc--;
    }
    if (argc == 1) {
        jim_wide wideValue;

        if (Jim_GetWide(interp, argv[0], &wideValue) != JIM_OK)
            return JIM_ERR;
        if (wideValue < 0) {
            Jim_SetResultString(interp, "invalid parameter: negative len", -1);
            return JIM_ERR;
        }
        neededLen = (int)wideValue;
    }
    else if (argc) {
        return -1;
    }
    objPtr = Jim_NewStringObj(interp, NULL, 0);
    while (neededLen != 0) {
        int retval;
        int readlen;

        if (neededLen == -1) {
            readlen = AIO_BUF_LEN;
        }
        else {
            readlen = (neededLen > AIO_BUF_LEN ? AIO_BUF_LEN : neededLen);
        }
        retval = fread(buf, 1, readlen, af->fp);
        if (retval > 0) {
            Jim_AppendString(interp, objPtr, buf, retval);
            if (neededLen != -1) {
                neededLen -= retval;
            }
        }
        if (retval != readlen)
            break;
    }
    /* Check for error conditions */
    if (ferror(af->fp)) {
        clearerr(af->fp);
        /* eof and EAGAIN are not error conditions */
        if (!feof(af->fp) && errno != EAGAIN) {
            /* I/O error */
            Jim_FreeNewObj(interp, objPtr);
            JimAioSetError(interp, af->filename);
            return JIM_ERR;
        }
    }
    if (nonewline) {
        int len;
        const char *s = Jim_GetString(objPtr, &len);

        if (len > 0 && s[len - 1] == '\n') {
            objPtr->length--;
            objPtr->bytes[objPtr->length] = '\0';
        }
    }
    Jim_SetResult(interp, objPtr);
    return JIM_OK;
}

static int aio_cmd_copy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    long count = 0;
    long maxlen = LONG_MAX;
    FILE *outfh = Jim_AioFilehandle(interp, argv[0]);

    if (outfh == NULL) {
        return JIM_ERR;
    }

    if (argc == 2) {
        if (Jim_GetLong(interp, argv[1], &maxlen) != JIM_OK) {
            return JIM_ERR;
        }
    }

    while (count < maxlen) {
        int ch = fgetc(af->fp);

        if (ch == EOF || fputc(ch, outfh) == EOF) {
            break;
        }
        count++;
    }

    if (ferror(af->fp)) {
        Jim_SetResultFormatted(interp, "error while reading: %s", strerror(errno));
        clearerr(af->fp);
        return JIM_ERR;
    }

    if (ferror(outfh)) {
        Jim_SetResultFormatted(interp, "error while writing: %s", strerror(errno));
        clearerr(outfh);
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, count);

    return JIM_OK;
}

static int aio_cmd_gets(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char buf[AIO_BUF_LEN];
    Jim_Obj *objPtr;

    errno = 0;

    objPtr = Jim_NewStringObj(interp, NULL, 0);
    while (1) {
        int more = 0;

        buf[AIO_BUF_LEN - 1] = '_';
        if (fgets(buf, AIO_BUF_LEN, af->fp) == NULL)
            break;
        if (buf[AIO_BUF_LEN - 1] == '\0' && buf[AIO_BUF_LEN - 2] != '\n')
            more = 1;
        if (more) {
            Jim_AppendString(interp, objPtr, buf, AIO_BUF_LEN - 1);
        }
        else {
            int len = strlen(buf);

            if (len) {
                int hasnl = (buf[len - 1] == '\n');

                /* strip "\n" */
                Jim_AppendString(interp, objPtr, buf, strlen(buf) - hasnl);
            }
        }
        if (!more)
            break;
    }
    if (ferror(af->fp) && errno != EAGAIN && errno != EINTR) {
        /* I/O error */
        Jim_FreeNewObj(interp, objPtr);
        JimAioSetError(interp, af->filename);
        clearerr(af->fp);
        return JIM_ERR;
    }
    /* On EOF returns -1 if varName was specified, or the empty string. */
    if (feof(af->fp) && Jim_Length(objPtr) == 0) {
        Jim_FreeNewObj(interp, objPtr);
        if (argc) {
            Jim_SetResultInt(interp, -1);
        }
        return JIM_OK;
    }
    if (argc) {
        int totLen;

        Jim_GetString(objPtr, &totLen);
        if (Jim_SetVariable(interp, argv[0], objPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, objPtr);
            return JIM_ERR;
        }
        Jim_SetResultInt(interp, totLen);
    }
    else {
        Jim_SetResult(interp, objPtr);
    }
    return JIM_OK;
}

static int aio_cmd_puts(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int wlen;
    const char *wdata;
    Jim_Obj *strObj;

    if (argc == 2) {
        if (!Jim_CompareStringImmediate(interp, argv[0], "-nonewline")) {
            return -1;
        }
        strObj = argv[1];
    }
    else {
        strObj = argv[0];
    }

    wdata = Jim_GetString(strObj, &wlen);
    if (fwrite(wdata, 1, wlen, af->fp) == (unsigned)wlen) {
        if (argc == 2 || putc('\n', af->fp) != EOF) {
            return JIM_OK;
        }
    }
    JimAioSetError(interp, af->filename);
    return JIM_ERR;
}

#ifndef JIM_ANSIC
static int aio_cmd_recvfrom(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    char *buf;
    union sockaddr_any sa;
    long len;
    socklen_t salen = sizeof(sa);
    int rlen;

    if (Jim_GetLong(interp, argv[0], &len) != JIM_OK) {
        return JIM_ERR;
    }

    buf = Jim_Alloc(len + 1);

    rlen = recvfrom(fileno(af->fp), buf, len, 0, &sa.sa, &salen);
    if (rlen < 0) {
        Jim_Free(buf);
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }
    buf[rlen] = 0;
    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, buf, rlen));

    if (argc > 1) {
        /* INET6_ADDRSTRLEN is 46. Add some for [] and port */
        char addrbuf[60];

#if IPV6
        if (sa.sa.sa_family == PF_INET6) {
            addrbuf[0] = '[';
            /* Allow 9 for []:65535\0 */
            inet_ntop(sa.sa.sa_family, &sa.sin6.sin6_addr, addrbuf + 1, sizeof(addrbuf) - 9);
            snprintf(addrbuf + strlen(addrbuf), 8, "]:%d", ntohs(sa.sin.sin_port));
        }
        else
#endif
        {
            /* Allow 7 for :65535\0 */
            inet_ntop(sa.sa.sa_family, &sa.sin.sin_addr, addrbuf, sizeof(addrbuf) - 7);
            snprintf(addrbuf + strlen(addrbuf), 7, ":%d", ntohs(sa.sin.sin_port));
        }

        if (Jim_SetVariable(interp, argv[1], Jim_NewStringObj(interp, addrbuf, -1)) != JIM_OK) {
            return JIM_ERR;
        }
    }

    return JIM_OK;
}


static int aio_cmd_sendto(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int wlen;
    int len;
    const char *wdata;
    union sockaddr_any sa;
    const char *addr = Jim_String(argv[1]);
    int salen;

    if (IPV6 && af->addr_family == PF_INET6) {
        if (JimParseIPv6Address(interp, addr, &sa, &salen) != JIM_OK) {
            return JIM_ERR;
        }
    }
    else if (JimParseIpAddress(interp, addr, &sa, &salen) != JIM_OK) {
        return JIM_ERR;
    }
    wdata = Jim_GetString(argv[0], &wlen);

    /* Note that we don't validate the socket type. Rely on sendto() failing if appropriate */
    len = sendto(fileno(af->fp), wdata, wlen, 0, &sa.sa, salen);
    if (len < 0) {
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, len);
    return JIM_OK;
}

static int aio_cmd_accept(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *serv_af = Jim_CmdPrivData(interp);
    int sock;
    union sockaddr_any sa;
    socklen_t addrlen = sizeof(sa);
    AioFile *af;
    char buf[AIO_CMD_LEN];

    sock = accept(serv_af->fd, &sa.sa, &addrlen);
    if (sock < 0)
        return JIM_ERR;

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    af->fd = sock;
#ifdef FD_CLOEXEC
    fcntl(af->fd, F_SETFD, FD_CLOEXEC);
#endif
    af->filename = Jim_NewStringObj(interp, "accept", -1);
    Jim_IncrRefCount(af->filename);
    af->fp = fdopen(sock, "r+");

    af->OpenFlags = 0;
#ifdef O_NDELAY
    af->flags = fcntl(af->fd, F_GETFL);
#endif
    af->rEvent = NULL;
    af->wEvent = NULL;
    af->eEvent = NULL;
    af->addr_family = serv_af->addr_family;
    snprintf(buf, sizeof(buf), "aio.sockstream%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimAioSubCmdProc, af, JimAioDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

#endif

static int aio_cmd_flush(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    if (fflush(af->fp) == EOF) {
        JimAioSetError(interp, af->filename);
        return JIM_ERR;
    }
    return JIM_OK;
}

static int aio_cmd_eof(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    Jim_SetResultInt(interp, feof(af->fp));
    return JIM_OK;
}

static int aio_cmd_close(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_DeleteCommand(interp, Jim_String(argv[0]));
    return JIM_OK;
}

static int aio_cmd_seek(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);
    int orig = SEEK_SET;
    long offset;

    if (argc == 2) {
        if (Jim_CompareStringImmediate(interp, argv[1], "start"))
            orig = SEEK_SET;
        else if (Jim_CompareStringImmediate(interp, argv[1], "current"))
            orig = SEEK_CUR;
        else if (Jim_CompareStringImmediate(interp, argv[1], "end"))
            orig = SEEK_END;
        else {
            return -1;
        }
    }
    if (Jim_GetLong(interp, argv[0], &offset) != JIM_OK) {
        return JIM_ERR;
    }
    if (fseek(af->fp, offset, orig) == -1) {
        JimAioSetError(interp, af->filename);
        return JIM_ERR;
    }
    return JIM_OK;
}

static int aio_cmd_tell(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    Jim_SetResultInt(interp, ftell(af->fp));
    return JIM_OK;
}

static int aio_cmd_filename(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    Jim_SetResult(interp, af->filename);
    return JIM_OK;
}

#ifdef O_NDELAY
static int aio_cmd_ndelay(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    int fmode = af->flags;

    if (argc) {
        long nb;

        if (Jim_GetLong(interp, argv[0], &nb) != JIM_OK) {
            return JIM_ERR;
        }
        if (nb) {
            fmode |= O_NDELAY;
        }
        else {
            fmode &= ~O_NDELAY;
        }
        fcntl(af->fd, F_SETFL, fmode);
        af->flags = fmode;
    }
    Jim_SetResultInt(interp, (fmode & O_NONBLOCK) ? 1 : 0);
    return JIM_OK;
}
#endif

static int aio_cmd_buffering(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    static const char * const options[] = {
        "none",
        "line",
        "full",
        NULL
    };
    enum
    {
        OPT_NONE,
        OPT_LINE,
        OPT_FULL,
    };
    int option;

    if (Jim_GetEnum(interp, argv[0], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }
    switch (option) {
        case OPT_NONE:
            setvbuf(af->fp, NULL, _IONBF, 0);
            break;
        case OPT_LINE:
            setvbuf(af->fp, NULL, _IOLBF, BUFSIZ);
            break;
        case OPT_FULL:
            setvbuf(af->fp, NULL, _IOFBF, BUFSIZ);
            break;
    }
    return JIM_OK;
}

#ifdef jim_ext_eventloop
static void JimAioFileEventFinalizer(Jim_Interp *interp, void *clientData)
{
    Jim_Obj *objPtr = clientData;

    Jim_DecrRefCount(interp, objPtr);
}

static int JimAioFileEventHandler(Jim_Interp *interp, void *clientData, int mask)
{
    Jim_Obj *objPtr = clientData;

    return Jim_EvalObjBackground(interp, objPtr);
}

static int aio_eventinfo(Jim_Interp *interp, AioFile * af, unsigned mask, Jim_Obj **scriptHandlerObj,
    int argc, Jim_Obj * const *argv)
{
    int scriptlen = 0;

    if (argc == 0) {
        /* Return current script */
        if (*scriptHandlerObj) {
            Jim_SetResult(interp, *scriptHandlerObj);
        }
        return JIM_OK;
    }

    if (*scriptHandlerObj) {
        /* Delete old handler */
        Jim_DeleteFileHandler(interp, af->fp);
        *scriptHandlerObj = NULL;
    }

    /* Now possibly add the new script(s) */
    Jim_GetString(argv[0], &scriptlen);
    if (scriptlen == 0) {
        /* Empty script, so done */
        return JIM_OK;
    }

    /* A new script to add */
    Jim_IncrRefCount(argv[0]);
    *scriptHandlerObj = argv[0];

    Jim_CreateFileHandler(interp, af->fp, mask,
        JimAioFileEventHandler, *scriptHandlerObj, JimAioFileEventFinalizer);

    return JIM_OK;
}

static int aio_cmd_readable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_READABLE, &af->rEvent, argc, argv);
}

static int aio_cmd_writable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_WRITABLE, &af->wEvent, argc, argv);
}

static int aio_cmd_onexception(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    AioFile *af = Jim_CmdPrivData(interp);

    return aio_eventinfo(interp, af, JIM_EVENT_EXCEPTION, &af->wEvent, argc, argv);
}
#endif

static const jim_subcmd_type aio_command_table[] = {
    {   .cmd = "read",
        .args = "?-nonewline? ?len?",
        .function = aio_cmd_read,
        .minargs = 0,
        .maxargs = 2,
        .description = "Read and return bytes from the stream. To eof if no len."
    },
    {   .cmd = "copyto",
        .args = "handle ?size?",
        .function = aio_cmd_copy,
        .minargs = 1,
        .maxargs = 2,
        .description = "Copy up to 'size' bytes to the given filehandle, or to eof if no size."
    },
    {   .cmd = "gets",
        .args = "?var?",
        .function = aio_cmd_gets,
        .minargs = 0,
        .maxargs = 1,
        .description = "Read one line and return it or store it in the var"
    },
    {   .cmd = "puts",
        .args = "?-nonewline? str",
        .function = aio_cmd_puts,
        .minargs = 1,
        .maxargs = 2,
        .description = "Write the string, with newline unless -nonewline"
    },
#ifndef JIM_ANSIC
    {   .cmd = "recvfrom",
        .args = "len ?addrvar?",
        .function = aio_cmd_recvfrom,
        .minargs = 1,
        .maxargs = 2,
        .description = "Receive up to 'len' bytes on the socket. Sets 'addrvar' with receive address, if set"
    },
    {   .cmd = "sendto",
        .args = "str address",
        .function = aio_cmd_sendto,
        .minargs = 2,
        .maxargs = 2,
        .description = "Send 'str' to the given address (dgram only)"
    },
    {   .cmd = "accept",
        .function = aio_cmd_accept,
        .description = "Server socket only: Accept a connection and return stream"
    },
#endif
    {   .cmd = "flush",
        .function = aio_cmd_flush,
        .description = "Flush the stream"
    },
    {   .cmd = "eof",
        .function = aio_cmd_eof,
        .description = "Returns 1 if stream is at eof"
    },
    {   .cmd = "close",
        .flags = JIM_MODFLAG_FULLARGV,
        .function = aio_cmd_close,
        .description = "Closes the stream"
    },
    {   .cmd = "seek",
        .args = "offset ?start|current|end",
        .function = aio_cmd_seek,
        .minargs = 1,
        .maxargs = 2,
        .description = "Seeks in the stream (default 'current')"
    },
    {   .cmd = "tell",
        .function = aio_cmd_tell,
        .description = "Returns the current seek position"
    },
    {   .cmd = "filename",
        .function = aio_cmd_filename,
        .description = "Returns the original filename"
    },
#ifdef O_NDELAY
    {   .cmd = "ndelay",
        .args = "?0|1?",
        .function = aio_cmd_ndelay,
        .minargs = 0,
        .maxargs = 1,
        .description = "Set O_NDELAY (if arg). Returns current/new setting."
    },
#endif
    {   .cmd = "buffering",
        .args = "none|line|full",
        .function = aio_cmd_buffering,
        .minargs = 1,
        .maxargs = 1,
        .description = "Sets buffering"
    },
#ifdef jim_ext_eventloop
    {   .cmd = "readable",
        .args = "?readable-script?",
        .minargs = 0,
        .maxargs = 1,
        .function = aio_cmd_readable,
        .description = "Returns script, or invoke readable-script when readable, {} to remove",
    },
    {   .cmd = "writable",
        .args = "?writable-script?",
        .minargs = 0,
        .maxargs = 1,
        .function = aio_cmd_writable,
        .description = "Returns script, or invoke writable-script when writable, {} to remove",
    },
    {   .cmd = "onexception",
        .args = "?exception-script?",
        .minargs = 0,
        .maxargs = 1,
        .function = aio_cmd_onexception,
        .description = "Returns script, or invoke exception-script when oob data, {} to remove",
    },
#endif
    { 0 }
};

static int JimAioSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, aio_command_table, argc, argv), argc, argv);
}

static int JimAioOpenCommand(Jim_Interp *interp, int argc, 
        Jim_Obj *const *argv)
{
    FILE *fp;
    AioFile *af;
    char buf[AIO_CMD_LEN];
    int OpenFlags = 0;
    const char *cmdname;

    if (argc != 2 && argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "filename ?mode?");
        return JIM_ERR;
    }
    cmdname = Jim_String(argv[1]);
    if (Jim_CompareStringImmediate(interp, argv[1], "stdin")) {
        OpenFlags |= AIO_KEEPOPEN;
        fp = stdin;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "stdout")) {
        OpenFlags |= AIO_KEEPOPEN;
        fp = stdout;
    }
    else if (Jim_CompareStringImmediate(interp, argv[1], "stderr")) {
        OpenFlags |= AIO_KEEPOPEN;
        fp = stderr;
    }
    else {
        const char *mode = (argc == 3) ? Jim_String(argv[2]) : "r";
        const char *filename = Jim_String(argv[1]);

#ifdef jim_ext_tclcompat
        /* If the filename starts with '|', use popen instead */
        if (*filename == '|') {
            Jim_Obj *evalObj[3];

            evalObj[0] = Jim_NewStringObj(interp, "popen", -1);
            evalObj[1] = Jim_NewStringObj(interp, filename + 1, -1);
            evalObj[2] = Jim_NewStringObj(interp, mode, -1);

            return Jim_EvalObjVector(interp, 3, evalObj);
        }
#endif
        fp = fopen(filename, mode);
        if (fp == NULL) {
            JimAioSetError(interp, argv[1]);
            return JIM_ERR;
        }
        /* Get the next file id */
        snprintf(buf, sizeof(buf), "aio.handle%ld", Jim_GetId(interp));
        cmdname = buf;
    }

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    af->fp = fp;
    af->fd = fileno(fp);
#ifdef FD_CLOEXEC
    if ((OpenFlags & AIO_KEEPOPEN) == 0) {
        fcntl(af->fd, F_SETFD, FD_CLOEXEC);
    }
#endif
#ifdef O_NDELAY
    af->flags = fcntl(af->fd, F_GETFL);
#endif
    af->filename = argv[1];
    Jim_IncrRefCount(af->filename);
    af->OpenFlags = OpenFlags;
    af->rEvent = NULL;
    af->wEvent = NULL;
    af->eEvent = NULL;
    Jim_CreateCommand(interp, cmdname, JimAioSubCmdProc, af, JimAioDelProc);
    Jim_SetResultString(interp, cmdname, -1);
    return JIM_OK;
}

#ifndef JIM_ANSIC

/**
 * Creates a channel for fd.
 * 
 * hdlfmt is a sprintf format for the filehandle. Anything with %ld at the end will do.
 * mode is usual "r+", but may be another fdopen() mode as required.
 *
 * Creates the command and lappends the name of the command to the current result.
 *
 */
static int JimMakeChannel(Jim_Interp *interp, Jim_Obj *filename, const char *hdlfmt, int fd, int family,
    const char *mode)
{
    AioFile *af;
    char buf[AIO_CMD_LEN];

    FILE *fp = fdopen(fd, mode);

    if (fp == NULL) {
        close(fd);
        JimAioSetError(interp, NULL);
        return JIM_ERR;
    }

    /* Create the file command */
    af = Jim_Alloc(sizeof(*af));
    af->fp = fp;
    af->fd = fd;
    fcntl(af->fd, F_SETFD, FD_CLOEXEC);
    af->OpenFlags = 0;
    af->filename = filename;
    Jim_IncrRefCount(af->filename);
#ifdef O_NDELAY
    af->flags = fcntl(af->fd, F_GETFL);
#endif
    af->rEvent = NULL;
    af->wEvent = NULL;
    af->eEvent = NULL;
    af->addr_family = family;
    snprintf(buf, sizeof(buf), hdlfmt, Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimAioSubCmdProc, af, JimAioDelProc);

    Jim_ListAppendElement(interp, Jim_GetResult(interp), Jim_NewStringObj(interp, buf, -1));

    return JIM_OK;
}

static int JimAioSockCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *hdlfmt = "aio.unknown%ld";
    const char *socktypes[] = {
        "unix",
        "unix.server",
        "dgram",
        "dgram.server",
        "stream",
        "stream.server",
        "pipe",
        NULL
    };
    enum
    {
        SOCK_UNIX,
        SOCK_UNIX_SERVER,
        SOCK_DGRAM_CLIENT,
        SOCK_DGRAM_SERVER,
        SOCK_STREAM_CLIENT,
        SOCK_STREAM_SERVER,
        SOCK_STREAM_PIPE,
        SOCK_DGRAM6_CLIENT,
        SOCK_DGRAM6_SERVER,
        SOCK_STREAM6_CLIENT,
        SOCK_STREAM6_SERVER,
    };
    int socktype;
    int sock;
    const char *hostportarg = NULL;
    int res;
    int on = 1;
    const char *mode = "r+";
    int family = PF_INET;
    Jim_Obj *argv0 = argv[0];
    int ipv6 = 0;

    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[1], "-ipv6")) {
        if (!IPV6) {
            Jim_SetResultString(interp, "ipv6 not supported", -1);
            return JIM_ERR;
        }
        ipv6 = 1;
        family = PF_INET6;
    }
    argc -= ipv6;
    argv += ipv6;

    if (argc < 2) {
      wrongargs:
        Jim_WrongNumArgs(interp, 1, &argv0, "?-ipv6? type ?address?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], socktypes, &socktype, "socket type", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;

    Jim_SetResultString(interp, "", 0);

    hdlfmt = "aio.sock%ld";

    if (argc > 2) {
        hostportarg = Jim_String(argv[2]);
    }

    switch (socktype) {
        case SOCK_DGRAM_CLIENT:
            if (argc == 2) {
                /* No address, so an unconnected dgram socket */
                sock = socket(family, SOCK_DGRAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                break;
            }
            /* fall through */
        case SOCK_STREAM_CLIENT:
            {
                union sockaddr_any sa;
                int salen;

                if (argc != 3) {
                    goto wrongargs;
                }

                if (ipv6) {
                    if (JimParseIPv6Address(interp, hostportarg, &sa, &salen) != JIM_OK) {
                        return JIM_ERR;
                    }
                }
                else if (JimParseIpAddress(interp, hostportarg, &sa, &salen) != JIM_OK) {
                    return JIM_ERR;
                }
                sock = socket(family, (socktype == SOCK_DGRAM_CLIENT) ? SOCK_DGRAM : SOCK_STREAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                res = connect(sock, &sa.sa, salen);
                if (res) {
                    JimAioSetError(interp, argv[2]);
                    close(sock);
                    return JIM_ERR;
                }
            }
            break;

        case SOCK_STREAM_SERVER:
        case SOCK_DGRAM_SERVER:
            {
                union sockaddr_any sa;
                int salen;

                if (argc != 3) {
                    goto wrongargs;
                }

                if (ipv6) {
                    if (JimParseIPv6Address(interp, hostportarg, &sa, &salen) != JIM_OK) {
                        return JIM_ERR;
                    }
                }
                else if (JimParseIpAddress(interp, hostportarg, &sa, &salen) != JIM_OK) {
                    return JIM_ERR;
                }
                sock = socket(family, (socktype == SOCK_DGRAM_SERVER) ? SOCK_DGRAM : SOCK_STREAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }

                /* Enable address reuse */
                setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on));

                res = bind(sock, &sa.sa, salen);
                if (res) {
                    JimAioSetError(interp, argv[2]);
                    close(sock);
                    return JIM_ERR;
                }
                if (socktype == SOCK_STREAM_SERVER) {
                    res = listen(sock, 5);
                    if (res) {
                        JimAioSetError(interp, NULL);
                        close(sock);
                        return JIM_ERR;
                    }
                }
                hdlfmt = "aio.socksrv%ld";
            }
            break;

#ifdef HAVE_SYS_UN_H
        case SOCK_UNIX:
            {
                struct sockaddr_un sa;
                socklen_t len;

                if (argc != 3 || ipv6) {
                    goto wrongargs;
                }

                if (JimParseDomainAddress(interp, hostportarg, &sa) != JIM_OK) {
                    JimAioSetError(interp, argv[2]);
                    return JIM_ERR;
                }
                family = PF_UNIX;
                sock = socket(PF_UNIX, SOCK_STREAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                len = strlen(sa.sun_path) + 1 + sizeof(sa.sun_family);
                res = connect(sock, (struct sockaddr *)&sa, len);
                if (res) {
                    JimAioSetError(interp, argv[2]);
                    close(sock);
                    return JIM_ERR;
                }
                hdlfmt = "aio.sockunix%ld";
                break;
            }

        case SOCK_UNIX_SERVER:
            {
                struct sockaddr_un sa;
                socklen_t len;

                if (argc != 3 || ipv6) {
                    goto wrongargs;
                }

                if (JimParseDomainAddress(interp, hostportarg, &sa) != JIM_OK) {
                    JimAioSetError(interp, argv[2]);
                    return JIM_ERR;
                }
                family = PF_UNIX;
                sock = socket(PF_UNIX, SOCK_STREAM, 0);
                if (sock < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                len = strlen(sa.sun_path) + 1 + sizeof(sa.sun_family);
                res = bind(sock, (struct sockaddr *)&sa, len);
                if (res) {
                    JimAioSetError(interp, argv[2]);
                    close(sock);
                    return JIM_ERR;
                }
                res = listen(sock, 5);
                if (res) {
                    JimAioSetError(interp, NULL);
                    close(sock);
                    return JIM_ERR;
                }
                hdlfmt = "aio.sockunixsrv%ld";
                break;
            }
#endif

#ifdef HAVE_PIPE
        case SOCK_STREAM_PIPE:
            {
                int p[2];

                if (argc != 2 || ipv6) {
                    goto wrongargs;
                }

                if (pipe(p) < 0) {
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }

                hdlfmt = "aio.pipe%ld";
                if (JimMakeChannel(interp, argv[1], hdlfmt, p[0], family, "r") != JIM_OK) {
                    close(p[0]);
                    close(p[1]);
                    JimAioSetError(interp, NULL);
                    return JIM_ERR;
                }
                /* Note, if this fails it will leave p[0] open, but this should never happen */
                mode = "w";
                sock = p[1];
            }
            break;
#endif
        default:
            Jim_SetResultString(interp, "Unsupported socket type", -1);
            return JIM_ERR;
    }

    return JimMakeChannel(interp, argv[1], hdlfmt, sock, family, mode);
}
#endif

FILE *Jim_AioFilehandle(Jim_Interp *interp, Jim_Obj *command)
{
    Jim_Cmd *cmdPtr = Jim_GetCommand(interp, command, JIM_ERRMSG);

    if (cmdPtr && !cmdPtr->isproc && cmdPtr->u.native.cmdProc == JimAioSubCmdProc) {
        return ((AioFile *) cmdPtr->u.native.privData)->fp;
    }
    Jim_SetResultFormatted(interp, "Not a filehandle: \"%#s\"", command);
    return NULL;
}

int Jim_aioInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "aio", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "open", JimAioOpenCommand, NULL, NULL);
#ifndef JIM_ANSIC
    Jim_CreateCommand(interp, "socket", JimAioSockCommand, NULL, NULL);
#endif

    /* Takeover stdin, stdout and stderr */
    Jim_EvalGlobal(interp, "open stdin; open stdout; open stderr");

    return JIM_OK;
}

/*
 * Tcl readdir command.
 *
 * (c) 2008 Steve Bennett <steveb@worware.net.au>
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
 * Based on original work by:
 *-----------------------------------------------------------------------------
 * Copyright 1991-1994 Karl Lehenbauer and Mark Diekhans.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies.  Karl Lehenbauer and
 * Mark Diekhans make no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *-----------------------------------------------------------------------------
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>


/*
 *-----------------------------------------------------------------------------
 *
 * Jim_ReaddirCmd --
 *     Implements the rename TCL command:
 *         readdir ?-nocomplain? dirPath
 *
 * Results:
 *      Standard TCL result.
 *-----------------------------------------------------------------------------
 */
int Jim_ReaddirCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *dirPath;
    DIR *dirPtr;
    struct dirent *entryPtr;
    int nocomplain = 0;

    if (argc == 3 && Jim_CompareStringImmediate(interp, argv[1], "-nocomplain")) {
        nocomplain = 1;
    }
    if (argc != 2 && !nocomplain) {
        Jim_WrongNumArgs(interp, 1, argv, "?-nocomplain? dirPath");
        return JIM_ERR;
    }

    dirPath = Jim_String(argv[1 + nocomplain]);

    dirPtr = opendir(dirPath);
    if (dirPtr == NULL) {
        if (nocomplain) {
            return JIM_OK;
        }
        Jim_SetResultString(interp, strerror(errno), -1);
        return JIM_ERR;
    }
    Jim_SetResultString(interp, strerror(errno), -1);

    Jim_SetResult(interp, Jim_NewListObj(interp, NULL, 0));

    while ((entryPtr = readdir(dirPtr)) != NULL) {
        if (entryPtr->d_name[0] == '.') {
            if (entryPtr->d_name[1] == '\0') {
                continue;
            }
            if ((entryPtr->d_name[1] == '.') && (entryPtr->d_name[2] == '\0'))
                continue;
        }
        Jim_ListAppendElement(interp, Jim_GetResult(interp), Jim_NewStringObj(interp,
                entryPtr->d_name, -1));
    }
    closedir(dirPtr);

    return JIM_OK;
}

int Jim_readdirInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "readdir", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "readdir", Jim_ReaddirCmd, NULL, NULL);
    return JIM_OK;
}
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


static void FreeRegexpInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    regfree(objPtr->internalRep.regexpValue.compre);
    Jim_Free(objPtr->internalRep.regexpValue.compre);
}

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
        objPtr->internalRep.regexpValue.compre && objPtr->internalRep.regexpValue.flags == flags) {
        /* nothing to do */
        return objPtr->internalRep.regexpValue.compre;
    }

    /* Not a regexp or the flags do not match */
    if (objPtr->typePtr == &regexpObjType) {
        FreeRegexpInternalRep(interp, objPtr);
        objPtr->typePtr = NULL;
    }

    /* Get the string representation */
    pattern = Jim_String(objPtr);
    compre = Jim_Alloc(sizeof(regex_t));

    if ((ret = regcomp(compre, pattern, REG_EXTENDED | flags)) != 0) {
        char buf[100];

        regerror(ret, compre, buf, sizeof(buf));
        Jim_SetResultFormatted(interp, "couldn't compile regular expression pattern: %s", buf);
        regfree(compre);
        Jim_Free(compre);
        return NULL;
    }

    objPtr->typePtr = &regexpObjType;
    objPtr->internalRep.regexpValue.flags = flags;
    objPtr->internalRep.regexpValue.compre = compre;

    return compre;
}

int Jim_RegexpCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int opt_indices = 0;
    int opt_all = 0;
    int opt_inline = 0;
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
        OPT_INDICES,  OPT_NOCASE, OPT_LINE, OPT_ALL, OPT_INLINE, OPT_START, OPT_END
    };
    static const char * const options[] = {
        "-indices", "-nocase", "-line", "-all", "-inline", "-start", "--", NULL
    };

    if (argc < 3) {
      wrongNumArgs:
        Jim_WrongNumArgs(interp, 1, argv,
            "?switches? exp string ?matchVar? ?subMatchVar subMatchVar ...?");
        return JIM_ERR;
    }

    for (i = 1; i < argc; i++) {
        const char *opt = Jim_String(argv[i]);

        if (*opt != '-') {
            break;
        }
        if (Jim_GetEnum(interp, argv[i], options, &option, "switch", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
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
                break;

            case OPT_ALL:
                opt_all = 1;
                break;

            case OPT_INLINE:
                opt_inline = 1;
                break;

            case OPT_START:
                if (++i == argc) {
                    goto wrongNumArgs;
                }
                if (Jim_GetIndex(interp, argv[i], &offset) != JIM_OK) {
                    return JIM_ERR;
                }
                break;
        }
    }
    if (argc - i < 2) {
        goto wrongNumArgs;
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
            source_str += offset;
        }
        eflags |= REG_NOTBOL;
    }

    if (opt_inline) {
        resultListObj = Jim_NewListObj(interp, NULL, 0);
    }

  next_match:
    match = regexec(regex, source_str, num_vars + 1, pmatch, eflags);
    if (match >= REG_BADPAT) {
        char buf[100];

        regerror(match, regex, buf, sizeof(buf));
        Jim_SetResultFormatted(interp, "error while matching pattern: %s", buf);
        result = JIM_ERR;
        goto done;
    }

    if (match == REG_NOMATCH) {
        goto done;
    }

    num_matches++;

    if (opt_all && !opt_inline) {
        /* Just count the number of matches, so skip the substitution h */
        goto try_next_match;
    }

    /*
     * If additional variable names have been specified, return
     * index information in those variables.
     */

    j = 0;
    for (i += 2; opt_inline ? j < num_vars : i < argc; i++, j++) {
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
            int len = pmatch[j].rm_eo - pmatch[j].rm_so;

            if (opt_indices) {
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp,
                        offset + pmatch[j].rm_so));
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp,
                        offset + pmatch[j].rm_so + len - 1));
            }
            else {
                Jim_AppendString(interp, resultObj, source_str + pmatch[j].rm_so, len);
            }
        }

        if (opt_inline) {
            Jim_ListAppendElement(interp, resultListObj, resultObj);
        }
        else {
            /* And now set the result variable */
            result = Jim_SetVariable(interp, argv[i], resultObj);

            if (result != JIM_OK) {
                Jim_FreeObj(interp, resultObj);
                break;
            }
        }
    }

  try_next_match:
    if (opt_all && (pattern[0] != '^' || (regcomp_flags & REG_NEWLINE)) && *source_str) {
        if (pmatch[0].rm_eo) {
            offset += pmatch[0].rm_eo;
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
    int offset = 0;
    regex_t *regex;
    const char *p;
    int result;
    regmatch_t pmatch[MAX_SUB_MATCHES + 1];
    int num_matches = 0;

    int i, j, n;
    Jim_Obj *varname;
    Jim_Obj *resultObj;
    const char *source_str;
    int source_len;
    const char *replace_str;
    int replace_len;
    const char *pattern;
    int option;
    enum {
        OPT_NOCASE, OPT_LINE, OPT_ALL, OPT_START, OPT_END
    };
    static const char * const options[] = {
        "-nocase", "-line", "-all", "-start", "--", NULL
    };

    if (argc < 4) {
      wrongNumArgs:
        Jim_WrongNumArgs(interp, 1, argv,
            "?switches? exp string subSpec ?varName?");
        return JIM_ERR;
    }

    for (i = 1; i < argc; i++) {
        const char *opt = Jim_String(argv[i]);

        if (*opt != '-') {
            break;
        }
        if (Jim_GetEnum(interp, argv[i], options, &option, "switch", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
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

            case OPT_ALL:
                opt_all = 1;
                break;

            case OPT_START:
                if (++i == argc) {
                    goto wrongNumArgs;
                }
                if (Jim_GetIndex(interp, argv[i], &offset) != JIM_OK) {
                    return JIM_ERR;
                }
                break;
        }
    }
    if (argc - i != 3 && argc - i != 4) {
        goto wrongNumArgs;
    }

    regex = SetRegexpFromAny(interp, argv[i], regcomp_flags);
    if (!regex) {
        return JIM_ERR;
    }
    pattern = Jim_String(argv[i]);

    source_str = Jim_GetString(argv[i + 1], &source_len);
    replace_str = Jim_GetString(argv[i + 2], &replace_len);
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
    do {
        int match = regexec(regex, p, MAX_SUB_MATCHES, pmatch, regexec_flags);

        if (match >= REG_BADPAT) {
            char buf[100];

            regerror(match, regex, buf, sizeof(buf));
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
                    Jim_AppendString(interp, resultObj, replace_str + j - 1, 2);
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

        p += pmatch[0].rm_eo;
        n -= pmatch[0].rm_eo;

        /* If -all is not specified, or there is no source left, we are done */
        if (!opt_all || n == 0) {
            break;
        }

        /* An anchored pattern without -line must be done */
        if ((regcomp_flags & REG_NEWLINE) == 0 && pattern[0] == '^') {
            break;
        }

        /* If the pattern is empty, need to step forwards */
        if (pattern[0] == '\0' && n) {
            /* Need to copy the char we are moving over */
            Jim_AppendString(interp, resultObj, p, 1);
            p++;
            n--;
        }
        
        regexec_flags |= REG_NOTBOL;
    } while (n);

    /*
     * Copy the portion of the string after the last match to the
     * result variable.
     */
    Jim_AppendString(interp, resultObj, p, -1);

    /* And now set or return the result variable */
    if (argc - i == 4) {
        result = Jim_SetVariable(interp, varname, resultObj);

        if (result == JIM_OK) {
            Jim_SetResultInt(interp, num_matches);
        }
        else {
            Jim_FreeObj(interp, resultObj);
        }
    }
    else {
        Jim_SetResult(interp, resultObj);
        result = JIM_OK;
    }

    return result;
}

int Jim_regexpInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "regexp", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "regexp", Jim_RegexpCmd, NULL, NULL);
    Jim_CreateCommand(interp, "regsub", Jim_RegsubCmd, NULL, NULL);
    return JIM_OK;
}
/* 
 * Implements the file command for jim
 *
 * (c) 2008 Steve Bennett <steveb@workware.net.au>
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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/param.h>


# ifndef MAXPATHLEN
# define MAXPATHLEN JIM_PATH_LEN
# endif

/*
 *----------------------------------------------------------------------
 *
 * JimGetFileType --
 *
 *  Given a mode word, returns a string identifying the type of a
 *  file.
 *
 * Results:
 *  A static text string giving the file type from mode.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static const char *JimGetFileType(int mode)
{
    if (S_ISREG(mode)) {
        return "file";
    }
    else if (S_ISDIR(mode)) {
        return "directory";
    }
    else if (S_ISCHR(mode)) {
        return "characterSpecial";
    }
    else if (S_ISBLK(mode)) {
        return "blockSpecial";
    }
    else if (S_ISFIFO(mode)) {
        return "fifo";
#ifdef S_ISLNK
    }
    else if (S_ISLNK(mode)) {
        return "link";
#endif
#ifdef S_ISSOCK
    }
    else if (S_ISSOCK(mode)) {
        return "socket";
#endif
    }
    return "unknown";
}

/*
 *----------------------------------------------------------------------
 *
 * StoreStatData --
 *
 *  This is a utility procedure that breaks out the fields of a
 *  "stat" structure and stores them in textual form into the
 *  elements of an associative array.
 *
 * Results:
 *  Returns a standard Tcl return value.  If an error occurs then
 *  a message is left in interp->result.
 *
 * Side effects:
 *  Elements of the associative array given by "varName" are modified.
 *
 *----------------------------------------------------------------------
 */

static int set_array_int_value(Jim_Interp *interp, Jim_Obj *container, const char *key,
    jim_wide value)
{
    Jim_Obj *nameobj = Jim_NewStringObj(interp, key, -1);
    Jim_Obj *valobj = Jim_NewWideObj(interp, value);

    if (Jim_SetDictKeysVector(interp, container, &nameobj, 1, valobj) != JIM_OK) {
        Jim_FreeObj(interp, nameobj);
        Jim_FreeObj(interp, valobj);
        return JIM_ERR;
    }
    return JIM_OK;
}

static int set_array_string_value(Jim_Interp *interp, Jim_Obj *container, const char *key,
    const char *value)
{
    Jim_Obj *nameobj = Jim_NewStringObj(interp, key, -1);
    Jim_Obj *valobj = Jim_NewStringObj(interp, value, -1);

    if (Jim_SetDictKeysVector(interp, container, &nameobj, 1, valobj) != JIM_OK) {
        Jim_FreeObj(interp, nameobj);
        Jim_FreeObj(interp, valobj);
        return JIM_ERR;
    }
    return JIM_OK;
}

static int StoreStatData(Jim_Interp *interp, Jim_Obj *varName, const struct stat *sb)
{
    if (set_array_int_value(interp, varName, "dev", sb->st_dev) != JIM_OK) {
        Jim_SetResultFormatted(interp, "can't set \"%#s(dev)\": variables isn't array", varName);
        return JIM_ERR;
    }
    set_array_int_value(interp, varName, "ino", sb->st_ino);
    set_array_int_value(interp, varName, "mode", sb->st_mode);
    set_array_int_value(interp, varName, "nlink", sb->st_nlink);
    set_array_int_value(interp, varName, "uid", sb->st_uid);
    set_array_int_value(interp, varName, "gid", sb->st_gid);
    set_array_int_value(interp, varName, "size", sb->st_size);
    set_array_int_value(interp, varName, "atime", sb->st_atime);
    set_array_int_value(interp, varName, "mtime", sb->st_mtime);
    set_array_int_value(interp, varName, "ctime", sb->st_ctime);
    set_array_string_value(interp, varName, "type", JimGetFileType((int)sb->st_mode));

    /* And also return the value */
    Jim_SetResult(interp, Jim_GetVariable(interp, varName, 0));

    return JIM_OK;
}

static int file_cmd_dirname(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *path = Jim_String(argv[0]);
    const char *p = strrchr(path, '/');

    if (!p) {
        Jim_SetResultString(interp, ".", -1);
    }
    else if (p == path) {
        Jim_SetResultString(interp, "/", -1);
    }
#if defined(__MINGW32__)
    else if (p[-1] == ':') {
        /* z:/dir => z:/ */
        Jim_SetResultString(interp, path, p - path + 1);
    }
#endif
    else {
        Jim_SetResultString(interp, path, p - path);
    }
    return JIM_OK;
}

static int file_cmd_rootname(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *path = Jim_String(argv[0]);
    const char *lastSlash = strrchr(path, '/');
    const char *p = strrchr(path, '.');

    if (p == NULL || (lastSlash != NULL && lastSlash > p)) {
        Jim_SetResult(interp, argv[0]);
    }
    else {
        Jim_SetResultString(interp, path, p - path);
    }
    return JIM_OK;
}

static int file_cmd_extension(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *path = Jim_String(argv[0]);
    const char *lastSlash = strrchr(path, '/');
    const char *p = strrchr(path, '.');

    if (p == NULL || (lastSlash != NULL && lastSlash >= p)) {
        p = "";
    }
    Jim_SetResultString(interp, p, -1);
    return JIM_OK;
}

static int file_cmd_tail(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *path = Jim_String(argv[0]);
    const char *lastSlash = strrchr(path, '/');

    if (lastSlash) {
        Jim_SetResultString(interp, lastSlash + 1, -1);
    }
    else {
        Jim_SetResult(interp, argv[0]);
    }
    return JIM_OK;
}

static int file_cmd_normalize(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
#ifdef HAVE_REALPATH
    const char *path = Jim_String(argv[0]);
    char *newname = Jim_Alloc(MAXPATHLEN + 1);

    if (realpath(path, newname)) {
        Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, newname, -1));
    }
    else {
        Jim_Free(newname);
        Jim_SetResult(interp, argv[0]);
    }
    return JIM_OK;
#else
    Jim_SetResultString(interp, "Not implemented", -1);
    return JIM_ERR;
#endif
}

static int file_cmd_join(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    char *newname = Jim_Alloc(MAXPATHLEN + 1);
    char *last = newname;

    *newname = 0;

    /* Simple implementation for now */
    for (i = 0; i < argc; i++) {
        int len;
        const char *part = Jim_GetString(argv[i], &len);

        if (*part == '/') {
            /* Absolute component, so go back to the start */
            last = newname;
        }
#if defined(__MINGW32__)
        else if (strchr(part, ':')) {
            /* Absolute compontent on mingw, so go back to the start */
            last = newname;
        }
#endif
        else if (part[0] == '.') {
            if (part[1] == '/') {
                part += 2;
                len -= 2;
            }
            else if (part[1] == 0 && last != newname) {
                /* Adding '.' to an existing path does nothing */
                continue;
            }
        }

        /* Add a slash if needed */
        if (last != newname && last[-1] != '/') {
            *last++ = '/';
        }

        if (len) {
            if (last + len - newname >= MAXPATHLEN) {
                Jim_Free(newname);
                Jim_SetResultString(interp, "Path too long", -1);
                return JIM_ERR;
            }
            memcpy(last, part, len);
            last += len;
        }

        /* Remove a slash if needed */
        if (last > newname + 1 && last[-1] == '/') {
            *--last = 0;
        }
    }

    *last = 0;

    /* Probably need to handle some special cases ... */

    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, newname, last - newname));

    return JIM_OK;
}

static int file_access(Jim_Interp *interp, Jim_Obj *filename, int mode)
{
    const char *path = Jim_String(filename);
    int rc = access(path, mode);

    Jim_SetResultBool(interp, rc != -1);

    return JIM_OK;
}

static int file_cmd_readable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return file_access(interp, argv[0], R_OK);
}

static int file_cmd_writable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return file_access(interp, argv[0], W_OK);
}

static int file_cmd_executable(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return file_access(interp, argv[0], X_OK);
}

static int file_cmd_exists(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return file_access(interp, argv[0], F_OK);
}

static int file_cmd_delete(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int force = Jim_CompareStringImmediate(interp, argv[0], "-force");

    if (force || Jim_CompareStringImmediate(interp, argv[0], "--")) {
        argc++;
        argv--;
    }

    while (argc--) {
        const char *path = Jim_String(argv[0]);

        if (unlink(path) == -1 && errno != ENOENT) {
            if (rmdir(path) == -1) {
                /* Maybe try using the script helper */
                if (!force || Jim_EvalObjPrefix(interp, "file delete force", 1, argv) != JIM_OK) {
                    Jim_SetResultFormatted(interp, "couldn't delete file \"%s\": %s", path,
                        strerror(errno));
                    return JIM_ERR;
                }
            }
        }
        argv++;
    }
    return JIM_OK;
}

#ifdef HAVE_MKDIR_ONE_ARG
#define MKDIR_DEFAULT(PATHNAME) mkdir(PATHNAME)
#else
#define MKDIR_DEFAULT(PATHNAME) mkdir(PATHNAME, 0755)
#endif

/**
 * Create directory, creating all intermediate paths if necessary.
 *
 * Returns 0 if OK or -1 on failure (and sets errno)
 *
 * Note: The path may be modified.
 */
static int mkdir_all(char *path)
{
    int ok = 1;

    /* First time just try to make the dir */
    goto first;

    while (ok--) {
        /* Must have failed the first time, so recursively make the parent and try again */
        char *slash = strrchr(path, '/');

        if (slash && slash != path) {
            *slash = 0;
            if (mkdir_all(path) != 0) {
                return -1;
            }
            *slash = '/';
        }
      first:
        if (MKDIR_DEFAULT(path) == 0) {
            return 0;
        }
        if (errno == ENOENT) {
            /* Create the parent and try again */
            continue;
        }
        /* Maybe it already exists as a directory */
        if (errno == EEXIST) {
            struct stat sb;

            if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
                return 0;
            }
            /* Restore errno */
            errno = EEXIST;
        }
        /* Failed */
        break;
    }
    return -1;
}

static int file_cmd_mkdir(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    while (argc--) {
        char *path = Jim_StrDup(Jim_String(argv[0]));
        int rc = mkdir_all(path);

        Jim_Free(path);
        if (rc != 0) {
            Jim_SetResultFormatted(interp, "can't create directory \"%#s\": %s", argv[0],
                strerror(errno));
            return JIM_ERR;
        }
        argv++;
    }
    return JIM_OK;
}

#ifdef HAVE_MKSTEMP
static int file_cmd_tempfile(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int fd;
    char *filename;
    const char *template = "/tmp/tcl.tmp.XXXXXX";

    if (argc >= 1) {
        template = Jim_String(argv[0]);
    }
    filename = Jim_StrDup(template);

    fd = mkstemp(filename);
    if (fd < 0) {
        Jim_SetResultString(interp, "Failed to create tempfile", -1);
        return JIM_ERR;
    }
    close(fd);

    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, filename, -1));
    return JIM_OK;
}
#endif

static int file_cmd_rename(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *source;
    const char *dest;
    int force = 0;

    if (argc == 3) {
        if (!Jim_CompareStringImmediate(interp, argv[0], "-force")) {
            return -1;
        }
        force++;
        argv++;
        argc--;
    }

    source = Jim_String(argv[0]);
    dest = Jim_String(argv[1]);

    if (!force && access(dest, F_OK) == 0) {
        Jim_SetResultFormatted(interp, "error renaming \"%#s\" to \"%#s\": target exists", argv[0],
            argv[1]);
        return JIM_ERR;
    }

    if (rename(source, dest) != 0) {
        Jim_SetResultFormatted(interp, "error renaming \"%#s\" to \"%#s\": %s", argv[0], argv[1],
            strerror(errno));
        return JIM_ERR;
    }

    return JIM_OK;
}

static int file_stat(Jim_Interp *interp, Jim_Obj *filename, struct stat *sb)
{
    const char *path = Jim_String(filename);

    if (stat(path, sb) == -1) {
        Jim_SetResultFormatted(interp, "could not read \"%#s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    return JIM_OK;
}

#ifndef HAVE_LSTAT
#define lstat stat
#endif

static int file_lstat(Jim_Interp *interp, Jim_Obj *filename, struct stat *sb)
{
    const char *path = Jim_String(filename);

    if (lstat(path, sb) == -1) {
        Jim_SetResultFormatted(interp, "could not read \"%#s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    return JIM_OK;
}

static int file_cmd_atime(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, sb.st_atime);
    return JIM_OK;
}

static int file_cmd_mtime(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, sb.st_mtime);
    return JIM_OK;
}

static int file_cmd_copy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_EvalObjPrefix(interp, "file copy", argc, argv);
}

static int file_cmd_size(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, sb.st_size);
    return JIM_OK;
}

static int file_cmd_isdirectory(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;
    int ret = 0;

    if (file_stat(interp, argv[0], &sb) == JIM_OK) {
        ret = S_ISDIR(sb.st_mode);
    }
    Jim_SetResultInt(interp, ret);
    return JIM_OK;
}

static int file_cmd_isfile(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;
    int ret = 0;

    if (file_stat(interp, argv[0], &sb) == JIM_OK) {
        ret = S_ISREG(sb.st_mode);
    }
    Jim_SetResultInt(interp, ret);
    return JIM_OK;
}

#ifdef HAVE_GETEUID
static int file_cmd_owned(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;
    int ret = 0;

    if (file_stat(interp, argv[0], &sb) == JIM_OK) {
        ret = (geteuid() == sb.st_uid);
    }
    Jim_SetResultInt(interp, ret);
    return JIM_OK;
}
#endif

#if defined(HAVE_READLINK)
static int file_cmd_readlink(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *path = Jim_String(argv[0]);
    char *linkValue = Jim_Alloc(MAXPATHLEN + 1);

    int linkLength = readlink(path, linkValue, MAXPATHLEN);

    if (linkLength == -1) {
        Jim_Free(linkValue);
        Jim_SetResultFormatted(interp, "couldn't readlink \"%s\": %s", argv[0], strerror(errno));
        return JIM_ERR;
    }
    linkValue[linkLength] = 0;
    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, linkValue, linkLength));
    return JIM_OK;
}
#endif

static int file_cmd_type(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (file_lstat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultString(interp, JimGetFileType((int)sb.st_mode), -1);
    return JIM_OK;
}

static int file_cmd_lstat(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (file_lstat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    return StoreStatData(interp, argv[1], &sb);
}

static int file_cmd_stat(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct stat sb;

    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    return StoreStatData(interp, argv[1], &sb);
}

static const jim_subcmd_type file_command_table[] = {
    {   .cmd = "atime",
        .args = "name",
        .function = file_cmd_atime,
        .minargs = 1,
        .maxargs = 1,
        .description = "Last access time"
    },
    {   .cmd = "mtime",
        .args = "name",
        .function = file_cmd_mtime,
        .minargs = 1,
        .maxargs = 1,
        .description = "Last modification time"
    },
    {   .cmd = "copy",
        .args = "?-force? source dest",
        .function = file_cmd_copy,
        .minargs = 2,
        .maxargs = 3,
        .description = "Copy source file to destination file"
    },
    {   .cmd = "dirname",
        .args = "name",
        .function = file_cmd_dirname,
        .minargs = 1,
        .maxargs = 1,
        .description = "Directory part of the name"
    },
    {   .cmd = "rootname",
        .args = "name",
        .function = file_cmd_rootname,
        .minargs = 1,
        .maxargs = 1,
        .description = "Name without any extension"
    },
    {   .cmd = "extension",
        .args = "name",
        .function = file_cmd_extension,
        .minargs = 1,
        .maxargs = 1,
        .description = "Last extension including the dot"
    },
    {   .cmd = "tail",
        .args = "name",
        .function = file_cmd_tail,
        .minargs = 1,
        .maxargs = 1,
        .description = "Last component of the name"
    },
    {   .cmd = "normalize",
        .args = "name",
        .function = file_cmd_normalize,
        .minargs = 1,
        .maxargs = 1,
        .description = "Normalized path of name"
    },
    {   .cmd = "join",
        .args = "name ?name ...?",
        .function = file_cmd_join,
        .minargs = 1,
        .maxargs = -1,
        .description = "Join multiple path components"
    },
    {   .cmd = "readable",
        .args = "name",
        .function = file_cmd_readable,
        .minargs = 1,
        .maxargs = 1,
        .description = "Is file readable"
    },
    {   .cmd = "writable",
        .args = "name",
        .function = file_cmd_writable,
        .minargs = 1,
        .maxargs = 1,
        .description = "Is file writable"
    },
    {   .cmd = "executable",
        .args = "name",
        .function = file_cmd_executable,
        .minargs = 1,
        .maxargs = 1,
        .description = "Is file executable"
    },
    {   .cmd = "exists",
        .args = "name",
        .function = file_cmd_exists,
        .minargs = 1,
        .maxargs = 1,
        .description = "Does file exist"
    },
    {   .cmd = "delete",
        .args = "?-force|--? name ...",
        .function = file_cmd_delete,
        .minargs = 1,
        .maxargs = -1,
        .description = "Deletes the files or directories (must be empty unless -force)"
    },
    {   .cmd = "mkdir",
        .args = "dir ...",
        .function = file_cmd_mkdir,
        .minargs = 1,
        .maxargs = -1,
        .description = "Creates the directories"
    },
#ifdef HAVE_MKSTEMP
    {   .cmd = "tempfile",
        .args = "?template?",
        .function = file_cmd_tempfile,
        .minargs = 0,
        .maxargs = 1,
        .description = "Creates a temporary filename"
    },
#endif
    {   .cmd = "rename",
        .args = "?-force? source dest",
        .function = file_cmd_rename,
        .minargs = 2,
        .maxargs = 3,
        .description = "Renames a file"
    },
#if defined(HAVE_READLINK)
    {   .cmd = "readlink",
        .args = "name",
        .function = file_cmd_readlink,
        .minargs = 1,
        .maxargs = 1,
        .description = "Value of the symbolic link"
    },
#endif
    {   .cmd = "size",
        .args = "name",
        .function = file_cmd_size,
        .minargs = 1,
        .maxargs = 1,
        .description = "Size of file"
    },
    {   .cmd = "stat",
        .args = "name var",
        .function = file_cmd_stat,
        .minargs = 2,
        .maxargs = 2,
        .description = "Stores results of stat in var array"
    },
    {   .cmd = "lstat",
        .args = "name var",
        .function = file_cmd_lstat,
        .minargs = 2,
        .maxargs = 2,
        .description = "Stores results of lstat in var array"
    },
    {   .cmd = "type",
        .args = "name",
        .function = file_cmd_type,
        .minargs = 1,
        .maxargs = 1,
        .description = "Returns type of the file"
    },
#ifdef HAVE_GETEUID
    {   .cmd = "owned",
        .args = "name",
        .function = file_cmd_owned,
        .minargs = 1,
        .maxargs = 1,
        .description = "Returns 1 if owned by the current owner"
    },
#endif
    {   .cmd = "isdirectory",
        .args = "name",
        .function = file_cmd_isdirectory,
        .minargs = 1,
        .maxargs = 1,
        .description = "Returns 1 if name is a directory"
    },
    {   .cmd = "isfile",
        .args = "name",
        .function = file_cmd_isfile,
        .minargs = 1,
        .maxargs = 1,
        .description = "Returns 1 if name is a file"
    },
    {
        .cmd = 0
    }
};

static int Jim_CdCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *path;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "dirname");
        return JIM_ERR;
    }

    path = Jim_String(argv[1]);

    if (chdir(path) != 0) {
        Jim_SetResultFormatted(interp, "couldn't change working directory to \"%s\": %s", path,
            strerror(errno));
        return JIM_ERR;
    }
    return JIM_OK;
}

static int Jim_PwdCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const int cwd_len = 2048;
    char *cwd = malloc(cwd_len);

    if (getcwd(cwd, cwd_len) == NULL) {
        Jim_SetResultString(interp, "Failed to get pwd", -1);
        return JIM_ERR;
    }
#if defined(__MINGW32__)
    {
        /* Try to keep backlashes out of paths */
        char *p = cwd;
        while ((p = strchr(p, '\\')) != NULL) {
            *p++ = '/';
        }
    }
#endif

    Jim_SetResultString(interp, cwd, -1);

    free(cwd);
    return JIM_OK;
}

int Jim_fileInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "file", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "file", Jim_SubCmdProc, (void *)file_command_table, NULL);
    Jim_CreateCommand(interp, "pwd", Jim_PwdCmd, NULL, NULL);
    Jim_CreateCommand(interp, "cd", Jim_CdCmd, NULL, NULL);
    return JIM_OK;
}

/* 
 * (c) 2008 Steve Bennett <steveb@workware.net.au>
 *
 * Implements the exec command for Jim
 *
 * Based on code originally from Tcl 6.7 by John Ousterhout.
 * From that code:
 *
 * The Tcl_Fork and Tcl_WaitPids procedures are based on code
 * contributed by Karl Lehenbauer, Mark Diekhans and Peter
 * da Silva.
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

#include <string.h>
#include <signal.h>


#if defined(HAVE_VFORK) && defined(HAVE_WAITPID)


#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

#if defined(__GNUC__) && !defined(__clang__)
#define IGNORE_RC(EXPR) ((EXPR) < 0 ? -1 : 0)
#else
#define IGNORE_RC(EXPR) EXPR
#endif

/* These two could be moved into the Tcl core */
static void Jim_SetResultErrno(Jim_Interp *interp, const char *msg)
{
    Jim_SetResultFormatted(interp, "%s: %s", msg, strerror(errno));
}

static void Jim_RemoveTrailingNewline(Jim_Obj *objPtr)
{
    int len;
    const char *s = Jim_GetString(objPtr, &len);

    if (len > 0 && s[len - 1] == '\n') {
        objPtr->length--;
        objPtr->bytes[objPtr->length] = '\0';
    }
}

/**
 * Read from 'fd' and append the data to strObj
 * Returns JIM_OK if OK, or JIM_ERR on error.
 */
static int JimAppendStreamToString(Jim_Interp *interp, int fd, Jim_Obj *strObj)
{
    while (1) {
        char buffer[256];
        int count;

        count = read(fd, buffer, sizeof(buffer));

        if (count == 0) {
            Jim_RemoveTrailingNewline(strObj);
            return JIM_OK;
        }
        if (count < 0) {
            return JIM_ERR;
        }
        Jim_AppendString(interp, strObj, buffer, count);
    }
}

/*
 * If the last character of the result is a newline, then remove
 * the newline character (the newline would just confuse things).
 *
 * Note: Ideally we could do this by just reducing the length of stringrep
 *       by 1, but there is no API for this :-(
 */
static void JimTrimTrailingNewline(Jim_Interp *interp)
{
    int len;
    const char *p = Jim_GetString(Jim_GetResult(interp), &len);

    if (len > 0 && p[len - 1] == '\n') {
        Jim_SetResultString(interp, p, len - 1);
    }
}

/**
 * Builds the environment array from $::env
 *
 * If $::env is not set, simply returns environ.
 *
 * Otherwise allocates the environ array from the contents of $::env
 *
 * If the exec fails, memory can be freed via JimFreeEnv()
 */
static char **JimBuildEnv(Jim_Interp *interp)
{
#ifdef jim_ext_tclcompat
    int i;
    int len;
    int n;
    char **env;

    Jim_Obj *objPtr = Jim_GetGlobalVariableStr(interp, "env", JIM_NONE);

    if (!objPtr) {
        return Jim_GetEnviron();
    }

    /* Calculate the required size */
    len = Jim_ListLength(interp, objPtr);
    if (len % 2) {
        len--;
    }

    env = Jim_Alloc(sizeof(*env) * (len / 2 + 1));

    n = 0;
    for (i = 0; i < len; i += 2) {
        int l1, l2;
        const char *s1, *s2;
        Jim_Obj *elemObj;

        Jim_ListIndex(interp, objPtr, i, &elemObj, JIM_NONE);
        s1 = Jim_GetString(elemObj, &l1);
        Jim_ListIndex(interp, objPtr, i + 1, &elemObj, JIM_NONE);
        s2 = Jim_GetString(elemObj, &l2);

        env[n] = Jim_Alloc(l1 + l2 + 2);
        sprintf(env[n], "%s=%s", s1, s2);
        n++;
    }
    env[n] = NULL;

    return env;
#else
    return Jim_GetEnviron();
#endif
}

/**
 * Frees the environment allocated by JimBuildEnv()
 *
 * Must pass original_environ.
 */
static void JimFreeEnv(Jim_Interp *interp, char **env, char **original_environ)
{
#ifdef jim_ext_tclcompat
    if (env != original_environ) {
        int i;
        for (i = 0; env[i]; i++) {
            Jim_Free(env[i]);
        }
        Jim_Free(env);
    }
#endif
}

/*
 * Create error messages for unusual process exits.  An
 * extra newline gets appended to each error message, but
 * it gets removed below (in the same fashion that an
 * extra newline in the command's output is removed).
 */
static int JimCheckWaitStatus(Jim_Interp *interp, int pid, int waitStatus)
{
    Jim_Obj *errorCode = Jim_NewListObj(interp, NULL, 0);
    int rc = JIM_ERR;

    if (WIFEXITED(waitStatus)) {
        if (WEXITSTATUS(waitStatus) == 0) {
            Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "NONE", -1));
            rc = JIM_OK;
        }
        else {
            Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "CHILDSTATUS", -1));
            Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, pid));
            Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, WEXITSTATUS(waitStatus)));
        }
    }
    else {
        const char *type;
        const char *action;

        if (WIFSIGNALED(waitStatus)) {
            type = "CHILDKILLED";
            action = "killed";
        }
        else {
            type = "CHILDSUSP";
            action = "suspended";
        }

        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, type, -1));

#ifdef jim_ext_signal
        Jim_SetResultFormatted(interp, "child %s by signal %s", action, Jim_SignalId(WTERMSIG(waitStatus)));
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, Jim_SignalId(WTERMSIG(waitStatus)), -1));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, pid));
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, Jim_SignalName(WTERMSIG(waitStatus)), -1));
#else
        Jim_SetResultFormatted(interp, "child %s by signal %d", action, WTERMSIG(waitStatus));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, WTERMSIG(waitStatus)));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, pid));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, WTERMSIG(waitStatus)));
#endif
    }
    Jim_SetGlobalVariableStr(interp, "errorCode", errorCode);
    return rc;
}

/*
 * Data structures of the following type are used by JimFork and
 * JimWaitPids to keep track of child processes.
 */

struct WaitInfo
{
    int pid;                    /* Process id of child. */
    int status;                 /* Status returned when child exited or suspended. */
    int flags;                  /* Various flag bits;  see below for definitions. */
};

struct WaitInfoTable {
    struct WaitInfo *info;
    int size;
    int used;
};

/*
 * Flag bits in WaitInfo structures:
 *
 * WI_DETACHED -        Non-zero means no-one cares about the
 *                      process anymore.  Ignore it until it
 *                      exits, then forget about it.
 */

#define WI_DETACHED 2

#define WAIT_TABLE_GROW_BY 4

static void JimFreeWaitInfoTable(struct Jim_Interp *interp, void *privData)
{
    struct WaitInfoTable *table = privData;

    Jim_Free(table->info);
    Jim_Free(table);
}

static struct WaitInfoTable *JimAllocWaitInfoTable(void)
{
    struct WaitInfoTable *table = Jim_Alloc(sizeof(*table));
    table->info = NULL;
    table->size = table->used = 0;

    return table;
}

static int Jim_CreatePipeline(Jim_Interp *interp, int argc, Jim_Obj *const *argv,
    int **pidArrayPtr, int *inPipePtr, int *outPipePtr, int *errFilePtr);
static void JimDetachPids(Jim_Interp *interp, int numPids, const int *pidPtr);
static int Jim_CleanupChildren(Jim_Interp *interp, int numPids, int *pidPtr, int errorId);

static int Jim_ExecCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int outputId;               /* File id for output pipe.  -1
                                 * means command overrode. */
    int errorId;                /* File id for temporary file
                                 * containing error output. */
    int *pidPtr;
    int numPids, result;

    /*
     * See if the command is to be run in background;  if so, create
     * the command, detach it, and return.
     */
    if (argc > 1 && Jim_CompareStringImmediate(interp, argv[argc - 1], "&")) {
        Jim_Obj *listObj;
        int i;

        argc--;
        numPids = Jim_CreatePipeline(interp, argc - 1, argv + 1, &pidPtr, NULL, NULL, NULL);
        if (numPids < 0) {
            return JIM_ERR;
        }
        /* The return value is a list of the pids */
        listObj = Jim_NewListObj(interp, NULL, 0);
        for (i = 0; i < numPids; i++) {
            Jim_ListAppendElement(interp, listObj, Jim_NewIntObj(interp, pidPtr[i]));
        }
        Jim_SetResult(interp, listObj);
        JimDetachPids(interp, numPids, pidPtr);
        Jim_Free(pidPtr);
        return JIM_OK;
    }

    /*
     * Create the command's pipeline.
     */
    numPids =
        Jim_CreatePipeline(interp, argc - 1, argv + 1, &pidPtr, (int *)NULL, &outputId, &errorId);
    if (numPids < 0) {
        return JIM_ERR;
    }

    /*
     * Read the child's output (if any) and put it into the result.
     */
    Jim_SetResultString(interp, "", 0);

    result = JIM_OK;
    if (outputId != -1) {
        result = JimAppendStreamToString(interp, outputId, Jim_GetResult(interp));
        if (result < 0) {
            Jim_SetResultErrno(interp, "error reading from output pipe");
        }
        close(outputId);
    }

    if (Jim_CleanupChildren(interp, numPids, pidPtr, errorId) != JIM_OK) {
        result = JIM_ERR;
    }
    return result;
}

void Jim_ReapDetachedPids(struct WaitInfoTable *table)
{
    struct WaitInfo *waitPtr;
    int count;

    if (!table) {
        return;
    }

    for (waitPtr = table->info, count = table->used; count > 0; waitPtr++, count--) {
        if (waitPtr->flags & WI_DETACHED) {
            int status;
            int pid = waitpid(waitPtr->pid, &status, WNOHANG);
            if (pid > 0) {
                if (waitPtr != &table->info[table->used - 1]) {
                    *waitPtr = table->info[table->used - 1];
                }
                table->used--;
            }
        }
    }
}

/**
 * Does waitpid() on the given pid, and then removes the
 * entry from the wait table.
 * 
 * Returns the pid if OK and updates *statusPtr with the status,
 * or -1 if the pid was not in the table.
 */
static int JimWaitPid(struct WaitInfoTable *table, int pid, int *statusPtr)
{
    int i;

    /* Find it in the table */
    for (i = 0; i < table->used; i++) {
        if (pid == table->info[i].pid) {
            /* wait for it */
            waitpid(pid, statusPtr, 0);

            /* Remove it from the table */
            if (i != table->used - 1) {
                table->info[i] = table->info[table->used - 1];
            }
            table->used--;
            return pid;
        }
    }

    /* Not found */
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * JimDetachPids --
 *
 *  This procedure is called to indicate that one or more child
 *  processes have been placed in background and are no longer
 *  cared about.  These children can be cleaned up with JimReapDetachedPids().
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static void JimDetachPids(Jim_Interp *interp, int numPids, const int *pidPtr)
{
    int j;
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);

    for (j = 0; j < numPids; j++) {
        /* Find it in the table */
        int i;
        for (i = 0; i < table->used; i++) {
            if (pidPtr[j] == table->info[i].pid) {
                table->info[i].flags |= WI_DETACHED;
                break;
            }
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Jim_CreatePipeline --
 *
 *  Given an argc/argv array, instantiate a pipeline of processes
 *  as described by the argv.
 *
 * Results:
 *  The return value is a count of the number of new processes
 *  created, or -1 if an error occurred while creating the pipeline.
 *  *pidArrayPtr is filled in with the address of a dynamically
 *  allocated array giving the ids of all of the processes.  It
 *  is up to the caller to free this array when it isn't needed
 *  anymore.  If inPipePtr is non-NULL, *inPipePtr is filled in
 *  with the file id for the input pipe for the pipeline (if any):
 *  the caller must eventually close this file.  If outPipePtr
 *  isn't NULL, then *outPipePtr is filled in with the file id
 *  for the output pipe from the pipeline:  the caller must close
 *  this file.  If errFilePtr isn't NULL, then *errFilePtr is filled
 *  with a file id that may be used to read error output after the
 *  pipeline completes.
 *
 * Side effects:
 *  Processes and pipes are created.
 *
 *----------------------------------------------------------------------
 */
static int
Jim_CreatePipeline(Jim_Interp *interp, int argc, Jim_Obj *const *argv, int **pidArrayPtr,
    int *inPipePtr, int *outPipePtr, int *errFilePtr)
{
    int *pidPtr = NULL;         /* Points to malloc-ed array holding all
                                 * the pids of child processes. */
    int numPids = 0;            /* Actual number of processes that exist
                                 * at *pidPtr right now. */
    int cmdCount;               /* Count of number of distinct commands
                                 * found in argc/argv. */
    const char *input = NULL;   /* Describes input for pipeline, depending
                                 * on "inputFile".  NULL means take input
                                 * from stdin/pipe. */

#define FILE_NAME   0           /* input/output: filename */
#define FILE_APPEND 1           /* output only:  filename, append */
#define FILE_HANDLE 2           /* input/output: filehandle */
#define FILE_TEXT   3           /* input only:   input is actual text */

    int inputFile = FILE_NAME;  /* 1 means input is name of input file.
                                 * 2 means input is filehandle name.
                                 * 0 means input holds actual
                                 * text to be input to command. */

    int outputFile = FILE_NAME; /* 0 means output is the name of output file.
                                 * 1 means output is the name of output file, and append.
                                 * 2 means output is filehandle name.
                                 * All this is ignored if output is NULL
                                 */
    int errorFile = FILE_NAME;  /* 0 means error is the name of error file.
                                 * 1 means error is the name of error file, and append.
                                 * 2 means error is filehandle name.
                                 * All this is ignored if error is NULL
                                 */
    const char *output = NULL;  /* Holds name of output file to pipe to,
                                 * or NULL if output goes to stdout/pipe. */
    const char *error = NULL;   /* Holds name of stderr file to pipe to,
                                 * or NULL if stderr goes to stderr/pipe. */
    int inputId = -1;           /* Readable file id input to current command in
                                 * pipeline (could be file or pipe).  -1
                                 * means use stdin. */
    int outputId = -1;          /* Writable file id for output from current
                                 * command in pipeline (could be file or pipe).
                                 * -1 means use stdout. */
    int errorId = -1;           /* Writable file id for all standard error
                                 * output from all commands in pipeline.  -1
                                 * means use stderr. */
    int lastOutputId = -1;      /* Write file id for output from last command
                                 * in pipeline (could be file or pipe).
                                 * -1 means use stdout. */
    int pipeIds[2];             /* File ids for pipe that's being created. */
    int firstArg, lastArg;      /* Indexes of first and last arguments in
                                 * current command. */
    int lastBar;
    char *execName;
    int i, pid;
    char **orig_environ;
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);

    /* Holds the args which will be used to exec */
    char **arg_array = Jim_Alloc(sizeof(*arg_array) * (argc + 1));
    int arg_count = 0;

    Jim_ReapDetachedPids(table);

    if (inPipePtr != NULL) {
        *inPipePtr = -1;
    }
    if (outPipePtr != NULL) {
        *outPipePtr = -1;
    }
    if (errFilePtr != NULL) {
        *errFilePtr = -1;
    }
    pipeIds[0] = pipeIds[1] = -1;

    /*
     * First, scan through all the arguments to figure out the structure
     * of the pipeline.  Count the number of distinct processes (it's the
     * number of "|" arguments).  If there are "<", "<<", or ">" arguments
     * then make note of input and output redirection and remove these
     * arguments and the arguments that follow them.
     */
    cmdCount = 1;
    lastBar = -1;
    for (i = 0; i < argc; i++) {
        const char *arg = Jim_String(argv[i]);

        if (arg[0] == '<') {
            inputFile = FILE_NAME;
            input = arg + 1;
            if (*input == '<') {
                inputFile = FILE_TEXT;
                input++;
            }
            else if (*input == '@') {
                inputFile = FILE_HANDLE;
                input++;
            }

            if (!*input && ++i < argc) {
                input = Jim_String(argv[i]);
            }
        }
        else if (arg[0] == '>') {
            int dup_error = 0;

            outputFile = FILE_NAME;

            output = arg + 1;
            if (*output == '>') {
                outputFile = FILE_APPEND;
                output++;
            }
            if (*output == '&') {
                /* Redirect stderr too */
                output++;
                dup_error = 1;
            }
            if (*output == '@') {
                outputFile = FILE_HANDLE;
                output++;
            }
            if (!*output && ++i < argc) {
                output = Jim_String(argv[i]);
            }
            if (dup_error) {
                errorFile = outputFile;
                error = output;
            }
        }
        else if (arg[0] == '2' && arg[1] == '>') {
            error = arg + 2;
            errorFile = FILE_NAME;

            if (*error == '@') {
                errorFile = FILE_HANDLE;
                error++;
            }
            else if (*error == '>') {
                errorFile = FILE_APPEND;
                error++;
            }
            if (!*error && ++i < argc) {
                error = Jim_String(argv[i]);
            }
        }
        else {
            if (strcmp(arg, "|") == 0 || strcmp(arg, "|&") == 0) {
                if (i == lastBar + 1 || i == argc - 1) {
                    Jim_SetResultString(interp, "illegal use of | or |& in command", -1);
                    goto badargs;
                }
                lastBar = i;
                cmdCount++;
            }
            /* Either |, |& or a "normal" arg, so store it in the arg array */
            arg_array[arg_count++] = (char *)arg;
            continue;
        }

        if (i >= argc) {
            Jim_SetResultFormatted(interp, "can't specify \"%s\" as last word in command", arg);
            goto badargs;
        }
    }

    if (arg_count == 0) {
        Jim_SetResultString(interp, "didn't specify command to execute", -1);
badargs:
        Jim_Free(arg_array);
        return -1;
    }

    /* Must do this before vfork(), so do it now */
    orig_environ = Jim_GetEnviron();
    Jim_SetEnviron(JimBuildEnv(interp));

    /*
     * Set up the redirected input source for the pipeline, if
     * so requested.
     */
    if (input != NULL) {
        if (inputFile == FILE_TEXT) {
            /*
             * Immediate data in command.  Create temporary file and
             * put data into file.
             */

#define TMP_STDIN_NAME "/tmp/tcl.in.XXXXXX"
            char inName[sizeof(TMP_STDIN_NAME) + 1];
            int length;

            strcpy(inName, TMP_STDIN_NAME);
            inputId = mkstemp(inName);
            if (inputId < 0) {
                Jim_SetResultErrno(interp, "couldn't create input file for command");
                goto error;
            }
            length = strlen(input);
            if (write(inputId, input, length) != length) {
                Jim_SetResultErrno(interp, "couldn't write file input for command");
                goto error;
            }
            if (lseek(inputId, 0L, SEEK_SET) == -1 || unlink(inName) == -1) {
                Jim_SetResultErrno(interp, "couldn't reset or remove input file for command");
                goto error;
            }
        }
        else if (inputFile == FILE_HANDLE) {
            /* Should be a file descriptor */
            Jim_Obj *fhObj = Jim_NewStringObj(interp, input, -1);
            FILE *fh = Jim_AioFilehandle(interp, fhObj);

            Jim_FreeNewObj(interp, fhObj);
            if (fh == NULL) {
                goto error;
            }
            inputId = dup(fileno(fh));
        }
        else {
            /*
             * File redirection.  Just open the file.
             */
            inputId = open(input, O_RDONLY, 0);
            if (inputId < 0) {
                Jim_SetResultFormatted(interp, "couldn't read file \"%s\": %s", input,
                    strerror(errno));
                goto error;
            }
        }
    }
    else if (inPipePtr != NULL) {
        if (pipe(pipeIds) != 0) {
            Jim_SetResultErrno(interp, "couldn't create input pipe for command");
            goto error;
        }
        inputId = pipeIds[0];
        *inPipePtr = pipeIds[1];
        pipeIds[0] = pipeIds[1] = -1;
    }

    /*
     * Set up the redirected output sink for the pipeline from one
     * of two places, if requested.
     */
    if (output != NULL) {
        if (outputFile == FILE_HANDLE) {
            Jim_Obj *fhObj = Jim_NewStringObj(interp, output, -1);
            FILE *fh = Jim_AioFilehandle(interp, fhObj);

            Jim_FreeNewObj(interp, fhObj);
            if (fh == NULL) {
                goto error;
            }
            fflush(fh);
            lastOutputId = dup(fileno(fh));
        }
        else {
            /*
             * Output is to go to a file.
             */
            int mode = O_WRONLY | O_CREAT | O_TRUNC;

            if (outputFile == FILE_APPEND) {
                mode = O_WRONLY | O_CREAT | O_APPEND;
            }

            lastOutputId = open(output, mode, 0666);
            if (lastOutputId < 0) {
                Jim_SetResultFormatted(interp, "couldn't write file \"%s\": %s", output,
                    strerror(errno));
                goto error;
            }
        }
    }
    else if (outPipePtr != NULL) {
        /*
         * Output is to go to a pipe.
         */
        if (pipe(pipeIds) != 0) {
            Jim_SetResultErrno(interp, "couldn't create output pipe");
            goto error;
        }
        lastOutputId = pipeIds[1];
        *outPipePtr = pipeIds[0];
        pipeIds[0] = pipeIds[1] = -1;
    }

    /* If we are redirecting stderr with 2>filename or 2>@fileId, then we ignore errFilePtr */
    if (error != NULL) {
        if (errorFile == FILE_HANDLE) {
            if (strcmp(error, "1") == 0) {
                /* Special 2>@1 */
                if (lastOutputId >= 0) {
                    errorId = dup(lastOutputId);
                }
                else {
                    /* No redirection of stdout, so just use 2>@stdout */
                    error = "stdout";
                }
            }
            if (errorId < 0) {
                Jim_Obj *fhObj = Jim_NewStringObj(interp, error, -1);
                FILE *fh = Jim_AioFilehandle(interp, fhObj);

                Jim_FreeNewObj(interp, fhObj);
                if (fh == NULL) {
                    goto error;
                }
                fflush(fh);
                errorId = dup(fileno(fh));
            }
        }
        else {
            /*
             * Output is to go to a file.
             */
            int mode = O_WRONLY | O_CREAT | O_TRUNC;

            if (errorFile == FILE_APPEND) {
                mode = O_WRONLY | O_CREAT | O_APPEND;
            }

            errorId = open(error, mode, 0666);
            if (errorId < 0) {
                Jim_SetResultFormatted(interp, "couldn't write file \"%s\": %s", error,
                    strerror(errno));
            }
        }
    }
    else if (errFilePtr != NULL) {
        /*
         * Set up the standard error output sink for the pipeline, if
         * requested.  Use a temporary file which is opened, then deleted.
         * Could potentially just use pipe, but if it filled up it could
         * cause the pipeline to deadlock:  we'd be waiting for processes
         * to complete before reading stderr, and processes couldn't complete
         * because stderr was backed up.
         */

#define TMP_STDERR_NAME "/tmp/tcl.err.XXXXXX"
        char errName[sizeof(TMP_STDERR_NAME) + 1];

        strcpy(errName, TMP_STDERR_NAME);
        errorId = mkstemp(errName);
        if (errorId < 0) {
          errFileError:
            Jim_SetResultErrno(interp, "couldn't create error file for command");
            goto error;
        }
        *errFilePtr = open(errName, O_RDONLY, 0);
        if (*errFilePtr < 0) {
            goto errFileError;
        }
        if (unlink(errName) == -1) {
            Jim_SetResultErrno(interp, "couldn't remove error file for command");
            goto error;
        }
    }

    /*
     * Scan through the argc array, forking off a process for each
     * group of arguments between "|" arguments.
     */

    pidPtr = (int *)Jim_Alloc(cmdCount * sizeof(*pidPtr));
    for (i = 0; i < numPids; i++) {
        pidPtr[i] = -1;
    }
    for (firstArg = 0; firstArg < arg_count; numPids++, firstArg = lastArg + 1) {
        int pipe_dup_err = 0;
        int origErrorId = errorId;
        char execerr[64];
        int execerrlen;

        for (lastArg = firstArg; lastArg < arg_count; lastArg++) {
            if (arg_array[lastArg][0] == '|') {
                if (arg_array[lastArg][1] == '&') {
                    pipe_dup_err = 1;
                }
                break;
            }
        }
        /* Replace | with NULL for execv() */
        arg_array[lastArg] = NULL;
        if (lastArg == arg_count) {
            outputId = lastOutputId;
        }
        else {
            if (pipe(pipeIds) != 0) {
                Jim_SetResultErrno(interp, "couldn't create pipe");
                goto error;
            }
            outputId = pipeIds[1];
        }
        execName = arg_array[firstArg];

        /* Now fork the child */

        /*
         * Disable SIGPIPE signals:  if they were allowed, this process
         * might go away unexpectedly if children misbehave.  This code
         * can potentially interfere with other application code that
         * expects to handle SIGPIPEs;  what's really needed is an
         * arbiter for signals to allow them to be "shared".
         */
        if (table->info == NULL) {
            (void)signal(SIGPIPE, SIG_IGN);
        }

        /* Need to do this befor vfork() */
        if (pipe_dup_err) {
            errorId = outputId;
        }

        /* Need to prep an error message before vfork(), just in case */
        snprintf(execerr, sizeof(execerr), "couldn't exec \"%s\"", execName);
        execerrlen = strlen(execerr);

        /*
         * Make a new process and enter it into the table if the fork
         * is successful.
         */
        pid = vfork();
        if (pid < 0) {
            Jim_SetResultErrno(interp, "couldn't fork child process");
            goto error;
        }
        if (pid == 0) {
            /* Child */

            if (inputId != -1) dup2(inputId, 0);
            if (outputId != -1) dup2(outputId, 1);
            if (errorId != -1) dup2(errorId, 2);

            for (i = 3; (i <= outputId) || (i <= inputId) || (i <= errorId); i++) {
                close(i);
            }

            execvp(execName, &arg_array[firstArg]);

            /* we really can ignore the error here! */
            IGNORE_RC(write(2, execerr, execerrlen));
            _exit(127);
        }

        /* parent */

        /*
         * Enlarge the wait table if there isn't enough space for a new
         * entry.
         */
        if (table->used == table->size) {
            table->size += WAIT_TABLE_GROW_BY;
            table->info = Jim_Realloc(table->info, table->size * sizeof(*table->info));
        }

        table->info[table->used].pid = pid;
        table->info[table->used].flags = 0;
        table->used++;

        pidPtr[numPids] = pid;

        /* Restore in case of pipe_dup_err */
        errorId = origErrorId;

        /*
         * Close off our copies of file descriptors that were set up for
         * this child, then set up the input for the next child.
         */

        if (inputId != -1) {
            close(inputId);
        }
        if (outputId != -1) {
            close(outputId);
        }
        inputId = pipeIds[0];
        pipeIds[0] = pipeIds[1] = -1;
    }
    *pidArrayPtr = pidPtr;

    /*
     * All done.  Cleanup open files lying around and then return.
     */

  cleanup:
    if (inputId != -1) {
        close(inputId);
    }
    if (lastOutputId != -1) {
        close(lastOutputId);
    }
    if (errorId != -1) {
        close(errorId);
    }
    Jim_Free(arg_array);

    JimFreeEnv(interp, Jim_GetEnviron(), orig_environ);
    Jim_SetEnviron(orig_environ);

    return numPids;

    /*
     * An error occurred.  There could have been extra files open, such
     * as pipes between children.  Clean them all up.  Detach any child
     * processes that have been created.
     */

  error:
    if ((inPipePtr != NULL) && (*inPipePtr != -1)) {
        close(*inPipePtr);
        *inPipePtr = -1;
    }
    if ((outPipePtr != NULL) && (*outPipePtr != -1)) {
        close(*outPipePtr);
        *outPipePtr = -1;
    }
    if ((errFilePtr != NULL) && (*errFilePtr != -1)) {
        close(*errFilePtr);
        *errFilePtr = -1;
    }
    if (pipeIds[0] != -1) {
        close(pipeIds[0]);
    }
    if (pipeIds[1] != -1) {
        close(pipeIds[1]);
    }
    if (pidPtr != NULL) {
        for (i = 0; i < numPids; i++) {
            if (pidPtr[i] != -1) {
                JimDetachPids(interp, 1, &pidPtr[i]);
            }
        }
        Jim_Free(pidPtr);
    }
    numPids = -1;
    goto cleanup;
}

/*
 *----------------------------------------------------------------------
 *
 * CleanupChildren --
 *
 *  This is a utility procedure used to wait for child processes
 *  to exit, record information about abnormal exits, and then
 *  collect any stderr output generated by them.
 *
 * Results:
 *  The return value is a standard Tcl result.  If anything at
 *  weird happened with the child processes, JIM_ERROR is returned
 *  and a message is left in interp->result.
 *
 * Side effects:
 *  If the last character of interp->result is a newline, then it
 *  is removed.  File errorId gets closed, and pidPtr is freed
 *  back to the storage allocator.
 *
 *----------------------------------------------------------------------
 */

static int Jim_CleanupChildren(Jim_Interp *interp, int numPids, int *pidPtr, int errorId)
{
    struct WaitInfoTable *table = Jim_CmdPrivData(interp);
    int result = JIM_OK;
    int i;

    for (i = 0; i < numPids; i++) {
        int waitStatus = 0;
        if (JimWaitPid(table, pidPtr[i], &waitStatus) > 0) {
            if (JimCheckWaitStatus(interp, pidPtr[i], waitStatus) != JIM_OK) {
                result = JIM_ERR;
            }
        }
    }
    Jim_Free(pidPtr);

    /*
     * Read the standard error file.  If there's anything there,
     * then add the file's contents to the result
     * string.
     */
    if (errorId >= 0) {
        if (JimAppendStreamToString(interp, errorId, Jim_GetResult(interp)) != JIM_OK) {
            Jim_SetResultErrno(interp, "error reading from stderr output file");
            result = JIM_ERR;
        }
        close(errorId);
    }

    JimTrimTrailingNewline(interp);

    return result;
}

int Jim_execInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "exec", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "exec", Jim_ExecCmd, JimAllocWaitInfoTable(), JimFreeWaitInfoTable);
    return JIM_OK;
}
#else
/* e.g. Windows. Poor mans implementation of exec with system()
 *      The system() call *may* do command line redirection, etc.
 *      The standard output is not available.
 *      Can't redirect filehandles.
 */
static int Jim_ExecCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *cmdlineObj = Jim_NewEmptyStringObj(interp);
    int i, j;
    int rc;

    /* Create a quoted command line */
    for (i = 1; i < argc; i++) {
        int len;
        const char *arg = Jim_GetString(argv[i], &len);

        if (i > 1) {
            Jim_AppendString(interp, cmdlineObj, " ", 1);
        }
        if (strpbrk(arg, "\\\" ") == NULL) {
            /* No quoting required */
            Jim_AppendString(interp, cmdlineObj, arg, len);
            continue;
        }

        Jim_AppendString(interp, cmdlineObj, "\"", 1);
        for (j = 0; j < len; j++) {
            if (arg[j] == '\\' || arg[j] == '"') {
                Jim_AppendString(interp, cmdlineObj, "\\", 1);
            }
            Jim_AppendString(interp, cmdlineObj, &arg[j], 1);
        }
        Jim_AppendString(interp, cmdlineObj, "\"", 1);
    }
    rc = system(Jim_String(cmdlineObj));

    Jim_FreeNewObj(interp, cmdlineObj);

    if (rc) {
        Jim_Obj *errorCode = Jim_NewListObj(interp, NULL, 0);
        Jim_ListAppendElement(interp, errorCode, Jim_NewStringObj(interp, "CHILDSTATUS", -1));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, 0));
        Jim_ListAppendElement(interp, errorCode, Jim_NewIntObj(interp, rc));
        Jim_SetGlobalVariableStr(interp, "errorCode", errorCode);
        return JIM_ERR;
    }

    return JIM_OK;
}

int Jim_execInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "exec", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "exec", Jim_ExecCmd, NULL, NULL);
    return JIM_OK;
}
#endif

/*
 * tcl_clock.c
 *
 * Implements the clock command
 */

/* For strptime() */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>


static int clock_cmd_format(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    /* How big is big enough? */
    char buf[100];
    time_t t;
    long seconds;

    const char *format = "%a %b  %d %H:%M:%S %Z %Y";

    if (argc == 2 || (argc == 3 && !Jim_CompareStringImmediate(interp, argv[1], "-format"))) {
        return -1;
    }

    if (argc == 3) {
        format = Jim_String(argv[2]);
    }

    if (Jim_GetLong(interp, argv[0], &seconds) != JIM_OK) {
        return JIM_ERR;
    }
    t = seconds;

    strftime(buf, sizeof(buf), format, localtime(&t));

    Jim_SetResultString(interp, buf, -1);

    return JIM_OK;
}

#ifdef HAVE_STRPTIME
static int clock_cmd_scan(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char *pt;
    struct tm tm;
    time_t now = time(0);

    if (!Jim_CompareStringImmediate(interp, argv[1], "-format")) {
        return -1;
    }

    /* Initialise with the current date/time */
    localtime_r(&now, &tm);

    pt = strptime(Jim_String(argv[0]), Jim_String(argv[2]), &tm);
    if (pt == 0 || *pt != 0) {
        Jim_SetResultString(interp, "Failed to parse time according to format", -1);
        return JIM_ERR;
    }

    /* Now convert into a time_t */
    Jim_SetResultInt(interp, mktime(&tm));

    return JIM_OK;
}
#endif

static int clock_cmd_seconds(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_SetResultInt(interp, time(NULL));

    return JIM_OK;
}

static int clock_cmd_micros(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    Jim_SetResultInt(interp, (jim_wide) tv.tv_sec * 1000000 + tv.tv_usec);

    return JIM_OK;
}

static int clock_cmd_millis(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    Jim_SetResultInt(interp, (jim_wide) tv.tv_sec * 1000 + tv.tv_usec / 1000);

    return JIM_OK;
}

static const jim_subcmd_type clock_command_table[] = {
    {   .cmd = "seconds",
        .function = clock_cmd_seconds,
        .minargs = 0,
        .maxargs = 0,
        .description = "Returns the current time as seconds since the epoch"
    },
    {   .cmd = "clicks",
        .function = clock_cmd_micros,
        .minargs = 0,
        .maxargs = 0,
        .description = "Returns the current time in 'clicks'"
    },
    {   .cmd = "microseconds",
        .function = clock_cmd_micros,
        .minargs = 0,
        .maxargs = 0,
        .description = "Returns the current time in microseconds"
    },
    {   .cmd = "milliseconds",
        .function = clock_cmd_millis,
        .minargs = 0,
        .maxargs = 0,
        .description = "Returns the current time in milliseconds"
    },
    {   .cmd = "format",
        .args = "seconds ?-format format?",
        .function = clock_cmd_format,
        .minargs = 1,
        .maxargs = 3,
        .description = "Format the given time"
    },
#ifdef HAVE_STRPTIME
    {   .cmd = "scan",
        .args = "str -format format",
        .function = clock_cmd_scan,
        .minargs = 3,
        .maxargs = 3,
        .description = "Determine the time according to the given format"
    },
#endif
    { 0 }
};

int Jim_clockInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "clock", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "clock", Jim_SubCmdProc, (void *)clock_command_table, NULL);
    return JIM_OK;
}

/* 
 * Implements the array command for jim
 *
 * (c) 2008 Steve Bennett <steveb@workware.net.au>
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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>


static int array_cmd_exists(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    /* Just a regular [info exists] */
    Jim_SetResultInt(interp, Jim_GetVariable(interp, argv[0], 0) != 0);
    return JIM_OK;
}

static int array_cmd_get(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    int len;
    int all = 0;
    Jim_Obj *resultObj;
    Jim_Obj *objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);
    Jim_Obj *dictObj;
    Jim_Obj **dictValuesObj;

    if (!objPtr) {
        return JIM_OK;
    }

    if (argc == 1 || Jim_CompareStringImmediate(interp, argv[1], "*")) {
        all = 1;
    }

    /* If it is a dictionary or list with an even number of elements, nothing else to do */
    if (all) {
        if (Jim_IsDict(objPtr) || (Jim_IsList(objPtr) && Jim_ListLength(interp, objPtr) % 2 == 0)) {
            Jim_SetResult(interp, objPtr);
            return JIM_OK;
        }
    }

    if (Jim_DictKeysVector(interp, objPtr, NULL, 0, &dictObj, JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    if (Jim_DictPairs(interp, dictObj, &dictValuesObj, &len) != JIM_OK) {
        return JIM_ERR;
    }

    if (all) {
        /* Return the whole array */
        Jim_SetResult(interp, dictObj);
    }
    else {
        /* Only return the matching values */
        resultObj = Jim_NewListObj(interp, NULL, 0);

        for (i = 0; i < len; i += 2) {
            if (Jim_StringMatchObj(interp, argv[1], dictValuesObj[i], 0)) {
                Jim_ListAppendElement(interp, resultObj, dictValuesObj[i]);
                Jim_ListAppendElement(interp, resultObj, dictValuesObj[i + 1]);
            }
        }

        Jim_SetResult(interp, resultObj);
    }
    Jim_Free(dictValuesObj);
    return JIM_OK;

}

static int array_cmd_names(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);

    if (!objPtr) {
        return JIM_OK;
    }

    return Jim_DictKeys(interp, objPtr, argc == 1 ? NULL : argv[1]);
}

static int array_cmd_unset(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    int len;
    Jim_Obj *resultObj;
    Jim_Obj *objPtr;
    Jim_Obj *dictObj;
    Jim_Obj **dictValuesObj;

    if (argc == 1 || Jim_CompareStringImmediate(interp, argv[1], "*")) {
        /* Unset the whole array */
        Jim_UnsetVariable(interp, argv[0], JIM_NONE);
        return JIM_OK;
    }

    objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);

    if (Jim_DictKeysVector(interp, objPtr, NULL, 0, &dictObj, JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    if (Jim_DictPairs(interp, dictObj, &dictValuesObj, &len) != JIM_OK) {
        return JIM_ERR;
    }

    /* Create a new object with the values which don't match */
    resultObj = Jim_NewDictObj(interp, NULL, 0);

    for (i = 0; i < len; i += 2) {
        if (!Jim_StringMatchObj(interp, argv[1], dictValuesObj[i], 0)) {
            Jim_DictAddElement(interp, resultObj, dictValuesObj[i], dictValuesObj[i + 1]);
        }
    }
    Jim_Free(dictValuesObj);

    Jim_SetVariable(interp, argv[0], resultObj);
    return JIM_OK;
}

static int array_cmd_size(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    int len = 0;

    /* Not found means zero length */
    objPtr = Jim_GetVariable(interp, argv[0], JIM_NONE);
    if (objPtr) {
        len = Jim_DictSize(interp, objPtr);
        if (len < 0) {
            return JIM_ERR;
        }
    }

    Jim_SetResultInt(interp, len);

    return JIM_OK;
}

static int array_cmd_set(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int i;
    int len;
    int rc = JIM_OK;
    Jim_Obj *listObj = argv[1];

    if (Jim_GetVariable(interp, argv[0], JIM_NONE) == NULL) {
        /* Doesn't exist, so just set the list directly */
        return Jim_SetVariable(interp, argv[0], listObj);
    }

    len = Jim_ListLength(interp, listObj);
    if (len % 2) {
        Jim_SetResultString(interp, "list must have an even number of elements", -1);
        return JIM_ERR;
    }
    for (i = 0; i < len && rc == JIM_OK; i += 2) {
        Jim_Obj *nameObj;
        Jim_Obj *valueObj;

        Jim_ListIndex(interp, listObj, i, &nameObj, JIM_NONE);
        Jim_ListIndex(interp, listObj, i + 1, &valueObj, JIM_NONE);

        rc = Jim_SetDictKeysVector(interp, argv[0], &nameObj, 1, valueObj);
    }

    return rc;
}

static const jim_subcmd_type array_command_table[] = {
        {       .cmd = "exists",
                .args = "arrayName",
                .function = array_cmd_exists,
                .minargs = 1,
                .maxargs = 1,
                .description = "Does array exist?"
        },
        {       .cmd = "get",
                .args = "arrayName ?pattern?",
                .function = array_cmd_get,
                .minargs = 1,
                .maxargs = 2,
                .description = "Array contents as name value list"
        },
        {       .cmd = "names",
                .args = "arrayName ?pattern?",
                .function = array_cmd_names,
                .minargs = 1,
                .maxargs = 2,
                .description = "Array keys as a list"
        },
        {       .cmd = "set",
                .args = "arrayName list",
                .function = array_cmd_set,
                .minargs = 2,
                .maxargs = 2,
                .description = "Set array from list"
        },
        {       .cmd = "size",
                .args = "arrayName",
                .function = array_cmd_size,
                .minargs = 1,
                .maxargs = 1,
                .description = "Number of elements in array"
        },
        {       .cmd = "unset",
                .args = "arrayName ?pattern?",
                .function = array_cmd_unset,
                .minargs = 1,
                .maxargs = 2,
                .description = "Unset elements of an array"
        },
        {       .cmd = 0,
        }
};

int Jim_arrayInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "array", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "array", Jim_SubCmdProc, (void *)array_command_table, NULL);
    return JIM_OK;
}
int Jim_InitStaticExtensions(Jim_Interp *interp)
{
extern int Jim_bootstrapInit(Jim_Interp *);
extern int Jim_aioInit(Jim_Interp *);
extern int Jim_readdirInit(Jim_Interp *);
extern int Jim_globInit(Jim_Interp *);
extern int Jim_regexpInit(Jim_Interp *);
extern int Jim_fileInit(Jim_Interp *);
extern int Jim_execInit(Jim_Interp *);
extern int Jim_clockInit(Jim_Interp *);
extern int Jim_arrayInit(Jim_Interp *);
extern int Jim_stdlibInit(Jim_Interp *);
extern int Jim_tclcompatInit(Jim_Interp *);
Jim_bootstrapInit(interp);
Jim_aioInit(interp);
Jim_readdirInit(interp);
Jim_globInit(interp);
Jim_regexpInit(interp);
Jim_fileInit(interp);
Jim_execInit(interp);
Jim_clockInit(interp);
Jim_arrayInit(interp);
Jim_stdlibInit(interp);
Jim_tclcompatInit(interp);
return JIM_OK;
}

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

#include <unistd.h>
#include <sys/time.h>


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

const char *jim_tt_name(int type);

#ifdef JIM_DEBUG_PANIC
static void JimPanicDump(int panic_condition, Jim_Interp *interp, const char *fmt, ...);
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
static void JimChangeCallFrameId(Jim_Interp *interp, Jim_CallFrame *cf);
static void JimFreeCallFrame(Jim_Interp *interp, Jim_CallFrame *cf, int flags);
static int ListSetIndex(Jim_Interp *interp, Jim_Obj *listPtr, int listindex, Jim_Obj *newObjPtr,
    int flags);
static Jim_Obj *JimExpandDictSugar(Jim_Interp *interp, Jim_Obj *objPtr);
static void SetDictSubstFromAny(Jim_Interp *interp, Jim_Obj *objPtr);
static void JimSetFailedEnumResult(Jim_Interp *interp, const char *arg, const char *badtype,
    const char *prefix, const char *const *tablePtr, const char *name);
static void JimDeleteLocalProcs(Jim_Interp *interp);
static int JimCallProcedure(Jim_Interp *interp, Jim_Cmd *cmd, const char *filename, int linenr,
    int argc, Jim_Obj *const *argv);
static int JimEvalObjVector(Jim_Interp *interp, int objc, Jim_Obj *const *objv,
    const char *filename, int linenr);
static int JimGetWideNoErr(Jim_Interp *interp, Jim_Obj *objPtr, jim_wide * widePtr);
static int JimSign(jim_wide w);
static int JimValidName(Jim_Interp *interp, const char *type, Jim_Obj *nameObjPtr);
static void JimPrngSeed(Jim_Interp *interp, unsigned char *seed, int seedLen);
static void JimRandomBytes(Jim_Interp *interp, void *dest, unsigned int len);


static const Jim_HashTableType JimVariablesHashTableType;

/* Fast access to the int (wide) value of an object which is known to be of int type */
#define JimWideValue(objPtr) (objPtr)->internalRep.wideValue

#define JimObjTypeName(O) (objPtr->typePtr ? objPtr->typePtr->name : "none")

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
 *       slen is a char length, not byte counts.
 */
static int GlobMatch(const char *pattern, const char *string, int nocase)
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
                    if (GlobMatch(pattern, string, nocase))
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

static int JimStringMatch(Jim_Interp *interp, Jim_Obj *patternObj, const char *string, int nocase)
{
    return GlobMatch(Jim_String(patternObj), string, nocase);
}

/**
 * string comparison works on binary data.
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
 * No-case version.
 *
 * If maxchars is -1, compares to end of string.
 * Otherwise compares at most 'maxchars' characters.
 */
static int JimStringCompareNoCase(const char *s1, const char *s2, int maxchars)
{
    while (*s1 && *s2 && maxchars) {
        int c1, c2;
        s1 += utf8_tounicode_case(s1, &c1, 1);
        s2 += utf8_tounicode_case(s2, &c2, 1);
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

int Jim_WideToString(char *buf, jim_wide wideValue)
{
    const char *fmt = "%" JIM_WIDE_MODIFIER;

    return sprintf(buf, fmt, wideValue);
}

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

int Jim_StringToWide(const char *str, jim_wide * widePtr, int base)
{
    char *endptr;

    *widePtr = strtoull(str, &endptr, base);

    return JimCheckConversion(str, endptr);
}

int Jim_DoubleToString(char *buf, double doubleValue)
{
    int len;
    char *buf0 = buf;

    len = sprintf(buf, "%.12g", doubleValue);

    /* Add a final ".0" if it's a number. But not
     * for NaN or InF */
    while (*buf) {
        if (*buf == '.' || isalpha(UCHAR(*buf))) {
            /* inf -> Inf, nan -> Nan */
            if (*buf == 'i' || *buf == 'n') {
                *buf = toupper(UCHAR(*buf));
            }
            if (*buf == 'I') {
                /* Infinity -> Inf */
                buf[3] = '\0';
                len = buf - buf0 + 3;
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
/* Note that 'interp' may be NULL if not available in the
 * context of the panic. It's only useful to get the error
 * file descriptor, it will default to stderr otherwise. */
void JimPanicDump(int condition, Jim_Interp *interp, const char *fmt, ...)
{
    va_list ap;

    if (!condition) {
        return;
    }

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
            fprintf(stderr, "[backtrace] %s" JIM_NL, strings[i]);
        fprintf(stderr, "[backtrace] Include the above lines and the output" JIM_NL);
        fprintf(stderr, "[backtrace] of 'nm <executable>' in the bug report." JIM_NL);
    }
#endif

    abort();
}
#endif

/* -----------------------------------------------------------------------------
 * Memory allocation
 * ---------------------------------------------------------------------------*/

void *Jim_Alloc(int size)
{
    return malloc(size);
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

/* Returns microseconds of CPU used since start. */
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
static int JimExpandHashTableIfNeeded(Jim_HashTable *ht);
static unsigned int JimHashTableNextPower(unsigned int size);
static int JimInsertHashEntry(Jim_HashTable *ht, const void *key);

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

/* reset a hashtable already initialized with ht_init().
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
int Jim_InitHashTable(Jim_HashTable *ht, const Jim_HashTableType *type, void *privDataPtr)
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
    Jim_HashTable n;            /* the new hashtable */
    unsigned int realsize = JimHashTableNextPower(size), i;

    /* the size is invalid if it is smaller than the number of
     * elements already inside the hashtable */
    if (ht->used >= size)
        return JIM_ERR;

    Jim_InitHashTable(&n, ht->type, ht->privdata);
    n.size = realsize;
    n.sizemask = realsize - 1;
    n.table = Jim_Alloc(realsize * sizeof(Jim_HashEntry *));

    /* Initialize all the pointers to NULL */
    memset(n.table, 0, realsize * sizeof(Jim_HashEntry *));

    /* Copy all the elements from the old to the new table:
     * note that if the old hash table is empty ht->size is zero,
     * so Jim_ExpandHashTable just creates an hash table. */
    n.used = ht->used;
    for (i = 0; i < ht->size && ht->used > 0; i++) {
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
    return JIM_OK;
}

/* Add an element to the target hash table */
int Jim_AddHashEntry(Jim_HashTable *ht, const void *key, void *val)
{
    int idx;
    Jim_HashEntry *entry;

    /* Get the index of the new element, or -1 if
     * the element already exists. */
    if ((idx = JimInsertHashEntry(ht, key)) == -1)
        return JIM_ERR;

    /* Allocates the memory and stores key */
    entry = Jim_Alloc(sizeof(*entry));
    entry->next = ht->table[idx];
    ht->table[idx] = entry;

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

/* Destroy an entire hash table */
int Jim_FreeHashTable(Jim_HashTable *ht)
{
    unsigned int i;

    /* Free all the elements */
    for (i = 0; i < ht->size && ht->used > 0; i++) {
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

    if (ht->size == 0)
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
static int JimExpandHashTableIfNeeded(Jim_HashTable *ht)
{
    /* If the hash table is empty expand it to the intial size,
     * if the table is "full" dobule its size. */
    if (ht->size == 0)
        return Jim_ExpandHashTable(ht, JIM_HT_INITIAL_SIZE);
    if (ht->size == ht->used)
        return Jim_ExpandHashTable(ht, ht->size * 2);
    return JIM_OK;
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
    while (he) {
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
    char *copy = Jim_Alloc(len + 1);

    JIM_NOTUSED(privdata);

    memcpy(copy, key, len);
    copy[len] = '\0';
    return copy;
}

static void *JimStringKeyValCopyHTValDup(void *privdata, const void *val)
{
    int len = strlen(val);
    char *copy = Jim_Alloc(len + 1);

    JIM_NOTUSED(privdata);

    memcpy(copy, val, len);
    copy[len] = '\0';
    return copy;
}

static int JimStringCopyHTKeyCompare(void *privdata, const void *key1, const void *key2)
{
    JIM_NOTUSED(privdata);

    return strcmp(key1, key2) == 0;
}

static void JimStringCopyHTKeyDestructor(void *privdata, const void *key)
{
    JIM_NOTUSED(privdata);

    Jim_Free((void *)key);      /* ATTENTION: const cast */
}

static void JimStringKeyValCopyHTValDestructor(void *privdata, void *val)
{
    JIM_NOTUSED(privdata);

    Jim_Free((void *)val);      /* ATTENTION: const cast */
}

#if 0
static Jim_HashTableType JimStringCopyHashTableType = {
    JimStringCopyHTHashFunction,        /* hash function */
    JimStringCopyHTKeyDup,      /* key dup */
    NULL,                       /* val dup */
    JimStringCopyHTKeyCompare,  /* key compare */
    JimStringCopyHTKeyDestructor,       /* key destructor */
    NULL                        /* val destructor */
};
#endif

/* This is like StringCopy but does not auto-duplicate the key.
 * It's used for intepreter's shared strings. */
static const Jim_HashTableType JimSharedStringsHashTableType = {
    JimStringCopyHTHashFunction,        /* hash function */
    NULL,                       /* key dup */
    NULL,                       /* val dup */
    JimStringCopyHTKeyCompare,  /* key compare */
    JimStringCopyHTKeyDestructor,       /* key destructor */
    NULL                        /* val destructor */
};

/* This is like StringCopy but also automatically handle dynamic
 * allocated C strings as values. */
static const Jim_HashTableType JimStringKeyValCopyHashTableType = {
    JimStringCopyHTHashFunction,        /* hash function */
    JimStringCopyHTKeyDup,      /* key dup */
    JimStringKeyValCopyHTValDup,        /* val dup */
    JimStringCopyHTKeyCompare,  /* key compare */
    JimStringCopyHTKeyDestructor,       /* key destructor */
    JimStringKeyValCopyHTValDestructor, /* val destructor */
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
    JimStringCopyHTHashFunction,        /* hash function */
    JimStringCopyHTKeyDup,      /* key dup */
    NULL,                       /* val dup */
    JimStringCopyHTKeyCompare,  /* key compare */
    JimStringCopyHTKeyDestructor,       /* key destructor */
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
 * Parser
 * ---------------------------------------------------------------------------*/

/* Token types */
#define JIM_TT_NONE    0           /* No token returned */
#define JIM_TT_STR     1          /* simple string */
#define JIM_TT_ESC     2          /* string that needs escape chars conversion */
#define JIM_TT_VAR     3          /* var substitution */
#define JIM_TT_DICTSUGAR   4      /* Syntax sugar for [dict get], $foo(bar) */
#define JIM_TT_CMD     5          /* command substitution */
/* Note: Keep these three together for TOKEN_IS_SEP() */
#define JIM_TT_SEP     6          /* word separator. arg is # of tokens. -ve if {*} */
#define JIM_TT_EOL     7          /* line separator */
#define JIM_TT_EOF     8          /* end of script */

#define JIM_TT_LINE    9          /* special 'start-of-line' token. arg is # of arguments to the command. -ve if {*} */
#define JIM_TT_WORD   10          /* special 'start-of-word' token. arg is # of tokens to combine. -ve if {*} */

/* Additional token types needed for expressions */
#define JIM_TT_SUBEXPR_START  11
#define JIM_TT_SUBEXPR_END    12
#define JIM_TT_EXPR_INT       13
#define JIM_TT_EXPR_DOUBLE    14

#define JIM_TT_EXPRSUGAR      15  /* $(expression) */

/* Operator token types start here */
#define JIM_TT_EXPR_OP        20

#define TOKEN_IS_SEP(type) (type >= JIM_TT_SEP && type <= JIM_TT_EOF)

/* Parser states */
#define JIM_PS_DEF 0            /* Default state */
#define JIM_PS_QUOTE 1          /* Inside "" */
#define JIM_PS_DICTSUGAR 2      /* Tokenising abc(def) into 4 separate tokens */

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
    int state;                  /* Parser state */
    int comment;                /* Non zero if the next chars may be a comment. */
    char missing;               /* At end of parse, ' ' if complete, '{' if braces incomplete, '"' if quotes incomplete */
    int missingline;            /* Line number starting the missing token */
};

/**
 * Results of missing quotes, braces, etc. from parsing.
 */
struct JimParseResult {
    char missing;               /* From JimParserCtx.missing */
    int line;                   /* From JimParserCtx.missingline */
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
static void JimParseSubCmd(struct JimParserCtx *pc);
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
    pc->state = JIM_PS_DEF;
    pc->linenr = linenr;
    pc->comment = 1;
    pc->missing = ' ';
    pc->missingline = linenr;
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
                if (*(pc->p + 1) == '\n' && pc->state == JIM_PS_DEF) {
                    return JimParseSep(pc);
                }
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
                    pc->tstart = pc->tend = pc->p++;
                    pc->len--;
                    pc->tline = pc->linenr;
                    pc->tt = JIM_TT_STR;
                    return JIM_OK;
                }
                else
                    return JIM_OK;
                break;
            case '#':
                if (pc->comment) {
                    JimParseComment(pc);
                    continue;
                }
                else {
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
        (*pc->p == '\\' && *(pc->p + 1) == '\n')) {
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
    while (*pc->p == ' ' || *pc->p == '\n' || *pc->p == '\t' || *pc->p == '\r' || *pc->p == ';') {
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
    pc->missing = '{';
    pc->missingline = pc->tline;
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
    pc->missing = '"';
    pc->missingline = line;
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
    pc->missing = '[';
    pc->missingline = line;
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
                pc->p += 2;
                pc->len -= 2;
                continue;
            }
            if (isalnum(UCHAR(*pc->p)) || *pc->p == '_') {
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
    int newword = (pc->tt == JIM_TT_SEP || pc->tt == JIM_TT_EOL ||
        pc->tt == JIM_TT_NONE || pc->tt == JIM_TT_STR);
    if (newword && *pc->p == '{') {
        return JimParseBrace(pc);
    }
    else if (newword && *pc->p == '"') {
        pc->state = JIM_PS_QUOTE;
        pc->p++;
        pc->len--;
        /* In case the end quote is missing */
        pc->missingline = pc->tline;
    }
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (1) {
        if (pc->len == 0) {
            if (pc->state == JIM_PS_QUOTE) {
                pc->missing = '"';
            }
            pc->tend = pc->p - 1;
            pc->tt = JIM_TT_ESC;
            return JIM_OK;
        }
        switch (*pc->p) {
            case '\\':
                if (pc->state == JIM_PS_DEF && *(pc->p + 1) == '\n') {
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
            case ';':
                if (pc->state == JIM_PS_DEF) {
                    pc->tend = pc->p - 1;
                    pc->tt = JIM_TT_ESC;
                    return JIM_OK;
                }
                else if (*pc->p == '\n') {
                    pc->linenr++;
                }
                break;
            case '"':
                if (pc->state == JIM_PS_QUOTE) {
                    pc->tend = pc->p - 1;
                    pc->tt = JIM_TT_ESC;
                    pc->p++;
                    pc->len--;
                    pc->state = JIM_PS_DEF;
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
        if (*pc->p == '\n') {
            pc->linenr++;
            if (*(pc->p - 1) != '\\') {
                pc->p++;
                pc->len--;
                return JIM_OK;
            }
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
                    case 'x':
                        /* A unicode or hex sequence.
                         * \u Expect 1-4 hex chars and convert to utf-8.
                         * \x Expect 1-2 hex chars and convert to hex.
                         * An invalid sequence means simply the escaped char.
                         */
                        {
                            int val = 0;
                            int k;

                            i++;

                            for (k = 0; k < (s[i] == 'u' ? 4 : 2); k++) {
                                int c = xdigitval(s[i + k + 1]);
                                if (c == -1) {
                                    break;
                                }
                                val = (val << 4) | c;
                            }
                            if (k) {
                                /* Got a valid sequence, so convert */
                                if (s[i] == 'u') {
                                    p += utf8_fromunicode(p, val);
                                }
                                else {
                                    *p++ = val;
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

/* Parses the given string to determine if it represents a complete script.
 *
 * This is useful for interactive shells implementation, for [info complete].
 *
 * If 'stateCharPtr' != NULL, the function stores ' ' on complete script,
 * '{' on scripts incomplete missing one or more '}' to be balanced.
 * '[' on scripts incomplete missing one or more ']' to be balanced.
 * '"' on scripts incomplete missing a '"' char.
 *
 * If the script is complete, 1 is returned, otherwise 0.
 */
int Jim_ScriptIsComplete(const char *s, int len, char *stateCharPtr)
{
    struct JimParserCtx parser;

    JimParserInit(&parser, s, len, 1);
    while (!parser.eof) {
        JimParseScript(&parser);
    }
    if (stateCharPtr) {
        *stateCharPtr = parser.missing;
    }
    return parser.missing == ' ';
}

/* -----------------------------------------------------------------------------
 * Tcl Lists parsing
 * ---------------------------------------------------------------------------*/
static int JimParseListSep(struct JimParserCtx *pc);
static int JimParseListStr(struct JimParserCtx *pc);
static int JimParseListQuote(struct JimParserCtx *pc);

static int JimParseList(struct JimParserCtx *pc)
{
    switch (*pc->p) {
        case ' ':
        case '\n':
        case '\t':
        case '\r':
            return JimParseListSep(pc);

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
    while (*pc->p == ' ' || *pc->p == '\t' || *pc->p == '\r' || *pc->p == '\n') {
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
        switch (*pc->p) {
            case '\\':
                if (--pc->len == 0) {
                    /* Trailing backslash */
                    pc->tend = pc->p;
                    return JIM_OK;
                }
                pc->tt = JIM_TT_ESC;
                pc->p++;
                break;
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                pc->tend = pc->p - 1;
                return JIM_OK;
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
    JimPanic((objPtr->refCount != 0, interp, "!!!Object %p freed with bad refcount %d, type=%s", objPtr,
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
    }
    else {
        objPtr->bytes = Jim_Alloc(length + 1);
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
    }
    else {
        Jim_InitStringRep(dupPtr, objPtr->bytes, objPtr->length);
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

/* Return the string representation for objPtr. If the object
 * string representation is invalid, calls the method to create
 * a new one starting from the internal representation of the object. */
const char *Jim_GetString(Jim_Obj *objPtr, int *lenPtr)
{
    if (objPtr->bytes == NULL) {
        /* Invalid string repr. Generate it. */
        JimPanic((objPtr->typePtr->updateStringProc == NULL, NULL, "UpdateStringProc called against '%s' type.", objPtr->typePtr->name));
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

static void DupStringInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    JIM_NOTUSED(interp);

    /* This is a bit subtle: the only caller of this function
     * should be Jim_DuplicateObj(), that will copy the
     * string representaion. After the copy, the duplicated
     * object will not have more room in teh buffer than
     * srcPtr->length bytes. So we just set it to length. */
    dupPtr->internalRep.strValue.maxLength = srcPtr->length;

    dupPtr->internalRep.strValue.charLength = srcPtr->internalRep.strValue.charLength;
}

static int SetStringFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
{
    /* Get a fresh string representation. */
    (void)Jim_String(objPtr);
    /* Free any other internal representation. */
    Jim_FreeIntRep(interp, objPtr);
    /* Set it as string, i.e. just set the maxLength field. */
    objPtr->typePtr = &stringObjType;
    objPtr->internalRep.strValue.maxLength = objPtr->length;
    /* Don't know the utf-8 length yet */
    objPtr->internalRep.strValue.charLength = -1;
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
    if (objPtr->typePtr != &stringObjType)
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
        objPtr->length = 0;
    }
    else {
        objPtr->bytes = Jim_Alloc(len + 1);
        objPtr->length = len;
        memcpy(objPtr->bytes, s, len);
        objPtr->bytes[len] = '\0';
    }

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

    if (len == -1)
        len = strlen(s);
    Jim_SetStringRep(objPtr, s, len);
    objPtr->typePtr = NULL;
    return objPtr;
}

/* Low-level string append. Use it only against objects
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

/* Higher level API to append strings to objects. */
void Jim_AppendString(Jim_Interp *interp, Jim_Obj *objPtr, const char *str, int len)
{
    JimPanic((Jim_IsShared(objPtr), interp, "Jim_AppendString called with shared object"));
    if (objPtr->typePtr != &stringObjType)
        SetStringFromAny(interp, objPtr);
    StringAppendString(objPtr, str, len);
}

void Jim_AppendObj(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *appendObjPtr)
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
        char *s = va_arg(ap, char *);

        if (s == NULL)
            break;
        Jim_AppendString(interp, objPtr, s, -1);
    }
    va_end(ap);
}

int Jim_StringEqObj(Jim_Obj *aObjPtr, Jim_Obj *bObjPtr)
{
    const char *aStr, *bStr;
    int aLen, bLen;

    if (aObjPtr == bObjPtr)
        return 1;
    aStr = Jim_GetString(aObjPtr, &aLen);
    bStr = Jim_GetString(bObjPtr, &bLen);
    if (aLen != bLen)
        return 0;
    return JimStringCompare(aStr, aLen, bStr, bLen) == 0;
}

int Jim_StringMatchObj(Jim_Interp *interp, Jim_Obj *patternObjPtr, Jim_Obj *objPtr, int nocase)
{
    return JimStringMatch(interp, patternObjPtr, Jim_String(objPtr), nocase);
}

int Jim_StringCompareObj(Jim_Interp *interp, Jim_Obj *firstObjPtr, Jim_Obj *secondObjPtr, int nocase)
{
    const char *s1, *s2;
    int l1, l2;

    s1 = Jim_GetString(firstObjPtr, &l1);
    s2 = Jim_GetString(secondObjPtr, &l2);

    if (nocase) {
        return JimStringCompareNoCase(s1, s2, -1);
    }
    return JimStringCompare(s1, l1, s2, l2);
}

/* Convert a range, as returned by Jim_GetRange(), into
 * an absolute index into an object of the specified length.
 * This function may return negative values, or values
 * bigger or equal to the length of the list if the index
 * is out of range. */
static int JimRelToAbsIndex(int len, int idx)
{
    if (idx < 0)
        return len + idx;
    return idx;
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
    }
    else {
        rangeLen = last - first + 1;
        if (rangeLen) {
            if (first < 0) {
                rangeLen += first;
                first = 0;
            }
            if (last >= len) {
                rangeLen -= (last - (len - 1));
                last = len - 1;
            }
        }
    }
    if (rangeLen < 0)
        rangeLen = 0;

    *firstPtr = first;
    *lastPtr = last;
    *rangeLenPtr = rangeLen;
}

Jim_Obj *Jim_StringByteRangeObj(Jim_Interp *interp,
    Jim_Obj *strObjPtr, Jim_Obj *firstObjPtr, Jim_Obj *lastObjPtr)
{
    int first, last;
    const char *str;
    int rangeLen;
    int bytelen;

    if (Jim_GetIndex(interp, firstObjPtr, &first) != JIM_OK ||
        Jim_GetIndex(interp, lastObjPtr, &last) != JIM_OK)
        return NULL;
    str = Jim_GetString(strObjPtr, &bytelen);
    first = JimRelToAbsIndex(bytelen, first);
    last = JimRelToAbsIndex(bytelen, last);
    JimRelToAbsRange(bytelen, first, last, &first, &last, &rangeLen);
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

    if (Jim_GetIndex(interp, firstObjPtr, &first) != JIM_OK ||
        Jim_GetIndex(interp, lastObjPtr, &last) != JIM_OK)
        return NULL;
    str = Jim_GetString(strObjPtr, &bytelen);
    len = Jim_Utf8Length(interp, strObjPtr);
    first = JimRelToAbsIndex(len, first);
    last = JimRelToAbsIndex(len, last);
    JimRelToAbsRange(len, first, last, &first, &last, &rangeLen);
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

static Jim_Obj *JimStringToLower(Jim_Interp *interp, Jim_Obj *strObjPtr)
{
    char *buf, *p;
    int len;
    const char *str;

    if (strObjPtr->typePtr != &stringObjType) {
        SetStringFromAny(interp, strObjPtr);
    }

    str = Jim_GetString(strObjPtr, &len);

    buf = p = Jim_Alloc(len + 1);
    while (*str) {
        int c;
        str += utf8_tounicode(str, &c);
        p += utf8_fromunicode(p, utf8_lower(c));
    }
    *p = 0;
    return Jim_NewStringObjNoAlloc(interp, buf, len);
}

static Jim_Obj *JimStringToUpper(Jim_Interp *interp, Jim_Obj *strObjPtr)
{
    char *buf, *p;
    int len;
    const char *str;

    if (strObjPtr->typePtr != &stringObjType) {
        SetStringFromAny(interp, strObjPtr);
    }

    str = Jim_GetString(strObjPtr, &len);

    buf = p = Jim_Alloc(len + 1);
    while (*str) {
        int c;
        str += utf8_tounicode(str, &c);
        p += utf8_fromunicode(p, utf8_upper(c));
    }
    *p = 0;
    return Jim_NewStringObjNoAlloc(interp, buf, len);
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

    if (strObjPtr->typePtr != &stringObjType) {
        SetStringFromAny(interp, strObjPtr);
    }
    Jim_GetString(strObjPtr, &len);
    nontrim = JimFindTrimRight(strObjPtr->bytes, len, trimchars, trimcharslen);

    if (nontrim == NULL) {
        /* All trim, so return a zero-length string */
        return Jim_NewEmptyStringObj(interp);
    }
    if (nontrim == strObjPtr->bytes + len) {
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

    if (objPtr != strObjPtr) {
        /* Note that we don't want this object to be leaked */
        Jim_IncrRefCount(objPtr);
        Jim_DecrRefCount(interp, objPtr);
    }

    return strObjPtr;
}


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
        Jim_SetResultInt(interp, !strict);
        return JIM_OK;
    }

    switch (strclass) {
        case STR_IS_INTEGER:
            {
                jim_wide w;
                Jim_SetResultInt(interp, JimGetWideNoErr(interp, strObjPtr, &w) == JIM_OK);
                return JIM_OK;
            }

        case STR_IS_DOUBLE:
            {
                double d;
                Jim_SetResultInt(interp, Jim_GetDouble(interp, strObjPtr, &d) == JIM_OK && errno != ERANGE);
                return JIM_OK;
            }

        case STR_IS_ALPHA: isclassfunc = isalpha; break;
        case STR_IS_ALNUM: isclassfunc = isalnum; break;
        case STR_IS_ASCII: isclassfunc = isascii; break;
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
            Jim_SetResultInt(interp, 0);
            return JIM_OK;
        }
    }
    Jim_SetResultInt(interp, 1);
    return JIM_OK;
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
int Jim_CompareStringImmediate(Jim_Interp *interp, Jim_Obj *objPtr, const char *str)
{
    if (objPtr->typePtr == &comparedStringObjType && objPtr->internalRep.ptr == str)
        return 1;
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
    Jim_ReleaseSharedString(interp, objPtr->internalRep.sourceValue.fileName);
}

void DupSourceInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr)
{
    dupPtr->internalRep.sourceValue.fileName =
        Jim_GetSharedString(interp, srcPtr->internalRep.sourceValue.fileName);
    dupPtr->internalRep.sourceValue.lineNumber = dupPtr->internalRep.sourceValue.lineNumber;
    dupPtr->typePtr = &sourceObjType;
}

static void JimSetSourceInfo(Jim_Interp *interp, Jim_Obj *objPtr,
    const char *fileName, int lineNumber)
{
    if (fileName) {
        JimPanic((Jim_IsShared(objPtr), interp, "JimSetSourceInfo called with shared object"));
        JimPanic((objPtr->typePtr != NULL, interp, "JimSetSourceInfo called with typePtr != NULL"));
        objPtr->internalRep.sourceValue.fileName = Jim_GetSharedString(interp, fileName);
        objPtr->internalRep.sourceValue.lineNumber = lineNumber;
        objPtr->typePtr = &sourceObjType;
    }
}

/* -----------------------------------------------------------------------------
 * Script Object
 * ---------------------------------------------------------------------------*/

static const Jim_ObjType scriptLineObjType = {
    "scriptline",
    NULL,
    NULL,
    NULL,
    0,
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

#define JIM_CMDSTRUCT_EXPAND -1

static void FreeScriptInternalRep(Jim_Interp *interp, Jim_Obj *objPtr);
static void DupScriptInternalRep(Jim_Interp *interp, Jim_Obj *srcPtr, Jim_Obj *dupPtr);
static int SetScriptFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr, struct JimParseResult *result);

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
typedef struct ScriptToken
{
    int type;
    Jim_Obj *objPtr;
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
 * will produce a ScriptObj with the following Tokens:
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
 * and "subst" objects. In the second case, the there are no LIN and WRD
 * tokens. Instead SEP and EOL tokens are added as-is.
 * In addition, the field 'substFlags' is used to represent the flags used to turn
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
typedef struct ScriptObj
{
    int len;                    /* Length as number of tokens. */
    ScriptToken *token;         /* Tokens array. */
    int substFlags;             /* flags used for the compilation of "subst" objects */
    int inUse;                  /* Used to share a ScriptObj. Currently
                                   only used by Jim_EvalObj() as protection against
                                   shimmering of the currently evaluated object. */
    const char *fileName;
    int line;                   /* Line number of the first line */
} ScriptObj;

void FreeScriptInternalRep(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int i;
    struct ScriptObj *script = (void *)objPtr->internalRep.ptr;

    script->inUse--;
    if (script->inUse != 0)
        return;
    for (i = 0; i < script->len; i++) {
        Jim_DecrRefCount(interp, script->token[i].objPtr);
    }
    Jim_Free(script->token);
    if (script->fileName) {
        Jim_ReleaseSharedString(interp, script->fileName);
    }
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

/* Counts the number of adjoining non-separator.
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
        /* Convert the backlash escapes . */
        int len = t->len;
        char *str = Jim_Alloc(len + 1);
        len = JimEscape(str, t->token, len);
        objPtr = Jim_NewStringObjNoAlloc(interp, str, len);
    }
    else {
        /* REVIST: Strictly, JIM_TT_STR should replace <backslash><newline><whitespace>
         *         with a single space. This is currently not done.
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
    linenr = script->line = tokenlist->list[0].line;

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
            /* More than 1, or {expand}, so insert a WORD token */
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

            /* Every object is initially a string, but the
             * internal type may be specialized during execution of the
             * script. */
            JimSetSourceInfo(interp, token->objPtr, script->fileName, t->line);
            token++;
        }
    }

    if (lineargs == 0) {
        token--;
    }

    script->len = token - script->token;

    assert(script->len < count);

#ifdef DEBUG_SHOW_SCRIPT
    printf("==== Script (%s) ====\n", script->fileName);
    for (i = 0; i < script->len; i++) {
        const ScriptToken *t = &script->token[i];
        printf("[%2d] %s %s\n", i, jim_tt_name(t->type), Jim_String(t->objPtr));
    }
#endif

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
 * of the script. */
static int SetScriptFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr, struct JimParseResult *result)
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
    if (result && parser.missing != ' ') {
        ScriptTokenListFree(&tokenlist);
        result->missing = parser.missing;
        result->line = parser.missingline;
        return JIM_ERR;
    }

    /* Add a final EOF token */
    ScriptAddToken(&tokenlist, scriptText + scriptTextLen, 0, JIM_TT_EOF, 0);

    /* Create the "real" script tokens from the initial token list */
    script = Jim_Alloc(sizeof(*script));
    memset(script, 0, sizeof(*script));
    script->inUse = 1;
    script->line = line;
    if (objPtr->typePtr == &sourceObjType) {
        script->fileName = Jim_GetSharedString(interp, objPtr->internalRep.sourceValue.fileName);
    }

    ScriptObjAddTokens(interp, script, &tokenlist);

    /* No longer need the token list */
    ScriptTokenListFree(&tokenlist);

    if (!script->fileName) {
        script->fileName = Jim_GetSharedString(interp, "");
    }

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
        SetScriptFromAny(interp, objPtr, NULL);
    }
    return (ScriptObj *) Jim_GetIntRepPtr(objPtr);
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
            if (cmdPtr->u.proc.staticVars) {
                Jim_FreeHashTable(cmdPtr->u.proc.staticVars);
                Jim_Free(cmdPtr->u.proc.staticVars);
            }
            if (cmdPtr->u.proc.prevCmd) {
                /* Delete any pushed command too */
                JimDecrCmdRefCount(interp, cmdPtr->u.proc.prevCmd);
            }
        }
        else {
            /* native (C) */
            if (cmdPtr->u.native.delProc) {
                cmdPtr->u.native.delProc(interp, cmdPtr->u.native.privData);
            }
        }
        Jim_Free(cmdPtr);
    }
}

/* Commands HashTable Type.
 *
 * Keys are dynamic allocated strings, Values are Jim_Cmd structures. */
static void JimCommandsHT_ValDestructor(void *interp, void *val)
{
    JimDecrCmdRefCount(interp, val);
}

static const Jim_HashTableType JimCommandsHashTableType = {
    JimStringCopyHTHashFunction,        /* hash function */
    JimStringCopyHTKeyDup,      /* key dup */
    NULL,                       /* val dup */
    JimStringCopyHTKeyCompare,  /* key compare */
    JimStringCopyHTKeyDestructor,       /* key destructor */
    JimCommandsHT_ValDestructor        /* val destructor */
};

/* ------------------------- Commands related functions --------------------- */

int Jim_CreateCommand(Jim_Interp *interp, const char *cmdName,
    Jim_CmdProc cmdProc, void *privData, Jim_DelCmdProc delProc)
{
    Jim_Cmd *cmdPtr;

    if (Jim_DeleteHashEntry(&interp->commands, cmdName) != JIM_ERR) {
        /* Command existed so incr proc epoch */
        Jim_InterpIncrProcEpoch(interp);
    }

    cmdPtr = Jim_Alloc(sizeof(*cmdPtr));

    /* Store the new details for this proc */
    memset(cmdPtr, 0, sizeof(*cmdPtr));
    cmdPtr->inUse = 1;
    cmdPtr->u.native.delProc = delProc;
    cmdPtr->u.native.cmdProc = cmdProc;
    cmdPtr->u.native.privData = privData;

    Jim_AddHashEntry(&interp->commands, cmdName, cmdPtr);

    /* There is no need to increment the 'proc epoch' because
     * creation of a new procedure can never affect existing
     * cached commands. We don't do negative caching. */
    return JIM_OK;
}

static int JimCreateProcedure(Jim_Interp *interp, Jim_Obj *cmdName,
    Jim_Obj *argListObjPtr, Jim_Obj *staticsListObjPtr, Jim_Obj *bodyObjPtr)
{
    Jim_Cmd *cmdPtr;
    Jim_HashEntry *he;
    int argListLen;
    int i;

    if (JimValidName(interp, "procedure", cmdName) != JIM_OK) {
        return JIM_ERR;
    }

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
    Jim_IncrRefCount(argListObjPtr);
    Jim_IncrRefCount(bodyObjPtr);

    /* Create the statics hash table. */
    if (staticsListObjPtr) {
        int len, i;

        len = Jim_ListLength(interp, staticsListObjPtr);
        if (len != 0) {
            cmdPtr->u.proc.staticVars = Jim_Alloc(sizeof(Jim_HashTable));
            Jim_InitHashTable(cmdPtr->u.proc.staticVars, &JimVariablesHashTableType, interp);
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
                        initObjPtr = Jim_GetVariable(interp, nameObjPtr, JIM_NONE);
                        if (initObjPtr == NULL) {
                            Jim_SetResultFormatted(interp,
                                "variable for initialization of static \"%#s\" not found in the local context",
                                nameObjPtr);
                            goto err;
                        }
                    }
                    else {
                        Jim_ListIndex(interp, objPtr, 1, &initObjPtr, JIM_NONE);
                    }
                    if (JimValidName(interp, "static variable", nameObjPtr) != JIM_OK) {
                        goto err;
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
                        goto err;
                    }
                }
                else {
                    Jim_SetResultFormatted(interp, "too many fields in static specifier \"%#s\"",
                        objPtr);
                    goto err;
                }
            }
        }
    }

    /* Parse the args out into arglist, validating as we go */
    /* Examine the argument list for default parameters and 'args' */
    for (i = 0; i < argListLen; i++) {
        Jim_Obj *argPtr;
        Jim_Obj *nameObjPtr;
        Jim_Obj *defaultObjPtr;
        int len;
        int n = 1;

        /* Examine a parameter */
        Jim_ListIndex(interp, argListObjPtr, i, &argPtr, JIM_NONE);
        len = Jim_ListLength(interp, argPtr);
        if (len == 0) {
            Jim_SetResultString(interp, "procedure has argument with no name", -1);
            goto err;
        }
        if (len > 2) {
            Jim_SetResultString(interp, "procedure has argument with too many fields", -1);
            goto err;
        }

        if (len == 2) {
            /* Optional parameter */
            Jim_ListIndex(interp, argPtr, 0, &nameObjPtr, JIM_NONE);
            Jim_ListIndex(interp, argPtr, 1, &defaultObjPtr, JIM_NONE);
        }
        else {
            /* Required parameter */
            nameObjPtr = argPtr;
            defaultObjPtr = NULL;
        }


        if (Jim_CompareStringImmediate(interp, nameObjPtr, "args")) {
            if (cmdPtr->u.proc.argsPos >= 0) {
                Jim_SetResultString(interp, "procedure has 'args' specified more than once", -1);
                goto err;
            }
            cmdPtr->u.proc.argsPos = i;
        }
        else {
            if (len == 2) {
                cmdPtr->u.proc.optArity += n;
            }
            else {
                cmdPtr->u.proc.reqArity += n;
            }
        }

        cmdPtr->u.proc.arglist[i].nameObjPtr = nameObjPtr;
        cmdPtr->u.proc.arglist[i].defaultObjPtr = defaultObjPtr;
    }

    /* Add the new command */

    /* It may already exist, so we try to delete the old one.
     * Note that reference count means that it won't be deleted yet if
     * it exists in the call stack.
     *
     * BUT, if 'local' is in force, instead of deleting the existing
     * proc, we stash a reference to the old proc here.
     */
    he = Jim_FindHashEntry(&interp->commands, Jim_String(cmdName));
    if (he) {
        /* There was an old procedure with the same name, this requires
         * a 'proc epoch' update. */

        /* If a procedure with the same name didn't existed there is no need
         * to increment the 'proc epoch' because creation of a new procedure
         * can never affect existing cached commands. We don't do
         * negative caching. */
        Jim_InterpIncrProcEpoch(interp);
    }

    if (he && interp->local) {
        /* Just push this proc over the top of the previous one */
        cmdPtr->u.proc.prevCmd = he->u.val;
        he->u.val = cmdPtr;
    }
    else {
        if (he) {
            /* Replace the existing proc */
            Jim_DeleteHashEntry(&interp->commands, Jim_String(cmdName));
        }

        Jim_AddHashEntry(&interp->commands, Jim_String(cmdName), cmdPtr);
    }

    /* Unlike Tcl, set the name of the proc as the result */
    Jim_SetResult(interp, cmdName);
    return JIM_OK;

  err:
    if (cmdPtr->u.proc.staticVars) {
        Jim_FreeHashTable(cmdPtr->u.proc.staticVars);
    }
    Jim_Free(cmdPtr->u.proc.staticVars);
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

int Jim_RenameCommand(Jim_Interp *interp, const char *oldName, const char *newName)
{
    Jim_HashEntry *he;

    /* Does it exist? */
    he = Jim_FindHashEntry(&interp->commands, oldName);
    if (he == NULL) {
        Jim_SetResultFormatted(interp, "can't %s \"%s\": command doesn't exist",
            newName[0] ? "rename" : "delete", oldName);
        return JIM_ERR;
    }

    if (newName[0] == '\0')     /* Delete! */
        return Jim_DeleteCommand(interp, oldName);

    /* rename */
    if (Jim_FindHashEntry(&interp->commands, newName)) {
        Jim_SetResultFormatted(interp, "can't rename to \"%s\": command already exists", newName);
        return JIM_ERR;
    }

    /* Add the new name first */
    JimIncrCmdRefCount(he->u.val);
    Jim_AddHashEntry(&interp->commands, newName, he->u.val);

    /* Now remove the old name */
    Jim_DeleteHashEntry(&interp->commands, oldName);

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
    cmdName = Jim_String(objPtr);
    /* Lookup this name into the commands hash table */
    he = Jim_FindHashEntry(&interp->commands, cmdName);
    if (he == NULL)
        return JIM_ERR;

    /* Free the old internal repr and set the new one. */
    Jim_FreeIntRep(interp, objPtr);
    objPtr->typePtr = &commandObjType;
    objPtr->internalRep.cmdValue.procEpoch = interp->procEpoch;
    objPtr->internalRep.cmdValue.cmdPtr = (void *)he->u.val;
    return JIM_OK;
}

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

    if ((objPtr->typePtr != &commandObjType ||
            objPtr->internalRep.cmdValue.procEpoch != interp->procEpoch) &&
        SetCommandFromAny(interp, objPtr) == JIM_ERR) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp, "invalid command name \"%#s\"", objPtr);
        }
        return NULL;
    }
    cmd = objPtr->internalRep.cmdValue.cmdPtr;
    while (cmd->isproc && cmd->u.proc.upcall) {
        cmd = cmd->u.proc.prevCmd;
    }
    return cmd;
}

/* -----------------------------------------------------------------------------
 * Variables
 * ---------------------------------------------------------------------------*/

/* Variables HashTable Type.
 *
 * Keys are dynamic allocated strings, Values are Jim_Var structures. */
static void JimVariablesHTValDestructor(void *interp, void *val)
{
    Jim_Var *varPtr = (void *)val;

    Jim_DecrRefCount(interp, varPtr->objPtr);
    Jim_Free(val);
}

static const Jim_HashTableType JimVariablesHashTableType = {
    JimStringCopyHTHashFunction,        /* hash function */
    JimStringCopyHTKeyDup,      /* key dup */
    NULL,                       /* val dup */
    JimStringCopyHTKeyCompare,  /* key compare */
    JimStringCopyHTKeyDestructor,       /* key destructor */
    JimVariablesHTValDestructor /* val destructor */
};

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

/* Return true if the string "str" looks like syntax sugar for [dict]. I.e.
 * is in the form "varname(key)". */
static int JimNameIsDictSugar(const char *str, int len)
{
    if (len && str[len - 1] == ')' && strchr(str, '(') != NULL)
        return 1;
    return 0;
}

/**
 * Check that the name does not contain embedded nulls.
 *
 * Variable and procedure names are maniplated as null terminated strings, so
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
 * JIM_ERR if it does not exists, JIM_DICT_SUGAR if it's not
 * a variable name, but syntax glue for [dict] i.e. the last
 * character is ')' */
static int SetVariableFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    Jim_HashEntry *he;
    const char *varName;
    int len;
    Jim_CallFrame *framePtr = interp->framePtr;

    /* Check if the object is already an uptodate variable */
    if (objPtr->typePtr == &variableObjType &&
        objPtr->internalRep.varValue.callFrameId == framePtr->id) {
        return JIM_OK;          /* nothing to do */
    }

    if (objPtr->typePtr == &dictSubstObjType) {
        return JIM_DICT_SUGAR;
    }

    if (JimValidName(interp, "variable", objPtr) != JIM_OK) {
        return JIM_ERR;
    }

    /* Get the string representation */
    varName = Jim_GetString(objPtr, &len);

    /* Make sure it's not syntax glue to get/set dict. */
    if (JimNameIsDictSugar(varName, len)) {
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
    objPtr->internalRep.varValue.varPtr = (void *)he->u.val;
    return JIM_OK;
}

/* -------------------- Variables related functions ------------------------- */
static int JimDictSugarSet(Jim_Interp *interp, Jim_Obj *ObjPtr, Jim_Obj *valObjPtr);
static Jim_Obj *JimDictSugarGet(Jim_Interp *interp, Jim_Obj *ObjPtr, int flags);

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

        if (JimValidName(interp, "variable", nameObjPtr) != JIM_OK) {
            return JIM_ERR;
        }

        /* New variable to create */
        name = Jim_String(nameObjPtr);

        var = Jim_Alloc(sizeof(*var));
        var->objPtr = valObjPtr;
        Jim_IncrRefCount(valObjPtr);
        var->linkFramePtr = NULL;
        /* Insert the new variable */
        if (name[0] == ':' && name[1] == ':') {
            /* Into the top level frame */
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
    }
    else {
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
    int len;

    varName = Jim_GetString(nameObjPtr, &len);

    if (varName[0] == ':' && varName[1] == ':') {
        /* Linking a global var does nothing */
        return JIM_OK;
    }

    if (JimNameIsDictSugar(varName, len)) {
        Jim_SetResultString(interp, "Dict key syntax invalid as link source", -1);
        return JIM_ERR;
    }

    /* Check for an existing variable or link */
    if (SetVariableFromAny(interp, nameObjPtr) == JIM_OK) {
        Jim_Var *varPtr = nameObjPtr->internalRep.varValue.varPtr;

        if (varPtr->linkFramePtr == NULL) {
            Jim_SetResultFormatted(interp, "variable \"%#s\" already exists", nameObjPtr);
            return JIM_ERR;
        }

        /* It exists, but is a link, so delete the link */
        varPtr->linkFramePtr = NULL;
    }

    /* Check for cycles. */
    if (interp->framePtr == targetCallFrame) {
        Jim_Obj *objPtr = targetNameObjPtr;
        Jim_Var *varPtr;

        /* Cycles are only possible with 'uplevel 0' */
        while (1) {
            if (Jim_StringEqObj(objPtr, nameObjPtr)) {
                Jim_SetResultString(interp, "can't upvar from variable to itself", -1);
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
        }
        else {
            Jim_CallFrame *framePtr = interp->framePtr;

            name = Jim_String(nameObjPtr);
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
    int len, keyLen;
    Jim_Obj *varObjPtr, *keyObjPtr;

    str = Jim_GetString(objPtr, &len);

    p = strchr(str, '(');
    JimPanic((p == NULL, interp, "JimDictSugarParseVarKey() called for non-dict-sugar (%s)", str));

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
        &objPtr->internalRep.dictSubstValue.indexObjPtr, 1, valObjPtr);

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
    else if ((flags & JIM_UNSHARED) && Jim_IsShared(dictObjPtr)) {
        dictObjPtr = Jim_DuplicateObj(interp, dictObjPtr);
        if (Jim_SetVariable(interp, varObjPtr, dictObjPtr) != JIM_OK) {
            /* This can probably never happen */
            JimPanic((1, interp, "SetVariable failed for JIM_UNSHARED"));
        }
        /* We know that the key exists. Get the result in the now-unshared dictionary */
        Jim_DictKey(interp, dictObjPtr, keyObjPtr, &resObjPtr, JIM_NONE);
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

static Jim_CallFrame *JimCreateCallFrame(Jim_Interp *interp, Jim_CallFrame *parent)
{
    Jim_CallFrame *cf;

    if (interp->freeFramesList) {
        cf = interp->freeFramesList;
        interp->freeFramesList = cf->nextFramePtr;
    }
    else {
        cf = Jim_Alloc(sizeof(*cf));
        cf->vars.table = NULL;
    }

    cf->id = interp->callFrameEpoch++;
    cf->parentCallFrame = parent;
    cf->level = parent ? parent->level + 1 : 0;
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

#define JIM_FCF_NONE 0          /* no flags */
#define JIM_FCF_NOHT 1          /* don't free the hash table */
static void JimFreeCallFrame(Jim_Interp *interp, Jim_CallFrame *cf, int flags)
{
    if (cf->procArgsObjPtr)
        Jim_DecrRefCount(interp, cf->procArgsObjPtr);
    if (cf->procBodyObjPtr)
        Jim_DecrRefCount(interp, cf->procBodyObjPtr);
    if (!(flags & JIM_FCF_NOHT))
        Jim_FreeHashTable(&cf->vars);
    else {
        int i;
        Jim_HashEntry **table = cf->vars.table, *he;

        for (i = 0; i < JIM_HT_INITIAL_SIZE; i++) {
            he = table[i];
            while (he != NULL) {
                Jim_HashEntry *nextEntry = he->next;
                Jim_Var *varPtr = (void *)he->u.val;

                Jim_DecrRefCount(interp, varPtr->objPtr);
                Jim_Free(he->u.val);
                Jim_Free((void *)he->key);      /* ATTENTION: const cast */
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
    const jim_wide *widePtr = key;
    unsigned int intValue = (unsigned int)*widePtr;

    return Jim_IntHashFunction(intValue);
}

static const void *JimReferencesHTKeyDup(void *privdata, const void *key)
{
    void *copy = Jim_Alloc(sizeof(jim_wide));

    JIM_NOTUSED(privdata);

    memcpy(copy, key, sizeof(jim_wide));
    return copy;
}

static int JimReferencesHTKeyCompare(void *privdata, const void *key1, const void *key2)
{
    JIM_NOTUSED(privdata);

    return memcmp(key1, key2, sizeof(jim_wide)) == 0;
}

static void JimReferencesHTKeyDestructor(void *privdata, const void *key)
{
    JIM_NOTUSED(privdata);

    Jim_Free((void *)key);
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
    char buf[JIM_REFERENCE_SPACE + 1];
    Jim_Reference *refPtr;

    refPtr = objPtr->internalRep.refValue.refPtr;
    len = JimFormatReference(buf, refPtr, objPtr->internalRep.refValue.id);
    objPtr->bytes = Jim_Alloc(len + 1);
    memcpy(objPtr->bytes, buf, len + 1);
    objPtr->length = len;
}

/* returns true if 'c' is a valid reference tag character.
 * i.e. inside the range [_a-zA-Z0-9] */
static int isrefchar(int c)
{
    return (c == '_' || isalnum(c));
}

static int SetReferenceFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
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
    /* Try to convert the ID into a jim_wide */
    if (Jim_StringToWide(refId, &wideValue, 10) != JIM_OK)
        goto badformat;
    /* Check if the reference really exists! */
    he = Jim_FindHashEntry(&interp->references, &wideValue);
    if (he == NULL) {
        Jim_SetResultFormatted(interp, "invalid reference id \"%#s\"", objPtr);
        return JIM_ERR;
    }
    refPtr = he->u.val;
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
Jim_Obj *Jim_NewReference(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *tagPtr, Jim_Obj *cmdNamePtr)
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
    while (objPtr) {
        if (objPtr->typePtr == NULL || objPtr->typePtr->flags & JIM_TYPE_REFERENCES) {
            const char *str, *p;
            int len;

            /* If the object is of type reference, to get the
             * Id is simple... */
            if (objPtr->typePtr == &referenceObjType) {
                Jim_AddHashEntry(&marks, &objPtr->internalRep.refValue.id, NULL);
#ifdef JIM_DEBUG_GC
                printf("MARK (reference): %d refcount: %d" JIM_NL,
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
                jim_wide id;
                char buf[21];

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
                memcpy(buf, p + 21, 20);
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
            refPtr = he->u.val;
            if (refPtr->finalizerCmdNamePtr) {
                char *refstr = Jim_Alloc(JIM_REFERENCE_SPACE + 1);
                Jim_Obj *objv[3], *oldResult;

                JimFormatReference(refstr, refPtr, *refId);

                objv[0] = refPtr->finalizerCmdNamePtr;
                objv[1] = Jim_NewStringObjNoAlloc(interp, refstr, 32);
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
            }
            else {
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


    if (elapsedId > JIM_COLLECT_ID_PERIOD || elapsedTime > JIM_COLLECT_TIME_PERIOD) {
        Jim_Collect(interp);
    }
}
#endif

static int JimIsBigEndian(void)
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

    i->errorFileName = Jim_StrDup("");
    i->maxNestingDepth = JIM_MAX_NESTING_DEPTH;
    i->lastCollectTime = time(NULL);

    /* Note that we can create objects only after the
     * interpreter liveList and freeList pointers are
     * initialized to NULL. */
    Jim_InitHashTable(&i->commands, &JimCommandsHashTableType, i);
#ifdef JIM_REFERENCES
    Jim_InitHashTable(&i->references, &JimReferencesHashTableType, i);
#endif
    Jim_InitHashTable(&i->sharedStrings, &JimSharedStringsHashTableType, NULL);
    Jim_InitHashTable(&i->assocData, &JimAssocDataHashTableType, i);
    Jim_InitHashTable(&i->packages, &JimStringKeyValCopyHashTableType, NULL);
    i->framePtr = i->topFramePtr = JimCreateCallFrame(i, NULL);
    i->emptyObj = Jim_NewEmptyStringObj(i);
    i->trueObj = Jim_NewIntObj(i, 1);
    i->falseObj = Jim_NewIntObj(i, 0);
    i->result = i->emptyObj;
    i->stackTrace = Jim_NewListObj(i, NULL, 0);
    i->unknown = Jim_NewStringObj(i, "unknown", -1);
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
    Jim_SetVariableStrWithStr(i, JIM_LIBPATH, TCL_LIBRARY);
    Jim_SetVariableStrWithStr(i, JIM_INTERACTIVE, "0");

    Jim_SetVariableStrWithStr(i, "tcl_platform(os)", TCL_PLATFORM_OS);
    Jim_SetVariableStrWithStr(i, "tcl_platform(platform)", TCL_PLATFORM_PLATFORM);
    Jim_SetVariableStrWithStr(i, "tcl_platform(pathSeparator)", TCL_PLATFORM_PATH_SEPARATOR);
    Jim_SetVariableStrWithStr(i, "tcl_platform(byteOrder)", JimIsBigEndian() ? "bigEndian" : "littleEndian");
    Jim_SetVariableStrWithStr(i, "tcl_platform(threaded)", "0");
    Jim_SetVariableStr(i, "tcl_platform(pointerSize)", Jim_NewIntObj(i, sizeof(void *)));
    Jim_SetVariableStr(i, "tcl_platform(wordSize)", Jim_NewIntObj(i, sizeof(jim_wide)));

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
    Jim_Free((void *)i->errorFileName);
    Jim_DecrRefCount(i, i->currentScriptObj);
    Jim_FreeHashTable(&i->commands);
#ifdef JIM_REFERENCES
    Jim_FreeHashTable(&i->references);
#endif
    Jim_FreeHashTable(&i->packages);
    Jim_Free(i->prngState);
    Jim_FreeHashTable(&i->assocData);
    JimDeleteLocalProcs(i);

    /* Free the call frames list */
    while (cf) {
        prevcf = cf->parentCallFrame;
        JimFreeCallFrame(i, cf, JIM_FCF_NONE);
        cf = prevcf;
    }
    /* Check that the live object list is empty, otherwise
     * there is a memory leak. */
    if (i->liveList != NULL) {
        objPtr = i->liveList;

        printf(JIM_NL "-------------------------------------" JIM_NL);
        printf("Objects still in the free list:" JIM_NL);
        while (objPtr) {
            const char *type = objPtr->typePtr ? objPtr->typePtr->name : "string";

            printf("%p (%d) %-10s: '%.20s'" JIM_NL,
                (void *)objPtr, objPtr->refCount, type, objPtr->bytes ? objPtr->bytes : "(null)");
            if (objPtr->typePtr == &sourceObjType) {
                printf("FILE %s LINE %d" JIM_NL,
                    objPtr->internalRep.sourceValue.fileName,
                    objPtr->internalRep.sourceValue.lineNumber);
            }
            objPtr = objPtr->nextObjPtr;
        }
        printf("-------------------------------------" JIM_NL JIM_NL);
        JimPanic((1, i, "Live list non empty freeing the interpreter! Leak?"));
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
    while (cf) {
        nextcf = cf->nextFramePtr;
        if (cf->vars.table != NULL)
            Jim_Free(cf->vars.table);
        Jim_Free(cf);
        cf = nextcf;
    }
#ifdef jim_ext_load
    Jim_FreeLoadHandles(i);
#endif

    /* Free the sharedString hash table. Make sure to free it
     * after every other Jim_Object was freed. */
    Jim_FreeHashTable(&i->sharedStrings);
    /* Free the interpreter structure. */
    Jim_Free(i);
}

/* Returns the call frame relative to the level represented by
 * levelObjPtr. If levelObjPtr == NULL, the * level is assumed to be '1'.
 *
 * This function accepts the 'level' argument in the form
 * of the commands [uplevel] and [upvar].
 *
 * For a function accepting a relative integer as level suitable
 * for implementation of [info level ?level?] check the
 * JimGetCallFrameByInteger() function.
 *
 * Returns NULL on error.
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

            level = strtol(str + 1, &endptr, 0);
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
        for (framePtr = interp->framePtr; framePtr; framePtr = framePtr->parentCallFrame) {
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
        for (framePtr = interp->framePtr; framePtr; framePtr = framePtr->parentCallFrame) {
            if (framePtr->level == level) {
                return framePtr;
            }
        }
    }

    Jim_SetResultFormatted(interp, "bad level \"%#s\"", levelObjPtr);
    return NULL;
}

static void JimSetErrorFileName(Jim_Interp *interp, const char *filename)
{
    Jim_Free((void *)interp->errorFileName);
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
    if (strcmp(procname, "unknown") == 0) {
        procname = "";
    }
    if (!*procname && !*filename) {
        /* No useful info here */
        return;
    }

    if (Jim_IsShared(interp->stackTrace)) {
        Jim_DecrRefCount(interp, interp->stackTrace);
        interp->stackTrace = Jim_DuplicateObj(interp, interp->stackTrace);
        Jim_IncrRefCount(interp->stackTrace);
    }

    /* If we have no procname but the previous element did, merge with that frame */
    if (!*procname && *filename) {
        /* Just a filename. Check the previous entry */
        int len = Jim_ListLength(interp, interp->stackTrace);

        if (len >= 3) {
            Jim_Obj *procnameObj;
            Jim_Obj *filenameObj;

            if (Jim_ListIndex(interp, interp->stackTrace, len - 3, &procnameObj, JIM_NONE) == JIM_OK
                && Jim_ListIndex(interp, interp->stackTrace, len - 2, &filenameObj,
                    JIM_NONE) == JIM_OK) {

                const char *prev_procname = Jim_String(procnameObj);
                const char *prev_filename = Jim_String(filenameObj);

                if (*prev_procname && !*prev_filename) {
                    ListSetIndex(interp, interp->stackTrace, len - 2, Jim_NewStringObj(interp,
                            filename, -1), 0);
                    ListSetIndex(interp, interp->stackTrace, len - 1, Jim_NewIntObj(interp, linenr),
                        0);
                    return;
                }
            }
        }
    }

    Jim_ListAppendElement(interp, interp->stackTrace, Jim_NewStringObj(interp, procname, -1));
    Jim_ListAppendElement(interp, interp->stackTrace, Jim_NewStringObj(interp, filename, -1));
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
        AssocDataValue *assocEntryPtr = (AssocDataValue *) entryPtr->u.val;

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

        Jim_AddHashEntry(&interp->sharedStrings, strCopy, NULL);
	he = Jim_FindHashEntry(&interp->sharedStrings, strCopy);
	he->u.intval = 1;
        return strCopy;
    }
    else {
        he->u.intval++;
        return he->key;
    }
}

void Jim_ReleaseSharedString(Jim_Interp *interp, const char *str)
{
    Jim_HashEntry *he = Jim_FindHashEntry(&interp->sharedStrings, str);

    JimPanic((he == NULL, interp, "Jim_ReleaseSharedString called with " "unknown shared string '%s'", str));

    if (--he->u.intval == 0) {
        Jim_DeleteHashEntry(&interp->sharedStrings, str);
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
    char buf[JIM_INTEGER_SPACE + 1];

    len = Jim_WideToString(buf, JimWideValue(objPtr));
    objPtr->bytes = Jim_Alloc(len + 1);
    memcpy(objPtr->bytes, buf, len + 1);
    objPtr->length = len;
}

int SetIntFromAny(Jim_Interp *interp, Jim_Obj *objPtr, int flags)
{
    jim_wide wideValue;
    const char *str;

    if (objPtr->typePtr == &coercedDoubleObjType) {
        /* Simple switcheroo */
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

void UpdateStringOfDouble(struct Jim_Obj *objPtr)
{
    int len;
    char buf[JIM_DOUBLE_SPACE + 1];

    len = Jim_DoubleToString(buf, objPtr->internalRep.doubleValue);
    objPtr->bytes = Jim_Alloc(len + 1);
    memcpy(objPtr->bytes, buf, len + 1);
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
static int ListElementQuotingType(const char *s, int len)
{
    int i, level, blevel, trySimple = 1;

    /* Try with the SIMPLE case */
    if (len == 0)
        return JIM_ELESTR_BRACE;
    if (s[0] == '#')
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

/* Returns the malloc-ed representation of a string
 * using backslash to quote special chars. */
static char *BackslashQuoteString(const char *s, int len, int *qlenPtr)
{
    char *q = Jim_Alloc(len * 2 + 1), *p;

    p = q;
    while (*s) {
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
    *qlenPtr = p - q;
    return q;
}

static void UpdateStringOfList(struct Jim_Obj *objPtr)
{
    int i, bufLen, realLength;
    const char *strRep;
    char *p;
    int *quotingType;
    Jim_Obj **ele = objPtr->internalRep.listValue.ele;

    /* (Over) Estimate the space needed. */
    quotingType = Jim_Alloc(sizeof(int) * objPtr->internalRep.listValue.len + 1);
    bufLen = 0;
    for (i = 0; i < objPtr->internalRep.listValue.len; i++) {
        int len;

        strRep = Jim_GetString(ele[i], &len);
        quotingType[i] = ListElementQuotingType(strRep, len);
        switch (quotingType[i]) {
            case JIM_ELESTR_SIMPLE:
                bufLen += len;
                break;
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
    for (i = 0; i < objPtr->internalRep.listValue.len; i++) {
        int len, qlen;
        char *q;

        strRep = Jim_GetString(ele[i], &len);

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
                q = BackslashQuoteString(strRep, len, &qlen);
                memcpy(p, q, qlen);
                Jim_Free(q);
                p += qlen;
                realLength += qlen;
                break;
        }
        /* Add a separating space */
        if (i + 1 != objPtr->internalRep.listValue.len) {
            *p++ = ' ';
            realLength++;
        }
    }
    *p = '\0';                  /* nul term. */
    objPtr->length = realLength;
    Jim_Free(quotingType);
}

int SetListFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    struct JimParserCtx parser;
    const char *str;
    int strLen;
    const char *filename = NULL;
    int linenr = 1;

    /* Try to preserve information about filename / line number */
    if (objPtr->typePtr == &sourceObjType) {
        filename = Jim_GetSharedString(interp, objPtr->internalRep.sourceValue.fileName);
        linenr = objPtr->internalRep.sourceValue.lineNumber;
    }

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
    JimParserInit(&parser, str, strLen, linenr);
    while (!parser.eof) {
        Jim_Obj *elementPtr;

        JimParseList(&parser);
        if (parser.tt != JIM_TT_STR && parser.tt != JIM_TT_ESC)
            continue;
        elementPtr = JimParserGetTokenObj(interp, &parser);
        JimSetSourceInfo(interp, elementPtr, filename, parser.tline);
        ListAppendElement(objPtr, elementPtr);
    }
    if (filename) {
        Jim_ReleaseSharedString(interp, filename);
    }
    return JIM_OK;
}

Jim_Obj *Jim_NewListObj(Jim_Interp *interp, Jim_Obj *const *elements, int len)
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
struct lsort_info {
    jmp_buf jmpbuf;
    Jim_Obj *command;
    Jim_Interp *interp;
    enum {
        JIM_LSORT_ASCII,
        JIM_LSORT_NOCASE,
        JIM_LSORT_INTEGER,
        JIM_LSORT_COMMAND
    } type;
    int order;
    int index;
    int indexed;
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

/* Sort a list *in place*. MUST be called with non-shared objects. */
static int ListSortElements(Jim_Interp *interp, Jim_Obj *listObjPtr, struct lsort_info *info)
{
    struct lsort_info *prev_info;

    typedef int (qsort_comparator) (const void *, const void *);
    int (*fn) (Jim_Obj **, Jim_Obj **);
    Jim_Obj **vector;
    int len;
    int rc;

    JimPanic((Jim_IsShared(listObjPtr), interp, "Jim_ListSortElements called with shared object"));
    if (!Jim_IsList(listObjPtr))
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
        case JIM_LSORT_COMMAND:
            fn = ListSortCommand;
            break;
        default:
            fn = NULL;          /* avoid warning */
            JimPanic((1, interp, "ListSort called with invalid sort type"));
    }

    if (info->indexed) {
        /* Need to interpose a "list index" function */
        info->subfn = fn;
        fn = ListSortIndexHelper;
    }

    if ((rc = setjmp(info->jmpbuf)) == 0) {
        qsort(vector, len, sizeof(Jim_Obj *), (qsort_comparator *) fn);
    }
    Jim_InvalidateStringRep(listObjPtr);
    sort_info = prev_info;

    return rc;
}

/* This is the low-level function to insert elements into a list.
 * The higher-level Jim_ListInsertElements() performs shared object
 * check and invalidate the string repr. This version is used
 * in the internals of the List Object and is not exported.
 *
 * NOTE: this function can be called only against objects
 * with internal type of List. */
static void ListInsertElements(Jim_Obj *listPtr, int idx, int elemc, Jim_Obj *const *elemVec)
{
    int currentLen = listPtr->internalRep.listValue.len;
    int requiredLen = currentLen + elemc;
    int i;
    Jim_Obj **point;

    if (requiredLen > listPtr->internalRep.listValue.maxLen) {
        int maxLen = requiredLen * 2;

        listPtr->internalRep.listValue.ele =
            Jim_Realloc(listPtr->internalRep.listValue.ele, sizeof(Jim_Obj *) * maxLen);
        listPtr->internalRep.listValue.maxLen = maxLen;
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
    ListInsertElements(listPtr, listPtr->internalRep.listValue.len, 1, &objPtr);
}


/* Appends every element of appendListPtr into listPtr.
 * Both have to be of the list type.
 * Convenience call to ListInsertElements()
 */
static void ListAppendList(Jim_Obj *listPtr, Jim_Obj *appendListPtr)
{
    ListInsertElements(listPtr, listPtr->internalRep.listValue.len,
        appendListPtr->internalRep.listValue.len, appendListPtr->internalRep.listValue.ele);
}

void Jim_ListAppendElement(Jim_Interp *interp, Jim_Obj *listPtr, Jim_Obj *objPtr)
{
    JimPanic((Jim_IsShared(listPtr), interp, "Jim_ListAppendElement called with shared object"));
    if (!Jim_IsList(listPtr))
        SetListFromAny(interp, listPtr);
    Jim_InvalidateStringRep(listPtr);
    ListAppendElement(listPtr, objPtr);
}

void Jim_ListAppendList(Jim_Interp *interp, Jim_Obj *listPtr, Jim_Obj *appendListPtr)
{
    JimPanic((Jim_IsShared(listPtr), interp, "Jim_ListAppendList called with shared object"));
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

void Jim_ListInsertElements(Jim_Interp *interp, Jim_Obj *listPtr, int idx,
    int objc, Jim_Obj *const *objVec)
{
    JimPanic((Jim_IsShared(listPtr), interp, "Jim_ListInsertElement called with shared object"));
    if (!Jim_IsList(listPtr))
        SetListFromAny(interp, listPtr);
    if (idx >= 0 && idx > listPtr->internalRep.listValue.len)
        idx = listPtr->internalRep.listValue.len;
    else if (idx < 0)
        idx = 0;
    Jim_InvalidateStringRep(listPtr);
    ListInsertElements(listPtr, idx, objc, objVec);
}

int Jim_ListIndex(Jim_Interp *interp, Jim_Obj *listPtr, int idx, Jim_Obj **objPtrPtr, int flags)
{
    if (!Jim_IsList(listPtr))
        SetListFromAny(interp, listPtr);
    if ((idx >= 0 && idx >= listPtr->internalRep.listValue.len) ||
        (idx < 0 && (-idx - 1) >= listPtr->internalRep.listValue.len)) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultString(interp, "list index out of range", -1);
        }
        *objPtrPtr = NULL;
        return JIM_ERR;
    }
    if (idx < 0)
        idx = listPtr->internalRep.listValue.len + idx;
    *objPtrPtr = listPtr->internalRep.listValue.ele[idx];
    return JIM_OK;
}

static int ListSetIndex(Jim_Interp *interp, Jim_Obj *listPtr, int idx,
    Jim_Obj *newObjPtr, int flags)
{
    if (!Jim_IsList(listPtr))
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

/* Modify the list stored into the variable named 'varNamePtr'
 * setting the element specified by the 'indexc' indexes objects in 'indexv',
 * with the new element 'newObjptr'. */
int Jim_SetListIndex(Jim_Interp *interp, Jim_Obj *varNamePtr,
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
    }
    else {
        /* Else... we have to glue strings together */
        int len = 0, objLen;
        char *bytes, *p;

        /* Compute the length */
        for (i = 0; i < objc; i++) {
            Jim_GetString(objv[i], &objLen);
            len += objLen;
        }
        if (objc)
            len += objc - 1;
        /* Create the string rep, and a string object holding it. */
        p = bytes = Jim_Alloc(len + 1);
        for (i = 0; i < objc; i++) {
            const char *s = Jim_GetString(objv[i], &objLen);

            /* Remove leading space */
            while (objLen && (*s == ' ' || *s == '\t' || *s == '\n')) {
                s++;
                objLen--;
                len--;
            }
            /* And trailing space */
            while (objLen && (s[objLen - 1] == ' ' ||
                    s[objLen - 1] == '\n' || s[objLen - 1] == '\t')) {
                /* Handle trailing backslash-space case */
                if (objLen > 1 && s[objLen - 2] == '\\') {
                    break;
                }
                objLen--;
                len--;
            }
            memcpy(p, s, objLen);
            p += objLen;
            if (objLen && i + 1 != objc) {
                *p++ = ' ';
            }
            else if (i + 1 != objc) {
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
    JimRelToAbsRange(len, first, last, &first, &last, &rangeLen);
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
    const char *str;
    Jim_Obj *objPtr = (Jim_Obj *)key;
    int len, h;

    str = Jim_GetString(objPtr, &len);
    h = Jim_GenHashFunction((unsigned char *)str, len);
    return h;
}

static int JimObjectHTKeyCompare(void *privdata, const void *key1, const void *key2)
{
    JIM_NOTUSED(privdata);

    return Jim_StringEqObj((Jim_Obj *)key1, (Jim_Obj *)key2);
}

static void JimObjectHTKeyValDestructor(void *interp, void *val)
{
    Jim_Obj *objPtr = val;

    Jim_DecrRefCount(interp, objPtr);
}

static const Jim_HashTableType JimDictHashTableType = {
    JimObjectHTHashFunction,    /* hash function */
    NULL,                       /* key dup */
    NULL,                       /* val dup */
    JimObjectHTKeyCompare,      /* key compare */
    (void (*)(void *, const void *))    /* ATTENTION: const cast */
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
        Jim_Obj *valObjPtr = he->u.val;

        Jim_IncrRefCount((Jim_Obj *)keyObjPtr); /* ATTENTION: const cast */
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
    objc = ht->used * 2;
    objv = Jim_Alloc(objc * sizeof(Jim_Obj *));
    htiter = Jim_GetHashTableIterator(ht);
    i = 0;
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        objv[i++] = (Jim_Obj *)he->key; /* ATTENTION: const cast */
        objv[i++] = he->u.val;
    }
    Jim_FreeHashTableIterator(htiter);
    /* (Over) Estimate the space needed. */
    quotingType = Jim_Alloc(sizeof(int) * objc);
    bufLen = 0;
    for (i = 0; i < objc; i++) {
        int len;

        strRep = Jim_GetString(objv[i], &len);
        quotingType[i] = ListElementQuotingType(strRep, len);
        switch (quotingType[i]) {
            case JIM_ELESTR_SIMPLE:
                bufLen += len;
                break;
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
        char *q;

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
                q = BackslashQuoteString(strRep, len, &qlen);
                memcpy(p, q, qlen);
                Jim_Free(q);
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
    Jim_Free(quotingType);
    Jim_Free(objv);
}

static int SetDictFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    int listlen;

    /* Get the string representation. Do this first so we don't
     * change order in case of fast conversion to dict.
     */
    Jim_String(objPtr);

    /* For simplicity, convert a non-list object to a list and then to a dict */
    listlen = Jim_ListLength(interp, objPtr);
    if (listlen % 2) {
        Jim_SetResultString(interp,
            "invalid dictionary value: must be a list with an even number of elements", -1);
        return JIM_ERR;
    }
    else {
        /* Now it is easy to convert to a dict from a list, and it can't fail */
        Jim_HashTable *ht;
        int i;

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
                Jim_DecrRefCount(interp, (Jim_Obj *)he->u.val);
                he->u.val = valObjPtr;
            }
        }

        Jim_FreeIntRep(interp, objPtr);
        objPtr->typePtr = &dictObjType;
        objPtr->internalRep.ptr = ht;

        return JIM_OK;
    }
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

    if (valueObjPtr == NULL) {  /* unset */
        return Jim_DeleteHashEntry(ht, keyObjPtr);
    }
    Jim_IncrRefCount(keyObjPtr);
    Jim_IncrRefCount(valueObjPtr);
    if (Jim_AddHashEntry(ht, keyObjPtr, valueObjPtr) != JIM_OK) {
        Jim_HashEntry *he = Jim_FindHashEntry(ht, keyObjPtr);

        Jim_DecrRefCount(interp, keyObjPtr);
        /* ATTENTION: const cast */
        Jim_DecrRefCount(interp, (Jim_Obj *)he->u.val);
        he->u.val = valueObjPtr;
    }
    return JIM_OK;
}

/* Add an element, higher-level interface for DictAddElement().
 * If valueObjPtr == NULL, the key is removed if it exists. */
int Jim_DictAddElement(Jim_Interp *interp, Jim_Obj *objPtr,
    Jim_Obj *keyObjPtr, Jim_Obj *valueObjPtr)
{
    int retcode;

    JimPanic((Jim_IsShared(objPtr), interp, "Jim_DictAddElement called with shared object"));
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

    JimPanic((len % 2, interp, "Jim_NewDictObj() 'len' argument must be even"));

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
    *objPtrPtr = he->u.val;
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
    objv = Jim_Alloc((ht->used * 2) * sizeof(Jim_Obj *));
    htiter = Jim_GetHashTableIterator(ht);
    i = 0;
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        objv[i++] = (Jim_Obj *)he->key; /* ATTENTION: const cast */
        objv[i++] = he->u.val;
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

    varObjPtr = objPtr =
        Jim_GetVariable(interp, varNamePtr, newObjPtr == NULL ? JIM_ERRMSG : JIM_NONE);
    if (objPtr == NULL) {
        if (newObjPtr == NULL)  /* Cannot remove a key from non existing var */
            return JIM_ERR;
        varObjPtr = objPtr = Jim_NewDictObj(interp, NULL, 0);
        if (Jim_SetVariable(interp, varNamePtr, objPtr) != JIM_OK) {
            Jim_FreeNewObj(interp, varObjPtr);
            return JIM_ERR;
        }
    }
    if ((shared = Jim_IsShared(objPtr)))
        varObjPtr = objPtr = Jim_DuplicateObj(interp, objPtr);
    for (i = 0; i < keyc - 1; i++) {
        dictObjPtr = objPtr;

        /* Check if it's a valid dictionary */
        if (dictObjPtr->typePtr != &dictObjType) {
            if (SetDictFromAny(interp, dictObjPtr) != JIM_OK)
                goto err;
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
            if (newObjPtr == NULL)
                goto err;
            /* Otherwise set an empty dictionary
             * as key's value. */
            objPtr = Jim_NewDictObj(interp, NULL, 0);
            DictAddElement(interp, dictObjPtr, keyv[i], objPtr);
        }
    }
    if (Jim_DictAddElement(interp, objPtr, keyv[keyc - 1], newObjPtr) != JIM_OK) {
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
    char buf[JIM_INTEGER_SPACE + 1];

    if (objPtr->internalRep.indexValue >= 0)
        len = sprintf(buf, "%d", objPtr->internalRep.indexValue);
    else if (objPtr->internalRep.indexValue == -1)
        len = sprintf(buf, "end");
    else {
        len = sprintf(buf, "end%d", objPtr->internalRep.indexValue + 1);
    }
    objPtr->bytes = Jim_Alloc(len + 1);
    memcpy(objPtr->bytes, buf, len + 1);
    objPtr->length = len;
}

int SetIndexFromAny(Jim_Interp *interp, Jim_Obj *objPtr)
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
        idx = strtol(str, &endptr, 10);

        if (endptr == str) {
            goto badindex;
        }
        str = endptr;
    }

    /* Now str may include or +<num> or -<num> */
    if (*str == '+' || *str == '-') {
        int sign = (*str == '+' ? 1 : -1);

        idx += sign * strtol(++str, &endptr, 10);
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
    objPtr->internalRep.indexValue = idx;
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

        if (!(val < LONG_MIN) && !(val > LONG_MAX)) {
            *indexPtr = (val < 0) ? -INT_MAX : (long)val;;
            return JIM_OK;
        }
    }
    if (objPtr->typePtr != &indexObjType && SetIndexFromAny(interp, objPtr) == JIM_ERR)
        return JIM_ERR;
    *indexPtr = objPtr->internalRep.indexValue;
    return JIM_OK;
}

/* -----------------------------------------------------------------------------
 * Return Code Object.
 * ---------------------------------------------------------------------------*/

/* NOTE: These must be kept in the same order as JIM_OK, JIM_ERR, ... */
static const char * const jimReturnCodes[] = {
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
        returnCode = (int)wideValue;
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
    if (objPtr->typePtr != &returnCodeObjType && SetReturnCodeFromAny(interp, objPtr) == JIM_ERR)
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
enum
{
    /* Continues on from the JIM_TT_ space */
    /* Operations */
    JIM_EXPROP_MUL = JIM_TT_EXPR_OP,    /* 15 */
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
    JIM_EXPROP_BITAND,          /* 30 */
    JIM_EXPROP_BITXOR,
    JIM_EXPROP_BITOR,

    /* Note must keep these together */
    JIM_EXPROP_LOGICAND,        /* 33 */
    JIM_EXPROP_LOGICAND_LEFT,
    JIM_EXPROP_LOGICAND_RIGHT,

    /* and these */
    JIM_EXPROP_LOGICOR,         /* 36 */
    JIM_EXPROP_LOGICOR_LEFT,
    JIM_EXPROP_LOGICOR_RIGHT,

    /* and these */
    /* Ternary operators */
    JIM_EXPROP_TERNARY,         /* 39 */
    JIM_EXPROP_TERNARY_LEFT,
    JIM_EXPROP_TERNARY_RIGHT,

    /* and these */
    JIM_EXPROP_COLON,           /* 42 */
    JIM_EXPROP_COLON_LEFT,
    JIM_EXPROP_COLON_RIGHT,

    JIM_EXPROP_POW,             /* 45 */

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
    JIM_EXPROP_FUNC_RAND,
    JIM_EXPROP_FUNC_SRAND,

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
    int precedence;
    int arity;
    int (*funcop) (Jim_Interp *interp, struct JimExprState * e);
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
            case JIM_EXPROP_FUNC_INT:
                wC = wA;
                break;
            case JIM_EXPROP_FUNC_ROUND:
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
            case JIM_EXPROP_UNARYPLUS:
                wC = wA;
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
                wC = dA;
                intresult = 1;
                break;
            case JIM_EXPROP_FUNC_ROUND:
                wC = dA < 0 ? (dA - 0.5) : (dA + 0.5);
                intresult = 1;
                break;
            case JIM_EXPROP_FUNC_DOUBLE:
                dC = dA;
                break;
            case JIM_EXPROP_FUNC_ABS:
                dC = dA >= 0 ? dA : -dA;
                break;
            case JIM_EXPROP_UNARYMINUS:
                dC = -dA;
                break;
            case JIM_EXPROP_UNARYPLUS:
                dC = dA;
                break;
            case JIM_EXPROP_NOT:
                wC = !dA;
                intresult = 1;
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
    JimPanic((e->opcode != JIM_EXPROP_FUNC_RAND));

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
    int intresult = 0;
    int rc = JIM_OK;
    double dA, dB, dC = 0;
    jim_wide wA, wB, wC = 0;

    Jim_Obj *B = ExprPop(e);
    Jim_Obj *A = ExprPop(e);

    if ((A->typePtr != &doubleObjType || A->bytes) &&
        (B->typePtr != &doubleObjType || B->bytes) &&
        JimGetWideNoErr(interp, A, &wA) == JIM_OK && JimGetWideNoErr(interp, B, &wB) == JIM_OK) {

        /* Both are ints */

        intresult = 1;

        switch (e->opcode) {
            case JIM_EXPROP_POW:
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
        switch (e->opcode) {
            case JIM_EXPROP_POW:
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

        /* REVISIT: Could optimise the eq/ne case by checking lengths */
        int i = Jim_StringCompareObj(interp, A, B, 0);

        intresult = 1;

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
        Jim_Obj *objPtr;

        Jim_ListIndex(interp, listObjPtr, i, &objPtr, JIM_NONE);

        if (Jim_StringEqObj(objPtr, valObj)) {
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
        case JIM_EXPROP_STRNE: {
            int Alen, Blen;
            const char *sA = Jim_GetString(A, &Alen);
            const char *sB = Jim_GetString(B, &Blen);

            if (e->opcode == JIM_EXPROP_STREQ) {
                wC = (Alen == Blen && memcmp(sA, sB, Alen) == 0);
            }
            else {
                wC = (Alen != Blen || memcmp(sA, sB, Alen) != 0);
            }
            break;
        }
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

/* name - precedence - arity - opcode */
static const struct Jim_ExprOperator Jim_ExprOperators[] = {
    [JIM_EXPROP_FUNC_INT] = {"int", 400, 1, JimExprOpNumUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_DOUBLE] = {"double", 400, 1, JimExprOpNumUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_ABS] = {"abs", 400, 1, JimExprOpNumUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_ROUND] = {"round", 400, 1, JimExprOpNumUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_RAND] = {"rand", 400, 0, JimExprOpNone, LAZY_NONE},
    [JIM_EXPROP_FUNC_SRAND] = {"srand", 400, 1, JimExprOpIntUnary, LAZY_NONE},

#ifdef JIM_MATH_FUNCTIONS
    [JIM_EXPROP_FUNC_SIN] = {"sin", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_COS] = {"cos", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_TAN] = {"tan", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_ASIN] = {"asin", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_ACOS] = {"acos", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_ATAN] = {"atan", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_SINH] = {"sinh", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_COSH] = {"cosh", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_TANH] = {"tanh", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_CEIL] = {"ceil", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_FLOOR] = {"floor", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_EXP] = {"exp", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_LOG] = {"log", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_LOG10] = {"log10", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
    [JIM_EXPROP_FUNC_SQRT] = {"sqrt", 400, 1, JimExprOpDoubleUnary, LAZY_NONE},
#endif

    [JIM_EXPROP_NOT] = {"!", 300, 1, JimExprOpNumUnary, LAZY_NONE},
    [JIM_EXPROP_BITNOT] = {"~", 300, 1, JimExprOpIntUnary, LAZY_NONE},
    [JIM_EXPROP_UNARYMINUS] = {NULL, 300, 1, JimExprOpNumUnary, LAZY_NONE},
    [JIM_EXPROP_UNARYPLUS] = {NULL, 300, 1, JimExprOpNumUnary, LAZY_NONE},

    [JIM_EXPROP_POW] = {"**", 250, 2, JimExprOpBin, LAZY_NONE},

    [JIM_EXPROP_MUL] = {"*", 200, 2, JimExprOpBin, LAZY_NONE},
    [JIM_EXPROP_DIV] = {"/", 200, 2, JimExprOpBin, LAZY_NONE},
    [JIM_EXPROP_MOD] = {"%", 200, 2, JimExprOpIntBin, LAZY_NONE},

    [JIM_EXPROP_SUB] = {"-", 100, 2, JimExprOpBin, LAZY_NONE},
    [JIM_EXPROP_ADD] = {"+", 100, 2, JimExprOpBin, LAZY_NONE},

    [JIM_EXPROP_ROTL] = {"<<<", 90, 2, JimExprOpIntBin, LAZY_NONE},
    [JIM_EXPROP_ROTR] = {">>>", 90, 2, JimExprOpIntBin, LAZY_NONE},
    [JIM_EXPROP_LSHIFT] = {"<<", 90, 2, JimExprOpIntBin, LAZY_NONE},
    [JIM_EXPROP_RSHIFT] = {">>", 90, 2, JimExprOpIntBin, LAZY_NONE},

    [JIM_EXPROP_LT] = {"<", 80, 2, JimExprOpBin, LAZY_NONE},
    [JIM_EXPROP_GT] = {">", 80, 2, JimExprOpBin, LAZY_NONE},
    [JIM_EXPROP_LTE] = {"<=", 80, 2, JimExprOpBin, LAZY_NONE},
    [JIM_EXPROP_GTE] = {">=", 80, 2, JimExprOpBin, LAZY_NONE},

    [JIM_EXPROP_NUMEQ] = {"==", 70, 2, JimExprOpBin, LAZY_NONE},
    [JIM_EXPROP_NUMNE] = {"!=", 70, 2, JimExprOpBin, LAZY_NONE},

    [JIM_EXPROP_STREQ] = {"eq", 60, 2, JimExprOpStrBin, LAZY_NONE},
    [JIM_EXPROP_STRNE] = {"ne", 60, 2, JimExprOpStrBin, LAZY_NONE},

    [JIM_EXPROP_STRIN] = {"in", 55, 2, JimExprOpStrBin, LAZY_NONE},
    [JIM_EXPROP_STRNI] = {"ni", 55, 2, JimExprOpStrBin, LAZY_NONE},

    [JIM_EXPROP_BITAND] = {"&", 50, 2, JimExprOpIntBin, LAZY_NONE},
    [JIM_EXPROP_BITXOR] = {"^", 49, 2, JimExprOpIntBin, LAZY_NONE},
    [JIM_EXPROP_BITOR] = {"|", 48, 2, JimExprOpIntBin, LAZY_NONE},

    [JIM_EXPROP_LOGICAND] = {"&&", 10, 2, NULL, LAZY_OP},
    [JIM_EXPROP_LOGICOR] = {"||", 9, 2, NULL, LAZY_OP},

    [JIM_EXPROP_TERNARY] = {"?", 5, 2, JimExprOpNull, LAZY_OP},
    [JIM_EXPROP_COLON] = {":", 5, 2, JimExprOpNull, LAZY_OP},

    /* private operators */
    [JIM_EXPROP_TERNARY_LEFT] = {NULL, 5, 2, JimExprOpTernaryLeft, LAZY_LEFT},
    [JIM_EXPROP_TERNARY_RIGHT] = {NULL, 5, 2, JimExprOpNull, LAZY_RIGHT},
    [JIM_EXPROP_COLON_LEFT] = {NULL, 5, 2, JimExprOpColonLeft, LAZY_LEFT},
    [JIM_EXPROP_COLON_RIGHT] = {NULL, 5, 2, JimExprOpNull, LAZY_RIGHT},
    [JIM_EXPROP_LOGICAND_LEFT] = {NULL, 10, 2, JimExprOpAndLeft, LAZY_LEFT},
    [JIM_EXPROP_LOGICAND_RIGHT] = {NULL, 10, 2, JimExprOpAndOrRight, LAZY_RIGHT},
    [JIM_EXPROP_LOGICOR_LEFT] = {NULL, 9, 2, JimExprOpOrLeft, LAZY_LEFT},
    [JIM_EXPROP_LOGICOR_RIGHT] = {NULL, 9, 2, JimExprOpAndOrRight, LAZY_RIGHT},
};

#define JIM_EXPR_OPERATORS_NUM \
    (sizeof(Jim_ExprOperators)/sizeof(struct Jim_ExprOperator))

static int JimParseExpression(struct JimParserCtx *pc)
{
    /* Discard spaces and quoted newline */
    while (isspace(UCHAR(*pc->p)) || (*(pc->p) == '\\' && *(pc->p + 1) == '\n')) {
        pc->p++;
        pc->len--;
    }

    if (pc->len == 0) {
        pc->tstart = pc->tend = pc->p;
        pc->tline = pc->linenr;
        pc->tt = JIM_TT_EOL;
        pc->eof = 1;
        return JIM_OK;
    }
    switch (*(pc->p)) {
        case '(':
            pc->tstart = pc->tend = pc->p;
            pc->tline = pc->linenr;
            pc->tt = JIM_TT_SUBEXPR_START;
            pc->p++;
            pc->len--;
            break;
        case ')':
            pc->tstart = pc->tend = pc->p;
            pc->tline = pc->linenr;
            pc->tt = JIM_TT_SUBEXPR_END;
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
    int allowdot = 1;
    int allowhex = 0;

    /* Assume an integer for now */
    pc->tt = JIM_TT_EXPR_INT;
    pc->tstart = pc->p;
    pc->tline = pc->linenr;
    while (isdigit(UCHAR(*pc->p))
        || (allowhex && isxdigit(UCHAR(*pc->p)))
        || (allowdot && *pc->p == '.')
        || (pc->p - pc->tstart == 1 && *pc->tstart == '0' && (*pc->p == 'x' || *pc->p == 'X'))
        ) {
        if ((*pc->p == 'x') || (*pc->p == 'X')) {
            allowhex = 1;
            allowdot = 0;
        }
        if (*pc->p == '.') {
            allowdot = 0;
            pc->tt = JIM_TT_EXPR_DOUBLE;
        }
        pc->p++;
        pc->len--;
        if (!allowhex && (*pc->p == 'e' || *pc->p == 'E') && (pc->p[1] == '-' || pc->p[1] == '+'
                || isdigit(UCHAR(pc->p[1])))) {
            pc->p += 2;
            pc->len -= 2;
            pc->tt = JIM_TT_EXPR_DOUBLE;
        }
    }
    pc->tend = pc->p - 1;
    return JIM_OK;
}

static int JimParseExprIrrational(struct JimParserCtx *pc)
{
    const char *Tokens[] = { "NaN", "nan", "NAN", "Inf", "inf", "INF", NULL };
    const char **token;

    for (token = Tokens; *token != NULL; token++) {
        int len = strlen(*token);

        if (strncmp(*token, pc->p, len) == 0) {
            pc->tstart = pc->p;
            pc->tend = pc->p + len - 1;
            pc->p += len;
            pc->len -= len;
            pc->tline = pc->linenr;
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

        while (len && isspace(UCHAR(*p))) {
            len--;
            p++;
        }
        if (*p != '(') {
            return JIM_ERR;
        }
    }
    pc->tstart = pc->p;
    pc->tend = pc->p + bestLen - 1;
    pc->p += bestLen;
    pc->len -= bestLen;
    pc->tline = pc->linenr;

    pc->tt = bestIdx;
    return JIM_OK;
}

static const struct Jim_ExprOperator *JimExprOperatorInfoByOpcode(int opcode)
{
    return &Jim_ExprOperators[opcode];
}

const char *jim_tt_name(int type)
{
    static const char * const tt_names[JIM_TT_EXPR_OP] =
        { "NIL", "STR", "ESC", "VAR", "ARY", "CMD", "SEP", "EOL", "EOF", "LIN", "WRD", "(((", ")))", "INT",
            "DBL", "$()" };
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
    int len;                    /* Length as number of tokens. */
    ScriptToken *token;         /* Tokens array. */
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

    /* Now we rely on the fact the the left and right version have opcodes
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
        if (JimExprOperatorInfoByOpcode(expr->token[i].type)->lazy == LAZY_LEFT) {
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

static ExprByteCode *ExprCreateByteCode(Jim_Interp *interp, const ParseTokenList *tokenlist)
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

        if (JimExprOperatorInfoByOpcode(t->type)->lazy == LAZY_OP) {
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
int SetExprFromAny(Jim_Interp *interp, struct Jim_Obj *objPtr)
{
    int exprTextLen;
    const char *exprText;
    struct JimParserCtx parser;
    struct ExprByteCode *expr;
    ParseTokenList tokenlist;
    int rc = JIM_ERR;
    int line = 1;

    /* Try to get information about filename / line number */
    if (objPtr->typePtr == &sourceObjType) {
        line = objPtr->internalRep.sourceValue.lineNumber;
    }

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
        printf("==== Expr Tokens ====\n");
        for (i = 0; i < tokenlist.count; i++) {
            printf("[%2d]@%d %s '%.*s'\n", i, tokenlist.list[i].line, jim_tt_name(tokenlist.list[i].type),
                tokenlist.list[i].len, tokenlist.list[i].token);
        }
    }
#endif

    /* Now create the expression bytecode from the tokenlist */
    expr = ExprCreateByteCode(interp, &tokenlist);

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
                    if (objPtr && JimIsWide(objPtr)
                        && Jim_GetWide(interp, objPtr, &wideValue) == JIM_OK) {
                        *exprResultPtrPtr = wideValue ? interp->falseObj : interp->trueObj;
                        Jim_IncrRefCount(*exprResultPtrPtr);
                        return JIM_OK;
                    }
                }
                break;

            case 3:
                if (expr->token[0].type == JIM_TT_VAR && (expr->token[1].type == JIM_TT_EXPR_INT
                        || expr->token[1].type == JIM_TT_VAR)) {
                    switch (expr->token[2].type) {
                        case JIM_EXPROP_LT:
                        case JIM_EXPROP_LTE:
                        case JIM_EXPROP_GT:
                        case JIM_EXPROP_GTE:
                        case JIM_EXPROP_NUMEQ:
                        case JIM_EXPROP_NUMNE:{
                                /* optimise ok */
                                jim_wide wideValueA;
                                jim_wide wideValueB;

                                objPtr = Jim_GetVariable(interp, expr->token[0].objPtr, JIM_NONE);
                                if (objPtr && JimIsWide(objPtr)
                                    && Jim_GetWide(interp, objPtr, &wideValueA) == JIM_OK) {
                                    if (expr->token[1].type == JIM_TT_VAR) {
                                        objPtr =
                                            Jim_GetVariable(interp, expr->token[1].objPtr,
                                            JIM_NONE);
                                    }
                                    else {
                                        objPtr = expr->token[1].objPtr;
                                    }
                                    if (objPtr && JimIsWide(objPtr)
                                        && Jim_GetWide(interp, objPtr, &wideValueB) == JIM_OK) {
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
                                            default:   /*notreached */
                                                cmpRes = 0;
                                        }
                                        *exprResultPtrPtr =
                                            cmpRes ? interp->trueObj : interp->falseObj;
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
    char type;                  /* Type of conversion (e.g. c, d, f) */
    char modifier;              /* Modify type (e.g. l - long, h - short */
    size_t width;               /* Maximal width of input to be converted */
    int pos;                    /* -1 - no assign, 0 - natural pos, >0 - XPG3 pos */
    char *arg;                  /* Specification of a CHARSET conversion */
    char *prefix;               /* Prefix to be scanned literally before conversion */
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

void UpdateStringOfScanFmt(Jim_Obj *objPtr)
{
    char *bytes = ((ScanFmtStringObj *) objPtr->internalRep.ptr)->stringRep;

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
        /* XXX: Should be checking strLen, not str[pos] */
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
    JimPanic((fmtObjPtr->typePtr != &scanFmtStringObjType, interp, "Jim_ScanString() for non-scan format"));

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
    resultList = Jim_NewListObj(interp, 0, 0);
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
static int JimUnknown(Jim_Interp *interp, int argc, Jim_Obj *const *argv, const char *filename,
    int linenr)
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
    if (argc + 1 <= JIM_EVAL_SARGV_LEN)
        v = sv;
    else
        v = Jim_Alloc(sizeof(Jim_Obj *) * (argc + 1));
    /* Make a copy of the arguments vector, but shifted on
     * the right of one position. The command name of the
     * command will be instead the first argument of the
     * [unknown] call. */
    memcpy(v + 1, argv, sizeof(Jim_Obj *) * argc);
    v[0] = interp->unknown;
    /* Call it */
    interp->unknown_called++;
    retCode = JimEvalObjVector(interp, argc + 1, v, filename, linenr);
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
static int JimEvalObjVector(Jim_Interp *interp, int objc, Jim_Obj *const *objv,
    const char *filename, int linenr)
{
    int i, retcode;
    Jim_Cmd *cmdPtr;

    /* Incr refcount of arguments. */
    for (i = 0; i < objc; i++)
        Jim_IncrRefCount(objv[i]);
    /* Command lookup */
    cmdPtr = Jim_GetCommand(interp, objv[0], JIM_ERRMSG);
    if (cmdPtr == NULL) {
        retcode = JimUnknown(interp, objc, objv, filename, linenr);
    }
    else {
        /* Call it -- Make sure result is an empty object. */
        JimIncrCmdRefCount(cmdPtr);
        Jim_SetEmptyResult(interp);
        if (cmdPtr->isproc) {
            retcode = JimCallProcedure(interp, cmdPtr, filename, linenr, objc, objv);
        }
        else {
            interp->cmdPrivData = cmdPtr->u.native.privData;
            retcode = cmdPtr->u.native.cmdProc(interp, objc, objv);
        }
        JimDecrCmdRefCount(interp, cmdPtr);
    }
    /* Decr refcount of arguments and return the retcode */
    for (i = 0; i < objc; i++)
        Jim_DecrRefCount(interp, objv[i]);

    return retcode;
}

int Jim_EvalObjVector(Jim_Interp *interp, int objc, Jim_Obj *const *objv)
{
    return JimEvalObjVector(interp, objc, objv, NULL, 0);
}

/**
 * Invokes 'prefix' as a command with the objv array as arguments.
 */
int Jim_EvalObjPrefix(Jim_Interp *interp, const char *prefix, int objc, Jim_Obj *const *objv)
{
    int i;
    int ret;
    Jim_Obj **nargv = Jim_Alloc((objc + 1) * sizeof(*nargv));

    nargv[0] = Jim_NewStringObj(interp, prefix, -1);
    for (i = 0; i < objc; i++) {
        nargv[i + 1] = objv[i];
    }
    ret = Jim_EvalObjVector(interp, objc + 1, nargv);
    Jim_Free(nargv);
    return ret;
}

static void JimAddErrorToStack(Jim_Interp *interp, int retcode, const char *filename, int line)
{
    int rc = retcode;

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

        JimAppendStackTrace(interp, Jim_String(interp->errorProc), filename, line);

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

/* And delete any local procs */
static void JimDeleteLocalProcs(Jim_Interp *interp)
{
    if (interp->localProcs) {
        char *procname;

        while ((procname = Jim_StackPop(interp->localProcs)) != NULL) {
            /* If there is a pushed command, find it */
            Jim_Cmd *prevCmd = NULL;
            Jim_HashEntry *he = Jim_FindHashEntry(&interp->commands, procname);
            if (he) {
                Jim_Cmd *cmd = (Jim_Cmd *)he->u.val;
                if (cmd->isproc && cmd->u.proc.prevCmd) {
                    prevCmd = cmd->u.proc.prevCmd;
                    cmd->u.proc.prevCmd = NULL;
                }
            }

            /* Delete the local proc */
            Jim_DeleteCommand(interp, procname);

            if (prevCmd) {
                /* And restore the pushed command */
                Jim_AddHashEntry(&interp->commands, procname, prevCmd);
            }
            Jim_Free(procname);
        }
        Jim_FreeStack(interp->localProcs);
        Jim_Free(interp->localProcs);
        interp->localProcs = NULL;
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
            JimPanic((1, interp,
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
        objPtr->internalRep.twoPtrValue.ptr1 = (void *)token;
        objPtr->internalRep.twoPtrValue.ptr2 = intv[2];
        Jim_IncrRefCount(intv[2]);
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


/* If listPtr is a list, call JimEvalObjVector() with the given source info.
 * Otherwise eval with Jim_EvalObj()
 */
int Jim_EvalObjList(Jim_Interp *interp, Jim_Obj *listPtr, const char *filename, int linenr)
{
    if (!Jim_IsList(listPtr)) {
        return Jim_EvalObj(interp, listPtr);
    }
    else {
        int retcode = JIM_OK;

        if (listPtr->internalRep.listValue.len) {
            Jim_IncrRefCount(listPtr);
            retcode = JimEvalObjVector(interp,
                listPtr->internalRep.listValue.len,
                listPtr->internalRep.listValue.ele, filename, linenr);
            Jim_DecrRefCount(interp, listPtr);
        }
        return retcode;
    }
}

int Jim_EvalObj(Jim_Interp *interp, Jim_Obj *scriptObjPtr)
{
    int i;
    ScriptObj *script;
    ScriptToken *token;
    int retcode = JIM_OK;
    Jim_Obj *sargv[JIM_EVAL_SARGV_LEN], **argv = NULL;
    int linenr = 0;

    interp->errorFlag = 0;

    /* If the object is of type "list", we can call
     * a specialized version of Jim_EvalObj() */
    if (Jim_IsList(scriptObjPtr)) {
        return Jim_EvalObjList(interp, scriptObjPtr, NULL, 0);
    }

    Jim_IncrRefCount(scriptObjPtr);     /* Make sure it's shared. */
    script = Jim_GetScript(interp, scriptObjPtr);

    /* Reset the interpreter result. This is useful to
     * return the empty result in the case of empty program. */
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
    if (script->len == 3
        && script->token[1].objPtr->typePtr == &commandObjType
        && script->token[1].objPtr->internalRep.cmdValue.cmdPtr->isproc == 0
        && script->token[1].objPtr->internalRep.cmdValue.cmdPtr->u.native.cmdProc == Jim_IncrCoreCommand
        && script->token[2].objPtr->typePtr == &variableObjType) {

        Jim_Obj *objPtr = Jim_GetVariable(interp, script->token[2].objPtr, JIM_NONE);

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

    token = script->token;
    argv = sargv;

    /* Execute every command sequentially until the end of the script
     * or an error occurs.
     */
    for (i = 0; i < script->len && retcode == JIM_OK; ) {
        int argc;
        int j;
        Jim_Cmd *cmd;

        /* First token of the line is always JIM_TT_LINE */
        argc = token[i].objPtr->internalRep.scriptLineValue.argc;
        linenr = token[i].objPtr->internalRep.scriptLineValue.line;

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
                        JimPanic((1, interp, "default token type reached " "in Jim_EvalObj()."));
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
            /* Lookup the command to call */
            cmd = Jim_GetCommand(interp, argv[0], JIM_ERRMSG);
            if (cmd != NULL) {
                /* Call it -- Make sure result is an empty object. */
                JimIncrCmdRefCount(cmd);
                Jim_SetEmptyResult(interp);
                if (cmd->isproc) {
                    retcode =
                        JimCallProcedure(interp, cmd, script->fileName, linenr, argc, argv);
                } else {
                    interp->cmdPrivData = cmd->u.native.privData;
                    retcode = cmd->u.native.cmdProc(interp, argc, argv);
                }
                JimDecrCmdRefCount(interp, cmd);
            }
            else {
                /* Call [unknown] */
                retcode = JimUnknown(interp, argc, argv, script->fileName, linenr);
            }
            if (interp->signal_level && interp->sigmask) {
                /* Check for a signal after each command */
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
    JimAddErrorToStack(interp, retcode, script->fileName, linenr);

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

        interp->framePtr = interp->framePtr->parentCallFrame;
        objPtr = Jim_GetVariable(interp, argValObj, JIM_ERRMSG);
        interp->framePtr = savedCallFrame;
        if (!objPtr) {
            return JIM_ERR;
        }

        /* It exists, so perform the binding. */
        objPtr = Jim_NewStringObj(interp, varname + 1, -1);
        Jim_IncrRefCount(objPtr);
        retcode = Jim_SetVariableLink(interp, objPtr, argValObj, interp->framePtr->parentCallFrame);
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
                Jim_AppendString(interp, argmsg, "?argument ...?", -1);
            }
        }
        else {
            if (cmd->u.proc.arglist[i].defaultObjPtr) {
                Jim_AppendString(interp, argmsg, "?", 1);
                Jim_AppendObj(interp, argmsg, cmd->u.proc.arglist[i].nameObjPtr);
                Jim_AppendString(interp, argmsg, "?", 1);
            }
            else {
                Jim_AppendObj(interp, argmsg, cmd->u.proc.arglist[i].nameObjPtr);
            }
        }
    }
    Jim_SetResultFormatted(interp, "wrong # args: should be \"%#s%#s\"", procNameObj, argmsg);
    Jim_FreeNewObj(interp, argmsg);
}

/* Call a procedure implemented in Tcl.
 * It's possible to speed-up a lot this function, currently
 * the callframes are not cached, but allocated and
 * destroied every time. What is expecially costly is
 * to create/destroy the local vars hash table every time.
 *
 * This can be fixed just implementing callframes caching
 * in JimCreateCallFrame() and JimFreeCallFrame(). */
static int JimCallProcedure(Jim_Interp *interp, Jim_Cmd *cmd, const char *filename, int linenr, int argc,
    Jim_Obj *const *argv)
{
    Jim_CallFrame *callFramePtr;
    Jim_Stack *prevLocalProcs;
    int i, d, retcode, optargs;

    /* Check arity */
    if (argc - 1 < cmd->u.proc.reqArity ||
        (cmd->u.proc.argsPos < 0 && argc - 1 > cmd->u.proc.reqArity + cmd->u.proc.optArity)) {
        JimSetProcWrongArgs(interp, argv[0], cmd);
        return JIM_ERR;
    }

    /* Check if there are too nested calls */
    if (interp->framePtr->level == interp->maxNestingDepth) {
        Jim_SetResultString(interp, "Too many nested calls. Infinite recursion?", -1);
        return JIM_ERR;
    }

    /* Create a new callframe */
    callFramePtr = JimCreateCallFrame(interp, interp->framePtr);
    callFramePtr->argv = argv;
    callFramePtr->argc = argc;
    callFramePtr->procArgsObjPtr = cmd->u.proc.argListObjPtr;
    callFramePtr->procBodyObjPtr = cmd->u.proc.bodyObjPtr;
    callFramePtr->staticVars = cmd->u.proc.staticVars;
    callFramePtr->filename = filename;
    callFramePtr->line = linenr;
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

    /* Install a new stack for local procs */
    prevLocalProcs = interp->localProcs;
    interp->localProcs = NULL;

    /* Eval the body */
    retcode = Jim_EvalObj(interp, cmd->u.proc.bodyObjPtr);

    /* Delete any local procs */
    JimDeleteLocalProcs(interp);
    interp->localProcs = prevLocalProcs;

badargset:
    /* Destroy the callframe */
    interp->framePtr = interp->framePtr->parentCallFrame;
    if (callFramePtr->vars.size != JIM_HT_INITIAL_SIZE) {
        JimFreeCallFrame(interp, callFramePtr, JIM_FCF_NONE);
    }
    else {
        JimFreeCallFrame(interp, callFramePtr, JIM_FCF_NOHT);
    }
    /* Handle the JIM_EVAL return code */
    while (retcode == JIM_EVAL) {
        Jim_Obj *resultScriptObjPtr = Jim_GetResult(interp);

        Jim_IncrRefCount(resultScriptObjPtr);
        /* Should be a list! */
        retcode = Jim_EvalObjList(interp, resultScriptObjPtr, filename, linenr);
        Jim_DecrRefCount(interp, resultScriptObjPtr);
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
    return Jim_Eval_Named(interp, script, NULL, 0);
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
    struct JimParseResult result;

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
    JimSetSourceInfo(interp, scriptObjPtr, filename, 1);
    Jim_IncrRefCount(scriptObjPtr);

    /* Now check the script for unmatched braces, etc. */
    if (SetScriptFromAny(interp, scriptObjPtr, &result) == JIM_ERR) {
        const char *msg;
        char linebuf[20];

        switch (result.missing) {
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

        snprintf(linebuf, sizeof(linebuf), "%d", result.line);

        Jim_SetResultFormatted(interp, "%s in \"%s\" at line %s",
            msg, filename, linebuf);
        Jim_DecrRefCount(interp, scriptObjPtr);
        return JIM_ERR;
    }

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
    while (pc->len && *pc->p != '$' && *pc->p != '[') {
        if (*pc->p == '\\' && pc->len > 1) {
            pc->p++;
            pc->len--;
        }
        pc->p++;
        pc->len--;
    }
    pc->tend = pc->p - 1;
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
    switch (*pc->p) {
        case '[':
            retval = JimParseCmd(pc);
            if (flags & JIM_SUBST_NOCMD) {
                pc->tstart--;
                pc->tend++;
                pc->tt = (flags & JIM_SUBST_NOESC) ? JIM_TT_STR : JIM_TT_ESC;
            }
            return retval;
            break;
        case '$':
            if (JimParseVar(pc) == JIM_ERR) {
                pc->tstart = pc->tend = pc->p++;
                pc->len--;
                pc->tline = pc->linenr;
                pc->tt = JIM_TT_STR;
            }
            else {
                if (flags & JIM_SUBST_NOVAR) {
                    pc->tstart--;
                    if (flags & JIM_SUBST_NOESC)
                        pc->tt = JIM_TT_STR;
                    else
                        pc->tt = JIM_TT_ESC;
                    if (*pc->tstart == '{') {
                        pc->tstart--;
                        if (*(pc->tend + 1))
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
    script->fileName = NULL;
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
    int i;
    Jim_Obj *objPtr = Jim_NewEmptyStringObj(interp);

    Jim_AppendString(interp, objPtr, "wrong # args: should be \"", -1);
    for (i = 0; i < argc; i++) {
        Jim_AppendObj(interp, objPtr, argv[i]);
        if (!(i + 1 == argc && msg[0] == '\0'))
            Jim_AppendString(interp, objPtr, " ", 1);
    }
    Jim_AppendString(interp, objPtr, msg, -1);
    Jim_AppendString(interp, objPtr, "\"", 1);
    Jim_SetResult(interp, objPtr);
}

#define JimTrivialMatch(pattern)	(strpbrk((pattern), "*[?\\") == NULL)

/* type is: 0=commands, 1=procs, 2=channels */
static Jim_Obj *JimCommandsList(Jim_Interp *interp, Jim_Obj *patternObjPtr, int type)
{
    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;
    Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);

    /* Check for the non-pattern case. We can do this much more efficiently. */
    if (patternObjPtr && JimTrivialMatch(Jim_String(patternObjPtr))) {
        Jim_Cmd *cmdPtr = Jim_GetCommand(interp, patternObjPtr, JIM_NONE);
        if (cmdPtr) {
            if (type == 1 && !cmdPtr->isproc) {
                /* not a proc */
            }
            else if (type == 2 && !Jim_AioFilehandle(interp, patternObjPtr)) {
                /* not a channel */
            }
            else {
                Jim_ListAppendElement(interp, listObjPtr, patternObjPtr);
            }
        }
        return listObjPtr;
    }

    htiter = Jim_GetHashTableIterator(&interp->commands);
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        Jim_Cmd *cmdPtr = he->u.val;
        Jim_Obj *cmdNameObj;

        if (type == 1 && !cmdPtr->isproc) {
            /* not a proc */
            continue;
        }
        if (patternObjPtr && !JimStringMatch(interp, patternObjPtr, he->key, 0))
            continue;

        cmdNameObj = Jim_NewStringObj(interp, he->key, -1);

        /* Is it a channel? */
        if (type == 2 && !Jim_AioFilehandle(interp, cmdNameObj)) {
            Jim_FreeNewObj(interp, cmdNameObj);
            continue;
        }

        Jim_ListAppendElement(interp, listObjPtr, cmdNameObj);
    }
    Jim_FreeHashTableIterator(htiter);
    return listObjPtr;
}

/* Keep this in order */
#define JIM_VARLIST_GLOBALS 0
#define JIM_VARLIST_LOCALS 1
#define JIM_VARLIST_VARS 2

static Jim_Obj *JimVariablesList(Jim_Interp *interp, Jim_Obj *patternObjPtr, int mode)
{
    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;
    Jim_Obj *listObjPtr = Jim_NewListObj(interp, NULL, 0);

    if (mode == JIM_VARLIST_GLOBALS) {
        htiter = Jim_GetHashTableIterator(&interp->topFramePtr->vars);
    }
    else {
        /* For [info locals], if we are at top level an emtpy list
         * is returned. I don't agree, but we aim at compatibility (SS) */
        if (mode == JIM_VARLIST_LOCALS && interp->framePtr == interp->topFramePtr)
            return listObjPtr;
        htiter = Jim_GetHashTableIterator(&interp->framePtr->vars);
    }
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        Jim_Var *varPtr = (Jim_Var *)he->u.val;

        if (mode == JIM_VARLIST_LOCALS) {
            if (varPtr->linkFramePtr != NULL)
                continue;
        }
        if (patternObjPtr && !JimStringMatch(interp, patternObjPtr, he->key, 0))
            continue;
        Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, he->key, -1));
    }
    Jim_FreeHashTableIterator(htiter);
    return listObjPtr;
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
        Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp,
                targetCallFrame->filename ? targetCallFrame->filename : "", -1));
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
        unsigned jim_wide procEpoch;
        Jim_Obj *objPtr;
        int cmpOffset;

        /* Do it only if there aren't shared arguments */
        expr = JimGetExpression(interp, argv[2]);
        incrScript = Jim_GetScript(interp, argv[3]);

        /* Ensure proper lengths to start */
        if (incrScript->len != 3 || !expr || expr->len != 3) {
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
        procEpoch = interp->procEpoch;
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
                /* If there was a change in procedures/command continue
                 * with the usual [for] command implementation */
                if (procEpoch != interp->procEpoch) {
                    goto evalnext;
                }

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

/* foreach + lmap implementation. */
static int JimForeachMapHelper(Jim_Interp *interp, int argc, Jim_Obj *const *argv, int doMap)
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
    script = argv[argc - 1];    /* Last argument is a script */
    nbrOfLists = (argc - 1 - 1) / 2;    /* argc - 'foreach' - script */
    listsIdx = (int *)Jim_Alloc(nbrOfLists * sizeof(int));
    listsEnd = (int *)Jim_Alloc(nbrOfLists * 2 * sizeof(int));
    /* Initialize iterators and remember max nbr elements each list */
    memset(listsIdx, 0, nbrOfLists * sizeof(int));
    /* Remember lengths of all lists and calculate how much rounds to loop */
    for (i = 0; i < nbrOfLists * 2; i += 2) {
        div_t cnt;
        int count;

        listsEnd[i] = Jim_ListLength(interp, argv[i + 1]);
        listsEnd[i + 1] = Jim_ListLength(interp, argv[i + 2]);
        if (listsEnd[i] == 0) {
            Jim_SetResultString(interp, "foreach varlist is empty", -1);
            goto err;
        }
        cnt = div(listsEnd[i + 1], listsEnd[i]);
        count = cnt.quot + (cnt.rem ? 1 : 0);
        if (count > nbrOfLoops)
            nbrOfLoops = count;
    }
    for (; nbrOfLoops-- > 0;) {
        for (i = 0; i < nbrOfLists; ++i) {
            int varIdx = 0, var = i * 2;

            while (varIdx < listsEnd[var]) {
                Jim_Obj *varName, *ele;
                int lst = i * 2 + 1;

                /* List index operations below can't fail */
                Jim_ListIndex(interp, argv[var + 1], varIdx, &varName, JIM_NONE);
                if (listsIdx[i] < listsEnd[lst]) {
                    Jim_ListIndex(interp, argv[lst + 1], listsIdx[i], &ele, JIM_NONE);
                    /* Avoid shimmering */
                    Jim_IncrRefCount(ele);
                    result = Jim_SetVariable(interp, varName, ele);
                    Jim_DecrRefCount(interp, ele);
                    if (result == JIM_OK) {
                        ++listsIdx[i];  /* Remember next iterator of current list */
                        ++varIdx;       /* Next variable */
                        continue;
                    }
                }
                else if (Jim_SetVariable(interp, varName, emptyStr) == JIM_OK) {
                    ++varIdx;   /* Next variable */
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
static int Jim_ForeachCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimForeachMapHelper(interp, argc, argv, 0);
}

/* [lmap] */
static int Jim_LmapCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimForeachMapHelper(interp, argc, argv, 1);
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
        const char *option = Jim_GetString(argv[opt], 0);

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

    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "list index ?...?");
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
        Jim_Obj *objPtr;
        int eq = 0;

        Jim_ListIndex(interp, argv[0], i, &objPtr, JIM_NONE);
        switch (opt_match) {
            case OPT_EXACT:
                eq = Jim_StringCompareObj(interp, objPtr, argv[1], opt_nocase) == 0;
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
    int shared, i;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "varName ?value value ...?");
        return JIM_ERR;
    }
    listObjPtr = Jim_GetVariable(interp, argv[1], JIM_UNSHARED);
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
static int Jim_LinsertCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int idx, len;
    Jim_Obj *listPtr;

    if (argc < 4) {
        Jim_WrongNumArgs(interp, 1, argv, "list index element " "?element ...?");
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
static int Jim_LsetCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "listVar ?index...? newVal");
        return JIM_ERR;
    }
    else if (argc == 3) {
        if (Jim_SetVariable(interp, argv[1], argv[2]) != JIM_OK)
            return JIM_ERR;
        Jim_SetResult(interp, argv[2]);
        return JIM_OK;
    }
    if (Jim_SetListIndex(interp, argv[1], argv + 2, argc - 3, argv[argc - 1])
        == JIM_ERR)
        return JIM_ERR;
    return JIM_OK;
}

/* [lsort] */
static int Jim_LsortCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const argv[])
{
    static const char * const options[] = {
        "-ascii", "-nocase", "-increasing", "-decreasing", "-command", "-integer", "-index", NULL
    };
    enum
    { OPT_ASCII, OPT_NOCASE, OPT_INCREASING, OPT_DECREASING, OPT_COMMAND, OPT_INTEGER, OPT_INDEX };
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
    info.command = NULL;
    info.interp = interp;

    for (i = 1; i < (argc - 1); i++) {
        int option;

        if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ERRMSG)
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
            case OPT_INCREASING:
                info.order = 1;
                break;
            case OPT_DECREASING:
                info.order = -1;
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
        Jim_WrongNumArgs(interp, 1, argv, "varName ?value value ...?");
        return JIM_ERR;
    }
    if (argc == 2) {
        stringObjPtr = Jim_GetVariable(interp, argv[1], JIM_ERRMSG);
        if (!stringObjPtr)
            return JIM_ERR;
    }
    else {
        int freeobj = 0;
        stringObjPtr = Jim_GetVariable(interp, argv[1], JIM_UNSHARED);
        if (!stringObjPtr) {
            /* Create the string if it doesn't exist */
            stringObjPtr = Jim_NewEmptyStringObj(interp);
            freeobj = 1;
        }
        else if (Jim_IsShared(stringObjPtr)) {
            freeobj = 1;
            stringObjPtr = Jim_DuplicateObj(interp, stringObjPtr);
        }
        for (i = 2; i < argc; i++) {
            Jim_AppendObj(interp, stringObjPtr, argv[i]);
        }
        if (Jim_SetVariable(interp, argv[1], stringObjPtr) != JIM_OK) {
            if (freeobj) {
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
#ifdef JIM_DEBUG_COMMAND
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
        charlen = Jim_Utf8Length(interp, argv[2]);
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
        script = Jim_GetScript(interp, argv[2]);
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
#else
    Jim_SetResultString(interp, "unsupported", -1);
    return JIM_ERR;
#endif
}

/* [eval] */
static int Jim_EvalCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
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
        Jim_Obj *objPtr;
        const char *str;

        /* Save the old callframe pointer */
        savedCallFrame = interp->framePtr;

        /* Lookup the target frame pointer */
        str = Jim_String(argv[1]);
        if ((str[0] >= '0' && str[0] <= '9') || str[0] == '#') {
            targetCallFrame =Jim_GetCallFrameByLevel(interp, argv[1]);
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
            argv--;
            Jim_WrongNumArgs(interp, 1, argv, "?level? command ?arg ...?");
            return JIM_ERR;
        }
        /* Eval the code in the target callframe. */
        interp->framePtr = targetCallFrame;
        if (argc == 2) {
            retcode = Jim_EvalObj(interp, argv[1]);
        }
        else {
            objPtr = Jim_ConcatObj(interp, argc - 1, argv + 1);
            Jim_IncrRefCount(objPtr);
            retcode = Jim_EvalObj(interp, objPtr);
            Jim_DecrRefCount(interp, objPtr);
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
    Jim_Obj *objPtr;

    objPtr = Jim_NewListObj(interp, argv + 1, argc - 1);
    Jim_SetResult(interp, objPtr);
    return JIM_EVAL;
}

/* [proc] */
static int Jim_ProcCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 4 && argc != 5) {
        Jim_WrongNumArgs(interp, 1, argv, "name arglist ?statics? body");
        return JIM_ERR;
    }

    if (argc == 4) {
        return JimCreateProcedure(interp, argv[1], argv[2], NULL, argv[3]);
    }
    else {
        return JimCreateProcedure(interp, argv[1], argv[2], argv[3], argv[4]);
    }
}

/* [local] */
static int Jim_LocalCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int retcode;

    /* Evaluate the arguments with 'local' in force */
    interp->local++;
    retcode = Jim_EvalObjVector(interp, argc - 1, argv + 1);
    interp->local--;


    /* If OK, and the result is a proc, add it to the list of local procs */
    if (retcode == 0) {
        const char *procname = Jim_String(Jim_GetResult(interp));

        if (Jim_FindHashEntry(&interp->commands, procname) == NULL) {
            Jim_SetResultFormatted(interp, "not a proc: \"%s\"", procname);
            return JIM_ERR;
        }
        if (interp->localProcs == NULL) {
            interp->localProcs = Jim_Alloc(sizeof(*interp->localProcs));
            Jim_InitStack(interp->localProcs);
        }
        Jim_StackPush(interp->localProcs, Jim_StrDup(procname));
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
        if (cmdPtr == NULL || !cmdPtr->isproc || !cmdPtr->u.proc.prevCmd) {
            Jim_SetResultFormatted(interp, "no previous proc: \"%#s\"", argv[1]);
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
        if (Jim_SetVariableLink(interp, argv[i], argv[i], interp->topFramePtr) != JIM_OK)
            return JIM_ERR;
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

            Jim_ListIndex(interp, mapListObjPtr, i, &objPtr, JIM_NONE);
            k = Jim_String(objPtr);
            kl = Jim_Utf8Length(interp, objPtr);

            if (strLen >= kl && kl) {
                int rc;
                if (nocase) {
                    rc = JimStringCompareNoCase(str, k, kl);
                }
                else {
                    rc = JimStringCompare(str, kl, k, kl);
                }
                if (rc == 0) {
                    if (noMatchStart) {
                        Jim_AppendString(interp, resultObjPtr, noMatchStart, str - noMatchStart);
                        noMatchStart = NULL;
                    }
                    Jim_ListIndex(interp, mapListObjPtr, i + 1, &objPtr, JIM_NONE);
                    Jim_AppendObj(interp, resultObjPtr, objPtr);
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
        "bytelength", "length", "compare", "match", "equal", "is", "byterange", "range", "map",
        "repeat", "reverse", "index", "first", "last",
        "trim", "trimleft", "trimright", "tolower", "toupper", NULL
    };
    enum
    {
        OPT_BYTELENGTH, OPT_LENGTH, OPT_COMPARE, OPT_MATCH, OPT_EQUAL, OPT_IS, OPT_BYTERANGE, OPT_RANGE, OPT_MAP,
        OPT_REPEAT, OPT_REVERSE, OPT_INDEX, OPT_FIRST, OPT_LAST,
        OPT_TRIM, OPT_TRIMLEFT, OPT_TRIMRIGHT, OPT_TOLOWER, OPT_TOUPPER
    };
    static const char * const nocase_options[] = {
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

        case OPT_COMPARE:
        case OPT_EQUAL:
            if (argc != 4 &&
                (argc != 5 ||
                    Jim_GetEnum(interp, argv[2], nocase_options, &opt_case, NULL,
                        JIM_ENUM_ABBREV) != JIM_OK)) {
                Jim_WrongNumArgs(interp, 2, argv, "?-nocase? string1 string2");
                return JIM_ERR;
            }
            if (opt_case == 0) {
                argv++;
            }
            if (option == OPT_COMPARE || !opt_case) {
                Jim_SetResultInt(interp, Jim_StringCompareObj(interp, argv[2], argv[3], !opt_case));
            }
            else {
                Jim_SetResultBool(interp, Jim_StringEqObj(argv[2], argv[3]));
            }
            return JIM_OK;

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
                if (!str) {
                    return JIM_ERR;
                }

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

    /* Which return codes are caught? These are the defaults */
    jim_wide mask =
        (1 << JIM_OK | 1 << JIM_ERR | 1 << JIM_BREAK | 1 << JIM_CONTINUE | 1 << JIM_RETURN);

    /* Reset the error code before catch.
     * Note that this is not strictly correct.
     */
    Jim_SetGlobalVariableStr(interp, "errorCode", Jim_NewStringObj(interp, "NONE", -1));

    for (i = 1; i < argc - 1; i++) {
        const char *arg = Jim_String(argv[i]);
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
        Jim_WrongNumArgs(interp, 1, argv,
            "?-?no?code ... --? script ?resultVarName? ?optionVarName?");
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

    /* Catch or pass through? Only the first 32/64 codes can be passed through */
    if (exitCode >= 0 && exitCode < (int)sizeof(mask) * 8 && ((1 << exitCode) & mask) == 0) {
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
    Jim_HashTableIterator *htiter;
    Jim_HashEntry *he;

    listObjPtr = Jim_NewListObj(interp, NULL, 0);

    htiter = Jim_GetHashTableIterator(&interp->references);
    while ((he = Jim_NextHashEntry(htiter)) != NULL) {
        char buf[JIM_REFERENCE_SPACE];
        Jim_Reference *refPtr = he->u.val;
        const jim_wide *refId = he->key;

        JimFormatReference(buf, refPtr, *refId);
        Jim_ListAppendElement(interp, listObjPtr, Jim_NewStringObj(interp, buf, -1));
    }
    Jim_FreeHashTableIterator(htiter);
    Jim_SetResult(interp, listObjPtr);
    return JIM_OK;
}
#endif

/* [rename] */
static int Jim_RenameCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *oldName, *newName;

    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "oldName newName");
        return JIM_ERR;
    }

    if (JimValidName(interp, "new procedure", argv[2])) {
        return JIM_ERR;
    }

    oldName = Jim_String(argv[1]);
    newName = Jim_String(argv[2]);
    return Jim_RenameCommand(interp, oldName, newName);
}

int Jim_DictKeys(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *patternObj)
{
    int i;
    int len;
    Jim_Obj *resultObj;
    Jim_Obj *dictObj;
    Jim_Obj **dictValuesObj;

    if (Jim_DictKeysVector(interp, objPtr, NULL, 0, &dictObj, JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    /* XXX: Could make the exact-match case much more efficient here.
     *      See JimCommandsList()
     */
    if (Jim_DictPairs(interp, dictObj, &dictValuesObj, &len) != JIM_OK) {
        return JIM_ERR;
    }

    /* Only return the matching values */
    resultObj = Jim_NewListObj(interp, NULL, 0);

    for (i = 0; i < len; i += 2) {
        if (patternObj == NULL || Jim_StringMatchObj(interp, patternObj, dictValuesObj[i], 0)) {
            Jim_ListAppendElement(interp, resultObj, dictValuesObj[i]);
        }
    }
    Jim_Free(dictValuesObj);

    Jim_SetResult(interp, resultObj);
    return JIM_OK;
}

int Jim_DictSize(Jim_Interp *interp, Jim_Obj *objPtr)
{
    if (SetDictFromAny(interp, objPtr) != JIM_OK) {
        return -1;
    }
    return ((Jim_HashTable *)objPtr->internalRep.ptr)->used;
}

/* [dict] */
static int Jim_DictCoreCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr;
    int option;
    static const char * const options[] = {
        "create", "get", "set", "unset", "exists", "keys", "merge", "size", "with", NULL
    };
    enum
    {
        OPT_CREATE, OPT_GET, OPT_SET, OPT_UNSET, OPT_EXIST, OPT_KEYS, OPT_MERGE, OPT_SIZE, OPT_WITH,
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
                Jim_WrongNumArgs(interp, 2, argv, "varName ?key ...?");
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
            return Jim_SetDictKeysVector(interp, argv[2], argv + 3, argc - 4, argv[argc - 1]);

        case OPT_EXIST:
            if (argc < 3) {
                Jim_WrongNumArgs(interp, 2, argv, "varName ?key ...?");
                return JIM_ERR;
            }
            Jim_SetResultBool(interp, Jim_DictKeysVector(interp, argv[2], argv + 3, argc - 3,
                    &objPtr, JIM_ERRMSG) == JIM_OK);
            return JIM_OK;

        case OPT_UNSET:
            if (argc < 4) {
                Jim_WrongNumArgs(interp, 2, argv, "varName key ?key ...?");
                return JIM_ERR;
            }
            return Jim_SetDictKeysVector(interp, argv[2], argv + 3, argc - 3, NULL);

        case OPT_KEYS:
            if (argc != 3 && argc != 4) {
                Jim_WrongNumArgs(interp, 2, argv, "dictVar ?pattern?");
                return JIM_ERR;
            }
            return Jim_DictKeys(interp, argv[2], argc == 4 ? argv[3] : NULL);

        case OPT_SIZE: {
            int size;

            if (argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "dictVar");
                return JIM_ERR;
            }

            size = Jim_DictSize(interp, argv[2]);
            if (size < 0) {
                return JIM_ERR;
            }
            Jim_SetResultInt(interp, size);
            return JIM_OK;
        }

        case OPT_MERGE:
            if (argc == 2) {
                return JIM_OK;
            }
            else if (argv[2]->typePtr != &dictObjType && SetDictFromAny(interp, argv[2]) != JIM_OK) {
                return JIM_ERR;
            }
            else {
                return Jim_EvalObjPrefix(interp, "dict merge", argc - 2, argv + 2);
            }

        case OPT_WITH:
            if (argc < 4) {
                Jim_WrongNumArgs(interp, 2, argv, "dictVar ?key ...? script");
                return JIM_ERR;
            }
            else if (Jim_GetVariable(interp, argv[2], JIM_ERRMSG) == NULL) {
                return JIM_ERR;
            }
            else {
                return Jim_EvalObjPrefix(interp, "dict with", argc - 2, argv + 2);
            }

        case OPT_CREATE:
            if (argc % 2) {
                Jim_WrongNumArgs(interp, 2, argv, "?key value ...?");
                return JIM_ERR;
            }
            objPtr = Jim_NewDictObj(interp, argv + 2, argc - 2);
            Jim_SetResult(interp, objPtr);
            return JIM_OK;

        default:
            abort();
    }
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
        "body", "commands", "procs", "channels", "exists", "globals", "level", "frame", "locals",
        "vars", "version", "patchlevel", "complete", "args", "hostname",
        "script", "source", "stacktrace", "nameofexecutable", "returncodes",
        "references", NULL
    };
    enum
    { INFO_BODY, INFO_COMMANDS, INFO_PROCS, INFO_CHANNELS, INFO_EXISTS, INFO_GLOBALS, INFO_LEVEL,
        INFO_FRAME, INFO_LOCALS, INFO_VARS, INFO_VERSION, INFO_PATCHLEVEL, INFO_COMPLETE, INFO_ARGS,
        INFO_HOSTNAME, INFO_SCRIPT, INFO_SOURCE, INFO_STACKTRACE, INFO_NAMEOFEXECUTABLE,
        INFO_RETURNCODES, INFO_REFERENCES,
    };

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
        case INFO_EXISTS:{
                if (argc != 3) {
                    Jim_WrongNumArgs(interp, 2, argv, "varName");
                    return JIM_ERR;
                }
                Jim_SetResultBool(interp, Jim_GetVariable(interp, argv[2], 0) != NULL);
                break;
            }

        case INFO_CHANNELS:
#ifndef jim_ext_aio
            Jim_SetResultString(interp, "aio not enabled", -1);
            return JIM_ERR;
#endif
        case INFO_COMMANDS:
        case INFO_PROCS:
            if (argc != 2 && argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "?pattern?");
                return JIM_ERR;
            }
            Jim_SetResult(interp, JimCommandsList(interp, (argc == 3) ? argv[2] : NULL,
                    (cmd - INFO_COMMANDS)));
            break;

        case INFO_VARS:
            mode++;             /* JIM_VARLIST_VARS */
        case INFO_LOCALS:
            mode++;             /* JIM_VARLIST_LOCALS */
        case INFO_GLOBALS:
            /* mode 0 => JIM_VARLIST_GLOBALS */
            if (argc != 2 && argc != 3) {
                Jim_WrongNumArgs(interp, 2, argv, "?pattern?");
                return JIM_ERR;
            }
            Jim_SetResult(interp, JimVariablesList(interp, argc == 3 ? argv[2] : NULL, mode));
            break;

        case INFO_SCRIPT:
            if (argc != 2) {
                Jim_WrongNumArgs(interp, 2, argv, "");
                return JIM_ERR;
            }
            Jim_SetResultString(interp, Jim_GetScript(interp, interp->currentScriptObj)->fileName,
                -1);
            break;

        case INFO_SOURCE:{
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
                    line = script->line;
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
                Jim_SetResult(interp,
                    cmd == INFO_BODY ? cmdPtr->u.proc.bodyObjPtr : cmdPtr->u.proc.argListObjPtr);
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
                int len;
                const char *s = Jim_GetString(argv[2], &len);
                char missing;

                Jim_SetResultBool(interp, Jim_ScriptIsComplete(s, len, &missing));
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

    static const char * const options[] = {
        "-command", "-proc", "-var", NULL
    };
    enum
    {
        OPT_COMMAND, OPT_PROC, OPT_VAR
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

    /* Test for the the most common commands first, just in case it makes a difference */
    switch (option) {
        case OPT_VAR:
            Jim_SetResultBool(interp, Jim_GetVariable(interp, objPtr, 0) != NULL);
            break;

        case OPT_COMMAND:
        case OPT_PROC: {
            Jim_Cmd *cmd = Jim_GetCommand(interp, objPtr, JIM_NONE);
            Jim_SetResultBool(interp, cmd != NULL && (option == OPT_COMMAND || cmd->isproc));
            break;
        }
    }
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
    }
    else {
        joinStr = Jim_GetString(argv[2], &joinStrLen);
    }
    listLen = Jim_ListLength(interp, argv[1]);
    resObjPtr = Jim_NewStringObj(interp, NULL, 0);
    /* Split */
    for (i = 0; i < listLen; i++) {
        Jim_Obj *objPtr = 0;

        Jim_ListIndex(interp, argv[1], i, &objPtr, JIM_NONE);
        Jim_AppendObj(interp, resObjPtr, objPtr);
        if (i + 1 != listLen) {
            Jim_AppendString(interp, resObjPtr, joinStr, joinStrLen);
        }
    }
    Jim_SetResult(interp, resObjPtr);
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
        int i;

        for (i = 0; i < argc; i++) {
            ListAppendElement(objPtr, argv[i]);
        }
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
    {"loop", Jim_LoopCoreCommand},
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
#include <stdio.h>
#include <string.h>


/**
 * Implements the common 'commands' subcommand
 */
static int subcmd_null(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    /* Nothing to do, since the result has already been created */
    return JIM_OK;
}

/**
 * Do-nothing command to support -commands and -usage
 */
static const jim_subcmd_type dummy_subcmd = {
    .cmd = "dummy",
    .function = subcmd_null,
    .flags = JIM_MODFLAG_HIDDEN,
};

static void add_commands(Jim_Interp *interp, const jim_subcmd_type * ct, const char *sep)
{
    const char *s = "";

    for (; ct->cmd; ct++) {
        if (!(ct->flags & JIM_MODFLAG_HIDDEN)) {
            Jim_AppendStrings(interp, Jim_GetResult(interp), s, ct->cmd, NULL);
            s = sep;
        }
    }
}

static void bad_subcmd(Jim_Interp *interp, const jim_subcmd_type * command_table, const char *type,
    Jim_Obj *cmd, Jim_Obj *subcmd)
{
    Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
    Jim_AppendStrings(interp, Jim_GetResult(interp), Jim_String(cmd), ", ", type,
        " command \"", Jim_String(subcmd), "\": should be ", NULL);
    add_commands(interp, command_table, ", ");
}

static void show_cmd_usage(Jim_Interp *interp, const jim_subcmd_type * command_table, int argc,
    Jim_Obj *const *argv)
{
    Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
    Jim_AppendStrings(interp, Jim_GetResult(interp), "Usage: \"", Jim_String(argv[0]),
        " command ... \", where command is one of: ", NULL);
    add_commands(interp, command_table, ", ");
}

static void add_cmd_usage(Jim_Interp *interp, const jim_subcmd_type * ct, Jim_Obj *cmd)
{
    if (cmd) {
        Jim_AppendStrings(interp, Jim_GetResult(interp), Jim_String(cmd), " ", NULL);
    }
    Jim_AppendStrings(interp, Jim_GetResult(interp), ct->cmd, NULL);
    if (ct->args && *ct->args) {
        Jim_AppendStrings(interp, Jim_GetResult(interp), " ", ct->args, NULL);
    }
}

static void show_full_usage(Jim_Interp *interp, const jim_subcmd_type * ct, int argc,
    Jim_Obj *const *argv)
{
    Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
    for (; ct->cmd; ct++) {
        if (!(ct->flags & JIM_MODFLAG_HIDDEN)) {
            /* subcmd */
            add_cmd_usage(interp, ct, argv[0]);
            if (ct->description) {
                Jim_AppendStrings(interp, Jim_GetResult(interp), "\n\n    ", ct->description, NULL);
            }
            Jim_AppendStrings(interp, Jim_GetResult(interp), "\n\n", NULL);
        }
    }
}

static void set_wrong_args(Jim_Interp *interp, const jim_subcmd_type * command_table, Jim_Obj *subcmd)
{
    Jim_SetResultString(interp, "wrong # args: must be \"", -1);
    add_cmd_usage(interp, command_table, subcmd);
    Jim_AppendStrings(interp, Jim_GetResult(interp), "\"", NULL);
}

const jim_subcmd_type *Jim_ParseSubCmd(Jim_Interp *interp, const jim_subcmd_type * command_table,
    int argc, Jim_Obj *const *argv)
{
    const jim_subcmd_type *ct;
    const jim_subcmd_type *partial = 0;
    int cmdlen;
    Jim_Obj *cmd;
    const char *cmdstr;
    const char *cmdname;
    int help = 0;

    cmdname = Jim_String(argv[0]);

    if (argc < 2) {
        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        Jim_AppendStrings(interp, Jim_GetResult(interp), "wrong # args: should be \"", cmdname,
            " command ...\"\n", NULL);
        Jim_AppendStrings(interp, Jim_GetResult(interp), "Use \"", cmdname, " -help\" or \"",
            cmdname, " -help command\" for help", NULL);
        return 0;
    }

    cmd = argv[1];

    if (argc == 2 && Jim_CompareStringImmediate(interp, cmd, "-usage")) {
        /* Show full usage */
        show_full_usage(interp, command_table, argc, argv);
        return &dummy_subcmd;
    }

    /* Check for the help command */
    if (Jim_CompareStringImmediate(interp, cmd, "-help")) {
        if (argc == 2) {
            /* Usage for the command, not the subcommand */
            show_cmd_usage(interp, command_table, argc, argv);
            return &dummy_subcmd;
        }
        help = 1;

        /* Skip the 'help' command */
        cmd = argv[2];
    }

    /* Check for special builtin '-commands' command first */
    if (Jim_CompareStringImmediate(interp, cmd, "-commands")) {
        /* Build the result here */
        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        add_commands(interp, command_table, " ");
        return &dummy_subcmd;
    }

    cmdstr = Jim_GetString(cmd, &cmdlen);

    for (ct = command_table; ct->cmd; ct++) {
        if (Jim_CompareStringImmediate(interp, cmd, ct->cmd)) {
            /* Found an exact match */
            break;
        }
        if (strncmp(cmdstr, ct->cmd, cmdlen) == 0) {
            if (partial) {
                /* Ambiguous */
                if (help) {
                    /* Just show the top level help here */
                    show_cmd_usage(interp, command_table, argc, argv);
                    return &dummy_subcmd;
                }
                bad_subcmd(interp, command_table, "ambiguous", argv[0], argv[1 + help]);
                return 0;
            }
            partial = ct;
        }
        continue;
    }

    /* If we had an unambiguous partial match */
    if (partial && !ct->cmd) {
        ct = partial;
    }

    if (!ct->cmd) {
        /* No matching command */
        if (help) {
            /* Just show the top level help here */
            show_cmd_usage(interp, command_table, argc, argv);
            return &dummy_subcmd;
        }
        bad_subcmd(interp, command_table, "unknown", argv[0], argv[1 + help]);
        return 0;
    }

    if (help) {
        Jim_SetResultString(interp, "Usage: ", -1);
        /* subcmd */
        add_cmd_usage(interp, ct, argv[0]);
        if (ct->description) {
            Jim_AppendStrings(interp, Jim_GetResult(interp), "\n\n", ct->description, NULL);
        }
        return &dummy_subcmd;
    }

    /* Check the number of args */
    if (argc - 2 < ct->minargs || (ct->maxargs >= 0 && argc - 2 > ct->maxargs)) {
        Jim_SetResultString(interp, "wrong # args: must be \"", -1);
        /* subcmd */
        add_cmd_usage(interp, ct, argv[0]);
        Jim_AppendStrings(interp, Jim_GetResult(interp), "\"", NULL);

        return 0;
    }

    /* Good command */
    return ct;
}

int Jim_CallSubCmd(Jim_Interp *interp, const jim_subcmd_type * ct, int argc, Jim_Obj *const *argv)
{
    int ret = JIM_ERR;

    if (ct) {
        if (ct->flags & JIM_MODFLAG_FULLARGV) {
            ret = ct->function(interp, argc, argv);
        }
        else {
            ret = ct->function(interp, argc - 2, argv + 2);
        }
        if (ret < 0) {
            set_wrong_args(interp, ct, argv[0]);
            ret = JIM_ERR;
        }
    }
    return ret;
}

int Jim_SubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const jim_subcmd_type *ct =
        Jim_ParseSubCmd(interp, (const jim_subcmd_type *)Jim_CmdPrivData(interp), argc, argv);

    return Jim_CallSubCmd(interp, ct, argc, argv);
}

/* The following two functions are for normal commands */
int
Jim_CheckCmdUsage(Jim_Interp *interp, const jim_subcmd_type * command_table, int argc,
    Jim_Obj *const *argv)
{
    /* -usage or -help */
    if (argc == 2) {
        if (Jim_CompareStringImmediate(interp, argv[1], "-usage")
            || Jim_CompareStringImmediate(interp, argv[1], "-help")) {
            Jim_SetResultString(interp, "Usage: ", -1);
            add_cmd_usage(interp, command_table, NULL);
            if (command_table->description) {
                Jim_AppendStrings(interp, Jim_GetResult(interp), "\n\n", command_table->description,
                    NULL);
            }
            return JIM_OK;
        }
    }
    if (argc >= 2 && command_table->function) {
        /* This is actually a sub command table */

        Jim_Obj *nargv[4];
        int nargc = 0;
        const char *subcmd = NULL;

        if (Jim_CompareStringImmediate(interp, argv[1], "-subcommands")) {
            Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
            add_commands(interp, (jim_subcmd_type *) command_table->function, " ");
            return JIM_OK;
        }

        if (Jim_CompareStringImmediate(interp, argv[1], "-subhelp")
            || Jim_CompareStringImmediate(interp, argv[1], "-help")) {
            subcmd = "-help";
        }
        else if (Jim_CompareStringImmediate(interp, argv[1], "-subusage")) {
            subcmd = "-usage";
        }

        if (subcmd) {
            nargv[nargc++] = Jim_NewStringObj(interp, "$handle", -1);
            nargv[nargc++] = Jim_NewStringObj(interp, subcmd, -1);
            if (argc >= 3) {
                nargv[nargc++] = argv[2];
            }
            Jim_ParseSubCmd(interp, (jim_subcmd_type *) command_table->function, nargc, nargv);
            Jim_FreeNewObj(interp, nargv[0]);
            Jim_FreeNewObj(interp, nargv[1]);
            return 0;
        }
    }

    /* Check the number of args */
    if (argc - 1 < command_table->minargs || (command_table->maxargs >= 0
            && argc - 1 > command_table->maxargs)) {
        set_wrong_args(interp, command_table, NULL);
        Jim_AppendStrings(interp, Jim_GetResult(interp), "\nUse \"", Jim_String(argv[0]),
            " -help\" for help", NULL);
        return JIM_ERR;
    }

    /* Not usage, but passed arg checking */
    return -1;
}
/**
 * UTF-8 utility functions
 *
 * (c) 2010 Steve Bennett <steveb@workware.net.au>
 *
 * See LICENCE for licence details.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* This one is always implemented */
int utf8_fromunicode(char *p, unsigned short uc)
{
    if (uc <= 0x7f) {
        *p = uc;
        return 1;
    }
    else if (uc <= 0x7ff) {
        *p++ = 0xc0 | ((uc & 0x7c0) >> 6);
        *p = 0x80 | (uc & 0x3f);
        return 2;
    }
    else {
        *p++ = 0xe0 | ((uc & 0xf000) >> 12);
        *p++ = 0x80 | ((uc & 0xfc0) >> 6);
        *p = 0x80 | (uc & 0x3f);
        return 3;
    }
}

#ifdef JIM_UTF8
int utf8_charlen(int c)
{
    if ((c & 0x80) == 0) {
        return 1;
    }
    if ((c & 0xe0) == 0xc0) {
        return 2;
    }
    if ((c & 0xf0) == 0xe0) {
        return 3;
    }
    if ((c & 0xf8) == 0xf0) {
        return 4;
    }
    /* Invalid sequence */
    return -1;
}

int utf8_strlen(const char *str, int bytelen)
{
    int charlen = 0;
    if (bytelen < 0) {
        bytelen = strlen(str);
    }
    while (bytelen) {
        int c;
        int l = utf8_tounicode(str, &c);
        charlen++;
        str += l;
        bytelen -= l;
    }
    return charlen;
}

int utf8_index(const char *str, int index)
{
    const char *s = str;
    while (index--) {
        int c;
        s += utf8_tounicode(s, &c);
    }
    return s - str;
}

int utf8_charequal(const char *s1, const char *s2)
{
    int c1, c2;

    utf8_tounicode(s1, &c1);
    utf8_tounicode(s2, &c2);

    return c1 == c2;
}

int utf8_prev_len(const char *str, int len)
{
    int n = 1;

    assert(len > 0);

    /* Look up to len chars backward for a start-of-char byte */
    while (--len) {
        if ((str[-n] & 0x80) == 0) {
            /* Start of a 1-byte char */
            break;
        }
        if ((str[-n] & 0xc0) == 0xc0) {
            /* Start of a multi-byte char */
            break;
        }
        n++;
    }
    return n;
}

int utf8_tounicode(const char *str, int *uc)
{
    unsigned const char *s = (unsigned const char *)str;

    if (s[0] < 0xc0) {
        *uc = s[0];
        return 1;
    }
    if (s[0] < 0xe0) {
        if ((s[1] & 0xc0) == 0x80) {
            *uc = ((s[0] & ~0xc0) << 6) | (s[1] & ~0x80);
            return 2;
        }
    }
    else if (s[0] < 0xf0) {
        if (((str[1] & 0xc0) == 0x80) && ((str[2] & 0xc0) == 0x80)) {
            *uc = ((s[0] & ~0xe0) << 12) | ((s[1] & ~0x80) << 6) | (s[2] & ~0x80);
            return 3;
        }
    }

    /* Invalid sequence, so just return the byte */
    *uc = *s;
    return 1;
}

struct casemap {
    unsigned short code;    /* code point */
    signed char lowerdelta; /* add for lowercase, or if -128 use the ext table */
    signed char upperdelta; /* add for uppercase, or offset into the ext table */
};

/* Extended table for codepoints where |delta| > 127 */
struct caseextmap {
    unsigned short lower;
    unsigned short upper;
};

/* Generated mapping tables */
#include "_unicode_mapping.c"

#define NUMCASEMAP sizeof(unicode_case_mapping) / sizeof(*unicode_case_mapping)

static int cmp_casemap(const void *key, const void *cm)
{
    return *(int *)key - (int)((const struct casemap *)cm)->code;
}

static int utf8_map_case(int uc, int upper)
{
    const struct casemap *cm = bsearch(&uc, unicode_case_mapping, NUMCASEMAP, sizeof(*unicode_case_mapping), cmp_casemap);

    if (cm) {
        if (cm->lowerdelta == -128) {
            uc = upper ? unicode_extmap[cm->upperdelta].upper : unicode_extmap[cm->upperdelta].lower;
        }
        else {
            uc += upper ? cm->upperdelta : cm->lowerdelta;
        }
    }
    return uc;
}

int utf8_upper(int uc)
{
    if (isascii(uc)) {
        return toupper(uc);
    }
    return utf8_map_case(uc, 1);
}

int utf8_lower(int uc)
{
    if (isascii(uc)) {
        return tolower(uc);
    }

    return utf8_map_case(uc, 0);
}

#endif
#include <errno.h>
#include <string.h>

#ifdef USE_LINENOISE
#include <unistd.h>
#include "linenoise.h"
#else

#define MAX_LINE_LEN 512

static char *linenoise(const char *prompt)
{
    char *line = malloc(MAX_LINE_LEN);

    fputs(prompt, stdout);
    fflush(stdout);

    if (fgets(line, MAX_LINE_LEN, stdin) == NULL) {
        free(line);
        return NULL;
    }
    return line;
}
#endif

int Jim_InteractivePrompt(Jim_Interp *interp)
{
    int retcode = JIM_OK;
    char *history_file = NULL;
#ifdef USE_LINENOISE
    const char *home;

    home = getenv("HOME");
    if (home && isatty(STDIN_FILENO)) {
        int history_len = strlen(home) + sizeof("/.jim_history");
        history_file = Jim_Alloc(history_len);
        snprintf(history_file, history_len, "%s/.jim_history", home);
        linenoiseHistoryLoad(history_file);
    }
#endif

    printf("Welcome to Jim version %d.%d" JIM_NL,
        JIM_VERSION / 100, JIM_VERSION % 100);
    Jim_SetVariableStrWithStr(interp, JIM_INTERACTIVE, "1");

    while (1) {
        Jim_Obj *scriptObjPtr;
        const char *result;
        int reslen;
        char prompt[20];
        const char *str;

        if (retcode != 0) {
            const char *retcodestr = Jim_ReturnCode(retcode);

            if (*retcodestr == '?') {
                snprintf(prompt, sizeof(prompt) - 3, "[%d] ", retcode);
            }
            else {
                snprintf(prompt, sizeof(prompt) - 3, "[%s] ", retcodestr);
            }
        }
        else {
            prompt[0] = '\0';
        }
        strcat(prompt, ". ");

        scriptObjPtr = Jim_NewStringObj(interp, "", 0);
        Jim_IncrRefCount(scriptObjPtr);
        while (1) {
            char state;
            int len;
            char *line;

            line = linenoise(prompt);
            if (line == NULL) {
                if (errno == EINTR) {
                    continue;
                }
                Jim_DecrRefCount(interp, scriptObjPtr);
                goto out;
            }
            if (Jim_Length(scriptObjPtr) != 0) {
                Jim_AppendString(interp, scriptObjPtr, "\n", 1);
            }
            Jim_AppendString(interp, scriptObjPtr, line, -1);
            free(line);
            str = Jim_GetString(scriptObjPtr, &len);
            if (len == 0) {
                continue;
            }
            if (Jim_ScriptIsComplete(str, len, &state))
                break;

            snprintf(prompt, sizeof(prompt), "%c> ", state);
        }
#ifdef USE_LINENOISE
        if (strcmp(str, "h") == 0) {
            /* built-in history command */
            int i;
            int len;
            char **history = linenoiseHistory(&len);
            for (i = 0; i < len; i++) {
                printf("%4d %s\n", i + 1, history[i]);
            }
            Jim_DecrRefCount(interp, scriptObjPtr);
            continue;
        }

        linenoiseHistoryAdd(Jim_String(scriptObjPtr));
        if (history_file) {
            linenoiseHistorySave(history_file);
        }
#endif
        retcode = Jim_EvalObj(interp, scriptObjPtr);
        Jim_DecrRefCount(interp, scriptObjPtr);



        if (retcode == JIM_EXIT) {
            Jim_Free(history_file);
            return JIM_EXIT;
        }
        if (retcode == JIM_ERR) {
            Jim_MakeErrorMessage(interp);
        }
        result = Jim_GetString(Jim_GetResult(interp), &reslen);
        if (reslen) {
            printf("%s\n", result);
        }
    }
  out:
    Jim_Free(history_file);
    return JIM_OK;
}
/* 
 * Implements the internals of the format command for jim
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
 *
 * Based on code originally from Tcl 8.5:
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 1999 by Scriptics Corporation.
 *
 * See the file "tcl.license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#include <ctype.h>
#include <string.h>


#define JIM_UTF_MAX 3
#define JIM_INTEGER_SPACE 24
#define MAX_FLOAT_WIDTH 320

/**
 * Apply the printf-like format in fmtObjPtr with the given arguments.
 *
 * Returns a new object with zero reference count if OK, or NULL on error.
 */
Jim_Obj *Jim_FormatString(Jim_Interp *interp, Jim_Obj *fmtObjPtr, int objc, Jim_Obj *const *objv)
{
    const char *span, *format, *formatEnd, *msg;
    int numBytes = 0, objIndex = 0, gotXpg = 0, gotSequential = 0;
    static const char *mixedXPG =
	    "cannot mix \"%\" and \"%n$\" conversion specifiers";
    static const char *badIndex[2] = {
	"not enough arguments for all format specifiers",
	"\"%n$\" argument index out of range"
    };
    int formatLen;
    Jim_Obj *resultPtr;

    /* A single buffer is used to store numeric fields (with sprintf())
     * This buffer is allocated/reallocated as necessary
     */
    char *num_buffer = NULL;
    int num_buffer_size = 0;

    span = format = Jim_GetString(fmtObjPtr, &formatLen);
    formatEnd = format + formatLen;
    resultPtr = Jim_NewStringObj(interp, "", 0);

    while (format != formatEnd) {
	char *end;
	int gotMinus, sawFlag;
	int gotPrecision, useShort;
	long width, precision;
	int newXpg;
	int ch;
	int step;
	int doubleType;
	char pad = ' ';
	char spec[2*JIM_INTEGER_SPACE + 12];
	char *p;

	int formatted_chars;
	int formatted_bytes;
	const char *formatted_buf;

	step = utf8_tounicode(format, &ch);
	format += step;
	if (ch != '%') {
	    numBytes += step;
	    continue;
	}
	if (numBytes) {
	    Jim_AppendString(interp, resultPtr, span, numBytes);
	    numBytes = 0;
	}

	/*
	 * Saw a % : process the format specifier.
	 *
	 * Step 0. Handle special case of escaped format marker (i.e., %%).
	 */

	step = utf8_tounicode(format, &ch);
	if (ch == '%') {
	    span = format;
	    numBytes = step;
	    format += step;
	    continue;
	}

	/*
	 * Step 1. XPG3 position specifier
	 */

	newXpg = 0;
	if (isdigit(ch)) {
	    int position = strtoul(format, &end, 10);
	    if (*end == '$') {
		newXpg = 1;
		objIndex = position - 1;
		format = end + 1;
		step = utf8_tounicode(format, &ch);
	    }
	}
	if (newXpg) {
	    if (gotSequential) {
		msg = mixedXPG;
		goto errorMsg;
	    }
	    gotXpg = 1;
	} else {
	    if (gotXpg) {
		msg = mixedXPG;
		goto errorMsg;
	    }
	    gotSequential = 1;
	}
	if ((objIndex < 0) || (objIndex >= objc)) {
	    msg = badIndex[gotXpg];
	    goto errorMsg;
	}

	/*
	 * Step 2. Set of flags. Also build up the sprintf spec.
	 */
	p = spec;
	*p++ = '%';

	gotMinus = 0;
	sawFlag = 1;
	do {
	    switch (ch) {
	    case '-':
		gotMinus = 1;
		break;
	    case '0':
		pad = ch;
		break;
	    case ' ':
	    case '+':
	    case '#':
		break;
	    default:
		sawFlag = 0;
		continue;
	    }
	    *p++ = ch;
	    format += step;
	    step = utf8_tounicode(format, &ch);
	} while (sawFlag);

	/*
	 * Step 3. Minimum field width.
	 */

	width = 0;
	if (isdigit(ch)) {
	    width = strtoul(format, &end, 10);
	    format = end;
	    step = utf8_tounicode(format, &ch);
	} else if (ch == '*') {
	    if (objIndex >= objc - 1) {
		msg = badIndex[gotXpg];
		goto errorMsg;
	    }
	    if (Jim_GetLong(interp, objv[objIndex], &width) != JIM_OK) {
		goto error;
	    }
	    if (width < 0) {
		width = -width;
		if (!gotMinus) {
		    *p++ = '-';
		    gotMinus = 1;
		}
	    }
	    objIndex++;
	    format += step;
	    step = utf8_tounicode(format, &ch);
	}

	/*
	 * Step 4. Precision.
	 */

	gotPrecision = precision = 0;
	if (ch == '.') {
	    gotPrecision = 1;
	    format += step;
	    step = utf8_tounicode(format, &ch);
	}
	if (isdigit(ch)) {
	    precision = strtoul(format, &end, 10);
	    format = end;
	    step = utf8_tounicode(format, &ch);
	} else if (ch == '*') {
	    if (objIndex >= objc - 1) {
		msg = badIndex[gotXpg];
		goto errorMsg;
	    }
	    if (Jim_GetLong(interp, objv[objIndex], &precision) != JIM_OK) {
		goto error;
	    }

	    /*
	     * TODO: Check this truncation logic.
	     */

	    if (precision < 0) {
		precision = 0;
	    }
	    objIndex++;
	    format += step;
	    step = utf8_tounicode(format, &ch);
	}

	/*
	 * Step 5. Length modifier.
	 */

	useShort = 0;
	if (ch == 'h') {
	    useShort = 1;
	    format += step;
	    step = utf8_tounicode(format, &ch);
	} else if (ch == 'l') {
	    /* Just for compatibility. All non-short integers are wide. */
	    format += step;
	    step = utf8_tounicode(format, &ch);
	    if (ch == 'l') {
		format += step;
		step = utf8_tounicode(format, &ch);
	    }
	}

	format += step;
	span = format;

	/*
	 * Step 6. The actual conversion character.
	 */

	if (ch == 'i') {
	    ch = 'd';
	}

	doubleType = 0;

	/* Each valid conversion will set:
	 * formatted_buf   - the result to be added
	 * formatted_chars - the length of formatted_buf in characters 
	 * formatted_bytes - the length of formatted_buf in bytes
	 */
	switch (ch) {
	case '\0':
	    msg = "format string ended in middle of field specifier";
	    goto errorMsg;
	case 's': {
	    formatted_buf = Jim_GetString(objv[objIndex], &formatted_bytes);
	    formatted_chars = Jim_Utf8Length(interp, objv[objIndex]);
	    if (gotPrecision && (precision < formatted_chars)) {
		/* Need to build a (null terminated) truncated string */
		formatted_chars = precision;
		formatted_bytes = utf8_index(formatted_buf, precision);
	    }
	    break;
	}
	case 'c': {
	    jim_wide code;

	    if (Jim_GetWide(interp, objv[objIndex], &code) != JIM_OK) {
		goto error;
	    }
	    /* Just store the value in the 'spec' buffer */
	    formatted_bytes = utf8_fromunicode(spec, code);
	    formatted_buf = spec;
	    formatted_chars = 1;
	    break;
	}

	case 'e':
	case 'E':
	case 'f':
	case 'g':
	case 'G':
	    doubleType = 1;
	    /* fall through */
	case 'd':
	case 'u':
	case 'o':
	case 'x':
	case 'X': {
	    jim_wide w;
	    double d;
	    int length;

	    /* Fill in the width and precision */
	    if (width) {
		p += sprintf(p, "%ld", width);
	    }
	    if (gotPrecision) {
		p += sprintf(p, ".%ld", precision);
	    }

	    /* Now the modifier, and get the actual value here */
	    if (doubleType) {
		if (Jim_GetDouble(interp, objv[objIndex], &d) != JIM_OK) {
		    goto error;
		}
		length = MAX_FLOAT_WIDTH;
	    }
	    else {
		if (Jim_GetWide(interp, objv[objIndex], &w) != JIM_OK) {
		    goto error;
		}
		length = JIM_INTEGER_SPACE;
		if (useShort) {
		    *p++ = 'h';
		    if (ch == 'd') {
			w = (short)w;
		    }
		    else {
			w = (unsigned short)w;
		    }
		}
		else {
		    *p++ = 'l';
#ifdef HAVE_LONG_LONG
		    if (sizeof(long long) == sizeof(jim_wide)) {
			*p++ = 'l';
		    }
#endif
		}
	    }

	    *p++ = (char) ch;
	    *p = '\0';

	    /* Adjust length for width and precision */
	    if (width > length) {
		length = width;
	    }
	    if (gotPrecision) {
		length += precision;
	    }

	    /* Increase the size of the buffer if needed */
	    if (num_buffer_size < length + 1) {
		num_buffer_size = length + 1;
		num_buffer = Jim_Realloc(num_buffer, num_buffer_size);
	    }

	    if (doubleType) {
		snprintf(num_buffer, length + 1, spec, d);
	    }
	    else {
		formatted_bytes = snprintf(num_buffer, length + 1, spec, w);
	    }
	    formatted_chars = formatted_bytes = strlen(num_buffer);
	    formatted_buf = num_buffer;
	    break;
	}

	default: {
	    /* Just reuse the 'spec' buffer */
	    spec[0] = ch;
	    spec[1] = '\0';
	    Jim_SetResultFormatted(interp, "bad field specifier \"%s\"", spec);
	    goto error;
	}
	}

	if (!gotMinus) {
	    while (formatted_chars < width) {
		Jim_AppendString(interp, resultPtr, &pad, 1);
		formatted_chars++;
	    }
	}

	Jim_AppendString(interp, resultPtr, formatted_buf, formatted_bytes);

	while (formatted_chars < width) {
	    Jim_AppendString(interp, resultPtr, &pad, 1);
	    formatted_chars++;
	}

	objIndex += gotSequential;
    }
    if (numBytes) {
	Jim_AppendString(interp, resultPtr, span, numBytes);
    }

    Jim_Free(num_buffer);
    return resultPtr;

  errorMsg:
    Jim_SetResultString(interp, msg, -1);
  error:
    Jim_FreeNewObj(interp, resultPtr);
    Jim_Free(num_buffer);
    return NULL;
}
/*
 * regcomp and regexec -- regsub and regerror are elsewhere
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 *** THIS IS AN ALTERED VERSION.  It was altered by John Gilmore,
 *** hoptoad!gnu, on 27 Dec 1986, to add \n as an alternative to |
 *** to assist in implementing egrep.
 *** THIS IS AN ALTERED VERSION.  It was altered by John Gilmore,
 *** hoptoad!gnu, on 27 Dec 1986, to add \< and \> for word-matching
 *** as in BSD grep and ex.
 *** THIS IS AN ALTERED VERSION.  It was altered by John Gilmore,
 *** hoptoad!gnu, on 28 Dec 1986, to optimize characters quoted with \.
 *** THIS IS AN ALTERED VERSION.  It was altered by James A. Woods,
 *** ames!jaw, on 19 June 1987, to quash a regcomp() redundancy.
 *** THIS IS AN ALTERED VERSION.  It was altered by Christopher Seiwald
 *** seiwald@vix.com, on 28 August 1993, for use in jam.  Regmagic.h
 *** was moved into regexp.h, and the include of regexp.h now uses "'s
 *** to avoid conflicting with the system regexp.h.  Const, bless its
 *** soul, was removed so it can compile everywhere.  The declaration
 *** of strchr() was in conflict on AIX, so it was removed (as it is
 *** happily defined in string.h).
 *** THIS IS AN ALTERED VERSION.  It was altered by Christopher Seiwald
 *** seiwald@perforce.com, on 20 January 2000, to use function prototypes.
 *** THIS IS AN ALTERED VERSION.  It was altered by Christopher Seiwald
 *** seiwald@perforce.com, on 05 November 2002, to const string literals.
 *
 *   THIS IS AN ALTERED VERSION.  It was altered by Steve Bennett <steveb@workware.net.au>
 *   on 16 October 2010, to remove static state and add better Tcl ARE compatibility.
 *   This includes counted repetitions, UTF-8 support, character classes,
 *   shorthand character classes, increased number of parentheses to 100,
 *   backslash escape sequences. It also removes \n as an alternative to |.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 */
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


#if !defined(HAVE_REGCOMP) || defined(JIM_REGEXP)

/*
 * Structure for regexp "program".  This is essentially a linear encoding
 * of a nondeterministic finite-state machine (aka syntax charts or
 * "railroad normal form" in parsing technology).  Each node is an opcode
 * plus a "next" pointer, possibly plus an operand.  "Next" pointers of
 * all nodes except BRANCH implement concatenation; a "next" pointer with
 * a BRANCH on both ends of it is connecting two alternatives.  (Here we
 * have one of the subtle syntax dependencies:  an individual BRANCH (as
 * opposed to a collection of them) is never concatenated with anything
 * because of operator precedence.)  The operand of some types of node is
 * a literal string; for others, it is a node leading into a sub-FSM.  In
 * particular, the operand of a BRANCH node is the first node of the branch.
 * (NB this is *not* a tree structure:  the tail of the branch connects
 * to the thing following the set of BRANCHes.)  The opcodes are:
 */

/* This *MUST* be less than (255-20)/2=117 */
#define REG_MAX_PAREN 100

/* definition	number	opnd?	meaning */
#define	END	0	/* no	End of program. */
#define	BOL	1	/* no	Match "" at beginning of line. */
#define	EOL	2	/* no	Match "" at end of line. */
#define	ANY	3	/* no	Match any one character. */
#define	ANYOF	4	/* str	Match any character in this string. */
#define	ANYBUT	5	/* str	Match any character not in this string. */
#define	BRANCH	6	/* node	Match this alternative, or the next... */
#define	BACK	7	/* no	Match "", "next" ptr points backward. */
#define	EXACTLY	8	/* str	Match this string. */
#define	NOTHING	9	/* no	Match empty string. */
#define	REP		10	/* max,min	Match this (simple) thing [min,max] times. */
#define	REPMIN	11	/* max,min	Match this (simple) thing [min,max] times, mininal match. */
#define	REPX	12	/* max,min	Match this (complex) thing [min,max] times. */
#define	REPXMIN	13	/* max,min	Match this (complex) thing [min,max] times, minimal match. */

#define	WORDA	15	/* no	Match "" at wordchar, where prev is nonword */
#define	WORDZ	16	/* no	Match "" at nonwordchar, where prev is word */
#define	OPEN	20	/* no	Mark this point in input as start of #n. */
			/*	OPEN+1 is number 1, etc. */
#define	CLOSE	(OPEN+REG_MAX_PAREN)	/* no	Analogous to OPEN. */
#define	CLOSE_END	(CLOSE+REG_MAX_PAREN)

/*
 * The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define	REG_MAGIC	0xFADED00D

/*
 * Opcode notes:
 *
 * BRANCH	The set of branches constituting a single choice are hooked
 *		together with their "next" pointers, since precedence prevents
 *		anything being concatenated to any individual branch.  The
 *		"next" pointer of the last BRANCH in a choice points to the
 *		thing following the whole choice.  This is also where the
 *		final "next" pointer of each individual branch points; each
 *		branch starts with the operand node of a BRANCH node.
 *
 * BACK		Normal "next" pointers all implicitly point forward; BACK
 *		exists to make loop structures possible.
 *
 * STAR,PLUS	'?', and complex '*' and '+', are implemented as circular
 *		BRANCH structures using BACK.  Simple cases (one character
 *		per match) are implemented with STAR and PLUS for speed
 *		and to minimize recursive plunges.
 *
 * OPEN,CLOSE	...are numbered at compile time.
 */

/*
 * A node is one char of opcode followed by two chars of "next" pointer.
 * "Next" pointers are stored as two 8-bit pieces, high order first.  The
 * value is a positive offset from the opcode of the node containing it.
 * An operand, if any, simply follows the node.  (Note that much of the
 * code generation knows about this implicit relationship.)
 *
 * Using two bytes for the "next" pointer is vast overkill for most things,
 * but allows patterns to get big without disasters.
 */
#define	OP(preg, p)	(preg->program[p])
#define	NEXT(preg, p)	(preg->program[p + 1])
#define	OPERAND(p)	((p) + 2)

/*
 * See regmagic.h for one further detail of program structure.
 */


/*
 * Utility definitions.
 */

#define	FAIL(R,M)	{ (R)->err = (M); return (M); }
#define	ISMULT(c)	((c) == '*' || (c) == '+' || (c) == '?' || (c) == '{')
#define	META	"^$.[()|?{+*"

/*
 * Flags to be passed up and down.
 */
#define	HASWIDTH	01	/* Known never to match null string. */
#define	SIMPLE		02	/* Simple enough to be STAR/PLUS operand. */
#define	SPSTART		04	/* Starts with * or +. */
#define	WORST		0	/* Worst case. */

#define MAX_REP_COUNT 1000000

/*
 * Forward declarations for regcomp()'s friends.
 */
static int reg(regex_t *preg, int paren /* Parenthesized? */, int *flagp );
static int regpiece(regex_t *preg, int *flagp );
static int regbranch(regex_t *preg, int *flagp );
static int regatom(regex_t *preg, int *flagp );
static int regnode(regex_t *preg, int op );
static int regnext(regex_t *preg, int p );
static void regc(regex_t *preg, int b );
static int reginsert(regex_t *preg, int op, int size, int opnd );
static void regtail_(regex_t *preg, int p, int val, int line );
static void regoptail(regex_t *preg, int p, int val );
#define regtail(PREG, P, VAL) regtail_(PREG, P, VAL, __LINE__)

static int reg_range_find(const int *string, int c);
static const char *str_find(const char *string, int c, int nocase);
static int prefix_cmp(const int *prog, int proglen, const char *string, int nocase);

/*#define DEBUG*/
#ifdef DEBUG
int regnarrate = 0;
static void regdump(regex_t *preg);
static const char *regprop( int op );
#endif


/**
 * Returns the length of the null-terminated integer sequence.
 */
static int str_int_len(const int *seq)
{
	int n = 0;
	while (*seq++) {
		n++;
	}
	return n;
}

/*
 - regcomp - compile a regular expression into internal code
 *
 * We can't allocate space until we know how big the compiled form will be,
 * but we can't compile it (and thus know how big it is) until we've got a
 * place to put the code.  So we cheat:  we compile it twice, once with code
 * generation turned off and size counting turned on, and once "for real".
 * This also means that we don't allocate space until we are sure that the
 * thing really will compile successfully, and we never have to move the
 * code and thus invalidate pointers into it.  (Note that it has to be in
 * one piece because free() must be able to free it all.)
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.
 */
int regcomp(regex_t *preg, const char *exp, int cflags)
{
	int scan;
	int longest;
	unsigned len;
	int flags;

#ifdef DEBUG
	fprintf(stderr, "Compiling: '%s'\n", exp);
#endif
	memset(preg, 0, sizeof(*preg));

	if (exp == NULL)
		FAIL(preg, REG_ERR_NULL_ARGUMENT);

	/* First pass: determine size, legality. */
	preg->cflags = cflags;
	preg->regparse = exp;
	/* XXX: For now, start unallocated */
	preg->program = NULL;
	preg->proglen = 0;

#if 1
	/* Allocate space. */
	preg->proglen = (strlen(exp) + 1) * 5;
	preg->program = malloc(preg->proglen * sizeof(int));
	if (preg->program == NULL)
		FAIL(preg, REG_ERR_NOMEM);
#endif

	/* Note that since we store a magic value as the first item in the program,
	 * program offsets will never be 0
	 */
	regc(preg, REG_MAGIC);
	if (reg(preg, 0, &flags) == 0) {
		return preg->err;
	}

	/* Small enough for pointer-storage convention? */
	if (preg->re_nsub >= REG_MAX_PAREN)		/* Probably could be 65535L. */
		FAIL(preg,REG_ERR_TOO_BIG);

	/* Dig out information for optimizations. */
	preg->regstart = 0;	/* Worst-case defaults. */
	preg->reganch = 0;
	preg->regmust = 0;
	preg->regmlen = 0;
	scan = 1;			/* First BRANCH. */
	if (OP(preg, regnext(preg, scan)) == END) {		/* Only one top-level choice. */
		scan = OPERAND(scan);

		/* Starting-point info. */
		if (OP(preg, scan) == EXACTLY) {
			preg->regstart = preg->program[OPERAND(scan)];
		}
		else if (OP(preg, scan) == BOL)
			preg->reganch++;

		/*
		 * If there's something expensive in the r.e., find the
		 * longest literal string that must appear and make it the
		 * regmust.  Resolve ties in favor of later strings, since
		 * the regstart check works with the beginning of the r.e.
		 * and avoiding duplication strengthens checking.  Not a
		 * strong reason, but sufficient in the absence of others.
		 */
		if (flags&SPSTART) {
			longest = 0;
			len = 0;
			for (; scan != 0; scan = regnext(preg, scan)) {
				if (OP(preg, scan) == EXACTLY) {
					int plen = str_int_len(preg->program + OPERAND(scan));
					if (plen >= len) {
						longest = OPERAND(scan);
						len = plen;
					}
				}
			}
			preg->regmust = longest;
			preg->regmlen = len;
		}
	}

#ifdef DEBUG
	regdump(preg);
#endif

	return 0;
}

/*
 - reg - regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
static int reg(regex_t *preg, int paren /* Parenthesized? */, int *flagp )
{
	int ret;
	int br;
	int ender;
	int parno = 0;
	int flags;

	*flagp = HASWIDTH;	/* Tentatively. */

	/* Make an OPEN node, if parenthesized. */
	if (paren) {
		parno = ++preg->re_nsub;
		ret = regnode(preg, OPEN+parno);
	} else
		ret = 0;

	/* Pick up the branches, linking them together. */
	br = regbranch(preg, &flags);
	if (br == 0)
		return 0;
	if (ret != 0)
		regtail(preg, ret, br);	/* OPEN -> first. */
	else
		ret = br;
	if (!(flags&HASWIDTH))
		*flagp &= ~HASWIDTH;
	*flagp |= flags&SPSTART;
	while (*preg->regparse == '|') {
		preg->regparse++;
		br = regbranch(preg, &flags);
		if (br == 0)
			return 0;
		regtail(preg, ret, br);	/* BRANCH -> BRANCH. */
		if (!(flags&HASWIDTH))
			*flagp &= ~HASWIDTH;
		*flagp |= flags&SPSTART;
	}

	/* Make a closing node, and hook it on the end. */
	ender = regnode(preg, (paren) ? CLOSE+parno : END);
	regtail(preg, ret, ender);

	/* Hook the tails of the branches to the closing node. */
	for (br = ret; br != 0; br = regnext(preg, br))
		regoptail(preg, br, ender);

	/* Check for proper termination. */
	if (paren && *preg->regparse++ != ')') {
		preg->err = REG_ERR_UNMATCHED_PAREN;
		return 0;
	} else if (!paren && *preg->regparse != '\0') {
		if (*preg->regparse == ')') {
			preg->err = REG_ERR_UNMATCHED_PAREN;
			return 0;
		} else {
			preg->err = REG_ERR_JUNK_ON_END;
			return 0;
		}
	}

	return(ret);
}

/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
static int regbranch(regex_t *preg, int *flagp )
{
	int ret;
	int chain;
	int latest;
	int flags;

	*flagp = WORST;		/* Tentatively. */

	ret = regnode(preg, BRANCH);
	chain = 0;
	while (*preg->regparse != '\0' && *preg->regparse != ')' &&
	       *preg->regparse != '|') {
		latest = regpiece(preg, &flags);
		if (latest == 0)
			return 0;
		*flagp |= flags&HASWIDTH;
		if (chain == 0) {/* First piece. */
			*flagp |= flags&SPSTART;
		}
		else {
			regtail(preg, chain, latest);
		}
		chain = latest;
	}
	if (chain == 0)	/* Loop ran zero times. */
		(void) regnode(preg, NOTHING);

	return(ret);
}

/*
 - regpiece - something followed by possible [*+?]
 *
 * Note that the branching code sequences used for ? and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
static int regpiece(regex_t *preg, int *flagp)
{
	int ret;
	char op;
	int next;
	int flags;
	int chain = 0;
	int min;
	int max;

	ret = regatom(preg, &flags);
	if (ret == 0)
		return 0;

	op = *preg->regparse;
	if (!ISMULT(op)) {
		*flagp = flags;
		return(ret);
	}

	if (!(flags&HASWIDTH) && op != '?') {
		preg->err = REG_ERR_OPERAND_COULD_BE_EMPTY;
		return 0;
	}

	/* Handle braces (counted repetition) by expansion */
	if (op == '{') {
		char *end;

		min = strtoul(preg->regparse + 1, &end, 10);
		if (end == preg->regparse + 1) {
			preg->err = REG_ERR_BAD_COUNT;
			return 0;
		}
		if (*end == '}') {
			max = min;
		}
		else {
			preg->regparse = end;
			max = strtoul(preg->regparse + 1, &end, 10);
			if (*end != '}') {
				preg->err = REG_ERR_UNMATCHED_BRACES;
				return 0;
			}
		}
		if (end == preg->regparse + 1) {
			max = MAX_REP_COUNT;
		}
		else if (max < min || max >= 100) {
			preg->err = REG_ERR_BAD_COUNT;
			return 0;
		}
		if (min >= 100) {
			preg->err = REG_ERR_BAD_COUNT;
			return 0;
		}

		preg->regparse = strchr(preg->regparse, '}');
	}
	else {
		min = (op == '+');
		max = (op == '?' ? 1 : MAX_REP_COUNT);
	}

	if (preg->regparse[1] == '?') {
		preg->regparse++;
		next = reginsert(preg, flags & SIMPLE ? REPMIN : REPXMIN, 5, ret);
	}
	else {
		next = reginsert(preg, flags & SIMPLE ? REP: REPX, 5, ret);
	}
	preg->program[ret + 2] = max;
	preg->program[ret + 3] = min;
	preg->program[ret + 4] = 0;

	*flagp = (min) ? (WORST|HASWIDTH) : (WORST|SPSTART);

	if (!(flags & SIMPLE)) {
		int back = regnode(preg, BACK);
		regtail(preg, back, ret);
		regtail(preg, next, back);
	}

	preg->regparse++;
	if (ISMULT(*preg->regparse)) {
		preg->err = REG_ERR_NESTED_COUNT;
		return 0;
	}

	return chain ? chain : ret;
}

/**
 * Add all characters in the inclusive range between lower and upper.
 * 
 * Handles a swapped range (upper < lower).
 */
static void reg_addrange(regex_t *preg, int lower, int upper)
{
	if (lower > upper) {
		reg_addrange(preg, upper, lower);
	}
	/* Add a range as length, start */
	regc(preg, upper - lower + 1);
	regc(preg, lower);
}

/**
 * Add a null-terminated literal string as a set of ranges.
 */
static void reg_addrange_str(regex_t *preg, const char *str)
{
	while (*str) {
		reg_addrange(preg, *str, *str);
		str++;
	}
}

/**
 * Extracts the next unicode char from utf8.
 * 
 * If 'upper' is set, converts the char to uppercase.
 */
static int reg_utf8_tounicode_case(const char *s, int *uc, int upper)
{
	int l = utf8_tounicode(s, uc);
	if (upper) {
		*uc = utf8_upper(*uc);
	}
	return l;
}

/**
 * Converts a hex digit to decimal.
 * 
 * Returns -1 for an invalid hex digit.
 */
static int hexdigitval(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/**
 * Parses up to 'n' hex digits at 's' and stores the result in *uc.
 *
 * Returns the number of hex digits parsed.
 * If there are no hex digits, returns 0 and stores nothing.
 */
static int parse_hex(const char *s, int n, int *uc)
{
	int val = 0;
	int k;

	for (k = 0; k < n; k++) {
		int c = hexdigitval(*s++);
		if (c == -1) {
			break;
		}
		val = (val << 4) | c;
	}
	if (k) {
		*uc = val;
	}
	return k;
}

/**
 * Call for chars after a backlash to decode the escape sequence.
 * 
 * Stores the result in *ch.
 *
 * Returns the number of bytes consumed.
 */
static int reg_decode_escape(const char *s, int *ch)
{
	int n;
	const char *s0 = s;

	*ch = *s++;

	switch (*ch) {
		case 'b': *ch = '\b'; break;
		case 'e': *ch = 27; break;
		case 'f': *ch = '\f'; break;
		case 'n': *ch = '\n'; break;
		case 'r': *ch = '\r'; break;
		case 't': *ch = '\t'; break;
		case 'v': *ch = '\v'; break;
		case 'u':
			if ((n = parse_hex(s, 4, ch)) > 0) {
				s += n;
			}
			break;
		case 'x':
			if ((n = parse_hex(s, 2, ch)) > 0) {
				s += n;
			}
			break;
		case '\0':
			s--;
			*ch = '\\';
			break;
	}
	return s - s0;
}

/*
 - regatom - the lowest level
 *
 * Optimization:  gobbles an entire sequence of ordinary characters so that
 * it can turn them into a single node, which is smaller to store and
 * faster to run.  Backslashed characters are exceptions, each becoming a
 * separate node; the code is simpler that way and it's not worth fixing.
 */
static int regatom(regex_t *preg, int *flagp)
{
	int ret;
	int flags;
	int nocase = (preg->cflags & REG_ICASE);

	int ch;
	int n = reg_utf8_tounicode_case(preg->regparse, &ch, nocase);

	*flagp = WORST;		/* Tentatively. */

	preg->regparse += n;
	switch (ch) {
	/* FIXME: these chars only have meaning at beg/end of pat? */
	case '^':
		ret = regnode(preg, BOL);
		break;
	case '$':
		ret = regnode(preg, EOL);
		break;
	case '.':
		ret = regnode(preg, ANY);
		*flagp |= HASWIDTH|SIMPLE;
		break;
	case '[': {
			const char *pattern = preg->regparse;

			if (*pattern == '^') {	/* Complement of range. */
				ret = regnode(preg, ANYBUT);
				pattern++;
			} else
				ret = regnode(preg, ANYOF);

			/* Special case. If the first char is ']' or '-', it is part of the set */
			if (*pattern == ']' || *pattern == '-') {
				reg_addrange(preg, *pattern, *pattern);
				pattern++;
			}

			while (*pattern && *pattern != ']') {
				/* Is this a range? a-z */
				int start;
				int end;

				pattern += reg_utf8_tounicode_case(pattern, &start, nocase);
				if (start == '\\') {
					pattern += reg_decode_escape(pattern, &start);
					if (start == 0) {
						preg->err = REG_ERR_NULL_CHAR;
						return 0;
					}
				}
				if (pattern[0] == '-' && pattern[1]) {
					/* skip '-' */
					pattern += utf8_tounicode(pattern, &end);
					pattern += reg_utf8_tounicode_case(pattern, &end, nocase);
					if (end == '\\') {
						pattern += reg_decode_escape(pattern, &end);
						if (end == 0) {
							preg->err = REG_ERR_NULL_CHAR;
							return 0;
						}
					}

					reg_addrange(preg, start, end);
					continue;
				}
				if (start == '[') {
					if (strncmp(pattern, ":alpha:]", 8) == 0) {
						if ((preg->cflags & REG_ICASE) == 0) {
							reg_addrange(preg, 'a', 'z');
						}
						reg_addrange(preg, 'A', 'Z');
						pattern += 8;
						continue;
					}
					if (strncmp(pattern, ":alnum:]", 8) == 0) {
						if ((preg->cflags & REG_ICASE) == 0) {
							reg_addrange(preg, 'a', 'z');
						}
						reg_addrange(preg, 'A', 'Z');
						reg_addrange(preg, '0', '9');
						pattern += 8;
						continue;
					}
					if (strncmp(pattern, ":space:]", 8) == 0) {
						reg_addrange_str(preg, " \t\r\n\f\v");
						pattern += 8;
						continue;
					}
				}
				/* Not a range, so just add the char */
				reg_addrange(preg, start, start);
			}
			regc(preg, '\0');

			if (*pattern) {
				pattern++;
			}
			preg->regparse = pattern;

			*flagp |= HASWIDTH|SIMPLE;
		}
		break;
	case '(':
		ret = reg(preg, 1, &flags);
		if (ret == 0)
			return 0;
		*flagp |= flags&(HASWIDTH|SPSTART);
		break;
	case '\0':
	case '|':
	case ')':
		preg->err = REG_ERR_INTERNAL;
		return 0;	/* Supposed to be caught earlier. */
	case '?':
	case '+':
	case '*':
	case '{':
		preg->err = REG_ERR_COUNT_FOLLOWS_NOTHING;
		return 0;
	case '\\':
		switch (*preg->regparse++) {
		case '\0':
			preg->err = REG_ERR_TRAILING_BACKSLASH;
			return 0;
		case '<':
		case 'm':
			ret = regnode(preg, WORDA);
			break;
		case '>':
		case 'M':
			ret = regnode(preg, WORDZ);
			break;
		case 'd':
			ret = regnode(preg, ANYOF);
			reg_addrange(preg, '0', '9');
			regc(preg, '\0');
			*flagp |= HASWIDTH|SIMPLE;
			break;
		case 'w':
			ret = regnode(preg, ANYOF);
			if ((preg->cflags & REG_ICASE) == 0) {
				reg_addrange(preg, 'a', 'z');
			}
			reg_addrange(preg, 'A', 'Z');
			reg_addrange(preg, '0', '9');
			reg_addrange(preg, '_', '_');
			regc(preg, '\0');
			*flagp |= HASWIDTH|SIMPLE;
			break;
		case 's':
			ret = regnode(preg, ANYOF);
			reg_addrange_str(preg," \t\r\n\f\v");
			regc(preg, '\0');
			*flagp |= HASWIDTH|SIMPLE;
			break;
		/* FIXME: Someday handle \1, \2, ... */
		default:
			/* Handle general quoted chars in exact-match routine */
			/* Back up to include the backslash */
			preg->regparse--;
			goto de_fault;
		}
		break;
	de_fault:
	default: {
			/*
			 * Encode a string of characters to be matched exactly.
			 */
			int added = 0;

			/* Back up to pick up the first char of interest */
			preg->regparse -= n;

			ret = regnode(preg, EXACTLY);

			/* Note that a META operator such as ? or * consumes the
			 * preceding char.
			 * Thus we must be careful to look ahead by 2 and add the
			 * last char as it's own EXACTLY if necessary
			 */

			/* Until end of string or a META char is reached */
			while (*preg->regparse && strchr(META, *preg->regparse) == NULL) {
				n = reg_utf8_tounicode_case(preg->regparse, &ch, (preg->cflags & REG_ICASE));
				if (ch == '\\' && preg->regparse[n]) {
					/* Non-trailing backslash.
					 * Is this a special escape, or a regular escape?
					 */
					if (strchr("<>mMwds", preg->regparse[n])) {
						/* A special escape. All done with EXACTLY */
						break;
					}
					/* Decode it. Note that we add the length for the escape
					 * sequence to the length for the backlash so we can skip
					 * the entire sequence, or not as required.
					 */
					n += reg_decode_escape(preg->regparse + n, &ch);
					if (ch == 0) {
						preg->err = REG_ERR_NULL_CHAR;
						return 0;
					}
				}

				/* Now we have one char 'ch' of length 'n'.
				 * Check to see if the following char is a MULT
				 */

				if (ISMULT(preg->regparse[n])) {
					/* Yes. But do we already have some EXACTLY chars? */
					if (added) {
						/* Yes, so return what we have and pick up the current char next time around */
						break;
					}
					/* No, so add this single char and finish */
					regc(preg, ch);
					added++;
					preg->regparse += n;
					break;
				}

				/* No, so just add this char normally */
				regc(preg, ch);
				added++;
				preg->regparse += n;
			}
			regc(preg, '\0');

			*flagp |= HASWIDTH;
			if (added == 1)
				*flagp |= SIMPLE;
			break;
		}
		break;
	}

	return(ret);
}

static void reg_grow(regex_t *preg, int n)
{
	if (preg->p + n >= preg->proglen) {
		preg->proglen = (preg->p + n) * 2;
		preg->program = realloc(preg->program, preg->proglen * sizeof(int));
	}
}

/*
 - regnode - emit a node
 */
/* Location. */
static int regnode(regex_t *preg, int op)
{
	reg_grow(preg, 2);

	preg->program[preg->p++] = op;
	preg->program[preg->p++] = 0;

	/* Return the start of the node */
	return preg->p - 2;
}

/*
 - regc - emit (if appropriate) a byte of code
 */
static void regc(regex_t *preg, int b )
{
	reg_grow(preg, 1);
	preg->program[preg->p++] = b;
}

/*
 - reginsert - insert an operator in front of already-emitted operand
 *
 * Means relocating the operand.
 * Returns the new location of the original operand.
 */
static int reginsert(regex_t *preg, int op, int size, int opnd )
{
	reg_grow(preg, size);

	/* Move everything from opnd up */
	memmove(preg->program + opnd + size, preg->program + opnd, sizeof(int) * (preg->p - opnd));
	/* Zero out the new space */
	memset(preg->program + opnd, 0, sizeof(int) * size);

	preg->program[opnd] = op;

	preg->p += size;

	return opnd + size;
}

/*
 - regtail - set the next-pointer at the end of a node chain
 */
static void regtail_(regex_t *preg, int p, int val, int line )
{
	int scan;
	int temp;
	int offset;

	/* Find last node. */
	scan = p;
	for (;;) {
		temp = regnext(preg, scan);
		if (temp == 0)
			break;
		scan = temp;
	}

	if (OP(preg, scan) == BACK)
		offset = scan - val;
	else
		offset = val - scan;

	preg->program[scan + 1] = offset;
}

/*
 - regoptail - regtail on operand of first argument; nop if operandless
 */

static void regoptail(regex_t *preg, int p, int val )
{
	/* "Operandless" and "op != BRANCH" are synonymous in practice. */
	if (p != 0 && OP(preg, p) == BRANCH) {
		regtail(preg, OPERAND(p), val);
	}
}

/*
 * regexec and friends
 */

/*
 * Forwards.
 */
static int regtry(regex_t *preg, const char *string );
static int regmatch(regex_t *preg, int prog);
static int regrepeat(regex_t *preg, int p, int max);

/*
 - regexec - match a regexp against a string
 */
int regexec(regex_t  *preg,  const  char *string, size_t nmatch, regmatch_t pmatch[], int eflags)
{
	const char *s;
	int scan;

	/* Be paranoid... */
	if (preg == NULL || preg->program == NULL || string == NULL) {
		return REG_ERR_NULL_ARGUMENT;
	}

	/* Check validity of program. */
	if (*preg->program != REG_MAGIC) {
		return REG_ERR_CORRUPTED;
	}

#ifdef DEBUG
	fprintf(stderr, "regexec: %s\n", string);
	regdump(preg);
#endif

	preg->eflags = eflags;
	preg->pmatch = pmatch;
	preg->nmatch = nmatch;
	preg->start = string;	/* All offsets are computed from here */

	/* Must clear out the embedded repeat counts */
	for (scan = OPERAND(1); scan != 0; scan = regnext(preg, scan)) {
		switch (OP(preg, scan)) {
		case REP:
		case REPMIN:
		case REPX:
		case REPXMIN:
			preg->program[scan + 4] = 0;
			break;
		}
	}

	/* If there is a "must appear" string, look for it. */
	if (preg->regmust != 0) {
		s = string;
		while ((s = str_find(s, preg->program[preg->regmust], preg->cflags & REG_ICASE)) != NULL) {
			if (prefix_cmp(preg->program + preg->regmust, preg->regmlen, s, preg->cflags & REG_ICASE) >= 0) {
				break;
			}
			s++;
		}
		if (s == NULL)	/* Not present. */
			return REG_NOMATCH;
	}

	/* Mark beginning of line for ^ . */
	preg->regbol = string;

	/* Simplest case:  anchored match need be tried only once (maybe per line). */
	if (preg->reganch) {
		if (eflags & REG_NOTBOL) {
			/* This is an anchored search, but not an BOL, so possibly skip to the next line */
			goto nextline;
		}
		while (1) {
			int ret = regtry(preg, string);
			if (ret) {
				return REG_NOERROR;
			}
			if (*string) {
nextline:
				if (preg->cflags & REG_NEWLINE) {
					/* Try the next anchor? */
					string = strchr(string, '\n');
					if (string) {
						preg->regbol = ++string;
						continue;
					}
				}
			}
			return REG_NOMATCH;
		}
	}

	/* Messy cases:  unanchored match. */
	s = string;
	if (preg->regstart != '\0') {
		/* We know what char it must start with. */
		while ((s = str_find(s, preg->regstart, preg->cflags & REG_ICASE)) != NULL) {
			if (regtry(preg, s))
				return REG_NOERROR;
			s++;
		}
	}
	else
		/* We don't -- general case. */
		while (1) {
			if (regtry(preg, s))
				return REG_NOERROR;
			if (*s == '\0') {
				break;
			}
			s += utf8_charlen(*s);
		}

	/* Failure. */
	return REG_NOMATCH;
}

/*
 - regtry - try match at specific point
 */
			/* 0 failure, 1 success */
static int regtry( regex_t *preg, const char *string )
{
	int i;

	preg->reginput = string;

	for (i = 0; i < preg->nmatch; i++) {
		preg->pmatch[i].rm_so = -1;
		preg->pmatch[i].rm_eo = -1;
	}
	if (regmatch(preg, 1)) {
		preg->pmatch[0].rm_so = string - preg->start;
		preg->pmatch[0].rm_eo = preg->reginput - preg->start;
		return(1);
	} else
		return(0);
}

/**
 * Returns bytes matched if 'pattern' is a prefix of 'string'.
 *
 * If 'nocase' is non-zero, does a case-insensitive match.
 *
 * Returns -1 on not found.
 */
static int prefix_cmp(const int *prog, int proglen, const char *string, int nocase)
{
	const char *s = string;
	while (proglen && *s) {
		int ch;
		int n = reg_utf8_tounicode_case(s, &ch, nocase);
		if (ch != *prog) {
			return -1;
		}
		prog++;
		s += n;
		proglen--;
	}
	if (proglen == 0) {
		return s - string;
	}
	return -1;
}

/**
 * Searchs for 'c' in the range 'range'.
 * 
 * Returns 1 if found, or 0 if not.
 */
static int reg_range_find(const int *range, int c)
{
	while (*range) {
		/*printf("Checking %d in range [%d,%d]\n", c, range[1], (range[0] + range[1] - 1));*/
		if (c >= range[1] && c <= (range[0] + range[1] - 1)) {
			return 1;
		}
		range += 2;
	}
	return 0;
}

/**
 * Search for the character 'c' in the utf-8 string 'string'.
 * 
 * If 'nocase' is set, the 'string' is assumed to be uppercase
 * and 'c' is converted to uppercase before matching.
 *
 * Returns the byte position in the string where the 'c' was found, or
 * NULL if not found.
 */
static const char *str_find(const char *string, int c, int nocase)
{
	if (nocase) {
		/* The "string" should already be converted to uppercase */
		c = utf8_upper(c);
	}
	while (*string) {
		int ch;
		int n = reg_utf8_tounicode_case(string, &ch, nocase);
		if (c == ch) {
			return string;
		}
		string += n;
	}
	return NULL;
}

/**
 * Returns true if 'ch' is an end-of-line char.
 * 
 * In REG_NEWLINE mode, \n is considered EOL in
 * addition to \0
 */
static int reg_iseol(regex_t *preg, int ch)
{
	if (preg->cflags & REG_NEWLINE) {
		return ch == '\0' || ch == '\n';
	}
	else {
		return ch == '\0';
	}
}

static int regmatchsimplerepeat(regex_t *preg, int scan, int matchmin)
{
	int nextch = '\0';
	const char *save;
	int no;
	int c;

	int max = preg->program[scan + 2];
	int min = preg->program[scan + 3];
	int next = regnext(preg, scan);

	/*
	 * Lookahead to avoid useless match attempts
	 * when we know what character comes next.
	 */
	if (OP(preg, next) == EXACTLY) {
		nextch = preg->program[OPERAND(next)];
	}
	save = preg->reginput;
	no = regrepeat(preg, scan + 5, max);
	if (no < min) {
		return 0;
	}
	if (matchmin) {
		/* from min up to no */
		max = no;
		no = min;
	}
	/* else from no down to min */
	while (1) {
		if (matchmin) {
			if (no > max) {
				break;
			}
		}
		else {
			if (no < min) {
				break;
			}
		}
		preg->reginput = save + utf8_index(save, no);
		reg_utf8_tounicode_case(preg->reginput, &c, (preg->cflags & REG_ICASE));
		/* If it could work, try it. */
		if (reg_iseol(preg, nextch) || c == nextch) {
			if (regmatch(preg, next)) {
				return(1);
			}
		}
		if (matchmin) {
			/* Couldn't or didn't, add one more */
			no++;
		}
		else {
			/* Couldn't or didn't -- back up. */
			no--;
		}
	}
	return(0);
}

static int regmatchrepeat(regex_t *preg, int scan, int matchmin)
{
	int *scanpt = preg->program + scan;

	int max = scanpt[2];
	int min = scanpt[3];

	/* Have we reached min? */
	if (scanpt[4] < min) {
		/* No, so get another one */
		scanpt[4]++;
		if (regmatch(preg, scan + 5)) {
			return 1;
		}
		scanpt[4]--;
		return 0;
	}
	if (scanpt[4] > max) {
		return 0;
	}

	if (matchmin) {
		/* minimal, so try other branch first */
		if (regmatch(preg, regnext(preg, scan))) {
			return 1;
		}
		/* No, so try one more */
		scanpt[4]++;
		if (regmatch(preg, scan + 5)) {
			return 1;
		}
		scanpt[4]--;
		return 0;
	}
	/* maximal, so try this branch again */
	if (scanpt[4] < max) {
		scanpt[4]++;
		if (regmatch(preg, scan + 5)) {
			return 1;
		}
		scanpt[4]--;
	}
	/* At this point we are at max with no match. Try the other branch */
	return regmatch(preg, regnext(preg, scan));
}

/*
 - regmatch - main matching routine
 *
 * Conceptually the strategy is simple:  check to see whether the current
 * node matches, call self recursively to see whether the rest matches,
 * and then act accordingly.  In practice we make some effort to avoid
 * recursion, in particular by going through "ordinary" nodes (that don't
 * need to know whether the rest of the match failed) by a loop instead of
 * by recursion.
 */
/* 0 failure, 1 success */
static int regmatch(regex_t *preg, int prog)
{
	int scan;	/* Current node. */
	int next;		/* Next node. */

	scan = prog;

#ifdef DEBUG
	if (scan != 0 && regnarrate)
		fprintf(stderr, "%s(\n", regprop(scan));
#endif
	while (scan != 0) {
		int n;
		int c;
#ifdef DEBUG
		if (regnarrate) {
			//fprintf(stderr, "%s...\n", regprop(scan));
			fprintf(stderr, "%3d: %s...\n", scan, regprop(OP(preg, scan)));	/* Where, what. */
		}
#endif
		next = regnext(preg, scan);
		n = reg_utf8_tounicode_case(preg->reginput, &c, (preg->cflags & REG_ICASE));

		switch (OP(preg, scan)) {
		case BOL:
			if (preg->reginput != preg->regbol)
				return(0);
			break;
		case EOL:
			if (!reg_iseol(preg, c)) {
				return(0);
			}
			break;
		case WORDA:
			/* Must be looking at a letter, digit, or _ */
			if ((!isalnum(UCHAR(c))) && c != '_')
				return(0);
			/* Prev must be BOL or nonword */
			if (preg->reginput > preg->regbol &&
				(isalnum(UCHAR(preg->reginput[-1])) || preg->reginput[-1] == '_'))
				return(0);
			break;
		case WORDZ:
			/* Can't match at BOL */
			if (preg->reginput > preg->regbol) {
				/* Current must be EOL or nonword */
				if (reg_iseol(preg, c) || !isalnum(UCHAR(c)) || c != '_') {
					c = preg->reginput[-1];
					/* Previous must be word */
					if (isalnum(UCHAR(c)) || c == '_') {
						break;
					}
				}
			}
			/* No */
			return(0);

		case ANY:
			if (reg_iseol(preg, c))
				return 0;
			preg->reginput += n;
			break;
		case EXACTLY: {
				int opnd;
				int len;
				int slen;

				opnd = OPERAND(scan);
				len = str_int_len(preg->program + opnd);

				slen = prefix_cmp(preg->program + opnd, len, preg->reginput, preg->cflags & REG_ICASE);
				if (slen < 0) {
					return(0);
				}
				preg->reginput += slen;
			}
			break;
		case ANYOF:
			if (reg_iseol(preg, c) || reg_range_find(preg->program + OPERAND(scan), c) == 0) {
				return(0);
			}
			preg->reginput += n;
			break;
		case ANYBUT:
			if (reg_iseol(preg, c) || reg_range_find(preg->program + OPERAND(scan), c) != 0) {
				return(0);
			}
			preg->reginput += n;
			break;
		case NOTHING:
			break;
		case BACK:
			break;
		case BRANCH: {
				const char *save;

				if (OP(preg, next) != BRANCH)		/* No choice. */
					next = OPERAND(scan);	/* Avoid recursion. */
				else {
					do {
						save = preg->reginput;
						if (regmatch(preg, OPERAND(scan))) {
							return(1);
						}
						preg->reginput = save;
						scan = regnext(preg, scan);
					} while (scan != 0 && OP(preg, scan) == BRANCH);
					return(0);
					/* NOTREACHED */
				}
			}
			break;
		case REP:
		case REPMIN:
			return regmatchsimplerepeat(preg, scan, OP(preg, scan) == REPMIN);

		case REPX:
		case REPXMIN:
			return regmatchrepeat(preg, scan, OP(preg, scan) == REPXMIN);

		case END:
			return(1);	/* Success! */
			break;
		default:
			if (OP(preg, scan) >= OPEN+1 && OP(preg, scan) < CLOSE_END) {
				const char *save;

				save = preg->reginput;

				if (regmatch(preg, next)) {
					int no;
					/*
					 * Don't set startp if some later
					 * invocation of the same parentheses
					 * already has.
					 */
					if (OP(preg, scan) < CLOSE) {
						no = OP(preg, scan) - OPEN;
						if (no < preg->nmatch && preg->pmatch[no].rm_so == -1) {
							preg->pmatch[no].rm_so = save - preg->start;
						}
					}
					else {
						no = OP(preg, scan) - CLOSE;
						if (no < preg->nmatch && preg->pmatch[no].rm_eo == -1) {
							preg->pmatch[no].rm_eo = save - preg->start;
						}
					}
					return(1);
				} else
					return(0);
			}
			return REG_ERR_INTERNAL;
		}

		scan = next;
	}

	/*
	 * We get here only if there's trouble -- normally "case END" is
	 * the terminating point.
	 */
	return REG_ERR_INTERNAL;
}

/*
 - regrepeat - repeatedly match something simple, report how many
 */
static int regrepeat(regex_t *preg, int p, int max)
{
	int count = 0;
	const char *scan;
	int opnd;
	int ch;
	int n;

	scan = preg->reginput;
	opnd = OPERAND(p);
	switch (OP(preg, p)) {
	case ANY:
		/* No need to handle utf8 specially here */
		while (!reg_iseol(preg, *scan) && count < max) {
			count++;
			scan++;
		}
		break;
	case EXACTLY:
		while (count < max) {
			n = reg_utf8_tounicode_case(scan, &ch, preg->cflags & REG_ICASE);
			if (preg->program[opnd] != ch) {
				break;
			}
			count++;
			scan += n;
		}
		break;
	case ANYOF:
		while (count < max) {
			n = reg_utf8_tounicode_case(scan, &ch, preg->cflags & REG_ICASE);
			if (reg_iseol(preg, ch) || reg_range_find(preg->program + opnd, ch) == 0) {
				break;
			}
			count++;
			scan += n;
		}
		break;
	case ANYBUT:
		while (count < max) {
			n = reg_utf8_tounicode_case(scan, &ch, preg->cflags & REG_ICASE);
			if (reg_iseol(preg, ch) || reg_range_find(preg->program + opnd, ch) != 0) {
				break;
			}
			count++;
			scan += n;
		}
		break;
	default:		/* Oh dear.  Called inappropriately. */
		preg->err = REG_ERR_INTERNAL;
		count = 0;	/* Best compromise. */
		break;
	}
	preg->reginput = scan;

	return(count);
}

/*
 - regnext - dig the "next" pointer out of a node
 */
static int regnext(regex_t *preg, int p )
{
	int offset;

	offset = NEXT(preg, p);

	if (offset == 0)
		return 0;

	if (OP(preg, p) == BACK)
		return(p-offset);
	else
		return(p+offset);
}

#ifdef DEBUG

/*
 - regdump - dump a regexp onto stdout in vaguely comprehensible form
 */
static void regdump(regex_t *preg)
{
	int s;
	int op = EXACTLY;	/* Arbitrary non-END op. */
	int next;
	char buf[4];

	int i;
	for (i = 1; i < preg->p; i++) {
		printf("%02x ", preg->program[i]);
		if (i % 16 == 15) {
			printf("\n");
		}
	}
	printf("\n");

	s = 1;
	while (op != END && s < preg->p) {	/* While that wasn't END last time... */
		op = OP(preg, s);
		printf("%3d: %s", s, regprop(op));	/* Where, what. */
		next = regnext(preg, s);
		if (next == 0)		/* Next ptr. */
			printf("(0)");
		else
			printf("(%d)", next);
		s += 2;
		if (op == REP || op == REPMIN || op == REPX || op == REPXMIN) {
			int max = preg->program[s];
			int min = preg->program[s + 1];
			if (max == 65535) {
				printf("{%d,*}", min);
			}
			else {
				printf("{%d,%d}", min, max);
			}
			printf(" %d", preg->program[s + 2]);
			s += 3;
		}
		else if (op == ANYOF || op == ANYBUT) {
			/* set of ranges */

			while (preg->program[s]) {
				int len = preg->program[s++];
				int first = preg->program[s++];
				buf[utf8_fromunicode(buf, first)] = 0;
				printf("%s", buf);
				if (len > 1) {
					buf[utf8_fromunicode(buf, first + len - 1)] = 0;
					printf("-%s", buf);
				}
			}
			s++;
		}
		else if (op == EXACTLY) {
			/* Literal string, where present. */

			while (preg->program[s]) {
				buf[utf8_fromunicode(buf, preg->program[s])] = 0;
				printf("%s", buf);
				s++;
			}
			s++;
		}
		putchar('\n');
	}

	if (op == END) {
		/* Header fields of interest. */
		if (preg->regstart) {
			buf[utf8_fromunicode(buf, preg->regstart)] = 0;
			printf("start '%s' ", buf);
		}
		if (preg->reganch)
			printf("anchored ");
		if (preg->regmust != 0) {
			int i;
			printf("must have:");
			for (i = 0; i < preg->regmlen; i++) {
				putchar(preg->program[preg->regmust + i]);
			}
			putchar('\n');
		}
	}
	printf("\n");
}

/*
 - regprop - printable representation of opcode
 */
static const char *regprop( int op )
{
	static char buf[50];

	switch (op) {
	case BOL:
		return "BOL";
	case EOL:
		return "EOL";
	case ANY:
		return "ANY";
	case ANYOF:
		return "ANYOF";
	case ANYBUT:
		return "ANYBUT";
	case BRANCH:
		return "BRANCH";
	case EXACTLY:
		return "EXACTLY";
	case NOTHING:
		return "NOTHING";
	case BACK:
		return "BACK";
	case END:
		return "END";
	case REP:
		return "REP";
	case REPMIN:
		return "REPMIN";
	case REPX:
		return "REPX";
	case REPXMIN:
		return "REPXMIN";
	case WORDA:
		return "WORDA";
	case WORDZ:
		return "WORDZ";
	default:
		if (op >= OPEN && op < CLOSE) {
			snprintf(buf, sizeof(buf), "OPEN%d", op-OPEN);
		}
		else if (op >= CLOSE && op < CLOSE_END) {
			snprintf(buf, sizeof(buf), "CLOSE%d", op-CLOSE);
		}
		else {
			snprintf(buf, sizeof(buf), "?%d?\n", op);
		}
		return(buf);
	}
}
#endif

size_t regerror(int errcode, const regex_t *preg, char *errbuf,  size_t errbuf_size)
{
	static const char *error_strings[] = {
		"success",
		"no match",
		"bad pattern",
		"null argument",
		"unknown error",
		"too big",
		"out of memory",
		"too many ()",
		"parentheses () not balanced",
		"braces {} not balanced",
		"invalid repetition count(s)",
		"extra characters",
		"*+ of empty atom",
		"nested count",
		"internal error",
		"count follows nothing",
		"trailing backslash",
		"corrupted program",
		"contains null char",
	};
	const char *err;

	if (errcode < 0 || errcode >= REG_ERR_NUM) {
		err = "Bad error code";
	}
	else {
		err = error_strings[errcode];
	}

	return snprintf(errbuf, errbuf_size, "%s", err);
}

void regfree(regex_t *preg)
{
	free(preg->program);
}

#endif

/* Jimsh - An interactive shell for Jim
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 * Copyright 2009 Steve Bennett <steveb@workware.net.au>
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
#include <stdlib.h>
#include <string.h>


/* From initjimsh.tcl */
extern int Jim_initjimshInit(Jim_Interp *interp);

static void JimSetArgv(Jim_Interp *interp, int argc, char *const argv[])
{
    int n;
    Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);

    /* Populate argv global var */
    for (n = 0; n < argc; n++) {
        Jim_Obj *obj = Jim_NewStringObj(interp, argv[n], -1);

        Jim_ListAppendElement(interp, listObj, obj);
    }

    Jim_SetVariableStr(interp, "argv", listObj);
    Jim_SetVariableStr(interp, "argc", Jim_NewIntObj(interp, argc));
}

int main(int argc, char *const argv[])
{
    int retcode;
    Jim_Interp *interp;

    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("%d.%d\n", JIM_VERSION / 100, JIM_VERSION % 100);
        return 0;
    }

    /* Create and initialize the interpreter */
    interp = Jim_CreateInterp();
    Jim_RegisterCoreCommands(interp);

    /* Register static extensions */
    if (Jim_InitStaticExtensions(interp) != JIM_OK) {
        Jim_MakeErrorMessage(interp);
        fprintf(stderr, "%s\n", Jim_String(Jim_GetResult(interp)));
    }

    Jim_SetVariableStrWithStr(interp, "jim_argv0", argv[0]);
    Jim_SetVariableStrWithStr(interp, JIM_INTERACTIVE, argc == 1 ? "1" : "0");
    retcode = Jim_initjimshInit(interp);

    if (argc == 1) {
        if (retcode == JIM_ERR) {
            Jim_MakeErrorMessage(interp);
            fprintf(stderr, "%s\n", Jim_String(Jim_GetResult(interp)));
        }
        if (retcode != JIM_EXIT) {
            JimSetArgv(interp, 0, NULL);
            retcode = Jim_InteractivePrompt(interp);
        }
    }
    else {
        if (argc > 2 && strcmp(argv[1], "-e") == 0) {
            JimSetArgv(interp, argc - 3, argv + 3);
            retcode = Jim_Eval(interp, argv[2]);
            if (retcode != JIM_ERR) {
                printf("%s\n", Jim_String(Jim_GetResult(interp)));
            }
        }
        else {
            Jim_SetVariableStr(interp, "argv0", Jim_NewStringObj(interp, argv[1], -1));
            JimSetArgv(interp, argc - 2, argv + 2);
            retcode = Jim_EvalFile(interp, argv[1]);
        }
        if (retcode == JIM_ERR) {
            Jim_MakeErrorMessage(interp);
            fprintf(stderr, "%s\n", Jim_String(Jim_GetResult(interp)));
        }
    }
    if (retcode == JIM_EXIT) {
        retcode = Jim_GetExitCode(interp);
    }
    else if (retcode == JIM_ERR) {
        retcode = 1;
    }
    else {
        retcode = 0;
    }
    Jim_FreeInterp(interp);
    return retcode;
}
