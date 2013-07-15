/* 
 * Widget for displaying extended information in a separate subindow.
 */
#include "whowatch.h"
#include "config.h"
#include <dlfcn.h>

static void siglist(void *p)
{
	return;
	//println("Signal list will be here.");
}

static struct dsource {
	int key;
	void (*f)(void *);
	int want_crsr;		/* if data is related to main display cursor */
	int (*keyh)(int);

} dss[] = {{'d', euser, 1, 0 }, {'s', esys, 0, 0}, {'p',siglist, 0, 0 } };

static struct dsource *cur_ds = &dss[0];
static struct dsource *mainw = &dss[0];   /* related to main window (proc/user details */
struct wdgt *exti;		          /* for compatibility with pluglib.c          */
static void *cur_val;
static char *ehint = " <- -> [a]up [z]down ";

static void get_crsr(struct wdgt *w)
{
	if(!cur_ds->want_crsr) return;
	cur_val = wmsg_send(w, MWANT_CRSR_VAL, 0);
	WNEED_REDRAW(w);
}

static void exti_redraw(struct wdgt *w)
{
	scr_werase(w);
	scr_box(w, ehint, w->color);	
	/* 
	 * get current cursor value now, not in keyh because
	 * at the time it is called we don't know which of the
	 * ulist/plist main display is visible
	 */
	get_crsr(w);
	if(cur_ds->f) cur_ds->f(cur_val);
	scr_output_end(w);
	DBG("EXTI NOW HAS %d lines", w->nlines);
}

static int do_exti_keyh(struct wdgt *w, int k)
{
	if(WIN_HIDDEN(w)) return KEY_SKIPPED;
	return scr_keyh(w, k);
}
	
/*
 * Even if not visible exti has to keep track of ulist/plist changes
 * to know what extended info should be displyed. 
 */
static void *exti_msgh(struct wdgt *w, int type, struct wdgt *s, void *v)
{
	if(type == MSND_ESOURCE){
		DBG("Received data source from '%s'", s->name);
		w->vx = w->vy = 0;
		return mainw->f = (void (*)(void*))v;
	}	
	return 0;
}

static int exti_disable(struct wdgt *w)
{
	DBG("DISABLING EXTI");	
	w->wrefresh = 0;
	w->redraw = 0;
	wmsg_send(w, MALL_CRSR_UNREG, 0);
	return 1;
}		

static void exti_enable(struct wdgt *w)
{
	DBG("ENABLING EXTI");	
	scr_werase(w);
	w->wrefresh = scr_wrefresh;
	w->redraw = exti_redraw;
	WNEED_REDRAW(w);
}

static void exti_switch(struct wdgt *w, struct dsource *ds, int k, int p)
{
	cur_ds = ds;
	if(!WIN_HIDDEN(w)) {
		if(k == p) {
			exti_disable(w);
			return;
		}
		/* data source has changed */
		WNEED_REDRAW(w);
		w->vy = w->vx = 0;
	} else exti_enable(w);
//	if(ds->want_crsr) wmsg_send(w, MALL_CRSR_REG, 0);
//	else wmsg_send(w, MALL_CRSR_UNREG, 0);
}

static int exti_keyh(struct wdgt *w, int key)
{
	int i, ret = KEY_SKIPPED;
	static int pkey;

	for(i = 0; i < sizeof dss/sizeof(struct dsource); i++) {
		if(dss[i].key != key) continue;
		exti_switch(w, &dss[i], key, pkey);
		pkey = key;
		return KEY_HANDLED;
	}
	if(WIN_HIDDEN(w)) return ret;
	if(cur_ds->keyh) cur_ds->keyh(key);
	switch(key) {
		case 'a': return do_exti_keyh(w, KBD_UP);
		case 'z': return do_exti_keyh(w, KBD_DOWN);
		case KBD_ESC: exti_disable(w); return KEY_FINAL;
		case KBD_LEFT:
		case KBD_RIGHT: scr_keyh(w, key); break;
	}		 
	return ret;
}

void exti_reg(struct wdgt *w)
{
	exti	 = w;
	w->keyh  = exti_keyh;
	w->msgh	 = exti_msgh;
	mwin_msg_on(w);
}

