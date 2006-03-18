#include "whowatch.h"
#include "config.h"
#include "ulist.h"

/* indexes in prot_tab[] */
#define	SSH		0
#define TELNET		1
#define LOCAL		2

static struct prot_t prot_tab[] = {
#ifdef HAVE_PROCESS_SYSCTL
	{ "sshd", 22, 0 }, { "telnetd", 23, 0 }, { "init", -1, 0 }
#else
	{ "(sshd", 22, 0 }, { "(in.telnetd)", 23, 0 }, { "(init)", -1, 0 }
#endif
};
//static char *hlp = "\001[F1]Help [F9]Menu [ENT]proc all[t]ree [i]dle/cmd [c]md [d]etails [s]ysinfo",
static char *hlp = "[ENT]proc all[t]ree [d]etails [s]ysinfo";
	
static u32 nusers;
static struct wdgt *self;

static void u_count(char *name, int p)
{
	int i;
	struct prot_t *t;
	nusers += p;
	for(i = 0; i < sizeof prot_tab/sizeof(struct prot_t); i++){
		t = &prot_tab[i];
		if(strncmp(t->s, name, strlen(t->s))) continue;
		t->nr += p;
	}
}

/*
 * After deleting line, update line numbers in each user structure 
 */
void update_line(int line)
{
	struct user_t *u;
	struct list_head *tmp;
	list_for_each(tmp, &users_l) {
		u = list_entry(tmp, struct user_t, head); 
		if(u->line > line) u->line--;
	}	
}
			
/* 
 * Create new user structure and fill it
 */
struct user_t *alloc_user(struct utmp *entry)
{
	struct user_t *u;
	int ppid;
	
	u = calloc(1, sizeof *u);
	if(!u) errx(1, "Cannot allocate memory.");
	strncpy(u->name, entry->ut_user, UT_NAMESIZE);
	strncpy(u->tty, entry->ut_line, UT_LINESIZE);
	strncpy(u->host, entry->ut_host, UT_HOSTSIZE);
#ifdef HAVE_UTPID		
	u->pid = entry->ut_pid;
#else
	u->pid = get_login_pid(u->tty);
#endif
 	if((ppid = get_ppid(u->pid)) == -1)
		strncpy(u->parent, "can't access", sizeof u->parent);
	else 	strncpy(u->parent, get_name(ppid), sizeof u->parent - 1);
	u->line = nusers;	
	return u;
}

static struct user_t* new_user(struct utmp *ut)
{
	struct user_t *u;
	u = alloc_user(ut);
	list_add(&u->head, &users_l);
	u_count(u->parent, LOGIN);
	return u;
}
	
static void uprint(struct user_t *u, struct wdgt *w)
{
	int n;
	scr_attr_set(w, A_BOLD);
	n = snprintf(w->mwin->gbuf, w->mwin->gbsize, "%-14.14s %-9.9s %-6.6s %-19.19s %s",
		u->parent, u->name, u->tty, u->host, 
		toggle?count_idle(u->tty):get_w(u->pid));
	scr_maddstr(w, w->mwin->gbuf, u->line, 0, n);
}

void uredraw(struct wdgt *w)
{
	struct list_head *tmp;
	struct user_t *u;
	scr_werase(w);	
	scr_output_start(w);
	list_for_each_r(tmp, &users_l) {
		u = list_entry(tmp, struct user_t, head);
		uprint(u, w);
	}
	scr_output_end(w);
}
	
/*
 * Gather informations about users currently on the machine
 * Called only at start or restart
 */
void read_utmp(void)		
{
	int fd, i;
	static struct utmp entry;
	struct user_t *u;
	
	if ((fd = open(UTMP_FILE ,O_RDONLY)) == -1) err_exit(1, "Cannot open utmp");
	while((i = read(fd, &entry,sizeof entry)) > 0) {
		if(i != sizeof entry) errx(1, "Error reading " UTMP_FILE );
#ifdef HAVE_USER_PROCESS
		if(entry.ut_type != USER_PROCESS) continue;
#else
		if(!entry.ut_name[0]) continue;
#endif
		u = new_user(&entry);
	}
	close(fd);
	return;
}

/*
 * get user entry from specific line (cursor position)
 */
struct user_t *cursor_user(int line)	
{
	struct user_t *u;
	struct list_head *h;
	DBG("looking for user on line %d", line);
//	int line = 0; //current->cursor + current->offset;
	list_for_each(h, &users_l) {
		u = list_entry(h, struct user_t, head);
		if(u->line == line) return u;
	}
	return 0;
}

static void udel(struct user_t *u, struct wdgt *w)
{
	scr_delline(w, u->line);
	scr_ldeleted(w, u->line);
	update_line(u->line);
	u_count(u->parent, LOGOUT);
	list_del(&u->head);
	free(u);
}

char *proc_ucount(void)
{
        static char buf[64];

	int other = nusers - prot_tab[LOCAL].nr - prot_tab[TELNET].nr - prot_tab[SSH].nr;
        snprintf(buf, sizeof buf - 1,"\x1%d users: %d local, %d telnet, %d ssh, %d other\x3",
   		nusers, prot_tab[LOCAL].nr, prot_tab[TELNET].nr, prot_tab[SSH].nr, other);
	return buf;
}

static void open_wtmp(int *wtmp_fd)
{
        if((*wtmp_fd = open(WTMP_FILE ,O_RDONLY)) == -1) err_exit(1, "Cannot open wtmp");
        if(lseek(*wtmp_fd, 0, SEEK_END) == -1) err_exit(1, "Cannot seek end wtmp");
}

/*
 * Check wtmp for logouts or new logins
 */
static void check_wtmp(struct wdgt *w)
{
	static int wtmp_fd;
	struct user_t *u;
	struct list_head *h;
	struct utmp entry;
	int i, changed = 0;
	if(!wtmp_fd) open_wtmp(&wtmp_fd);	
	
	while((i = read(wtmp_fd, &entry, sizeof entry)) > 0){ 
		if (i < sizeof entry) prg_exit("Error reading wtmp");
		/* user just logged in */
#ifdef HAVE_USER_PROCESS
		if(entry.ut_type == USER_PROCESS) {
#else
		if(entry.ut_user[0]) {
#endif
			u = new_user(&entry);
			changed = 1;
			continue;
		}
#ifdef HAVE_DEAD_PROCESS
		if(entry.ut_type != DEAD_PROCESS) continue;
#else
//		if(entry.ut_line[0]) continue;
#endif
	/* user just logged out */
		list_for_each(h, &users_l) {
			u = list_entry(h, struct user_t, head);
			if(strncmp(u->tty, entry.ut_line, UT_LINESIZE)) 
				continue;
			udel(u, w);	
			changed = 1;
			break;
		}
	}
	if(!changed) return;
}

static void ulist_hlp(struct wdgt *w)
{		
	DBG("sending %s", hlp);
	wmsg_send(w, MCUR_HLP, hlp);
}

void users_init(void)
{
	read_utmp();
}

static void ulist_periodic(struct wdgt *w)
{
	static int i;

	if(!i) {
		wmsg_send(w, MCUR_HLP, hlp);
		i = 1;
	}
	DBG("ulist periodic");
	check_wtmp(w);
}

static void *cval(void)
{
	static struct user_t *u;
	
	u = cursor_user(self->crsr);
	if(u) return &u->pid;
	return 0;
}

/* 
 * Needed for search function. If parent, name, tty, host or command line
 * matches then returns line number of this user.
 */
static int user_search(struct wdgt *w, int t)
{
	struct user_t *u;
	struct list_head *h;
	int l = w->crsr;
	
	list_for_each(h, &users_l) {
		u = list_entry(h, struct user_t, head);
		if(!t && u->line <= l) continue;
		if(t == 1 && u->line < l) continue;
		if(reg_match(u->parent)) goto found;
		if(reg_match(u->name)) goto found;
		if(reg_match(u->tty)) goto found;
		if(reg_match(u->host)) goto found;
		if(reg_match(toggle?count_idle(u->tty):get_w(u->pid))) 
			goto found;
	}
	return 2;
found:
	scr_crsr_jmp(w, u->line);
	return 1;
}

static void *umsgh(struct wdgt *w, int type, struct wdgt *s, void *d)
{	
	struct user_t *u;
	/* accept it even not visible, ptree can ask about user pid*/
	if(type == MWANT_UPID) {
		u = cursor_user(w->crsr);
		if(u) return &u->pid;
		else return 0;
	}
	if(WIN_HIDDEN(w)) return 0;
DBG("ulist responded to type %d", type);	
	switch(type) {
		case MALL_CRSR_REG: w->flags |= WDGT_CRSR_SND; break;
		case MALL_CRSR_UNREG: w->flags &= ~ WDGT_CRSR_SND; break;
		case MWANT_CRSR_VAL: u = cursor_user(w->crsr);
				if(u) return u->name;
				else return "No user found";
		case MSND_SEARCH:
				return user_search(w, (u32)d);
				break;
	}
	
	return 0;
}

/*
 * Give up main window if currently printing output there
 * or start printing if not there
 */
static void uswitch(struct wdgt *w, int k, int p)
{
	if(!WIN_HIDDEN(w)) {
		DBG("Deleting ulist from lists");
		w->redraw = 0;
		w->wrefresh = 0;
	}
	else {
		if(k == 't') return;
		ulist_hlp(w);
		w->redraw = uredraw;
		w->wrefresh = scr_wrefresh;
		WNEED_REDRAW(w);
		wmsg_send(w, MSND_ESOURCE, euser);
	}	
}

static void crsr_send(struct wdgt *w)
{
	struct user_t *u;
	
	if(!(w->flags & WDGT_CRSR_SND)) return;
	u = cursor_user(w->crsr);
	if(!u) return;
	DBG("sending current name %s", u->name);
	wmsg_send(w, MCUR_CRSR, u->name);
}


static int ukeyh(struct wdgt *w, int key)
{
	int ret = KEY_SKIPPED;
	static int pkey;

	/* hide/unhide widget */
	if(key == 't' || key == KBD_ENTER) {
		uswitch(w, key, pkey);
		pkey = key;
		return KEY_HANDLED;
	}
	if(WIN_HIDDEN(w)) return ret;	
	switch(key) {
		default:  ret = scr_keyh(w, key);
			  if(ret == KEY_HANDLED) crsr_send(w);
	}
	return ret;
}

void ulist_reg(struct wdgt *w)
{
	w->periodic = ulist_periodic;
	w->redraw = uredraw;
	w->wrefresh = scr_wrefresh;
	w->keyh = ukeyh;
	w->msgh = umsgh;
	w->mwin->cval = cval;
	mwin_msg_on(w);
	self = w;
}

