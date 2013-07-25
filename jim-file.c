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
#include <sys/stat.h>

#include <jimautoconf.h>
#include <jim-subcmd.h>

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

    if (Jim_SetDictKeysVector(interp, container, &nameobj, 1, valobj, JIM_ERRMSG) != JIM_OK) {
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

    if (Jim_SetDictKeysVector(interp, container, &nameobj, 1, valobj, JIM_ERRMSG) != JIM_OK) {
        Jim_FreeObj(interp, nameobj);
        Jim_FreeObj(interp, valobj);
        return JIM_ERR;
    }
    return JIM_OK;
}

static int StoreStatData(Jim_Interp *interp, Jim_Obj *varName, const struct stat *sb)
{
    if (set_array_int_value(interp, varName, "dev", sb->st_dev) != JIM_OK) {
        Jim_SetResultFormatted(interp, "can't set \"%#s(dev)\": variable isn't array", varName);
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

    if (!p && path[0] == '.' && path[1] == '.' && path[2] == '\0') {
        Jim_SetResultString(interp, "..", -1);
    } else if (!p) {
        Jim_SetResultString(interp, ".", -1);
    }
    else if (p == path) {
        Jim_SetResultString(interp, "/", -1);
    }
#if defined(__MINGW32__) || defined(_MSC_VER)
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
#if defined(__MINGW32__) || defined(_MSC_VER)
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
#ifdef X_OK
    return file_access(interp, argv[0], X_OK);
#else
    /* XXX: X_OK doesn't work under Windows.
     * In any case, may need to add .exe, etc. so just lie!
     */
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
        argc++;
        argv--;
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

    if (argc == 2) {
#ifdef HAVE_UTIMES
        jim_wide newtime;
        struct timeval times[2];

        if (Jim_GetWide(interp, argv[1], &newtime) != JIM_OK) {
            return JIM_ERR;
        }

        times[1].tv_sec = times[0].tv_sec = newtime;
        times[1].tv_usec = times[0].tv_usec = 0;

        if (utimes(Jim_String(argv[0]), times) != 0) {
            Jim_SetResultFormatted(interp, "can't set time on \"%#s\": %s", argv[0], strerror(errno));
            return JIM_ERR;
        }
#else
        Jim_SetResultString(interp, "Not implemented", -1);
        return JIM_ERR;
#endif
    }
    if (file_stat(interp, argv[0], &sb) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_SetResultInt(interp, sb.st_mtime);
    return JIM_OK;
}

static int file_cmd_copy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_EvalPrefix(interp, "file copy", argc, argv);
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
        Jim_SetResultFormatted(interp, "couldn't readlink \"%#s\": %s", argv[0], strerror(errno));
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
#ifdef HAVE_MKSTEMP
    {   "tempfile",
        "?template?",
        file_cmd_tempfile,
        0,
        1,
        /* Description: Creates a temporary filename */
    },
#endif
    {   "rename",
        "?-force? source dest",
        file_cmd_rename,
        2,
        3,
        /* Description: Renames a file */
    },
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
        "name var",
        file_cmd_stat,
        2,
        2,
        /* Description: Stores results of stat in var array */
    },
    {   "lstat",
        "name var",
        file_cmd_lstat,
        2,
        2,
        /* Description: Stores results of lstat in var array */
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
    const int cwd_len = 2048;
    char *cwd = malloc(cwd_len);

    if (getcwd(cwd, cwd_len) == NULL) {
        Jim_SetResultString(interp, "Failed to get pwd", -1);
        return JIM_ERR;
    }
#if defined(__MINGW32__) || defined(_MSC_VER)
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
