#include "whowatch.h"

#define UTMP 	"/var/run/utmp"
#define WTMP 	"/var/log/wtmp"

/* uncomment this if you want to disable automatic refresh of processes info.
   (to safe CPU time)
   In this case it can be done manually by pressing 'w' key.
*/
//#define NOAUTO 

/* uncomment this if you don't want colors */
//#define NOCOLOR 


/* number of seconds before another attempt of reading wtmp*/
#define TIMEOUT	3



/*--------- end of user's configuration section -----------------------*/

#define CMD_COLUMN 52

FILE *debug_file;
WINDOW *mainw;

int toggle = 0;		/* if 0 show cmd line else show idle time */

int how_many, telnet_users, ssh_users, local_users;
struct user *begin ;

/*
 * Each user has its own line. 'lines' variable holds number which will
 * be assigned to the next user. 'fline' (first line) holds line number
 * which is currently at the top of the screen. 'lline' points to the last
 * visible line. 
 */
int fline, lline, lines;

int fdwtmp;		/* wtmp file descriptor */
int proctree;	 	/* if 0 we show process tree else users list */
int dont_scroll;	/* do not scroll up processes tree */
int cursor_line;

enum key { ENTER=0x100, UP, DOWN, LEFT, RIGHT, DELETE, ESC };

char clear_buf[32];

char * const ssh    = 	"(sshd";    /* we don't know version of sshd */
char * const local  =	"(init)";
char * const telnet = 	"(in.telnetd)";

int ssh_port = 22;
int telnet_port = 23;
int local_port = -1;
int tree_pid = 1;

void allocate_error(){
	fprintf(stderr,"Cannot allocate memory.\n");
	endwin();
	exit (-1);
}

/*  
 *  update number of users (ssh users, telnet users...) and change 
 *  prot in the appropriate user structure
 */
void update_nr_users(char *login_type, int *pprot)
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
			(*(int *) tab[i+2])++;
			break;
		}
	}
}

/* 
 * Create new user structure and fill it
 */
struct user *allocate_user(struct utmp *entry)
{
	struct user *tmp;
	
	if (!(tmp = malloc(sizeof (struct user)) ))
		allocate_error();

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
	tmp->line = lines;
	return tmp;
}
	

void read_utmp()		/* when program starts */
{
	int fd, ppid;
	static struct utmp entry;
	struct user *tmp, **current = &begin;
	char *count;
	
	if ((fd = open(UTMP,O_RDONLY)) == -1){
		fprintf(stderr,"Cannot open " UTMP "\n");
		exit (-1);
	}
	
	tmp = NULL;
	move(TOP,0);
	fline = lline = lines = 0;
	
	while(read(fd, &entry,sizeof entry) > 0){
		
		if (entry.ut_type != 7) continue;
		
		tmp = allocate_user(&entry);
		if ((ppid = get_ppid(tmp->pid)) == -1)
			count = "can't access";
		else
			count = get_name(ppid);
		
		update_nr_users(count,&tmp->prot);		
		if (in_scr(lines)){
			lline++;		
			print_user(tmp);
		}
		how_many++;
		lines++;
		
		*current = tmp;
		tmp->prev = current;
		current = &tmp->next;
#ifdef DEBUG
		fprintf(debug_file,"%-8s %-5s (%-12d <- %-12d)%d\n",
			tmp->name, tmp->tty,(int) *tmp->prev,(int) tmp,tmp->line);
#endif
	}
	*current = NULL;
#ifdef DEBUG
	fflush(debug_file);
#endif
	close(fd);
	return;
}



struct user* new_user(struct utmp *newone)
{
	struct user *i = begin,*tmp;
	char *count;
	int ppid;

	tmp = allocate_user(newone);
	lines++;
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
	
	if ((ppid = get_ppid(newone->ut_pid)) == -1)
		count = "can't access";
	else
		count = get_name(ppid);
	
	update_nr_users(count,&tmp->prot);

	how_many++;
	return tmp;
}

void cleanup()
{
	struct user *i = begin, *tmp;
	while (i){
		tmp = i->next;
		free(i->name);
		free(i->tty);
		free(i);
		i = tmp;
	}
	begin = NULL;
	close(fdwtmp);
}

void restart()
{
	cursor_line = how_many = ssh_users = telnet_users = local_users = 0;
	dont_scroll = 0;
	endwin();
	curses_init();
	read_utmp();
	if ((fdwtmp = open(WTMP,O_RDONLY)) == -1){
		fprintf(stderr,"Cannot open " WTMP "\n");
		exit (-1);
	}
	update_info();
	lseek(fdwtmp,0,SEEK_END);
}

int read_key()
{
	int c;
	c = getc(stdin);
	switch (c){
		case 0xD:
		case 0xA: return ENTER;
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

void key_action(int key)
{
	if (proctree && (key == 't' || key == 'r')) return ;
	switch(key){
		case ENTER:
			proctree = proctree?0:1;
			werase(mainw);
			wrefresh(mainw);
			if (proctree){
				tree_pid = pid_from_cursor();
				process_nr = 0;
				dont_scroll = 0;
				maintree(tree_pid);
			}
			else redraw();
			break;	
		case UP:
			if (proctree && process_nr){
				process_nr++;
				werase(mainw);
				maintree(tree_pid);
			}
			else if (!proctree)
				cursor_up();
			break;
		case DOWN:
			if (proctree && !dont_scroll){
				process_nr--;
				werase(mainw);
				maintree(tree_pid);
			}
			else if (!proctree)			
				cursor_down();
			break;
#ifdef NOAUTO
		case 't': toggle = toggle?0:1;
		case 'w': show_cmd_or_idle(toggle); 
	  		break;
#else
		case 't': 
			toggle = toggle?0:1;
			show_cmd_or_idle(toggle);
			break;
#endif
		case 'x':
			cleanup();
			restart();
			break;
		case 'r': 
			redraw();
		  	break;
		case 'q': 
			endprg(); exit(0);
		case 'i':
			if (!proctree) proctree = 1;
			werase(mainw);
			wrefresh(mainw);
			if (proctree){
				tree_pid = 1;
				process_nr = 0;
				maintree(tree_pid);
			}
			else redraw();
			break;	
		default:
			break;
	}
}

void bye()
{
	endprg();		/* restore terminal settings */
	exit(0);
}

void check_wtmp()
{
	
	struct user *f, *tmp;
	struct utmp entry;
	while (read(fdwtmp, &entry,sizeof entry)){ 

		/* type 7 is USER_PROCESS (user just log in)*/
		if (entry.ut_type == 7){
			tmp = new_user(&entry);
			if (in_scr(tmp->line)){
				lline++;
				if (!proctree) print_user(tmp);
			}
			update_info();
			continue;
		}
		if (entry.ut_type != 8) continue;
		
		/* type 8 is DEAD_PROCESS (user just logged out) */
		f = begin;
		while(f){
			if (strncmp(f->tty,entry.ut_line,UT_LINESIZE)) {
				f = f->next;
				continue;
			}
			delete_user(f);
			update_info();

			tmp = f->next;
			*(f->prev) = tmp;
			if (tmp) tmp->prev = f->prev;

			free(f->name);
			free(f->tty);
			free(f);
		}
	}
}

void main()
{
	struct timeval tv;
	fd_set rfds;
	int retval;
	
	curses_init();
	bzero(clear_buf, sizeof clear_buf);
	memset(clear_buf, ' ', sizeof clear_buf - 1);
#ifdef DEBUG
	if (!(debug_file = fopen("./wlogs","a"))){
		perror("fopen");
		exit (-1);
	}
#endif	
	read_utmp();
	update_info();
	update_tree();
	signal(SIGINT, bye);

	if ((fdwtmp = open(WTMP,O_RDONLY)) == -1){
		fprintf(stderr,"Cannot open " WTMP "\n");
		exit (-1);
	}
	
	if (lseek(fdwtmp,0,SEEK_END) == -1){
		perror("lseek");
		endwin();
		exit (0);
	}
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	for(;;){
		FD_ZERO(&rfds);
		FD_SET(0,&rfds);
		retval = select(1,&rfds,0,0,&tv);
		if (retval){
			int key = read_key();
			key_action(key);
		}
		if (!tv.tv_sec && !tv.tv_usec){
			check_wtmp();
			if (proctree){
				update_tree();
				werase(mainw);
				maintree(tree_pid);
				wrefresh(mainw);
			}
			else show_cmd_or_idle(toggle);
			tv.tv_sec = TIMEOUT;
		}
	}
}

