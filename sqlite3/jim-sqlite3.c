/* Jim Tcl version of the sqlite3 Tcl binding.
 * From sqlite3 3.6.22
 *
 * This version is (c) Steve Bennett <steveb@workware.net.au>
 * Copyright of the original version is below.
 */

/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** A TCL Interface to SQLite.  Append this file to sqlite3.c and
** compile the whole thing to build a TCL-enabled version of SQLite.
**
** Compile-time options:
**
**  -D SQLITE_TEST         When used in conjuction with -DTCLSH=1, add
**                        hundreds of new commands used for testing
**                        SQLite.  This option implies -DSQLITE_TCLMD5.
*/
#include <jim.h>
#include <jim-config.h>
#include <jim-eventloop.h>
#include <errno.h>

/*
** Some additional include files are needed if this file is not
** appended to the amalgamation.
*/
#ifndef SQLITE_AMALGAMATION
# include "sqlite3.h"
# include <stdlib.h>
# include <string.h>
# include <assert.h>
  typedef unsigned char u8;
#endif
#include <ctype.h>

#define NUM_PREPARED_STMTS 10
#define MAX_PREPARED_STMTS 100

/*
** If Jim Tcl uses UTF-8 and SQLite is configured to use iso8859, then we
#ifdef JIM_UTF8
#define SQLITE_UTF8
#endif

** have to do a translation when going between the two.  Set the 
** UTF_TRANSLATION_NEEDED macro to indicate that we need to do
** this translation.  
*/
#if defined(JIM_UTF8) && !defined(SQLITE_UTF8)
# define UTF_TRANSLATION_NEEDED 1
# warning Jim Tcl can not translate encoding from iso8859 to utf-8
#endif

/*
** New SQL functions can be created as TCL scripts.  Each such function
** is described by an instance of the following structure.
*/
typedef struct SqlFunc SqlFunc;
struct SqlFunc {
  Jim_Interp *interp;   /* The TCL interpret to execute the function */
  Jim_Obj *pScript;     /* The Jim_Obj representation of the script */
  int useEvalObjv;      /* True if it is safe to use Jim_EvalObjv */
  char *zName;          /* Name of this function */
  SqlFunc *pNext;       /* Next function on the list of them all */
};

/*
** New collation sequences function can be created as TCL scripts.  Each such
** function is described by an instance of the following structure.
*/
typedef struct SqlCollate SqlCollate;
struct SqlCollate {
  Jim_Interp *interp;   /* The TCL interpret to execute the function */
  char *zScript;        /* The script to be run */
  SqlCollate *pNext;    /* Next function on the list of them all */
};

/*
** Prepared statements are cached for faster execution.  Each prepared
** statement is described by an instance of the following structure.
*/
typedef struct SqlPreparedStmt SqlPreparedStmt;
struct SqlPreparedStmt {
  SqlPreparedStmt *pNext;  /* Next in linked list */
  SqlPreparedStmt *pPrev;  /* Previous on the list */
  sqlite3_stmt *pStmt;     /* The prepared statement */
  int nSql;                /* chars in zSql[] */
  const char *zSql;        /* Text of the SQL statement */
  int nParm;               /* Size of apParm array */
  Jim_Obj **apParm;        /* Array of referenced object pointers */
};

typedef struct IncrblobChannel IncrblobChannel;

/*
** There is one instance of this structure for each SQLite database
** that has been opened by the SQLite TCL interface.
*/
typedef struct SqliteDb SqliteDb;
struct SqliteDb {
  sqlite3 *db;               /* The "real" database structure. MUST BE FIRST */
  Jim_Interp *interp;        /* The interpreter used for this database */
  char *zBusy;               /* The busy callback routine */
  char *zCommit;             /* The commit hook callback routine */
  char *zTrace;              /* The trace callback routine */
  char *zProfile;            /* The profile callback routine */
  char *zProgress;           /* The progress callback routine */
  char *zAuth;               /* The authorization callback routine */
  int disableAuth;           /* Disable the authorizer if it exists */
  char *zNull;               /* Text to substitute for an SQL NULL value */
  SqlFunc *pFunc;            /* List of SQL functions */
  Jim_Obj *pUpdateHook;      /* Update hook script (if any) */
  Jim_Obj *pRollbackHook;    /* Rollback hook script (if any) */
  Jim_Obj *pUnlockNotify;    /* Unlock notify script (if any) */
  SqlCollate *pCollate;      /* List of SQL collation functions */
  int rc;                    /* Return code of most recent sqlite3_exec() */
  Jim_Obj *pCollateNeeded;   /* Collation needed script */
  SqlPreparedStmt *stmtList; /* List of prepared statements*/
  SqlPreparedStmt *stmtLast; /* Last statement in the list */
  int maxStmt;               /* The next maximum number of stmtList */
  int nStmt;                 /* Number of statements in stmtList */
  IncrblobChannel *pIncrblob;/* Linked list of open incrblob channels */
  int nStep, nSort;          /* Statistics for most recent operation */
  int nTransaction;          /* Number of nested [transaction] methods */
};

struct IncrblobChannel {
  sqlite3_blob *pBlob;      /* sqlite3 blob handle */
  SqliteDb *pDb;            /* Associated database connection */
  int iSeek;                /* Current seek offset */
  Jim_Obj *channel;         /* Channel identifier */
  IncrblobChannel *pNext;   /* Linked list of all open incrblob channels */
  IncrblobChannel *pPrev;   /* Linked list of all open incrblob channels */
};

/*
** Compute a string length that is limited to what can be stored in
** lower 30 bits of a 32-bit signed integer.
*/
static int strlen30(const char *z){
  const char *z2 = z;
  while( *z2 ){ z2++; }
  return 0x3fffffff & (int)(z2 - z);
}


#ifndef SQLITE_OMIT_INCRBLOB
/*
** Close all incrblob channels opened using database connection pDb.
** This is called when shutting down the database connection.
*/
static void closeIncrblobChannels(SqliteDb *pDb){
  IncrblobChannel *p;
  IncrblobChannel *pNext;

  for(p=pDb->pIncrblob; p; p=pNext){
    pNext = p->pNext;

    /* Note: Calling unregister here call Jim_Close on the incrblob channel, 
    ** which deletes the IncrblobChannel structure at *p. So do not
    ** call Jim_Free() here.
    */
    Jim_UnregisterChannel(pDb->interp, p->channel);
  }
}

/*
** Close an incremental blob channel.
*/
static int incrblobClose(ClientData instanceData, Jim_Interp *interp){
  IncrblobChannel *p = (IncrblobChannel *)instanceData;
  int rc = sqlite3_blob_close(p->pBlob);
  sqlite3 *db = p->pDb->db;

  /* Remove the channel from the SqliteDb.pIncrblob list. */
  if( p->pNext ){
    p->pNext->pPrev = p->pPrev;
  }
  if( p->pPrev ){
    p->pPrev->pNext = p->pNext;
  }
  if( p->pDb->pIncrblob==p ){
    p->pDb->pIncrblob = p->pNext;
  }

  /* Free the IncrblobChannel structure */
  Jim_Free((char *)p);

  if( rc!=SQLITE_OK ){
    Jim_SetResult(interp, (char *)sqlite3_errmsg(db), JIM_VOLATILE);
    return JIM_ERR;
  }
  return JIM_OK;
}

/*
** Read data from an incremental blob channel.
*/
static int incrblobInput(
  ClientData instanceData, 
  char *buf, 
  int bufSize,
  int *errorCodePtr
){
  IncrblobChannel *p = (IncrblobChannel *)instanceData;
  int nRead = bufSize;         /* Number of bytes to read */
  int nBlob;                   /* Total size of the blob */
  int rc;                      /* sqlite error code */

  nBlob = sqlite3_blob_bytes(p->pBlob);
  if( (p->iSeek+nRead)>nBlob ){
    nRead = nBlob-p->iSeek;
  }
  if( nRead<=0 ){
    return 0;
  }

  rc = sqlite3_blob_read(p->pBlob, (void *)buf, nRead, p->iSeek);
  if( rc!=SQLITE_OK ){
    *errorCodePtr = rc;
    return -1;
  }

  p->iSeek += nRead;
  return nRead;
}

/*
** Write data to an incremental blob channel.
*/
static int incrblobOutput(
  ClientData instanceData, 
  CONST char *buf, 
  int toWrite,
  int *errorCodePtr
){
  IncrblobChannel *p = (IncrblobChannel *)instanceData;
  int nWrite = toWrite;        /* Number of bytes to write */
  int nBlob;                   /* Total size of the blob */
  int rc;                      /* sqlite error code */

  nBlob = sqlite3_blob_bytes(p->pBlob);
  if( (p->iSeek+nWrite)>nBlob ){
    *errorCodePtr = EINVAL;
    return -1;
  }
  if( nWrite<=0 ){
    return 0;
  }

  rc = sqlite3_blob_write(p->pBlob, (void *)buf, nWrite, p->iSeek);
  if( rc!=SQLITE_OK ){
    *errorCodePtr = EIO;
    return -1;
  }

  p->iSeek += nWrite;
  return nWrite;
}

/*
** Seek an incremental blob channel.
*/
static int incrblobSeek(
  ClientData instanceData, 
  long offset,
  int seekMode,
  int *errorCodePtr
){
  IncrblobChannel *p = (IncrblobChannel *)instanceData;

  switch( seekMode ){
    case SEEK_SET:
      p->iSeek = offset;
      break;
    case SEEK_CUR:
      p->iSeek += offset;
      break;
    case SEEK_END:
      p->iSeek = sqlite3_blob_bytes(p->pBlob) + offset;
      break;

    default: assert(!"Bad seekMode");
  }

  return p->iSeek;
}


static void incrblobWatch(ClientData instanceData, int mode){ 
  /* NO-OP */ 
}
static int incrblobHandle(ClientData instanceData, int dir, ClientData *hPtr){
  return JIM_ERR;
}

static Jim_ChannelType IncrblobChannelType = {
  "incrblob",                        /* typeName                             */
  JIM_CHANNEL_VERSION_2,             /* version                              */
  incrblobClose,                     /* closeProc                            */
  incrblobInput,                     /* inputProc                            */
  incrblobOutput,                    /* outputProc                           */
  incrblobSeek,                      /* seekProc                             */
  0,                                 /* setOptionProc                        */
  0,                                 /* getOptionProc                        */
  incrblobWatch,                     /* watchProc (this is a no-op)          */
  incrblobHandle,                    /* getHandleProc (always returns error) */
  0,                                 /* close2Proc                           */
  0,                                 /* blockModeProc                        */
  0,                                 /* flushProc                            */
  0,                                 /* handlerProc                          */
  0,                                 /* wideSeekProc                         */
};

/*
** Create a new incrblob channel.
*/
static int createIncrblobChannel(
  Jim_Interp *interp, 
  SqliteDb *pDb, 
  const char *zDb,
  const char *zTable, 
  const char *zColumn, 
  sqlite_int64 iRow,
  int isReadonly
){
  IncrblobChannel *p;
  sqlite3 *db = pDb->db;
  sqlite3_blob *pBlob;
  int rc;
  int flags = JIM_READABLE|(isReadonly ? 0 : JIM_WRITABLE);

  /* This variable is used to name the channels: "incrblob_[incr count]" */
  static int count = 0;
  char zChannel[64];

  rc = sqlite3_blob_open(db, zDb, zTable, zColumn, iRow, !isReadonly, &pBlob);
  if( rc!=SQLITE_OK ){
    Jim_SetResult(interp, (char *)sqlite3_errmsg(pDb->db), JIM_VOLATILE);
    return JIM_ERR;
  }

  p = (IncrblobChannel *)Jim_Alloc(sizeof(IncrblobChannel));
  p->iSeek = 0;
  p->pBlob = pBlob;

  sqlite3_snprintf(sizeof(zChannel), zChannel, "incrblob_%d", ++count);
  p->channel = Jim_CreateChannel(&IncrblobChannelType, zChannel, p, flags);
  Jim_RegisterChannel(interp, p->channel);

  /* Link the new channel into the SqliteDb.pIncrblob list. */
  p->pNext = pDb->pIncrblob;
  p->pPrev = 0;
  if( p->pNext ){
    p->pNext->pPrev = p;
  }
  pDb->pIncrblob = p;
  p->pDb = pDb;

  Jim_SetResult(interp, (char *)Jim_GetChannelName(p->channel), JIM_VOLATILE);
  return JIM_OK;
}
#else  /* else clause for "#ifndef SQLITE_OMIT_INCRBLOB" */
  #define closeIncrblobChannels(pDb)
#endif

/*
** Look at the script prefix in pCmd.  We will be executing this script
** after first appending one or more arguments.  This routine analyzes
** the script to see if it is safe to use Jim_EvalObjv() on the script
** rather than the more general Jim_EvalEx().  Jim_EvalObjv() is much
** faster.
**
** Scripts that are safe to use with Jim_EvalObjv() consists of a
** command name followed by zero or more arguments with no [...] or $
** or {...} or ; to be seen anywhere.  Most callback scripts consist
** of just a single procedure name and they meet this requirement.
*/
static int safeToUseEvalObjv(Jim_Interp *interp, Jim_Obj *pCmd){
  /* We could try to do something with Jim_Parse().  But we will instead
  ** just do a search for forbidden characters.  If any of the forbidden
  ** characters appear in pCmd, we will report the string as unsafe.
  */
  const char *z;
  int n;
  z = Jim_GetString(pCmd, &n);
  while( n-- > 0 ){
    int c = *(z++);
    if( c=='$' || c=='[' || c==';' ) return 0;
  }
  return 1;
}

/*
** Find an SqlFunc structure with the given name.  Or create a new
** one if an existing one cannot be found.  Return a pointer to the
** structure.
*/
static SqlFunc *findSqlFunc(SqliteDb *pDb, const char *zName){
  SqlFunc *p, *pNew;
  int i;
  pNew = (SqlFunc*)Jim_Alloc( sizeof(*pNew) + strlen30(zName) + 1 );
  pNew->zName = (char*)&pNew[1];
  for(i=0; zName[i]; i++){ pNew->zName[i] = tolower((unsigned)zName[i]); }
  pNew->zName[i] = 0;
  for(p=pDb->pFunc; p; p=p->pNext){ 
    if( strcmp(p->zName, pNew->zName)==0 ){
      Jim_Free((char*)pNew);
      return p;
    }
  }
  pNew->interp = pDb->interp;
  pNew->pScript = 0;
  pNew->pNext = pDb->pFunc;
  pDb->pFunc = pNew;
  return pNew;
}

/*
** Finalize and free a list of prepared statements
*/
static void flushStmtCache( SqliteDb *pDb ){
  SqlPreparedStmt *pPreStmt;

  while(  pDb->stmtList ){
    sqlite3_finalize( pDb->stmtList->pStmt );
    pPreStmt = pDb->stmtList;
    pDb->stmtList = pDb->stmtList->pNext;
    Jim_Free( (char*)pPreStmt );
  }
  pDb->nStmt = 0;
  pDb->stmtLast = 0;
}

/*
** TCL calls this procedure when an sqlite3 database command is
** deleted.
*/
static void DbDeleteCmd(Jim_Interp *interp, void *db){
  SqliteDb *pDb = (SqliteDb*)db;
  flushStmtCache(pDb);
  closeIncrblobChannels(pDb);
  sqlite3_close(pDb->db);
  while( pDb->pFunc ){
    SqlFunc *pFunc = pDb->pFunc;
    pDb->pFunc = pFunc->pNext;
    Jim_DecrRefCount(interp, pFunc->pScript);
    Jim_Free((char*)pFunc);
  }
  while( pDb->pCollate ){
    SqlCollate *pCollate = pDb->pCollate;
    pDb->pCollate = pCollate->pNext;
    Jim_Free((char*)pCollate);
  }
  if( pDb->zBusy ){
    Jim_Free(pDb->zBusy);
  }
  if( pDb->zTrace ){
    Jim_Free(pDb->zTrace);
  }
  if( pDb->zProfile ){
    Jim_Free(pDb->zProfile);
  }
  if( pDb->zAuth ){
    Jim_Free(pDb->zAuth);
  }
  if( pDb->zNull ){
    Jim_Free(pDb->zNull);
  }
  if( pDb->pUpdateHook ){
    Jim_DecrRefCount(interp, pDb->pUpdateHook);
  }
  if( pDb->pRollbackHook ){
    Jim_DecrRefCount(interp, pDb->pRollbackHook);
  }
  if( pDb->pCollateNeeded ){
    Jim_DecrRefCount(interp, pDb->pCollateNeeded);
  }
  Jim_Free((char*)pDb);
}

/*
** This routine is called when a database file is locked while trying
** to execute SQL.
*/
static int DbBusyHandler(void *cd, int nTries){
  SqliteDb *pDb = (SqliteDb*)cd;
  int rc;
  char zVal[30];
  Jim_Obj *objPtr;

  sqlite3_snprintf(sizeof(zVal), zVal, "%d", nTries);

  objPtr = Jim_NewStringObj(pDb->interp, pDb->zBusy, -1);
  Jim_AppendStrings(pDb->interp, objPtr, " ", zVal, NULL);
  rc = Jim_EvalObj(pDb->interp, objPtr);
  if( rc!=JIM_OK || atoi(Jim_String(Jim_GetResult(pDb->interp))) ){
    return 0;
  }
  return 1;
}

#ifndef SQLITE_OMIT_PROGRESS_CALLBACK
/*
** This routine is invoked as the 'progress callback' for the database.
*/
static int DbProgressHandler(void *cd){
  SqliteDb *pDb = (SqliteDb*)cd;
  int rc;

  assert( pDb->zProgress );
  rc = Jim_Eval(pDb->interp, pDb->zProgress);
  if( rc!=JIM_OK || atoi(Jim_String(Jim_GetResult(pDb->interp))) ){
    return 1;
  }
  return 0;
}
#endif

#ifndef SQLITE_OMIT_TRACE
/*
** This routine is called by the SQLite trace handler whenever a new
** block of SQL is executed.  The TCL script in pDb->zTrace is executed.
*/
static void DbTraceHandler(void *cd, const char *zSql){
  SqliteDb *pDb = (SqliteDb*)cd;

  Jim_Obj *str = Jim_NewStringObj(pDb->interp, pDb->zTrace, -1);
  Jim_ListAppendElement(pDb->interp, str, Jim_NewStringObj(pDb->interp, zSql, -1));
  Jim_Eval(pDb->interp, zSql);
  Jim_SetEmptyResult(pDb->interp);
}
#endif

#ifndef SQLITE_OMIT_TRACE
/*
** This routine is called by the SQLite profile handler after a statement
** SQL has executed.  The TCL script in pDb->zProfile is evaluated.
*/
static void DbProfileHandler(void *cd, const char *zSql, sqlite_uint64 tm){
  SqliteDb *pDb = (SqliteDb*)cd;
  Jim_Obj *str;
  char zTm[100];

  sqlite3_snprintf(sizeof(zTm)-1, zTm, "%lld", tm);
  str = Jim_NewStringObj(pDb->interp, pDb->zProfile, -1);
  Jim_ListAppendElement(pDb->interp, str, Jim_NewStringObj(pDb->interp, zSql, -1));
  Jim_ListAppendElement(pDb->interp, str, Jim_NewStringObj(pDb->interp, zTm, -1));
  Jim_EvalObj(pDb->interp, str);
  Jim_SetEmptyResult(pDb->interp);
}
#endif

/*
** This routine is called when a transaction is committed.  The
** TCL script in pDb->zCommit is executed.  If it returns non-zero or
** if it throws an exception, the transaction is rolled back instead
** of being committed.
*/
static int DbCommitHandler(void *cd){
  SqliteDb *pDb = (SqliteDb*)cd;
  int rc;

  rc = Jim_Eval(pDb->interp, pDb->zCommit);
  if( rc!=JIM_OK || atoi(Jim_String(Jim_GetResult(pDb->interp))) ){
    return 1;
  }
  return 0;
}

static void DbRollbackHandler(void *clientData){
  SqliteDb *pDb = (SqliteDb*)clientData;
  assert(pDb->pRollbackHook);
  Jim_EvalObjBackground(pDb->interp, pDb->pRollbackHook);
}

#if defined(SQLITE_TEST) && defined(SQLITE_ENABLE_UNLOCK_NOTIFY)
static void setTestUnlockNotifyVars(Jim_Interp *interp, int iArg, int nArg){
  char zBuf[64];
  sprintf(zBuf, "%d", iArg);
  Jim_SetVar(interp, "sqlite_unlock_notify_arg", zBuf, JIM_GLOBAL_ONLY);
  sprintf(zBuf, "%d", nArg);
  Jim_SetVar(interp, "sqlite_unlock_notify_argcount", zBuf, JIM_GLOBAL_ONLY);
}
#else
# define setTestUnlockNotifyVars(x,y,z)
#endif

#ifdef SQLITE_ENABLE_UNLOCK_NOTIFY
static void DbUnlockNotify(void **apArg, int nArg){
  int i;
  for(i=0; i<nArg; i++){
    const int flags = (JIM_EVAL_GLOBAL|JIM_EVAL_DIRECT);
    SqliteDb *pDb = (SqliteDb *)apArg[i];
    setTestUnlockNotifyVars(pDb->interp, i, nArg);
    assert( pDb->pUnlockNotify);
    Jim_EvalObjEx(pDb->interp, pDb->pUnlockNotify, flags);
    Jim_DecrRefCount(interp, pDb->pUnlockNotify);
    pDb->pUnlockNotify = 0;
  }
}
#endif

static void DbUpdateHandler(
  void *p, 
  int op,
  const char *zDb, 
  const char *zTbl, 
  sqlite_int64 rowid
){
  SqliteDb *pDb = (SqliteDb *)p;
  Jim_Obj *pCmd;

  assert( pDb->pUpdateHook );
  assert( op==SQLITE_INSERT || op==SQLITE_UPDATE || op==SQLITE_DELETE );

  pCmd = Jim_DuplicateObj(pDb->interp, pDb->pUpdateHook);
  Jim_IncrRefCount(pCmd);
  Jim_ListAppendElement(0, pCmd, Jim_NewStringObj(pDb->interp, 
    ( (op==SQLITE_INSERT)?"INSERT":(op==SQLITE_UPDATE)?"UPDATE":"DELETE"), -1));
  Jim_ListAppendElement(pDb->interp, pCmd, Jim_NewStringObj(pDb->interp, zDb, -1));
  Jim_ListAppendElement(pDb->interp, pCmd, Jim_NewStringObj(pDb->interp, zTbl, -1));
  Jim_ListAppendElement(pDb->interp, pCmd, Jim_NewIntObj(pDb->interp, rowid));
  Jim_EvalObj(pDb->interp, pCmd);
}

static void tclCollateNeeded(
  void *pCtx,
  sqlite3 *db,
  int enc,
  const char *zName
){
  SqliteDb *pDb = (SqliteDb *)pCtx;
  Jim_Obj *pScript = Jim_DuplicateObj(pDb->interp, pDb->pCollateNeeded);
  //Jim_IncrRefCount(pScript);
  Jim_ListAppendElement(pDb->interp, pScript, Jim_NewStringObj(pDb->interp, zName, -1));
  Jim_EvalObj(pDb->interp, pScript);
  //Jim_DecrRefCount(pDb->interp, pScript);
}

/*
** This routine is called to evaluate an SQL collation function implemented
** using TCL script.
*/
static int tclSqlCollate(
  void *pCtx,
  int nA,
  const void *zA,
  int nB,
  const void *zB
){
  SqlCollate *p = (SqlCollate *)pCtx;
  Jim_Obj *pCmd;

  pCmd = Jim_NewStringObj(p->interp, p->zScript, -1);
  //Jim_IncrRefCount(pCmd);
  Jim_ListAppendElement(p->interp, pCmd, Jim_NewStringObj(p->interp, zA, nA));
  Jim_ListAppendElement(p->interp, pCmd, Jim_NewStringObj(p->interp, zB, nB));
  Jim_EvalObj(p->interp, pCmd);
  //Jim_DecrRefCount(interp, pCmd);
  return (atoi(Jim_String(Jim_GetResult(p->interp))));
}

/*
** This routine is called to evaluate an SQL function implemented
** using TCL script.
*/
static void tclSqlFunc(sqlite3_context *context, int argc, sqlite3_value**argv){
  SqlFunc *p = sqlite3_user_data(context);
  Jim_Obj *pCmd;
  int i;
  int rc;

  if( argc==0 ){
    /* If there are no arguments to the function, call Jim_EvalObjEx on the
    ** script object directly.  This allows the TCL compiler to generate
    ** bytecode for the command on the first invocation and thus make
    ** subsequent invocations much faster. */
    pCmd = p->pScript;
    //Jim_IncrRefCount(pCmd);
    rc = Jim_EvalObj(p->interp, pCmd);
    //Jim_DecrRefCount(interp, pCmd);
  }else{
    /* If there are arguments to the function, make a shallow copy of the
    ** script object, lappend the arguments, then evaluate the copy.
    **
    ** By "shallow" copy, we mean a only the outer list Jim_Obj is duplicated.
    ** The new Jim_Obj contains pointers to the original list elements. 
    ** That way, when Jim_EvalObjv() is run and shimmers the first element
    ** of the list to tclCmdNameType, that alternate representation will
    ** be preserved and reused on the next invocation.
    */
    pCmd = Jim_DuplicateObj(p->interp, p->pScript);
    Jim_IncrRefCount(pCmd);
    for(i=0; i<argc; i++){
      sqlite3_value *pIn = argv[i];
      Jim_Obj *pVal;
            
      /* Set pVal to contain the i'th column of this row. */
      switch( sqlite3_value_type(pIn) ){
        case SQLITE_BLOB: {
          int bytes = sqlite3_value_bytes(pIn);
          pVal = Jim_NewStringObj(p->interp, sqlite3_value_blob(pIn), bytes);
          break;
        }
        case SQLITE_INTEGER: {
          sqlite_int64 v = sqlite3_value_int64(pIn);
          pVal = Jim_NewIntObj(p->interp, v);
          break;
        }
        case SQLITE_FLOAT: {
          double r = sqlite3_value_double(pIn);
          pVal = Jim_NewDoubleObj(p->interp, r);
          break;
        }
        case SQLITE_NULL: {
          pVal = Jim_NewStringObj(p->interp, "", 0);
          break;
        }
        default: {
          int bytes = sqlite3_value_bytes(pIn);
          pVal = Jim_NewStringObj(p->interp, (char *)sqlite3_value_text(pIn), bytes);
          break;
        }
      }
      Jim_ListAppendElement(p->interp, pCmd, pVal);
    }
    if( !p->useEvalObjv ){
      /* Jim_EvalOb() will automatically call Jim_EvalObjVector() if pCmd
      ** is a list without a string representation.  To prevent this from
      ** happening, make sure pCmd has a valid string representation */
      Jim_String(pCmd);
    }
    rc = Jim_EvalObj(p->interp, pCmd);
    Jim_DecrRefCount(p->interp, pCmd);
  }

  if( rc && rc!=JIM_RETURN ){
    sqlite3_result_error(context, Jim_String(Jim_GetResult(p->interp)), -1); 
  }else{
    Jim_Obj *pVar = Jim_GetResult(p->interp);
    int n;
    u8 *data;
    /* XXX: Jim Tcl doesn't have bytearray or boolean */
    const char *zType = (pVar->typePtr ? pVar->typePtr->name : "");
    char c = zType[0];
#if 0
    if( c=='b' && strcmp(zType,"bytearray")==0 && pVar->bytes==0 ){
      /* Only return a BLOB type if the Tcl variable is a bytearray and
      ** has no string representation. */
      data = Jim_GetByteArrayFromObj(pVar, &n);
      sqlite3_result_blob(context, data, n, SQLITE_TRANSIENT);
    }else if( c=='b' && strcmp(zType,"boolean")==0 ){
      Jim_GetWide(0, pVar, &n);
      sqlite3_result_int(context, n);
    }else
#endif
    if( c=='d' && strcmp(zType,"double")==0 ){
      double r;
      Jim_GetDouble(0, pVar, &r);
      sqlite3_result_double(context, r);
      /* XXX: Is a cooerced double better as a double or an int? */
    }else if( (c=='c' && strcmp(zType,"coerced-double")==0) ||
          (c=='i' && strcmp(zType,"int")==0) ){
      jim_wide v;
      Jim_GetWide(p->interp, pVar, &v);
      sqlite3_result_int64(context, v);
    }else{
      data = (unsigned char *)Jim_GetString(pVar, &n);
      sqlite3_result_text(context, (char *)data, n, SQLITE_TRANSIENT);
    }
  }
}

#ifndef SQLITE_OMIT_AUTHORIZATION
/*
** This is the authentication function.  It appends the authentication
** type code and the two arguments to zCmd[] then invokes the result
** on the interpreter.  The reply is examined to determine if the
** authentication fails or succeeds.
*/
static int auth_callback(
  void *pArg,
  int code,
  const char *zArg1,
  const char *zArg2,
  const char *zArg3,
  const char *zArg4
){
  char *zCode;
  Jim_Obj *str;
  int rc;
  const char *zReply;
  SqliteDb *pDb = (SqliteDb*)pArg;
  if( pDb->disableAuth ) return SQLITE_OK;

  switch( code ){
    case SQLITE_COPY              : zCode="SQLITE_COPY"; break;
    case SQLITE_CREATE_INDEX      : zCode="SQLITE_CREATE_INDEX"; break;
    case SQLITE_CREATE_TABLE      : zCode="SQLITE_CREATE_TABLE"; break;
    case SQLITE_CREATE_TEMP_INDEX : zCode="SQLITE_CREATE_TEMP_INDEX"; break;
    case SQLITE_CREATE_TEMP_TABLE : zCode="SQLITE_CREATE_TEMP_TABLE"; break;
    case SQLITE_CREATE_TEMP_TRIGGER: zCode="SQLITE_CREATE_TEMP_TRIGGER"; break;
    case SQLITE_CREATE_TEMP_VIEW  : zCode="SQLITE_CREATE_TEMP_VIEW"; break;
    case SQLITE_CREATE_TRIGGER    : zCode="SQLITE_CREATE_TRIGGER"; break;
    case SQLITE_CREATE_VIEW       : zCode="SQLITE_CREATE_VIEW"; break;
    case SQLITE_DELETE            : zCode="SQLITE_DELETE"; break;
    case SQLITE_DROP_INDEX        : zCode="SQLITE_DROP_INDEX"; break;
    case SQLITE_DROP_TABLE        : zCode="SQLITE_DROP_TABLE"; break;
    case SQLITE_DROP_TEMP_INDEX   : zCode="SQLITE_DROP_TEMP_INDEX"; break;
    case SQLITE_DROP_TEMP_TABLE   : zCode="SQLITE_DROP_TEMP_TABLE"; break;
    case SQLITE_DROP_TEMP_TRIGGER : zCode="SQLITE_DROP_TEMP_TRIGGER"; break;
    case SQLITE_DROP_TEMP_VIEW    : zCode="SQLITE_DROP_TEMP_VIEW"; break;
    case SQLITE_DROP_TRIGGER      : zCode="SQLITE_DROP_TRIGGER"; break;
    case SQLITE_DROP_VIEW         : zCode="SQLITE_DROP_VIEW"; break;
    case SQLITE_INSERT            : zCode="SQLITE_INSERT"; break;
    case SQLITE_PRAGMA            : zCode="SQLITE_PRAGMA"; break;
    case SQLITE_READ              : zCode="SQLITE_READ"; break;
    case SQLITE_SELECT            : zCode="SQLITE_SELECT"; break;
    case SQLITE_TRANSACTION       : zCode="SQLITE_TRANSACTION"; break;
    case SQLITE_UPDATE            : zCode="SQLITE_UPDATE"; break;
    case SQLITE_ATTACH            : zCode="SQLITE_ATTACH"; break;
    case SQLITE_DETACH            : zCode="SQLITE_DETACH"; break;
    case SQLITE_ALTER_TABLE       : zCode="SQLITE_ALTER_TABLE"; break;
    case SQLITE_REINDEX           : zCode="SQLITE_REINDEX"; break;
    case SQLITE_ANALYZE           : zCode="SQLITE_ANALYZE"; break;
    case SQLITE_CREATE_VTABLE     : zCode="SQLITE_CREATE_VTABLE"; break;
    case SQLITE_DROP_VTABLE       : zCode="SQLITE_DROP_VTABLE"; break;
    case SQLITE_FUNCTION          : zCode="SQLITE_FUNCTION"; break;
    case SQLITE_SAVEPOINT         : zCode="SQLITE_SAVEPOINT"; break;
    default                       : zCode="????"; break;
  }
  str = Jim_NewStringObj(pDb->interp, pDb->zAuth, -1);
  /* XXX: list or string here? */
  Jim_ListAppendElement(pDb->interp, str, Jim_NewStringObj(pDb->interp, zCode, -1));
  if (zArg1) {
    Jim_ListAppendElement(pDb->interp, str, Jim_NewStringObj(pDb->interp, zArg1, -1));
  }
  if (zArg2) {
    Jim_ListAppendElement(pDb->interp, str, Jim_NewStringObj(pDb->interp, zArg2, -1));
  }
  if (zArg3) {
    Jim_ListAppendElement(pDb->interp, str, Jim_NewStringObj(pDb->interp, zArg3, -1));
  }
  if (zArg4) {
    Jim_ListAppendElement(pDb->interp, str, Jim_NewStringObj(pDb->interp, zArg4, -1));
  }
  Jim_IncrRefCount(str);
  rc = Jim_EvalGlobal(pDb->interp, Jim_String(str));
  Jim_DecrRefCount(pDb->interp, str);
  zReply = Jim_String(Jim_GetResult(pDb->interp));
  if( strcmp(zReply,"SQLITE_OK")==0 ){
    rc = SQLITE_OK;
  }else if( strcmp(zReply,"SQLITE_DENY")==0 ){
    rc = SQLITE_DENY;
  }else if( strcmp(zReply,"SQLITE_IGNORE")==0 ){
    rc = SQLITE_IGNORE;
  }else{
    rc = 999;
  }
  return rc;
}
#endif /* SQLITE_OMIT_AUTHORIZATION */

/*
** Note that Jim Tcl can't do encoding conversion,
** so this simply returns the string as an object.
*/
static Jim_Obj *dbTextToObj(Jim_Interp *interp, char const *zText){
  return Jim_NewStringObj(interp, zText ? zText : "", -1);
}

/*
** This routine reads a line of text from FILE in, stores
** the text in memory obtained from malloc() and returns a pointer
** to the text.  NULL is returned at end of file.
**
** The interface is like "readline" but no command-line editing
** is done.
**
** copied from shell.c from '.import' command
*/
static char *local_getline(char *zPrompt, FILE *in){
  char *zLine;
  int nLine;
  int n;
  int eol;

  nLine = 100;
  zLine = Jim_Alloc( nLine );
  n = 0;
  eol = 0;
  while( !eol ){
    if( n+100>nLine ){
      nLine = nLine*2 + 100;
      zLine = realloc(zLine, nLine);
      if( zLine==0 ) return 0;
    }
    if( fgets(&zLine[n], nLine - n, in)==0 ){
      if( n==0 ){
        Jim_Free(zLine);
        return 0;
      }
      zLine[n] = 0;
      eol = 1;
      break;
    }
    while( zLine[n] ){ n++; }
    if( n>0 && zLine[n-1]=='\n' ){
      n--;
      zLine[n] = 0;
      eol = 1;
    }
  }
  zLine = realloc( zLine, n+1 );
  return zLine;
}


/*
** This function is part of the implementation of the command:
**
**   $db transaction [-deferred|-immediate|-exclusive] SCRIPT
**
** It is invoked after evaluating the script SCRIPT to commit or rollback
** the transaction or savepoint opened by the [transaction] command.
*/
static int DbTransPostCmd(
  Jim_Interp *interp,                  /* Tcl interpreter */
  SqliteDb *pDb,
  int result                           /* Result of evaluating SCRIPT */
){
  static const char *azEnd[] = {
    "RELEASE _tcl_transaction",        /* rc==JIM_ERR, nTransaction!=0 */
    "COMMIT",                          /* rc!=JIM_ERR, nTransaction==0 */
    "ROLLBACK TO _tcl_transaction ; RELEASE _tcl_transaction",
    "ROLLBACK"                         /* rc==JIM_ERR, nTransaction==0 */
  };
  int rc = result;
  const char *zEnd;

  pDb->nTransaction--;
  zEnd = azEnd[(rc==JIM_ERR)*2 + (pDb->nTransaction==0)];

  pDb->disableAuth++;
  if( sqlite3_exec(pDb->db, zEnd, 0, 0, 0) ){
      /* This is a tricky scenario to handle. The most likely cause of an
      ** error is that the exec() above was an attempt to commit the 
      ** top-level transaction that returned SQLITE_BUSY. Or, less likely,
      ** that an IO-error has occured. In either case, throw a Tcl exception
      ** and try to rollback the transaction.
      **
      ** But it could also be that the user executed one or more BEGIN, 
      ** COMMIT, SAVEPOINT, RELEASE or ROLLBACK commands that are confusing
      ** this method's logic. Not clear how this would be best handled.
      */
    if( rc!=JIM_ERR ){
      Jim_AppendString(interp, Jim_GetResult(interp), sqlite3_errmsg(pDb->db), -1);
      rc = JIM_ERR;
    }
    sqlite3_exec(pDb->db, "ROLLBACK", 0, 0, 0);
  }
  pDb->disableAuth--;

  return rc;
}

/*
** Search the cache for a prepared-statement object that implements the
** first SQL statement in the buffer pointed to by parameter zIn. If
** no such prepared-statement can be found, allocate and prepare a new
** one. In either case, bind the current values of the relevant Tcl
** variables to any $var, :var or @var variables in the statement. Before
** returning, set *ppPreStmt to point to the prepared-statement object.
**
** Output parameter *pzOut is set to point to the next SQL statement in
** buffer zIn, or to the '\0' byte at the end of zIn if there is no
** next statement.
**
** If successful, JIM_OK is returned. Otherwise, JIM_ERR is returned
** and an error message loaded into interpreter pDb->interp.
*/
static int dbPrepareAndBind(
  SqliteDb *pDb,                  /* Database object */
  char const *zIn,                /* SQL to compile */
  char const **pzOut,             /* OUT: Pointer to next SQL statement */
  SqlPreparedStmt **ppPreStmt     /* OUT: Object used to cache statement */
){
  const char *zSql = zIn;         /* Pointer to first SQL statement in zIn */
  sqlite3_stmt *pStmt;            /* Prepared statement object */
  SqlPreparedStmt *pPreStmt;      /* Pointer to cached statement */
  int nSql;                       /* Length of zSql in bytes */
  int nVar;                       /* Number of variables in statement */
  int iParm = 0;                  /* Next free entry in apParm */
  int i;
  Jim_Interp *interp = pDb->interp;

  *ppPreStmt = 0;

  /* Trim spaces from the start of zSql and calculate the remaining length. */
  while( isspace((unsigned)zSql[0]) ){ zSql++; }
  nSql = strlen30(zSql);

  for(pPreStmt = pDb->stmtList; pPreStmt; pPreStmt=pPreStmt->pNext){
    int n = pPreStmt->nSql;
    if( nSql>=n 
        && memcmp(pPreStmt->zSql, zSql, n)==0
        && (zSql[n]==0 || zSql[n-1]==';')
    ){
      pStmt = pPreStmt->pStmt;
      *pzOut = &zSql[pPreStmt->nSql];

      /* When a prepared statement is found, unlink it from the
      ** cache list.  It will later be added back to the beginning
      ** of the cache list in order to implement LRU replacement.
      */
      if( pPreStmt->pPrev ){
        pPreStmt->pPrev->pNext = pPreStmt->pNext;
      }else{
        pDb->stmtList = pPreStmt->pNext;
      }
      if( pPreStmt->pNext ){
        pPreStmt->pNext->pPrev = pPreStmt->pPrev;
      }else{
        pDb->stmtLast = pPreStmt->pPrev;
      }
      pDb->nStmt--;
      nVar = sqlite3_bind_parameter_count(pStmt);
      break;
    }
  }
  
  /* If no prepared statement was found. Compile the SQL text. Also allocate
  ** a new SqlPreparedStmt structure.  */
  if( pPreStmt==0 ){
    int nByte;

    if( SQLITE_OK!=sqlite3_prepare_v2(pDb->db, zSql, -1, &pStmt, pzOut) ){
      Jim_SetResult(interp, dbTextToObj(pDb->interp, sqlite3_errmsg(pDb->db)));
      return JIM_ERR;
    }
    if( pStmt==0 ){
      if( SQLITE_OK!=sqlite3_errcode(pDb->db) ){
        /* A compile-time error in the statement. */
        Jim_SetResult(interp, dbTextToObj(pDb->interp, sqlite3_errmsg(pDb->db)));
        return JIM_ERR;
      }else{
        /* The statement was a no-op.  Continue to the next statement
        ** in the SQL string.
        */
        return JIM_OK;
      }
    }

    assert( pPreStmt==0 );
    nVar = sqlite3_bind_parameter_count(pStmt);
    nByte = sizeof(SqlPreparedStmt) + nVar*sizeof(Jim_Obj *);
    pPreStmt = (SqlPreparedStmt*)Jim_Alloc(nByte);
    memset(pPreStmt, 0, nByte);

    pPreStmt->pStmt = pStmt;
    pPreStmt->nSql = (*pzOut - zSql);
    pPreStmt->zSql = sqlite3_sql(pStmt);
    pPreStmt->apParm = (Jim_Obj **)&pPreStmt[1];
  }
  assert( pPreStmt );
  assert( strlen30(pPreStmt->zSql)==pPreStmt->nSql );
  assert( 0==memcmp(pPreStmt->zSql, zSql, pPreStmt->nSql) );

  /* Bind values to parameters that begin with $ or : */  
  for(i=1; i<=nVar; i++){
    const char *zVar = sqlite3_bind_parameter_name(pStmt, i);
    if( zVar!=0 && (zVar[0]=='$' || zVar[0]==':' || zVar[0]=='@') ){
      Jim_Obj *pVar = Jim_GetVariableStr(interp, &zVar[1], 0);
      if( pVar ){
        int n;
        u8 *data;
        const char *zType = (pVar->typePtr ? pVar->typePtr->name : "");
        char c = zType[0];
    /* XXX: Jim Tcl doesn't have bytearray or boolean */
        if( zVar[0]=='@') {
#if 0
        ||
           (c=='b' && strcmp(zType,"bytearray")==0 && pVar->bytes==0) ){
          /* Load a BLOB type if the Tcl variable is a bytearray and
          ** it has no string representation or the host
          ** parameter name begins with "@". */
          data = Jim_GetByteArrayFromObj(pVar, &n);
#else
          data = (unsigned char *)Jim_GetString(pVar, &n);
#endif
          sqlite3_bind_blob(pStmt, i, data, n, SQLITE_STATIC);
          Jim_IncrRefCount(pVar);
          pPreStmt->apParm[iParm++] = pVar;
#if 0
        }else if( c=='b' && strcmp(zType,"boolean")==0 ){
          Jim_GetWide(interp, pVar, &n);
          sqlite3_bind_int(pStmt, i, n);
#endif        
        }else if( c=='d' && strcmp(zType,"double")==0 ){
          double r;
          Jim_GetDouble(interp, pVar, &r);
          sqlite3_bind_double(pStmt, i, r);
        }else if( (c=='c' && strcmp(zType,"coerced-double")==0) ||
              (c=='i' && strcmp(zType,"int")==0) ){
          jim_wide v;
          Jim_GetWide(interp, pVar, &v);
          sqlite3_bind_int64(pStmt, i, v);
        }else{
          data = (unsigned char *)Jim_GetString(pVar, &n);
          sqlite3_bind_text(pStmt, i, (char *)data, n, SQLITE_STATIC);
          Jim_IncrRefCount(pVar);
          pPreStmt->apParm[iParm++] = pVar;
        }
      }else{
        sqlite3_bind_null(pStmt, i);
      }
    }
  }
  pPreStmt->nParm = iParm;
  *ppPreStmt = pPreStmt;

  return JIM_OK;
}


/*
** Release a statement reference obtained by calling dbPrepareAndBind().
** There should be exactly one call to this function for each call to
** dbPrepareAndBind().
**
** If the discard parameter is non-zero, then the statement is deleted
** immediately. Otherwise it is added to the LRU list and may be returned
** by a subsequent call to dbPrepareAndBind().
*/
static void dbReleaseStmt(
  SqliteDb *pDb,                  /* Database handle */
  SqlPreparedStmt *pPreStmt,      /* Prepared statement handle to release */
  int discard                     /* True to delete (not cache) the pPreStmt */
){
  int i;

  /* Free the bound string and blob parameters */
  for(i=0; i<pPreStmt->nParm; i++){
    Jim_DecrRefCount(pDb->interp, pPreStmt->apParm[i]);
  }
  pPreStmt->nParm = 0;

  if( pDb->maxStmt<=0 || discard ){
    /* If the cache is turned off, deallocated the statement */
    sqlite3_finalize(pPreStmt->pStmt);
    Jim_Free((char *)pPreStmt);
  }else{
    /* Add the prepared statement to the beginning of the cache list. */
    pPreStmt->pNext = pDb->stmtList;
    pPreStmt->pPrev = 0;
    if( pDb->stmtList ){
     pDb->stmtList->pPrev = pPreStmt;
    }
    pDb->stmtList = pPreStmt;
    if( pDb->stmtLast==0 ){
      assert( pDb->nStmt==0 );
      pDb->stmtLast = pPreStmt;
    }else{
      assert( pDb->nStmt>0 );
    }
    pDb->nStmt++;
   
    /* If we have too many statement in cache, remove the surplus from 
    ** the end of the cache list.  */
    while( pDb->nStmt>pDb->maxStmt ){
      sqlite3_finalize(pDb->stmtLast->pStmt);
      pDb->stmtLast = pDb->stmtLast->pPrev;
      Jim_Free((char*)pDb->stmtLast->pNext);
      pDb->stmtLast->pNext = 0;
      pDb->nStmt--;
    }
  }
}

/*
** Structure used with dbEvalXXX() functions:
**
**   dbEvalInit()
**   dbEvalStep()
**   dbEvalFinalize()
**   dbEvalRowInfo()
**   dbEvalColumnValue()
*/
typedef struct DbEvalContext DbEvalContext;
struct DbEvalContext {
  SqliteDb *pDb;                  /* Database handle */
  Jim_Obj *pSql;                  /* Object holding string zSql */
  const char *zSql;               /* Remaining SQL to execute */
  SqlPreparedStmt *pPreStmt;      /* Current statement */
  int nCol;                       /* Number of columns returned by pStmt */
  Jim_Obj *pArray;                /* Name of array variable */
  Jim_Obj **apColName;            /* Array of column names */
};

/*
** Release any cache of column names currently held as part of
** the DbEvalContext structure passed as the first argument.
*/
static void dbReleaseColumnNames(DbEvalContext *p){
  if( p->apColName ){
    int i;
    for(i=0; i<p->nCol; i++){
      Jim_DecrRefCount(p->pDb->interp, p->apColName[i]);
    }
    Jim_Free((char *)p->apColName);
    p->apColName = 0;
  }
  p->nCol = 0;
}

/*
** Initialize a DbEvalContext structure.
**
** If pArray is not NULL, then it contains the name of a Tcl array
** variable. The "*" member of this array is set to a list containing
** the names of the columns returned by the statement as part of each
** call to dbEvalStep(), in order from left to right. e.g. if the names 
** of the returned columns are a, b and c, it does the equivalent of the 
** tcl command:
**
**     set ${pArray}(*) {a b c}
*/
static void dbEvalInit(
  DbEvalContext *p,               /* Pointer to structure to initialize */
  SqliteDb *pDb,                  /* Database handle */
  Jim_Obj *pSql,                  /* Object containing SQL script */
  Jim_Obj *pArray                 /* Name of Tcl array to set (*) element of */
){
  memset(p, 0, sizeof(DbEvalContext));
  p->pDb = pDb;
  p->zSql = Jim_String(pSql);
  p->pSql = pSql;
  Jim_IncrRefCount(pSql);
  if( pArray ){
    p->pArray = pArray;
    Jim_IncrRefCount(pArray);
  }
}

/*
** Obtain information about the row that the DbEvalContext passed as the
** first argument currently points to.
*/
static void dbEvalRowInfo(
  DbEvalContext *p,               /* Evaluation context */
  int *pnCol,                     /* OUT: Number of column names */
  Jim_Obj ***papColName           /* OUT: Array of column names */
){
  /* Compute column names */
  if( 0==p->apColName ){
    sqlite3_stmt *pStmt = p->pPreStmt->pStmt;
    int i;                        /* Iterator variable */
    int nCol;                     /* Number of columns returned by pStmt */
    Jim_Obj **apColName = 0;      /* Array of column names */

    p->nCol = nCol = sqlite3_column_count(pStmt);
    if( nCol>0 && (papColName || p->pArray) ){
      apColName = (Jim_Obj**)Jim_Alloc( sizeof(Jim_Obj*)*nCol );
      for(i=0; i<nCol; i++){
        apColName[i] = dbTextToObj(p->pDb->interp, sqlite3_column_name(pStmt,i));
        Jim_IncrRefCount(apColName[i]);
      }
      p->apColName = apColName;
    }

    /* If results are being stored in an array variable, then create
    ** the array(*) entry for that array
    */
    if( p->pArray ){
      Jim_Interp *interp = p->pDb->interp;
      Jim_Obj *pColList = Jim_NewListObj(interp, apColName, nCol);
      Jim_Obj *pStar = Jim_NewStringObj(interp, "*", -1);
      Jim_IncrRefCount(pStar);
      Jim_SetDictKeysVector(interp, p->pArray, &pStar, 1, pColList, 0);
      Jim_DecrRefCount(interp, pStar);
    }
  }

  if( papColName ){
    *papColName = p->apColName;
  }
  if( pnCol ){
    *pnCol = p->nCol;
  }
}

/*
** Return one of JIM_OK, JIM_BREAK or JIM_ERR. If JIM_ERR is
** returned, then an error message is stored in the interpreter before
** returning.
**
** A return value of JIM_OK means there is a row of data available. The
** data may be accessed using dbEvalRowInfo() and dbEvalColumnValue(). This
** is analogous to a return of SQLITE_ROW from sqlite3_step(). If JIM_BREAK
** is returned, then the SQL script has finished executing and there are
** no further rows available. This is similar to SQLITE_DONE.
*/
static int dbEvalStep(DbEvalContext *p){
  while( p->zSql[0] || p->pPreStmt ){
    int rc;
    if( p->pPreStmt==0 ){
      rc = dbPrepareAndBind(p->pDb, p->zSql, &p->zSql, &p->pPreStmt);
      if( rc!=JIM_OK ) return rc;
    }else{
      int rcs;
      SqliteDb *pDb = p->pDb;
      SqlPreparedStmt *pPreStmt = p->pPreStmt;
      sqlite3_stmt *pStmt = pPreStmt->pStmt;

      rcs = sqlite3_step(pStmt);
      if( rcs==SQLITE_ROW ){
        return JIM_OK;
      }
      if( p->pArray ){
        dbEvalRowInfo(p, 0, 0);
      }
      rcs = sqlite3_reset(pStmt);

      pDb->nStep = sqlite3_stmt_status(pStmt,SQLITE_STMTSTATUS_FULLSCAN_STEP,1);
      pDb->nSort = sqlite3_stmt_status(pStmt,SQLITE_STMTSTATUS_SORT,1);
      dbReleaseColumnNames(p);
      p->pPreStmt = 0;

      if( rcs!=SQLITE_OK ){
        /* If a run-time error occurs, report the error and stop reading
        ** the SQL.  */
        Jim_SetResult(pDb->interp, dbTextToObj(pDb->interp, sqlite3_errmsg(pDb->db)));
        dbReleaseStmt(pDb, pPreStmt, 1);
        return JIM_ERR;
      }else{
        dbReleaseStmt(pDb, pPreStmt, 0);
      }
    }
  }

  /* Finished */
  return JIM_BREAK;
}

/*
** Free all resources currently held by the DbEvalContext structure passed
** as the first argument. There should be exactly one call to this function
** for each call to dbEvalInit().
*/
static void dbEvalFinalize(DbEvalContext *p){
  if( p->pPreStmt ){
    sqlite3_reset(p->pPreStmt->pStmt);
    dbReleaseStmt(p->pDb, p->pPreStmt, 0);
    p->pPreStmt = 0;
  }
  if( p->pArray ){
    Jim_DecrRefCount(p->pDb->interp, p->pArray);
    p->pArray = 0;
  }
  Jim_DecrRefCount(p->pDb->interp, p->pSql);
  dbReleaseColumnNames(p);
}

/*
** Return a pointer to a Jim_Obj structure with ref-count 0 that contains
** the value for the iCol'th column of the row currently pointed to by
** the DbEvalContext structure passed as the first argument.
*/
static Jim_Obj *dbEvalColumnValue(DbEvalContext *p, int iCol){
  sqlite3_stmt *pStmt = p->pPreStmt->pStmt;
  switch( sqlite3_column_type(pStmt, iCol) ){
    case SQLITE_BLOB: {
      int bytes = sqlite3_column_bytes(pStmt, iCol);
      const char *zBlob = sqlite3_column_blob(pStmt, iCol);
      if( !zBlob ) bytes = 0;
      //return Jim_NewByteArrayObj((u8*)zBlob, bytes);
      return Jim_NewStringObj(p->pDb->interp, zBlob, bytes);
    }
    case SQLITE_INTEGER: {
      sqlite_int64 v = sqlite3_column_int64(pStmt, iCol);
      return Jim_NewIntObj(p->pDb->interp, v);
    }
    case SQLITE_FLOAT: {
      return Jim_NewDoubleObj(p->pDb->interp, sqlite3_column_double(pStmt, iCol));
    }
    case SQLITE_NULL: {
      return dbTextToObj(p->pDb->interp, p->pDb->zNull);
    }
  }

  return dbTextToObj(p->pDb->interp, (char *)sqlite3_column_text(pStmt, iCol));
}

static int Jim_ObjSetVar2(Jim_Interp *interp, Jim_Obj *nameObjPtr, Jim_Obj *keyObjPtr, Jim_Obj *valObjPtr)
{
    return Jim_SetDictKeysVector(interp, nameObjPtr, &keyObjPtr, 1, valObjPtr, 0);
}

/*
** This function is part of the implementation of the command:
**
**   $db eval SQL ?ARRAYNAME? SCRIPT
*/
static int DbEvalNextCmd(
  Jim_Interp *interp,                  /* Tcl interpreter */
  DbEvalContext *p,
  Jim_Obj *pScript,
  int result                           /* Result so far */
){
  int rc = result;                     /* Return code */

  Jim_Obj *pArray = p->pArray;

  while( (rc==JIM_OK || rc==JIM_CONTINUE) && JIM_OK==(rc = dbEvalStep(p)) ){
    int i;
    int nCol;
    Jim_Obj **apColName;
    dbEvalRowInfo(p, &nCol, &apColName);
    for(i=0; i<nCol; i++){
      Jim_Obj *pVal = dbEvalColumnValue(p, i);
      if( pArray==0 ){
        Jim_SetVariable(interp, apColName[i], pVal);
      }else{
        Jim_ObjSetVar2(interp, pArray, apColName[i], pVal);
      }
    }

    /* The required interpreter variables are now populated with the data 
    ** from the current row.
    **
    ** No NRE in Jim Tcl, so evaluate pScript directly and continue with the
    ** next iteration of this while(...) loop.  */
    rc = Jim_EvalObj(interp, pScript);
  }

  Jim_DecrRefCount(interp, pScript);
  dbEvalFinalize(p);
  Jim_Free((char *)p);

  if( rc==JIM_OK || rc==JIM_BREAK ){
    Jim_SetEmptyResult(interp);
    rc = JIM_OK;
  }
  return rc;
}

/*
** The "sqlite" command below creates a new Tcl command for each
** connection it opens to an SQLite database.  This routine is invoked
** whenever one of those connection-specific commands is executed
** in Tcl.  For example, if you run Tcl code like this:
**
**       sqlite3 db1  "my_database"
**       db1 close
**
** The first command opens a connection to the "my_database" database
** and calls that connection "db1".  The second command causes this
** subroutine to be invoked.
*/
static int DbObjCmd(Jim_Interp *interp, int objc,Jim_Obj *const*objv){
  SqliteDb *pDb = (SqliteDb*)Jim_CmdPrivData(interp);
  int choice;
  int rc = JIM_OK;
  static const char *DB_strs[] = {
    "authorizer",         "backup",            "busy",
    "cache",              "changes",           "close",
    "collate",            "collation_needed",  "commit_hook",
    "complete",           "copy",              "enable_load_extension",
    "errorcode",          "eval",              "exists",
    "function",           "incrblob",          "interrupt",
    "last_insert_rowid",  "nullvalue",         "onecolumn",
    "profile",            "progress",          "rekey",
    "restore",            "rollback_hook",     "status",
    "timeout",            "total_changes",     "trace",
    "transaction",        "unlock_notify",     "update_hook",
    "version",            0                    
  };
  enum DB_enum {
    DB_AUTHORIZER,        DB_BACKUP,           DB_BUSY,
    DB_CACHE,             DB_CHANGES,          DB_CLOSE,
    DB_COLLATE,           DB_COLLATION_NEEDED, DB_COMMIT_HOOK,
    DB_COMPLETE,          DB_COPY,             DB_ENABLE_LOAD_EXTENSION,
    DB_ERRORCODE,         DB_EVAL,             DB_EXISTS,
    DB_FUNCTION,          DB_INCRBLOB,         DB_INTERRUPT,
    DB_LAST_INSERT_ROWID, DB_NULLVALUE,        DB_ONECOLUMN,
    DB_PROFILE,           DB_PROGRESS,         DB_REKEY,
    DB_RESTORE,           DB_ROLLBACK_HOOK,    DB_STATUS,
    DB_TIMEOUT,           DB_TOTAL_CHANGES,    DB_TRACE,
    DB_TRANSACTION,       DB_UNLOCK_NOTIFY,    DB_UPDATE_HOOK,
    DB_VERSION,
  };
  /* don't leave trailing commas on DB_enum, it confuses the AIX xlc compiler */

  if( objc<2 ){
    Jim_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
    return JIM_ERR;
  }
  if( Jim_GetEnum(interp, objv[1], DB_strs, &choice, "option", JIM_ERRMSG | JIM_ENUM_ABBREV) ){
    return JIM_ERR;
  }

  switch( (enum DB_enum)choice ){

  /*    $db authorizer ?CALLBACK?
  **
  ** Invoke the given callback to authorize each SQL operation as it is
  ** compiled.  5 arguments are appended to the callback before it is
  ** invoked:
  **
  **   (1) The authorization type (ex: SQLITE_CREATE_TABLE, SQLITE_INSERT, ...)
  **   (2) First descriptive name (depends on authorization type)
  **   (3) Second descriptive name
  **   (4) Name of the database (ex: "main", "temp")
  **   (5) Name of trigger that is doing the access
  **
  ** The callback should return on of the following strings: SQLITE_OK,
  ** SQLITE_IGNORE, or SQLITE_DENY.  Any other return value is an error.
  **
  ** If this method is invoked with no arguments, the current authorization
  ** callback string is returned.
  */
  case DB_AUTHORIZER: {
#ifdef SQLITE_OMIT_AUTHORIZATION
    Jim_SetResultString(interp, "authorization not available in this build", -1);
    return JIM_ERR;
#else
    if( objc>3 ){
      Jim_WrongNumArgs(interp, 2, objv, "?CALLBACK?");
      return JIM_ERR;
    }else if( objc==2 ){
      if( pDb->zAuth ){
        Jim_SetResultString(interp, pDb->zAuth, -1);
      }
    }else{
      const char *zAuth;
      int len;
      if( pDb->zAuth ){
        Jim_Free(pDb->zAuth);
      }
      zAuth = Jim_GetString(objv[2], &len);
      if( zAuth && len>0 ){
        pDb->zAuth = Jim_Alloc( len + 1 );
        memcpy(pDb->zAuth, zAuth, len+1);
      }else{
        pDb->zAuth = 0;
      }
      if( pDb->zAuth ){
        pDb->interp = interp;
        sqlite3_set_authorizer(pDb->db, auth_callback, pDb);
      }else{
        sqlite3_set_authorizer(pDb->db, 0, 0);
      }
    }
#endif
    break;
  }

  /*    $db backup ?DATABASE? FILENAME
  **
  ** Open or create a database file named FILENAME.  Transfer the
  ** content of local database DATABASE (default: "main") into the
  ** FILENAME database.
  */
  case DB_BACKUP: {
    const char *zDestFile;
    const char *zSrcDb;
    sqlite3 *pDest;
    sqlite3_backup *pBackup;

    if( objc==3 ){
      zSrcDb = "main";
      zDestFile = Jim_String(objv[2]);
    }else if( objc==4 ){
      zSrcDb = Jim_String(objv[2]);
      zDestFile = Jim_String(objv[3]);
    }else{
      Jim_WrongNumArgs(interp, 2, objv, "?DATABASE? FILENAME");
      return JIM_ERR;
    }
    rc = sqlite3_open(zDestFile, &pDest);
    if( rc!=SQLITE_OK ){
      Jim_SetResultFormatted(interp, "cannot open target database: %s", sqlite3_errmsg(pDest));
      sqlite3_close(pDest);
      return JIM_ERR;
    }
    pBackup = sqlite3_backup_init(pDest, "main", pDb->db, zSrcDb);
    if( pBackup==0 ){
      Jim_SetResultFormatted(interp, "backup failed: %s", sqlite3_errmsg(pDest));
      sqlite3_close(pDest);
      return JIM_ERR;
    }
    while(  (rc = sqlite3_backup_step(pBackup,100))==SQLITE_OK ){}
    sqlite3_backup_finish(pBackup);
    if( rc==SQLITE_DONE ){
      rc = JIM_OK;
    }else{
      Jim_SetResultFormatted(interp, "backup failed: %s", sqlite3_errmsg(pDest));
      rc = JIM_ERR;
    }
    sqlite3_close(pDest);
    break;
  }

  /*    $db busy ?CALLBACK?
  **
  ** Invoke the given callback if an SQL statement attempts to open
  ** a locked database file.
  */
  case DB_BUSY: {
    if( objc>3 ){
      Jim_WrongNumArgs(interp, 2, objv, "CALLBACK");
      return JIM_ERR;
    }else if( objc==2 ){
      if( pDb->zBusy ){
        Jim_SetResultString(interp, pDb->zBusy, -1);
      }
    }else{
      const char *zBusy;
      int len;
      if( pDb->zBusy ){
        Jim_Free(pDb->zBusy);
      }
      zBusy = Jim_GetString(objv[2], &len);
      if( zBusy && len>0 ){
        pDb->zBusy = Jim_Alloc( len + 1 );
        memcpy(pDb->zBusy, zBusy, len+1);
      }else{
        pDb->zBusy = 0;
      }
      if( pDb->zBusy ){
        pDb->interp = interp;
        sqlite3_busy_handler(pDb->db, DbBusyHandler, pDb);
      }else{
        sqlite3_busy_handler(pDb->db, 0, 0);
      }
    }
    break;
  }

  /*     $db cache flush
  **     $db cache size n
  **
  ** Flush the prepared statement cache, or set the maximum number of
  ** cached statements.
  */
  case DB_CACHE: {
    const char *subCmd;

    if( objc<=2 ){
      Jim_WrongNumArgs(interp, 1, objv, "cache option ?arg?");
      return JIM_ERR;
    }
    subCmd = Jim_String( objv[2]);
    if( *subCmd=='f' && strcmp(subCmd,"flush")==0 ){
      if( objc!=3 ){
        Jim_WrongNumArgs(interp, 2, objv, "flush");
        return JIM_ERR;
      }else{
        flushStmtCache( pDb );
      }
    }else if( *subCmd=='s' && strcmp(subCmd,"size")==0 ){
      if( objc!=4 ){
        Jim_WrongNumArgs(interp, 2, objv, "size n");
        return JIM_ERR;
      }else{
        jim_wide w;
        if( JIM_ERR==Jim_GetWide(interp, objv[3], &w) ){
          return JIM_ERR;
        }else{
          if( w<0 ){
            flushStmtCache( pDb );
            w = 0;
          }else if( w>MAX_PREPARED_STMTS ){
            w = MAX_PREPARED_STMTS;
          }
          pDb->maxStmt = w;
        }
      }
    }else{
      Jim_SetResultFormatted(interp, "bad option \"%#s\": must be flush or size", objv[2]);
      return JIM_ERR;
    }
    break;
  }

  /*     $db changes
  **
  ** Return the number of rows that were modified, inserted, or deleted by
  ** the most recent INSERT, UPDATE or DELETE statement, not including 
  ** any changes made by trigger programs.
  */
  case DB_CHANGES: {
    if( objc!=2 ){
      Jim_WrongNumArgs(interp, 2, objv, "");
      return JIM_ERR;
    }
    Jim_SetResultInt(interp, sqlite3_changes(pDb->db));
    break;
  }

  /*    $db close
  **
  ** Shutdown the database
  */
  case DB_CLOSE: {
    Jim_DeleteCommand(interp, Jim_String(objv[0]));
    break;
  }

  /*
  **     $db collate NAME SCRIPT
  **
  ** Create a new SQL collation function called NAME.  Whenever
  ** that function is called, invoke SCRIPT to evaluate the function.
  */
  case DB_COLLATE: {
    SqlCollate *pCollate;
    const char *zName;
    const char *zScript;
    int nScript;
    if( objc!=4 ){
      Jim_WrongNumArgs(interp, 2, objv, "NAME SCRIPT");
      return JIM_ERR;
    }
    zName = Jim_String(objv[2]);
    zScript = Jim_GetString(objv[3], &nScript);
    pCollate = (SqlCollate*)Jim_Alloc( sizeof(*pCollate) + nScript + 1 );
    if( pCollate==0 ) return JIM_ERR;
    pCollate->interp = interp;
    pCollate->pNext = pDb->pCollate;
    pCollate->zScript = (char*)&pCollate[1];
    pDb->pCollate = pCollate;
    memcpy(pCollate->zScript, zScript, nScript+1);
    if( sqlite3_create_collation(pDb->db, zName, SQLITE_UTF8, 
        pCollate, tclSqlCollate) ){
      Jim_SetResultString(interp, (char *)sqlite3_errmsg(pDb->db), -1);
      return JIM_ERR;
    }
    break;
  }

  /*
  **     $db collation_needed SCRIPT
  **
  ** Create a new SQL collation function called NAME.  Whenever
  ** that function is called, invoke SCRIPT to evaluate the function.
  */
  case DB_COLLATION_NEEDED: {
    if( objc!=3 ){
      Jim_WrongNumArgs(interp, 2, objv, "SCRIPT");
      return JIM_ERR;
    }
    if( pDb->pCollateNeeded ){
      Jim_DecrRefCount(interp, pDb->pCollateNeeded);
    }
    pDb->pCollateNeeded = Jim_DuplicateObj(pDb->interp, objv[2]);
    Jim_IncrRefCount(pDb->pCollateNeeded);
    sqlite3_collation_needed(pDb->db, pDb, tclCollateNeeded);
    break;
  }

  /*    $db commit_hook ?CALLBACK?
  **
  ** Invoke the given callback just before committing every SQL transaction.
  ** If the callback throws an exception or returns non-zero, then the
  ** transaction is aborted.  If CALLBACK is an empty string, the callback
  ** is disabled.
  */
  case DB_COMMIT_HOOK: {
    if( objc>3 ){
      Jim_WrongNumArgs(interp, 2, objv, "?CALLBACK?");
      return JIM_ERR;
    }else if( objc==2 ){
      if( pDb->zCommit ){
        Jim_SetResultString(interp, pDb->zCommit, -1);
      }
    }else{
      const char *zCommit;
      int len;
      if( pDb->zCommit ){
        Jim_Free(pDb->zCommit);
      }
      zCommit = Jim_GetString(objv[2], &len);
      if( zCommit && len>0 ){
        pDb->zCommit = Jim_Alloc( len + 1 );
        memcpy(pDb->zCommit, zCommit, len+1);
      }else{
        pDb->zCommit = 0;
      }
      if( pDb->zCommit ){
        pDb->interp = interp;
        sqlite3_commit_hook(pDb->db, DbCommitHandler, pDb);
      }else{
        sqlite3_commit_hook(pDb->db, 0, 0);
      }
    }
    break;
  }

  /*    $db complete SQL
  **
  ** Return TRUE if SQL is a complete SQL statement.  Return FALSE if
  ** additional lines of input are needed.  This is similar to the
  ** built-in "info complete" command of Tcl.
  */
  case DB_COMPLETE: {
#ifndef SQLITE_OMIT_COMPLETE
    if( objc!=3 ){
      Jim_WrongNumArgs(interp, 2, objv, "SQL");
      return JIM_ERR;
    }
    Jim_SetResultInt(interp, sqlite3_complete( Jim_String(objv[2]) ));
#endif
    break;
  }

  /*    $db copy conflict-algorithm table filename ?SEPARATOR? ?NULLINDICATOR?
  **
  ** Copy data into table from filename, optionally using SEPARATOR
  ** as column separators.  If a column contains a null string, or the
  ** value of NULLINDICATOR, a NULL is inserted for the column.
  ** conflict-algorithm is one of the sqlite conflict algorithms:
  **    rollback, abort, fail, ignore, replace
  ** On success, return the number of lines processed, not necessarily same
  ** as 'db changes' due to conflict-algorithm selected.
  **
  ** This code is basically an implementation/enhancement of
  ** the sqlite3 shell.c ".import" command.
  **
  ** This command usage is equivalent to the sqlite2.x COPY statement,
  ** which imports file data into a table using the PostgreSQL COPY file format:
  **   $db copy $conflit_algo $table_name $filename \t \\N
  */
  case DB_COPY: {
    const char *zTable;               /* Insert data into this table */
    const char *zFile;                /* The file from which to extract data */
    const char *zConflict;            /* The conflict algorithm to use */
    sqlite3_stmt *pStmt;        /* A statement */
    int nCol;                   /* Number of columns in the table */
    int nByte;                  /* Number of bytes in an SQL string */
    int i, j;                   /* Loop counters */
    int nSep;                   /* Number of bytes in zSep[] */
    int nNull;                  /* Number of bytes in zNull[] */
    char *zSql;                 /* An SQL statement */
    char *zLine;                /* A single line of input from the file */
    char **azCol;               /* zLine[] broken up into columns */
    char *zCommit;              /* How to commit changes */
    FILE *in;                   /* The input file */
    int lineno = 0;             /* Line number of input file */
    char zLineNum[80];          /* Line number print buffer */

    const char *zSep;
    const char *zNull;
    if( objc<5 || objc>7 ){
      Jim_WrongNumArgs(interp, 2, objv, 
         "CONFLICT-ALGORITHM TABLE FILENAME ?SEPARATOR? ?NULLINDICATOR?");
      return JIM_ERR;
    }
    if( objc>=6 ){
      zSep = Jim_String(objv[5]);
    }else{
      zSep = "\t";
    }
    if( objc>=7 ){
      zNull = Jim_String(objv[6]);
    }else{
      zNull = "";
    }
    zConflict = Jim_String(objv[2]);
    zTable = Jim_String(objv[3]);
    zFile = Jim_String(objv[4]);
    nSep = strlen30(zSep);
    nNull = strlen30(zNull);
    if( nSep==0 ){
      Jim_SetResultString(interp, "Error: non-null separator required for copy", -1);
      return JIM_ERR;
    }
    if(strcmp(zConflict, "rollback") != 0 &&
       strcmp(zConflict, "abort"   ) != 0 &&
       strcmp(zConflict, "fail"    ) != 0 &&
       strcmp(zConflict, "ignore"  ) != 0 &&
       strcmp(zConflict, "replace" ) != 0 ) {
      Jim_SetResultFormatted(interp, "Error: \"%s\", conflict-algorithm must be one of: rollback, "
            "abort, fail, ignore, or replace", zConflict);
      return JIM_ERR;
    }
    zSql = sqlite3_mprintf("SELECT * FROM '%q'", zTable);
    if( zSql==0 ){
      Jim_SetResultFormatted(interp, "Error: no such table: %s", zTable);
      return JIM_ERR;
    }
    nByte = strlen30(zSql);
    rc = sqlite3_prepare(pDb->db, zSql, -1, &pStmt, 0);
    sqlite3_free(zSql);
    if( rc ){
      Jim_SetResultFormatted(interp, "Error: %s", sqlite3_errmsg(pDb->db));
      nCol = 0;
    }else{
      nCol = sqlite3_column_count(pStmt);
    }
    sqlite3_finalize(pStmt);
    if( nCol==0 ) {
      return JIM_ERR;
    }
    zSql = Jim_Alloc( nByte + 50 + nCol*2 );
    sqlite3_snprintf(nByte+50, zSql, "INSERT OR %q INTO '%q' VALUES(?",
         zConflict, zTable);
    j = strlen30(zSql);
    for(i=1; i<nCol; i++){
      zSql[j++] = ',';
      zSql[j++] = '?';
    }
    zSql[j++] = ')';
    zSql[j] = 0;
    rc = sqlite3_prepare(pDb->db, zSql, -1, &pStmt, 0);
    Jim_Free(zSql);
    if( rc ){
      Jim_SetResultFormatted(interp, "Error: %s", sqlite3_errmsg(pDb->db));
      sqlite3_finalize(pStmt);
      return JIM_ERR;
    }
    in = fopen(zFile, "rb");
    if( in==0 ){
      Jim_SetResultFormatted(interp, "Error: cannot open file: %s", zFile);
      sqlite3_finalize(pStmt);
      return JIM_ERR;
    }
    azCol = Jim_Alloc( sizeof(azCol[0])*(nCol+1) );
    (void)sqlite3_exec(pDb->db, "BEGIN", 0, 0, 0);
    zCommit = "COMMIT";
    while( (zLine = local_getline(0, in))!=0 ){
      char *z;
      i = 0;
      lineno++;
      azCol[0] = zLine;
      for(i=0, z=zLine; *z; z++){
        if( *z==zSep[0] && strncmp(z, zSep, nSep)==0 ){
          *z = 0;
          i++;
          if( i<nCol ){
            azCol[i] = &z[nSep];
            z += nSep-1;
          }
        }
      }
      if( i+1!=nCol ){
        char *zErr;
        int nErr = strlen30(zFile) + 200;
        zErr = Jim_Alloc(nErr);
        sqlite3_snprintf(nErr, zErr,
           "Error: %s line %d: expected %d columns of data but found %d",
           zFile, lineno, nCol, i+1);
        Jim_SetResultString(interp, zErr, -1);
        Jim_Free(zErr);
        zCommit = "ROLLBACK";
        break;
      }
      for(i=0; i<nCol; i++){
        /* check for null data, if so, bind as null */
        if( (nNull>0 && strcmp(azCol[i], zNull)==0)
          || strlen30(azCol[i])==0 
        ){
          sqlite3_bind_null(pStmt, i+1);
        }else{
          sqlite3_bind_text(pStmt, i+1, azCol[i], -1, SQLITE_STATIC);
        }
      }
      sqlite3_step(pStmt);
      rc = sqlite3_reset(pStmt);
      Jim_Free(zLine);
      if( rc!=SQLITE_OK ){
        Jim_SetResultFormatted(interp, "Error: %s", sqlite3_errmsg(pDb->db));
        zCommit = "ROLLBACK";
        break;
      }
    }
    Jim_Free(azCol);
    fclose(in);
    sqlite3_finalize(pStmt);
    (void)sqlite3_exec(pDb->db, zCommit, 0, 0, 0);

    if( zCommit[0] == 'C' ){
      /* success, set result as number of lines processed */
      Jim_SetResultInt(interp, lineno);
      rc = JIM_OK;
    }else{
      /* failure, append lineno where failed */
      sqlite3_snprintf(sizeof(zLineNum), zLineNum,"%d",lineno);
      Jim_AppendStrings(interp, Jim_GetResult(interp), ", failed while processing line: ", zLineNum, NULL);
      rc = JIM_ERR;
    }
    break;
  }

  /*
  **    $db enable_load_extension BOOLEAN
  **
  ** Turn the extension loading feature on or off.  It if off by
  ** default.
  */
  case DB_ENABLE_LOAD_EXTENSION: {
#ifndef SQLITE_OMIT_LOAD_EXTENSION
    long onoff;
    if( objc!=3 ){
      Jim_WrongNumArgs(interp, 2, objv, "BOOLEAN");
      return JIM_ERR;
    }
    if( Jim_GetLong(interp, objv[2], &onoff) ){
      return JIM_ERR;
    }
    sqlite3_enable_load_extension(pDb->db, onoff);
    break;
#else
    Jim_SetResultString(interp, "extension loading is turned off at compile-time", -1);
    return JIM_ERR;
#endif
  }

  /*
  **    $db errorcode
  **
  ** Return the numeric error code that was returned by the most recent
  ** call to sqlite3_exec().
  */
  case DB_ERRORCODE: {
    Jim_SetResultInt(interp, sqlite3_errcode(pDb->db));
    break;
  }

  /*
  **    $db exists $sql
  **    $db onecolumn $sql
  **
  ** The onecolumn method is the equivalent of:
  **     lindex [$db eval $sql] 0
  */
  case DB_EXISTS: 
  case DB_ONECOLUMN: {
    DbEvalContext sEval;
    if( objc!=3 ){
      Jim_WrongNumArgs(interp, 2, objv, "SQL");
      return JIM_ERR;
    }

    dbEvalInit(&sEval, pDb, objv[2], 0);
    rc = dbEvalStep(&sEval);
    if( choice==DB_ONECOLUMN ){
      if( rc==JIM_OK ){
        Jim_SetResult(interp, dbEvalColumnValue(&sEval, 0));
      }
    }else if( rc==JIM_BREAK || rc==JIM_OK ){
      Jim_SetResultInt(interp, rc==JIM_OK);
    }
    dbEvalFinalize(&sEval);

    if( rc==JIM_BREAK ){
      rc = JIM_OK;
    }
    break;
  }
   
  /*
  **    $db eval $sql ?array? ?{  ...code... }?
  **
  ** The SQL statement in $sql is evaluated.  For each row, the values are
  ** placed in elements of the array named "array" and ...code... is executed.
  ** If "array" and "code" are omitted, then no callback is every invoked.
  ** If "array" is an empty string, then the values are placed in variables
  ** that have the same name as the fields extracted by the query.
  */
  case DB_EVAL: {
    if( objc<3 || objc>5 ){
      Jim_WrongNumArgs(interp, 2, objv, "SQL ?ARRAY-NAME? ?SCRIPT?");
      return JIM_ERR;
    }

    if( objc==3 ){
      DbEvalContext sEval;
      Jim_Obj *pRet = Jim_NewListObj(interp, NULL, 0);
      Jim_IncrRefCount(pRet);
      dbEvalInit(&sEval, pDb, objv[2], 0);
      while( JIM_OK==(rc = dbEvalStep(&sEval)) ){
        int i;
        int nCol;
        dbEvalRowInfo(&sEval, &nCol, 0);
        for(i=0; i<nCol; i++){
          Jim_ListAppendElement(interp, pRet, dbEvalColumnValue(&sEval, i));
        }
      }
      dbEvalFinalize(&sEval);
      if( rc==JIM_BREAK ){
        Jim_SetResult(interp, pRet);
        rc = JIM_OK;
      }
      Jim_DecrRefCount(interp, pRet);
    }else{
      DbEvalContext *p;
      Jim_Obj *pArray = 0;
      Jim_Obj *pScript;

      if( objc==5 && Jim_Length(objv[3]) ){
        pArray = objv[3];
      }
      pScript = objv[objc-1];
      Jim_IncrRefCount(pScript);
      
      p = (DbEvalContext *)Jim_Alloc(sizeof(DbEvalContext));
      dbEvalInit(p, pDb, objv[2], pArray);

      rc = DbEvalNextCmd(interp, p, pScript, JIM_OK);
    }
    break;
  }

  /*
  **     $db function NAME [-argcount N] SCRIPT
  **
  ** Create a new SQL function called NAME.  Whenever that function is
  ** called, invoke SCRIPT to evaluate the function.
  */
  case DB_FUNCTION: {
    SqlFunc *pFunc;
    Jim_Obj *pScript;
    const char *zName;
    long nArg = -1;
    if( objc==6 ){
      const char *z = Jim_String(objv[3]);
      int n = strlen30(z);
      if( n>2 && strncmp(z, "-argcount",n)==0 ){
        if( Jim_GetLong(interp, objv[4], &nArg) ) return JIM_ERR;
        if( nArg<0 ){
          Jim_SetResultString(interp, "number of arguments must be non-negative", -1);
          return JIM_ERR;
        }
      }
      pScript = objv[5];
    }else if( objc!=4 ){
      Jim_WrongNumArgs(interp, 2, objv, "NAME [-argcount N] SCRIPT");
      return JIM_ERR;
    }else{
      pScript = objv[3];
    }
    zName = Jim_String(objv[2]);
    pFunc = findSqlFunc(pDb, zName);
    if( pFunc==0 ) return JIM_ERR;
    if( pFunc->pScript ){
      Jim_DecrRefCount(interp, pFunc->pScript);
    }
    pFunc->pScript = pScript;
    Jim_IncrRefCount(pScript);
    pFunc->useEvalObjv = safeToUseEvalObjv(interp, pScript);
    rc = sqlite3_create_function(pDb->db, zName, nArg, SQLITE_UTF8,
        pFunc, tclSqlFunc, 0, 0);
    if( rc!=SQLITE_OK ){
      rc = JIM_ERR;
      Jim_SetResultString(interp, (char *)sqlite3_errmsg(pDb->db), -1);
    }
    break;
  }

  /*
  **     $db incrblob ?-readonly? ?DB? TABLE COLUMN ROWID
  */
  case DB_INCRBLOB: {
#ifdef SQLITE_OMIT_INCRBLOB
    Jim_SetResultString(interp, "incrblob not available in this build", -1);
    return JIM_ERR;
#else
    int isReadonly = 0;
    const char *zDb = "main";
    const char *zTable;
    const char *zColumn;
    sqlite_int64 iRow;

    /* Check for the -readonly option */
    if( objc>3 && strcmp(Jim_GetString(objv[2]), "-readonly")==0 ){
      isReadonly = 1;
    }

    if( objc!=(5+isReadonly) && objc!=(6+isReadonly) ){
      Jim_WrongNumArgs(interp, 2, objv, "?-readonly? ?DB? TABLE COLUMN ROWID");
      return JIM_ERR;
    }

    if( objc==(6+isReadonly) ){
      zDb = Jim_GetString(objv[2]);
    }
    zTable = Jim_GetString(objv[objc-3]);
    zColumn = Jim_GetString(objv[objc-2]);
    rc = Jim_GetWide(interp, objv[objc-1], &iRow);

    if( rc==JIM_OK ){
      rc = createIncrblobChannel(
          interp, pDb, zDb, zTable, zColumn, iRow, isReadonly
      );
    }
#endif
    break;
  }

  /*
  **     $db interrupt
  **
  ** Interrupt the execution of the inner-most SQL interpreter.  This
  ** causes the SQL statement to return an error of SQLITE_INTERRUPT.
  */
  case DB_INTERRUPT: {
    sqlite3_interrupt(pDb->db);
    break;
  }

  /*
  **     $db nullvalue ?STRING?
  **
  ** Change text used when a NULL comes back from the database. If ?STRING?
  ** is not present, then the current string used for NULL is returned.
  ** If STRING is present, then STRING is returned.
  **
  */
  case DB_NULLVALUE: {
    if( objc!=2 && objc!=3 ){
      Jim_WrongNumArgs(interp, 2, objv, "NULLVALUE");
      return JIM_ERR;
    }
    if( objc==3 ){
      int len;
      const char *zNull = Jim_GetString(objv[2], &len);
      if( pDb->zNull ){
        Jim_Free(pDb->zNull);
      }
      if( zNull && len>0 ){
        pDb->zNull = Jim_Alloc( len + 1 );
        strncpy(pDb->zNull, zNull, len);
        pDb->zNull[len] = '\0';
      }else{
        pDb->zNull = 0;
      }
    }
    Jim_SetResult(interp, dbTextToObj(interp, pDb->zNull));
    break;
  }

  /*
  **     $db last_insert_rowid 
  **
  ** Return an integer which is the ROWID for the most recent insert.
  */
  case DB_LAST_INSERT_ROWID: {
    if( objc!=2 ){
      Jim_WrongNumArgs(interp, 2, objv, "");
      return JIM_ERR;
    }
    Jim_SetResultInt(interp, sqlite3_last_insert_rowid(pDb->db));
    break;
  }

  /*
  ** The DB_ONECOLUMN method is implemented together with DB_EXISTS.
  */

  /*    $db progress ?N CALLBACK?
  ** 
  ** Invoke the given callback every N virtual machine opcodes while executing
  ** queries.
  */
  case DB_PROGRESS: {
    if( objc==2 ){
      if( pDb->zProgress ){
        Jim_AppendString(interp, Jim_GetResult(interp), pDb->zProgress, -1);
      }
    }else if( objc==4 ){
      const char *zProgress;
      int len;
      long N;
      if( JIM_OK!=Jim_GetLong(interp, objv[2], &N) ){
        return JIM_ERR;
      };
      if( pDb->zProgress ){
        Jim_Free(pDb->zProgress);
      }
      zProgress = Jim_GetString(objv[3], &len);
      if( zProgress && len>0 ){
        pDb->zProgress = Jim_Alloc( len + 1 );
        memcpy(pDb->zProgress, zProgress, len+1);
      }else{
        pDb->zProgress = 0;
      }
#ifndef SQLITE_OMIT_PROGRESS_CALLBACK
      if( pDb->zProgress ){
        pDb->interp = interp;
        sqlite3_progress_handler(pDb->db, N, DbProgressHandler, pDb);
      }else{
        sqlite3_progress_handler(pDb->db, 0, 0, 0);
      }
#endif
    }else{
      Jim_WrongNumArgs(interp, 2, objv, "N CALLBACK");
      return JIM_ERR;
    }
    break;
  }

  /*    $db profile ?CALLBACK?
  **
  ** Make arrangements to invoke the CALLBACK routine after each SQL statement
  ** that has run.  The text of the SQL and the amount of elapse time are
  ** appended to CALLBACK before the script is run.
  */
  case DB_PROFILE: {
    if( objc>3 ){
      Jim_WrongNumArgs(interp, 2, objv, "?CALLBACK?");
      return JIM_ERR;
    }else if( objc==2 ){
      if( pDb->zProfile ){
        Jim_SetResultString(interp, pDb->zProfile, -1);
      }
    }else{
      const char *zProfile;
      int len;
      if( pDb->zProfile ){
        Jim_Free(pDb->zProfile);
      }
      zProfile = Jim_GetString(objv[2], &len);
      if( zProfile && len>0 ){
        pDb->zProfile = Jim_Alloc( len + 1 );
        memcpy(pDb->zProfile, zProfile, len+1);
      }else{
        pDb->zProfile = 0;
      }
#ifndef SQLITE_OMIT_TRACE
      if( pDb->zProfile ){
        pDb->interp = interp;
        sqlite3_profile(pDb->db, DbProfileHandler, pDb);
      }else{
        sqlite3_profile(pDb->db, 0, 0);
      }
#endif
    }
    break;
  }

  /*
  **     $db rekey KEY
  **
  ** Change the encryption key on the currently open database.
  */
  case DB_REKEY: {
    int nKey;
    const char *pKey;
    if( objc!=3 ){
      Jim_WrongNumArgs(interp, 2, objv, "KEY");
      return JIM_ERR;
    }
    //pKey = Jim_GetByteArrayFromObj(objv[2], &nKey);
    pKey = Jim_GetString(objv[2], &nKey);
#ifdef SQLITE_HAS_CODEC
    rc = sqlite3_rekey(pDb->db, pKey, nKey);
    if( rc ){
      Jim_SetResultString(interp, sqlite3ErrStr(rc), -1);
      rc = JIM_ERR;
    }
#endif
    break;
  }

  /*    $db restore ?DATABASE? FILENAME
  **
  ** Open a database file named FILENAME.  Transfer the content 
  ** of FILENAME into the local database DATABASE (default: "main").
  */
  case DB_RESTORE: {
    const char *zSrcFile;
    const char *zDestDb;
    sqlite3 *pSrc;
    sqlite3_backup *pBackup;
    int nTimeout = 0;

    if( objc==3 ){
      zDestDb = "main";
      zSrcFile = Jim_String(objv[2]);
    }else if( objc==4 ){
      zDestDb = Jim_String(objv[2]);
      zSrcFile = Jim_String(objv[3]);
    }else{
      Jim_WrongNumArgs(interp, 2, objv, "?DATABASE? FILENAME");
      return JIM_ERR;
    }
    rc = sqlite3_open_v2(zSrcFile, &pSrc, SQLITE_OPEN_READONLY, 0);
    if( rc!=SQLITE_OK ){
      Jim_SetResultFormatted(interp, "cannot open source database: %s", sqlite3_errmsg(pSrc));
      sqlite3_close(pSrc);
      return JIM_ERR;
    }
    pBackup = sqlite3_backup_init(pDb->db, zDestDb, pSrc, "main");
    if( pBackup==0 ){
      Jim_SetResultFormatted(interp, "restore failed: %s", sqlite3_errmsg(pDb->db));
      sqlite3_close(pSrc);
      return JIM_ERR;
    }
    while( (rc = sqlite3_backup_step(pBackup,100))==SQLITE_OK
              || rc==SQLITE_BUSY ){
      if( rc==SQLITE_BUSY ){
        if( nTimeout++ >= 3 ) break;
        sqlite3_sleep(100);
      }
    }
    sqlite3_backup_finish(pBackup);
    if( rc==SQLITE_DONE ){
      rc = JIM_OK;
    }else if( rc==SQLITE_BUSY || rc==SQLITE_LOCKED ){
      Jim_SetResultString(interp, "restore failed: source database busy", -1);
      rc = JIM_ERR;
    }else{
      Jim_SetResultFormatted(interp, "restore failed: %s", sqlite3_errmsg(pDb->db));
      rc = JIM_ERR;
    }
    sqlite3_close(pSrc);
    break;
  }

  /*
  **     $db status (step|sort)
  **
  ** Display SQLITE_STMTSTATUS_FULLSCAN_STEP or 
  ** SQLITE_STMTSTATUS_SORT for the most recent eval.
  */
  case DB_STATUS: {
    int v;
    const char *zOp;
    if( objc!=3 ){
      Jim_WrongNumArgs(interp, 2, objv, "(step|sort)");
      return JIM_ERR;
    }
    zOp = Jim_String(objv[2]);
    if( strcmp(zOp, "step")==0 ){
      v = pDb->nStep;
    }else if( strcmp(zOp, "sort")==0 ){
      v = pDb->nSort;
    }else{
      Jim_SetResultString(interp, "bad argument: should be step or sort", -1);
      return JIM_ERR;
    }
    Jim_SetResultInt(interp, v);
    break;
  }
  
  /*
  **     $db timeout MILLESECONDS
  **
  ** Delay for the number of milliseconds specified when a file is locked.
  */
  case DB_TIMEOUT: {
    long ms;
    if( objc!=3 ){
      Jim_WrongNumArgs(interp, 2, objv, "MILLISECONDS");
      return JIM_ERR;
    }
    if( Jim_GetLong(interp, objv[2], &ms) ) return JIM_ERR;
    sqlite3_busy_timeout(pDb->db, ms);
    break;
  }
  
  /*
  **     $db total_changes
  **
  ** Return the number of rows that were modified, inserted, or deleted 
  ** since the database handle was created.
  */
  case DB_TOTAL_CHANGES: {
    if( objc!=2 ){
      Jim_WrongNumArgs(interp, 2, objv, "");
      return JIM_ERR;
    }
    Jim_SetResultInt(interp, sqlite3_total_changes(pDb->db));
    break;
  }

  /*    $db trace ?CALLBACK?
  **
  ** Make arrangements to invoke the CALLBACK routine for each SQL statement
  ** that is executed.  The text of the SQL is appended to CALLBACK before
  ** it is executed.
  */
  case DB_TRACE: {
    if( objc>3 ){
      Jim_WrongNumArgs(interp, 2, objv, "?CALLBACK?");
      return JIM_ERR;
    }else if( objc==2 ){
      if( pDb->zTrace ){
        Jim_AppendString(interp, Jim_GetResult(interp), pDb->zTrace, -1);
      }
    }else{
      const char *zTrace;
      int len;
      if( pDb->zTrace ){
        Jim_Free(pDb->zTrace);
      }
      zTrace = Jim_GetString(objv[2], &len);
      if( zTrace && len>0 ){
        pDb->zTrace = Jim_Alloc( len + 1 );
        memcpy(pDb->zTrace, zTrace, len+1);
      }else{
        pDb->zTrace = 0;
      }
#ifndef SQLITE_OMIT_TRACE
      if( pDb->zTrace ){
        pDb->interp = interp;
        sqlite3_trace(pDb->db, DbTraceHandler, pDb);
      }else{
        sqlite3_trace(pDb->db, 0, 0);
      }
#endif
    }
    break;
  }

  /*    $db transaction [-deferred|-immediate|-exclusive] SCRIPT
  **
  ** Start a new transaction (if we are not already in the midst of a
  ** transaction) and execute the TCL script SCRIPT.  After SCRIPT
  ** completes, either commit the transaction or roll it back if SCRIPT
  ** throws an exception.  Or if no new transation was started, do nothing.
  ** pass the exception on up the stack.
  **
  ** This command was inspired by Dave Thomas's talk on Ruby at the
  ** 2005 O'Reilly Open Source Convention (OSCON).
  */
  case DB_TRANSACTION: {
    Jim_Obj *pScript;
    const char *zBegin = "SAVEPOINT _tcl_transaction";
    if( objc!=3 && objc!=4 ){
      Jim_WrongNumArgs(interp, 2, objv, "[TYPE] SCRIPT");
      return JIM_ERR;
    }

    if( pDb->nTransaction==0 && objc==4 ){
      static const char *TTYPE_strs[] = {
        "deferred",   "exclusive",  "immediate", 0
      };
      enum TTYPE_enum {
        TTYPE_DEFERRED, TTYPE_EXCLUSIVE, TTYPE_IMMEDIATE
      };
      int ttype;
      if( Jim_GetEnum(interp, objv[2], TTYPE_strs, &ttype, "transaction type", JIM_ERRMSG | JIM_ENUM_ABBREV) ){
        return JIM_ERR;
      }
      switch( (enum TTYPE_enum)ttype ){
        case TTYPE_DEFERRED:    /* no-op */;                 break;
        case TTYPE_EXCLUSIVE:   zBegin = "BEGIN EXCLUSIVE";  break;
        case TTYPE_IMMEDIATE:   zBegin = "BEGIN IMMEDIATE";  break;
      }
    }
    pScript = objv[objc-1];

    /* Run the SQLite BEGIN command to open a transaction or savepoint. */
    pDb->disableAuth++;
    rc = sqlite3_exec(pDb->db, zBegin, 0, 0, 0);
    pDb->disableAuth--;
    if( rc!=SQLITE_OK ){
      Jim_SetResultString(interp, sqlite3_errmsg(pDb->db), -1);
      return JIM_ERR;
    }
    pDb->nTransaction++;

    /* No NRE in Jim Tcl, so evaluate the script directly, then
    ** call function DbTransPostCmd() to commit (or rollback) the transaction 
    ** or savepoint.  */
    rc = DbTransPostCmd(interp, pDb, Jim_EvalObj(interp, pScript));
    break;
  }

  /*
  **    $db unlock_notify ?script?
  */
  case DB_UNLOCK_NOTIFY: {
#ifndef SQLITE_ENABLE_UNLOCK_NOTIFY
    Jim_SetResultString(interp, "unlock_notify not available in this build", -1);
    rc = JIM_ERR;
#else
    if( objc!=2 && objc!=3 ){
      Jim_WrongNumArgs(interp, 2, objv, "?SCRIPT?");
      rc = JIM_ERR;
    }else{
      void (*xNotify)(void **, int) = 0;
      void *pNotifyArg = 0;

      if( pDb->pUnlockNotify ){
        Jim_DecrRefCount(interp, pDb->pUnlockNotify);
        pDb->pUnlockNotify = 0;
      }
  
      if( objc==3 ){
        xNotify = DbUnlockNotify;
        pNotifyArg = (void *)pDb;
        pDb->pUnlockNotify = objv[2];
        Jim_IncrRefCount(pDb->pUnlockNotify);
      }
  
      if( sqlite3_unlock_notify(pDb->db, xNotify, pNotifyArg) ){
        Jim_SetResultString(interp, sqlite3_errmsg(pDb->db), -1);
        rc = JIM_ERR;
      }
    }
#endif
    break;
  }

  /*
  **    $db update_hook ?script?
  **    $db rollback_hook ?script?
  */
  case DB_UPDATE_HOOK: 
  case DB_ROLLBACK_HOOK: {

    /* set ppHook to point at pUpdateHook or pRollbackHook, depending on 
    ** whether [$db update_hook] or [$db rollback_hook] was invoked.
    */
    Jim_Obj **ppHook; 
    if( choice==DB_UPDATE_HOOK ){
      ppHook = &pDb->pUpdateHook;
    }else{
      ppHook = &pDb->pRollbackHook;
    }

    if( objc!=2 && objc!=3 ){
       Jim_WrongNumArgs(interp, 2, objv, "?SCRIPT?");
       return JIM_ERR;
    }
    if( *ppHook ){
      Jim_SetResult(interp, *ppHook);
      if( objc==3 ){
        Jim_DecrRefCount(interp, *ppHook);
        *ppHook = 0;
      }
    }
    if( objc==3 ){
      assert( !(*ppHook) );
      if( Jim_Length(objv[2])>0 ){
        *ppHook = objv[2];
        Jim_IncrRefCount(*ppHook);
      }
    }

    sqlite3_update_hook(pDb->db, (pDb->pUpdateHook?DbUpdateHandler:0), pDb);
    sqlite3_rollback_hook(pDb->db,(pDb->pRollbackHook?DbRollbackHandler:0),pDb);

    break;
  }

  /*    $db version
  **
  ** Return the version string for this database.
  */
  case DB_VERSION: {
    Jim_SetResultString(interp, sqlite3_libversion(), -1);
    break;
  }


  } /* End of the SWITCH statement */
  return rc;
}

/*
**   sqlite3 DBNAME FILENAME ?-vfs VFSNAME? ?-key KEY? ?-readonly BOOLEAN?
**                           ?-create BOOLEAN? ?-nomutex BOOLEAN?
**
** This is the main Tcl command.  When the "sqlite" Tcl command is
** invoked, this routine runs to process that command.
**
** The first argument, DBNAME, is an arbitrary name for a new
** database connection.  This command creates a new command named
** DBNAME that is used to control that connection.  The database
** connection is deleted when the DBNAME command is deleted.
**
** The second argument is the name of the database file.
**
*/
static int DbMain(Jim_Interp *interp, int objc, Jim_Obj *const*objv){
  SqliteDb *p;
  const char *pKey = 0;
  int nKey = 0;
  const char *zArg;
  char *zErrMsg;
  int i;
  const char *zFile;
  const char *zVfs = 0;
  int flags;

  /* Not threading in Jim, so no mutexing is needed */
  flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;

  if( objc==2 ){
    zArg = Jim_String(objv[1]);
    if( strcmp(zArg,"-version")==0 ){
      Jim_SetResultString(interp, sqlite3_version, -1);
      return JIM_OK;
    }
    if( strcmp(zArg,"-has-codec")==0 ){
#ifdef SQLITE_HAS_CODEC
      Jim_SetResultInt(interp, 1);
#else
      Jim_SetResultInt(interp, 0);
#endif
      return JIM_OK;
    }
  }
  for(i=3; i+1<objc; i+=2){
    zArg = Jim_String(objv[i]);
    if( strcmp(zArg,"-key")==0 ){
      pKey = Jim_GetString(objv[i+1], &nKey);
    }else if( strcmp(zArg, "-vfs")==0 ){
      i++;
      zVfs = Jim_String(objv[i]);
    }else if( strcmp(zArg, "-readonly")==0 ){
      long b;
      if( Jim_GetLong(interp, objv[i+1], &b) ) return JIM_ERR;
      if( b ){
        flags &= ~(SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE);
        flags |= SQLITE_OPEN_READONLY;
      }else{
        flags &= ~SQLITE_OPEN_READONLY;
        flags |= SQLITE_OPEN_READWRITE;
      }
    }else if( strcmp(zArg, "-create")==0 ){
      long b;
      if( Jim_GetLong(interp, objv[i+1], &b) ) return JIM_ERR;
      if( b && (flags & SQLITE_OPEN_READONLY)==0 ){
        flags |= SQLITE_OPEN_CREATE;
      }else{
        flags &= ~SQLITE_OPEN_CREATE;
      }
    }else if( strcmp(zArg, "-nomutex")==0 ){
      long b;
      if( Jim_GetLong(interp, objv[i+1], &b) ) return JIM_ERR;
      if( b ){
        flags |= SQLITE_OPEN_NOMUTEX;
        flags &= ~SQLITE_OPEN_FULLMUTEX;
      }else{
        flags &= ~SQLITE_OPEN_NOMUTEX;
      }
   }else if( strcmp(zArg, "-fullmutex")==0 ){
      long b;
      if( Jim_GetLong(interp, objv[i+1], &b) ) return JIM_ERR;
      if( b ){
        flags |= SQLITE_OPEN_FULLMUTEX;
        flags &= ~SQLITE_OPEN_NOMUTEX;
      }else{
        flags &= ~SQLITE_OPEN_FULLMUTEX;
      }
    }else{
      Jim_SetResultFormatted(interp, "unknown option: %s", zArg);
      return JIM_ERR;
    }
  }
  if( objc<3 || (objc&1)!=1 ){
    Jim_WrongNumArgs(interp, 1, objv, 
      "HANDLE FILENAME ?-vfs VFSNAME? ?-readonly BOOLEAN? ?-create BOOLEAN?"
      " ?-nomutex BOOLEAN? ?-fullmutex BOOLEAN?"
#ifdef SQLITE_HAS_CODEC
      " ?-key CODECKEY?"
#endif
    );
    return JIM_ERR;
  }
  zErrMsg = 0;
  p = (SqliteDb*)Jim_Alloc( sizeof(*p) );
  memset(p, 0, sizeof(*p));
  zFile = Jim_String(objv[2]);
  sqlite3_open_v2(zFile, &p->db, flags, zVfs);
  if( SQLITE_OK!=sqlite3_errcode(p->db) ){
    zErrMsg = sqlite3_mprintf("%s", sqlite3_errmsg(p->db));
    sqlite3_close(p->db);
    p->db = 0;
  }
#ifdef SQLITE_HAS_CODEC
  if( p->db ){
    sqlite3_key(p->db, pKey, nKey);
  }
#endif
  if( p->db==0 ){
    Jim_SetResultString(interp, zErrMsg, -1);
    Jim_Free((char*)p);
    sqlite3_free(zErrMsg);
    return JIM_ERR;
  }
  p->maxStmt = NUM_PREPARED_STMTS;
  p->interp = interp;
  zArg = Jim_String(objv[1]);
  Jim_CreateCommand(interp, zArg, DbObjCmd, p, DbDeleteCmd);
  return JIM_OK;
}

/*
** Make sure we have a PACKAGE_VERSION macro defined.  This will be
** defined automatically by the TEA makefile.  But other makefiles
** do not define it.
*/
#ifndef PACKAGE_VERSION
# define PACKAGE_VERSION SQLITE_VERSION
#endif

#define EXTERN
/*
** Initialize this module.
**
** This Tcl module contains only a single new Tcl command named "sqlite".
** (Hence there is no namespace.  There is no point in using a namespace
** if the extension only supplies one new name!)  The "sqlite" command is
** used to open a new SQLite database.  See the DbMain() routine above
** for additional information.
*/
EXTERN int Jim_sqlite3Init(Jim_Interp *interp){
  Jim_CreateCommand(interp, "sqlite3", DbMain, 0, 0);
  Jim_PackageProvide(interp, "sqlite3", PACKAGE_VERSION, 0);
  Jim_CreateCommand(interp, "sqlite", DbMain, 0, 0);
  Jim_PackageProvide(interp, "sqlite", PACKAGE_VERSION, 0);
  return JIM_OK;
}
