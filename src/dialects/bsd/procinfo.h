#ifdef HAVE_PROCESS_SYSCTL
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/stat.h>
#endif

#ifdef __FreeBSD__
#include <sys/user.h>
#endif

#ifdef HAVE_LIBKVM
#include <kvm.h>
#endif
