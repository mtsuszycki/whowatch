/* 
 * Subwindow displays process/user/system details, static
 * information (ie. help) or signal list. Some subwindows has a builtin
 * plugin and can handle loaded plugins.
 */
#include "whowatch.h"
#include "config.h"
#include "subwin.h"
#include "pluglib.h"
#include <dlfcn.h>

struct subwin sub_info;		/* doesn't support plugins, for static info */
struct subwin sub_main;		/* contains functions for printing sysinfo  */
struct subwin sub_proc;		/* selected process details		    */
struct subwin sub_user;		/* selected user details		    */
struct subwin sub_signal;	/* signal list - this one has an arrow	    */
struct subwin *sub_current;	/* details currently displayed		    */
struct pad_t *main_pad;		/* ncurses pad for printing details	    */
static WINDOW *border_wd;	/* ncurses pad for printing frame	    */
char dlerr[128];		/* error returned by dlopen and dlsym	    */
static int arrow_prev;		/* previous arrow position		    */
static int cur_pid;		/* pid of the selected process		    */

struct signal_t {
	unsigned int sig;
	char *descr;
};

/* 
 * Signals description used only in sub_signal.
 * "sig" field is a signal number and it is used to
 * send appropriate signal according to arrow position.
 */
static struct signal_t signals[] = {
	{ 1, "HUP Hangup detected on controlling terminal" },
	{ 2, "INT Interrupt from keyboard" },
	{ 3, "QUIT Quit from keyboard" },
	{ 4, "ILL Illegal Instruction" },
	{ 6, "ABRT Abort signal from abort(3)" },
	{ 8, "FPE Floating point exception" },
	{ 9, "KILL Kill signal" },
	{ 11, "SEGV Invalid memory reference" }, 
	{ 13, "PIPE Broken pipe: write to pipe with no readers" }, 
	{ 14, "ALRM Timer signal from alarm(2)" },
	{ 15, "TERM Termination signal" }
};

/* 
 * Signal list has an arrow. 'z' and 'a' keys are handled
 * differently. 'y' key is used to accept sending selected
 * signal.
 */
static int handle_arrow(int key)
{
assert(sub_current);
assert(main_pad);
	switch(key) {
	case 'z':
		if(sub_current->arrow < sub_current->lines-1) 
			sub_current->arrow++;
		if(sub_current->arrow > sub_current->offset + main_pad->size_y - PAD_Y) 
			sub_current->offset++;
		break;
	case 'a':
		if(sub_current->arrow) sub_current->arrow--; 
		if(sub_current->arrow == sub_current->offset - 1) 
			sub_current->offset--;
		break;
	case 'y':
dolog(__FUNCTION__": sending %d signal to %d\n" ,signals[sub_current->arrow].sig, cur_pid);
		do_signal(signals[sub_current->arrow].sig, cur_pid);
		return KEY_HANDLED;
	default: return KEY_SKIPPED;
	}
assert(main_pad->wd);
	mvwaddstr(main_pad->wd, arrow_prev, 0, "  ");
	boldon();
	mvwaddstr(main_pad->wd, sub_current->arrow, 0, "->");
	boldoff();
	arrow_prev = sub_current->arrow;
	return KEY_HANDLED;
}

static int keys_inside(int key)
{
assert(sub_current);
assert(main_pad);
	if(!main_pad->wd) return 0;
	if(sub_current->arrow >= 0 && handle_arrow(key)) return KEY_HANDLED;
	switch(key) {
	case 'z':
		if(sub_current->lines > sub_current->offset + main_pad->size_y - PAD_Y) 
			sub_current->offset++;
		break;
	case 'a':
		if(sub_current->offset) sub_current->offset--;
		break;
	case KBD_RIGHT:
		if(SUBWIN_COLS - sub_current->xoffset > main_pad->size_x - PAD_X + 1) 
			 sub_current->xoffset++;
		break;
	case KBD_LEFT:
		if(sub_current->xoffset) sub_current->xoffset--;
		break;
	default: return KEY_SKIPPED;
	}
	return KEY_HANDLED;
}

/* 
 * Builtin draw function for sub_signal. Subwindow with signal
 * list doesn't support plugins.
 */
static void signal_list(void *p)
{
	int size = sizeof signals/sizeof (struct signal_t);
	char buf[16];
	int i, pid = *(int *) p;
dolog(__FUNCTION__": pid %d\n", pid);
	if(pid <= 0) {
		title("No valid pid selected");
		return;
	}
	cur_pid = pid;
	wattrset(border_wd, COLOR_PAIR(7));
	snprintf(buf, sizeof buf, " PID %d", pid);
	mvwaddstr(border_wd, 0, 1, buf);
	waddstr(border_wd,  " - choose signal and press 'y' to send ");
	wattrset(border_wd, COLOR_PAIR(8));
	for(i = 0; i < size; i++) {
		snprintf(buf, sizeof buf, "%d ", signals[i].sig);
		title("  %s ", buf);
		println("%s",signals[i].descr);
	}
	mvwaddstr(main_pad->wd, sub_current->arrow, 0, "->");
}


static void set_size(struct pad_t *p)
{
	p->size_y = screen_rows - PAD_Y;
	p->size_x = screen_cols - PAD_X;
}

static inline void print_titles(void)
{
	mvwaddstr(border_wd, BORDER_ROWS,  BORDER_COLS - 22, 
		" <- -> [a]up, [z]down ");
}

/* 
 * Create ncurses pad with a frame.
 */
static void pad_create(struct subwin *w)
{
assert(sub_current);
	main_pad->wd = newpad(SUBWIN_ROWS, SUBWIN_COLS);
	if(!main_pad->wd) prg_exit(__FUNCTION__ ": cannot create details window.");
	set_size(main_pad);
	w->offset = w->lines = w->xoffset = 0;
	border_wd = newpad(BORDER_ROWS+1, BORDER_COLS+1);
	if(!border_wd) prg_exit(__FUNCTION__ ": cannot create details window.");
	wbkgd(border_wd, COLOR_PAIR(8));
	werase(border_wd);
	box(border_wd, ACS_VLINE, ACS_HLINE);
	print_titles();
	wbkgd(main_pad->wd, COLOR_PAIR(8));
	werase(main_pad->wd);
}	

/* 
 * Destroy subwindow.
 */
static inline void pad_destroy(void)
{
	if(delwin(main_pad->wd) == ERR) prg_exit("Cannot delete details window.");
	if(delwin(border_wd) == ERR) prg_exit("Cannot delete details window.");
	main_pad->wd = 0;
	redrawwin(main_win);
}

void pad_refresh(void)
{
	if(!main_pad->wd) return;
	pnoutrefresh(border_wd, 0, 0, MARG_Y-1, MARG_X-1, LR_Y+1 , LR_X+1);
	pnoutrefresh(main_pad->wd, sub_current->offset, sub_current->xoffset, 
		MARG_Y, MARG_X, LR_Y, LR_X);
}

static inline void draw_plugin(void *p)
{
assert(sub_current->plugin_draw);
	sub_current->plugin_draw(p);
}

/*
 * Get pid (if process tree) or uid (if users list) from current cursor position.
 */
static void *on_cursor(void)
{
	static void *p = 0;
	static int pid;
	if(current == &users_list)
		(char *)p = cursor_user()->name;
	else {
		pid = cursor_pid();
		p = &pid;
	}
	return p;
}

/* 
 * Print data inside subwindow. Usually data comes from builtin plugin
 * and/or dynamically loaded plugin. Function is called periodically and
 * right after cursor movement.
 */
void pad_draw(void)
{
	void *p;
assert(sub_current);
dolog(__FUNCTION__":entering\n");	
	if(!main_pad->wd) return;
	werase(main_pad->wd);
	sub_current->lines = 0; 

	if(sub_current == &sub_info) {
		draw_plugin(0);
//		pad_refresh();
dolog(__FUNCTION__"; info only..skipping draw\n");
		return;
	}
	p = on_cursor();
	/* handle flags returned by loaded plugin */
	if(sub_current->flags & OVERWRITE) 
		draw_plugin(p);
	else if(sub_current->flags & INSERT) {
		draw_plugin(p);
		sub_current->builtin_draw(p);
	}
	else if(sub_current->flags & APPEND) {
		sub_current->builtin_draw(p);
		draw_plugin(p);
	}		
	else {
dolog(__FUNCTION__"; only builtin draw\n");
		sub_current->builtin_draw(p);
	}	
	/* number of data lines probably has changed - adjust offset */
	if(sub_current->offset + main_pad->size_y - PAD_Y > sub_current->lines) 
		sub_current->offset = sub_current->lines - (main_pad->size_y - PAD_Y);
}

/* 
 * Needed for SIGWINCH handling.
 */
void pad_resize(void)
{
	set_size(main_pad);
	if(!main_pad->wd) return;
//	wresize(border_wd, main_pad->size_y-PAD_Y+3, main_pad->size_x-PAD_X+3);
	wresize(border_wd, BORDER_ROWS+1, BORDER_COLS+1);
	werase(border_wd);
	box(border_wd, ACS_VLINE, ACS_HLINE);
	print_titles();
	wbkgd(main_pad->wd, COLOR_PAIR(8));
	werase(main_pad->wd);
	pad_draw();
	pad_refresh();
}	

/* 
 * If subwindow with detailed information doesn't exists then create it.
 * Otherwise destroy.
 */
static void sub_change(struct subwin *w)
{
	if(w->arrow >= 0) w->arrow = arrow_prev = 0;
	if(main_pad->wd) {
		box(border_wd, ACS_VLINE, ACS_HLINE);
		print_titles();
	}
	if(!main_pad->wd) pad_create(w);
assert(w->plugin_draw);
	pad_draw();

//	} else {
//		if(w->plugin_clear) w->plugin_clear();
//		pad_destroy();
//	}
}	
		
static int plugin_dummy(void *p)
{
	return 0;
}
static void dummy(void)
{
	return;
}

static void dummy_draw(void *p)
{
	println("Plugin error: ");
}


char *plugin_load(char *file) 
{
	void *h;
	char *err;
	int i, *type = 0;
	struct subwin *sb[] = { &sub_proc, &sub_user, &sub_main };
	struct subwin *target;
AGAIN:	
	if(!(h = dlopen(file, RTLD_LAZY))) {
		snprintf(dlerr, sizeof dlerr, "%s", dlerror());
		return dlerr;
	}
dolog(__FUNCTION__" dlopen returned %x\n", h);
	/* 
	 * Check if the same plugin has been loaded. 
	 * Plugin's name could be the same but code could be different.
	 * If so, then probably it has changed so we have to unload it
	 * twice and load again. Ses dlopen(3) for reasons of doing this.
	 */
	for(i = 0; i < sizeof sb/sizeof(struct subwin *); i++) {
		if(sb[i]->handle == h) {
dolog(__FUNCTION__": subwin %d has handle %x\n", i, sb[i]->handle); 		
			dlclose(h);
			dlclose(sb[i]->handle);
dolog(__FUNCTION__": plugin already loaded in subwin %d, count %d\n", i, i);
			sb[i]->handle = h = 0;
			goto AGAIN;
		break;
		}
	}	
	type = dlsym(h, "plugin_type");
	if((err = dlerror())) goto ERROR;	
	if(*type >= SUBWIN_NR) {
		snprintf(dlerr, sizeof dlerr, "Unknown plugin type [%d]", *type);
		dlclose(h);
		return dlerr;
	}	
	target = sb[*type];
	target->plugin_init = dlsym(h, "plugin_init");
	if((err = dlerror())) goto ERROR;
	target->plugin_draw = dlsym(h, "plugin_draw");
	if((err = dlerror())) goto ERROR;
	target->plugin_clear = dlsym(h, "plugin_clear");
	target->plugin_cleanup = dlsym(h, "plugin_cleanup");
	/* close previous library if it was loaded */
	if(target->handle) {
		int i;
		i = dlclose(target->handle);
dolog(__FUNCTION__": closing prev library: %d %s\n", i, dlerror());
}
dolog(__FUNCTION__": plugin loaded\n");	
	target->handle = h;
	target->flags = target->plugin_init(on_cursor());
	return 0;
ERROR:
dolog(__FUNCTION__": plugin not loaded\n");
	snprintf(dlerr, sizeof dlerr, "%s", err);
	dlclose(h);
	return dlerr;
}		

/* 
 * For each plugin type set pointer to builtin plugin draw function. 
 */
static inline void builtin_set(void)
{
	sub_proc.builtin_draw = builtin_proc_draw;
	sub_main.builtin_draw = builtin_sys_draw;
	sub_user.builtin_draw = builtin_user_draw;
	sub_signal.builtin_draw = signal_list;
}

/* 
 * Change subwindow according to current main window.
 * Help and sysinfo doesn't change when switching
 * from users list or process tree.
 */
void sub_switch(void)
{
	if(sub_current == &sub_info || sub_current == &sub_main) return;
	if(current == &users_list) {
		if(sub_current == &sub_signal && main_pad->wd) pad_destroy();
		else sub_current = &sub_user;
	}
	else sub_current = &sub_proc;
}

void subwin_init(void)
{
	sub_main.plugin_init = plugin_dummy;
	sub_main.plugin_draw = dummy_draw;
	sub_main.plugin_clear = dummy;
	sub_main.plugin_cleanup = dummy;
	main_pad = calloc(1, sizeof(struct pad_t));
	if(!main_pad) prg_exit(__FUNCTION__": cannot allocate memory.");
	sub_main.arrow = -1;
	memcpy(&sub_proc, &sub_main, sizeof(sub_main));
	memcpy(&sub_user, &sub_main, sizeof(sub_main));
	memcpy(&sub_signal, &sub_main, sizeof(sub_main));
	memcpy(&sub_info, &sub_main, sizeof(sub_main));
	sub_proc.flags |= PERIODIC;
	sub_main.flags |= PERIODIC;
	sub_signal.flags |= PERIODIC;
	sub_signal.arrow = 0;
//sub_user.flags |= INSERT;
	sub_current = &sub_user;
	/* set builtin plugins */
	builtin_set();
dolog(__FUNCTION__": %d %d\n", sub_main.arrow, sub_proc.arrow);
	
}

int sub_keys(int key)
{
	if(keys_inside(key)) return KEY_HANDLED;
	switch(key) {
	case 'd': 
		if(current == &users_list) {
			 sub_current = &sub_user;
			 break;
		}
		sub_current = &sub_proc;
		break;
	case 'l':
		if(current == &users_list) return KEY_HANDLED;
		sub_current = &sub_signal;
		break;	
	case 's':
		sub_current = &sub_main;
		break;
	case 'q':
	case KBD_ESC:
		if(main_pad->wd) {
			pad_destroy();
			return KEY_HANDLED;
		}	
	default: return KEY_SKIPPED;
	}
dolog(__FUNCTION__": key processed\n");	
dolog(__FUNCTION__": cur %d, %d %d\n", sub_current->arrow, sub_main.arrow, sub_proc.arrow);

	sub_change(sub_current);
dolog(__FUNCTION__": cur %d, %d %d\n", sub_current->arrow, sub_main.arrow, sub_proc.arrow);

	return KEY_HANDLED;
}

void sub_periodic(void)
{
	if(!main_pad->wd) return;
	if(sub_current->flags & PERIODIC) {
dolog(__FUNCTION__": doing plugin (and perhaps builtin) draw\n");
		pad_draw();
	}
dolog(__FUNCTION__": doing refresh\n");
	pad_refresh();
}

/*
 * Create subwindow for printing static data (ie. 'about' info).
 */
void new_sub(void (*f)(void *))
{
	sub_info.plugin_draw = f;
	sub_current = &sub_info;
	sub_change(sub_current);
}

/* 
 * Used by key_action() to know if subwin should be redrawn.
 * We don't want to redraw sys info (or 'help') after 
 * cursor movement.
 */	
int can_draw(void)
{
	if(sub_current == &sub_info || sub_current == &sub_main) return 0;
	return 1;
}
