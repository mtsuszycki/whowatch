#include "whowatch.h"
#include "proctree.h"

int allocated;
int lines_before_curs;	/* how many lines was inserted before cursor */

static struct process *begin;
static pid_t tree_root = 1;
static unsigned int show_owner;

static void proc_del(struct process *p)
{						
	*p->prev=p->next;				
	if (p->next) p->next->prev = p->prev;
	if (p->proc) p->proc->priv = 0;	
	free(p);
	proc_win.d_lines--;					
	allocated--;					
}

static void mark_del(void *vp)
{
	struct process *p = (struct process *) vp;
	struct proc_t *q;
	q = p->proc;
	for(q = q->child; q; q = q->broth.nx)
		if (q->priv) mark_del(q->priv);

	p->proc->priv = 0;	
	p->proc = 0;
}

static inline int is_marked(struct process *p)
{
	return p->proc?0:1;
}

static void clear_list()
{
	struct process *p, *q;
	for(p = begin; p; p = q){
		q = p->next;
		proc_del(p);
	}
}

/*
 * If some lines are inserted above cursor, move virtual screen so
 * cursor stays at the same position.
 */
static void check_line(int line)
{
	if (!proc_win.offset && !proc_win.cursor) return;
	if (line <= proc_win.cursor + proc_win.offset)
		proc_win.offset++;
}

static void synchronize(void)
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
		allocated++;
		proc_win.d_lines++;
		memset(z, 0, sizeof *z);
		check_line(l);
		z->line = l++;
		(struct process *) p->priv = z;
		z->proc = p;
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

static void delete_tree_lines()
{
	struct process *u, *p;
	p = begin;
	while(p){
		if (!is_marked(p)){
			p = p->next;
			continue;
		}
//dolog(__FUNCTION__": line %d\n", p->line);
		delete_line(&proc_win, p->line);
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

static char *prepare_line(struct process *p)
{
	char *tree;
	if (!p) return 0;
	tree = tree_string(tree_root, p->proc);
	get_state(p);
	if(show_owner) 
		snprintf(line_buf, buf_size,"\x3%5d %c%c \x3%-8s \x2%s \x3%s", 
			p->proc->pid, get_state_color(p->state), 
			p->state, get_owner_name(p->uid), tree, 
			get_cmdline(p->proc->pid));
	else 
		snprintf(line_buf, buf_size,"\x3%5d %c%c \x2%s \x3%s", 
			p->proc->pid, get_state_color(p->state), 
			p->state, tree, get_cmdline(p->proc->pid));
		
	return line_buf;
}

	
static char *proc_give_line(int line)
{
	struct process *p;
	for (p = begin; p ; p = p->next){
		if (p->line == line){
			if (!p->proc) return "\x1 deleted";
			return prepare_line(p);
		}
	}
	return NULL;
}

static pid_t pid_from_tree(int line)
{
	struct process *p;
	for (p = begin; p; p = p->next){
		if (p->line == line) return p->proc->pid;
	}
	return 0;
}	

pid_t cursor_pid(void) 
{
	return pid_from_tree(proc_win.cursor + proc_win.offset);
}

/*
 * Find process by its command line. 
 * Processes that have line number (data line, not screen)
 * lower than second argument are skipped. Called by do_search.
 */
unsigned int getprocbyname(int l)
{
	struct process *p;
	char *tmp, buf[8];
	for(p = begin; p ; p = p->next){
		if(!p->proc) continue;
		if(p->line <= l) continue;
		/* try the pid first */
		snprintf(buf, sizeof buf, "%d", p->proc->pid);
		if(reg_match(buf)) return p->line;
		/* next process owner */
		if(show_owner && reg_match(get_owner_name(p->uid))) 
			return p->line;
		tmp = get_cmdline(p->proc->pid);
		if(reg_match(tmp)) return p->line;
	}
	return -1;
}

#ifdef DEBUG
static void dump_list(void )
{
	struct process *p = begin;
	fprintf(debug_file,"\nbegin %p\n", begin);
	while(p){
		fprintf(debug_file, "%p next %p, line %d, pid %d %s\n",p,p->next,
			p->line,p->proc->pid, get_cmdline(p->proc->pid));
		p = p->next;
	}
	fflush(debug_file);
}
#endif

void tree_title(struct user_t *u)
{
	char buf[64];
	if(!u) snprintf(buf, sizeof buf, "%d processes", proc_win.d_lines);
	else snprintf(buf, sizeof buf, "%-14.14s %-9.9s %-6.6s %s",
                	u->parent, u->name, u->tty, u->host);
	wattrset(info_win.wd, A_BOLD);
        echo_line(&info_win, buf, 1);
}

static void clear_tree_title(void)
{
	WINDOW *w = info_win.wd;
	wmove(w, 1, 0);
	wclrtoeol(w);
	wnoutrefresh(w);
}


static void draw_tree(void)
{
	struct process *p;
	if(!begin) {
		WINDOW *w = proc_win.wd;
		wmove(w, 0, 0);
		wclrtoeol(w);
		wattrset(w, A_NORMAL);
		waddstr(w, "User logged out");
		current->d_lines = 1;		
		return;
	}
	for(p = begin; p ;p = p->next) {
		if(!p->proc) continue;
		if(above(p->line, &proc_win)) continue;
		if(below(p->line, &proc_win)) break;
		print_line(&proc_win,prepare_line(p), p->line, 0);
	}
}

static void tree_periodic(void)
{
	update_tree(mark_del);
	delete_tree_lines();
	synchronize();
	draw_tree();
}

void show_tree(pid_t pid)
{
//        print_help(state);
	proc_win.offset = proc_win.cursor = 0;
	tree_root = INIT_PID;
	if(pid > 0) tree_root = pid; 
        tree_periodic();
//        tree_title(pid);
//	tree_title(0); // change it!
}

void do_signal(int sig, int pid)
{
	send_signal(sig, pid);
	tree_periodic();
	/* get details - pid on cursor probably has changed */
	pad_draw();
}

static int signal_keys(int key)
{
        int signal = 0;

	if(!(key&KBD_CTRL)) return KEY_SKIPPED;
	key &= KBD_MASK;
dolog(__FUNCTION__": %x %x\n", key, 'H');	

	switch(key) {
	case 'K': signal = 9; break;
	case 'U': signal = 1; break;
	case 'T': signal = 15; break;
	}
	if(signal) do_signal(signal, cursor_pid());
	return KEY_HANDLED;
}	

static int proc_key(int key)
{
	if(signal_keys(key)) return KEY_HANDLED;
        switch(key) {
        case ENTER:
                werase(main_win);
                current = &users_list;
		print_help();
                clear_tree_title();
                clear_list();
                users_list_refresh();
                sub_switch();
		pad_draw();
		break;
        case 'o':
                show_owner ^= 1;
                draw_tree();
                break;
        case 't':
                show_tree(INIT_PID);
		tree_title(0);
                break;
        default: return KEY_SKIPPED;
        }
        return KEY_HANDLED;
}


void procwin_init(void)
{
	proc_win.giveme_line = proc_give_line;
	proc_win.keys = proc_key;
	proc_win.periodic = tree_periodic;
	proc_win.redraw = draw_tree;
}
