/* 
 * Get process info (ppid, tpgid, name of executable and so on).
 * This is OS dependent: in Linux reading files from "/proc" is
 * needed, in FreeBSD and OpenBSD sysctl() is used (which
 * gives better performance)
 */
#include <err.h>
#include "whowatch.h"
#include "proctree.h"
#include "config.h"
#include "procinfo.h"

#ifdef HAVE_LIBKVM
kvm_t *kd;
extern int can_use_kvm;
#endif

#define EXEC_FILE	128
#define elemof(x)	(sizeof (x) / sizeof*(x))
#define endof(x)	((x) + elemof(x))

struct procinfo
{
	int ppid;			/* parent pid		*/
	int tpgid;			/* tty process group id */
	int cterm;			/* controlling terminal	*/
	int euid;			/* effective uid	*/
	char stat;			/* process status	*/
	char exec_file[EXEC_FILE+1];	/* executable name	*/
};

/*
 * Get process info
 */
#ifndef HAVE_PROCESS_SYSCTL
void get_info(int pid, struct procinfo *p)
{
    	char buf[32];
    	FILE *f;

	p->ppid = -1;
	p->cterm = -1;
	/*
	 *  getting uid by stat() on "/proc/pid" is in
	 * separate function, see below get_stat() 
	 */
	p->euid = -1;
	p->stat = ' ';
	p->tpgid = -1;
	strcpy(p->exec_file, "can't access");
    	snprintf(buf, sizeof buf, "/proc/%d/stat", pid);
    	if (!(f = fopen(buf,"rt"))) 
    		return;
    	if(fscanf(f,"%*d %128s %*c %d %*d %*d %*d %d",
    			p->exec_file, &p->ppid, &p->tpgid) != 3) {
    		fclose(f);
    		return;
    	}
    	fclose(f);	
	p->exec_file[EXEC_FILE] = '\0';
}
#else
int fill_kinfo(struct kinfo_proc *info, int pid)
{
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
	size_t len = sizeof *info;
	if(sysctl(mib, 4, info, &len, 0, 0) == -1) 
		return -1;
	return len?0:-1;
}
		
void get_info(int pid, struct procinfo *p)
{
	struct kinfo_proc info;	
	p->ppid = -1;
	p->cterm = -1;
	p->euid = -1;
	p->stat = ' ';
	p->tpgid = -1;
	strcpy(p->exec_file, "can't access");
	
	if(fill_kinfo(&info, pid) == -1) return;
	
	p->ppid = info.kinfo_ppid;
	p->tpgid = info.kinfo_tpgid;
	p->euid = info.kinfo_svuid;
	p->stat = info.kinfo_stat;
	strncpy(p->exec_file, info.kinfo_comm, EXEC_FILE);
	p->cterm = info.kinfo_tdev;
	p->exec_file[EXEC_FILE] = '\0';
}
#endif

/*
 * Get parent pid
 */
int get_ppid(int pid)
{
	static struct procinfo p;
	get_info(pid, &p);
	return p.ppid;
}

#ifdef HAVE_PROCESS_SYSCTL
/*
 * Get terminal
 */
int get_term(char *tty)
{
	struct stat s;
	char buf[32];
	memset(buf, 0, sizeof buf);
	snprintf(buf, sizeof buf - 1,  "/dev/%s", tty);
	if(stat(buf, &s) == -1) return -1;
	return s.st_rdev;
}

/*
 * Find pid of the process which parent doesn't have control terminal.
 * Hopefully it is a pid of the login shell (ut_pid in Linux)
 */
int get_login_pid(char *tty)
{
	int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_TTY, 0};
	int t, el, i, pid, cndt = -1, l;
	size_t len;
	struct kinfo_proc *info;
	struct procinfo p;
	
	/* this is for ftp logins */
	if(!strncmp(tty, "ftp", 3)) 
		return atoi(tty+3);
		
	if((t = get_term(tty)) == -1) return -1;
	mib[3] = t;
	if(sysctl(mib, 4, 0, &len, 0, 0) == -1)
		return -1;
	info = calloc(1, len);
	if(!info) return -1;
	el = len/sizeof(struct kinfo_proc);
	if(sysctl(mib, 4, info, &len, 0, 0) == -1)
		return -1;
	for(i = 0; i < el; i++) {
		if(!(pid = info[i].kinfo_pid)) continue;
		get_info(get_ppid(pid), &p);
		if(p.cterm == -1 || p.cterm != t) {
			cndt = pid;
			l = strlen(info[i].kinfo_comm);
			/*
			 * This is our best match: parent of the process
			 * doesn't have controlling terminal and process'
			 * name ends with "sh"
			 *
			 */
			if(l > 1 && !strncmp("sh",info[i].kinfo_comm+l-2,2)) {
				free(info);
				return pid;
			}
		}
	}
	free(info);
	return cndt;
}

/*
 * Get information about all system processes
 */
int get_all_info(struct kinfo_proc **info)
{
	int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
	int el;
	size_t len;

	if(sysctl(mib, 3, 0, &len, 0, 0) == -1)
		return 0;
	*info = calloc(1, len);
	if(!*info) return 0;
	el = len/sizeof(struct kinfo_proc);
	if(sysctl(mib, 3, *info, &len, 0, 0) == -1)
		return 0;
	return el;
}
#endif 

/* 
 * Return the complete command line for the process
 */
#ifndef HAVE_PROCESS_SYSCTL
char *get_cmdline(int pid)
{
        static char buf[512];
	int fd, i, n;
	struct procinfo p;
	char *s;
//        if(!full_cmd) goto no_full;
	snprintf(buf, sizeof buf, "/proc/%d/cmdline", pid);
	if((fd = open(buf, O_RDONLY)) == -1)
		return "-";
	n = read(fd, buf, sizeof buf);
	close(fd);
 	if(n == -1) return "-";
	if(!n) goto no_full;
	for(i = 0; i < n; i++) if(!buf[i]) buf[i] = ' ';
	buf[i-1] = 0;
	return buf;
no_full:
	get_info(pid, &p);
	s = p.exec_file;
	n = strlen(p.exec_file);
	if(*s == '(') s++;
	if(p.exec_file[--n] == ')') p.exec_file[n] = 0;
	memcpy(buf, s, n);
	return buf;
//	strncpy(buf, t, sizeof buf - 1);
  //      return buf;
}
#endif

#ifdef HAVE_LIBKVM
int kvm_init()
{
	kd = kvm_openfiles(0, 0, 0, O_RDONLY, 0);
	if(!kd) return 0;
	return 1;
}
#endif

#ifdef HAVE_PROCESS_SYSCTL
char *get_cmdline(int pid)
{
	static char buf[512];
	struct kinfo_proc info;
	
	char **p, *s = buf;
	bzero(buf, sizeof buf);
	if(fill_kinfo(&info, pid) == -1)
		return "-";
	memcpy(buf, info.kinfo_comm, sizeof buf - 1);
	if(!full_cmd) return buf;
#ifdef HAVE_LIBKVM
	if(!can_use_kvm) return buf;
	p = kvm_getargv(kd, &info, 0);
	if(!p) 	return buf;
	for(; *p; p++) {
		*s++ = ' ';	
		strncpy(s, *p, endof(buf) - s);
		s += strlen(*p);
		if(s >= endof(buf) - 1) break;
	}
	buf[sizeof buf - 1] = 0; 
	return buf + 1;
#else
	return buf;
#endif
}
#endif
	 
/* 
 * Get process group ID of the process which currently owns the tty
 * that the process is connected to and return its command line.
 */
char *get_w(int pid)
{
	struct procinfo p;
	get_info(pid, &p);
        return get_cmdline(p.tpgid);
}

/*
 * Get name of the executable
 */
char *get_name(int pid)
{
	static struct procinfo p;
	get_info(pid, &p);
	return p.exec_file;
}

int proc_pid_uid(u32 pid)
{
	char buf[32];
	struct stat s;
        
	snprintf(buf, sizeof buf, "/proc/%d", pid);
	if(stat(buf, &s) == -1) return -1;
	return s.st_uid;
}

int proc_getloadavg(double d[], int l)
{
#ifndef HAVE_GETLOADAVG
	FILE *f;
	int ret = -1;
	if(!(f = fopen("/proc/loadavg", "r"))) return ret;
	if(fscanf(f, "%lf %lf %lf", &d[0], &d[1], &d[2]) == 3)
		ret = 0;
	fclose(f);
	return ret;
#else
	return getloadavg(d, l);
#endif
}

/* 
 * It really shouldn't be in this file.
 * Count idle time.
 */
char *count_idle(char *tty)
{
	struct stat st;
	static char buf[32];
	time_t idle_time;
	
	sprintf(buf,"/dev/%s",tty);
	
	if(stat(buf,&st) == -1) return "?";
	idle_time = time(0) - st.st_atime;	
	
	if (idle_time >= 3600 * 24) 
		sprintf(buf,"%ldd", (long) idle_time/(3600 * 24) );
	else if (idle_time >= 3600){
		time_t min = (idle_time % 3600) / 60;
		sprintf(buf,"%ld:%02ld", (long)idle_time/3600, (long) min);
	}
	else if (idle_time >= 60)
		sprintf(buf,"%ld", (long) idle_time/60);
	else
		sprintf(buf," ");
	
	return buf;
}

