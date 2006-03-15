/*
 * Widgets other than main lists and extended information
 */
#include "whowatch.h"
#include <regex.h>


static char prev_search[64];
//static struct window *prev;
static regex_t cur_reg;

/* 
 * Called by getprocbyname() and user_search(), instead of simple strncmp().
 */
inline int reg_match(const char *s)
{
	return !regexec(&cur_reg, s, 0, 0, REG_NOTEOL); 
}

void do_search(char *s)
{
	unsigned int p;
	int err;
	static char errbuf[64];
	
//	if(prev && current != prev) clear_search();
//	prev = current;
	err = regcomp(&cur_reg, s, REG_EXTENDED | REG_ICASE | REG_NOSUB);
	if(err) {
		regerror(err, &cur_reg, errbuf, sizeof errbuf);
//		info_box(" Regex error ", errbuf);
		return;
	}
//	if(current == &proc_win)
//		p = getprocbyname(current->cursor + current->offset);
//	else p = user_search(current->cursor + current->offset);
	
	snprintf(prev_search, sizeof prev_search, "%s", s);
	//set_search(prev_search);
	regfree(&cur_reg);
	if(p == -1) return;
	/* move the cursor to appropriate line */
	//to_line(p, current);
	//pad_draw();
//dolog(__FUNCTION__": %d\n", p);
}

static char *cur_hlp;
static void hlp_wrefresh(struct wdgt *w)
{
	pnoutrefresh(w->wd, 0, 0, w->mwin->sy, 0, w->mwin->sy, w->mwin->sx);
//	scr_wrefresh(w);
}

static void hlp_redraw(struct wdgt *w)
{
	scr_addfstr(w, cur_hlp, 0, 0);
}	

static void *hlp_msgh(struct wdgt *w, int type, struct wdgt *sndr, void *s)
{
	if(type != MCUR_HLP) return 0;
	WNEED_REDRAW(w);
	return cur_hlp = s;
}

void hlp_reg(struct wdgt *w)
{
	w->wrefresh = hlp_wrefresh;
	w->redraw   = hlp_redraw;
	w->msgh	    = hlp_msgh;
}

/**************** input wdgt *******************/
static int cx;// cy;
static int ikeyh(struct wdgt *w, int);
//static char *hint = " Pattern ";
static struct input {
	WINDOW *wd;
	int  ishow;	/* if show input bar */
	int  cy,cx,xsize;
} input;

static struct input *inp = &input;

static void ihide(struct wdgt *w)
{
	w->wrefresh = 0;
	w->redraw = 0;
	curs_set(0);
}

static void iredraw(struct wdgt *w)
{
	scr_werase(w);
	scr_box(w, "bleble", 9);
}

static void input_ln(struct wdgt *w, char *s)
{
	if(inp->wd) return;
	inp->wd = newpad(1, w->xsize - w->x);
	wbkgd(inp->wd, COLOR_PAIR(7));
}

static void button(struct wdgt *w, int y, int x, char *s, int f)
{
	if(y == -1) y = (w->ysize - w->y)*9/10;
	if(f) wattrset((WINDOW*)w->wd, A_REVERSE);
	mvwaddstr(w->wd, y, x, s); 
	wattrset((WINDOW*)w->wd, A_NORMAL);
}	

void input_reg(struct wdgt *w)
{
	w->keyh = ikeyh;
}

static void buttons1(struct wdgt *w)
{
	button(w, -1, (w->xsize - w->x)/2, "[ Cancel ]", 0);
}

static void buttons2(struct wdgt *w)
{
	button(w, -1, (w->xsize - w->x)/2-8, "[ OK ]", 1);
	buttons1(w);
}

static void irefresh(struct wdgt *w)
{	
	scr_wrefresh(w);
	if(inp->wd) {
		int y = w->ysize - w->y;
		int x = w->xsize - w->x;
		inp->xsize = w->xsize - w->x;
		pnoutrefresh(inp->wd, 0, 0, w->y + y/2, w->x, w->y + y/2+1, x);
	}	
	buttons2(w);
}
	
static void input_unhide(struct wdgt *w)
{
//	w->wrefresh = scr_wrefresh;
	w->wrefresh = irefresh;
	w->redraw   = iredraw;
	w->prv	    = &input;
	iredraw(w);
	buttons2(w);
	input_ln(w, " Pattern ");
}	

static void kbd_bs(struct wdgt *w)
{
	if(!inp->cx) return;
	mvwdelch(inp->wd, inp->cy, --inp->cx);
		
}

static int ikeyh(struct wdgt *w, int key)
{
	if(WIN_HIDDEN(w) && key != '/') return KEY_SKIPPED ;
	switch(key) {
		case '/': input_unhide(w); break;
		case KBD_ESC: ihide(w); break;
		case KBD_DEL: wmove(w->wd, 0, --cx); break;
		case KBD_ENTER: ihide(w);
		case KBD_BS: kbd_bs(w); break;
		default: if(isascii(key) && inp->cx < inp->xsize-10) {
		wattrset(inp->wd, A_REVERSE);
					 waddch(inp->wd, key); 
					 inp->cx++;
				 }	 
		 wattrset(inp->wd, A_NORMAL);
					 break;
				 
	}
	return KEY_FINAL;
}


