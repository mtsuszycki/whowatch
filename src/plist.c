/*

SPDX-License-Identifier: GPL-2.0+

Copyright 2006 Michal Suszycki <mt_suszycki@yahoo.com>
Copyright 2017-2018 Paul Wise <pabs3@bonedaddy.net>

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

#include "whowatch.h"
#include "proctree.h"

#include <signal.h>

//static char *hlp = "\001[ENT]users [c]md [d]etails [o]owner [s]ysinfo sig[l]ist ^[T]ERM";
static char *hlp_init_pid = "\001[ENT]users [d]etails [o]wner [s]ysinfo [l]ine-numbers [^t]ERM [/]search";
static char *hlp = "\001[ENT]users all[t]ree [d]etails [o]wner [s]ysinfo [l]ine-numbers [^t]ERM [/]search";

static DECL_LIST_HEAD(plist);

static struct process *begin;
static pid_t tree_root = 1;
static unsigned int show_owner;
static unsigned int show_linenr;
static struct __pstat {
	u32	nr, rn, sl, st, zm;
} pstat;

static void pstat_update(struct process *p, int n)
{
	pstat.nr += n;
	if(!p->proc) return;
	switch(p->proc->state) {
		case 'S': pstat.sl += n; break;
		case 'R': pstat.rn += n; break;
		case 'Z': pstat.zm += n; break;
		case 'D': pstat.st += n; break;
	}
}

static void proc_del(struct process *p)
{
	*p->prev=p->next;
	if (p->next) p->next->prev = p->prev;

//	LIST_DEL(&p->plist_l);

	if (p->proc) p->proc->priv = 0;
	free(p);
}

static void mark_del(void *vp)
{
	struct process *p = (struct process *) vp;
	struct proc_t *q;
	q = p->proc;
	for(q = q->child; q; q = q->broth.nx)
		if (q->priv) mark_del(q->priv);
	pstat_update(p, -1);
	p->proc->priv = 0;
	p->proc = 0;
}

static inline int is_marked(struct process *p)
{
	return p->proc?0:1;
}

static void clear_list(void)
{
	/*struct list_head *h;
	struct process *p;

	h = plist.next;
	while(h != &plist) {
		h = h->next;
		p = list_entry(h, struct process, plist_l);
		proc_del(p);
	}*/

	struct process *p, *q;
	for(p = begin; p; p = q){
		q = p->next;
		proc_del(p);
	}
}

static void synchronize(struct wdgt *w)
{
	int l = 0;
	struct proc_t *p = tree_start(tree_root, tree_root);
	struct process **current = &begin, *z;
	while(p){
		if (*current && p->priv){
			(*current)->line = l++;
			(*current)->proc->priv = *current;
			p = tree_next();
			current = &((*current)->next);
			continue;
		}
		z = malloc(sizeof *z);
		if (!z) allocate_error();
//		allocated++;
//		proc_win.d_lines++;
		memset(z, 0, sizeof *z);
		scr_linserted(w, l);
		z->line = l++;
		p->priv = z;
		z->proc = p;
		pstat_update(z, 1);
		if (*current){
			z->next = *current;
			(*current)->prev = &z->next;
		}
		*current = z;
		z->prev = current;
		current = &(z->next);
		p = tree_next();
	}

}

static void delete_tree_lines(struct wdgt *w)
{
	struct process *u, *p;
	p = begin;
	while(p){
		if (!is_marked(p)){
			p = p->next;
			continue;
		}
//		delete_line(&proc_win, p->line);
		scr_ldeleted(w, p->line);
		u = p;
		while(u) {
			u->line--;
			u = u->next;
		}
		u = p->next;
		proc_del(p);
		p = u;
	}
}

static char get_state_color(char state)
{
	static char m[]="R DZT?", c[]="\5\2\6\4\7\7";
	char *s = strchr(m, state);
	if (!s) return '\3';
	return c[s - m];
}

static char *prepare_line(struct wdgt *w, struct process *p)
{
	char *tree, state = p->proc->state;
	int offset = 0;
	if (!p) return 0;
	tree = tree_string(tree_root, p->proc);
	if(state == 'S') state = ' ';
	if (show_linenr)
		offset = snprintf(w->mwin->gbuf, w->mwin->gbsize, "\x6%4d ", p->line + 1);
	if (offset < 0) return "";
	if(show_owner)
		snprintf(w->mwin->gbuf+offset, w->mwin->gbsize-offset ,"\x3%5d %c%c \x3%-8s \x2%s \x3%s",
			p->proc->pid, get_state_color(state),
			state, get_owner_name(proc_pid_uid(p->proc->pid)), tree,
			get_cmdline(p->proc->pid));
	else
		snprintf(w->mwin->gbuf+offset, w->mwin->gbsize-offset,"\x3%5d %c%c \x2%s \x3%s",
			p->proc->pid, get_state_color(state),
			state, tree, get_cmdline(p->proc->pid));

	return w->mwin->gbuf;
}

/*
 * Find process by its command line.
 * 't' is type of operation:
 *  0 - search below cursor only
 *  1 - below but including cursor line
 *  2 - from the beginning
 */
static int  getprocbyname(struct wdgt *w, int t)
{
	struct process *p;
	char *tmp, buf[8];
	int l = w->crsr;
	for(p = begin; p ; p = p->next){
		if(!p->proc) continue;
		if(!t && p->line <= l) continue;
		if(t == 1 && p->line < l) continue;
		/* try the pid first */
		snprintf(buf, sizeof buf, "%d", p->proc->pid);
		if(reg_match(buf)) goto found; //return p->line;
		/* next process owner */
		if(show_owner && reg_match(get_owner_name(p->uid)))
			goto found; //return p->line;
		tmp = get_cmdline(p->proc->pid);
		if(reg_match(tmp)) goto found; // return p->line;
	}
	return 2;
found:
	scr_crsr_jmp(w, p->line);
	return 1;
}
/*
 * Send current processes information to info widget
 */
static void ptreeinfo(struct wdgt *w, int i)
{
	int n, size = w->mwin->gbsize;
	char *buf = w->mwin->gbuf;
	if(!i) {
		wmsg_send(w, MSND_INFO, 0);
		return;
	}
	n = snprintf(buf, size, "%u tasks: %u running, %u sleeping",
			pstat.nr, pstat.rn, pstat.sl);
	if(pstat.st) n += snprintf(buf+n, size-n,", %u stopped", pstat.st);
	if(pstat.zm) n += snprintf(buf+n, size-n,", %u zombie", pstat.zm);
	wmsg_send(w, MSND_INFO, buf);
}

static void draw_tree(struct wdgt *w)
{
	struct process *p;
	/* init scr output function */
	scr_output_start(w);
	if(!begin) {
		scr_maddstr(w, "User has logged out", 0, 0, 20);
		return;
	}
	for(p = begin; p ;p = p->next) {
		if(!p->proc) continue;
		scr_addfstr(w, prepare_line(w, p), p->line, 0);
	}
	scr_output_end(w);
}

/*
 * Find pid of the process that is on cursor line
 */
static pid_t crsr_pid(int line)
{
	struct process *p;
	for(p = begin; p; p = p->next){
		if(p->line == line) return p->proc->pid;
	}
	return 0;
}
static void ptree_periodic(struct wdgt *w)
{
	DBG("**** building process tree root %d *****", tree_root);
	update_tree(mark_del);
	delete_tree_lines(w);
	synchronize(w);
	ptreeinfo(w, 1);
}

static void ptree_redraw(struct wdgt *w)
{
	DBG("--- redraw tree");
	scr_werase(w);
	draw_tree(w);
}

void do_signal(struct wdgt *w, int sig, int pid)
{
	send_signal(sig, pid);
	ptree_periodic(w);
	WNEED_REDRAW(w);
}

static int signal_keys(struct wdgt *w, int key)
{
	int signal = 0;

	if(!(key&KBD_CTRL)) return KEY_SKIPPED;
	key &= KBD_MASK;
	switch(key) {
	case 'I': signal = SIGINT; break;
	case 'K': signal = SIGKILL; break;
	case 'U': signal = SIGHUP; break;
	case 'T': signal = SIGTERM; break;
	}
	if(signal) do_signal(w, signal, crsr_pid(w->crsr));
	return KEY_HANDLED;
}

static void *pmsgh(struct wdgt *w, int type, struct wdgt *s, void *d)
{
	static u32 pid;
	switch(type) {
		case MWANT_CRSR_VAL:
			pid = crsr_pid(w->crsr);
			return &pid;
		case MSND_SEARCH:
			return (int *)(intptr_t)(getprocbyname(w, (int)d));
			break;
	}
	return 0;
}

static void ptree_hlp(struct wdgt *w)
{
	char *s = hlp_init_pid;
	if(tree_root != INIT_PID) s = hlp;
	wmsg_send(w, MCUR_HLP, s);
}

static void ptree_unhide(struct wdgt *w)
{
	ptree_hlp(w);
	ptree_periodic(w);
	w->periodic = ptree_periodic;
	w->msgh = pmsgh;
	w->redraw = ptree_redraw;
	w->wrefresh = scr_wrefresh;
	WNEED_REDRAW(w);
	w->vy = w->vy = w->crsr = 0;
}

static void ptree_hide(struct wdgt *w)
{
	DBG("Deleting ptree  from lists");
	w->redraw = w->wrefresh = 0;
	clear_list();
	w->periodic = 0;
	w->msgh = 0;
	ptreeinfo(w, 0);
	bzero(&pstat, sizeof pstat);
}

static void root_change(struct wdgt *w)
{
	void *p;
	p = wmsg_send(w, MWANT_UPID, 0);
	if(p) tree_root = *(pid_t*)p;
	else tree_root = INIT_PID;
}

/*
 * Give up main window if currently printing output there
 * or start printing if not there
 */
static void pswitch(struct wdgt *w, int k, int p)
{
	if(!WIN_HIDDEN(w)) {
		if(k == p || k == KBD_ENTER) {
			if(k != 't') ptree_hide(w);
			return;
		}
		if(k == 't') {
			tree_root = INIT_PID;
			ptree_periodic(w);
			WNEED_REDRAW(w);
			return;
		}
	} else {
		if(k == KBD_ENTER) root_change(w);
		else if(k == 't') tree_root = INIT_PID;
		wmsg_send(w, MSND_ESOURCE, eproc);
		ptree_unhide(w);
	}
}

static int pkeyh(struct wdgt *w, int key)
{
	int ret = KEY_HANDLED;
	int ctrl = key&KBD_MASK;
	static int pkey;

	/* hide or unhide wdgt */
	if(key == 't' || key == KBD_ENTER) {
		pswitch(w, key, pkey);
		pkey = key;
		return KEY_HANDLED;
	}
	/* must be visible to process keys below */
	if(WIN_HIDDEN(w)) return KEY_SKIPPED;
	if(ctrl == 'I' || ctrl == 'K' || ctrl == 'U' || ctrl == 'T') {
		signal_keys(w, key);
		return ret;
	}
	switch(key) {
		case 'o': show_owner ^= 1; WNEED_REDRAW(w); break;
		case 'r': ptree_periodic(w); WNEED_REDRAW(w); break;
		case 'l': show_linenr ^=1 ; WNEED_REDRAW(w); break;
/*
		case KBD_UP:
		case KBD_DOWN:
		case KBD_PAGE_UP:
		case KBD_PAGE_DOWN:
*/
		default: ret = scr_keyh(w, key);
	}
	return ret;
}

/*
 * Register all process tree functions here.
 */
void ptree_reg(struct wdgt *w)
{
	w->keyh     = pkeyh;
	w->msgh	    = pmsgh;
	mwin_msg_on(w);
}

