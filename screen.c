#include "whowatch.h"


int color;
void cursor_up();
char *infos[]={ 
	"enter - processes, t - command/idle, i - init tree, x - restart, q - quit",
	"enter - users list, x - restart, q - quit"};
	

void endprg()
{
	erase();
	werase(mainw);
	wrefresh(mainw);
	refresh();
	endwin();
	printf("\033[?25h");		/* enable cursor */
}

void help_line(int which)
{
        if (color) attrset(COLOR_PAIR(1));
	move(BOTTOM, 0);
	clrtoeol();
	addnstr(infos[which], COLS);
	refresh();
}

/*
 * print info about cursor position (in the upper right corner)
 */
void lst()
{	
	int y,x;
	getyx(mainw,y,x);
	attrset(A_NORMAL);
	mvprintw(0,COLS-7,"%d user ",cursor_line + fline + 1);
	refresh();
	wmove(mainw,y,x);
}

void curses_init()
{
        initscr();
        mainw = newwin(MAINW_LINES,COLS,TOP,0);
        printf("\033[?25l");			/* disable cursor */
	start_color();
        if (COLS < 80 || LINES < 10){
                 fprintf(stderr,"Terminal too small. I expect min. 80 columns and 10 lines.\n");  
		 sleep(2);
		 endprg();
                 exit(0);
        }
	if ((has_colors() == TRUE && can_change_color() == TRUE) ||
		force_color){
 	       	color = 1;
	        init_pair(1,COLOR_CYAN,COLOR_BLACK);/*  header info */
        	init_pair(2,COLOR_RED,COLOR_BLACK); /*  running process */
		init_pair(3,COLOR_MAGENTA,COLOR_BLACK);/* zombie */
		init_pair(4,COLOR_GREEN,COLOR_BLACK);/* tree */
		init_pair(5,COLOR_YELLOW,COLOR_BLACK);/* stopped proc (D) */
		

        }
#ifdef NOCOLORS
        color = 0;
#endif
        cbreak();
        nodelay(stdscr,TRUE);
	scrollok(mainw,TRUE);
        noecho();
	help_line(MAIN_INFO);
/*
	move(TOP-1,0);

	hline('.' | A_BOLD,COLS);
	if (BOTTOM < LINES){
		move(BOTTOM,0);
		hline('.' | A_BOLD,COLS);
	}
*/
}

void update_line_nr(int line)
{
        struct user *tmp;
        
	for (tmp = begin; tmp; tmp = tmp->next)
		if (tmp->line > line) tmp->line--;

}

/* 
 * check if line is on the visible screen 
 */		
int in_scr(int line)
{
	return !(line < fline || line - fline > MAINW_LINES - 1);
}

void update_info()
{
        char buf[128];
        if (color) attrset(COLOR_PAIR(1));
        sprintf(buf,"%d users: (%d local , %d telnet , %d ssh , %d other)  ",
        	how_many, local_users, telnet_users, ssh_users, 
		how_many - telnet_users - ssh_users - local_users);
	
	move(0,0);
        addstr(buf);
	
        attrset(A_BOLD);
        refresh();
}

void print_user(struct user *pentry)
{
	char buff[256];
	int ppid;
	char *count;
	lst();
	if (!in_scr(pentry->line)) return;
	if ((ppid = get_ppid(pentry->pid)) == -1)
		count = "can't access";
	else 
		count = get_name(ppid);
	
	snprintf(buff, sizeof buff, "%-14.14s %-9.9s %-6.6s %-19.19s %s",count,pentry->name,
			pentry->tty, pentry->host,
			toggle?count_idle(pentry->tty):get_w(pentry->pid));
	
	wattrset(mainw,A_BOLD);
	wmove(mainw,pentry->line - fline,0);	
	waddnstr(mainw,buff,COLS);
	if (pentry->line - fline == cursor_line){
		mvwchgat(mainw, pentry->line  - fline,0,-1,A_REVERSE,0,NULL);
		touchline(mainw,cursor_line,1);
	}	
	wrefresh(mainw);
}

void delete_user(struct user *p)
{	
	struct user *tmp;
	
	how_many--;
	switch(p->prot){
		case 23: telnet_users--; break;
		case 22: ssh_users--; break;
		case -1: local_users--; break;
	}
	update_line_nr(p->line); 

//	if (lines - fline <= MAINW_LINES) lline--;
	if (lline + fline >= lines) lline--;
	lines--;	
/* remove user which is above of our visible screen */
	if (p->line < fline) {		
		fline--;
		p->line = -1;		
		if(cursor_line){
			screen_up();
			cursor_up();
		}
		lst();
		return;
	}
	else if (!in_scr(p->line)) return;
	
	wmove(mainw,p->line - fline,0);
	wdeleteln(mainw);
	if (p->line - fline <= cursor_line) 
		cursor_up();
	wrefresh(mainw);
	
	p->line = -1;
	for(tmp=begin; tmp; tmp=tmp->next)	
		if (tmp->line - fline >= MAINW_LINES - 1){
			print_user(tmp);
			break;
		}
	return;
}

void show_cmd_or_idle(int what)	/* if what=0 show command line */
{
        struct user *tmp = begin;

	while(tmp){
                if (!in_scr(tmp->line)) {
                        tmp = tmp->next;
                        continue;
                }
        	wmove(mainw,tmp->line - fline,CMD_COLUMN);
	
		if (tmp->line - fline == cursor_line) wattrset(mainw,A_REVERSE);
		else wattrset(mainw,A_BOLD);
		
		mvwaddnstr(mainw, tmp->line - fline, CMD_COLUMN, clear_buf,
			COLS - CMD_COLUMN);
        	wmove(mainw, tmp->line - fline, CMD_COLUMN);	
	        waddnstr(mainw, what?count_idle(tmp->tty):get_w(tmp->pid),
			COLS - CMD_COLUMN);
		
	        tmp = tmp->next;
        }
        wrefresh(mainw);
}
																							
void redraw()
{
	struct user *tmp;
	for (tmp=begin; tmp; tmp=tmp->next)
		print_user(tmp);
}
																				
void screen_down()
{
	struct user *tmp = begin;
	if (!fline) return;
	fline--;
	
	wscrl(mainw,-1);
	while(tmp){
		if (tmp->line == fline){ 
			if (lline < MAINW_LINES) lline++;
			print_user(tmp);
			break;
		}
		tmp = tmp->next;
	}
}

void screen_up()
{	
	struct user *tmp;
	fline++;
	lline--;
	wscrl(mainw,1);
	wrefresh(mainw);
	
	for(tmp=begin; tmp; tmp=tmp->next)
		if (tmp->line - fline >= MAINW_LINES - 1 ){ 
			lline++;
			print_user(tmp);
			break;
		}
}

void cursor_up()
{
	if (!cursor_line) screen_down();
	else cursor_line--;
	
	lst();
	if (!proctree){ 
		mvwchgat(mainw,cursor_line+1,0,-1,A_BOLD,0,NULL);
		mvwchgat(mainw,cursor_line,0,-1,A_REVERSE,0,NULL);
 		touchline(mainw,cursor_line,2);
		wnoutrefresh(mainw);
		doupdate();
	}
}

void cursor_down()
{
	if (cursor_line >= lline - 1){    	/* cursor is at the bottom */
		if (fline + cursor_line + 1 >= lines ) 
			return; 		/*no more users*/
		screen_up();
	}
	else cursor_line++;
	lst();
	if (!proctree){
		mvwchgat(mainw, cursor_line-1 ,0 , -1, A_BOLD, 0, NULL);
		mvwchgat(mainw ,cursor_line, 0, -1, A_REVERSE,0,NULL);
		touchline(mainw ,cursor_line-1 , 2);
		wnoutrefresh(mainw);
		doupdate();
	}
}

int pid_from_cursor()
{
	struct user *u;
	for (u=begin; u; u=u->next)
		if (u->line - fline == cursor_line)
			return u->pid;
	return 1;	
}
								

struct user *get_user_by_line(int line)
{
	struct user *u;
	for (u=begin; u; u=u->next)
		if (u->line - fline == line)
			return u;
	return 0;
}

