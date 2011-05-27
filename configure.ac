#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([jim], [0.71], [steveb@workware.net.au])
AC_CONFIG_HEADER(jimautoconf.h)

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

# Checks for types
AC_CHECK_TYPES(long long, AC_SUBST(DEFINE_HAVE_LONG_LONG,["#define HAVE_LONG_LONG 1"]),AC_SUBST(DEFINE_HAVE_LONG_LONG,["#undef HAVE_LONG_LONG"]))

AC_CANONICAL_HOST

AC_CHECK_TOOL([AR], ar)
AC_CHECK_TOOL([RANLIB], ranlib, true)

iswin=no
# Shared library support. Because we don't believe in automake!
case $host in
*-*-darwin*)
    AC_SUBST(SH_CFLAGS,-dynamic)
    AC_SUBST(SH_LDFLAGS,"-dynamiclib -undefined suppress -flat_namespace")
    AC_SUBST(SHOBJ_CFLAGS,"-dynamic -fno-common")
    AC_SUBST(SHOBJ_LDFLAGS,"-bundle -undefined dynamic_lookup -flat_namespace")
    ;;
*-*-ming*)
    # Use the built-in dlopen wrapper
    have_dlopen=yes
    iswin=yes
    AC_SUBST(SHOBJ_LDFLAGS,-shared)
    AC_SUBST(SH_LDFLAGS,-shared)
    ;;
*-*-cygwin)
    iswin=yes
    AC_SUBST(SHOBJ_LDFLAGS,-shared)
    AC_SUBST(SH_LDFLAGS,-shared)
    ;;
*)
    # Generic Unix settings
    AC_SUBST(LINKFLAGS,-rdynamic)
    AC_SUBST(SH_CFLAGS,-fPIC)
    AC_SUBST(SH_LDFLAGS,-shared)
    AC_SUBST(SHOBJ_CFLAGS,-fPIC)
    AC_SUBST(SHOBJ_LDFLAGS,"-shared -nostartfiles")
    ;;
esac

AC_ARG_ENABLE(math,
    [  --enable-math           include support for math functions],
    [
        if test "x$enableval" = "xyes" ; then
            EXTRA_CFLAGS="$EXTRA_CFLAGS -DJIM_MATH_FUNCTIONS"
        fi
    ]
)
AC_ARG_ENABLE(ipv6,
    [  --enable-ipv6           include ipv6 support in the aio extension],
    [
        if test "x$enableval" = "xyes" ; then
            EXTRA_CFLAGS="$EXTRA_CFLAGS -DJIM_IPV6"
        fi
    ]
)
AC_ARG_ENABLE(utf8,
    [  --enable-utf8           include support for utf8-encoded strings],
    [
        if test "x$enableval" = "xyes" ; then
            dnl Note that utf-8 support requires the built-in regexp
            EXTRA_CFLAGS="$EXTRA_CFLAGS -DJIM_UTF8 -DJIM_REGEXP"
            AC_SUBST(JIM_UTF8,1)
        fi
    ]
)

lineedit=1
AC_ARG_ENABLE(lineedit,
    [  --disable-lineedit      disable line editing],
    [
        if test "x$enableval" = "xno" ; then
            lineedit=0
        fi
    ]
)

# Is $1 in list $2?
in_list()
{
    echo "$2" | tr ' ' '\n' | grep "^$1\$" >/dev/null
}

# Tcl extensions
ext_tcl="stdlib glob tclcompat tree rlprompt oo"
# C extensions
ext_c="load package readdir array clock exec file posix regexp signal aio eventloop syslog nvp readline sqlite sqlite3 win32"

# Tcl extensions which can be modules
ext_tcl_mod="glob tree rlprompt oo"
# C extensions which can be modules
ext_c_mod="readdir array clock file posix regexp syslog readline sqlite sqlite3 win32"

# All extensions
ext_all="$ext_c $ext_tcl"

# Default static extensions
ext_default="stdlib load package readdir glob array clock exec file posix regexp signal tclcompat aio eventloop syslog"

AC_ARG_WITH(jim-ext,
[  --with-jim-ext="ext1 ext2 ..."

       Specify additional jim extensions to include.
       These are enabled by default:

       aio       - ANSI I/O, including open and socket
       eventloop - after, vwait, update
       array     - Tcl-compatible array command
       clock     - Tcl-compatible clock command
       exec      - Tcl-compatible exec command
       file      - Tcl-compatible file command
       glob      - Tcl-compatible glob command
       readdir   - Required for glob
       package   - Package management with the package command
       load      - Load binary extensions at runtime with load or package
       posix     - Posix APIs including os.fork, os.wait, pid
       regexp    - Tcl-compatible regexp, regsub commands
       signal    - Signal handling
       stdlib    - Built-in commands including lassign, lambda, alias
       syslog    - System logging with syslog
       tclcompat - Tcl compatible read, gets, puts, parray, case, ...

       These are disabled by default:

       nvp       - Name-value pairs C-only API
       oo        - Jim OO extension
       tree      - OO tree structure, similar to tcllib ::struct::tree
       readline  - Interface to libreadline
       rlprompt  - Tcl wrapper around the readline extension
       sqlite    - Interface to sqlite
       sqlite3   - Interface to sqlite3
       win32     - Interface to win32
])
AC_ARG_WITH(out-jim-ext,
[  --with-out-jim-ext="default|ext1 ext2 ..."

       Specify jim extensions to exclude.
       If 'default' is given, the default extensions will not be added.
])
AC_ARG_WITH(jim-extmod,
[  --with-jim-extmod="ext1 ext2 ..."

       Specify jim extensions to build as separate modules (either C or Tcl).
       Note that not all extensions can be built as loadable modules.
])

if test "$with_out_jim_ext" = "default"; then
    # but we always include stdlib
    ext_default=stdlib
    with_out_jim_ext=
fi

# Check valid extension names
for i in $with_jim_ext $with_out_jim_ext $with_jim_extmod; do
    in_list "$i" "$ext_all" || AC_MSG_ERROR([Unknown extension: $i])
done

JIM_LIBTYPE=static
AC_ARG_WITH(jim-shared,
    [  --with-jim-shared       build a shared library instead of a static library],
    [
        if test "x$withval" = "xyes" ; then
            JIM_LIBTYPE=shared
        fi
    ]
)
AC_ARG_ENABLE(shared,
    [  --enable-shared         build a shared library instead of a static library],
    [
        if test "x$enableval" = "xyes" ; then
            JIM_LIBTYPE=shared
        fi
    ]
)
AC_ARG_ENABLE(static,
    [  --enable-static         build a static library instead of a shared library],
    [
        if test "x$enableval" = "xyes" ; then
            JIM_LIBTYPE=static
        fi
    ]
)
AC_SUBST(JIM_LIBTYPE,$JIM_LIBTYPE)

AC_ARG_WITH(jim-regexp,
    [  --with-jim-regexp       use the built-in (Tcl-compatible) regexp, even if POSIX regex is available],
    [
        if test "x$withval" = "xyes" ; then
            EXTRA_CFLAGS="$EXTRA_CFLAGS -DJIM_REGEXP"
        fi
    ]
)

AC_CHECK_HEADERS([sys/un.h dlfcn.h termios.h])
AC_CHECK_HEADER(termios.h,,lineedit=0)

AC_CHECK_FUNCS([ualarm sysinfo lstat fork vfork])
AC_CHECK_FUNCS([backtrace geteuid mkstemp realpath strptime])
AC_CHECK_FUNCS([regcomp waitpid sigaction sys_signame sys_siglist])
AC_CHECK_FUNCS([syslog opendir readlink sleep usleep pipe inet_ntop getaddrinfo])

# needs_xxx="yyy zzz" means that extension xxx needs environment variables $yyy and $zzz
#           to not be "no"
# dep_xxx="yyy zzz" means that if xxx is selected, so is yyy and zzz
# libs_xxx="yyy zzz" means that if xxx is selected, libs yyy and zzz may be required
libs_aio="socket"
needs_regexp="ac_cv_func_regcomp"
needs_syslog="ac_cv_func_syslog"
needs_exec="ac_cv_func_vfork ac_cv_func_waitpid"
needs_readdir="ac_cv_func_opendir"
dep_glob="readdir"
needs_posix="ac_cv_func_waitpid"
needs_load="have_dlopen"
libs_load="dl"
needs_signal="ac_cv_func_sigaction ac_cv_func_vfork"
needs_readline="ac_cv_search_readline"
dep_rlprompt="readline"
libs_readline="readline"
needs_sqlite="ac_cv_search_sqlite_open"
libs_sqlite="sqlite"
needs_sqlite3="ac_cv_search_sqlite3_open"
libs_sqlite3="sqlite3"
needs_win32="iswin"
dep_tree="oo"

dnl First handle dependencies. If an extension is enabled, also enable its dependency
for i in $ext_default $with_jim_ext; do
    in_list "$i" "$with_out_jim_ext" && continue
    eval "dep=\$dep_$i"
    test -z "$dep" && continue
    with_jim_ext="$with_jim_ext $dep"
done
for i in $with_jim_extmod; do
    eval "dep=\$dep_$i"
    dnl Theoretically, a mod could depend upon something which must be static
    test -z "$dep" && continue
    dnl If already configured static, don't make it a module
    for d in $dep; do
        in_list "$d" "$with_jim_ext" || with_jim_extmod="$with_jim_extmod $d"
    done
done

dnl Now what libraries may we need?
for i in $ext_default $with_jim_ext $with_jim_extmod; do
    in_list "$i" "$with_out_jim_ext" && continue
    eval "libs=\$libs_$i"
    needed_libs="$needed_libs $libs"
done

dnl Don't need to search for libs if the corresponding extensions aren't enabled
if in_list dl "$needed_libs"; then
    AC_SEARCH_LIBS(dlopen, dl, have_dlopen=yes; AC_DEFINE([HAVE_DLOPEN],[1],[Have the dlopen function]))
fi
if in_list readline "$needed_libs"; then
    AC_SEARCH_LIBS(readline, readline, AC_DEFINE([HAVE_READLINE],[1],[Have libreadline]))
fi
if in_list sqlite "$needed_libs"; then
    AC_SEARCH_LIBS(sqlite_open, sqlite, AC_DEFINE([HAVE_SQLITE],[1],[Have libsqlite]))
fi
if in_list sqlite3 "$needed_libs"; then
    AC_SEARCH_LIBS(sqlite3_open, sqlite3, AC_DEFINE([HAVE_SQLITE3],[1],[Have libsqlite3]))
fi
if in_list socket "$needed_libs"; then
    AC_SEARCH_LIBS(socket, socket)
fi

dnl Look for environ. If it is declared in unistd, no need to declare it
AC_MSG_CHECKING([environ declared in unistd.h?])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#define _GNU_SOURCE
#include <unistd.h>
int main(int argc, char **argv) { char **ep = environ; }
  ]])], [
    AC_MSG_RESULT([yes])
    AC_DEFINE(NO_ENVIRON_EXTERN, [1], [No need to declare extern 'environ'.])
  ],
  AC_MSG_RESULT([no])
)

# Now that we know what the platform supports:

# For all known extensions:
# - If it is disabled, remove it
# - Otherwise, check to see if it's pre-requisites are met
# -   If yes, add it if it is enabled or is a default
# -   If no, error if it is enabled, or do nothing otherwise
# - Modules may be either C or Tcl

for i in $ext_all; do
    AC_MSG_CHECKING(extension $i)
    # Disabled?
    in_list "$i" "$with_out_jim_ext"
    if test $? -eq 0; then
        AC_MSG_RESULT(disabled)
        continue
    fi
    # Check dependencies
    eval "dep=\$needs_$i"
    met=1
    for d in $dep; do
        eval "check=\$$d"
        if test "$check" == "no" -o -z "$check" ; then
            met=0
            break
        fi
    done
    # Selected as a module?
    in_list "$i" "$with_jim_extmod"
    if test $? -eq 0; then
        in_list "$i" "$ext_tcl_mod"
        if test $? -eq 0; then
            dnl Easy, a Tcl module
            AC_MSG_RESULT(tcl)
            extmodtcl="$exmodtcl $i"
            continue
        fi
        in_list "$i" "$ext_c_mod"
        if test $? -ne 0; then
            AC_MSG_ERROR(not a module)
        fi
        if test $met -eq 0; then
            AC_MSG_ERROR(dependencies not met)
        fi
        AC_MSG_RESULT(module)
        extmod="$extmod $i"
        continue
    fi
    # Selected as a static extension?
    in_list "$i" "$with_jim_ext"
    if test $? -eq 0; then
        if test $met -eq 0; then
            AC_MSG_ERROR(dependencies not met)
        fi
        AC_MSG_RESULT(enabled)
        ext="$ext $i"
        continue
    fi
    # Enabled by default?
    in_list "$i" "$ext_default"
    if test $? -eq 0; then
        if test $met -eq 0; then
            AC_MSG_RESULT(disabled (dependencies))
            continue
        fi
        AC_MSG_RESULT(enabled (default))
        ext="$ext $i"
        continue
    fi
    AC_MSG_RESULT(not enabled)
done

AC_MSG_RESULT(Jim static extensions:$ext)
AC_SUBST(JIM_EXTENSIONS,$ext)
if test -n "$extmod"; then
case "$iswin,$JIM_LIBTYPE" in
yes,static)
    AC_MSG_FAILURE([cygwin/mingw require --with-jim-shared for dynamic modules])
    ;;
esac
    AC_MSG_RESULT(Jim dynamic extensions:$extmod)
    AC_SUBST(JIM_MOD_EXTENSIONS,$extmod)
fi
if test -n "$extmodtcl"; then
    AC_MSG_RESULT(Jim Tcl extensions:$extmodtcl)
    AC_SUBST(JIM_TCL_EXTENSIONS,$extmodtcl)
fi
for i in $ext; do
    EXTRA_CFLAGS="$EXTRA_CFLAGS -Djim_ext_$i"
done

if test $iswin = yes; then
    in_list "aio" "$ext $extmod" && LIBS="$LIBS -lwsock32"
    EXTRA_OBJS="$EXTRA_OBJS jim-win32compat.o"
fi
if test $lineedit -eq 1; then
    EXTRA_CFLAGS="$EXTRA_CFLAGS -DUSE_LINENOISE"
    EXTRA_OBJS="$EXTRA_OBJS linenoise.o"
fi


AC_SUBST(EXTRA_CFLAGS,$EXTRA_CFLAGS)
AC_SUBST(EXTRA_OBJS,$EXTRA_OBJS)
AC_SUBST(TCL_LIBRARY,$libdir/jim)
AC_SUBST(PLATFORM_OS,`uname -s`)
AC_SUBST(PLATFORM_PLATFORM,unix)

AC_CONFIG_FILES([Makefile jim-config.h])
AC_OUTPUT
