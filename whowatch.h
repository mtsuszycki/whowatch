#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <utmp.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <assert.h>

#define CURSOR_COLOR	A_REVERSE
#define NORMAL_COLOR	A_NORMAL
#define CMD_COLUMN	52

#define DETAILS_WIN_COLS       64
#define DETAILS_WIN_ROWS      128

#define real_line_nr(x,y)	((x) - (y)->first_line)

#define for_each(e,l) 	for((e)=(l).next; (e)!=(void *) &(l); (e)=(e)->next)
#define addto_list(e,l)	{(e)->prev=(l).prev;		\
			 (e)->prev->next=(e);		\
			 (e)->next=(typeof(e))&l;	\
			 (l).prev=(e);}
#define delfrom_list(e,l) {(e)->prev->next=(e)->next;	\
			   (e)->next->prev=(e)->prev; }

#define dellist(e,l)	{ void *p;			\
			  for((e)=(l).next; (e)!=(void *) &(l); (e)=p){	\
			  	p=(e)->next;		\
				delfrom_list((e),(l));	\
				free((e));		\
			  }				\
			}		

			
extern int allocated;		/* number of processes */
extern FILE *debug_file;
extern int show_owner;		/* if 0 don't show process owner (in tree) */
extern int screen_rows, screen_cols;
extern char *line_buf;
extern int buf_size;

/*
 * Data associated with window are line numbered. If scrolling
 * inside window occurs then number of first displayed line changes
 * (first_line). 
 */
struct window
{
	unsigned int rows;
	unsigned int cols;
	int first_line;		/* nr of first displayed line	 	*/
	int last_line;
	int s_col;		/* display starts from this col		*/
	int d_lines;		/* current total number of data lines 	*/
	int cursor_line;	/* where is cursor		 	*/
	int has_cursor;		/* not yet implemented    		*/
	WINDOW *wd;		/* window descriptor			*/
	char *(*giveme_line) (int line);
};

extern struct window users_list;
extern struct window proc_win;
extern struct window help_win;
extern struct window info_win;

struct list_head
{
	void *next;
	void *prev;
};

struct user_t
{
	struct user_t *next;
	struct user_t *prev;
        char name[UT_NAMESIZE + 1];	/* login name 		*/
        char tty[UT_LINESIZE + 1];	/* tty			*/
        int prot;			
        int pid;			/* pid of login shell 	*/
	char parent[16];
        char host[UT_HOSTSIZE + 1];
        int line;
};

struct process
{
        struct process **prev;
        struct process *next;
        int line;
	char state;
	int uid;
	struct proc_t *proc;
};

extern int tree_pid;
extern int how_many, ssh_users, telnet_users, local_users;

void allocate_error();
void users_list_refresh();

/* process.c */
void tree_periodic();
void clear_list();
char *proc_give_line(int line);
void dump_list();
pid_t pid_from_tree(int line);
void maintree(int pid);
void tree_title(struct user_t *p);
void clear_tree_title();
void refresh_tree();
char get_state_color(char);

/* screen.c */								
int print_line(struct window *w, char *s, int line, int virtual);
int echo_line(struct window *w, char *s, int line);
void cursor_on(struct window *w, int line);
void cursor_off(struct window *w, int line);
void curses_init();
void curses_end();
void print_help(int state);
void print_info();
void cursor_down(struct window *w);
void cursor_up(struct window *w);
void delete_line(struct window *w, int line);
void virtual_delete_line(struct window *w, int line);

char *list_giveline(int line);
void page_down(struct window *, void (*)());
void page_up(struct window *, void (*)());
void key_home(struct window *, void (*)());
void key_end(struct window *, void (*)());
void update_load(void);
void updatescr();

/* proctree.c */
int update_tree();
void get_rows_cols(int *, int *);

/* procinfo.c */
#ifdef HAVE_PROCESS_SYSCTL
int get_login_pid(char *tty);
#endif
char *get_cmdline(int);
int get_ppid(int);
char *get_name(int);
char *get_w(int pid);
void delete_tree_line(void *line);
void get_state(struct process *p);
char *count_idle(char *tty);
#ifndef HAVE_GETLOADAVG
int getloadavg(double [], int);
#endif
void proc_details(struct window *, int );
void sys_info(struct window *, int );
void get_boot_time(void);

/* owner.c */
char *get_owner_name(int u);

