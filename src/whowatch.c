#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "whowatch.h"
#include "config.h"

#ifdef DEBUG
FILE *debug_file;
#endif

#define TIMEOUT 	3

unsigned long long ticks;	/* increased every TIMEOUT seconds	*/
struct window users_list, proc_win;
struct window *current;
int size_changed; 
int full_cmd = 1;	/* if 1 then show full cmd line in tree		*/
int signal_sent;
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
 
void update_load();

/*
 * Process these function after each TIMEOUT (default 3 seconds).
 * Order is important because some windows are on the top
 * of others.
 */
static void periodic(void)
{
	check_wtmp();
	update_load();		
	current->periodic();
	wnoutrefresh(main_win);
	wnoutrefresh(info_win.wd);
	sub_periodic();
	menu_refresh();
	box_refresh();
	info_refresh();
	doupdate();
}

void send_signal(int sig, pid_t pid)
{
	int p;
	char buf[64];
	/*  Protect init process */
	if(pid == INIT_PID) p = -1;
	else p = kill(pid, sig);
	signal_sent = 1;
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
	if(signal_sent) {
	    print_help();
	    signal_sent = 0;
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
	dolog("%s: doing refresh\n", __FUNCTION__);
	wnoutrefresh(main_win);
	wnoutrefresh(info_win.wd);
	pad_refresh();
	menu_refresh();
	box_refresh();
	info_refresh();
	doupdate();
}

static void get_rows_cols(int *y, int *x)
{
	struct winsize win;
	if (ioctl(1,TIOCGWINSZ,&win) != -1){
                *y = win.ws_row;
		*x = win.ws_col;
		return;
	}
	prg_exit("get_row_cols(): ioctl error: cannot read screen size.");
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
	get_rows_cols(&screen_rows, &screen_cols);
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
	print_info();
	update_load();
	menu_resize();
	box_resize();
	info_resize();
	doupdate();
	size_changed = 0;
}

static void int_handler()
{
	curses_end();
	exit(0);
}		

int main (int argc, char **argv)
{
	struct timeval tv;

#ifdef HAVE_LIBKVM
	if (kvm_init()) can_use_kvm = 1;
#endif
	
#ifdef DEBUG
	if (!(debug_file = fopen("debug", "w"))) {
		printf("file debug open error\n");
		exit(0);
	}
#endif
	get_boot_time();
	get_rows_cols(&screen_rows, &screen_cols);
	buf_size = screen_cols + screen_cols/2;
	line_buf = malloc(buf_size);
	if (!line_buf)
		errx(1, "Cannot allocate memory for buffer.");

	curses_init();
	current = &users_list;
	users_init();
	procwin_init();
	subwin_init();
	menu_init();
	signal(SIGINT, int_handler);
	signal(SIGWINCH, winch_handler);  
	//	signal(SIGSEGV, segv_handler);

	print_help();
	update_load();
	current->redraw();
	wnoutrefresh(main_win);
	wnoutrefresh(info_win.wd);
	wnoutrefresh(help_win.wd);
	doupdate();
	
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	
	for(;;) {				/* main loop */
#ifndef RETURN_TV_IN_SELECT
		struct timeval before;
		struct timeval after;
#endif
		fd_set rfds;
		int retval;
		
		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO,&rfds);
#ifdef RETURN_TV_IN_SELECT
		retval = select(1, &rfds, 0, 0, &tv);
		if(retval > 0) {
			int key = read_key();
			key_action(key);
		}
		if (!tv.tv_sec && !tv.tv_usec){
			ticks++;
			periodic();
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
		if (size_changed) resize();
	}
}
