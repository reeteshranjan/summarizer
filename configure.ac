#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([summarizer], [0.1], [reeteshr@outlook.com])
AM_INIT_AUTOMAKE([1.10 no-define foreign])
AC_CONFIG_SRCDIR([src/summarizer.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday memmove memset munmap strcasecmp strchr strcspn strspn])

AC_CONFIG_FILES([Makefile
                 dict/Makefile
                 src/Makefile])
AC_OUTPUT
