#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([jim], [0.62], [steveb@workware.net.au])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

# Checks for types
AC_CHECK_TYPES(long long,AC_SUBST(HAVE_LONG_LONG,["#define HAVE_LONG_LONG"]))


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
	[  --disable-fork          do not use fork],
	[
		if test "x$enableval" = "xno" ; then
			AC_MSG_RESULT(* disabling fork)
			EXTRA_CFLAGS="-DHAVE_NO_FORK"
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

AC_CHECK_FUNCS([backtrace geteuid lstat mkstemp fork strptime sysinfo ualarm])

AC_CHECK_FUNCS(vfork,,IGNORE="$IGNORE exec posix signal eventloop")
AC_CHECK_FUNCS(syslog,,IGNORE="$IGNORE syslog")
AC_CHECK_FUNCS(regcomp,,IGNORE="$IGNORE regexp")
AC_CHECK_LIB(dl, dlopen, AC_SUBST(LIBDL,-ldl), IGNORE="$IGNORE load")

# Does mkdir() include a mode_t argument?
AC_CHECK_DECLS(mkdir,
	 [AC_MSG_CHECKING(if mkdir takes 2 arguments)
	  AC_TRY_COMPILE(
		[#include <sys/stat.h>
		#include <sys/types.h>
		],
		[(void)mkdir("dummy");],
		[AC_MSG_RESULT(no)
			AC_DEFINE(MKDIR_ONE_ARG,[],[Define if mkdir(2) takes a single argument (no mode)])
		],
		[AC_MSG_RESULT(yes)],
	)]
)

# Remove extensions in $IGNORE from $jim_extensions

for i in $IGNORE; do
	jim_extensions=`echo "$jim_extensions" | sed -e "s/$i//"`
done

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


AC_SUBST(EXTRA_CFLAGS,$EXTRA_CFLAGS)
AC_SUBST(SRCDIR,`dirname $0`)

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([jim.h])
AC_OUTPUT
