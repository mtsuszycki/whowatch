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

extern int allocated;		/* number of processes */
extern FILE *debug_file;
extern int show_owner;		/* if 0 don't show process owner (in tree) */
extern int screen_rows, screen_cols;
extern char *line_buf;
extern int buf_size;

struct window
{
	unsigned int rows;
	unsigned int cols;
	int first_line;
	int last_line;
	int cursor_line;
	int has_cursor;		/* not yet implemented */
	WINDOW *descriptor;
	char *(*giveme_line) (int line);
};

extern struct window users_list;
extern struct window proc_win;
extern struct window help_win;
extern struct window info_win;

struct user
{
        char *name;
        char *tty;
        int prot;
        int pid;
	char parent[16];
        struct user **prev;
        struct user *next;
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
	struct proc *proc;
};

extern int tree_pid;
extern int how_many, ssh_users, telnet_users, local_users;

void allocate_error();

/* process.c */
void tree_periodic();
void clear_list();
char *proc_give_line(int line);
void dump_list();
pid_t pid_from_tree(int line);
void maintree(int pid);
void tree_title(struct user *p);
void clear_tree_title();

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

/* proctree.c */
int update_tree();


void get_rows_cols(int *, int *);

/* proc.c */
char *get_cmdline(int);
int get_ppid(int);
char *get_name(int);
char *get_w(int pid);
void delete_tree_line(void *line);
void get_state(struct process *p);
char *count_idle(char *tty);

/* owner.c */
char *get_owner_name(int u);


