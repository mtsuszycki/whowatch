/* 
 * Get process info (ppid, tpgid, name of executable and so on).
 * This is OS dependent: in Linux reading files from "/proc" is
 * performed, in FreeBSD and OpenBSD sysctl() is used (which
 * gives better performance)
 */
#include <err.h>
#include "whowatch.h"
#include "proctree.h"
#include "config.h"
#include "procinfo.h"

extern int full_cmd;

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
    	if(fscanf(f,"%*d %s %*c %d %*d %*d %*d %d",
    			p->exec_file, &p->ppid, &p->tpgid) != 3)
    		return;
    	fclose(f);	
	p->exec_file[EXEC_FILE] = '\0';
}
#else
int fill_kinfo(struct kinfo_proc *info, int pid)
{
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
	int len = sizeof *info;
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
	
    	p->ppid = info.kp_eproc.e_ppid;
    	p->tpgid = info.kp_eproc.e_tpgid;
    	p->euid = info.kp_eproc.e_pcred.p_svuid;
    	p->stat = info.kp_proc.p_stat;
    	strncpy(p->exec_file, info.kp_proc.p_comm, EXEC_FILE);
    	p->cterm = info.kp_eproc.e_tdev;
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
	sprintf(buf, "/dev/%s", tty);
	if(stat(buf, &s) == -1) return -1;
	return s.st_rdev;
}

/*
 * Find pid of the process which parent doesn't have control terminal.
 * Hopefully it is a pid of the login shell (ut_pid in Linux)
 */
int get_login_pid(char *tty)
{
	int mib[4], len, t, el, i, pid;
	struct kinfo_proc *info;
	struct procinfo p;
	
	/* this is for ftp logins */
	if(!strncmp(tty, "ftp", 3)) 
		return atoi(tty+3);
		
	t = get_term(tty);
	if(t == -1) return -1;
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_TTY;
	mib[3] = t;
	if(sysctl(mib, 4, 0, &len, 0, 0) == -1)
		return -1;
	info = malloc(len);
	if(!info) return -1;
	el = len/sizeof(struct kinfo_proc);
	if(sysctl(mib, 4, info, &len, 0, 0) == -1)
		return -1;
	for(i = 0; i < el; i++) {
		pid = info[i].kp_proc.p_pid;
		get_info(get_ppid(pid), &p);
		if(p.cterm == -1 || p.cterm != t) {
			free(info);
			return pid;
		}
	}
	free(info);
	return -1;
}

/*
 * Scan all processes 
 */
int get_all_info(struct kinfo_proc **info)
{
	int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
	int len, el;

	if(sysctl(mib, 3, 0, &len, 0, 0) == -1)
		return 0;
	*info = malloc(len);
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
	struct procinfo p;
	char *t;
	int c;
        FILE *f;
        int i = 0;
        if(!full_cmd) goto no_full;
        sprintf(buf, "/proc/%d/cmdline",pid);
        if (!(f = fopen(buf, "rt")))
                return "-";
        while (fread(buf+i,1,1,f) == 1){
	        if (buf[i] == '\0') buf[i] = ' ';
                if (i == sizeof buf - 1) break;
                i++;
        }
        fclose(f);
	buf[i] = '\0';
	if(!i) {
no_full:
		get_info(pid, &p);
		t = p.exec_file;
		bzero(buf, sizeof buf);
		c = strlen(t);
		if(p.exec_file[0] == '(') t++;
		if(p.exec_file[--c] == ')') p.exec_file[c] = 0;
		strncpy(buf, t, sizeof buf - 1);
	}	
        return buf;
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
	memcpy(buf, info.kp_proc.p_comm, sizeof buf - 1);
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


/*
 * Get state and owner (effective uid) of a process
 */
#ifdef HAVE_PROCESS_SYSCTL
void get_state(struct process *p)
{
	struct procinfo pi;
	/* state SSLEEP won't be marked in proc tree */
	char s[] = "FR DZ";
	get_info(p->proc->pid, &pi);
	p->uid = pi.euid;
	p->state = s[pi.stat-1];
}
#else
void get_state(struct process *p)
{
	static char buf[256];
	struct stat s;
	char state;
        FILE *f;
        sprintf(buf,"/proc/%d", p->proc->pid);
	p->uid = -1;
	if (stat(buf, &s) >= 0) p->uid = s.st_uid;  
	strcat(buf,"/stat");
        if (!(f = fopen(buf,"rt"))){
               	p->state = '?';
		return;
	}
        fscanf(f,"%*d %*s %c",&state);
	fclose(f);
	p->state = state=='S'?' ':state;
}
#endif

/* 
 * It really shouldn't be in this file.
 * Count idle time.
 */
char *count_idle(char *tty)
{
	struct stat st;
	static char buf[256];
	time_t idle_time;
	
	sprintf(buf,"/dev/%s",tty);
	
	if (stat(buf,&st) == -1) return "?";
	idle_time = time(0) - st.st_atime;	
	
	if (idle_time >= 3600 * 24) 
		sprintf(buf,"%ldd",idle_time/(3600 * 24) );
	else if (idle_time >= 3600){
		time_t min = (idle_time % 3600) / 60;
		if (min < 10)
			sprintf(buf,"%ld:0%ld", idle_time/3600, min);
		else
			sprintf(buf,"%ld:%ld", idle_time/3600, min);
	}
	else if (idle_time >= 60)
		sprintf(buf,"%ld",idle_time/60);
	else
		sprintf(buf," ");
	
	return buf;
}
