/*
 *  Jan Bobrowski <jb@wizard.ae.krakow.pl>
 *  version: 20000508
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "config.h"

#ifdef HAVE_PROCESS_SYSCTL
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#endif

#include "proctree.h"

#define PROCDIR "/proc"
#define HASHSIZE 256

#define list_add(l,p,f) ({			\
	(p)->f.nx = (l);			\
	(p)->f.ppv = &(l);			\
	if(l) (l)->f.ppv = &((p)->f.nx);	\
	(l) = (p);				\
})
		
#define list_del(p,f) ({				\
	*(p)->f.ppv = (p)->f.nx;			\
	if((p)->f.nx) (p)->f.nx->f.ppv = (p)->f.ppv;	\
})

#define is_on_list(p,f) (!!(p)->f.ppv)

#define change_head(a,b,f) ({b=a; if(a) a->f.ppv=&b;})

#ifdef HAVE_PROCESS_SYSCTL
int get_all_info(struct kinfo_proc **);
#endif 

struct pinfo {
	int pid;
	int ppid;
};

#ifndef HAVE_PROCESS_SYSCTL
static inline int get_pinfo(struct pinfo* i,DIR* d)
{
	static char name[32] = PROCDIR "/";
	struct dirent* e;

	for(;;) {
		int f,n;
		char buf[64],*p;

		e=readdir(d);
		if(!e) return 0;
		if(!isdigit(e->d_name[0])) continue;
		sprintf(name+sizeof PROCDIR,"%s/stat",e->d_name);
		f=open(name,0);
		if(!f) continue;
		n=read(f,buf,63);
		close(f);
		if(n<0) continue;
		buf[n]=0;
		p = strrchr(buf+4,')') + 4;
		i->ppid = atoi(p);
		i->pid = atoi(buf);
		if(i->pid<=0) continue;
		break;
	}
	return 1;
}
#endif

static struct proc_t proc_init = {1,&proc_init};
static struct proc_t *hash_table[HASHSIZE];
static struct proc_t *main_list = 0;
int num_proc = 1;

static inline int hash_fun(int n)
{
	return n&(HASHSIZE-1);
}

struct proc_t* find_by_pid(int n)
{
	struct proc_t* p;
	if(n<=1) return &proc_init;

	p = hash_table[hash_fun(n)];
	while(p) {
		if(p->pid == n) break;
		p=p->hash.nx;
	}
	return p;
}

static struct proc_t* cache = 0;

static inline void remove_proc(struct proc_t* p)
{
	list_del(p,hash);
	if(cache) free(cache);
	cache = p;
	num_proc--;
}

static inline struct proc_t* new_proc(int n)
{
	struct proc_t* p = cache;
	cache = 0;
	if(!p)
		p = (struct proc_t*) malloc(sizeof *p);
	memset(p,0,sizeof *p);
	p->pid = n;

	list_add(hash_table[hash_fun(n)], p, hash);
	num_proc++;

	return p;
}

static struct proc_t *validate_proc(int pid)
{
	struct proc_t* p;
	if(pid<=1) return &proc_init;

	p = find_by_pid(pid);
	if(p)
		list_del(p,mlist);
	else
		p = new_proc(pid);
	list_add(main_list,p,mlist);
	return p;
}

static inline void change_parent(struct proc_t* p,struct proc_t* q)
{
	if(is_on_list(p,broth))
		list_del(p,broth);
	list_add(q->child,p,broth);
	p->parent = q;
}

int update_tree(void del(void*))
{
#ifdef HAVE_PROCESS_SYSCTL
	struct kinfo_proc *pi;
	int i, el;
#else
	struct pinfo info;
	DIR *d;
#endif
	struct proc_t *p,*q;
	struct proc_t *old_list;
	int n = num_proc;
	change_head(main_list,old_list,mlist);
	main_list = 0;
	
#ifdef HAVE_PROCESS_SYSCTL
	el = get_all_info(&pi);
	for(i = 0; i < el; i++) {
		p = validate_proc(pi[i].kp_proc.p_pid);
		q = validate_proc(pi[i].kp_eproc.e_ppid);		 		
#else

	d=opendir(PROCDIR);
	if(d<0) return -1;

	while( get_pinfo(&info,d) ) {
		p = validate_proc(info.pid);
		q = validate_proc(info.ppid);
#endif
		if(p->parent != q){
			if(p->priv) del(p->priv);

			change_parent(p,q);
		}
	}
#ifdef HAVE_PROCESS_SYSCTL
	free(pi);
#else
	closedir(d);
#endif
	n = num_proc - n;

	for(p = old_list; p; p=q) {
		while(p->child)
			change_parent(p->child,&proc_init);
		if(is_on_list(p,broth))
			list_del(p,broth);
		q = p->mlist.nx;
		if(p->priv) del(p->priv);
		remove_proc(p);
		n++;
	}

	return n;
}

/* ---------------------- */

static struct proc_t *proc, *root;

struct proc_t* tree_start(int root_pid, int start_pid)
{
	root = find_by_pid(root_pid);
	if (!root) return 0;
	proc = find_by_pid(start_pid);
	return proc;
}

struct proc_t* tree_next()
{
	if(proc->child) {
		proc = proc->child;
	} else for(;;) {
		if(proc->broth.nx) {
			proc = proc->broth.nx;
			break;
		}
		
		proc = proc->parent;
		if(proc == root) {
			proc = 0;
			break;
		}
	}
	return proc;
}

static char buf[2+2*PROC_DEPTH];

char* tree_string(int root, struct proc_t *p)
{
	struct proc_t *q;
	char *s;
	int i, d;

	i = 0;
	for(q=p; q->pid!=root && q->pid!=1; q=q->parent)
		i++;
	d = i;

	while(i>PROC_DEPTH)
		p=p->parent, i--;

	s = buf + 2*i;
	s[1] = 0;
	s[0] = '.';

	if(d<=PROC_DEPTH) {
		s[0] = '-';
		if(i > 0 && !p->broth.nx) {
			*--s = '`';
			goto loop;
		}
	}

	while(i > 0) {
		*--s = p->broth.nx ? '|' : ' ';
loop:
		*--s = ' ';
		p=p->parent, i--;
	}

	return buf;
}
