@BOTTOM@

/* define if you can use sysctl to get process info (bsd systems) */
#undef HAVE_PROCESS_SYSCTL

/* define if there is USER_PROCESS type in utmp */
#undef HAVE_USER_PROCESS

/* define if there is  DEAD_PROCESS type in utmp */
#undef HAVE_DEAD_PROCESS

/* define if utmp structure has ut_pid member */
#undef HAVE_UTPID

/* define if we can use timeval struct after calling select */
#undef RETURN_TV_IN_SELECT

/* define if there is ut_name in utmp structure */
#undef HAVE_UT_NAME 

/* define if have kvm library */
#undef HAVE_LIBKVM
