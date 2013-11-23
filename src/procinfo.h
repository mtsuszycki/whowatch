#ifdef HAVE_PROCESS_SYSCTL
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/stat.h>
#endif

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/user.h>
#endif

#ifdef HAVE_LIBKVM
#include <kvm.h>
#endif
