/* Jim - A small embeddable Tcl interpreter
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

#ifndef __JIM__H
#define __JIM__H

#include <time.h>
#include <limits.h>

/* -----------------------------------------------------------------------------
 * System configuration
 * For most modern systems, you can leave the default.
 * For embedded systems some change may be required.
 * ---------------------------------------------------------------------------*/

#define HAVE_LONG_LONG

/* -----------------------------------------------------------------------------
 * Compiler specific fixes.
 * ---------------------------------------------------------------------------*/

/* MSC has _stricmp instead of strcasecmp */
#ifdef _MSC_VER
#  define strcasecmp _stricmp
#endif /* _MSC_VER */

/* Long Long type and related issues */
#ifdef HAVE_LONG_LONG
#  ifdef _MSC_VER /* MSC compiler */
#    define jim_wide _int64
#    ifndef LLONG_MAX
#      define LLONG_MAX    9223372036854775807I64
#    endif
#    ifndef LLONG_MIN
#      define LLONG_MIN    (-LLONG_MAX - 1I64)
#    endif
#    define JIM_WIDE_MIN LLONG_MIN
#    define JIM_WIDE_MAX LLONG_MAX
#  else /* Other compilers (mainly GCC) */
#    define jim_wide long long
#    ifndef LLONG_MAX
#      define LLONG_MAX    9223372036854775807LL
#    endif
#    ifndef LLONG_MIN
#      define LLONG_MIN    (-LLONG_MAX - 1LL)
#    endif
#    define JIM_WIDE_MIN LLONG_MIN
#    define JIM_WIDE_MAX LLONG_MAX
#  endif
#else
#  define jim_wide long
#  define JIM_WIDE_MIN LONG_MIN
#  define JIM_WIDE_MAX LONG_MAX
#endif

/* -----------------------------------------------------------------------------
 * LIBC specific fixes
 * ---------------------------------------------------------------------------*/

#ifdef __MSVCRT__
#    define JIM_LL_MODIFIER "I64d"
#else
#    define JIM_LL_MODIFIER "lld"
#endif

/* -----------------------------------------------------------------------------
 * Exported defines
 * ---------------------------------------------------------------------------*/

/* Jim version numbering: every version of jim is marked with a
 * successive integer number. This is version 0. The first
 * stable version will be 1, then 2, 3, and so on. */
#define JIM_VERSION 0

#define JIM_OK 0
#define JIM_ERR 1
#define JIM_RETURN 2
#define JIM_BREAK 3
#define JIM_CONTINUE 4
#define JIM_MAX_NESTING_DEPTH 5000 /* default max nesting depth */

/* Some function get an integer argument with flags to change
 * the behaviour. */
#define JIM_NONE 0    /* no flags set */
#define JIM_ERRMSG 1    /* set an error message in the interpreter. */

/* Flags for Jim_SubstObj() */
#define JIM_SUBST_NOVAR 1 /* don't perform variables substitutions */
#define JIM_SUBST_NOCMD 2 /* don't perform command substitutions */
#define JIM_SUBST_NOESC 4 /* don't perform escapes substitutions */

/* -----------------------------------------------------------------------------
 * Hash table
 * ---------------------------------------------------------------------------*/

typedef struct Jim_HashEntry {
    void *key;
    void *val;
    struct Jim_HashEntry *next;
} Jim_HashEntry;

typedef struct Jim_HashTableType {
    unsigned int (*hashFunction)(void *key);
    void *(*keyDup)(void *privdata, void *key);
    void *(*valDup)(void *privdata, void *obj);
    int (*keyCompare)(void *privdata, void *key1, void *key2);
    void (*keyDestructor)(void *privdata, void *key);
    void (*valDestructor)(void *privdata, void *obj);
} Jim_HashTableType;

typedef struct Jim_HashTable {
    Jim_HashEntry **table;
    Jim_HashTableType *type;
    unsigned int size;
    unsigned int sizemask;
    unsigned int used;
    unsigned int collisions;
    void *privdata;
} Jim_HashTable;

typedef struct Jim_HashTableIterator {
    Jim_HashTable *ht;
    int index;
    Jim_HashEntry *entry;
} Jim_HashTableIterator;

/* This is the initial size of every hash table */
#define JIM_HT_INITIAL_SIZE     256

/* ------------------------------- Macros ------------------------------------*/
#define Jim_FreeEntryVal(ht, entry) \
    if ((ht)->type->valDestructor) \
        (ht)->type->valDestructor((ht)->privdata, (entry)->val)

#define Jim_SetHashVal(ht, entry, _val_) do { \
    if ((ht)->type->valDup) \
        entry->val = (ht)->type->valDup((ht)->privdata, _val_); \
    else \
        entry->val = (_val_); \
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
    struct Jim_ObjType *typePtr; /* object type. */
    /* Internal representation union */
    union {
        /* integer number type */
        jim_wide wideValue;
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
        } strValue;
        /* Reference type */
        struct {
            jim_wide id;
            struct Jim_Reference *refPtr;
        } refValue;
        /* Source type */
        struct {
            char *fileName;
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
    char *name; /* The name of the type. */
    Jim_FreeInternalRepProc *freeIntRepProc;
    Jim_DupInternalRepProc *dupIntRepProc;
    Jim_UpdateStringProc *updateStringProc;
    int flags;
} Jim_ObjType;

/* Jim_ObjType flags */
#define JIM_TYPE_NONE 0        /* No flags */
#define JIM_TYPE_REFERENCES 1    /* The object may contain referneces. */

/* -----------------------------------------------------------------------------
 * Call frame, vars, commands structures
 * ---------------------------------------------------------------------------*/

/* Call frame */
typedef struct Jim_CallFrame {
    unsigned jim_wide id; /* Call Frame ID. Used for caching. */
    struct Jim_HashTable vars;
    struct Jim_CallFrame *parentCallFrame;
    Jim_Obj **argv; /* object vector of the current procedure call. */
    int argc; /* number of args of the current procedure call. */
    Jim_Obj *procArgsObjPtr; /* arglist object of the running procedure */
    Jim_Obj *procBodyObjPtr; /* body object of the running procedure */
    struct Jim_CallFrame *nextFramePtr;
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
typedef int (*Jim_CmdProc)(struct Jim_Interp *interp, int argc, Jim_Obj **argv);

/* A command is implemented in C if funcPtr is != NULL, otherwise
 * it's a Tcl procedure with the arglist and body represented by the
 * two objects referenced by arglistObjPtr and bodyoObjPtr. */
typedef struct Jim_Cmd {
    Jim_CmdProc cmdProc; /* Not-NULL for a C command. */
    void *privData; /* Only used for C commands. */
    Jim_Obj *argListObjPtr;
    Jim_Obj *bodyObjPtr;
    int arityMin; /* Min number of arguments. */
    int arityMax; /* Max number of arguments. */
} Jim_Cmd;

/* -----------------------------------------------------------------------------
 * Jim interpreter structure.
 * Fields similar to the real Tcl interpreter structure have the same names.
 * ---------------------------------------------------------------------------*/
typedef struct Jim_Interp {
    Jim_Obj *result; /* object returned by the last command called. */
    int errorLine; /* Error line where an error occurred. */
    char *errorFileName; /* Error file where an error occurred. */
    int numLevels; /* Number of current nested calls. */
    int maxNestingDepth; /* Used for infinite loop detection. */
    int returnCode; /* Completion code to return on JIM_RETURN. */
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
    Jim_Obj *liveList; /* Linked list of all the live objects. */
    Jim_Obj *freeList; /* Linked list of all the unused objects. */
    char *scriptFileName; /* File name of the script currently in execution. */
    Jim_Obj *emptyObj; /* Shared empty string object. */
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
    Jim_Obj *unknown; /* Unknown command cache */
    int errorFlag; /* Set if an error occurred during execution. */
    void *cmdPrivData; /* Used to pass the private data pointer to
                  command. It is set to what the user specified
                  via Jim_CreateCommand(). */

    struct Jim_HashTable stub; /* Stub hash table to export API */
    void *getApiFuncPtr; /* Jim_GetApi() function pointer. */
    struct Jim_CallFrame *freeFramesList; /* list of CallFrame structures. */
} Jim_Interp;

/* Currently provided as macro that performs the increment.
 * At some point may be a real function doing more work.
 * The proc epoch is used in order to know when a command lookup
 * cached can no longer considered valid. */
#define Jim_InterpIncrProcEpoch(i) (i)->procEpoch++
#define Jim_SetResultString(i,s,l) Jim_SetResult(i, Jim_NewStringObj(i,s,l))
#define Jim_SetEmptyResult(i) Jim_SetResult(i, (i)->emptyObj)
#define Jim_GetResult(i) ((i)->result)
#define Jim_CmdPrivData(i) ((i)->cmdPrivData)

/* Note that 'o' is expanded only one time inside this macro,
 * so it's safe to use side effects. */
#define Jim_SetResult(i,o) do {     \
    Jim_Obj *_resultObjPtr_ = (o);    \
    Jim_IncrRefCount(_resultObjPtr_); \
    Jim_DecrRefCount(i,(i)->result);  \
    (i)->result = _resultObjPtr_;     \
} while(0)

/* Reference structure. The interpreter pointer is held within privdata member in HashTable */
typedef struct Jim_Reference {
    Jim_Obj *objPtr;
    Jim_Obj *finalizerCmdNamePtr;
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
#define Jim_Free free
#define Jim_FreeHashTableIterator(iter) Jim_Free(iter)

#ifndef __JIM_CORE__
#define JIM_API(x) (*x)
#define JIM_STATIC
#else
#define JIM_API(x) x
#define JIM_STATIC static
#endif /* __JIM_CORE__ */

/* Memory allocation */
JIM_STATIC void * JIM_API(Jim_Alloc) (int size);
JIM_STATIC char * JIM_API(Jim_StrDup) (char *s);

/* evaluation */
JIM_STATIC int JIM_API(Jim_EvalObj) (Jim_Interp *interp, Jim_Obj *scriptObjPtr);
JIM_STATIC int JIM_API(Jim_EvalObjVector) (Jim_Interp *interp, int objc,
        Jim_Obj **objv);
JIM_STATIC int JIM_API(Jim_SubstObj) (Jim_Interp *interp, Jim_Obj *substObjPtr,
        Jim_Obj **resObjPtrPtr, int flags);

/* hash table */
JIM_STATIC int JIM_API(Jim_InitHashTable) (Jim_HashTable *ht,
        Jim_HashTableType *type, void *privdata);
JIM_STATIC int JIM_API(Jim_ExpandHashTable) (Jim_HashTable *ht,
        unsigned int size);
JIM_STATIC int JIM_API(Jim_AddHashEntry) (Jim_HashTable *ht, void *key,
        void *val);
JIM_STATIC int JIM_API(Jim_ReplaceHashEntry) (Jim_HashTable *ht, void *key,
        void *val);
JIM_STATIC int JIM_API(Jim_DeleteHashEntry) (Jim_HashTable *ht, void *key);
JIM_STATIC int JIM_API(Jim_FreeHashTable) (Jim_HashTable *ht);
JIM_STATIC Jim_HashEntry * JIM_API(Jim_FindHashEntry) (Jim_HashTable *ht,
        void *key);
JIM_STATIC int JIM_API(Jim_ResizeHashTable) (Jim_HashTable *ht);
JIM_STATIC Jim_HashTableIterator *JIM_API(Jim_GetHashTableIterator)
        (Jim_HashTable *ht);
JIM_STATIC Jim_HashEntry * JIM_API(Jim_NextHashEntry)
        (Jim_HashTableIterator *iterator);

/* objects */
JIM_STATIC Jim_Obj * JIM_API(Jim_NewObj) (Jim_Interp *interp);
JIM_STATIC void JIM_API(Jim_FreeObj) (Jim_Interp *interp, Jim_Obj *objPtr);
JIM_STATIC void JIM_API(Jim_InvalidateStringRep) (Jim_Obj *objPtr);
JIM_STATIC void JIM_API(Jim_InitStringRep) (Jim_Obj *objPtr, char *bytes,
        int length);
JIM_STATIC Jim_Obj * JIM_API(Jim_DuplicateObj) (Jim_Interp *interp,
        Jim_Obj *objPtr);
JIM_STATIC char * JIM_API(Jim_GetString)(Jim_Obj *objPtr, int *lenPtr);
JIM_STATIC void JIM_API(Jim_InvalidateStringRep)(Jim_Obj *objPtr);

/* string object */
JIM_STATIC Jim_Obj * JIM_API(Jim_NewStringObj) (Jim_Interp *interp,
        const char *s, int len);
JIM_STATIC Jim_Obj * JIM_API(Jim_NewStringObjNoAlloc) (Jim_Interp *interp,
        char *s, int len);
JIM_STATIC void JIM_API(Jim_AppendString) (Jim_Interp *interp, Jim_Obj *objPtr,
        char *str, int len);
JIM_STATIC void JIM_API(Jim_AppendObj) (Jim_Interp *interp, Jim_Obj *objPtr,
        Jim_Obj *appendObjPtr);
JIM_STATIC void JIM_API(Jim_AppendStrings) (Jim_Interp *interp,
        Jim_Obj *objPtr, ...);
JIM_STATIC int JIM_API(Jim_StringEqObj) (Jim_Obj *aObjPtr, Jim_Obj *bObjPtr,
        int nocase);
JIM_STATIC int JIM_API(Jim_StringMatchObj) (Jim_Obj *patternObjPtr,
        Jim_Obj *objPtr, int nocase);
JIM_STATIC Jim_Obj * JIM_API(Jim_StringRangeObj) (Jim_Interp *interp,
        Jim_Obj *strObjPtr, Jim_Obj *firstObjPtr,
        Jim_Obj *lastObjPtr);
JIM_STATIC int JIM_API(Jim_CompareStringImmediate) (Jim_Interp *interp,
        Jim_Obj *objPtr, char *str);

/* reference object */
JIM_STATIC Jim_Obj * JIM_API(Jim_NewReference) (Jim_Interp *interp,
        Jim_Obj *objPtr, Jim_Obj *cmdNamePtr);
JIM_STATIC Jim_Reference * JIM_API(Jim_GetReference) (Jim_Interp *interp,
        Jim_Obj *objPtr);

/* interpreter */
JIM_STATIC Jim_Interp * JIM_API(Jim_CreateInterp) (void);
JIM_STATIC void JIM_API(Jim_FreeInterp) (Jim_Interp *i);

/* commands */
JIM_STATIC void JIM_API(Jim_RegisterCoreCommands) (Jim_Interp *interp);
JIM_STATIC int JIM_API(Jim_CreateCommand) (Jim_Interp *interp, char *cmdName,
        Jim_CmdProc cmdProc, void *privData);
JIM_STATIC int JIM_API(Jim_CreateProcedure) (Jim_Interp *interp, char *cmdName,
                Jim_Obj *argListObjPtr, Jim_Obj *bodyObjPtr,
                int arityMin, int arityMax);
JIM_STATIC int JIM_API(Jim_DeleteCommand) (Jim_Interp *interp, char *cmdName);
JIM_STATIC int JIM_API(Jim_RenameCommand) (Jim_Interp *interp, char *oldName,
        char *newName);
JIM_STATIC Jim_Cmd * JIM_API(Jim_GetCommand) (Jim_Interp *interp,
        Jim_Obj *objPtr, int flags);
JIM_STATIC int JIM_API(Jim_SetVariable) (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, Jim_Obj *valObjPtr);
JIM_STATIC int JIM_API(Jim_SetVariableLink) (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, Jim_Obj *targetNameObjPtr,
        Jim_CallFrame *targetCallFrame);
JIM_STATIC Jim_Obj * JIM_API(Jim_GetVariable) (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, int flags);
JIM_STATIC int JIM_API(Jim_UnsetVariable) (Jim_Interp *interp,
        Jim_Obj *nameObjPtr, int flags);

/* call frame */
JIM_STATIC int JIM_API(Jim_GetCallFrameByLevel) (Jim_Interp *interp,
        Jim_Obj *levelObjPtr, Jim_CallFrame **framePtrPtr,
        int *newLevelPtr);

/* garbage collection */
JIM_STATIC int JIM_API(Jim_Collect) (Jim_Interp *interp);
JIM_STATIC void JIM_API(Jim_CollectIfNeeded) (Jim_Interp *interp);

/* index object */
JIM_STATIC int JIM_API(Jim_GetIndex) (Jim_Interp *interp, Jim_Obj *objPtr,
        int *indexPtr);

/* list object */
JIM_STATIC Jim_Obj * JIM_API(Jim_NewListObj) (Jim_Interp *interp,
        Jim_Obj **elements, int len);
JIM_STATIC void JIM_API(Jim_ListAppendElement) (Jim_Interp *interp,
        Jim_Obj *listPtr, Jim_Obj *objPtr);
JIM_STATIC void JIM_API(Jim_ListAppendList) (Jim_Interp *interp,
        Jim_Obj *listPtr, Jim_Obj *appendListPtr);
JIM_STATIC void JIM_API(Jim_ListLength) (Jim_Interp *interp, Jim_Obj *listPtr,
        int *intPtr);
JIM_STATIC int JIM_API(Jim_ListIndex) (Jim_Interp *interp, Jim_Obj *listPrt,
        int index, Jim_Obj **objPtrPtr, int seterr);
JIM_STATIC int JIM_API(Jim_SetListIndex) (Jim_Interp *interp,
        Jim_Obj *varNamePtr, Jim_Obj **indexv, int indexc,
        Jim_Obj *newObjPtr);
JIM_STATIC Jim_Obj * JIM_API(Jim_ConcatObj) (Jim_Interp *interp, int objc,
        Jim_Obj **objv);

/* dict object */
JIM_STATIC Jim_Obj * JIM_API(Jim_NewDictObj) (Jim_Interp *interp,
        Jim_Obj **elements, int len);
JIM_STATIC int JIM_API(Jim_DictKey) (Jim_Interp *interp, Jim_Obj *dictPtr,
        Jim_Obj *keyPtr, Jim_Obj **objPtrPtr, int flags);
JIM_STATIC int JIM_API(Jim_DictKeysVector) (Jim_Interp *interp,
        Jim_Obj *dictPtr, Jim_Obj **keyv, int keyc,
        Jim_Obj **objPtrPtr, int flags);
JIM_STATIC int JIM_API(Jim_GetIndex) (Jim_Interp *interp, Jim_Obj *objPtr,
        int *indexPtr);
JIM_STATIC int JIM_API(Jim_SetDictKeysVector) (Jim_Interp *interp,
        Jim_Obj *varNamePtr, Jim_Obj **keyv, int keyc,
        Jim_Obj *newObjPtr);

/* return code object */
JIM_STATIC int JIM_API(Jim_GetReturnCode) (Jim_Interp *interp, Jim_Obj *objPtr,
        int *intPtr);

/* expression object */
JIM_STATIC int JIM_API(Jim_EvalExpression) (Jim_Interp *interp,
        Jim_Obj *exprObjPtr, Jim_Obj **exprResultPtrPtr);
JIM_STATIC int JIM_API(Jim_GetBoolFromExpr) (Jim_Interp *interp,
        Jim_Obj *exprObjPtr, int *boolPtr);

/* integer object */
JIM_STATIC int JIM_API(Jim_GetWide) (Jim_Interp *interp, Jim_Obj *objPtr,
        jim_wide *widePtr);
JIM_STATIC int JIM_API(Jim_GetLong) (Jim_Interp *interp, Jim_Obj *objPtr,
        long *longPtr);
JIM_STATIC void JIM_API(Jim_SetWide) (Jim_Interp *interp, Jim_Obj *objPtr,
        jim_wide wideValue);
JIM_STATIC Jim_Obj * JIM_API(Jim_NewIntObj) (Jim_Interp *interp,
        jim_wide wideValue);

/* shared strings */
JIM_STATIC char JIM_API(*Jim_GetSharedString) (Jim_Interp *interp, char *str);
JIM_STATIC void JIM_API(Jim_ReleaseSharedString) (Jim_Interp *interp, char *str);

/* commands utilities */
JIM_STATIC void JIM_API(Jim_WrongNumArgs) (Jim_Interp *interp, int argc,
        Jim_Obj **argv, char *msg);

/* API import/export functions */
JIM_STATIC void* JIM_API(Jim_GetApi) (Jim_Interp *interp, char *funcname);
JIM_STATIC int JIM_API(Jim_RegisterApi) (Jim_Interp *interp, char *funcname,
        void *funcptr);

/* error messages */
JIM_STATIC void JIM_API(Jim_PrintErrorMessage) (Jim_Interp *interp);

/* interactive mode */
JIM_STATIC int JIM_API(Jim_InteractivePrompt) (void);

/* Misc */
JIM_STATIC void JIM_API(Jim_Panic) (char *fmt, ...);

#ifndef __JIM_CORE__
/* This must be included "inline" inside the extension */
static void Jim_InitExtension(Jim_Interp *interp, char *version)
{
  Jim_GetApi = interp->getApiFuncPtr;
  
  Jim_Alloc = Jim_GetApi(interp, "Jim_Alloc");
  Jim_EvalObj = Jim_GetApi(interp, "Jim_EvalObj");
  Jim_EvalObjVector = Jim_GetApi(interp, "Jim_EvalObjVector");
  Jim_InitHashTable = Jim_GetApi(interp, "Jim_InitHashTable");
  Jim_ExpandHashTable = Jim_GetApi(interp, "Jim_ExpandHashTable");
  Jim_AddHashEntry = Jim_GetApi(interp, "Jim_AddHashEntry");
  Jim_ReplaceHashEntry = Jim_GetApi(interp, "Jim_ReplaceHashEntry");
  Jim_DeleteHashEntry = Jim_GetApi(interp, "Jim_DeleteHashEntry");
  Jim_FreeHashTable = Jim_GetApi(interp, "Jim_FreeHashTable");
  Jim_FindHashEntry = Jim_GetApi(interp, "Jim_FindHashEntry");
  Jim_ResizeHashTable = Jim_GetApi(interp, "Jim_ResizeHashTable");
  Jim_GetHashTableIterator = Jim_GetApi(interp, "Jim_GetHashTableIterator");
  Jim_NextHashEntry = Jim_GetApi(interp, "Jim_NextHashEntry");
  Jim_NewObj = Jim_GetApi(interp, "Jim_NewObj");
  Jim_FreeObj = Jim_GetApi(interp, "Jim_FreeObj");
  Jim_InvalidateStringRep = Jim_GetApi(interp, "Jim_InvalidateStringRep");
  Jim_InitStringRep = Jim_GetApi(interp, "Jim_InitStringRep");
  Jim_DuplicateObj = Jim_GetApi(interp, "Jim_DuplicateObj");
  Jim_GetString = Jim_GetApi(interp, "Jim_GetString");
  Jim_InvalidateStringRep = Jim_GetApi(interp, "Jim_InvalidateStringRep");
  Jim_NewStringObj = Jim_GetApi(interp, "Jim_NewStringObj");
  Jim_NewStringObjNoAlloc = Jim_GetApi(interp, "Jim_NewStringObjNoAlloc");
  Jim_AppendString = Jim_GetApi(interp, "Jim_AppendString");
  Jim_AppendObj = Jim_GetApi(interp, "Jim_AppendObj");
  Jim_AppendStrings = Jim_GetApi(interp, "Jim_AppendStrings");
  Jim_StringEqObj = Jim_GetApi(interp, "Jim_StringEqObj");
  Jim_StringMatchObj = Jim_GetApi(interp, "Jim_StringMatchObj");
  Jim_StringRangeObj = Jim_GetApi(interp, "Jim_StringRangeObj");
  Jim_CompareStringImmediate = Jim_GetApi(interp, "Jim_CompareStringImmediate");
  Jim_NewReference = Jim_GetApi(interp, "Jim_NewReference");
  Jim_GetReference = Jim_GetApi(interp, "Jim_GetReference");
  Jim_CreateInterp = Jim_GetApi(interp, "Jim_CreateInterp");
  Jim_FreeInterp = Jim_GetApi(interp, "Jim_FreeInterp");
  Jim_RegisterCoreCommands = Jim_GetApi(interp, "Jim_RegisterCoreCommands");
  Jim_CreateCommand = Jim_GetApi(interp, "Jim_CreateCommand");
  Jim_CreateProcedure = Jim_GetApi(interp, "Jim_CreateProcedure");
  Jim_DeleteCommand = Jim_GetApi(interp, "Jim_DeleteCommand");
  Jim_RenameCommand = Jim_GetApi(interp, "Jim_RenameCommand");
  Jim_GetCommand = Jim_GetApi(interp, "Jim_GetCommand");
  Jim_SetVariable = Jim_GetApi(interp, "Jim_SetVariable");
  Jim_SetVariableLink = Jim_GetApi(interp, "Jim_SetVariableLink");
  Jim_GetVariable = Jim_GetApi(interp, "Jim_GetVariable");
  Jim_UnsetVariable = Jim_GetApi(interp, "Jim_UnsetVariable");
  Jim_GetCallFrameByLevel = Jim_GetApi(interp, "Jim_GetCallFrameByLevel");
  Jim_Collect = Jim_GetApi(interp, "Jim_Collect");
  Jim_CollectIfNeeded = Jim_GetApi(interp, "Jim_CollectIfNeeded");
  Jim_GetIndex = Jim_GetApi(interp, "Jim_GetIndex");
  Jim_NewListObj = Jim_GetApi(interp, "Jim_NewListObj");
  Jim_ListAppendElement = Jim_GetApi(interp, "Jim_ListAppendElement");
  Jim_ListAppendList = Jim_GetApi(interp, "Jim_ListAppendList");
  Jim_ListLength = Jim_GetApi(interp, "Jim_ListLength");
  Jim_ListIndex = Jim_GetApi(interp, "Jim_ListIndex");
  Jim_SetListIndex = Jim_GetApi(interp, "Jim_SetListIndex");
  Jim_ConcatObj = Jim_GetApi(interp, "Jim_ConcatObj");
  Jim_NewDictObj = Jim_GetApi(interp, "Jim_NewDictObj");
  Jim_DictKey = Jim_GetApi(interp, "Jim_DictKey");
  Jim_DictKeysVector = Jim_GetApi(interp, "Jim_DictKeysVector");
  Jim_GetIndex = Jim_GetApi(interp, "Jim_GetIndex");
  Jim_GetReturnCode = Jim_GetApi(interp, "Jim_GetReturnCode");
  Jim_EvalExpression = Jim_GetApi(interp, "Jim_EvalExpression");
  Jim_GetBoolFromExpr = Jim_GetApi(interp, "Jim_GetBoolFromExpr");
  Jim_GetWide = Jim_GetApi(interp, "Jim_GetWide");
  Jim_GetLong = Jim_GetApi(interp, "Jim_GetLong");
  Jim_SetWide = Jim_GetApi(interp, "Jim_SetWide");
  Jim_NewIntObj = Jim_GetApi(interp, "Jim_NewIntObj");
  Jim_WrongNumArgs = Jim_GetApi(interp, "Jim_WrongNumArgs");
  Jim_SetDictKeysVector = Jim_GetApi(interp, "Jim_SetDictKeysVector");
  Jim_SubstObj = Jim_GetApi(interp, "Jim_SubstObj");
  Jim_RegisterApi = Jim_GetApi(interp, "Jim_RegisterApi");
  Jim_PrintErrorMessage = Jim_GetApi(interp, "Jim_RegisterApi");
  Jim_InteractivePrompt = Jim_GetApi(interp, "Jim_InteractivePrompt");
  Jim_SetResultString(interp, version, -1);
}
#endif /* __JIM_CORE__ */

#endif /* __JIM__H */
