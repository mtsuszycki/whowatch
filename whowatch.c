#include <err.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "whowatch.h"
#include "config.h"

#ifndef UTMP_FILE
#define UTMP_FILE 	"/var/run/utmp"
#endif

#ifndef WTMP_FILE
#define WTMP_FILE 	"/var/log/wtmp"
#endif

#define TIMEOUT 	3
#define LOGIN		0
#define LOGOUT		1		

#ifdef HAVE_UT_NAME
#define ut_user ut_name
#endif

enum key {  ENTER=0x100, UP, DOWN, LEFT, RIGHT, DELETE, ESC, CTRL_K, CTRL_I,
	   PG_DOWN, PG_UP, HOME, END };
	   
enum State{ USERS_LIST, PROC_TREE, INIT_TREE } state;

struct window users_list, proc_win;
struct window *windows[] = { &users_list, &proc_win, &proc_win, 0 };
void (*rfrsh[])() = { users_list_refresh, refresh_tree, refresh_tree };

int screen_size_changed;	/* not yet implemented */

struct list_head users = { &users, &users };

#ifdef DEBUG
FILE *debug_file;
#endif

int toggle;		/* if 0 show cmd line else show idle time 	*/
int full_cmd = 1;	/* if 1 then show full cmd line in tree		*/
int signal_sent;

int how_many, telnet_users, ssh_users, local_users;

int wtmp_fd;		/* wtmp file descriptor 			*/
int screen_rows;	/* screen rows returned by ioctl		*/
int screen_cols;	/* screen cols returned by ioctl		*/

char *line_buf;		/* global buffer for line printing		*/
int buf_size;		/* allocated buffer size			*/

#ifdef HAVE_PROCESS_SYSCTL
char * const ssh    = 	"sshd";   
char * const local  =	"init";
char * const telnet = 	"telnetd";
#else
char * const ssh    = 	"(sshd";    /* we don't know version of sshd 	*/
char * const local  =	"(init)";
char * const telnet = 	"(in.telnetd)";
#endif

int ssh_port = 22;
int telnet_port = 23;
int local_port = -1;

int tree_pid = 1;	
int show_owner;		/* if 1 display processes owners		*/

void show_cmd_or_idle();
void dump_users();
void cleanup();

#ifdef HAVE_PROCESS_SYSCTL
int get_login_pid(char *);
#endif

#ifdef HAVE_LIBKVM
int kvm_init();
int can_use_kvm = 0;
#endif

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
	struct user_t *u;
	for_each(u, users) 
		if(u->line > line) u->line--;
}
			
/* 
 * Create new user structure and fill it
 */
struct user_t *allocate_user(struct utmp *entry)
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
	
	u->line = how_many;
	return u;
}
	
void print_user(struct user_t *u)
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
	struct user_t *u;
	dellist(u, users);
	/* clear list of processes */
	clear_list();			
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
	struct user_t *u;
	for_each(u, users) print_user(u);
	/* hide cursor */
	wmove(users_list.descriptor, users_list.cursor_line, users_list.cols+1);
	wrefresh(users_list.descriptor);
}
	
/*
 * Gather informations about users currently on the machine
 * Needed only at start or restart
 */
void read_utmp()		
{
	int fd, i;
	static struct utmp entry;
	struct user_t *u;
	
	if ((fd = open(UTMP_FILE ,O_RDONLY)) == -1){
		curses_end();
		errx(1, "Cannot open " UTMP_FILE);
	}
	while((i = read(fd, &entry,sizeof entry)) > 0) {
		if(i != sizeof entry) errx(1, "Error reading " UTMP_FILE );
#ifdef HAVE_USER_PROCESS
		if(entry.ut_type != USER_PROCESS) continue;
#else
		if(!entry.ut_name[0]) continue;
#endif
		u = allocate_user(&entry);
		print_user(u);
		update_nr_users(u->parent, &u->prot, LOGIN);
		how_many ++;
		users_list.d_lines = how_many;		
		addto_list(u, users);
	}
	close(fd);
	wrefresh(users_list.descriptor);
	return;
}

struct user_t* new_user(struct utmp *newone)
{
	struct user_t *u;
	u = allocate_user(newone);
	users_list.d_lines = how_many;
	how_many++;
	addto_list(u, users);
	update_nr_users(u->parent, &u->prot, LOGIN);
	return u;
}

/*
 * get user entry from specific line (cursor position)
 */
struct user_t *cursor_user(int line)	
{
	struct user_t *u;
	for_each(u, users) if(u->line == line) return u;
	return 0;
}

/*
 * Check wtmp for logouts or new logins
 */
void check_wtmp()
{
	struct user_t *u;
	struct utmp entry;
	int i;

	while((i = read(wtmp_fd, &entry, sizeof entry)) > 0){ 
		if (i < sizeof entry){
			curses_end();
			cleanup();
			errx(1, "Error reading " WTMP_FILE );
		}
		/* user just logged in */
#ifdef HAVE_USER_PROCESS
		if(entry.ut_type == USER_PROCESS) {
#else
		if(entry.ut_user[0]) {
#endif
			u = new_user(&entry);
			print_user(u);
			wrefresh(users_list.descriptor);
			print_info();
			continue;
		}
#ifdef HAVE_DEAD_PROCESS
		if(entry.ut_type != DEAD_PROCESS) continue;
#else
//		if(entry.ut_line[0]) continue;
#endif
	/* user just logged out */
		for_each(u, users) {
			if(strncmp(u->tty, entry.ut_line, UT_LINESIZE)) 
				continue;
			if (state == USERS_LIST) 
				delete_line(&users_list, u->line);
			else virtual_delete_line(&users_list, u->line);
			update_line_nr(u->line);
			how_many--;
			users_list.d_lines = how_many;
			update_nr_users(u->parent, &u->prot, LOGOUT);
			print_info();
			delfrom_list(u, users);
			break;
		}
	}
}

char *users_list_giveline(int line)
{
	struct user_t *u;
	for_each(u, users) {
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
	/* always check wtmp for logins and logouts */
	check_wtmp();		
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
			switch(c) {
				case 0x41: return UP;
				case 0x42: return DOWN;
				case 0x34:
				case 0x38:
				case 0x46: return END;
				case 0x36:
				case 0x47: return PG_DOWN;
				case 0x31:
				case 0x37:
				case 0x48: return HOME;
				case 0x35:
				case 0x49: return PG_UP;
			}
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
	if((wtmp_fd = open(WTMP_FILE ,O_RDONLY)) == -1) 
		errx(1, "Cannot open " WTMP_FILE);
	if(lseek(wtmp_fd, 0, SEEK_END) == -1) 
		errx(1, "Cannot seek in " WTMP_FILE);
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
	struct user_t *p;
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
	case PG_DOWN:
		page_down(windows[state], rfrsh[state]);
		break;
	case PG_UP:
		page_up(windows[state], rfrsh[state]);
		break;
	case HOME:
		key_home(windows[state], rfrsh[state]);
		break;
	case END:
		key_end(windows[state], rfrsh[state]);
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
	case 't':
		if (state < INIT_TREE){
			werase(windows[state]->descriptor);
			proc_tree_init();
			clear_list();
			state = INIT_TREE;
			print_help(state);
			tree_pid = 0; /* all processes */
			tree_periodic();
			tree_title(0);
		}
		break;
	case 'i': 
		if (state != USERS_LIST) break;
		toggle = toggle?0:1;
		show_cmd_or_idle();
		break;
	case 'o':
		if (state == USERS_LIST) break;
		show_owner ^= 1;
		refresh_tree();
		wrefresh(proc_win.descriptor);
/* ugly hack to force proper cursor display on older curses versions */
//cursor_off(&proc_win, proc_win.cursor_line);
//cursor_on(&proc_win, proc_win.cursor_line);
		break;
	case 'c':
		full_cmd ^= 1;
		(*rfrsh[state])(); //refresh_tree();
		wrefresh(windows[state]->descriptor);
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
	struct user_t *u;
	fprintf(debug_file, "%p %p\n", users.next, users.prev);
	for_each(u, users) 
		fprintf(debug_file,"%p, prev %p, next %p, %s, %s\n",
			u, 
			u->prev, 
			u->next, 
			u->name, 
			u->tty
			);
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
#ifndef RETURN_TV_IN_SELECT
  	struct timeval before;
  	struct timeval after;
#endif
	fd_set rfds;
	int retval;
	main_init();
#ifdef HAVE_LIBKVM
	if (kvm_init()) can_use_kvm = 1;
#endif
	
#ifdef DEBUG
	if (!(debug_file = fopen("debug", "w"))){
		printf("file debug open error\n");
		exit(0);
	}
#endif
	get_rows_cols(&screen_rows, &screen_cols);
	buf_size = screen_cols + screen_cols/2;
	line_buf = malloc(buf_size);
	if (!line_buf)
		errx(1, "Cannot allocate memory for buffer.");

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
	for(;;) {				/* main loop */
		FD_ZERO(&rfds);
		FD_SET(0,&rfds);
#ifdef RETURN_TV_IN_SELECT
		retval = select(1,&rfds,0,0,&tv);
		if(retval) {
			int key = read_key();
			key_action(key);
		}
		if (!tv.tv_sec && !tv.tv_usec){
			periodic();
			tv.tv_sec = TIMEOUT;
		}
#else
		gettimeofday(&before, 0);
		retval = select(1, &rfds, 0, 0, &tv);
		gettimeofday(&after, 0);
		tv.tv_sec -= (after.tv_sec - before.tv_sec);
		if(retval) {
			int key = read_key();
			key_action(key);
		}
		if(tv.tv_sec <= 0) {
			periodic();
			tv.tv_sec = TIMEOUT;
		}
#endif
	}
}

void show_cmd_or_idle()
{
	struct window *q = &users_list;
	struct user_t *u;
	int y, x;
	for_each(u, users) {
		if (u->line < q->first_line || 
			u->line > q->first_line + q->rows - 1) 
			continue;
	        wmove(q->descriptor, u->line - q->first_line, CMD_COLUMN);
		cursor_off(q, q->cursor_line);
	        wattrset(q->descriptor, A_BOLD);
		wmove(q->descriptor, u->line - q->first_line, CMD_COLUMN);
	        waddnstr(q->descriptor, toggle?count_idle(u->tty):get_w(u->pid),
	        	 COLS - CMD_COLUMN - 1);
	        getyx(q->descriptor, y, x);
		while(x++ < q->cols + 1)
			waddch(q->descriptor, ' ');
		cursor_on(q, q->cursor_line);
		wmove(q->descriptor, q->cursor_line, q->cols + 1);
	}
	wrefresh(q->descriptor);
}
