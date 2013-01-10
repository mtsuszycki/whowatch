/*
 * Widgets other than main lists and extended information
 */
#include "whowatch.h"

static regex_t cur_reg;

/* 
 * Called by getprocbyname() and user_search(), instead of simple strncmp().
 */
inline int reg_match(const char *s)
{
	return !regexec(&cur_reg, s, 0, 0, REG_NOTEOL); 
}

static int do_search(struct wdgt *w, char *s, int type)
{
	int err;
	void *ret;
	static char errbuf[64];
	DBG("Searching text %s", s);
	err = regcomp(&cur_reg, s, REG_EXTENDED | REG_ICASE | REG_NOSUB);
	if(err) {
		regerror(err, &cur_reg, errbuf, sizeof errbuf);
//		info_box(" Regex error ", errbuf);
		return 0;
	}
	ret = wmsg_send(w, MSND_SEARCH, type);
	regfree(&cur_reg);
	return (int)ret;
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
static u32 cx, cxs, nc, stype;
static char *hint = "Find: ";
static void ihide(struct wdgt *w)
{
	w->wrefresh = 0;
	w->redraw = 0;
	curs_set(0);
//	w->msgh = 0;
}

static void ido_search(struct wdgt *w, int type)
{
	int i;
	for(i = 0; i < cx-cxs; i++) {
		w->mwin->gbuf[i] = mvwinch(CWND(w), 0, i+cxs) & A_CHARTEXT;
		DBG("%d %c", i, w->mwin->gbuf[i]);
	}
	w->mwin->gbuf[i] = 0;
	if(do_search(w, w->mwin->gbuf, type) == 2 && !type) {
		do_search(w, w->mwin->gbuf, 2);
		stype = 2;
	}	
	wmove(CWND(w), 0, cx);
}

static void inpt_rfr(struct wdgt *w)
{
	pnoutrefresh(w->wd, 0, 0, w->mwin->sy, 0, w->mwin->sy, w->mwin->sx);
}

static void input_unhide(struct wdgt *w)
{
	curs_set(1);
	w->wrefresh = inpt_rfr;
	nc = 0;
}	

static int ikbd(struct wdgt *w, int key)
{
	if(key == KBD_BS) {
		if(cx == cxs) return KEY_FINAL;
		mvwdelch((WINDOW*)w->wd, 0, --cx);
//		goto SEARCH;
		return KEY_FINAL;
	}
	if(!isascii(key)) return KEY_SKIPPED;
	if(!nc++) {
		cx = cxs;
		wmove(CWND(w), 0, cx);
		wclrtoeol(CWND(w));
	}
	waddch((WINDOW*)w->wd, key);
	cx++;
//SEARCH:	
//	ido_search(w, 0);
	ido_search(w, 1);
	return KEY_FINAL;
}

static int ikeyh(struct wdgt *w, int key)
{
	if(WIN_HIDDEN(w)) {
		if(key == '/') {
			input_unhide(w);
			return KEY_FINAL;
		}
		return KEY_SKIPPED;
	}
	switch(key) {
		case KBD_ESC: ihide(w); break;
//	case KBD_DEL: wmove(w->wd, 0, --cx); break;
		case KBD_ENTER: if(cx != cxs) {
		//			DBG("Doing search stype %d", stype);
					ido_search(w, 0); 
				}
			        break; //ihide(w);
		default:  return ikbd(w, key);
	}
	return KEY_FINAL;
}

void input_reg(struct wdgt *w)
{
	w->keyh = ikeyh;
	cx = cxs = strlen(hint);
	scr_maddstr(w, hint, 0, 0, strlen(hint)+1);
}

/********** info - upper part of the screen *****************/

static char *get_load()
{
        double d[3] = { 0, 0, 0};
        static char buf[32];
	
        if(proc_getloadavg(d, 3) == -1) return "";
        snprintf(buf, sizeof buf, "load: %.2f, %.2f, %.2f", d[0], d[1], d[2]);
        return buf;
}

static void *infomsg(struct wdgt *w, int type, struct wdgt *s, void *d)
{
	if(type != MSND_INFO) return 0;
	if(!d) {
		wmove((WINDOW*)w->wd, 1, 0);
		wclrtoeol((WINDOW*)w->wd);
		scr_wrefresh(w);
		return (void*)1;
	}	
	scr_attr_set(w, A_BOLD);
	scr_maddstr(w, d, 1, w->x, strlen(d)+1);
	scr_attr_set(w, A_NORMAL);
	scr_wrefresh(w);
	return (void*)1;
}

static void infopr(struct wdgt *w)
{
	char *s;
	int n;
	s = get_load();
	n = strlen(s);
	scr_werase(w);
	scr_addfstr(w, proc_ucount(), 0, 0);
	scr_maddstr(w, s, 0, w->xsize - n + 1, n);
}

void info_reg(struct wdgt *w)
{
	w->periodic = infopr;
	w->wrefresh = scr_wrefresh;
	w->msgh	    = infomsg;
//	w->redraw   = inford;
}

