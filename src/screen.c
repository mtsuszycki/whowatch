/* 
 * Screen manipulation routines. Most of the code related to ncurses
 * is here.
 */
#include <err.h>
#include <termios.h>
#include <fcntl.h>

#include "config.h"
#include "whowatch.h"

struct window help_win, info_win;
static chtype *curs_buf;
extern int screen_cols;
int old_curs_vis = 1;	/* this is the cursor mode, set to normal as default */ 
WINDOW *main_win;

char *help_line[] = 
	{
	"\001[F1]Help [F9]Menu [ENT]proc all[t]ree [i]dle/cmd [c]md [d]etails [s]ysinfo",
	"\001[ENT]users [c]md all[t]ree [d]etails [o]wner [s]ysinfo sig[l]ist ^[K]ILL",
	"\001[ENT]users [c]md [d]etails [o]owner [s]ysinfo sig[l]ist ^[K]ILL",
	};

static void alloc_color(void)
{
	init_pair(1,COLOR_CYAN,COLOR_BLACK);
        init_pair(2,COLOR_GREEN,COLOR_BLACK);
        init_pair(3,COLOR_WHITE,COLOR_BLACK);
	init_pair(4,COLOR_MAGENTA,COLOR_BLACK);
	init_pair(5,COLOR_RED,COLOR_BLACK);
	init_pair(6,COLOR_YELLOW,COLOR_BLACK);
	init_pair(7,COLOR_RED, COLOR_CYAN);
 	init_pair(8,COLOR_BLACK, COLOR_CYAN);
 	init_pair(9,COLOR_BLACK, COLOR_WHITE);
}

#define RESERVED_LINES		3	/* reserved space for help info */
					/* and general info		*/


/* NEW *************************************************************/
void scr_doupdate(void)
{
	doupdate();
}

/*
 * Create new window and set 
 */
void *scr_newwin(u32 x, u32 y, u32 xsize, u32 ysize)
{
	WINDOW *w;
	if(!(w = newwin(ysize, xsize, y, x))) return 0;
	
	wattrset(w, COLOR_PAIR(3));
	wattrset(w, COLOR_PAIR(3));
	wbkgd(w, COLOR_PAIR(3)); 
	return w;
}

/*
 * Mark window to be refreshed
 */
void scr_wdirty(struct wdgt *w)
{
	wnoutrefresh(w->scrwd);
}

void scr_addstr(struct wdgt *w, char *s, u32 x, u32 y)
{
	wattrset((WINDOW *)w->scrwd, COLOR_PAIR(3));
	wmove(w->scrwd, y, x);
	waddstr(w->scrwd, s);
}

/*
 * Print string that contains formatting chatacters (for colors)
 */
int scr_addfstr(struct wdgt *w, char *s, u32 x, u32 y)
{
	char *p = s, *q = s;
	int i = 0;
	if (!p) return 1;
	wmove(w->scrwd, y, x);
	wclrtoeol(w->scrwd);
	while(*p){
	//	if (i > w->cols) break;
		if (*p < 17){
			i--;
			if(p - q != 0)
				waddnstr(w->scrwd, q, p - q);
			wattrset((WINDOW *)w->scrwd, COLOR_PAIR(*p));
			q = p + 1;
		}
		p++;
		i++;
	}
	waddnstr(w->scrwd, q, p - q);
	return 0;
}	



/**************************************************************/
/* 
 * Initialize windows parameters and allocate space
 * for a cursor buffer.
 */
void win_init(void)
{
	curs_buf = realloc(curs_buf, screen_cols * sizeof(chtype));
	if(!curs_buf) exit(0);
	
//errx(1, __FUNCTION__ ": Cannot allocate memory.");
	bzero(curs_buf, sizeof(chtype) * screen_cols);
	users_list.rows = screen_rows - RESERVED_LINES - 1;
	users_list.cols = screen_cols - 2; 	
	proc_win.rows = users_list.rows;
	info_win.cols = help_win.cols = proc_win.cols = users_list.cols;
}	

struct termios tio;

void term_raw()
{
	struct termios t;
	tcgetattr(0, &tio);
	t = tio;
	t.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(0, TCSANOW, &t);
	fcntl(0, F_SETFL, O_NONBLOCK);
}

void term_rest()
{
	tcsetattr(0, TCSANOW, &tio);
}


void curses_init()
{
	win_init();
	initscr();
	users_list.wd = newwin(users_list.rows + 1, COLS, 2 ,0);
	proc_win.wd = users_list.wd;

	help_win.wd = newwin(1, COLS, users_list.rows + RESERVED_LINES, 0);
//info_win.wd = newwin(2, COLS, 0, 0);
	if (!help_win.wd || !users_list.wd || !proc_win.wd){
		endwin();
		fprintf(stderr, "Ncurses library cannot create window\n");
		exit(0);
	}
	main_win = users_list.wd;
	wattrset(users_list.wd, A_BOLD);
        old_curs_vis = curs_set(0);                    /* disable cursor */
        start_color();
	alloc_color();
	wattrset(proc_win.wd, COLOR_PAIR(3));       
	wattrset(users_list.wd, COLOR_PAIR(3));       
	wattrset(help_win.wd, COLOR_PAIR(3));       
//wattrset(info_win.wd, COLOR_PAIR(3));
	wbkgd(users_list.wd, COLOR_PAIR(3)); 
	wbkgd(help_win.wd, COLOR_PAIR(3)); 
//wbkgd(info_win.wd, COLOR_PAIR(3)); 
	cbreak();
/*
        nodelay(stdscr,TRUE);
	keypad(stdscr, FALSE);
	meta(stdscr, FALSE);
*/
	scrollok(main_win, TRUE);
        noecho();
	term_raw();
}				

void curses_end()
{
	werase(help_win.wd);
	wrefresh(help_win.wd);
	endwin();
        curs_set(old_curs_vis);            /* enable cursor */
}

void cursor_on(struct window *w, int line)
{
	chtype c;
	int i;
	wattrset(w->wd, A_REVERSE);
	wmove(w->wd, line, 0);
/*
mvwchgat(w->wd, line, 0, w->cols + 1, A_REVERSE, -1, 0);	
wtouchln(w->wd, line, 1, 1);
*/
	for(i = 0; i <= w->cols; i++) { 
		c = mvwinch(w->wd, line, i);
		curs_buf[i] = c;
		waddch(w->wd, c & A_CHARTEXT);
	}
	curs_buf[i] = 0;
	wattrset(w->wd, A_BOLD);
}

	

void cursor_off(struct window *w, int line)
{
	int i;
/*		
mvwchgat(w->wd, line, 0, w->cols + 1, A_NORMAL, -1, 0);	
wtouchln(w->wd, line, 1, 1);
return;
*/
	wattrset(w->wd, A_NORMAL);
	wmove(w->wd, line, 0);
	for(i = 0; curs_buf[i]; i++)
		waddch(w->wd, curs_buf[i]);
	wattrset(w->wd, A_BOLD);
}

void move_cursor(struct window *w, int from, int to)
{
	cursor_off(w, from);
	cursor_on(w, to);
}

/*
 * parse string and print line with colors
 */
int echo_line(struct window *w, char *s, int line)
{
	char *p = s, *q = s;
	int i = 0;
	if (!p) return 1;
	wmove(w->wd, line, 0);
	wclrtoeol(w->wd);
	while(*p){
		if (i > w->cols) break;
		if (*p < 17){
			i--;
			if(p - q != 0)
				waddnstr(w->wd, q, p - q);
			wattrset(w->wd, COLOR_PAIR(*p));
			q = p + 1;
		}
		p++;
		i++;
	}
	waddnstr(w->wd, q, p - q);
	return 0;
}	

void print_help()
{
	int i = 0;
	if(current == &proc_win) i = 1; 
	echo_line(&help_win, help_line[i], 0);
	wnoutrefresh(help_win.wd);
}

int inline scr_line(int l, struct window *w)
{
	return (l - w->offset);
}

/*
 * Returns -1 if a given data line number is above visible part of a
 * window, 1 if below, 0 if inside.
 */
inline int outside(int l, struct window *w)
{
	if(l < w->offset) return -1;
	return (l > w->offset + w->rows);
}

inline int below(int l, struct window *w)
{
	return (outside(l, w) == 1);
}

inline int above(int l, struct window *w)
{
	return (outside(l, w) == -1);
}

/* 
 * Returns 1 if a given screen line is a last line of data.
 */
static inline int at_last(int line, struct window *w)
{
	return (line == w->d_lines - 1 - w->offset);
}

/* 
 * Returns 1 if line is at the end of window.
 */
static inline int at_end(int line, struct window *w)
{
	return (line == w->rows);
}
											
/*
 * If virtual is not 0 then update window parameters but don't display
 */	
int print_line(struct window *w, char *s, int line, int virtual)
{
	int r = scr_line(line, w);

	/* line is below screen */
//	if(below(line, w)) return 0;

	if (!virtual) echo_line(w, s, r);
	
	/* printed line is at the cursor position */
	if (r == w->cursor && !virtual)
		cursor_on(w, w->cursor);
	return 1;
}

void delete_line(struct window *w, int line)
{
	if(below(line, w)) return;
	if(above(line, w) || at_last(0, w)) {
		w->offset--;
		return;
	}
	wmove(w->wd, scr_line(line, w), 0);
	wdeleteln(w->wd);
	if(scr_line(line, w) < w->cursor ||
		(scr_line(line, w) == w->cursor && at_last(w->cursor, w))) 
			w->cursor--;
	return;
}


void cursor_down(struct window *w)
{
	char *buf;
	if(at_last(w->cursor, w)) return;
	if(!at_end(w->cursor, w)) {
		move_cursor(w, w->cursor, w->cursor+1);
		w->cursor++;
		return;
	}
	/* cursor is at the bottom and there is a line to display */
	buf = w->giveme_line(w->offset + w->rows + 1 );
	if(!buf) return;
	wscrl(w->wd, 1);
	echo_line(w, buf, w->cursor);
	w->offset++; 
	move_cursor(w, w->cursor-1, w->cursor);
}

void cursor_up(struct window *w)
{
	char *buf;
	if(w->cursor) {
		move_cursor(w, w->cursor, w->cursor-1);
		w->cursor--;
		return;
	}
	if(!w->offset) return;
	buf = w->giveme_line(w->offset - 1);
	if (!buf) return;
	wscrl(w->wd, -1);
	w->offset--;
	echo_line(w, buf, w->cursor);
	move_cursor(w, w->cursor+1, w->cursor);
}

void page_down(struct window *w)
{
	int z;
	int i = w->d_lines - 1 - w->offset - w->rows;
	
	if(at_last(w->cursor, w)) return;
	if(i <= 0) { 
		move_cursor(w, w->cursor, w->d_lines - w->offset - 1);
		w->cursor = w->d_lines - w->offset - 1;
		return;
	}
	if(i >= w->rows) z = w->rows;
	else z = i;
	w->offset += z;
	w->redraw();
}

void page_up(struct window *w)
{
	int z;
	int i = w->offset;
	
	/* no lines above visible screen */
	if(!i && !w->cursor) return;
	if(!i) {
		move_cursor(w, w->cursor, w->offset);
		w->cursor = w->offset;
		return;
	}
	if(i >= w->rows) z = w->rows;
	else z = i;
	w->offset -= z;
	w->redraw();
}

void key_home(struct window *w)
{
	int i = w->offset;
	
	if(!i && !w->cursor) return; 
	if(!i) {
		move_cursor(w, w->cursor, w->offset);
		w->cursor = w->offset;
		return;
	}
	w->offset = w->cursor = 0;
	w->redraw();
	return;	
}

void key_end(struct window *w)
{
	int i = w->d_lines - w->offset - 1;
	/* cursor is already at the end */
	if(at_last(w->cursor, w)) return;
	if(i <= w->rows ) { 
		move_cursor(w, w->cursor, i);
		w->cursor = i;
		return;
	}
	w->offset += i - w->rows;
	w->cursor = w->d_lines - w->offset - 1;
	w->redraw();
}

/* 
 * Move cursor to specified data line.
 * Perform scrolling if necessary. Used by search functions.
 */
void to_line(int line, struct window *w)
{
	int i = scr_line(line, w);
	if(!outside(line, w)) {
		move_cursor(w, w->cursor, i);
		w->cursor = i;
		return;
	}
	if(w->d_lines - line < w->rows - 1) i = w->d_lines - w->rows - 1;
	w->offset = i;
	w->cursor = scr_line(line, w);
	w->redraw();
}
