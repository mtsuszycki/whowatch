/* 
 * Get process info (ppid, tpgid, name of executable and so on).
 * This is OS dependent: in Linux reading files from "/proc" is
 * needed, in FreeBSD and OpenBSD sysctl() is used (which
 * gives better performance)
 */
#include <err.h>
#include <time.h>
#include "whowatch.h"
#include "proctree.h"
#include "config.h"
#include "procinfo.h"

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

/*
 * Get parent pid
 */
int get_ppid(int pid)
{
	static struct procinfo p;
	get_info(pid, &p);
	return p.ppid;
}


/* 
 * Return the complete command line for the process
 */
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
void get_state(struct process *p)
{
  char buf[64];
  struct stat s;
  char state;
  FILE *f;
  
  p->uid = -1;
  snprintf(buf, sizeof buf - 6, "/proc/%d", p->proc->pid);
  if (stat(buf, &s) >= 0) p->uid = s.st_uid;  

  p->state = '?';
  strcat(buf,"/stat");

  if (!(f = fopen(buf,"rt")))
    return;

  if (fscanf(f,"%*d %*s %c", &state) != 1) {
    fclose(f);
    return;
  }

  fclose(f);
  p->state = ((state == 'S') ? ' ' : state);
}

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

