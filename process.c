#include "whowatch.h"
#include "proctree.h"

int allocated;
int lines_before_curs;	/* how many lines was inserted before cursor */

static struct process *begin;

void proc_del(struct process *p)
{						
	*p->prev=p->next;				
	if (p->next) p->next->prev = p->prev;
	if (p->proc) p->proc->priv = 0;	
	free(p);
	proc_win.d_lines--;					
	allocated--;					
}

void mark_del(void *vp)
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

void clear_list()
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
void check_line(int line)
{
	if (!proc_win.first_line && !proc_win.cursor_line) return;
	if (line <= proc_win.cursor_line + proc_win.first_line)
		proc_win.first_line++;
}

void synchronize()
{
	int l = 0;
	struct proc_t *p = tree_start(tree_pid, tree_pid);
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

void delete_tree_lines()
{
	struct process *u, *p;
	p = begin;
	while(p){
		if (!is_marked(p)){
			p = p->next;
			continue;
		}
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

char get_state_color(char state)
{
	static char m[]="R DZT?", c[]="\5\2\6\4\7\7";
	char *s = strchr(m, state);
	if (!s) return '\3';
	return c[s - m];	
}

char *prepare_line(struct process *p)
{
	char *tree;
	if (!p) return 0;
	tree = tree_string(tree_pid, p->proc);
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

	
char *proc_give_line(int line)
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

pid_t pid_from_tree(int line)
{
	struct process *p;
	for (p = begin; p; p = p->next){
		if (p->line == line) return p->proc->pid;
	}
	return 0;
}	

void dump_list()
{
#ifdef DEBUG
	struct process *p = begin;
	fprintf(debug_file,"\nbegin %p\n", begin);
	while(p){
		fprintf(debug_file, "%p next %p, line %d, pid %d %s\n",p,p->next,
			p->line,p->proc->pid, get_cmdline(p->proc->pid));
		p = p->next;
	}
	fflush(debug_file);
#endif
}

void tree_title(struct user_t *u)
{
	char buf[128];
	if (!u) sprintf(buf,"%d processes", allocated);
	else 
        	snprintf(buf, sizeof buf,
                	"%-14.14s %-9.9s %-6.6s %s",
                	u->parent, u->name, u->tty, u->host);
        buf[sizeof buf - 1] = '\0';
	wattrset(info_win.wd, A_BOLD);
        echo_line(&info_win, buf, 1);
}

void clear_tree_title()
{
	WINDOW *w = info_win.wd;
	wmove(w, 1, 0);
	wclrtoeol(w);
	wrefresh(w);
}

void tree_periodic()
{
	update_tree(mark_del);
	delete_tree_lines();
	synchronize();
	maintree(tree_pid);
}

void maintree(int pid)
{
	struct process *p;
	if(!begin) {
		WINDOW *w = proc_win.wd;
		wmove(w, 0, 0);
		wclrtoeol(w);
		wattrset(w, A_NORMAL);
		waddstr(w, "User logged out");		
		return;
	}
	for(p = begin; p ;p = p->next) {
		if(!p->proc) continue;
		
		/* it is above visible screen */
		if(p->line < proc_win.first_line) continue;

		/* below visible screen */
		if(p->line > proc_win.first_line + proc_win.rows - 1)
			break;
		print_line(&proc_win,prepare_line(p), p->line, 0);
	}
}

void refresh_tree()
{
	maintree(tree_pid);
	updatescr(&proc_win);
}
