#include "whowatch.h"
#include <sys/types.h>
#include <sys/wait.h>

#ifndef UTMP_FILE
#define UTMP_FILE 	"/var/run/utmp"
#endif

#ifndef WTMP_FILE
#define WTMP_FILE 	"/var/log/wtmp"
#endif

#define TIMEOUT 	3
#define LOGIN		0
#define LOGOUT		1		

enum key { ENTER=0x100, UP, DOWN, LEFT, RIGHT, DELETE, ESC, CTRL_K, CTRL_I};
enum State{ USERS_LIST, PROC_TREE, INIT_TREE } state;

struct window users_list, proc_win;
struct window *windows[] = { &users_list, &proc_win, &proc_win };
int screen_size_changed;	/* not yet implemented */

#ifdef DEBUG
FILE *debug_file;
#endif

void segv_handler();

int toggle = 0;		/* if 0 show cmd line else show idle time */
int signal_sent;

int how_many, telnet_users, ssh_users, local_users;

struct user *begin;	/* pointer to begining of users list 		*/
int wtmp_fd;		/* wtmp file descriptor 			*/
int screen_rows;	/* screen rows returned by ioctl		*/
int screen_cols;	/* screen cols returned by ioctl		*/

char *line_buf;		/* global buffer for line printing		*/
int buf_size;		/* allocated buffer size			*/

char * const ssh    = 	"(sshd";    /* we don't know version of sshd 	*/
char * const local  =	"(init)";
char * const telnet = 	"(in.telnetd)";

int ssh_port = 22;
int telnet_port = 23;
int local_port = -1;

int tree_pid = 1;	
int show_owner;		/* if 1 display processes owners		*/

void show_cmd_or_idle();
void dump_users();	/* for debugging				*/
void cleanup();

void allocate_error(){
	curses_end();
	fprintf(stderr,"Cannot allocate memory.\n");
	exit (1);
}

/*  
 *  update number of users (ssh users, telnet users...) and change 
 *  prot in the appropriate user structure.
 */
void update_nr_users(char *login_type, int *pprot, int x)
{
	int i;
	void *tab[] = {	
			ssh, 	&ssh_port, 	&ssh_users,
			telnet, &telnet_port, 	&telnet_users,
			local, 	&local_port, 	&local_users,
	      	       };
	*pprot = 0;		       
	for (i = 0; i < 9 ; i += 3){
		if (!strncmp( (char *) tab[i], login_type, strlen(tab[i]))){
			*pprot = * (int *) tab[i+1];
			if (x == LOGIN) (*(int *) tab[i+2])++;
			else (*(int *) tab[i+2])--;
			break;
		}
	}
}
void update_line_nr(int line)
{
	struct user *p;
	for(p = begin; p; p = p->next)
		if (p->line > line)
			p->line--;
}
			
/* 
 * Create new user structure and fill it
 */
struct user *allocate_user(struct utmp *entry)
{
	struct user *tmp;
	int ppid;
	
	if (!(tmp = malloc(sizeof (struct user)) ))
		allocate_error();
	memset(tmp, 0, sizeof *tmp);
	if (!(tmp->name = malloc(UT_NAMESIZE + 1)))
		allocate_error();
	tmp->name[UT_NAMESIZE] = '\0';
	strncpy(tmp->name,entry->ut_user,UT_NAMESIZE);

	if (!(tmp->tty = malloc(UT_LINESIZE + 1)))
		allocate_error();
	tmp->tty[UT_LINESIZE] = '\0';	
	strncpy(tmp->tty,entry->ut_line,UT_LINESIZE);
	strncpy(tmp->host,entry->ut_host,UT_HOSTSIZE);
	tmp->host[UT_HOSTSIZE] = '\0';
		
	tmp->pid = entry->ut_pid;

 	if((ppid = get_ppid(tmp->pid)) == -1)
		strncpy(tmp->parent, "can't access", sizeof tmp->parent);
	else 	strncpy(tmp->parent, get_name(ppid), sizeof tmp->parent - 1);
	
	tmp->line = how_many;
	return tmp;
}
	
void print_user(struct user *u)
{
	wattrset(users_list.descriptor, A_BOLD);
	snprintf(line_buf, buf_size, 
		"%-14.14s %-9.9s %-6.6s %-19.19s %s", 
		u->parent, 
		u->name, 
		u->tty, 
		u->host, 
		toggle?count_idle(u->tty):get_w(u->pid));
	line_buf[buf_size - 1] = '\0';
	print_line(&users_list, line_buf ,u->line, state);
	wrefresh(users_list.descriptor);
}

void cleanup()
{
	struct user *u = begin, *p;
	while(u){
		p = u->next;
		free(u->name);
		free(u->tty);
		free(u);
		u = p;
	}
	begin = NULL;
	clear_list();			/* clear processes list */
	close(wtmp_fd);
}

void windows_init()
{
	struct window *p;
	int i = 0;
	for(p = windows[i]; p; p = windows[i++]){
		p->first_line = p->last_line = p->cursor_line = 0;
		p->has_cursor = 1;
	}
}

void users_list_refresh()
{
	struct user *p;
	for(p = begin; p; p = p->next) print_user(p);
	wrefresh(users_list.descriptor);
}
	

void read_utmp()		/* when program starts */
{
	int fd, i;
	static struct utmp entry;
	struct user *tmp, **current = &begin;
	
	if ((fd = open(UTMP_FILE ,O_RDONLY)) == -1){
		curses_end();
		fprintf(stderr,"Cannot open " UTMP_FILE "\n");
		exit (1);
	}
	tmp = NULL;
	while((i = read(fd, &entry,sizeof entry)) > 0){
		if (i != sizeof entry){
			fprintf(stderr, "Error reading " UTMP_FILE "\n");
			exit(1);
		}
		if (entry.ut_type != 7) continue;
		tmp = allocate_user(&entry);
		print_user(tmp);
		update_nr_users(tmp->parent, &tmp->prot, LOGIN);		
		how_many++;
		*current = tmp;
		tmp->prev = current;
		current = &tmp->next;
	}
	*current = NULL;
	close(fd);
	wrefresh(users_list.descriptor);
	return;
}

struct user* new_user(struct utmp *newone)
{
	struct user *i = begin,*tmp;

	tmp = allocate_user(newone);
	how_many++;
	tmp->next = NULL;
	
/* Begin could be null (if utmp was empty at the program's start) */
	if (!begin){
		begin = tmp;
		tmp->prev = &begin;
	}
	else {
		while(i->next) i=i->next;
		i->next = tmp;	
		tmp->prev = &i->next;
	}
	update_nr_users(tmp->parent, &tmp->prot, LOGIN);
	return tmp;
}

struct user *cursor_user(int line)	/* get user from cursos position */
{
	struct user *u;
	for(u = begin; u; u = u->next)
		if (u->line == line) return u;
	return 0;
}


void check_wtmp()
{
	struct user *f, *tmp;
	struct utmp entry;
	int i;
	while ((i = read(wtmp_fd, &entry, sizeof entry)) > 0){ 
		if (i < sizeof entry){
			curses_end();
			cleanup();
			fprintf(stderr,"Error reading " WTMP_FILE "\n.");
			exit(1);
		}
/* user just logged in */
		if (entry.ut_type == 7){
			tmp = new_user(&entry);
			print_user(tmp);
			wrefresh(users_list.descriptor);
			print_info();
			continue;
		}
		if (entry.ut_type != 8) continue;
/* user just logged out */
		f = begin;
		while(f){
			if (strncmp(f->tty, entry.ut_line, UT_LINESIZE)) {
				f = f->next;
				continue;
			}
			if (state == USERS_LIST) 
				delete_line(&users_list, f->line);
			else virtual_delete_line(&users_list, f->line);
			
			update_line_nr(f->line);
			how_many--;
			update_nr_users(f->parent, &f->prot, LOGOUT);
			print_info();

			tmp = f->next;
			*(f->prev) = tmp;
			if (tmp) tmp->prev = f->prev;
			free(f->tty);
			free(f->name);
			free(f);
			break;
		}
	}
}

char *users_list_giveline(int line)
{
	struct user *u;
	for(u = begin; u; u = u->next){
		if (line == u->line){
			snprintf(line_buf, buf_size, 
				"%-14.14s %-9.9s %-6.6s %-19.19s %s", 
				u->parent, 
				u->name, 
				u->tty, 
				u->host, 
				toggle?count_idle(u->tty):get_w(u->pid));
			return line_buf;
		}
	}
	return 0;
}

void periodic()
{
	check_wtmp();		/* always check wtmp for logins and logouts */
	
	switch(state){
		case INIT_TREE:
			tree_periodic();
			tree_title(0);
			break;
		case PROC_TREE:
			tree_periodic();
			break;
		case USERS_LIST:
			show_cmd_or_idle();
			break;
	}
}

int read_key()
{
	int c;
	c = getc(stdin);
	switch (c){
		case 0xD:
		case 0xA: return ENTER;
		case 0xB: return CTRL_K;
		case 0x9: return CTRL_I;
		case 0x1B:
			getc(stdin);
			c = getc(stdin);
			if (c == 0x41) return UP;
			else if (c == 0x42) return DOWN;
			break;
		default:
			break;
	}
	return c;
}

void proc_tree_init()
{
	proc_win.first_line = proc_win.last_line = proc_win.cursor_line = 0;
}


void send_signal(int sig, pid_t pid)
{
	int p;
	char buf[64];
	p = kill(pid, sig);
	signal_sent = 1;
	if (p == -1)
		sprintf(buf,"Can't send signal %d to process %d",
			sig, pid); 
	else sprintf(buf,"Signal %d was sent to process %d",sig, pid);
	werase(help_win.descriptor);
	echo_line(&help_win, buf, 0);
	wrefresh(help_win.descriptor);
}

void main_init()
{
	if ((wtmp_fd = open(WTMP_FILE ,O_RDONLY)) == -1){
		fprintf(stderr,"Cannot open " WTMP_FILE "\n");
		exit (1);
	}
	if (lseek(wtmp_fd, 0, SEEK_END) == -1){
		perror("lseek");
		exit (1);
	}
}

void restart()
{
	how_many = ssh_users = telnet_users = local_users = 0;
	toggle = show_owner = 0;
	cleanup();

	windows_init();
	clear_tree_title();
	state = USERS_LIST;
	werase(users_list.descriptor);
	wrefresh(users_list.descriptor);
	read_utmp();
	main_init();
	
	print_help(state);
	print_info();
}

void key_action(int key)
{
	int pid;
	struct user *p;
	if (signal_sent) {
	    print_help(state);
	    signal_sent = 0;
	}
	switch(key){
		case ENTER:
			werase(windows[state]->descriptor);
			switch(state){
				case USERS_LIST:
					state = PROC_TREE;
					print_help(state);
					proc_tree_init();
					p = cursor_user(
						users_list.cursor_line +
						users_list.first_line
						);
					if (!p) tree_pid = 1;
					else tree_pid = p->pid; 
					tree_title(p);
					tree_periodic();
					break;
				case INIT_TREE:
				case PROC_TREE:
					state = USERS_LIST;
					print_help(state);
					clear_tree_title();
					clear_list();
					users_list_refresh();
					break;
			}
			break;
		case CTRL_I:
			if (state < PROC_TREE) break;
			pid = pid_from_tree(proc_win.cursor_line 
				+ proc_win.first_line);
			send_signal(2, pid);
			tree_periodic();
			break;
		case CTRL_K:
			if (state < PROC_TREE) break;
			pid = pid_from_tree(proc_win.cursor_line 
				+ proc_win.first_line);
			send_signal(9, pid);
			tree_periodic();
			break;
		case UP:
			cursor_up(windows[state]);
			wrefresh(windows[state]->descriptor);
			break;
		case DOWN:
			cursor_down(windows[state]);
			wrefresh(windows[state]->descriptor);
			break;
		case 'q': 
			curses_end(); 
			cleanup();	
			free(line_buf);
			exit(0);
		case 'i':
			if (state < INIT_TREE){
				proc_tree_init();
				clear_list();
				state = INIT_TREE;
				print_help(state);
				tree_pid = 1;
				tree_periodic();
				tree_title(0);
			}
			break;
		case 't': 
			if (state != USERS_LIST) break;
			toggle = toggle?0:1;
			show_cmd_or_idle();
			break;
		case 'o':
			if (state == USERS_LIST) break;
			show_owner = show_owner?0:1;
			tree_periodic();
/* ugly hack to force proper cursor display on older curses versions */
cursor_off(&proc_win, proc_win.cursor_line);
cursor_on(&proc_win, proc_win.cursor_line);
			break;
#ifdef DEBUG
		case 'd':
			dump_list();
			dump_users();
			break;
#endif
		case 'x':
			restart();
			break;
	}
}

void get_rows_cols(int *y, int *x)
{
	struct winsize win;
	if (ioctl(1,TIOCGWINSZ,&win) != -1){
                *y = win.ws_row;
		*x = win.ws_col;
		return;
	}
	fprintf(stderr,"ioctl error: cannot read screen size\n");
	exit(0);
}								

void resize()	/* not yet implemented */
{
//	get_rows_cols(&users_list.rows, &users_list.cols);
	users_list.cursor_line = 0;
	curses_init();
	maintree(1);
}

void winch_handler()
{
	screen_size_changed++;
}

void dump_users()
{
#ifdef DEBUG
	struct user *u = begin;
	fprintf(debug_file,"begin %p\n", begin);
	for (u = begin; u ; u = u->next)
		fprintf(debug_file,"%p, prev %p, next %p, %s, %s\n",
			u, 
			u->prev, 
			u->next, 
			u->name, 
			u->tty);
	fflush(debug_file);
#endif

}
void segv_handler()
{
	signal(SIGSEGV,SIG_IGN);
	curses_end();
	printf("\nProgram has received a segmentation fault signal.\n");
	printf("It means that there is an error in the code.\n");
	printf("Please send a bug report to mike@wizard.ae.krakow.pl\n");
	printf("Your support will help to eliminate bugs once and for all.\n\n");
	fflush(stdout);
#ifdef DEBUG
	dump_list();
	dump_users();
#endif
	exit(1);
}

void int_handler()
{
	curses_end();
	exit(0);
}		

int main()
{
	struct timeval tv;
	fd_set rfds;
	int retval;
	main_init();	
#ifdef DEBUG
	if (!(debug_file = fopen("debug", "w"))){
		printf("file debug open error\n");
		exit(0);
	}
#endif
	get_rows_cols(&screen_rows, &screen_cols);
	buf_size = screen_cols + screen_cols/2;
	line_buf = malloc(buf_size);
	if (!line_buf){
		fprintf(stderr,"Cannot allocate memory for buffer.\n");
		exit(0);
	}
	curses_init();
	windows_init();
	
	proc_win.giveme_line = proc_give_line;
	users_list.giveme_line = users_list_giveline;
	
        signal(SIGINT, int_handler);
/*	signal(SIGWINCH, winch_handler);   not yet implemented */
	signal(SIGSEGV, segv_handler);

	wrefresh(users_list.descriptor);
	read_utmp();
	print_help(state);
	print_info();
	
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	for(;;){				/* main loop */
		FD_ZERO(&rfds);
		FD_SET(0,&rfds);
		retval = select(1,&rfds,0,0,&tv);
		if (retval){
			int key = read_key();
			key_action(key);
		}
		if (!tv.tv_sec && !tv.tv_usec){
			periodic();
			tv.tv_sec = TIMEOUT;
		}
	}
}

void show_cmd_or_idle()
{
	struct user *p = begin;
	struct window *q = &users_list;
	while(p){
		if (p->line < q->first_line || p->line > q->last_line) {
			p = p->next;
			continue;
	        }
	        wmove(q->descriptor, p->line - q->first_line, CMD_COLUMN);
		wclrtoeol(q->descriptor);
	        waddnstr(q->descriptor, toggle?count_idle(p->tty):get_w(p->pid),
	            COLS - CMD_COLUMN - 1);
	        p = p->next;
	}
	cursor_on(q, q->cursor_line);
	wattrset(q->descriptor, A_BOLD);
	wrefresh(q->descriptor);
}
