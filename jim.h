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
 *
 *--- Inline Header File Documentation ---
 *    [By Duane Ellis, openocd@duaneellis.com, 8/18/8]
 *
 * Belief is "Jim" would greatly benefit if Jim Internals where
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
#include <stdlib.h> /* In order to export the Jim_Free() macro */
#include <stdarg.h> /* In order to get type va_list */

/* -----------------------------------------------------------------------------
 * System configuration
 * autoconf (configure) will set these
 * ---------------------------------------------------------------------------*/
#include <jim-win32compat.h>

#ifndef HAVE_NO_AUTOCONF
#include <jim-config.h>
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

/** Cast a value to unsigned char before byte-oriented processing. */
#define UCHAR(c) ((unsigned char)(c))

/* -----------------------------------------------------------------------------
 * Exported defines
 * ---------------------------------------------------------------------------*/

/* Increment this every time the public ABI changes */
#define JIM_ABI_VERSION 102

/* Tcl return codes */
#define JIM_OK 0
#define JIM_ERR 1
#define JIM_RETURN 2
#define JIM_BREAK 3
#define JIM_CONTINUE 4
#define JIM_SIGNAL 5
#define JIM_EXIT 6
/* Special meaning */
#define JIM_USAGE -1    /* Throw a usage error */
/* The following are internal codes and should never been seen/used */
#define JIM_EVAL 7      /* tailcall */

#define JIM_MAX_CALLFRAME_DEPTH 1000 /* default max nesting depth for procs */
#define JIM_MAX_EVAL_DEPTH 2000 /* default max nesting depth for eval */

/* Some function get an integer argument with flags to change
 * the behaviour. */

/* Starting from 1 << 20 flags are reserved for private uses of
 * different calls. This way the same 'flags' argument may be used
 * to pass both global flags and private flags. */
#define JIM_PRIV_FLAG_SHIFT 20

#define JIM_NONE 0              /* no flags set */
#define JIM_ERRMSG 1            /* set an error message in the interpreter. */
#define JIM_UNSHARED 4          /* Jim_GetVariable() - return unshared object */
#define JIM_MUSTEXIST 8         /* Jim_SetDictKeysVector() - fail if non-existent */
#define JIM_NORESULT 16         /* Jim_SetDictKeysVector() - don't store the result in the interp result */

/* Flags for Jim_SubstObj() */
#define JIM_SUBST_NOVAR 1 /* don't perform variables substitutions */
#define JIM_SUBST_NOCMD 2 /* don't perform command substitutions */
#define JIM_SUBST_NOESC 4 /* don't perform escapes substitutions */
#define JIM_SUBST_FLAG 128 /* flag to indicate that this is a real substitution object */

#define JIM_TAINT_STD   1 /* The "normal" type of taint. Allows for multiple
                           * types of taint in the future
                           */
#define JIM_TAINT_ANY   ~0 /* Any type of taint at all */

/* Flags for Jim_GetEnum() */
#define JIM_ENUM_ABBREV 2    /* Allow unambiguous abbreviation */

/* Flags used by API calls getting a 'nocase' argument. */
#define JIM_CASESENS    0   /* case sensitive */
#define JIM_NOCASE      1   /* no case */
#define JIM_OPT_END     2   /* if implemented by a command (e.g. regexp), add -- to the argument list */

/* Filesystem related */
#define JIM_PATH_LEN 1024

/* Unused arguments generate annoying warnings... */
/** Explicitly mark a parameter or local as intentionally unused. */
#define JIM_NOTUSED(V) ((void) V)

#define JIM_LIBPATH "auto_path"
#define JIM_INTERACTIVE "tcl_interactive"

/* -----------------------------------------------------------------------------
 * Stack
 * ---------------------------------------------------------------------------*/

typedef struct Jim_Stack {
    int len;
    int maxlen;
    void **vector;
    void (*freefunc) (void *ptr);
} Jim_Stack;

/* -----------------------------------------------------------------------------
 * Hash table
 * ---------------------------------------------------------------------------*/

typedef struct Jim_HashEntry {
    void *key;
    union {
        void *val;
        int intval;
    } u;
    struct Jim_HashEntry *next;
} Jim_HashEntry;

typedef struct Jim_HashTableType {
    unsigned int (*hashFunction)(const void *key);
    void *(*keyDup)(void *privdata, const void *key);
    void *(*valDup)(void *privdata, const void *obj);
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
    void (*keyDestructor)(void *privdata, void *key);
    void (*valDestructor)(void *privdata, void *obj);
} Jim_HashTableType;

typedef struct Jim_HashTable {
    Jim_HashEntry **table;
    const Jim_HashTableType *type;
    void *privdata;
    unsigned int size;
    unsigned int sizemask;
    unsigned int used;
    unsigned int collisions;
    unsigned int uniq;
} Jim_HashTable;

typedef struct Jim_HashTableIterator {
    Jim_HashTable *ht;
    Jim_HashEntry *entry, *nextEntry;
    int index;
} Jim_HashTableIterator;

/* This is the initial size of every hash table */
#define JIM_HT_INITIAL_SIZE     16

/* ------------------------------- Macros ------------------------------------*/
/** Run the value destructor for a hash entry if one is configured. */
#define Jim_FreeEntryVal(ht, entry) \
    if ((ht)->type->valDestructor) \
        (ht)->type->valDestructor((ht)->privdata, (entry)->u.val)

/** Store a hash entry value, duplicating it first when the table type requires it. */
#define Jim_SetHashVal(ht, entry, _val_) do { \
    if ((ht)->type->valDup) \
        (entry)->u.val = (ht)->type->valDup((ht)->privdata, (_val_)); \
    else \
        (entry)->u.val = (_val_); \
} while(0)

/** Store an integer payload in a hash entry. */
#define Jim_SetHashIntVal(ht, entry, _val_) (entry)->u.intval = (_val_)

/** Run the key destructor for a hash entry if one is configured. */
#define Jim_FreeEntryKey(ht, entry) \
    if ((ht)->type->keyDestructor) \
        (ht)->type->keyDestructor((ht)->privdata, (entry)->key)

/** Store a hash entry key, duplicating it first when the table type requires it. */
#define Jim_SetHashKey(ht, entry, _key_) do { \
    if ((ht)->type->keyDup) \
        (entry)->key = (ht)->type->keyDup((ht)->privdata, (_key_)); \
    else \
        (entry)->key = (void *)(_key_); \
} while(0)

/** Compare two hash keys using the table's comparison hook when present. */
#define Jim_CompareHashKeys(ht, key1, key2) \
    (((ht)->type->keyCompare) ? \
        (ht)->type->keyCompare((ht)->privdata, (key1), (key2)) : \
        (key1) == (key2))

/** Compute the hash key value used for lookup in a table. */
#define Jim_HashKey(ht, key) ((ht)->type->hashFunction(key) + (ht)->uniq)

/** Return the key stored in a hash entry. */
#define Jim_GetHashEntryKey(he) ((he)->key)
/** Return the pointer value stored in a hash entry. */
#define Jim_GetHashEntryVal(he) ((he)->u.val)
/** Return the integer value stored in a hash entry. */
#define Jim_GetHashEntryIntVal(he) ((he)->u.intval)
/** Return the number of hash collisions recorded for a table. */
#define Jim_GetHashTableCollisions(ht) ((ht)->collisions)
/** Return the current bucket count for a table. */
#define Jim_GetHashTableSize(ht) ((ht)->size)
/** Return the number of live entries in a table. */
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
    char *bytes; /* string representation buffer. NULL = no string repr. */
    const struct Jim_ObjType *typePtr; /* object type. */
    int refCount; /* reference count */
    int length; /* number of bytes in 'bytes', not including the null term. */
    unsigned taint;  /* If this object is tainted */
    /* Internal representation union */
    union {
        /* integer number type */
        jim_wide wideValue;
        /* generic integer value (e.g. index, return code) */
        int intValue;
        /* double number type */
        double doubleValue;
        /* Generic pointer */
        void *ptr;
        /* Generic two pointers value */
        struct {
            void *ptr1;
            void *ptr2;
        } twoPtrValue;
        /* Generic pointer, int, int value */
        struct {
            void *ptr;
            int int1;
            int int2;
        } ptrIntValue;
        /* Variable object */
        struct {
            struct Jim_VarVal *vv;
            unsigned long callFrameId; /* for caching */
            int global; /* If the variable name is globally scoped with :: */
        } varValue;
        /* Command object */
        struct {
            struct Jim_Obj *nsObj;
            struct Jim_Cmd *cmdPtr;
            unsigned long procEpoch; /* for caching */
        } cmdValue;
        /* List object */
        struct {
            struct Jim_Obj **ele;    /* Elements vector */
            int len;        /* Length */
            int maxLen;        /* Allocated 'ele' length */
        } listValue;
        /* dict object */
        struct Jim_Dict *dictValue;
        /* String type */
        struct {
            int maxLength;
            int charLength;     /* utf-8 char length. -1 if unknown */
        } strValue;
        /* Reference type */
        struct {
            unsigned long id;
            struct Jim_Reference *refPtr;
        } refValue;
        /* Source type */
        struct {
            struct Jim_Obj *fileNameObj;
            int lineNumber;
        } sourceValue;
        /* Dict substitution type */
        struct {
            struct Jim_Obj *varNameObjPtr;
            struct Jim_Obj *indexObjPtr;
        } dictSubstValue;
        struct {
            int line;
            int argc;
        } scriptLineValue;
    } internalRep;
    /* These fields add 8 or 16 bytes more for every object
     * but this is required for efficient garbage collection
     * of Jim references. */
    struct Jim_Obj *prevObjPtr; /* pointer to the prev object. */
    struct Jim_Obj *nextObjPtr; /* pointer to the next object. */
} Jim_Obj;

/* Jim_Obj related macros */
/** Increment an object's reference count. */
#define Jim_IncrRefCount(objPtr) \
    ++(objPtr)->refCount
/** Decrement an object's reference count and free it when it reaches zero. */
#define Jim_DecrRefCount(interp, objPtr) \
    if (--(objPtr)->refCount <= 0) Jim_FreeObj(interp, objPtr)
/** Test whether an object has more than one reference. */
#define Jim_IsShared(objPtr) \
    ((objPtr)->refCount > 1)

/* This macro is used when we allocate a new object using
 * Jim_New...Obj(), but for some error we need to destroy it.
 * Instead to use Jim_IncrRefCount() + Jim_DecrRefCount() we
 * can just call Jim_FreeNewObj. To call Jim_Free directly
 * seems too raw, the object handling may change and we want
 * that Jim_FreeNewObj() can be called only against objects
 * that are believed to have refcount == 0. */
#define Jim_FreeNewObj Jim_FreeObj

/* Free the internal representation of the object. */
/** Free an object's internal representation if its type supplies a hook. */
#define Jim_FreeIntRep(i,o) \
    if ((o)->typePtr && (o)->typePtr->freeIntRepProc) \
        (o)->typePtr->freeIntRepProc(i, o)

/* Get the internal representation pointer */
/** Return the generic pointer from an object's internal representation. */
#define Jim_GetIntRepPtr(o) (o)->internalRep.ptr

/* Set the internal representation pointer */
/** Store a generic pointer in an object's internal representation. */
#define Jim_SetIntRepPtr(o, p) \
    (o)->internalRep.ptr = (p)

/* The object type structure.
 * There are three methods.
 *
 * - freeIntRepProc is used to free the internal representation of the object.
 *   Can be NULL if there is nothing to free.
 *
 * - dupIntRepProc is used to duplicate the internal representation of the object.
 *   If NULL, when an object is duplicated, the internalRep union is
 *   directly copied from an object to another.
 *   Note that it's up to the caller to free the old internal repr of the
 *   object before to call the Dup method.
 *
 * - updateStringProc is used to create the string from the internal repr.
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
#define JIM_TYPE_REFERENCES 1    /* The object may contain references. */

/* -----------------------------------------------------------------------------
 * Call frame, vars, commands structures
 * ---------------------------------------------------------------------------*/

/* Call frame */
typedef struct Jim_CallFrame {
    unsigned long id; /* Call Frame ID. Used for caching. */
    int level; /* Level of this call frame. 0 = global */
    struct Jim_HashTable vars; /* Where local vars are stored */
    struct Jim_HashTable *staticVars; /* pointer to procedure static vars */
    struct Jim_CallFrame *parent; /* The parent callframe */
    Jim_Obj *const *argv; /* object vector of the current procedure call. */
    int argc; /* number of args of the current procedure call. */
    Jim_Obj *procArgsObjPtr; /* arglist object of the running procedure */
    Jim_Obj *procBodyObjPtr; /* body object of the running procedure */
    struct Jim_CallFrame *next; /* Callframes are in a linked list */
    Jim_Obj *nsObj;             /* Namespace for this proc call frame */
    Jim_Stack *localCommands; /* commands to be destroyed when the call frame is destroyed */
    struct Jim_Obj *tailcallObj;  /* Pending tailcall invocation */
    struct Jim_Cmd *tailcallCmd;  /* Resolved command for pending tailcall invocation */
} Jim_CallFrame;

/* Evaluation frame */
typedef struct Jim_EvalFrame {
    Jim_CallFrame *framePtr;    /* Pointer to corresponding proc call frame */
    int level;                  /* Level of this evaluation frame. 0 = global */
    int procLevel;              /* Total proc depth */
    struct Jim_Cmd *cmd;        /* The currently executing command */
    struct Jim_EvalFrame *parent; /* The parent eval frame or NULL if at top */
    Jim_Obj *const *argv; /* object vector of the current command . */
    int argc; /* number of args */
    Jim_Obj *scriptObj;
} Jim_EvalFrame;

/* The var structure. It holds the pointer of the referenced
 * object and a reference count. If linkFramePtr is not NULL the variable is a link
 * to a variable of name stored in objPtr living in the given callframe
 * (this happens when the [global] or [upvar] command is used).
 * refCount is normally 1, but may be more than 1 if this has additional references
 * (e.g. from proc static &var)
 * bound to interpreters. */
typedef struct Jim_VarVal {
    Jim_Obj *objPtr;
    struct Jim_CallFrame *linkFramePtr;
    int refCount;
} Jim_VarVal;

/* The cmd structure. */
typedef int Jim_CmdProc(struct Jim_Interp *interp, int argc,
    Jim_Obj *const *argv);
typedef void Jim_DelCmdProc(struct Jim_Interp *interp, void *privData);

/* The dict structure. It uses the same approach as Python OrderedDict
 * of storing a hash table of table offsets into a table containing keys and objects.
 * This preserves order when adding and replacing elements.
 */
typedef struct Jim_Dict {
    struct JimDictHashEntry {
        int offset;
        unsigned hash;
    } *ht;		        /* Allocated hash table of size 'size' */
    unsigned int size;          /* Size of the hash table (0 or power of two) */
    unsigned int sizemask;      /* mask to apply to hash to index into offsets table */
    unsigned int uniq;          /* unique value to add to hash generator */
    Jim_Obj **table;            /* Table of alternating key, value elements */
    int len;                    /* Number of used elements in table */
    int maxLen;                 /* Allocated length of table */
    unsigned int dummy;         /* Number of dummy entries */
} Jim_Dict;

#define JIM_CMD_ISPROC 1
#define JIM_CMD_ISCHANNEL 2
#define JIM_CMD_ISALIAS 4

/* When a command is registered with this flag, it can't be called with
 * tainted data
 */
#define JIM_CMD_NOTAINT 0x100

/* A command is implemented in C if isproc is 0, otherwise
 * it is a Tcl procedure with the arglist and body represented by the
 * two objects referenced by arglistObjPtr and bodyObjPtr. */
typedef struct Jim_Cmd {
    int inUse;           /* Reference count */
    int flags;           /* JIM_CMD_XXX */
    struct Jim_Cmd *prevCmd;    /* Previous command defn if cmd created 'local' */
    Jim_Obj *cmdNameObj;       /* The fully resolved command name - just a pointer, not a reference */
    union {
        struct {
            /* native (C) command */
            Jim_CmdProc *cmdProc; /* The command implementation */
            Jim_DelCmdProc *delProc; /* Called when the command is deleted if != NULL */
            void *privData; /* command-private data available via Jim_CmdPrivData() */
            const char *usage;          /* If not NULL, usage text - used by 'info usage' */
            const char *help;           /* If not NULL, help text - used by 'info help' */
            short minargs;
            short maxargs;              /* -1 for unlimited */
        } native;
        struct {
            /* Tcl procedure */
            Jim_Obj *argListObjPtr;
            Jim_Obj *bodyObjPtr;
            Jim_HashTable *staticVars;  /* Static vars hash table. NULL if no statics. */
            int argListLen;             /* Length of argListObjPtr */
            int reqArity;               /* Number of required parameters */
            int optArity;               /* Number of optional parameters */
            int argsPos;                /* Position of 'args', if specified, or -1 */
            int upcall;                 /* True if proc is currently in upcall */
            struct Jim_ProcArg {
                Jim_Obj *nameObjPtr;    /* Name of this arg */
                Jim_Obj *defaultObjPtr; /* Default value, (or rename for $args) */
            } *arglist;
            Jim_Obj *nsObj;             /* Namespace for this proc */
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
    Jim_Obj *currentFilenameObj; /* filename of current Jim_EvalFile() */
    int break_level;       /* break/continue level */
    int maxCallFrameDepth; /* Used for infinite loop detection. */
    int maxEvalDepth; /* Used for infinite loop detection. */
    int evalDepth;  /* Current eval depth */
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
    unsigned long procEpoch; /* Incremented every time the result
                of procedures names lookup caching
                may no longer be valid. */
    unsigned long callFrameEpoch; /* Incremented every time a new
                callframe is created. This id is used for the
                'ID' field contained in the Jim_CallFrame
                structure. */
    int local; /* If 'local' is in effect, newly defined procs keep a reference to the old defn */
    int quitting; /* Set to 1 during Jim_FreeInterp() */
    int safeexpr; /* Set when evaluating a "safe" expression, no var subst or command eval */
    Jim_Obj *liveList; /* Linked list of all the live objects. */
    Jim_Obj *freeList; /* Linked list of all the unused objects. */
    Jim_EvalFrame topEvalFrame;  /* dummy top evaluation frame */
    Jim_EvalFrame *evalFrame;  /* evaluation stack */
    int procLevel;
    Jim_Obj *nullScriptObj; /* script representation of an empty string */
    Jim_Obj *emptyObj; /* Shared empty string object. */
    Jim_Obj *trueObj; /* Shared true int object. */
    Jim_Obj *falseObj; /* Shared false int object. */
    unsigned long referenceNextId; /* Next id for reference. */
    struct Jim_HashTable references; /* References hash table. */
    unsigned long lastCollectId; /* reference max Id of the last GC
                execution. It's set to ~0 while the collection
                is running as sentinel to avoid to recursive
                calls via the [collect] command inside
                finalizers. */
    jim_wide lastCollectTime; /* unix time of the last GC execution */
    Jim_Obj *stackTrace; /* Stack trace object. */
    Jim_Obj *errorProc; /* Name of last procedure which returned an error */
    Jim_Obj *unknown; /* Unknown command cache */
    Jim_Obj *defer; /* "jim::defer" */
    Jim_Obj *traceCmdObj; /* If non-null, execution trace command to invoke */
    int unknown_called; /* The unknown command has been invoked */
    int hasErrorStackTrace; /* If a stack trace has been set due to an error during execution. */
    void *cmdPrivData; /* Used to pass the private data pointer to
                  a command. It is set to what the user specified
                  via Jim_CreateCommand(). */

    Jim_Cmd *oldCmdCache; /* commands that have been deleted, but may still be cached */
    int oldCmdCacheSize; /* Number of delete commands */
    struct Jim_CallFrame *freeFramesList; /* list of CallFrame structures. */
    struct Jim_HashTable assocData; /* per-interp storage for use by packages */
    Jim_PrngState *prngState; /* per interpreter Random Number Gen. state. */
    struct Jim_HashTable packages; /* Provided packages hash table */
    Jim_Stack *loadHandles; /* handles of loaded modules [load] */
    unsigned taint;  /* Newly created objects get this taint */
} Jim_Interp;

/* Currently provided as macro that performs the increment.
 * At some point may be a real function doing more work.
 * The proc epoch is used in order to know when a command lookup
 * cached can no longer considered valid. */
/** Set the interpreter result to a newly created string object. */
#define Jim_SetResultString(i,s,l) Jim_SetResult(i, Jim_NewStringObj(i,s,l))
/** Set the interpreter result to a newly created integer object. */
#define Jim_SetResultInt(i,intval) Jim_SetResult(i, Jim_NewIntObj(i,intval))
/* Note: Using trueObj and falseObj here makes some things slower...*/
/** Set the interpreter result to a boolean value. */
#define Jim_SetResultBool(i,b) Jim_SetResultInt(i, b)
/** Reset the interpreter result to the shared empty object. */
#define Jim_SetEmptyResult(i) Jim_SetResult(i, (i)->emptyObj)
/** Return the current interpreter result object. */
#define Jim_GetResult(i) ((i)->result)
/** Return the private data pointer of the currently executing command. */
#define Jim_CmdPrivData(i) ((i)->cmdPrivData)

/* Note that 'o' is expanded only one time inside this macro,
 * so it's safe to use side effects. */
/** Replace the interpreter result with the supplied object. */
#define Jim_SetResult(i,o) do {     \
    Jim_Obj *_resultObjPtr_ = (o);    \
    Jim_IncrRefCount(_resultObjPtr_); \
    Jim_DecrRefCount(i,(i)->result);  \
    (i)->result = _resultObjPtr_;     \
} while(0)

/* Use this for filehandles, etc. which need a unique id */
/** Return a new interpreter-wide unique identifier. */
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

/** Create a new shared empty string object. */
#define Jim_NewEmptyStringObj(i) Jim_NewStringObj(i, "", 0)
/** Free a hash table iterator allocated by Jim_GetHashTableIterator(). */
#define Jim_FreeHashTableIterator(iter) Jim_Free(iter)

#define JIM_EXPORT extern

/* Memory allocation */

/* The default Jim Allocator can be replaced by assigning to Jim_Allocator.
 * This function does malloc/realloc/free depending on the arguments.
 * If size is 0, ptr is freed.
 * Otherwise malloc or realloc is done depending on whether ptr is NULL.
 */
JIM_EXPORT void *(*Jim_Allocator)(void *ptr, size_t size);

/** Free memory allocated through Jim_Allocator. */
#define Jim_Free(P) Jim_Allocator((P), 0)
/** Resize a Jim-allocated buffer. */
#define Jim_Realloc(P, S) Jim_Allocator((P), (S))
/** Allocate a new buffer through Jim_Allocator. */
#define Jim_Alloc(S) Jim_Allocator(NULL, (S))
/** Duplicate a NUL-terminated C string using the Jim allocator. */
JIM_EXPORT char * Jim_StrDup (const char *s);
/** Duplicate the first `l` bytes of a C string using the Jim allocator. */
JIM_EXPORT char *Jim_StrDupLen(const char *s, int l);

/* environment */
/** Return the process environment table currently used by Jim. */
JIM_EXPORT char **Jim_GetEnviron(void);
/** Replace the process environment table used by Jim. */
JIM_EXPORT void Jim_SetEnviron(char **env);
/** Create a temporary file from a template and optionally unlink it immediately. */
JIM_EXPORT int Jim_MakeTempFile(Jim_Interp *interp, const char *filename_template, int unlink_file);
#ifndef CLOCK_REALTIME
#  define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
#  define CLOCK_MONOTONIC 1
#endif
#ifndef CLOCK_MONOTONIC_RAW
#  define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC
#endif
/** Return a microsecond timestamp for the requested clock source. */
JIM_EXPORT jim_wide Jim_GetTimeUsec(unsigned type);

/* evaluation */
/** Evaluate a Tcl script string in the current scope. */
JIM_EXPORT int Jim_Eval(Jim_Interp *interp, const char *script);
/* in C code, you can do this and get better error messages */
/*   Jim_EvalSource( interp, __FILE__, __LINE__ , "some tcl commands"); */
/** Evaluate a Tcl script string with explicit filename and line metadata. */
JIM_EXPORT int Jim_EvalSource(Jim_Interp *interp, const char *filename, int lineno, const char *script);
/* Backwards compatibility */
/** Backward-compatible alias for Jim_EvalSource(). */
#define Jim_Eval_Named(I, S, F, L) Jim_EvalSource((I), (F), (L), (S))

/** Evaluate a Tcl script string in the global scope. */
JIM_EXPORT int Jim_EvalGlobal(Jim_Interp *interp, const char *script);
/** Evaluate a script file in the current scope. */
JIM_EXPORT int Jim_EvalFile(Jim_Interp *interp, const char *filename);
/** Evaluate a script file in the global scope. */
JIM_EXPORT int Jim_EvalFileGlobal(Jim_Interp *interp, const char *filename);
/** Evaluate a script held in an object. */
JIM_EXPORT int Jim_EvalObj (Jim_Interp *interp, Jim_Obj *scriptObjPtr);
/** Evaluate a command vector as if it were a parsed command. */
JIM_EXPORT int Jim_EvalObjVector (Jim_Interp *interp, int objc,
        Jim_Obj *const *objv);
/** Evaluate a list object as a command. */
JIM_EXPORT int Jim_EvalObjList(Jim_Interp *interp, Jim_Obj *listObj);
/** Evaluate a command by prepending a command prefix object to an argument vector. */
JIM_EXPORT int Jim_EvalObjPrefix(Jim_Interp *interp, Jim_Obj *prefix,
        int objc, Jim_Obj *const *objv);
/** Evaluate a command using a string prefix converted to an object. */
#define Jim_EvalPrefix(i, p, oc, ov) Jim_EvalObjPrefix((i), Jim_NewStringObj((i), (p), -1), (oc), (ov))
/** Evaluate a script object in the specified namespace. */
JIM_EXPORT int Jim_EvalNamespace(Jim_Interp *interp, Jim_Obj *scriptObj, Jim_Obj *nsObj);
/**
 * Perform substitutions on an object and return the substituted result object.
 *
 * Accepts JIM_SUBST_NOVAR, JIM_SUBST_NOCMD, and JIM_SUBST_NOESC to disable
 * variable, command, and backslash substitution respectively. JIM_SUBST_FLAG is
 * an internal marker used by the [subst] command implementation rather than a
 * normal external API flag.
 */
JIM_EXPORT int Jim_SubstObj (Jim_Interp *interp, Jim_Obj *substObjPtr,
        Jim_Obj **resObjPtrPtr, int flags);

/* source information */
/** Return source filename information for an object and optionally its line number. */
JIM_EXPORT Jim_Obj *Jim_GetSourceInfo(Jim_Interp *interp, Jim_Obj *objPtr,
        int *lineptr);
/* may only be called on an unshared object */
/** Attach source filename and line metadata to an unshared object. */
JIM_EXPORT void Jim_SetSourceInfo(Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj *fileNameObj, int lineNumber);


/* stack */
/** Initialize a stack structure. */
JIM_EXPORT void Jim_StackInit(Jim_Stack *stack, void (*freefunc) (void *ptr));
/** Free the storage used by a stack structure. */
JIM_EXPORT void Jim_StackFree(Jim_Stack *stack);
/** Push an element onto a stack. */
JIM_EXPORT void Jim_StackPush(Jim_Stack *stack, void *element);
/** Pop and return the top element from a stack. */
JIM_EXPORT void *Jim_StackPop(Jim_Stack *stack);

/* hash table */
/** Initialize a hash table with the supplied type hooks and private data. */
JIM_EXPORT int Jim_InitHashTable (Jim_HashTable *ht,
        const Jim_HashTableType *type, void *privdata);
/** Grow a hash table to at least the requested size. */
JIM_EXPORT void Jim_ExpandHashTable (Jim_HashTable *ht,
        unsigned int size);
/** Add a new key/value entry to a hash table. */
JIM_EXPORT int Jim_AddHashEntry (Jim_HashTable *ht, const void *key,
        void *val);
/** Add or replace a key/value entry in a hash table. */
JIM_EXPORT int Jim_ReplaceHashEntry (Jim_HashTable *ht,
        const void *key, void *val);
/** Delete a key from a hash table. */
JIM_EXPORT int Jim_DeleteHashEntry (Jim_HashTable *ht,
        const void *key);
/** Free all storage owned by a hash table. */
JIM_EXPORT int Jim_FreeHashTable (Jim_HashTable *ht);
/** Look up a hash entry by key. */
JIM_EXPORT Jim_HashEntry * Jim_FindHashEntry (Jim_HashTable *ht,
        const void *key);
/** Allocate and initialize an iterator for a hash table. */
JIM_EXPORT Jim_HashTableIterator *Jim_GetHashTableIterator
        (Jim_HashTable *ht);
/** Return the next entry from a hash table iterator. */
JIM_EXPORT Jim_HashEntry * Jim_NextHashEntry
        (Jim_HashTableIterator *iter);

/* objects */
/** Allocate a new empty Jim object. */
JIM_EXPORT Jim_Obj * Jim_NewObj (Jim_Interp *interp);
/** Free a Jim object whose reference count reached zero. */
JIM_EXPORT void Jim_FreeObj (Jim_Interp *interp, Jim_Obj *objPtr);
/** Invalidate an object's cached string representation. */
JIM_EXPORT void Jim_InvalidateStringRep (Jim_Obj *objPtr);
/** Duplicate an existing object, including its internal representation when needed. */
JIM_EXPORT Jim_Obj * Jim_DuplicateObj (Jim_Interp *interp,
        Jim_Obj *objPtr);
/** Return an object's string representation and optionally its byte length. */
JIM_EXPORT const char * Jim_GetString(Jim_Obj *objPtr,
        int *lenPtr);
/** Return an object's NUL-terminated string representation. */
JIM_EXPORT const char *Jim_String(Jim_Obj *objPtr);
/** Return the byte length of an object's string representation. */
JIM_EXPORT int Jim_Length(Jim_Obj *objPtr);

/* string object */
/** Create a string object from a byte sequence. */
JIM_EXPORT Jim_Obj * Jim_NewStringObj (Jim_Interp *interp,
        const char *s, int len);
/** Create a UTF-8 string object from a character-counted string. */
JIM_EXPORT Jim_Obj *Jim_NewStringObjUtf8(Jim_Interp *interp,
        const char *s, int charlen);
/** Create a string object that takes ownership of an allocated buffer. */
JIM_EXPORT Jim_Obj * Jim_NewStringObjNoAlloc (Jim_Interp *interp,
        char *s, int len);
/** Append raw bytes to a string object. */
JIM_EXPORT void Jim_AppendString (Jim_Interp *interp, Jim_Obj *objPtr,
        const char *str, int len);
/** Append one object's string representation to another object. */
JIM_EXPORT void Jim_AppendObj (Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj *appendObjPtr);
/** Append a NULL-terminated list of C strings to an object. */
JIM_EXPORT void Jim_AppendStrings (Jim_Interp *interp,
        Jim_Obj *objPtr, ...);
/** Compare two objects for string equality. */
JIM_EXPORT int Jim_StringEqObj(Jim_Obj *aObjPtr, Jim_Obj *bObjPtr);
/** Match an object string against a pattern object. */
JIM_EXPORT int Jim_StringMatchObj (Jim_Interp *interp, Jim_Obj *patternObjPtr,
        Jim_Obj *objPtr, int nocase);
/** Return a substring object selected by Tcl-style first/last indices. */
JIM_EXPORT Jim_Obj * Jim_StringRangeObj (Jim_Interp *interp,
        Jim_Obj *strObjPtr, Jim_Obj *firstObjPtr,
        Jim_Obj *lastObjPtr);
/** Apply Tcl-style formatting using a format object and argument vector. */
JIM_EXPORT Jim_Obj * Jim_FormatString (Jim_Interp *interp,
        Jim_Obj *fmtObjPtr, int objc, Jim_Obj *const *objv);
/** Parse a string with a Tcl-style scan format and return the scan result object. Accepts JIM_ERRMSG. */
JIM_EXPORT Jim_Obj * Jim_ScanString (Jim_Interp *interp, Jim_Obj *strObjPtr,
        Jim_Obj *fmtObjPtr, int flags);
/** Compare an object string against a literal C string. */
JIM_EXPORT int Jim_CompareStringImmediate (Jim_Interp *interp,
        Jim_Obj *objPtr, const char *str);
/** Compare two object strings with optional case folding. */
JIM_EXPORT int Jim_StringCompareObj(Jim_Interp *interp, Jim_Obj *firstObjPtr,
        Jim_Obj *secondObjPtr, int nocase);
/** Return the number of UTF-8 characters in an object's string representation. */
JIM_EXPORT int Jim_Utf8Length(Jim_Interp *interp, Jim_Obj *objPtr);

/* reference object */
/** Create a reference object tied to another object and optional finalizer metadata. */
JIM_EXPORT Jim_Obj * Jim_NewReference (Jim_Interp *interp,
        Jim_Obj *objPtr, Jim_Obj *tagPtr, Jim_Obj *cmdNamePtr);
/** Resolve a reference object to its underlying reference record. */
JIM_EXPORT Jim_Reference * Jim_GetReference (Jim_Interp *interp,
        Jim_Obj *objPtr);
/** Set or replace the finalizer command associated with a reference object. */
JIM_EXPORT int Jim_SetFinalizer (Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *cmdNamePtr);
/** Retrieve the finalizer command associated with a reference object. */
JIM_EXPORT int Jim_GetFinalizer (Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj **cmdNamePtrPtr);

/* interpreter */
/** Create and initialize a new interpreter. */
JIM_EXPORT Jim_Interp * Jim_CreateInterp (void);
/** Destroy an interpreter and free all resources it owns. */
JIM_EXPORT void Jim_FreeInterp (Jim_Interp *i);
/** Return the exit code stored in an interpreter after `exit`. */
JIM_EXPORT int Jim_GetExitCode (Jim_Interp *interp);
/** Return the textual name of a Jim return code. */
JIM_EXPORT const char *Jim_ReturnCode(int code);
/** Format and set the interpreter result using printf-style arguments. */
JIM_EXPORT void Jim_SetResultFormatted(Jim_Interp *interp, const char *format, ...);

/* commands */
JIM_EXPORT Jim_Cmd *Jim_RegisterCommand(Jim_Interp *interp, Jim_Obj *cmdNameObj,
    Jim_CmdProc *cmdProc,
    Jim_DelCmdProc *delProc,
    const char *usage,
    const char *help,
    short minargs,
    short maxargs,
    int flags,
    void *privData);
/* This is a this wrapper around Jim_RegisterCommand */
JIM_EXPORT int Jim_CreateCommand (Jim_Interp *interp,
        const char *cmdName, Jim_CmdProc *cmdProc, void *privData,
         Jim_DelCmdProc *delProc);
/* Simplify creating commands that specify minargs, maxargs and usage but
 * don't need delProc or privData
 */
#define Jim_RegisterSimpleCmd(interp, name, usage, minargs, maxargs, cmdproc) \
        Jim_RegisterCommand(interp, Jim_NewStringObj(interp, name, -1), cmdproc, NULL, usage, NULL, minargs, maxargs, 0, NULL)
/* And also slightly more complex where delProc, privData and flags may be needed */
#define Jim_RegisterCmd(interp, name, usage, minargs, maxargs, cmdproc, delproc, privdata, flags) \
        Jim_RegisterCommand(interp, Jim_NewStringObj(interp, name, -1), cmdproc, delproc, usage, NULL, minargs, maxargs, flags, privdata)

/** Register the built-in core commands in an interpreter. */
JIM_EXPORT void Jim_RegisterCoreCommands (Jim_Interp *interp);
/** Create a new command backed by a C callback. */
JIM_EXPORT int Jim_CreateCommand (Jim_Interp *interp,
        const char *cmdName, Jim_CmdProc *cmdProc, void *privData,
         Jim_DelCmdProc *delProc);
/** Delete a command by name object. */
JIM_EXPORT int Jim_DeleteCommand (Jim_Interp *interp,
        Jim_Obj *cmdNameObj);
/** Rename an existing command. */
JIM_EXPORT int Jim_RenameCommand (Jim_Interp *interp,
        Jim_Obj *oldNameObj, Jim_Obj *newNameObj);
/** Look up a command by name object. Accepts JIM_ERRMSG. */
JIM_EXPORT Jim_Cmd * Jim_GetCommand (Jim_Interp *interp,
        Jim_Obj *objPtr, int flags);
/** Set a variable using object names and values. */
/* Note that if Jim_SetVariable() fails, and valObjPtr has a zero reference count, it will be freed */
JIM_EXPORT int Jim_SetVariable (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, Jim_Obj *valObjPtr);
/** Set a variable from a C string name. */
JIM_EXPORT int Jim_SetVariableStr (Jim_Interp *interp,
        const char *name, Jim_Obj *objPtr);
/** Set a global variable from a C string name. */
JIM_EXPORT int Jim_SetGlobalVariableStr (Jim_Interp *interp,
        const char *name, Jim_Obj *objPtr);
/** Set a variable from C string name and value inputs. */
JIM_EXPORT int Jim_SetVariableStrWithStr (Jim_Interp *interp,
        const char *name, const char *val);
/** Link a variable name to another variable in a target call frame. */
JIM_EXPORT int Jim_SetVariableLink (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, Jim_Obj *targetNameObjPtr,
        Jim_CallFrame *targetCallFrame);
/** Build the fully qualified global namespace name for an object name. */
JIM_EXPORT Jim_Obj * Jim_MakeGlobalNamespaceName(Jim_Interp *interp,
        Jim_Obj *nameObjPtr);
/**
 * Get a variable by object name.
 *
 * Accepts JIM_ERRMSG and JIM_UNSHARED. JIM_UNSHARED only affects dict-sugar
 * access such as foo(bar): if the backing dictionary is shared, it is
 * duplicated before the element is returned so callers can safely update it.
 */
JIM_EXPORT Jim_Obj * Jim_GetVariable (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, int flags);
/** Get a global variable by object name. Accepts the same flags as Jim_GetVariable(). */
JIM_EXPORT Jim_Obj * Jim_GetGlobalVariable (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, int flags);
/** Get a variable by C string name. Accepts the same flags as Jim_GetVariable(). */
JIM_EXPORT Jim_Obj * Jim_GetVariableStr (Jim_Interp *interp,
        const char *name, int flags);
/** Get a global variable by C string name. Accepts the same flags as Jim_GetVariable(). */
JIM_EXPORT Jim_Obj * Jim_GetGlobalVariableStr (Jim_Interp *interp,
        const char *name, int flags);
/** Unset a variable by object name. Accepts JIM_ERRMSG. */
JIM_EXPORT int Jim_UnsetVariable (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, int flags);

/* call frame */
/** Resolve a Tcl-style stack level to the corresponding call frame. */
JIM_EXPORT Jim_CallFrame *Jim_GetCallFrameByLevel(Jim_Interp *interp,
        Jim_Obj *levelObjPtr);

/* garbage collection */
/** Run a garbage collection pass. */
JIM_EXPORT int Jim_Collect (Jim_Interp *interp);
/** Run garbage collection if the interpreter decides it is needed. */
JIM_EXPORT void Jim_CollectIfNeeded (Jim_Interp *interp);

/* index object */
/** Extract an integer index from an object. */
JIM_EXPORT int Jim_GetIndex (Jim_Interp *interp, Jim_Obj *objPtr,
        int *indexPtr);

/* list object */
/** Create a list object from an element vector. */
JIM_EXPORT Jim_Obj * Jim_NewListObj (Jim_Interp *interp,
        Jim_Obj *const *elements, int len);
/** Insert elements into a list object at the specified index. */
JIM_EXPORT void Jim_ListInsertElements (Jim_Interp *interp,
        Jim_Obj *listPtr, int listindex, int objc, Jim_Obj *const *objVec);
/** Append one element to a list object. */
JIM_EXPORT void Jim_ListAppendElement (Jim_Interp *interp,
        Jim_Obj *listPtr, Jim_Obj *objPtr);
/** Append all elements of one list object to another. */
JIM_EXPORT void Jim_ListAppendList (Jim_Interp *interp,
        Jim_Obj *listPtr, Jim_Obj *appendListPtr);
/** Return the element count of a list object. */
JIM_EXPORT int Jim_ListLength (Jim_Interp *interp, Jim_Obj *objPtr);
/** Return a list element by index, optionally reporting conversion errors. */
JIM_EXPORT int Jim_ListIndex (Jim_Interp *interp, Jim_Obj *listPrt,
        int listindex, Jim_Obj **objPtrPtr, int seterr);
/** Return a list element by index as an object pointer. */
JIM_EXPORT Jim_Obj *Jim_ListGetIndex(Jim_Interp *interp, Jim_Obj *listPtr, int idx);
/** Set a list element reachable through a variable and index vector. */
JIM_EXPORT int Jim_SetListIndex (Jim_Interp *interp,
        Jim_Obj *varNamePtr, Jim_Obj *const *indexv, int indexc,
        Jim_Obj *newObjPtr);
/** Concatenate multiple list-like objects using Tcl list concatenation rules. */
JIM_EXPORT Jim_Obj * Jim_ConcatObj (Jim_Interp *interp, int objc,
        Jim_Obj *const *objv);
/** Join the elements of a list into a string with the supplied separator. */
JIM_EXPORT Jim_Obj *Jim_ListJoin(Jim_Interp *interp,
        Jim_Obj *listObjPtr, const char *joinStr, int joinStrLen);

/* dict object */
/** Create a dictionary object from alternating key/value elements. */
JIM_EXPORT Jim_Obj * Jim_NewDictObj (Jim_Interp *interp,
        Jim_Obj *const *elements, int len);
/** Look up one key in a dictionary object. Accepts JIM_ERRMSG. */
JIM_EXPORT int Jim_DictKey (Jim_Interp *interp, Jim_Obj *dictPtr,
        Jim_Obj *keyPtr, Jim_Obj **objPtrPtr, int flags);
/** Follow a vector of keys through nested dictionaries. Accepts JIM_ERRMSG. */
JIM_EXPORT int Jim_DictKeysVector (Jim_Interp *interp,
        Jim_Obj *dictPtr, Jim_Obj *const *keyv, int keyc,
        Jim_Obj **objPtrPtr, int flags);
/**
 * Set a nested dictionary value addressed by a key vector inside a variable.
 *
 * Accepts JIM_ERRMSG, JIM_UNSHARED, JIM_MUSTEXIST, and JIM_NORESULT.
 * JIM_UNSHARED duplicates a shared container before modifying it.
 * JIM_MUSTEXIST makes missing variables or keys an error during unset-style updates.
 * JIM_NORESULT suppresses storing the updated dictionary in the interpreter result.
 */
JIM_EXPORT int Jim_SetDictKeysVector (Jim_Interp *interp,
        Jim_Obj *varNamePtr, Jim_Obj *const *keyv, int keyc,
        Jim_Obj *newObjPtr, int flags);
/** Return the alternating key/value object array used by a dictionary. */
JIM_EXPORT Jim_Obj **Jim_DictPairs(Jim_Interp *interp,
        Jim_Obj *dictPtr, int *len);
/** Add or replace one key/value pair in a dictionary object. */
JIM_EXPORT int Jim_DictAddElement(Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj *keyObjPtr, Jim_Obj *valueObjPtr);

#define JIM_DICTMATCH_KEYS 0x0001
#define JIM_DICTMATCH_VALUES 0x002

/** Match dictionary keys and/or values against a pattern and return the requested components. */
JIM_EXPORT int Jim_DictMatchTypes(Jim_Interp *interp, Jim_Obj *objPtr, Jim_Obj *patternObj, int match_type, int return_types);
/** Return the number of key/value pairs in a dictionary object. */
JIM_EXPORT int Jim_DictSize(Jim_Interp *interp, Jim_Obj *objPtr);
/** Return implementation-specific diagnostic information about a dictionary object. */
JIM_EXPORT int Jim_DictInfo(Jim_Interp *interp, Jim_Obj *objPtr);
/** Merge multiple dictionaries and return the merged dictionary object. */
JIM_EXPORT Jim_Obj *Jim_DictMerge(Jim_Interp *interp, int objc, Jim_Obj *const *objv);

/* return code object */
/** Extract a return code integer from an object. */
JIM_EXPORT int Jim_GetReturnCode (Jim_Interp *interp, Jim_Obj *objPtr,
        int *intPtr);

/* expression object */
/** Evaluate an expression object and leave the result in the interpreter. */
JIM_EXPORT int Jim_EvalExpression (Jim_Interp *interp,
        Jim_Obj *exprObjPtr);
/** Evaluate an expression object and convert the result to boolean. */
JIM_EXPORT int Jim_GetBoolFromExpr (Jim_Interp *interp,
        Jim_Obj *exprObjPtr, int *boolPtr);

/* boolean object */
/** Convert an object to boolean. */
JIM_EXPORT int Jim_GetBoolean(Jim_Interp *interp, Jim_Obj *objPtr,
        int *booleanPtr);

/* integer object */
/** Convert an object to Jim's wide integer type. */
JIM_EXPORT int Jim_GetWide (Jim_Interp *interp, Jim_Obj *objPtr,
        jim_wide *widePtr);
/** Evaluate an expression object and convert the result to a wide integer. */
JIM_EXPORT int Jim_GetWideExpr(Jim_Interp *interp, Jim_Obj *objPtr,
        jim_wide *widePtr);
/** Convert an object to a C long. */
JIM_EXPORT int Jim_GetLong (Jim_Interp *interp, Jim_Obj *objPtr,
        long *longPtr);
/** Backward-compatible alias for Jim_NewIntObj(). */
#define Jim_NewWideObj  Jim_NewIntObj
/** Create an integer object from a wide integer value. */
JIM_EXPORT Jim_Obj * Jim_NewIntObj (Jim_Interp *interp,
        jim_wide wideValue);

/* double object */
/** Convert an object to double. */
JIM_EXPORT int Jim_GetDouble(Jim_Interp *interp, Jim_Obj *objPtr,
        double *doublePtr);
/** Replace an object's internal value with a double. */
JIM_EXPORT void Jim_SetDouble(Jim_Interp *interp, Jim_Obj *objPtr,
        double doubleValue);
/** Create a new double object. */
JIM_EXPORT Jim_Obj * Jim_NewDoubleObj(Jim_Interp *interp, double doubleValue);

/* commands utilities */
/** Set the standard wrong-argument-count error message for a command. */
JIM_EXPORT void Jim_WrongNumArgs (Jim_Interp *interp, int argc,
        Jim_Obj *const *argv, const char *msg);
/**
 * Map an object string to an index in a name table.
 *
 * Accepts JIM_ERRMSG and JIM_ENUM_ABBREV. JIM_ENUM_ABBREV accepts
 * unambiguous abbreviations.
 */
JIM_EXPORT int Jim_GetEnum (Jim_Interp *interp, Jim_Obj *objPtr,
        const char * const *tablePtr, int *indexPtr, const char *name, int flags);
/** Implement standard `-commands` support for subcommand tables. */
JIM_EXPORT int Jim_CheckShowCommands(Jim_Interp *interp, Jim_Obj *objPtr,
        const char *const *tablePtr);
/** Check whether a script object is syntactically complete. */
JIM_EXPORT int Jim_ScriptIsComplete(Jim_Interp *interp,
        Jim_Obj *scriptObj, char *stateCharPtr);

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
/** Look up interpreter-associated package data by key. */
JIM_EXPORT void * Jim_GetAssocData(Jim_Interp *interp, const char *key);
/** Store interpreter-associated package data with an optional delete callback. */
JIM_EXPORT int Jim_SetAssocData(Jim_Interp *interp, const char *key,
        Jim_InterpDeleteProc *delProc, void *data);
/** Remove interpreter-associated package data by key. */
JIM_EXPORT int Jim_DeleteAssocData(Jim_Interp *interp, const char *key);
/** Check that an extension was built against the current Jim ABI. */
JIM_EXPORT int Jim_CheckAbiVersion(Jim_Interp *interp, int abi_version);

/* Packages C API */

/* jim-package.c */
/** Announce that a package is now provided in the interpreter. Accepts JIM_ERRMSG. */
JIM_EXPORT int Jim_PackageProvide (Jim_Interp *interp,
        const char *name, const char *ver, int flags);
/** Require and, if needed, load a package into the interpreter. Accepts JIM_ERRMSG. */
JIM_EXPORT int Jim_PackageRequire (Jim_Interp *interp,
        const char *name, int flags);
/** Verify ABI compatibility and provide a package from an extension init function. */
#define Jim_PackageProvideCheck(INTERP, NAME) \
        if (Jim_CheckAbiVersion(INTERP, JIM_ABI_VERSION) == JIM_ERR || Jim_PackageProvide(INTERP, NAME, "1.0", JIM_ERRMSG)) \
                return JIM_ERR

/* error messages */
/** Expand the current interpreter error state into a user-facing message. */
JIM_EXPORT void Jim_MakeErrorMessage (Jim_Interp *interp);

/* interactive mode */
/** Run the interactive command prompt. */
JIM_EXPORT int Jim_InteractivePrompt (Jim_Interp *interp);
/** Load history entries from a file. */
JIM_EXPORT void Jim_HistoryLoad(const char *filename);
/** Save history entries to a file. */
JIM_EXPORT void Jim_HistorySave(const char *filename);
/** Read one line of interactive input using the configured line editor. */
JIM_EXPORT char *Jim_HistoryGetline(Jim_Interp *interp, const char *prompt);
/** Install the callback used for interactive completion. */
JIM_EXPORT void Jim_HistorySetCompletion(Jim_Interp *interp, Jim_Obj *completionCommandObj);
/** Install the callback used for interactive hints. */
JIM_EXPORT void Jim_HistorySetHints(Jim_Interp *interp, Jim_Obj *hintsCommandObj);
/** Append one line to the interactive history. */
JIM_EXPORT void Jim_HistoryAdd(const char *line);
/** Print the interactive history. */
JIM_EXPORT void Jim_HistoryShow(void);
/** Set the maximum retained history length. */
JIM_EXPORT void Jim_HistorySetMaxLen(int length);
/** Return the maximum retained history length. */
JIM_EXPORT int Jim_HistoryGetMaxLen(void);

/* Misc */
/** Initialize all statically linked extensions in an interpreter. */
JIM_EXPORT int Jim_InitStaticExtensions(Jim_Interp *interp);
/** Convert a C string to a wide integer using the supplied base. */
JIM_EXPORT int Jim_StringToWide(const char *str, jim_wide *widePtr, int base);
/** Return non-zero on big-endian hosts. */
JIM_EXPORT int Jim_IsBigEndian(void);

/**
 * Returns 1 if a signal has been received while
 * in a catch -signal {} clause.
 */
#define Jim_CheckSignal(i) ((i)->signal_level && (i)->sigmask)
/** Mark the supplied signal mask as ignored by the signal handler layer. */
JIM_EXPORT void Jim_SignalSetIgnored(jim_wide mask);

/* jim-load.c */
/** Load a binary extension library from disk. */
JIM_EXPORT int Jim_LoadLibrary(Jim_Interp *interp, const char *pathName);
/** Free all module load handles owned by an interpreter. */
JIM_EXPORT void Jim_FreeLoadHandles(Jim_Interp *interp);

/* jim-aio.c */
/** Convert an AIO command object to its underlying filehandle. */
JIM_EXPORT int Jim_AioFilehandle(Jim_Interp *interp, Jim_Obj *command);

/* type inspection - avoid where possible */
/** Return non-zero if an object is currently represented as a dictionary. */
JIM_EXPORT int Jim_IsDict(Jim_Obj *objPtr);
/** Return non-zero if an object is currently represented as a list. */
JIM_EXPORT int Jim_IsList(Jim_Obj *objPtr);

/* taint */
JIM_EXPORT void Jim_SetTaintError(Jim_Interp *interp, int cmdargs, Jim_Obj *const *argv);
JIM_EXPORT int Jim_CalcTaint(int argc, Jim_Obj *const *argv);

#ifdef JIM_TAINT
#define Jim_CheckTaint(i, t) ((i)->taint & (t))
#define Jim_TaintObj(o,t) (o)->taint |= (t)
#define Jim_UntaintObj(o) (o)->taint = 0
#define Jim_GetObjTaint(o) (o)->taint
#else
#define Jim_CheckTaint(i, t) 0
#define Jim_TaintObj(o,t)
#define Jim_UntaintObj(o)
#define Jim_GetObjTaint(o) 0
#endif

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
