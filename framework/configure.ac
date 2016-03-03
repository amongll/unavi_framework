#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([UNavmeFrame], [0.1], [li.lei@youku.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_CONFIG_SRCDIR([include/unavi/UNaviCommon.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_SUBST([LIBUNAVI_VERSION],[0:1:0])

WITH_TEST=""
AC_ARG_WITH([test],
	AC_HELP_STRING([--with-test],[whether build test]),
	[WITH_TEST="yes"]
)

WITH_DOC=""
AC_ARG_WITH([doc],
	AC_HELP_STRING([--with-doc],[whether build doxygen doc]),
	[WITH_DOC="yes"]
)

WITH_TCMALLOC=""
AC_ARG_WITH([tcmalloc],
	AC_HELP_STRING([--with-tcmalloc],[whether build with tcmalloc]),
	[LDFLAGS="-ltcmalloc"]
)

AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--enable-debug],[debug program(default is no)])],
	[CXXFLAGS="-DDEBUG -g -O0"],
	[CXXFLAGS="-g -O2"])

AM_CONDITIONAL([BUILD_TEST],[test "$WITH_TEST" = "yes"])
AM_CONDITIONAL([BUILD_DOC],[test "$WITH_DOC" = "yes"])
AM_CONDITIONAL([BUILD_TCMALLOC],[test "$WITH_TCMALLOC" = "yes"])

# Checks for programs.
AC_PROG_CXX
AC_PROG_LIBTOOL

# Checks for libraries.
PKG_CHECK_MODULES([jansson],[jansson >= 2.6])

# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_INT64_T

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday])

AC_CONFIG_FILES([Makefile
		libunavi.pc
        include/Makefile
        src/Makefile
        test/Makefile
		doc/Makefile
])
AC_OUTPUT
