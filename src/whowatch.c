#include <sys/types.h>
#include <sys/wait.h>
#include "whowatch.h"
#include "config.h"

#define TIMEOUT		 	6000
/* Main window that holds all widgets and manages them 	*/
static struct win win;
static struct win *mwin = &win;

#define MWADD_TO_LIST(w, N)	list_add(&w->N ## _l, &mwin->N)
#define mwin_do_list(N, F...) 					\
static void mwin_ ## N  (void)					\
{								\
	struct wdgt *w;						\
	struct list_head *l;					\
	list_for_each_r(l, &mwin->wdgts) {			\
		w = list_entry(l, struct wdgt, wdgts_l);	\
		if(!w->N) continue;				\
		DBG("Calling " # N " for '%s'", w->name);	\
		w->N(w);					\
	}							\
	F;							\
}

unsigned long long ticks;	/* increased every TIMEOUT seconds	*/
static int size_changed; 
int full_cmd = 1;	/* if 1 then show full cmd line in tree		*/
	
#ifdef HAVE_LIBKVM
int kvm_init();
int can_use_kvm = 0;
#endif

mwin_do_list(wrefresh, scr_doupdate());
mwin_do_list(periodic);

void mwin_redraw(int f)
{
	struct wdgt *w;	
	struct list_head *l;
	list_for_each_r(l, &mwin->wdgts) {
		w = list_entry(l, struct wdgt, wdgts_l);
		if(!w->redraw) continue;
		if(!f && !(w->flags & WDGT_NEED_REDRAW)) continue;
		DBG("REDRAWing %s, flags %d", w->name, w->flags);
		w->flags &= ~WDGT_NEED_REDRAW;
		w->redraw(w);
	}
}

char *_msg[] = { "want_dsource", "want_crsr_val", "cur_crsr", "cur_hlp", "want_upid", 
	"crsr_reg", "crsr_unreg", "snd_esource", "snd_info", "snd_search", "snd_search_end"};

void *wmsg_send(struct wdgt *sndr, int type, void *data)
{
	struct wdgt *w;	
	struct list_head *l;
	void *ret = 0;	
	DBG("Sending message %s [%d] from %s", _msg[type], type, sndr->name);
	list_for_each_r(l, &mwin->msg) {
		w = list_entry(l, struct wdgt, msg_l);
		/* don't send to self */
		if(w == sndr || !w->msgh) continue;
		DBG("'%s' %s [%d] -> '%s'",sndr->name, _msg[type], type, w->name);
		ret = w->msgh(w, type, sndr, data);
		if(ret) {
			DBG("'%s' responded for %s [%d]", w->name, _msg[type], type);
			return ret;
		}	
	}
	DBG("End of sending %d", type);
	return ret;
}

static int mwin_keyh(int key)
{
	struct wdgt *w;	
	struct list_head *l;
	int ret = KEY_SKIPPED; 
	list_for_each(l, &mwin->wdgts) {
		w = list_entry(l, struct wdgt, wdgts_l);
		if(!w->keyh) continue;
		DBG("Calling keyh for '%s', key '%c'", w->name, key);
		ret = w->keyh(w, key);	
		if(ret == KEY_FINAL) return ret;
	}
	return ret;
}

void mwin_msg_on(struct wdgt *w)
{
	MWADD_TO_LIST(w, msg);
}

static void mwin_init(void)
{
	DBG("Initializing main window structure");
	INIT_LIST_HEAD(&mwin->wdgts);
	INIT_LIST_HEAD(&mwin->msg);
	mwin->gbsize = sizeof(mwin->gbuf);
}

/*
 * Handle exit differently if screen is in ncurses mode (scr == 1)
 */
void err_exit(int scr, char *s)
{
	errx(1, "%s", s);
}

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
 * Run it after each TIMEOUT (default 3 seconds).
 */
static void main_periodic(void)
{
	mwin_periodic();
	mwin_redraw(1);
	mwin_wrefresh();
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
	/*
	werase(help_win.wd);
	echo_line(&help_win, buf, 0);
	wnoutrefresh(help_win.wd);
	*/
}

static void key_action(int key)
{
	int ret = mwin_keyh(key);
	if(ret != KEY_FINAL && (key == 'q' || key == KBD_ESC)) {
		curses_end();
		exit(0);
	}
	/* if need_redraw is set then redraw only wdgts with WDGT_NEED_REDRAW */
	mwin_redraw(mwin->need_redraw);
	mwin->need_redraw = 0;
//	if(ret == KEY_SKIPPED) return;
	mwin_wrefresh();
}

static void get_scrsize(u32 *y, u32 *x)
{
	struct winsize win;
	if(ioctl(1,TIOCGWINSZ,&win) == -1)
		err_exit(1, "ioctl error: cannot read screen size");

	/* 
	 * We always use coordinates that starts from zero
	 * and physical screen size is a _number_ of rows/cols
	 */
	*y = win.ws_row-1;
	*x = win.ws_col-1;
}								

static void resize(void)
{
	struct wdgt *w;	
	struct list_head *l;
	u32 prev_sy = mwin->sy;
	u32 prev_sx = mwin->sx;
	
	get_scrsize(&mwin->sy, &mwin->sx);
	
	list_for_each_r(l, &mwin->wdgts) {
		w = list_entry(l, struct wdgt, wdgts_l);
		 /* 
		  * we exapand pad's width but not height 
		  * coz height depends on data source (output)
		  * */
		if(w->pxsize < mwin->sx) {
			w->pxsize = mwin->sx+1;
			scr_wresize(w, w->pysize, w->pxsize);
		}	
		w->xsize += mwin->sx - prev_sx;
		if(w->flags & WDGT_NO_YRESIZE) continue;
		w->ysize += mwin->sy - prev_sy;
		if(w->decor) scr_decor_resize(w);
werase(w->wd);
	}
	resizeterm(mwin->sy+1, mwin->sx+1);
	//mwin_periodic();
	mwin_redraw(1);
	mwin_wrefresh();
	size_changed = 0;
}

static void winch_handler()
{
	size_changed++;
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

static void wdgt_init(struct wdgt *w)
{
	INIT_LIST_HEAD(&w->wdgts_l);
	INIT_LIST_HEAD(&w->msg_l);
	MWADD_TO_LIST(w, wdgts);
}

static inline struct wdgt *wdgt_alloc(void)
{
	return calloc(1, sizeof(struct wdgt));
}

/*
 * Create window/pad structures specific to screen library used.
 */
static int wd_create(struct wdgt *w, u32 ysize, u32 xsize, u32 pysize, u32 pxsize, u8 c)
{
//w->wd = scr_newwin(x, y, xsize, ysize);
	w->wd = newpad(pysize+1, pxsize+1);
	w->color = c;
	w->pysize = pysize;
	w->pxsize = pxsize;
	wbkgd((WINDOW*)w->wd, COLOR_PAIR(c));
	if(!w->wd) return 0;
	return 1;
}

/*
 * Create new widget of given size
 */
static struct wdgt *wdgt_new(u32 y, u32 x, u32 ysize, u32 xsize, u32 pysize, u32 pxsize, char *name, u8 c)
{
	struct wdgt *w;
	if(!(w = wdgt_alloc())) err_exit(1, "Cannot allocate memory for a new widget");
	wdgt_init(w);
	memcpy(w->name, name, sizeof w->name - 1);
	w->x = x;
	w->y = y;
	w->xsize = xsize;
	w->ysize = ysize;
	w->vy = w->vx = 0;
	if(!pysize) pysize = ysize - y + 1;
	if(!pxsize) pxsize = xsize - x + 1;
	if(!wd_create(w, ysize, xsize, pysize, pxsize, c)) 
		err_exit(1, "Cannot create widget");
	DBG("New '%s', %d, %d, %d, %d, %d, %d", w->name, w->y, w->x, w->ysize, w->xsize, w->pysize, w->pxsize);
	/* disable cursor by default */
	w->crsr = -1;
	w->mwin = mwin;
	return w;
}

static void info_wdgt(u32 y, u32 x, u32 ysize, u32 xsize, u32 pysize, u32 pxsize, u8 c)
{
	struct wdgt *w;
	w = wdgt_new(y, x, ysize, xsize, pysize, pxsize, "info", c);
	MWADD_TO_LIST(w, msg);
	info_reg(w);
	w->flags |= WDGT_NO_YRESIZE;
}

static void hlp_wdgt(u32 y, u32 x, u32 ysize, u32 xsize, u32 pysize, u32 pxsize, u8 c)
{
	struct wdgt *w;
	w = wdgt_new(y, x, ysize, xsize, pysize, pxsize, "help", c);
	MWADD_TO_LIST(w, msg);
	w->flags |= WDGT_NO_YRESIZE;
	hlp_reg(w);
}


static void ulist_wdgt(u32 y, u32 x, u32 ysize, u32 xsize, u32 pysize, u32 pxsize, u8 c)
{
	struct wdgt *w;
	w = wdgt_new(y, x, ysize, xsize, pysize, pxsize, "ulist", c);
	/* enable cursor for this widget */
	w->crsr = 0;
	ulist_reg(w);
}

static void ptree_wdgt(u32 y, u32 x, u32 ysize, u32 xsize, u32 pysize, u32 pxsize, u8 c)
{
	struct wdgt *w;
	w = wdgt_new(y, x, ysize, xsize, pysize, pxsize, "ptree", c);
	/* enable cursor for this widget */
	w->crsr = 0;
	ptree_reg(w);
}

/* extended information subwindow */
static void exti_wdgt(u32 y, u32 x, u32 ysize, u32 xsize, u32 pysize, u32 pxsize, u8 c)
{
	struct wdgt *w;
	w = wdgt_new(y, x, ysize, xsize, pysize, pxsize, "exti", c);
	exti_reg(w);
}	
static void input_wdgt(u32 y, u32 x, u32 ysize, u32 xsize, u32 pysize, u32 pxsize, u8 c)
{	
	struct wdgt *w;
	w = wdgt_new(y, x, ysize, xsize, pysize, pxsize, "input", c);
	MWADD_TO_LIST(w, msg);
	w->flags |= WDGT_NO_YRESIZE;
	input_reg(w);
}

/*
 * Order of creating widgets is important, since
 * they overlap. On widget creation it is added
 * to mwin refresh/redraw list and calling to w->wrefresh has
 * to be done in proper order. Therefore widget doesn't remove itself 
 * from mwin wrefresh/redraw list to hide itelf but sets his own 
 * function pointers (wrefres/redraw) to zero.
 * Create widgets here from bottom to top, in terms of on screen
 * visibility. 
 * [sy,sx] - physical screen size, numbering starts from zero. 
 */
static void wdgts_create(int sy, int sx)
{
	/*
	 *  upper left corner: [y,x], lower right [y,x], 
	 * real size of the pad  [y,x] (numbering always starts from 0) 
	 */
	info_wdgt(0, 0, 1, sx, 1, sx, CLR_WHITE_BLACK);//CLR_CYAN_BLACK);	
	hlp_wdgt(sy, 0, sy, sx, sy, sx, CLR_CYAN_BLACK);	
	ulist_wdgt(3, 0, sy-1, sx, 256, sx, CLR_WHITE_BLACK); 
	ptree_wdgt(3, 0, sy-1, sx, 256, sx, CLR_WHITE_BLACK);
	exti_wdgt(sy/4, sx/5, sy-sy/4, sx-sx/5, 256, 128, CLR_BLACK_CYAN);
	input_wdgt(sy, 0, sy, sx, 0, 0, CLR_BLACK_WHITE);
}

static void scrlib_init(void)
{
	curses_init();
}

static void main_init(void)
{
	get_scrsize(&mwin->sy, &mwin->sx);
	mwin_init();

	/* initialize screen library */	
	scrlib_init();

	/* create all widgets */
	wdgts_create(mwin->sy, mwin->sx);
	users_init();
	set_sig();
}

static void main_start(void)
{
	main_periodic();
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

