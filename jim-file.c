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
#include <errno.h>

#include <jimautoconf.h>
#include <jim-subcmd.h>
#include <jimiocompat.h>

#ifdef HAVE_UTIMES
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#elif defined(_MSC_VER)
#include <direct.h>
#define F_OK 0
#define W_OK 2
#define R_OK 4
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

# ifndef MAXPATHLEN
# ifdef PATH_MAX
# define MAXPATHLEN PATH_MAX
# else
# define MAXPATHLEN JIM_PATH_LEN
# endif
# endif

#if defined(__MINGW32__) || defined(__MSYS__) || defined(_MSC_VER)
#define ISWINDOWS 1
#else
#define ISWINDOWS 0
#endif

/* extract nanosecond resolution mtime from struct stat */
#if defined(HAVE_STRUCT_STAT_ST_MTIMESPEC)
    #define STAT_MTIME_US(STAT) ((STAT).st_mtimespec.tv_sec * 1000000ll + (STAT).st_mtimespec.tv_nsec / 1000)
#elif defined(HAVE_STRUCT_STAT_ST_MTIM)
    #define STAT_MTIME_US(STAT) ((STAT).st_mtim.tv_sec * 1000000ll + (STAT).st_mtim.tv_nsec / 1000)
#endif

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
#ifdef S_ISCHR
    else if (S_ISCHR(mode)) {
        return "characterSpecial";
    }
#endif
#ifdef S_ISBLK
    else if (S_ISBLK(mode)) {
        return "blockSpecial";
    }
#endif
#ifdef S_ISFIFO
    else if (S_ISFIFO(mode)) {
        return "fifo";
    }
#endif
#ifdef S_ISLNK
    else if (S_ISLNK(mode)) {
        return "link";
    }
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(mode)) {
        return "socket";
    }
#endif
    return "unknown";
}

/*
 *----------------------------------------------------------------------
 *
 * Jim_FileStoreStatData --
 *
 *  This is a utility procedure that breaks out the fields of a
 *  "stat" structure and stores them in textual form into the
 *  elements of an associative array (dict).
 *  The result is also returned as the Tcl result.
 *  If varName is NULL, the result is only returned, not stored.
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
static void AppendStatElement(Jim_Interp *interp, Jim_Obj *listObj, const char *key, jim_wide value)
{
    Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, key, -1));
    Jim_ListAppendElement(interp, listObj, Jim_NewIntObj(interp, value));
}

int Jim_FileStoreStatData(Jim_Interp *interp, Jim_Obj *varName, const jim_stat_t *sb)
{
    /* Just use a list to store the data */
    Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);

    AppendStatElement(interp, listObj, "dev", sb->st_dev);
    AppendStatElement(interp, listObj, "ino", sb->st_ino);
    AppendStatElement(interp, listObj, "mode", sb->st_mode);
    AppendStatElement(interp, listObj, "nlink", sb->st_nlink);
    AppendStatElement(interp, listObj, "uid", sb->st_uid);
    AppendStatElement(interp, listObj, "gid", sb->st_gid);
    AppendStatElement(interp, listObj, "size", sb->st_size);
    AppendStatElement(interp, listObj, "atime", sb->st_atime);
    AppendStatElement(interp, listObj, "mtime", sb->st_mtime);
    AppendStatElement(interp, listObj, "ctime", sb->st_ctime);
#ifdef STAT_MTIME_US
    AppendStatElement(interp, listObj, "mtimeus", STAT_MTIME_US(*sb));
#endif
    Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, "type", -1));
    Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, JimGetFileType((int)sb->st_mode), -1));

    /* Was a variable specified? */
    if (varName) {
        Jim_Obj *objPtr;
        objPtr = Jim_GetVariable(interp, varName, JIM_NONE);

        if (objPtr) {
            Jim_Obj *objv[2];

            objv[0] = objPtr;
            objv[1] = listObj;

            objPtr = Jim_DictMerge(interp, 2, objv);
            if (objPtr == NULL) {
                /* This message matches the one from Tcl */
                Jim_SetResultFormatted(interp, "can't set \"%#s(dev)\": variable isn't array", varName);
                Jim_FreeNewObj(interp, listObj);
                return JIM_ERR;
            }

            Jim_InvalidateStringRep(objPtr);

            Jim_FreeNewObj(interp, listObj);
            listObj = objPtr;
        }
        Jim_SetVariable(interp, varName, listObj);
    }

    /* And also return the value */
    Jim_SetResult(interp, listObj);

    return JIM_OK;
}

/**
 * Give a path of length 'len', returns the length of the path
 * with any trailing slashes removed.
 */
static int JimPathLenNoTrailingSlashes(const char *path, int len)
{
    int i;
    for (i = len; i > 1 && path[i - 1] == '/'; i--) {
        /* Trailing slash, so remove it */
        if (ISWINDOWS && path[i - 2] == ':') {
            /* But on windows, we won't remove the trailing slash from c:/ */
            break;
        }
    }
    return i;
}

/**
 * Give a path in objPtr, returns a new path with any trailing slash removed.
 * Use Jim_DecrRefCount() on the returned object (which may be identical to objPtr).
 */
static Jim_Obj *JimStripTrailingSlashes(Jim_Interp *interp, Jim_Obj *objPtr)
{
    int len = Jim_Length(objPtr);
    const char *path = Jim_String(objPtr);
    int i = JimPathLenNoTrailingSlashes(path, len);
    if (i != len) {
        objPtr = Jim_NewStringObj(interp, path, i);
    }
    Jim_IncrRefCount(objPtr);
    return objPtr;
}

static int file_cmd_dirname(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr = JimStripTrailingSlashes(interp, argv[0]);
    const char *path = Jim_String(objPtr);
    const char *p = strrchr(path, '/');

    if (!p) {
        Jim_SetResultString(interp, ".", -1);
    }
    else if (p[1] == 0) {
        /* Trailing slash so do nothing */
        Jim_SetResult(interp, objPtr);
    }
    else if (p == path) {
        Jim_SetResultString(interp, "/", -1);
    }
    else if (ISWINDOWS && p[-1] == ':') {
        /* z:/dir => z:/ */
        Jim_SetResultString(interp, path, p - path + 1);
    }
    else {
        /* Strip any trailing slashes from the result */
        int len = JimPathLenNoTrailingSlashes(path, p - path);
        Jim_SetResultString(interp, path, len);
    }
    Jim_DecrRefCount(interp, objPtr);
    return JIM_OK;
}

static int file_cmd_split(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *listObj = Jim_NewListObj(interp, NULL, 0);
    const char *path = Jim_String(argv[0]);

    if (*path == '/') {
        Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, "/", 1));
    }

    while (1) {
        /* Remove leading slashes */
        while (*path == '/') {
            path++;
        }
        if (*path) {
            const char *pt = strchr(path, '/');
            if (pt) {
                Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, path, pt - path));
                path = pt;
                continue;
            }
            Jim_ListAppendElement(interp, listObj, Jim_NewStringObj(interp, path, -1));
        }
        break;
    }
    Jim_SetResult(interp, listObj);
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
    Jim_Obj *objPtr = JimStripTrailingSlashes(interp, argv[0]);
    const char *path = Jim_String(objPtr);
    const char *lastSlash = strrchr(path, '/');
    const char *p = strrchr(path, '.');

    if (p == NULL || (lastSlash != NULL && lastSlash >= p)) {
        p = "";
    }
    Jim_SetResultString(interp, p, -1);
    Jim_DecrRefCount(interp, objPtr);
    return JIM_OK;
}

static int file_cmd_tail(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *objPtr = JimStripTrailingSlashes(interp, argv[0]);
    const char *path = Jim_String(objPtr);
    const char *lastSlash = strrchr(path, '/');

    if (lastSlash) {
        Jim_SetResultString(interp, lastSlash + 1, -1);
    }
    else {
        Jim_SetResult(interp, objPtr);
    }
    Jim_DecrRefCount(interp, objPtr);
    return JIM_OK;
}

static int file_cmd_normalize(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
#ifdef HAVE_REALPATH
    const char *path = Jim_String(argv[0]);
    char *newname = Jim_Alloc(MAXPATHLEN + 1);

    if (realpath(path, newname)) {
        Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, newname, -1));
        return JIM_OK;
    }
    else {
        Jim_Free(newname);
        Jim_SetResultFormatted(interp, "can't normalize \"%#s\": %s", argv[0], strerror(errno));
        return JIM_ERR;
    }
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
        else if (ISWINDOWS && strchr(part, ':')) {
            /* Absolute component on mingw, so go back to the start */
            last = newname;
        }
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
            /* but on on Windows, leave the trailing slash on "c:/ " */
            if (!ISWINDOWS || !(last > newname + 2 && last[-2] == ':')) {
                *--last = 0;
            }
        }
    }

    *last = 0;

    /* Probably need to handle some special cases ... */

    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, newname, last - newname));

    return JIM_OK;
}

static int file_access(Jim_Interp *interp, Jim_Obj *filename, int mode)
{
    Jim_SetResultBool(interp, access(Jim_String(filename), mode) != -1);

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
#ifdef X_OK
    return file_access(interp, argv[0], X_OK);
#else
    /* If no X_OK, just assume true. */
    Jim_SetResultBool(interp, 1);
    return JIM_OK;
#endif
}

static int file_cmd_exists(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return file_access(interp, argv[0], F_OK);
}

static int file_cmd_delete(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int force = Jim_CompareStringImmediate(interp, argv[0], "-force");

    if (force || Jim_CompareStringImmediate(interp, argv[0], "--")) {
        argc--;
        argv++;
    }

    while (argc--) {
        const char *path = Jim_String(argv[0]);

        if (unlink(path) == -1 && errno != ENOENT) {
            if (rmdir(path) == -1) {
                /* Maybe try using the script helper */
                if (!force || Jim_EvalPrefix(interp, "file delete force", 1, argv) != JIM_OK) {
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
        {
            char *slash = strrchr(path, '/');

            if (slash && slash != path) {
                *slash = 0;
                if (mkdir_all(path) != 0) {
                    return -1;
                }
                *slash = '/';
            }
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
            jim_stat_t sb;

            if (Jim_Stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
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

static int file_cmd_tempfile(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int fd = Jim_MakeTempFile(interp, (argc >= 1) ? Jim_String(argv[0]) : NULL, 0);

    if (fd < 0) {
        return JIM_ERR;
    }
    close(fd);

    return JIM_OK;
}

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
#if ISWINDOWS
    if (access(dest, F_OK) == 0) {
        /* Windows won't rename over an existing file */
        remove(dest);
    }
#endif
    if (rename(source, dest) != 0) {
        Jim_SetResultFormatted(interp, "error renaming \"%#s\" to \"%#s\": %s", argv[0], argv[1],
            strerror(errno));
        return JIM_ERR;
    }

    return JIM_OK;
}

#if defined(HAVE_LINK) && defined(HAVE_SYMLINK)
static int file_cmd_link(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int ret;
    const char *source;
    const char *dest;
    static const char * const options[] = { "-hard", "-symbolic", NULL };
    enum { OPT_HARD, OPT_SYMBOLIC, };
    int option = OPT_HARD;

    if (argc == 3) {
        if (Jim_GetEnum(interp, argv[0], options, &option, NULL, JIM_ENUM_ABBREV | JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
        argv++;
        argc--;
    }

    dest = Jim_String(argv[0]);
    source = Jim_String(argv[1]);

    if (option == OPT_HARD) {
        ret = link(source, dest);
    }
    else {
        ret = symlink(source, dest);
    }

    if (ret != 0) {
        Jim_SetResultFormatted(interp, "error linking \"%#s\" to \"%#s\": %s", argv[0], argv[1],
            strerror(errno));
        return JIM_ERR;
    }

    return JIM_OK;
}
#endif

static int file_stat(Jim_Interp *interp, Jim_Obj *filename, jim_stat_t *sb)
{
    const char *path = Jim_String(filename);

    if (Jim_Stat(path, sb) == -1) {
        Jim_SetResultFormatted(interp, "could not read \"%#s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    return JIM_OK;
}

#ifdef HAVE_LSTAT
static int file_lstat(Jim_Interp *interp, Jim_Obj *filename, jim_stat_t *sb)
{
    const char *path = Jim_String(filename);

    if (lstat(path, sb) == -1) {
        Jim_SetResultFormatted(interp, "could not read \"%#s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    return JIM_OK;
}
#else
#define file_lstat file_stat
#endif

static int file_cmd_atime(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_stat_t sb;

    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, sb.st_atime);
    return JIM_OK;
}

/**
 * Set file atime/mtime to the given time in microseconds since the epoch.
 */
static int JimSetFileTimes(Jim_Interp *interp, const char *filename, jim_wide us)
{
#ifdef HAVE_UTIMES
    struct timeval times[2];

    times[1].tv_sec = times[0].tv_sec = us / 1000000;
    times[1].tv_usec = times[0].tv_usec = us % 1000000;

    if (utimes(filename, times) != 0) {
        Jim_SetResultFormatted(interp, "can't set time on \"%s\": %s", filename, strerror(errno));
        return JIM_ERR;
    }
    return JIM_OK;
#else
    Jim_SetResultString(interp, "Not implemented", -1);
    return JIM_ERR;
#endif
}

static int file_cmd_mtime(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_stat_t sb;

    if (argc == 2) {
        jim_wide secs;
        if (Jim_GetWide(interp, argv[1], &secs) != JIM_OK) {
            return JIM_ERR;
        }
        return JimSetFileTimes(interp, Jim_String(argv[0]), secs * 1000000);
    }
    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, sb.st_mtime);
    return JIM_OK;
}

#ifdef STAT_MTIME_US
static int file_cmd_mtimeus(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_stat_t sb;

    if (argc == 2) {
        jim_wide us;
        if (Jim_GetWide(interp, argv[1], &us) != JIM_OK) {
            return JIM_ERR;
        }
        return JimSetFileTimes(interp, Jim_String(argv[0]), us);
    }
    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, STAT_MTIME_US(sb));
    return JIM_OK;
}
#endif

static int file_cmd_copy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_EvalPrefix(interp, "file copy", argc, argv);
}

static int file_cmd_size(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_stat_t sb;

    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, sb.st_size);
    return JIM_OK;
}

static int file_cmd_isdirectory(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_stat_t sb;
    int ret = 0;

    if (file_stat(interp, argv[0], &sb) == JIM_OK) {
        ret = S_ISDIR(sb.st_mode);
    }
    Jim_SetResultInt(interp, ret);
    return JIM_OK;
}

static int file_cmd_isfile(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_stat_t sb;
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
    jim_stat_t sb;
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
        Jim_SetResultFormatted(interp, "could not read link \"%#s\": %s", argv[0], strerror(errno));
        return JIM_ERR;
    }
    linkValue[linkLength] = 0;
    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, linkValue, linkLength));
    return JIM_OK;
}
#endif

static int file_cmd_type(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_stat_t sb;

    if (file_lstat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultString(interp, JimGetFileType((int)sb.st_mode), -1);
    return JIM_OK;
}

#ifdef HAVE_LSTAT
static int file_cmd_lstat(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_stat_t sb;

    if (file_lstat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    return Jim_FileStoreStatData(interp, argc == 2 ? argv[1] : NULL, &sb);
}
#else
#define file_cmd_lstat file_cmd_stat
#endif

static int file_cmd_stat(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_stat_t sb;

    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    return Jim_FileStoreStatData(interp, argc == 2 ? argv[1] : NULL, &sb);
}

static const jim_subcmd_type file_command_table[] = {
    {   "atime",
        "name",
        file_cmd_atime,
        1,
        1,
        /* Description: Last access time */
    },
    {   "mtime",
        "name ?time?",
        file_cmd_mtime,
        1,
        2,
        /* Description: Get or set last modification time */
    },
#ifdef STAT_MTIME_US
    {   "mtimeus",
        "name ?time?",
        file_cmd_mtimeus,
        1,
        2,
        /* Description: Get or set last modification time in microseconds */
    },
#endif
    {   "copy",
        "?-force? source dest",
        file_cmd_copy,
        2,
        3,
        /* Description: Copy source file to destination file */
    },
    {   "dirname",
        "name",
        file_cmd_dirname,
        1,
        1,
        /* Description: Directory part of the name */
    },
    {   "rootname",
        "name",
        file_cmd_rootname,
        1,
        1,
        /* Description: Name without any extension */
    },
    {   "extension",
        "name",
        file_cmd_extension,
        1,
        1,
        /* Description: Last extension including the dot */
    },
    {   "tail",
        "name",
        file_cmd_tail,
        1,
        1,
        /* Description: Last component of the name */
    },
    {   "split",
        "name",
        file_cmd_split,
        1,
        1,
        /* Description: Split path into components as a list */
    },
    {   "normalize",
        "name",
        file_cmd_normalize,
        1,
        1,
        /* Description: Normalized path of name */
    },
    {   "join",
        "name ?name ...?",
        file_cmd_join,
        1,
        -1,
        /* Description: Join multiple path components */
    },
    {   "readable",
        "name",
        file_cmd_readable,
        1,
        1,
        /* Description: Is file readable */
    },
    {   "writable",
        "name",
        file_cmd_writable,
        1,
        1,
        /* Description: Is file writable */
    },
    {   "executable",
        "name",
        file_cmd_executable,
        1,
        1,
        /* Description: Is file executable */
    },
    {   "exists",
        "name",
        file_cmd_exists,
        1,
        1,
        /* Description: Does file exist */
    },
    {   "delete",
        "?-force|--? name ...",
        file_cmd_delete,
        1,
        -1,
        /* Description: Deletes the files or directories (must be empty unless -force) */
    },
    {   "mkdir",
        "dir ...",
        file_cmd_mkdir,
        1,
        -1,
        /* Description: Creates the directories */
    },
    {   "tempfile",
        "?template?",
        file_cmd_tempfile,
        0,
        1,
        /* Description: Creates a temporary filename */
    },
    {   "rename",
        "?-force? source dest",
        file_cmd_rename,
        2,
        3,
        /* Description: Renames a file */
    },
#if defined(HAVE_LINK) && defined(HAVE_SYMLINK)
    {   "link",
        "?-symbolic|-hard? newname target",
        file_cmd_link,
        2,
        3,
        /* Description: Creates a hard or soft link */
    },
#endif
#if defined(HAVE_READLINK)
    {   "readlink",
        "name",
        file_cmd_readlink,
        1,
        1,
        /* Description: Value of the symbolic link */
    },
#endif
    {   "size",
        "name",
        file_cmd_size,
        1,
        1,
        /* Description: Size of file */
    },
    {   "stat",
        "name ?var?",
        file_cmd_stat,
        1,
        2,
        /* Description: Returns results of stat, and may store in var array */
    },
    {   "lstat",
        "name ?var?",
        file_cmd_lstat,
        1,
        2,
        /* Description: Returns results of lstat, and may store in var array */
    },
    {   "type",
        "name",
        file_cmd_type,
        1,
        1,
        /* Description: Returns type of the file */
    },
#ifdef HAVE_GETEUID
    {   "owned",
        "name",
        file_cmd_owned,
        1,
        1,
        /* Description: Returns 1 if owned by the current owner */
    },
#endif
    {   "isdirectory",
        "name",
        file_cmd_isdirectory,
        1,
        1,
        /* Description: Returns 1 if name is a directory */
    },
    {   "isfile",
        "name",
        file_cmd_isfile,
        1,
        1,
        /* Description: Returns 1 if name is a file */
    },
    {
        NULL
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
    char *cwd = Jim_Alloc(MAXPATHLEN);

    if (getcwd(cwd, MAXPATHLEN) == NULL) {
        Jim_SetResultString(interp, "Failed to get pwd", -1);
        Jim_Free(cwd);
        return JIM_ERR;
    }
    else if (ISWINDOWS) {
        /* Try to keep backslashes out of paths */
        char *p = cwd;
        while ((p = strchr(p, '\\')) != NULL) {
            *p++ = '/';
        }
    }

    Jim_SetResultString(interp, cwd, -1);

    Jim_Free(cwd);
    return JIM_OK;
}

int Jim_fileInit(Jim_Interp *interp)
{
    Jim_PackageProvideCheck(interp, "file");
    Jim_CreateCommand(interp, "file", Jim_SubCmdProc, (void *)file_command_table, NULL);
    Jim_CreateCommand(interp, "pwd", Jim_PwdCmd, NULL, NULL);
    Jim_CreateCommand(interp, "cd", Jim_CdCmd, NULL, NULL);
    return JIM_OK;
}
