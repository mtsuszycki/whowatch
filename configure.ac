#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([whowatch], [1.6.1])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/whowatch.c])
AC_CONFIG_HEADERS([src/config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB

LDFLAGS="$LDFLAGS -rdynamic"

# Checks for libraries.
AC_CHECK_LIB(kvm, kvm_openfiles)

CURS=0
AC_CHECK_LIB(ncurses, scrollok, [CURS=1 ; LIBS="$LIBS -lncurses"])
if test "$CURS" !=  "1"; then
	AC_CHECK_LIB(curses, scrollok, [CURS=1 ; LIBS="$LIBS -lcurses"])
fi
if test "$CURS" != "1"; then
	AC_MSG_ERROR([Could not find proper curses library])
fi
AC_CHECK_LIB(dl, dlopen, [LIBS="$LIBS -ldl"])

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/ioctl.h sys/time.h unistd.h)
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/param.h sys/user.h sys/time.h termios.h unistd.h utmp.h curses.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_STRUCT_ST_RDEV
AC_HEADER_TIME
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_CHECK_MEMBERS([struct utmp.ut_pid, struct utmp.ut_name], [], [], [[
#include <sys/types.h>
#include <utmp.h>
]])
AC_CHECK_DECLS([USER_PROCESS, DEAD_PROCESS], [], [], [[
#include <sys/types.h>
#include <utmp.h>
]])

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([bzero gettimeofday inet_ntoa isascii memset regcomp select strchr strerror strncasecmp strrchr getloadavg])

AC_MSG_CHECKING([whether select() modifies the time value])
AC_TRY_RUN([#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
int retval;
fd_set rfds;
struct timeval tv = {1, 0};
FD_ZERO(&rfds); FD_SET(1,&rfds);
select(1,&rfds,NULL,NULL,&tv);
if(tv.tv_sec == 0) exit(0);
else exit(1);
}
],
[AC_MSG_RESULT(yes)
AC_DEFINE([RETURN_TV_IN_SELECT], [1],
          [Define to 1 if select() modifies the time value])],
AC_MSG_RESULT(no), AC_MSG_RESULT(no))

AC_MSG_CHECKING([whether sysctl() can be used])
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>],
[
int mib[]={CTL_KERN,KERN_PROC,KERN_PROC_PID,1};
int len;
sysctl(mib, 4, 0, &len, 0, 0);
],
[AC_MSG_RESULT(yes)
AC_DEFINE([HAVE_PROCESS_SYSCTL], [1],
          [Define to 1 if sysctl() can be used (BSD systems)])]
ac_cv_func_sysctl=yes,
AC_MSG_RESULT(no)
ac_cv_func_sysctl=no)

dnl Set SYSTEM according to choosen unix variant
if test "x$ac_cv_func_sysctl" = "xyes"; then
  system="bsd"
else
  system="linux"
fi

AC_SUBST([SYSTEM], $system)

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/sys/Makefile
                 src/sys/bsd/Makefile
                 src/sys/linux/Makefile])
AC_OUTPUT
