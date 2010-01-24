#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([jim], [0.60], [steveb@workware.net.au])

AC_SUBST(TARGET_PLATFORM,$ac_cv_host)

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

AC_ARG_ENABLE(fork,
	[  --disable-fork          Do not use fork (no exec, etc.)],
	[
		if test "x$enableval" = "xno" ; then
			AC_MSG_RESULT(* disabling fork)
			JIM_NOFORK=1
		fi
	],
)
AC_SUBST(JIM_NOFORK,$JIM_NOFORK)

jim_extensions=
AC_ARG_WITH(jim-ext,
	[  --with-jim-ext      Specify jim extensions to build (or all)],
	[
		if test "x$withval" != "xno" ; then
			if test "x$withval" = "xall" ; then
				jim_extensions="package readdir glob array clock exec file posix regexp signal tcl6 eventloop aio syslog"
			else
				jim_extensions="$withval"
			fi
		fi
	]
)
AC_MSG_RESULT(enabling jim extensions: $jim_extensions)
AC_SUBST(JIM_EXTENSIONS,$jim_extensions)
for i in $jim_extensions; do
	dnl AC_DEFINE_UNQUOTED(with_jim_ext_$i)
	EXTRA_CFLAGS="$EXTRA_CFLAGS -Dwith_jim_ext_$i"
done

AC_CHECK_FUNCS([ualarm],EXTRA_CFLAGS="$EXTRA_CFLAGS -DHAVE_UALARM")

AC_SUBST(JIM_EXTENSIONS,$JIM_EXTENSIONS)
AC_SUBST(EXTRA_CFLAGS,$EXTRA_CFLAGS)
AC_SUBST(SRCDIR,`dirname $0`)

AC_CHECK_LIB(dl, dlopen,AC_SUBST(LIBDL,-ldl))

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
