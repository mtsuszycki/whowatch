dnl Process this file with autoconf to produce a configure script.
AC_INIT([whowatch], [1.8.5], [http://github.com/mtsuszycki/whowatch/issues], [whowatch])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_SRCDIR([src/whowatch.c])
AC_CONFIG_HEADER(src/config.h)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

LDFLAGS="$LDFLAGS -rdynamic"

dnl Checks for libraries.
dnl Replace `main' with a function in -lkvm:
AC_CHECK_LIB(kvm, kvm_openfiles)
dnl Replace `main' with a function in -lncurses:
CURS=0
AC_CHECK_LIB(ncurses, scrollok, [CURS=1 ; LIBS="$LIBS -lncurses"])
if test "$CURS" !=  "1"; then
	AC_CHECK_LIB(curses, scrollok, [CURS=1 ; LIBS="$LIBS -lcurses"])
fi
if test "$CURS" != "1"; then
	AC_MSG_ERROR([Could not find proper curses library])
fi

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/ioctl.h sys/time.h unistd.h)
AC_CHECK_HEADERS(curses.h)
AC_CHECK_HEADERS([kvm.h stdint.h])
AC_CHECK_HEADERS([asm/param.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_STRUCT_ST_RDEV
AC_HEADER_TIME

AC_MSG_CHECKING([whether utmp has USER_PROCESS type])
AC_TRY_COMPILE([
#include <sys/types.h>
#include <utmp.h>
],
[int i=USER_PROCESS;],[AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_USER_PROCESS],[1],[define if utmp has USER_PROCESS type])], AC_MSG_RESULT(no))

AC_MSG_CHECKING([whether utmp has DEAD_PROCESS type])
AC_TRY_COMPILE([
#include <sys/types.h>
#include <utmp.h>
],
[int i=DEAD_PROCESS;],[AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_DEAD_PROCESS],[1],[define if utmp has DEAD_PROCESS type])], AC_MSG_RESULT(no))

AC_MSG_CHECKING([whether utmp struct has ut_pid member])
AC_TRY_COMPILE([
#include <sys/types.h>
#include <utmp.h>
],
[struct utmp u;int i=u.ut_pid;],[AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_UTPID],[1],[define if utmp struct has ut_pid member])], AC_MSG_RESULT(no))

AC_MSG_CHECKING([whether utmp struct has ut_name member])
AC_TRY_COMPILE([
#include <sys/types.h>
#include <utmp.h>
],
[struct utmp u;char *c=u.ut_name;],[AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_UT_NAME],[1],[define if utmp struct has ut_name member])], AC_MSG_RESULT(no))

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL
AC_CHECK_FUNCS(select)
AC_CHECK_FUNCS(getloadavg)

AC_MSG_CHECKING([whether select() modifies the time value])
AC_TRY_RUN([#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

int main()
{
int retval;
fd_set rfds;
struct timeval tv = {1, 0};
FD_ZERO(&rfds); FD_SET(0,&rfds);
select(1,&rfds,0,0,&tv);
if(tv.tv_sec == 0) exit(0);
else exit(1);
}
],
[AC_MSG_RESULT(yes); AC_DEFINE([RETURN_TV_IN_SELECT],[1],[define if select() modifies the time value])],
AC_MSG_RESULT(no), AC_MSG_RESULT(no))

AC_MSG_CHECKING([whether sysctl() can be used])
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>],
[
int mib[]={CTL_KERN,KERN_PROC,KERN_PROC_PID,1};
size_t len;
sysctl(mib, 4, 0, &len, 0, 0);
],
[AC_MSG_RESULT(yes); HAVE_PROCESS_SYSCTL=yes; AC_DEFINE([HAVE_PROCESS_SYSCTL],[1],[define if sysctl() can be used])],
AC_MSG_RESULT(no))

AC_CHECK_MEMBERS(
[struct kinfo_proc.kp_proc],,,
[#include <sys/types.h>
 #include <sys/param.h>
 #include <sys/sysctl.h>
 #include <sys/proc.h>
 ])

dnl Make symbolic links according to choosen unix dialect
if test "$HAVE_PROCESS_SYSCTL" = ""; then
	os="linux"
else
	os="bsd"
fi
AC_SUBST([os])

AC_OUTPUT(Makefile src/Makefile)

