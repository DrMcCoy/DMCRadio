AC_INIT(DMCRadio.c)
AM_INIT_AUTOMAKE(DMCRadio,1.1.4)

AC_PROG_CC
AC_PROG_CXX
AC_LANG_C
AC_PROG_INSTALL
AC_PROG_MAKE_SET

CFLAGS="-DYEAR=\"\\\"@YEAR@\\\"\" -DDATE=\"\\\"@DATE@\\\"\" -DDAY=\"\\\"@DAY@\\\"\" -DPREFIX=\"\\\"@PREFIX@\\\"\" -Wall"
LIBS="-lpthread -lm"

AC_HEADER_STDC
AC_CHECK_HEADERS(linux/videodev.h,,AC_MSG_ERROR(linux/videodev.h doesn't exist. Please install a newer version of glibc-devel))
AC_CHECK_LIB(ncurses,main,,AC_MSG_ERROR(Couldn't find ncurses. Please install and try again),,)

VERSION="1.1.4"
YEAR="2003-2004"
DATE="31.12.2004"
DAY="Friday"
PREFIX=${prefix}
AC_SUBST(VERSION)
AC_SUBST(YEAR)
AC_SUBST(DATE)
AC_SUBST(DAY)
AC_SUBST(PREFIX)

AC_OUTPUT(Makefile fonts/Makefile)
