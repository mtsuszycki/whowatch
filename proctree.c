/*
 *  Copyright (c) 1999 Jan Bobrowski
 *  email: jb@wizard.ae.krakow.pl
 *  licence: GNU LGPL
 *  version: 19990609
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#include "whowatch.h"
#include "proctree.h"

#define PROCDIR "/proc"
#define HASHSIZE 128

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

struct pinfo {
	int pid;
	int ppid;
};

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

static struct proc proc_init = {1,&proc_init};
static struct proc *hash_table[HASHSIZE];
static struct proc *main_list = 0;
int num_proc = 1;
int process_nr = 0;

static inline int hash_fun(int n)
{
	return n&(HASHSIZE-1);
}

struct proc* find_by_pid(int n)
{
	struct proc* p;
	if(n<=1) return &proc_init;

	p = hash_table[hash_fun(n)];
	while(p) {
		if(p->pid == n) break;
		p=p->hash.nx;
	}
	return p;
}

static struct proc* cache = 0;

static inline void remove_proc(struct proc* p)
{
	list_del(p,hash);
	if(cache) free(cache);
	cache = p;
	num_proc--;
}

static inline struct proc* new_proc(int n)
{
	struct proc* p = cache;
	cache = 0;
	if(!p)
		p = (struct proc*) malloc(sizeof *p);
	memset(p,0,sizeof *p);
	p->pid = n;

	list_add(hash_table[hash_fun(n)], p, hash);
	num_proc++;

	return p;
}

static struct proc *validate_proc(int pid)
{
	struct proc* p;
	if(pid<=1) return &proc_init;

	p = find_by_pid(pid);
	if(p)
		list_del(p,mlist);
	else
		p = new_proc(pid);
	list_add(main_list,p,mlist);
	return p;
}

static inline void change_parent(struct proc* p,struct proc* q)
{
	if(is_on_list(p,broth))
		list_del(p,broth);
	list_add(q->child,p,broth);
	p->parent = q;
}

#ifdef USE_PT_PRIV
int update_tree(void del(void*))
#else
int update_tree()
#endif
{
	DIR* d;
	struct pinfo info;
	struct proc *p,*q;
	struct proc *old_list;
	int n = num_proc;
	change_head(main_list,old_list,mlist);
	main_list = 0;

	d=opendir(PROCDIR);
	if(d<0) return -1;

	while( get_pinfo(&info,d) ) {
		p = validate_proc(info.pid);
		q = validate_proc(info.ppid);
		if(p->parent != q)
			change_parent(p,q);
	}
	closedir(d);

	n = num_proc - n;

	for(p = old_list; p; p=q) {
		while(p->child)
			change_parent(p->child,&proc_init);
		if(is_on_list(p,broth))
			list_del(p,broth);
		q = p->mlist.nx;
#ifdef USE_PT_PRIV
		if(p->priv) del(p->priv);
#endif
		remove_proc(p);
		n++;
	}

	return n;
}

static struct proc *ptr;
static char *lp;
static char *buf;

struct proc* tree_start(int pid,char *b)
{
	ptr = find_by_pid(pid);
	lp = buf = b;
	*lp = 0;
	return ptr;
}

struct proc* tree_next()
{
	if(ptr->child) {
		if(lp>buf && !ptr->broth.nx)
			lp[-1]=' ';
		ptr = ptr->child;
		memcpy(lp," |",3);
		lp+=2;
	} else
		for(;;) {
			if(lp==buf) return 0;
			if(ptr->broth.nx) {
				ptr = ptr->broth.nx;
				break;
			}
			lp -= 2;
			*lp=0;
			ptr=ptr->parent;
		}
	if(!ptr->broth.nx) lp[-1] = '`';
	return ptr;
}

void maintree(int pid)
{
	char buf[64];
	int n, y, x, nr = process_nr;
	struct proc *p;
	struct user *u;
	struct winsize win;
//	update_tree();
	
	if (ioctl(1,TIOCGWINSZ,&win) != -1){
		n = win.ws_row - TOP - 2;
	}
	else
		n = 18;
	p = tree_start(pid,buf);
	wmove(mainw,0,0);
	if (pid == 1){
		wattrset(mainw,A_BOLD);
		wprintw(mainw,"all processes (%d)\n\n",num_proc);
		}
	else if ((u = get_user_by_line(cursor_line))){
		wattrset(mainw,A_BOLD);
		wprintw(mainw,"%-14.14s %-9.9s %-6.6s %s\n\n",
			get_name(get_ppid(u->pid)), u->name,u->tty,
			strlen(u->host)?u->host:"local");
	}
	while(p) {
		char state = get_state(p->pid);

		if (nr++ < 0){		/* skip lines - simulate scrolling */
			p = tree_next();
			continue;
		}
		wattrset(mainw,A_NORMAL);
		wprintw(mainw,"%5d",p->pid);
		switch(state){
			case 'R':
				chkcolor(COLOR_PAIR(2));
				break;
			case 'Z':
				chkcolor(COLOR_PAIR(3));
				break;
			case 'D':
				chkcolor(COLOR_PAIR(5));
				break;
			default : 
				state = ' ';
				break;
		}
		wprintw(mainw," %c ",state);
		
		chkcolor(COLOR_PAIR(4));
		wprintw(mainw,"%s- ",buf);
		chkcolor(A_NORMAL);
		getyx(mainw, y, x);
		waddnstr(mainw, get_cmdline(p->pid), COLS - x - 1);
		wprintw(mainw,"\n");
		getyx(mainw, y, x);
		if (y >= n) break;	/* end of the screen */
		if (!(p = tree_next()) && nr < n - 5)
				dont_scroll = 1;		
		else 
			dont_scroll = 0;

	}
	wrefresh(mainw);	
}

