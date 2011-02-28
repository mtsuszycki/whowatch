#ifdef HAVE_PROCESS_SYSCTL
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_USER_H
#include <sys/user.h>
#endif

#ifdef HAVE_LIBKVM
#include <kvm.h>
#endif
