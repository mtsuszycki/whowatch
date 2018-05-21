/*

SPDX-License-Identifier: GPL-2.0+

Copyright 1999-2001, 2006 Michal Suszycki <mt_suszycki@yahoo.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

/*
 * Screen manipulation routines. Most of the code related to ncurses
 * is here.
 */
#include <err.h>
#include <termios.h>
#include <fcntl.h>
#include "config.h"
#include "whowatch.h"


static chtype curs_buf[512];
//WINDOW *main_win;

char *help_line[] = {
	"\001[F1]Help [F9]Menu [ENT]proc all[t]ree [i]dle/cmd [c]md [d]etails [s]ysinfo",
	"\001[ENT]users [c]md all[t]ree [d]etails [o]wner [s]ysinfo sig[l]ist ^[T]ERM",
	"\001[ENT]users [c]md [d]etails [o]owner [s]ysinfo sig[l]ist ^[T]ERM" };

static void scr_color_init(void)
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

void scr_wresize(struct wdgt *w, u32 y, u32 x)
{
	wresize(CWND(w), y, x);
}

static void scr_box_text(struct wdgt *w, char *s, u8 c)
{
	int l, pos;
	if(!s) return;
	l = strlen(s);
//	pos = (w->xsize-w->x-l)/2; 	/*    center 	*/
	pos = w->xsize-w->x-l;		/*        right	*/
//	pos = w->x+1;			/* left         */

	mvwaddnstr((WINDOW*)w->decor, w->ysize-w->y+2, pos , s, l);
}

static inline void sadd_box(struct wdgt *w)
{
	box(w->decor, ACS_VLINE, ACS_HLINE);
}

/*
 * Add box to a widget (if not exists yet )
 * and eventually place some text in it.
 */
void scr_box(struct wdgt *w, char *s, u8 c)
{
	if(w->decor) {
		scr_box_text(w, s, c);
		return;
	}
	/* box not yet created, do it now */
	DBG("Creating box for '%s'", w->name);
	w->decor = newpad(w->ysize - w->y + 3, w->xsize - w->x + 3);
	if(!w->decor) return;
	sadd_box(w);
	wbkgd(w->decor, COLOR_PAIR(c));
	scr_box_text(w, s, c);
}

void scr_decor_resize(struct wdgt *w)
{
	wresize(w->decor, w->ysize - w->y + 3, w->xsize - w->x + 3);
	sadd_box(w);
}

void scr_doupdate(void)
{
	DBG("--------- UPDATING SCREEN ----------");
	doupdate();
}

/*
 * Create new window
 */
void *scr_newwin(u32 x, u32 y, u32 xsize, u32 ysize)
{
	WINDOW *w;
	if(!(w = newwin(ysize, xsize, y, x))) return 0;

	wbkgd(w, COLOR_PAIR(8));
	return w;
}

static void crsr_off(struct wdgt *w, int line)
{
	int i;
DBG("DISABLING cursor on line %d", line);
	wattrset(CWND(w), A_NORMAL);
//	mvwchgat(CWND(w), line, 0, w->xsize+1, A_NORMAL, 0, 0);
	for(i = 0; curs_buf[i]; i++)
		waddch(CWND(w), curs_buf[i]);
}

static void crsr_on(struct wdgt *w, int line)
{
	DBG("ENABLING in %s on line %d", w->name, line);
	mvwinchnstr(CWND(w), line, 0, curs_buf, w->xsize+1);
	mvwchgat(CWND(w), line, 0, w->xsize+1, A_REVERSE, 0, 0);
	wattrset(CWND(w), A_NORMAL);
}

static void crsr_move(struct wdgt *w, int from, int to)
{
	crsr_off(w, from);
	crsr_on(w, to);
	w->crsr = to;
}

/*
 * One and only function that prints output to screen
 * at least for widgets that has cursor.
 * nlines is a coordinate, so if nlines==0 then
 * there is one line on the screen
 */
static void scr_lecho(struct wdgt *w, char *s, int n)
{
	waddnstr(CWND(w), s, n);
	w->nlines++;
//	if(w->crsr == w->nlines) crsr_on(w, w->crsr);
}

/* Start numbering lines from zero */
void scr_output_start(struct wdgt *w)
{
	w->nlines = -1;
}

/*
 * Adopt visible window after output has finished.
 */
void scr_output_end(struct wdgt *w)
{
	/* there are less lines than before and all
	 * of them are unvisible now
	 */
	if(w->vy >= w->nlines) {
		if(w->nlines > w->ysize) w->vy = w->nlines - w->ysize;
		else w->vy = 0;
	}
	/* cursor is below last line of the output */
	if(w->crsr > w->nlines) w->crsr = w->nlines;
	if(w->crsr != -1) crsr_on(w, w->crsr);
}

void scr_attr_set(struct wdgt *w, int n)
{
	wattrset(CWND(w), n);
}

void scr_clr_set(struct wdgt *w, int n)
{
	wattrset(CWND(w), COLOR_PAIR(n));
}

void scr_crsr_jmp(struct wdgt *w, int l)
{
//	int jmp = l - w->crsr;
	int size = w->ysize - w->y - 1;
	crsr_move(w, w->crsr, l);
//	DBG("size %d, crsr %d", size, w->crsr);
	if(l >= w->vy && l < w->vy + size) return;
	w->vy = w->crsr - size;
}

void scr_maddstr(struct wdgt *w, char *s, u32 y, u32 x, u32 n)
{
	wmove(w->wd, y, x);
	scr_lecho(w, s, n);
}

void scr_addstr(struct wdgt *w, char *s, u32 n)
{
	scr_lecho(w, s, n);
}

/*
 * If some lines are inserted above cursor, move virtual screen so
 * cursor stays at the same position.
 */
void scr_linserted(struct wdgt *w, int line)
{
	if(!w->vy && !w->crsr) return;
	if(line <= w->crsr) {
		w->vy++;
		w->crsr++;
	}
}

/*
 * Keep cursor on the same line and same position
 * as long as possible. Cursor moves up if lines
 * are deleted above it.
 */
void scr_ldeleted(struct wdgt *w, int l)
{
	if(l >= w->crsr) return;
	if(!w->vy) w->crsr--;
	else {
		w->vy--;
		w->crsr--;
	}
}
static inline void srefresh(struct wdgt *w)
{
	pnoutrefresh(CWND(w), w->vy, w->vx, w->y, w->x, w->ysize, w->xsize);
}

void scr_wrefresh(struct wdgt *w)
{
	DBG("'%s', %d %d %d %d %d %d", w->name, w->vy, w->vx, w->y, w->x,
			w->ysize, w->xsize);
	if(w->decor) pnoutrefresh(w->decor, 0, 0, w->y-1, w->x-1, w->ysize+1, w->xsize+1);
	srefresh(w);
}

/*
 * Print string that contains formatting characters (for colors)
 */
int scr_addfstr(struct wdgt *w, char *s, u32 y, u32 x)
{
	char *p = s, *q = s;
	int i = 0;
	if (!p) return 1;
	wmove(w->wd, y, x);
	wclrtoeol(w->wd);
	for(i = 0; *p && i <= w->xsize; p++, i++) {
		if(*p >= 17) continue;
		i--;
		if(p - q != 0) waddnstr(CWND(w), q, p - q);
		wattrset(CWND(w), COLOR_PAIR(*p));
		q = p + 1;
	}
	scr_lecho(w, q, p-q);
	return 0;
}

void scr_werase(struct wdgt *w)
{
	werase(w->wd);
}

void scr_delline(struct wdgt *w, u32 l)
{
	wmove(w->wd, l, 0);
	wdeleteln(w->wd);
}

static inline int s_nleft(struct wdgt *w)
{
	int n = w->ysize - w->y;
	return w->nlines - (w->vy + n);
}

static int kbd_page_up(struct wdgt *w)
{
	int left = w->vy;
	int n = MIN(left, w->ysize - w->y);

	if(left > 0) w->vy -= n;
	else {
		crsr_move(w, w->crsr, 0);
		return 0;
	}
	crsr_move(w, w->crsr, w->crsr-n);
//	w->crsr -= n;
	return 0;
}

static int kbd_up(struct wdgt *w)
{
	int crsr = (w->crsr == -1)?w->vy:w->crsr;

	/* wdgts without cursor */
	if(w->crsr == -1) {
		if(w->vy) w->vy--;
		return 0;
	}
	if(w->vy && crsr == w->vy) w->vy--;
	if(crsr > 0) crsr_move(w, w->crsr, w->crsr-1);
	return 0;
}

/*
 * Handle widget with visible cursor as well as
 * the ones that can only scroll (crsr == -1)
 */
static int kbd_down(struct wdgt *w)
{
	int nleft = s_nleft(w);

	/* more lines to display and cursor at the end of screen */
	/* first wdgts without cursor */
	if(w->crsr == -1) {
		if(nleft > 0) w->vy++;
		return 0;
	}
	/* cursor at the end of wdgt */
	if(w->crsr - w->vy == w->ysize - w->y) {
		if(nleft > 0) w->vy++;
		else return 0;
	}
	/* cursor enabled and not reached the last line yet */
	if(w->crsr < w->nlines) crsr_move(w, w->crsr, w->crsr+1);
	return 0;
}

static int kbd_page_down(struct wdgt *w)
{
	int nleft = s_nleft(w);
	int n = MIN(nleft, w->ysize - w->y);

	if(nleft > 0) w->vy += n;
	else {
		crsr_move(w, w->crsr, w->nlines);
		return 0;
	}
	if(w->crsr != -1)
		crsr_move(w, w->crsr, w->crsr+n);
	return 0;
}

static int kbd_home(struct wdgt *w)
{
	if(!w->vy && !w->crsr) return KEY_SKIPPED;
	w->vy = 0;
	crsr_move(w, w->crsr, 0);
	return KEY_HANDLED;
}

static int kbd_end(struct wdgt *w)
{
	int nleft = s_nleft(w);

	if(nleft > 0) w->vy += nleft;

	if(w->crsr < w->nlines) {
		crsr_move(w, w->crsr, w->crsr+nleft);
		return KEY_HANDLED;
	}
	return KEY_SKIPPED;
}

static int kbd_right(struct wdgt *w)
{
	/* currently handle wdgts without cursor */
	if(w->crsr == -1 && w->vx < w->pxsize - w->x - w->xsize) w->vx++;
	return 0;
}

static int kbd_left(struct wdgt *w)
{
	if(w->crsr == -1 && w->vx) w->vx--;
	return 0;
}

int scr_keyh(struct wdgt *w, int key)
{
	int ret = KEY_HANDLED;
	DBG("calling key %c for %s", key, w->name);
	switch(key) {
		case KBD_DOWN: 		kbd_down(w); break;
		case KBD_UP:		kbd_up(w); break;
		case KBD_PAGE_DOWN:	kbd_page_down(w); break;
		case KBD_PAGE_UP:	kbd_page_up(w); break;
		case KBD_LEFT:		kbd_left(w); break;
		case KBD_RIGHT:		kbd_right(w); break;
		case KBD_HOME:		ret = kbd_home(w); break;
		case KBD_END:		ret = kbd_end(w); break;
		default: 		ret = KEY_SKIPPED;
	}
	return ret;
}

struct termios tio;
int t_flags;
void term_raw()
{
	struct termios t;
	tcgetattr(0, &tio);
	t = tio;
	t.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(0, TCSANOW, &t);
	t_flags = fcntl(0, F_GETFL);
	fcntl(0, F_SETFL, O_NONBLOCK);
}

void term_rest()
{
	fcntl(0, F_SETFL, t_flags);
	tcsetattr(0, TCSANOW, &tio);
}

int old_curs_vis;	/* this is the cursor mode, set to normal as default */
void curses_init(void)
{
	initscr();
	/* disable cursor */
	old_curs_vis = curs_set(0);
	start_color();
	scr_color_init();
	cbreak();
/*
	nodelay(stdscr,TRUE);
	keypad(stdscr, FALSE);
	meta(stdscr, FALSE);
*/
//	scrollok(main_win, TRUE);
	noecho();
	term_raw();
}

void curses_end()
{
	term_rest();
	endwin();
	/* enable cursor */
	curs_set(old_curs_vis);
}

