/* 
 * Builtin proc plugin and sys plugin.
 * Get process info (ppid, tpgid, name of executable and so on).
 * This is OS dependent: in Linux reading files from "/proc" is
 * needed, in FreeBSD and OpenBSD sysctl() is used (which
 * gives better performance)
 */
#include <err.h>
#include "config.h"
#include "pluglib.h"
#include "whowatch.h"

#if defined(HAVE_LIBKVM) && defined(HAVE_STDINT_H) && defined(HAVE_KVM_H)
#include <stdint.h>
#include <kvm.h>
kvm_t *kd;
extern int can_use_kvm;
#endif

#define EXEC_FILE	128
#define elemof(x)	(sizeof (x) / sizeof*(x))
#define endof(x)	((x) + elemof(x))

static inline void no_info(void)
{
	println("Information unavailable.\n");
}

static inline char *_read_link(const char *path)
{
	static char buf[128];
	bzero(buf, sizeof buf);
	if(readlink(path, buf, sizeof buf) == -1)
		return 0;
	return buf;
}

static void read_link(int pid, char *name)
{
	char *v;
	char pbuf[32];
	snprintf(pbuf, sizeof pbuf, "/proc/%d/%s", pid, name); 
	v = _read_link(pbuf);
	if(!v) {
		no_info();
		return;
	}
	println(v);
	newln();
}

struct netconn_t {
	struct list_head n_list;
	struct list_head n_hash;
	short valid;
	unsigned int inode, state;
	unsigned int s_addr, d_addr;
	unsigned int s_port, d_port;
};

static char *tcp_state[] = {
	"TCP_ESTABILISHED",
	"TCP_SYN_SENT",
	"TCP_SYN_RECV",
	"TCP_FIN_WAIT1",
	"TCP_FIN_WAIT2",
	"TCP_TIME_WAIT",
	"TCP_CLOSE",
	"TCP_CLOSE_WAIT",
	"TCP_LAST_ACK",
	"TCP_LISTEN",
	"TCP_CLOSING"
};

#define HASH_SIZE 	128
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

static inline char *ip_addr(struct netconn_t *t)
{
	return inet_ntoa(*((struct in_addr*)&t->d_addr));
}

static inline int hash(int n)
{
	return n&(HASH_SIZE-1);
}

static LIST_HEAD(tcp_l);
static LIST_HEAD(tcp_blocks);
static struct list_head tcp_hashtable[HASH_SIZE];

static void hash_init(struct list_head *hash)
{
	int i = HASH_SIZE;
	do { 
		INIT_LIST_HEAD(hash);
		hash++;
		i--;
	} while(i);
}
	

static inline void add_to_hash(struct netconn_t *c, int inode)
{
	list_add(&c->n_hash, tcp_hashtable + hash(inode));
}		

static struct netconn_t *tcp_find(unsigned int inode, struct list_head *head) 
{
	struct list_head *h, *tmp;
	struct netconn_t *t;
	tmp  = head + hash(inode);
	list_for_each(h, tmp) {
		t = list_entry(h, struct netconn_t, n_hash);
		if(inode == t->inode) {
			return t;
}			
	}
	return 0;
}


static struct netconn_t *new_netconn(unsigned int inode, struct netconn_t *src)
{
	struct netconn_t *t = 0;
	t = get_empty(sizeof *t, &tcp_blocks);
	memcpy(t, src, sizeof *t);
//	t->used = 1;
	t->inode = inode;
	add_to_hash(t, inode);
	list_add(&t->n_list, &tcp_l);
	return t;
}

static struct netconn_t *validate(unsigned int inode, char *s)
{
	struct netconn_t t, *tmp;
	int i, offset = 2 * sizeof(struct list_head) + sizeof(int) + sizeof(short);
bzero(&t, sizeof(t));
	i = sscanf(s, "%x:%x %x:%x %x", &t.s_addr, &t.s_port, 
			&t.d_addr, &t.d_port, &t.state);
	if(i != 5) return 0;
	tmp = tcp_find(inode, tcp_hashtable);
	if(!tmp) {

		tmp =  new_netconn(inode, &t);		
		return tmp;
	}	
//	t.used = tmp->used = 0;
	if(!memcmp((char*)&t + offset,(char*)tmp + offset, sizeof(t) - offset)) {
		return tmp; 
	}


memcpy((char*)&t + offset, (char*)tmp + offset, sizeof(t) - offset);
/*
	tmp->s_addr = t.s_addr;	
	tmp->s_addr = t.s_addr;	
	tmp->d_addr = t.d_addr;	
	tmp->s_port = t.s_port;	
	tmp->d_port = t.d_port;	
	tmp->state = t.state;
*/
	return tmp;
}

/* 
 * Read all TCP connections from /proc/net/tcp. 
 */
static void read_tcp_conn(void)
{
	FILE *f;
	char buf[127], *tmp;
	int i;
	unsigned int inode;
	static int flag = 0;
return; //function not finished yet, more testing required
	if(!flag) {
		hash_init(tcp_hashtable); 
		flag = 1;
	}	
	if(!(f = fopen("/proc/net/tcp", "r"))) return;
	/* ignore the header */
	tmp = fgets(buf, sizeof buf, f);
	while(fgets(buf, sizeof buf, f)) {
		i = strlen(buf) - 1;
		for(tmp = buf + i; *tmp == ' ' && tmp>buf; tmp--);
		while(isdigit(*tmp) && tmp>buf) tmp--;
		if(sscanf(++tmp, "%d", &inode) != 1) continue;
		if(!(tmp = strchr(buf, ':'))) continue;
		tmp += 2;
		validate(inode, tmp);
	}
	fclose(f);
	return;
}

/*
 * Print resolved IP:port->IP:port inside process details
 * subwindow.
 */
static void print_net_conn(struct netconn_t *t)
{
	char buf[64];
	snprintf(buf, sizeof buf, "%s:%d", inet_ntoa(*((struct in_addr*) &t->s_addr)),
		t->s_port);
	boldon();
	print(buf);
	if(t->d_addr) 
		print(" -> %s:%d", inet_ntoa(*((struct in_addr*)&t->d_addr)), t->d_port);
	print(" %s", tcp_state[t->state-1]);
	boldoff();
}

static int show_net_conn(char *s)
{
	struct netconn_t *t;
	unsigned int inode = 0;	
return 1;	
	if(sscanf(s, "%d", &inode) != 1) return 1;
	t = tcp_find(inode, tcp_hashtable);
	if(!t) {
		return 0;
	}
	print_net_conn(t);
	return 1;
}

/*
static void del_conn(void)
{
	struct list_head *h;
	struct netconn_t *t;
////dolog(__FUNCTION__": looking for %d (hash %d)\n", inode, hash(inode));
	list_for_each(h, &tcp_l) {
		t = list_entry(h, struct netconn_t, n_list);
		if(t->valid	
*/
#include <sys/types.h>
#include <dirent.h>

/* 
 * Show opened file descriptors. Periodically call read_tcp_conn
 * to update stored TCP connections. Don't do it too often, because
 * it can generate load (/proc/net/tcp can be quite large).
 */
void open_fds(int pid, char *name)
{
	DIR *d;
	char *s;
	char buf[32];
	struct dirent *dn;
	static long long count = 0;
	snprintf(buf, sizeof buf, "/proc/%d/fd", pid);
	d = opendir(buf);
	if(!d) {
		no_info();
		return;
	}
	if(!count || ticks - count >= 2) {
		//write(1, "\a", 1);	
		read_tcp_conn();
		count = ticks;
	}	
	while((dn = readdir(d))) {
		if(dn->d_name[0] == '.') continue;
		print("%s - ", dn->d_name);
		snprintf(buf, sizeof buf, "/proc/%d/fd/%s", pid, dn->d_name);
		s = _read_link(buf);
		if(!s) no_info();
		else {
			//if(!strncmp("socket:[", s, 8) && show_net_conn(s+8));
			//else 
			show_net_conn(s+8);
			println("%s", s);
			//print("\n");
			newln();
		}	
	}
	closedir(d);
}

/*
 * Returns int value at 'pos' position from proc file
 * that contains numbers separated by spaces. 
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

static void read_proc_file(char *name, char *start, char *end)
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
		no_info();
		return;
	}
	if(!start) ok = 1;
	while(fgets(buf, sizeof buf, f)) {
		if(!ok && !strncmp(buf, start, slen)) ok = 1;
		if(end && !strncmp(buf, end, elen)) goto END;
		if(!ok) continue;
		println(buf);
		//newln();
	}
END:	
	if(!ok) no_info();
	fclose(f);
}	

static void read_meminfo(int pid, char *name)
{
	char buf[32];
	snprintf(buf, sizeof buf, "/proc/%d/status", pid);
	read_proc_file(buf, "Uid", "VmLib");
}

#define START_TIME_POS	21

/*
 * Returns time the process  
 * started (in jiffies) after system boot.
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


static time_t get_boot_time(void)
{
	char buf[32];
	static time_t boot_time;
	FILE *f;

	if(boot_time) return boot_time;
	if(!(f = fopen("/proc/stat", "r"))) return boot_time;
	while(fgets(buf, sizeof buf, f)) if(!strncmp(buf, "btime ", 6)) goto FOUND;
	fclose(f);
	return boot_time;
FOUND:
	fclose(f);
	sscanf(buf+5, "%ld", &boot_time);
       	return boot_time;
}		

#include <asm/param.h>	// for HZ

static void proc_starttime(int pid, char *name)
{
	unsigned long i, sec;
	char *s;
	time_t btime = get_boot_time();
	
	i = p_start_time(pid);
	if(i == -1 || !btime) {
		no_info();
		return;
	}
	sec = btime + i/HZ;
	s = ctime(&sec);
	print("%s", s);
}

struct proc_detail_t {
        char *title;             /* title of a particular information	*/
        void (* fn)(int pid, char *name);  
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


void eproc(void *p)
{
        int i;
	int pid = !p?1:*(u32*)p;
        struct proc_detail_t *t;
	int size;
	plgn_out_start();
//print("PID %d\n", pid);newln();
	size = sizeof proc_details_t / sizeof(struct proc_detail_t);
	for(i = 0; i < size; i++) {
                t = &proc_details_t[i];
		title("%s", t->title);
		t->fn(pid, t->name);
	}
}

static inline void print_boot_time(void)
{
	time_t btime = get_boot_time();
	if(btime) print("%s",ctime(&btime));
	else no_info();
}

struct cpu_info_t {
	unsigned long long u_mode, nice, s_mode, idle;
};

static struct cpu_info_t c_info, p_info, eff_info;
static struct cpu_info_t *cur_cpu_info = &c_info;
static struct cpu_info_t *prev_cpu_info = &p_info;

/*
 * Get current CPU load values and leave previous one.
 * (just a pointers replacement)
 */
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
	tmp = prev_cpu_info;
	prev_cpu_info = cur_cpu_info;
	cur_cpu_info = tmp;
	i = sscanf(buf+5, "%lld %lld %lld %lld", &tmp->u_mode, &tmp->nice,
		&tmp->s_mode, &tmp->idle);
	fclose(f);
	if(i != 4) return -1;
	eff_info.u_mode = cur_cpu_info->u_mode - prev_cpu_info->u_mode;
	eff_info.nice = cur_cpu_info->nice - prev_cpu_info->nice;
	eff_info.s_mode = cur_cpu_info->s_mode - prev_cpu_info->s_mode;
	eff_info.idle = cur_cpu_info->idle - prev_cpu_info->idle;
	return 0;
}

static inline float prcnt(unsigned long i, unsigned long v)
{
	if(!v) return 0;
	return ((float)(i*100))/(float)v;
}

/* 
 * Based on values taken by fill_cpu_info() calculate
 * and print CPU load (user, system, nice and idle).
 */
static void get_cpu_info()
{
	char buf[64];
	unsigned long z;
	if(fill_cpu_info() == -1) no_info();
	z = eff_info.u_mode + eff_info.nice + eff_info.s_mode + eff_info.idle;
	snprintf(buf, sizeof buf, "%.1f%% user %.1f%% sys %.1f%% nice %.1f%% idle\n",
		prcnt(eff_info.u_mode, z),
		prcnt(eff_info.s_mode, z),
		prcnt(eff_info.nice, z),
		prcnt(eff_info.idle, z)
		);
	print("%s", buf);
}

void esys(void *unused)
{
	int c;
	plgn_out_start();

	title("BOOT TIME: ");
	print_boot_time();
	title("CPU: ");
	get_cpu_info();
	title("MEMORY:"); newln();
	read_proc_file("/proc/meminfo", "MemTotal:", 0);
	title("USED FILES: "); ;

	c = read_file_pos("/proc/sys/fs/file-nr", 2);
	if(c == -1) no_info();
	else println("%d", c);
	title("\nUSED INODES: ");

	c = read_file_pos("/proc/sys/fs/inode-nr", 2);
	if(c == -1) no_info();
	else println("%d", c);
	
	title("\nMAX FILES: ");
	read_proc_file("/proc/sys/fs/file-max", 0, 0);
	title("MAX INODES: ");
	read_proc_file("/proc/sys/fs/inode-max", 0, 0);
	title("\nSTAT:"); newln();

	read_proc_file("/proc/stat", "cpu", "intr");
	title("\nLOADED MODULES:"); newln();

	read_proc_file("/proc/modules", 0, 0);
	title("\nFILESYSTEMS:"); newln();

	read_proc_file("/proc/filesystems", 0, 0);
	title("\nPARTITIONS:"); newln();

	read_proc_file("/proc/partitions", 0, 0);
	title("\nDEVICES:\n"); newln();

	read_proc_file("/proc/devices", 0, 0);
}	

