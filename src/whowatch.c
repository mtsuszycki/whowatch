#include <sys/types.h>
#include <sys/wait.h>
#include "whowatch.h"
#include "config.h"

#define TIMEOUT 	3

/****** new ******/
/*
 * Main window structure that holds all widgets and widgets' lists.
 */
static struct win *mwin = &win;

#define ADD_WDGTS(w) 		list_add(&w->wdgts_l, &mwin->wdgts)
#define ADD_PERIODIC(w) 	list_add(&w->periodic_l, &mwin->periodic)
#define ADD_REFRESH(w) 		list_add(&w->refresh_l, &mwin->refresh)

static void mwin_init(void)
{
	DBG("Initializing main window structure");
	INIT_LIST_HEAD(&mwin->wdgts);
	INIT_LIST_HEAD(&mwin->refresh);
	INIT_LIST_HEAD(&mwin->valchg);
	INIT_LIST_HEAD(&mwin->periodic);
}

/*
 * Refresh all widgets in reverse order, from top to bottom.
 */
static void mwin_refresh(void)
{
	struct wdgt *w;	
	struct list_head *l;
	
	list_for_each_r(l, &mwin->refresh) {
		w = list_entry(l, struct wdgt, refresh_l);
		if(w->wrefresh) w->wrefresh(w);	
		DBG("Refreshing widget '%s' [%d, %d]", w->name, w->xsize, w->ysize);
		scr_wdirty(w);
	}	
	scr_doupdate();
}


static void mwin_periodic(void)
{
	struct wdgt *w;	
	struct list_head *l;
	
	list_for_each_r(l, &mwin->periodic) {
		w = list_entry(l, struct wdgt, periodic_l);
		if(w->periodic) w->periodic(w);	
	}	
}

/*
 * Handle exit differently if screen is in ncurses mode (scr == 1)
 */
void err_exit(int scr, char *s)
{
	errx(1, "%s", s);
}

/************/


unsigned long long ticks;	/* increased every TIMEOUT seconds	*/
struct window users_list, proc_win;
struct window *current;
int size_changed; 
int full_cmd = 1;	/* if 1 then show full cmd line in tree		*/
int screen_rows;	/* screen rows returned by ioctl  		*/
int screen_cols;	/* screen cols returned by ioctl		*/
char *line_buf;		/* global buffer for line printing		*/
int buf_size;		/* allocated buffer size			*/

//enum key { ENTER=KEY_MAX + 1, ESC, CTRL_K, CTRL_I };

struct key_handler {
	enum key c;
	void (*handler)(struct window *);
};

/*
 * These are key bindings to handle cursor movement.
 */
static struct key_handler key_handlers[] = {
	{ KBD_UP, cursor_up	 	},
	{ KBD_DOWN, cursor_down 	},
	{ KBD_PAGE_DOWN, page_down	},
	{ KBD_PAGE_UP, page_up		},
	{ KBD_HOME, key_home		},
	{ KBD_END, key_end		}
};  

/*
 * Functions for key handling. They have to be called in proper order.
 * Function associated with object that is on top has to be called first.
 */
static int (*key_funct[])(int) = {
	info_box_keys,
	box_keys,
	menu_keys,
	sub_keys,
};
	
#ifdef HAVE_LIBKVM
int kvm_init();
int can_use_kvm = 0;
#endif

void prg_exit(char *s)
{
	curses_end();
	if(s) printf("%s\n", s);
	exit(0);
}

void allocate_error(){
	curses_end();
	fprintf(stderr,"Cannot allocate memory.\n");
	exit (1);
}
 
/*
 * Process these function after each TIMEOUT (default 3 seconds).
 * Order is important because some windows are on the top
 * of others.
 */
static void main_periodic(void)
{
	mwin_periodic();
	mwin_refresh();
	check_wtmp();
//update_load();		
	current->periodic();
	wnoutrefresh(main_win);
//wnoutrefresh(info_win.wd);
	sub_periodic();
	menu_refresh();
	box_refresh();
	info_refresh();
}

/*
 * Set signal_sent flag or return it if arg == -1
 */
static int signal_sent(int i)
{
	static int signal_sent;
	
	if(i == -1) return signal_sent;
	return signal_sent = i;
}

void send_signal(int sig, pid_t pid)
{
	int p;
	char buf[64];
	/*  Protect init process */
	if(pid == INIT_PID) p = -1;
	else p = kill(pid, sig);
	signal_sent(1);
	if(p == -1)
		sprintf(buf,"Can't send signal %d to process %d",
			sig, pid); 
	else sprintf(buf,"Signal %d was sent to process %d",sig, pid);
	werase(help_win.wd);
	echo_line(&help_win, buf, 0);
	wnoutrefresh(help_win.wd);
}

void m_search(void);
void help(void);

static void key_action(int key)
{
	int i, size;
	if(signal_sent(-1)) {
	    print_help();
	    signal_sent(0);
	}
	/* 
	 * First, try to process the key by object (subwindow, menu) that
	 * could be on top.
	 */
	size = sizeof key_funct/sizeof(int (*)(int));
	for(i = 0; i < size; i++)
		if(key_funct[i](key)) goto SKIP; 
	
	if(current->keys(key)) goto SKIP;
	/* cursor movement */
	size = sizeof key_handlers/sizeof(struct key_handler);
	for(i = 0; i < size; i++) 
		if(key_handlers[i].c == key) {
			key_handlers[i].handler(current);
			if(can_draw()) pad_draw();
			goto SKIP;
		}
	switch(key) {
	case 'c':
		full_cmd ^= 1;
		current->redraw();
		break;
	case '/':
		m_search();
		break;			
	case KBD_F1:
		help();
		break;
	case KBD_ESC:
	case 'q':
		curses_end();
		exit(0);
	default: return;
	}
SKIP:
	wnoutrefresh(main_win);
//wnoutrefresh(info_win.wd);
	pad_refresh();
	menu_refresh();
	box_refresh();
	info_refresh();
	doupdate();
}

static void get_scrsize(int *y, int *x)
{
	struct winsize win;
	if (ioctl(1,TIOCGWINSZ,&win) != -1){
                *y = win.ws_row;
		*x = win.ws_col;
		return;
	}
exit(0);
//	prg_exit(__FUNCTION__ ": ioctl error: cannot read screen size.");
}								

static void winch_handler()
{
	size_changed++;
}

/* 
 * Handle SIGWINCH. Order of calling various resize
 * functions is really important.
 */
static void resize(void)
{
	get_scrsize(&screen_rows, &screen_cols);
	resizeterm(screen_rows, screen_cols);
	wresize(main_win, screen_rows-3, screen_cols);
	win_init();
	mvwin(help_win.wd, screen_rows - 1, 0);	
//	wnoutrefresh(help_win.wd);
//	wnoutrefresh(info_win.wd);
	/* set the cursor position if necessary */
	if(current->cursor > current->rows)
		current->cursor = current->rows; 
	werase(main_win);
	current->redraw();
	wnoutrefresh(main_win);                                             
	print_help();
	pad_resize();
//	print_info();
//update_load();
	menu_resize();
	box_resize();
	info_resize();
	doupdate();
	size_changed = 0;
}

static void int_handler(int i)
{
	curses_end();
	exit(0);
}		

static void set_sig(void)
{
	signal(SIGINT, int_handler);
	signal(SIGWINCH, winch_handler);  
//	signal(SIGSEGV, segv_handler);
}

static void buf_alloc(int s)
{
	buf_size = s;
	line_buf = malloc(buf_size);
	if (!line_buf) err_exit(1, "Cannot allocate memory for buffer.");
}	
/****************************************************/

static void wdgt_init(struct wdgt *w)
{
	INIT_LIST_HEAD(&w->refresh_l);
	INIT_LIST_HEAD(&w->valchg_l);
	INIT_LIST_HEAD(&w->keyh_l);
	INIT_LIST_HEAD(&w->periodic_l);
}

static inline struct wdgt *wdgt_alloc(void)
{
	return calloc(1, sizeof(struct wdgt));
}

/*
 * Create new widget of given size
 */
static struct wdgt *wdgt_new(u32 x, u32 y, u32 xsize, u32 ysize, char *name)
{
	struct wdgt *w;
	if(!(w = wdgt_alloc())) err_exit(1, "Cannot allocate memory for a new widget");
	DBG("Allocated new widget %p, %d bytes, %d, %d, %d, %d", w, sizeof *w, x, y, xsize, ysize);
	wdgt_init(w);
	memcpy(w->name, name, sizeof w->name - 1);
	w->x = x;
	w->y = y;
	w->xsize = xsize;
	w->ysize = ysize;
	w->scrwd = scr_newwin(x, y, xsize, ysize);
	if(!w->scrwd) {
		DBG("Cannot create widget");
		free(w);
		return 0;
	}
	return w;
}

static char *get_load()
{
        double d[3] = { 0, 0, 0};
        static char buf[32];
	
        if(proc_getloadavg(d, 3) == -1) return "";
        snprintf(buf, sizeof buf, "load: %.2f, %.2f, %.2f", d[0], d[1], d[2]);
        return buf;
}

static void info_periodic(struct wdgt *w)
{
	char *s;
	s = get_load();
	
	DBG("Periodic for widget '%s'", w->name);
	scr_addfstr(w, proc_ucount(), 0, 0);
	scr_addstr(w, s, screen_cols-strlen(s), 0);
}

static void info_wdgt(u32 x, u32 y, u32 xsize, u32 ysize)
{
	struct wdgt *w;
	w = wdgt_new(x, y, xsize, ysize, "info");
	ADD_WDGTS(w);
	ADD_REFRESH(w);
	ADD_PERIODIC(w);
	w->periodic = info_periodic;
}

static void uplist_periodic(struct wdgt *w)
{
	scr_addstr(w, "dupa", 0, 0);
}
static void uplist_wdgt(u32 x, u32 y, u32 xsize, u32 ysize)
{
	struct wdgt *w;
	w = wdgt_new(x, y, xsize, ysize, "uplist");
	ADD_WDGTS(w);
	ADD_REFRESH(w);
	ADD_PERIODIC(w);
//	w->periodic = uplist_periodic;
}

/*
 * Create widgets
 */
static void wdgts_create(int sx, int sy)
{
	info_wdgt(0, 0, sx, 2);	
	uplist_wdgt(0, 2, sx, sy-3);
}

static void scrlib_init(void)
{
	curses_init();
}

static void main_init(void)
{
	get_scrsize(&screen_rows, &screen_cols);
	buf_alloc(screen_cols + screen_cols/2);

	mwin_init();

	/* initialize screen library */	
	scrlib_init();

	/* create all widgets */
	wdgts_create(screen_cols, screen_rows);
	
	current = &users_list;
	users_init();
        procwin_init();
	subwin_init();
	menu_init();
	set_sig();
}

static void main_start(void)
{
	mwin_periodic();
	mwin_refresh();

	print_help();
	current->redraw();
	wnoutrefresh(current->wd);
	wnoutrefresh(help_win.wd);
	doupdate();
}	

int main(int argc, char **argv)
{
	struct timeval tv;
#ifndef RETURN_TV_IN_SELECT
  	struct timeval before;
  	struct timeval after;
#endif
	fd_set rfds;
	int retval;
#ifdef HAVE_LIBKVM
	if (kvm_init()) can_use_kvm = 1;
#endif
	main_init();
	main_start();
		
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	for(;;) {				
		int key;
		FD_ZERO(&rfds);
		FD_SET(0,&rfds);
#ifdef RETURN_TV_IN_SELECT
		retval = select(1, &rfds, 0, 0, &tv);
		if(retval > 0) {
			key = getkey();
			if(key == KBD_MORE) {
				usleep(10000);
				key = getkey();
			}
			key_action(key);
		}
		if (!tv.tv_sec && !tv.tv_usec){
			ticks++;
			main_periodic();
			tv.tv_sec = TIMEOUT;
		}
#else
		gettimeofday(&before, 0);
		retval = select(1, &rfds, 0, 0, &tv);
		gettimeofday(&after, 0);
		tv.tv_sec -= (after.tv_sec - before.tv_sec);
		if(retval > 0) {
			int key = read_key();
			key_action(key);
		}
		if(tv.tv_sec <= 0) {
			ticks++;
			periodic();
			tv.tv_sec = TIMEOUT;
		}
#endif
		if(size_changed) resize();
	}
}

