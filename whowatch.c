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
int proctree = 0; 	/* if 0 we show process tree else users list */

int cursor_line = 0;

enum key { ENTER=0x100, UP, DOWN, LEFT, RIGHT, DELETE, ESC };

char clear_buf[32];

char * const ssh    = 	"(sshd";    /* we don't know version of sshd */
char * const local  =	"(init)";
char * const telnet = 	"(in.telnetd)";

int ssh_port = 22;
int telnet_port = 23;
int local_port = -1;


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

// for debugging only
void show_u()
{
 	struct user *f = begin ;
        fprintf(debug_file,"SIGSEGV caught\n");
        fprintf(debug_file,"begin = %p\n",begin);
        while (f){
        	fprintf(debug_file,"adress %p\tprev %-11p\tnext %-10p\t(%s,%s) %d\n",
                        f,*f->prev,f->next,f->name,f->tty,f->line);
                f = f->next;
        }
       	fflush(debug_file);
        endwin();
        fprintf(stderr,"SIGSEGV caught\n");
        kill(getpid(),6);
        exit (0);
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
	struct timeval tv;
	fd_set rfds;
	int retval;
	int c = 0;

	FD_ZERO(&rfds);
	FD_SET(0,&rfds);
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	retval = select(1,&rfds,0,0,&tv);
		if (retval){
			c = getc(stdin);
			switch (c){
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
				int pid = pid_from_cursor();
				maintree(pid);
			}
			else redraw();
			break;	
		case UP:
			cursor_up();
			break;	
		case DOWN:
			cursor_down();
			break;
#ifdef NOAUTO
		case 't': toggle = toggle?0:1;
		case 'w': show_cmd_or_idle(toggle); 
	  		break;
#else
		case 't': toggle = toggle?0:1; break;
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
		default:
			break;
	}
}

void main()
{
	struct user *f, *tmp;
	struct utmp entry;

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
#ifdef DEBUG
	signal(SIGINT, show_u);
	signal(SIGSEGV, show_u);
	fprintf(debug_file,"Started. begin = %p\n",begin);
#endif
	if ((fdwtmp = open(WTMP,O_RDONLY)) == -1){
		fprintf(stderr,"Cannot open " WTMP "\n");
		exit (-1);
	}
	
	if (lseek(fdwtmp,0,SEEK_END) == -1){
		perror("lseek");
		endwin();
		exit (0);
	}
	for(;;){
		while(!read(fdwtmp, &entry,sizeof entry)) {
			int key = read_key();
			key_action(key);
#ifndef NOAUTO
			if (!proctree)
				show_cmd_or_idle(toggle);
			
			else{
				int pid = pid_from_cursor();
				werase(mainw);
				maintree(pid);
				wrefresh(mainw);
			}
#endif
		}
		/* type 7 is USER_PROCESS (user just log in)*/
		if (entry.ut_type == 7){
			tmp = new_user(&entry);
			if (in_scr(tmp->line)){
				lline++;
				if (!proctree) print_user(tmp);
			}
			update_info();
#ifdef DEBUG
			fprintf(debug_file,"IN: %-9s%-5s, %-8s %-5s (%-12p<- %-12p) %d\n",
				entry.ut_user,entry.ut_line,tmp->name,tmp->tty,*tmp->prev,tmp,tmp->line);
			fflush(debug_file);
#endif
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
#ifdef DEBUG
			fprintf(debug_file,"OUT %-9s%-5s, %-9s %-5s (%-12p<- %-12p)%d\n",
				entry.ut_user,entry.ut_line,f->name,f->tty,*f->prev,f,f->line);
			fflush(debug_file);
#endif			
			delete_user(f);
			update_info();

			tmp = f->next;
			*(f->prev) = tmp;
			if (tmp) tmp->prev = f->prev;

			free(f->name);
			free(f->tty);
			free(f);
			break;
		}

	}
}

