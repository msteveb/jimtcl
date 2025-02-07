#ifndef JIMIOCOMPAT_H
#define JIMIOCOMPAT_H

/*
 * Cross-platform compatibility functions and types for I/O.
 * Currently used by jim-aio.c and jim-exec.c
 */

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

#include "jimautoconf.h"
#include <jim.h>
#include <jim-win32compat.h>

/**
 * Set an error result based on errno and the given message.
 */
void Jim_SetResultErrno(Jim_Interp *interp, const char *msg);

/**
 * Opens the file for writing (and appending if append is true).
 * Returns the file descriptor, or -1 on failure.
 */
int Jim_OpenForWrite(const char *filename, int append);

/**
 * Opens the file for reading.
 * Returns the file descriptor, or -1 on failure.
 */
int Jim_OpenForRead(const char *filename);

#if defined(__MINGW32__) || defined(_WIN32)
    #ifndef STRICT
    #define STRICT
    #endif
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <fcntl.h>
    #include <io.h>
    #include <process.h>

    typedef HANDLE phandle_t;
    #define JIM_BAD_PHANDLE INVALID_HANDLE_VALUE

    /* These seem to accord with the conventions used by msys/mingw32 */
    #define WIFEXITED(STATUS) (((STATUS) & 0xff00) == 0)
    #define WEXITSTATUS(STATUS) ((STATUS) & 0x00ff)
    #define WIFSIGNALED(STATUS) (((STATUS) & 0xff00) != 0)
    #define WTERMSIG(STATUS) (((STATUS) >> 8) & 0xff)
    #define WNOHANG 1

    /**
     * Unix-compatible errno
     */
    int Jim_Errno(void);

    long waitpid(phandle_t phandle, int *status, int nohang);
    /* Like waitpid() but takes a pid and returns a phandle */
    phandle_t JimWaitPid(long processid, int *status, int nohang);
    /* Return pid for a phandle */
    long JimProcessPid(phandle_t phandle);

    #define HAVE_PIPE
    #define pipe(P) _pipe((P), 0, O_NOINHERIT)

    typedef struct __stat64 jim_stat_t;
    #define Jim_Stat _stat64
    #define Jim_FileStat _fstat64
    #define Jim_Lseek _lseeki64
    #define O_TEXT _O_TEXT
    #ifndef STDIN_FILENO
    #define STDIN_FILENO 0
    #endif

#else
    #if defined(HAVE_STAT64)
        typedef struct stat64 jim_stat_t;
        #define Jim_Stat stat64
        #if defined(HAVE_FSTAT64)
            #define Jim_FileStat fstat64
        #endif
        #if defined(HAVE_LSTAT64)
            #define Jim_LinkStat lstat64
        #endif
    #else
        typedef struct stat jim_stat_t;
        #define Jim_Stat stat
        #if defined(HAVE_FSTAT)
            #define Jim_FileStat fstat
        #endif
        #if defined(HAVE_LSTAT)
            #define Jim_LinkStat lstat
        #endif
    #endif
    #if defined(HAVE_LSEEK64)
        #define Jim_Lseek lseek64
    #else
        #define Jim_Lseek lseek
    #endif

    #if defined(HAVE_UNISTD_H)
        #include <unistd.h>
        #include <fcntl.h>
        #include <sys/wait.h>

        typedef int phandle_t;
        #define Jim_Errno() errno
        #define JIM_BAD_PHANDLE -1
        #define JimProcessPid(PIDTYPE) (PIDTYPE)
        #define JimWaitPid waitpid

        #ifndef HAVE_EXECVPE
            #define execvpe(ARG0, ARGV, ENV) execvp(ARG0, ARGV)
        #endif
    #endif

    #ifndef O_TEXT
        #define O_TEXT 0
    #endif

#endif

/* jim-file.c */
/* Note that this is currently an internal function only.
 * It does not form part of the public Jim API
 */
int Jim_FileStoreStatData(Jim_Interp *interp, Jim_Obj *varName, const jim_stat_t *sb);

#endif
