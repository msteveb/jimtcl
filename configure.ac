#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([jim], [0.63], [steveb@workware.net.au])
AC_CONFIG_HEADER(jimautoconf.h)

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

# Checks for types
AC_CHECK_TYPES(long long, AC_SUBST(DEFINE_HAVE_LONG_LONG,["#define HAVE_LONG_LONG 1"]),AC_SUBST(DEFINE_HAVE_LONG_LONG,["#undef HAVE_LONG_LONG"]))

# Shared library support. Because we don't believe in automake!
AC_CANONICAL_HOST
case $host in
*-*-darwin*)
	AC_SUBST(SH_CFLAGS,-dynamic)
	AC_SUBST(SH_LDFLAGS,"-dynamiclib -undefined suppress -flat_namespace");;
*)
	AC_SUBST(SH_CFLAGS,-fPIC)
	AC_SUBST(SH_LDFLAGS,-shared);;
esac

if test -n "$host_alias"; then
	AC_SUBST(CROSS,$host_alias-)
fi

AC_ARG_ENABLE(fork,
	[  --disable-fork          do not use fork (no exec, etc.)],
	[
		if test "x$enableval" = "xno" ; then
			AC_MSG_RESULT(* disabling fork)
			EXTRA_CFLAGS="-DJIM_NOFORK"
		fi
	],
)
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

jim_extensions="stdlib load package readdir glob array clock exec file posix regexp signal tclcompat aio bio eventloop syslog"
AC_ARG_WITH(jim-ext,
	[  --with-jim-ext          specify jim extensions to build (or all, which is the default)],
	[
		if test "x$withval" != "xno" ; then
			if test "x$withval" != "xall" ; then
				jim_extensions="$withval"
			fi
		fi
	]
)
AC_MSG_RESULT(enabling jim extensions: $jim_extensions)
AC_SUBST(JIM_EXTENSIONS,$jim_extensions)
for i in $jim_extensions; do
	EXTRA_CFLAGS="$EXTRA_CFLAGS -Djim_ext_$i"
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
AC_SUBST(JIM_LIBTYPE,$JIM_LIBTYPE)

AC_CHECK_FUNCS([ualarm sysinfo lstat fork vfork])
AC_CHECK_FUNCS([backtrace geteuid mkstemp realpath strptime])
AC_CHECK_FUNCS([regcomp waitpid sigaction sys_signame sys_siglist])

AC_SUBST(EXTRA_CFLAGS,$EXTRA_CFLAGS)
AC_SUBST(SRCDIR,`dirname $0`)

AC_SEARCH_LIBS(dlopen, dl,
	AC_SUBST(LIBDL,${ac_cv_search_dlopen%none required})
	AC_DEFINE([HAVE_DLOPEN],[1],[Have the dlopen function])
)

# Now that we know what the platform supports:

# For all known extensions:
# - If it is disabled, remove it
# - Otherwise, check to see if it's pre-requisites are met
# -   If yes, add it if it is enabled or is a default
# -   If no, error if it is enabled, or do nothing otherwise

needs_regexp="ac_cv_func_regcomp"
needs_syslog="ac_cv_func_syslog"
needs_exec="ac_cv_func_vfork"
needs_posix="ac_cv_func_waitpid"
needs_load="ac_cv_search_dlopen"
needs_signal="ac_cv_func_sigaction"
needs_readdir="ac_cv_func_opendir"

ext_add=$(echo $with_jim_ext | tr ' ' '\n')
ext_del=$(echo $with_out_jim_ext | tr ' ' '\n')
for i in $ext_all; do
	AC_MSG_CHECKING(extension $i)
	echo "$ext_del" | grep "^$i\$" >/dev/null
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
	echo "$ext_add" | grep "^$i\$" >/dev/null
	if test $? -eq 0; then
		if test $met -eq 0; then
			AC_MSG_ERROR(dependencies not met)
		fi
		AC_MSG_RESULT(enabled)
		ext="$ext $i"
		continue
	fi
	echo "$ext_default" | grep "^$i\$" >/dev/null
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
	
ext=$(echo $ext | tr '\n' ' ')
AC_MSG_RESULT(Jim extensions: $ext)
AC_SUBST(JIM_EXTENSIONS,$ext)
for i in $ext; do
	EXTRA_CFLAGS="$EXTRA_CFLAGS -Djim_ext_$i"
done

AC_SUBST(EXTRA_CFLAGS,$EXTRA_CFLAGS)
AC_SUBST(SRCDIR,`dirname $0`)

AC_CONFIG_FILES([Makefile jimautoconfext.h])
AC_OUTPUT
