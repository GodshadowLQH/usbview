#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([usbview], [1.0],[385680223@qq.com])
AC_CONFIG_HEADER([config.h])
AC_CANONICAL_SYSTEM
# Checks for programs.
AC_PROG_CC

dnl Initialize automake
AM_INIT_AUTOMAKE(-Wall -Werror foreign)


# Checks for libraries.

# Checks for header files.
#AC_HEADER_STDC
#AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
#AC_C_CONST
#AC_TYPE_SIZE_T

# Checks for library functions.
#AC_FUNC_MALLOC
#AC_FUNC_STAT
#AC_CHECK_FUNCS([memset strchr strstr strtol uname strerror])

dnl check enable logging
AC_MSG_CHECKING([enable logging [--enable-log] ])
AC_ARG_ENABLE([log], [AS_HELP_STRING([--enable-log], [enable logging])],
        [log_enabled=$enableval],
        [log_enabled='no'])

if test "x$log_enabled" == "xyes"; then
        AC_MSG_RESULT([yes])
        AC_DEFINE([ENABLE_LOG], [1], [Message logging])
else
        AC_MSG_RESULT([no])
fi
AM_CONDITIONAL(ENABLE_LOG,test "x$log_enabled" = "xyes")


AC_CONFIG_FILES([Makefile
                 test/Makefile
                 test/lsusb/Makefile
                 test/usb-devices/Makefile
                 test/usbapi-test/Makefile])
AC_OUTPUT
