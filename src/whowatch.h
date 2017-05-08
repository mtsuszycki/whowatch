#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <utmp.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <ctype.h>
#include <assert.h>
#include <regex.h>
#include "list.h"
#include "kbd.h"
#include "var.h"

#define WIN_HIDDEN(w)	(!w->wrefresh)

#define CURSOR_COLOR	A_REVERSE
#define NORMAL_COLOR	A_NORMAL
#define CMD_COLUMN	52

/* wdgt flags*/
#define WDGT_NO_YRESIZE	1	/* prevents from Y-resizing 	*/
#define WDGT_CRSR_SND	2	/* propagate crsr changes	*/
#define WDGT_NEED_REDRAW 4
#define WNEED_REDRAW(w)	(w->flags |= WDGT_NEED_REDRAW)

#define KEY_SKIPPED	0
#define KEY_HANDLED	1
#define KEY_FINAL	2

#define MWANT_DSOURCE	0
#define MWANT_CRSR_VAL	1
#define MCUR_CRSR	2
#define MCUR_HLP	3
#define MWANT_UPID	4
#define MALL_CRSR_REG	5  /* wdgt wants to receive cursor changes */
#define MALL_CRSR_UNREG 6  /* doesn't want to receive it anymore   */
#define MSND_ESOURCE	7
#define MSND_INFO	8
#define MSND_SEARCH	9
#define MSND_SEARCH_END	10

#define CWND(w)		((WINDOW*)(w->wd))	/* ncurses window descriptor	*/

#define INIT_PID		1
#define real_line_nr(x,y)	((x) - (y)->offset)

/* wdgts colors - correspond to curses COLOR_PAIR(n) 	*/
#define CLR_WHITE_BLACK		3
#define CLR_BLACK_CYAN		8
#define CLR_CYAN_BLACK		1
#define CLR_RED_CYAN		7
#define CLR_BLACK_WHITE		9



enum key { ENTER=0x100, UP, DOWN, LEFT, RIGHT, DELETE, ESC, CTRL_K,
                CTRL_I, PG_DOWN, PG_UP, HOME, END, BACKSPACE, TAB };

extern unsigned long long ticks;
extern int full_cmd;

struct wdgt;
struct win {
	struct list_head wdgts;	/* list of all widgets                  */
	struct list_head msg;	/* message bus for wdgts communication  */
	u32	sy, sx;		/* physical screen coords (size+1)      */
	u8	gbuf[256];	/* global buf to be used by any wdgt 	*/
	u32	gbsize;
	void	*(*cval)(void);	/* returns current cursor value	(pid/name) */
	u8	need_redraw;	/* immediate redraw needed for all wdgts*/
//	struct wdgt *mwdgt;
};

struct wdgt {
	struct 	list_head wdgts_l;
	struct 	list_head msg_l;
	struct 	win *mwin;
	u8 	flags;				/* NO_YRESIZE, and for future use	*/
	char	name[8];			/* debugging identification		*/
	void 	*prv;				/* for use by widget itself		*/
	void 	(*wrefresh)(struct wdgt *); 	/* refreshes widget			*/
	void 	(*redraw)(struct wdgt *);	/* print output into the widget		*/
	int	(*keyh)(struct wdgt *, int);	/* widget's key handler			*/
	void 	(*periodic)(struct wdgt *);	/* to execute every step seconds	*/
						/* message handler			*/
	void 	*(*msgh)(struct wdgt *, int, struct wdgt *, void *);
	u32	x, y, xsize, ysize, vx, vy;	/* screen coords and offsets		*/
	u32	pysize, pxsize;			/* pad real dimension			*/
	void 	*wd;				/* screen window/pad descriptor		*/
	int 	crsr;				/* current cursor position		*/
	u8 	color;				/* fg and bg colors, CLR_* macros	*/
	int 	nlines;				/* total nr of lines in output		*/
	void 	*decor;				/* frame around widget, if any		*/
//	u32	*crsrcache;			/* holds pid/uid indexed by crsr line	*/
};

struct process
{
	struct process **prev;
	struct process *next;
	struct list_head plist_l;
	int 	line;
	int 	uid;
	struct 	proc_t *proc;
};

void err_exit(int , char *);
void mwin_redraw_on(struct wdgt *);
void mwin_refresh_on(struct wdgt *);
void mwin_periodic_on(struct wdgt *);
void mwin_msg_on(struct wdgt *);
void mwin_need_redraw(struct wdgt *);
void *wmsg_send(struct wdgt *, int, void *);
/* screen.c */
void scr_doupdate(void);
void *scr_newwin(u32 ,u32 , u32 , u32);
void scr_wdirty(struct wdgt *);
int  scr_addfstr(struct wdgt *, char *, u32 , u32 );
void scr_addstr(struct wdgt *, char *, u32);
void scr_maddstr(struct wdgt *, char *, u32 , u32, u32);
void scr_werase(struct wdgt *);
int scr_keyh(struct wdgt *, int);
void scr_ldeleted(struct wdgt *, int);
void scr_linserted(struct wdgt *, int);
void scr_output_start(struct wdgt *);
void scr_output_end(struct wdgt *);
void scr_wresize(struct wdgt *, u32 , u32 );
void scr_attr_set(struct wdgt *, int );
void scr_clr_set(struct wdgt *, int );
void scr_wrefresh(struct wdgt *w);
void scr_decor_resize(struct wdgt *w);
void scr_delline(struct wdgt *, u32 );
void scr_box(struct wdgt *w, char *s, u8 c);
void scr_crsr_jmp(struct wdgt *w, int l);

/* reg functions */
void ulist_reg(struct wdgt *);
void ptree_reg(struct wdgt *);
void exti_reg(struct wdgt *);
void users_init(void);
void hlp_reg(struct wdgt *);
void info_reg(struct wdgt *w);
void input_reg(struct wdgt *w);

/* ulist.c */
char *proc_ucount(void);

/* whowatch.c */
void allocate_error();
void prg_exit(char *);
void send_signal(int, pid_t);

/* screen.c */
void curses_init();
void curses_end();

/* proctree.c */
int update_tree();

/* plist.c */
void delete_tree_line(void *line);

/* procinfo.c */
#ifdef HAVE_PROCESS_SYSCTL
int get_login_pid(char *tty);
#endif
char *get_cmdline(int);
int get_ppid(int);
char *get_name(int);
char *get_w(int pid);
char *count_idle(char *tty);
int proc_pid_uid(u32);
#ifndef HAVE_GETLOADAVG
int proc_getloadavg(double [], int);
#endif

/* owner.c */
char *get_owner_name(int u);

/* block.c */
void *get_empty(int, struct list_head *);
int free_entry(void *, int, struct list_head *);
void dolog(const char *, ...);

/* proc_plugin.c */
void eproc(void *);
void esys(void *);

/* user_plugin.c */
void euser(void *);

/* wdgts.c */
int reg_match(const char *);

/* menu.c */
void set_search(char *);

/* kbd.c */
int getkey();

/* screen.c */
void term_raw();

