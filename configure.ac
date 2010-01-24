#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([jim], [0.61], [steveb@workware.net.au])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

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

jim_extensions="package readdir glob array clock exec file posix regexp signal tcl6 aio bio eventloop syslog"
AC_ARG_WITH(jim-ext,
	[  --with-jim-ext          Specify jim extensions to build (or all, which is the default)],
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
	dnl AC_DEFINE_UNQUOTED(with_jim_ext_$i)
	EXTRA_CFLAGS="$EXTRA_CFLAGS -Dwith_jim_ext_$i"
done

JIM_LIBTYPE=static
AC_ARG_WITH(jim-shared,
	[  --with-jim-shared       Build a shared library instead of a static library],
	[
		if test "x$withval" = "xyes" ; then
			JIM_LIBTYPE=shared
		fi
	]
)
AC_SUBST(JIM_LIBTYPE,$JIM_LIBTYPE)

AC_CHECK_FUNCS([ualarm],EXTRA_CFLAGS="$EXTRA_CFLAGS -DHAVE_UALARM")
AC_CHECK_FUNCS([sysinfo],EXTRA_CFLAGS="$EXTRA_CFLAGS -DHAVE_SYSINFO")

AC_SUBST(EXTRA_CFLAGS,$EXTRA_CFLAGS)
AC_SUBST(SRCDIR,`dirname $0`)

AC_CHECK_LIB(dl, dlopen,AC_SUBST(LIBDL,-ldl))

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
