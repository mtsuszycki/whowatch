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
	int len, t, el, i, pid, cndt = -1, l;
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
		if(!(pid = info[i].kp_proc.p_pid)) continue;
		get_info(get_ppid(pid), &p);
		if(p.cterm == -1 || p.cterm != t) {
			cndt = pid;
			l = strlen(info[i].kp_proc.p_comm);
			/*
			 * This is our best match: parent of the process
			 * doesn't have controlling terminal and process'
			 * name ends with "sh"
			 *
			 */
			if(l > 1 && !strncmp("sh",info[i].kp_proc.p_comm+l-2,2)) {
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
	int len, el;

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
	struct procinfo p;
	char *t;
	int c;
        FILE *f;
        int i = 0;
        if(!full_cmd) goto no_full;
        memset(buf, 0, sizeof buf);
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
	if(pi.stat == ' ') {
		p->state = '?';
		return;
	}
	p->state = s[pi.stat-1];
}
#else
void get_state(struct process *p)
{
	char buf[256];
	struct stat s;
	char state;
        FILE *f;
        snprintf(buf, sizeof buf - 6, "/proc/%d", p->proc->pid);
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

#ifndef HAVE_GETLOADAVG
int getloadavg(double d[], int l)
{
	FILE *f;
	if(!(f = fopen("/proc/loadavg", "r")))
		return -1;
	if(fscanf(f, "%lf %lf %lf", &d[0], &d[1], &d[2]) != 3) {
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}
#endif


static inline void no_info(struct window *w)
{
	waddstr(w->wd, "Information unavailable.\n");
	w->d_lines++;
}

static inline char *_read_link(const char *path)
{
	static char buf[128];
	bzero(buf, sizeof buf);
	if(readlink(path, buf, sizeof buf) == -1)
		return 0;
	return buf;
}

static void read_link(struct window *w, int pid, char *name)
{
	char *v;
	char pbuf[32];
	snprintf(pbuf, sizeof pbuf, "/proc/%d/%s", pid, name); 
	v = _read_link(pbuf);
	if(!v) {
		no_info(w);
		return;
	}
	waddstr(w->wd, v);
	waddstr(w->wd, "\n");
	w->d_lines++;
}

#include <ctype.h>

static void get_net_conn(struct window *w, char *s)
{
	FILE *f;
	char buf[127], *tmp;
	int i;
	unsigned int inode, l_inode;
	tmp = strchr(s, ']');
	if(!tmp) return;
//write(1, "\a", 1);
	*tmp = 0;
	if(sscanf(s, "%d", &l_inode) != 1) return;
	f = fopen("/proc/net/tcp", "r");
	if(!f) return;
	/* skip titles */
	fgets(buf, sizeof buf, f);
	while(fgets(buf, sizeof buf, f)) {
		i = strlen(buf) - 1;
		for(tmp = buf + i; isdigit(*tmp) && tmp>buf; tmp--);
		if(sscanf(tmp, "%d", &inode) != 1) continue;
		if(inode == l_inode) goto FOUND;
	}
	fclose(f);
	return;
FOUND:
	fclose(f);
	waddstr(w->wd, buf);
	return;
}
		

#include <sys/types.h>
#include <dirent.h>

void open_fds(struct window *w, int pid, char *name)
{
	DIR *d;
	char *s;
	char buf[32];
	struct dirent *dn;
	snprintf(buf, sizeof buf, "/proc/%d/fd", pid);
	d = opendir(buf);
	if(!d) {
		no_info(w);
		return;
	}
	while((dn = readdir(d))) {
		if(dn->d_name[0] == '.') continue;
		waddstr(w->wd, dn->d_name);
		waddstr(w->wd, " -> ");
		snprintf(buf, sizeof buf, "/proc/%d/fd/%s", pid, dn->d_name);
		s = _read_link(buf);
		if(!s) no_info(w);
		else {
			waddstr(w->wd, s);
	//		if(!strncmp("socket:[", s, 8))
	//			get_net_conn(w, s+8);
			waddstr(w->wd, "\n");
			w->d_lines++;
		}	
	}
	closedir(d);
}

/*
 * Returns int value at 'pos' position from proc file
 * that contains values separated by spaces. 
 */
static int read_file_pos(char *name, int pos)
{
	FILE *f;
	int i;
	int c = 1;
	f = fopen(name, "r");
	if(!f) return -1;
	while((i = fgetc(f)) != EOF) {
		if(i == ' ' || i == '\t') {
			c++;
			while((i = fgetc(f)) == ' ');
			ungetc(i, f);
		}
		if(c == pos) goto FOUND;
	}
	fclose(f);
	return -1;
FOUND:
	i = fscanf(f, "%d", &c);
	fclose(f);
	if(i != 1) return -1;
	return c;
}	

static void read_proc_file(struct window *w, char *name, char *start, char *end)
{
	char buf[128];
	int ok = 0;
	int slen, elen;
	FILE *f;
	slen = elen = 0;
	if(start) slen = strlen(start);
	if(end) elen = strlen(end);
	f = fopen(name, "r");
	if(!f) {
		no_info(w);
		return;
	}
	if(!start) ok = 1;
	while(fgets(buf, sizeof buf, f)) {
		if(!ok && !strncmp(buf, start, slen)) ok = 1;
		if(end && !strncmp(buf, end, elen)) goto END;
		if(!ok) continue;
//                waddnstr(w->wd, buf, DETAILS_WIN_COLS-strlen(buf));
		waddstr(w->wd, buf);
		w->d_lines++;
	}
END:	
	if(!ok) no_info(w);
	fclose(f);
}	

static void read_meminfo(struct window *w, int pid, char *name)
{
	char buf[32];
	snprintf(buf, sizeof buf, "/proc/%d/status", pid);
	read_proc_file(w, buf, "Uid", "VmLib");
}


#define START_TIME_POS	21
/*
 * Returns time the process with pid (argument) 
 *  started in jiffies after system boot.
 */
static unsigned long p_start_time(int pid)
{
	char buf[32];
	FILE *f;
	int i;
	unsigned long  c = 0;
	snprintf(buf, sizeof buf, "/proc/%d/stat", pid);
	f = fopen(buf, "r");
	if(!f) return -1;
	while((i = fgetc(f)) != EOF) {
		if(i == ' ') c++;
		if(c == START_TIME_POS) goto FOUND;
	}
	fclose(f);
	return -1;
FOUND:
	i = fscanf(f, "%ld", &c);
	fclose(f);
	if(i != 1) return -1;
	return c;
}		

static time_t boot_time;

void get_boot_time(void)
{
	char buf[32];
	FILE *f;
	unsigned long c;
	int i = 0;
	snprintf(buf, sizeof buf, "/proc/stat");
	f = fopen(buf, "r");
	if(!f) return;
	while(fgets(buf, sizeof buf, f)) {
		if(i == ' ') c++;
		if(!strncmp(buf, "btime ", 6)) goto FOUND;
	}
	fclose(f);
	return;
FOUND:
	i = sscanf(buf+5, "%ld", &c);
	fclose(f);
	if(i != 1) return;
	boot_time = (time_t) c;
}		

#include <asm/param.h>	// for HZ

static void proc_starttime(struct window *w, int pid, char *name)
{
	unsigned long i, sec;
	char *s;
	i = p_start_time(pid);
	if(i == -1 || !boot_time) {
		no_info(w);
		return;
	}
	sec = boot_time + i/HZ;
	s = ctime(&sec);
	waddstr(w->wd, s);
	w->d_lines++;
}

struct proc_detail_t {
        char *title;             /* title of a particular information	*/
        void (* fn)(struct window *w, int pid, char *name);  
	int t_lines;		/* nr of line for a title		*/    
	char *name;		/* used only to read links		*/
};

struct proc_detail_t proc_details_t[] = {
	{ "START: ", proc_starttime, 0, 0 		},
        { "EXE: ", read_link, 0, "exe" 			},
        { "ROOT: ", read_link, 0, "root" 		},
        { "CWD: ", read_link, 0, "cwd" 			},
	{ "\nSTATUS:\n", read_meminfo, 2, 0 		},
	{ "\nFILE DESCRIPTORS:\n", open_fds, 2, 0 	} 
};

void proc_details(struct window *w, int pid)
{
        int i;
        struct proc_detail_t *t;
        int size = sizeof proc_details_t / sizeof(struct proc_detail_t);
	for(i = 0; i < size; i++) {
                t = &proc_details_t[i];
                wattrset(w->wd, A_BOLD);
		waddstr(w->wd, t->title);
                wattrset(w->wd, A_NORMAL);
                w->d_lines += t->t_lines;
		t->fn(w, pid, t->name);
	}
}

static inline void print_boot_time(struct window *w)
{
	if(boot_time) waddstr(w->wd, ctime(&boot_time));
	else no_info(w);
}

struct cpu_info_t {
	unsigned long long u_mode, nice, s_mode, idle;
};

static struct cpu_info_t c_info, p_info, eff_info;

static struct cpu_info_t *cur_cpu_info = &c_info;
static struct cpu_info_t *prev_cpu_info = &p_info;

static int fill_cpu_info(void)
{
	char buf[64];
	FILE *f;
	struct cpu_info_t *tmp;
	int i = 0;
	snprintf(buf, sizeof buf, "/proc/stat");
	f = fopen(buf, "r");
	if(!f) return -1;
	while(fgets(buf, sizeof buf, f)) 
		if(!strncmp(buf, "cpu  ", 5)) goto FOUND;
	fclose(f);
	return -1;
FOUND:
	tmp = cur_cpu_info;
	cur_cpu_info = prev_cpu_info;
	prev_cpu_info = tmp;
	i = sscanf(buf+5, "%lld %lld %lld %lld", &tmp->u_mode, &tmp->nice,
		&tmp->s_mode, &tmp->idle);
	fclose(f);
	if(i != 4) return -1;
	eff_info.u_mode = prev_cpu_info->u_mode - cur_cpu_info->u_mode;
	eff_info.nice = prev_cpu_info->nice - cur_cpu_info->nice;
	eff_info.s_mode = prev_cpu_info->s_mode - cur_cpu_info->s_mode;
	eff_info.idle = prev_cpu_info->idle - cur_cpu_info->idle;
	return 0;
}

static inline float prcnt(unsigned long i, unsigned long v)
{
	if(!v) return 0;
	return ((float)(i*100))/(float)v;
}

static void get_cpu_info(struct window *w)
{
	char buf[64];
	unsigned long z;
	if(fill_cpu_info() == -1) no_info(w);
	z = eff_info.u_mode + eff_info.nice + eff_info.s_mode + eff_info.idle;
	snprintf(buf, sizeof buf, "%.1f%% user %.1f%% sys %.1f%% nice %.1f%% idle\n",
		prcnt(eff_info.u_mode, z),
		prcnt(eff_info.s_mode, z),
		prcnt(eff_info.nice, z),
		prcnt(eff_info.idle, z)
		);
	waddstr(w->wd, buf);
}

void sys_info(struct window *w, int i)
{
	char buf[32];
	int c;
	waddstr(w->wd, "BOOT TIME: ");
	print_boot_time(w);
	waddstr(w->wd, "CPU: ");
	get_cpu_info(w);
	waddstr(w->wd, "MEMORY:\n");
	read_proc_file(w, "/proc/meminfo", "MemTotal:", 0);
	waddstr(w->wd, "\nUSED FILES: ");
	c = read_file_pos("/proc/sys/fs/file-nr", 2);
	if(c == -1) no_info(w);
	else {
		snprintf(buf, sizeof buf, "%d\n", c);
		waddstr(w->wd, buf);
	}
	waddstr(w->wd, "USED INODES: ");
	c = read_file_pos("/proc/sys/fs/inode-nr", 2);
	if(c == -1) no_info(w);
	else {
		snprintf(buf, sizeof buf, "%d\n", c);
		waddstr(w->wd, buf);
	}
	waddstr(w->wd, "MAX FILES: ");
	read_proc_file(w, "/proc/sys/fs/file-max", 0, 0);
	waddstr(w->wd, "MAX INODES: ");
	read_proc_file(w, "/proc/sys/fs/inode-max", 0, 0);
	waddstr(w->wd, "\nSTAT:\n");
	read_proc_file(w, "/proc/stat", "disk", "intr");
	waddstr(w->wd, "\nLOADED MODULES:\n");
	read_proc_file(w, "/proc/modules", 0, 0);
	waddstr(w->wd, "\nFILESYSTEMS:\n");
	read_proc_file(w, "/proc/filesystems", 0, 0);
	waddstr(w->wd, "\nPARTITIONS:\n");
	read_proc_file(w, "/proc/partitions", 0, 0);
	waddstr(w->wd, "\nDEVICES:\n");
	read_proc_file(w, "/proc/devices", 0, 0);
	w->d_lines += 16;
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

