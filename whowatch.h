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
#include <curses.h>

#define chkcolor(c) if(color) wattrset(mainw,c)

/*
 * COLS and LINES are filled by ncurses (initscr()) with the size
 * of the screen
 */
#define TOP 2			/* number of lines reserved for header 	   */
#define BOTTOM LINES-1		/* number of empty line below main window  */
#define MAINW_LINES BOTTOM-TOP  /* number of the main window lines    	   */
#define CMD_COLUMN 52		/* x position for printing procs or idle   */

extern WINDOW *mainw;
extern int how_many, telnet_users, ssh_users, local_users, toggle;
extern int color;
extern int cursor_line;		/* current cursor's position */

extern int fline;	/* shows which line (from struct user) is on the
			   top of the main window (needed for scrolling) */
extern int lline;

extern int proctree;
extern int process_nr;	/* how many lines to skip in a processes tree */
extern int dont_scroll; /* don't do scrolling up processes tree any longer */

extern char clear_buf[32];
/* number of line which will be assigned to the next user */
extern int lines;	

struct user
{
        char *name;
        char *tty;
        int prot;
        int pid;
        struct user **prev;
        struct user *next;
	char host[UT_HOSTSIZE + 1];
        int line;
};
							
extern struct user *begin;
struct utmp *get_utmp_user(char *tty);
int read_key();						

/* proc.c */
int get_ppid(int pid);
char *get_name(int pid);
char *get_w(int pid);
char *count_idle(char *tty);
char *get_cmdline(int pid);
char get_state(int pid);

/* screen.c */
void endprg();
void update_info();
void print_user();
void delete_user(struct user *p);
void curses_init();
void show_cmd_or_idle(int what);
void screen_up();
void screen_down();
void redraw();
void cursor_up();
void cursor_down();
int in_scr(int line);
int pid_from_cursor();
struct user *get_user_by_line(int line);

/* treeps.cc */
void maintree();
int update_tree();
