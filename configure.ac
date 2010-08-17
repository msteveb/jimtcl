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
	[  --with-jim-ext      Specify jim extensions to build],
	[
		if test "x$withval" != "xno" ; then
			jim_extensions="$withval"
		fi
	]
)
AC_MSG_RESULT(enabling jim extensions: $jim_extensions)
for i in $jim_extensions; do
	AC_SUBST(JIM_EXTENSIONS,$jim_extensions)
done

AC_CHECK_FUNCS([ualarm],EXTRA_CFLAGS="$EXTRA_CFLAGS -DHAVE_UALARM")

AC_SUBST(JIM_EXTENSIONS,$JIM_EXTENSIONS)
AC_SUBST(EXTRA_CFLAGS,$EXTRA_CFLAGS)

AC_CHECK_LIB(dl, dlopen,AC_SUBST(LIBDL,-ldl))

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
