#include <err.h>
#include "config.h"
#include "whowatch.h"

struct window help_win;
struct window info_win;
static chtype *curs_buf;
extern int screen_cols;

char *help_line[] = 
	{
	"\001enter - proc tree, t - init tree, i - idle/cmd, c - cmd, x - refresh, q - quit",
	"\001enter - users, c - cmd, t - init, o - owner, ^I - send INT, ^K - send KILL",
	"\001enter - users list, c - cmd, o - owner, ^I - send INT, ^K - send KILL",
	};

void curses_init()
{
	curs_buf = calloc(screen_cols, sizeof(chtype));
	if(!curs_buf) errx(1, "Cannot allocate memory\n");
	initscr();
	users_list.rows = screen_rows - 3;
//	users_list.cols = screen_cols - 1;	
	users_list.cols = COLS - 2; 	
	proc_win.rows = users_list.rows;
	info_win.cols = help_win.cols = proc_win.cols = users_list.cols;
	
	users_list.descriptor = newwin(users_list.rows , COLS, 2 ,0);
	proc_win.descriptor = users_list.descriptor;

	help_win.descriptor = newwin(1, COLS, users_list.rows + 2, 0);
	info_win.descriptor = newwin(2, COLS, 0, 0);
	if (!info_win.descriptor || !help_win.descriptor || 
		!users_list.descriptor || !proc_win.descriptor){
		endwin();
		fprintf(stderr, "Ncurses library cannot create window\n");
		exit(0);
	}

	wattrset(users_list.descriptor, A_BOLD);
        printf("\033[?25l");                    /* disable cursor */

        start_color();
	init_pair(1,COLOR_CYAN,COLOR_BLACK);
        init_pair(2,COLOR_GREEN,COLOR_BLACK);
        init_pair(3,COLOR_WHITE,COLOR_BLACK);
	init_pair(4,COLOR_MAGENTA,COLOR_BLACK);
	init_pair(5,COLOR_RED,COLOR_BLACK);
	init_pair(6,COLOR_YELLOW,COLOR_BLACK);
	init_pair(7,COLOR_BLUE,COLOR_BLACK);
 	wattrset(proc_win.descriptor, COLOR_PAIR(3));       
	wattrset(users_list.descriptor, COLOR_PAIR(3));       
	wattrset(help_win.descriptor, COLOR_PAIR(3));       
	wattrset(info_win.descriptor, COLOR_PAIR(3));
	wbkgd(users_list.descriptor, COLOR_PAIR(3)); 
	wbkgd(help_win.descriptor, COLOR_PAIR(3)); 
	wbkgd(info_win.descriptor, COLOR_PAIR(3)); 
	
	cbreak();
        nodelay(stdscr,TRUE);
        scrollok(users_list.descriptor,TRUE);
        noecho();
}				

void curses_end()
{
	werase(help_win.descriptor);
	wrefresh(help_win.descriptor);
	endwin();
        printf("\033[?25h");            /* enable cursor */
}

void cursor_on(struct window *w, int line)
{
	chtype c;
	int i;
	wattrset(w->descriptor, A_REVERSE);
	for(i = 0; i <= w->cols; i++) { 
		c = mvwinch(w->descriptor, line, i);
		curs_buf[i] = c;
		waddch(w->descriptor, c & A_CHARTEXT);
	}
	curs_buf[i] = 0;
	wattrset(w->descriptor, A_BOLD);
}

void cursor_off(struct window *w, int line)
{
	int i;
	wattrset(w->descriptor, A_NORMAL);
	wmove(w->descriptor, line, 0);
	for(i = 0; curs_buf[i]; i++) 
		waddch(w->descriptor, curs_buf[i]);

	wattrset(w->descriptor, A_BOLD);
}

void move_cursor(struct window *w, int from, int to)
{
	cursor_off(w, from);
	cursor_on(w, to);
	wrefresh(w->descriptor);
}

/*
 * parse string and print line with colors
 */
int echo_line(struct window *w, char *s, int line)
{
	char *p = s, *q = s;
	int i = 0;
	if (!p) return 1;
	wmove(w->descriptor, line, 0);
//waddstr(w->descriptor, s);
//return;
	wclrtoeol(w->descriptor);
	while(*p){
		if (i > w->cols) break;
		if (*p < 17){
			i--;
			waddnstr(w->descriptor, q, p - q);
			wattrset(w->descriptor, COLOR_PAIR(*p));
			q = p + 1;
		}
		p++;
		i++;
	}
	waddnstr(w->descriptor, q, p - q);
	return 0;
}	

void print_help(int state)
{
	echo_line(&help_win, help_line[state], 0);
	wrefresh(help_win.descriptor);
}

void print_info()
{
        char buf[128];
        sprintf(buf,"\x1%d users: (%d local , %d telnet , %d ssh , %d other)  ",
                how_many, local_users, telnet_users, ssh_users,
        how_many - telnet_users - ssh_users - local_users);
	echo_line(&info_win, buf, 0);
	wrefresh(info_win.descriptor);					
}										
											
/*
 * If virtual is not 0 then update window parameters but don't display
 */	
int print_line(struct window *w, char *s, int line, int virtual)
{
	/* line is below screen */
	if (real_line_nr(line, w) >=  w->rows) return 0;
	if (!virtual) echo_line(w, s, real_line_nr(line, w));
	
	/* printed line is at the cursor position */
	if (real_line_nr(line, w) == w->cursor_line && !virtual)
		cursor_on(w, w->cursor_line);

	if (real_line_nr(line, w) > w->last_line) w->last_line++;
	return 1;
}

///// clean it!! (giveme_line)
void delete_line(struct window *w, int line)
{
	char *p = 0;
	/* line is below visible screen */
	if (real_line_nr(line, w) > w->last_line) return;
	
	if (line < w->first_line){
		w->first_line--;
		return;
	}
	wmove(w->descriptor, real_line_nr(line, w), 0);
	wdeleteln(w->descriptor);
	
	/* if there is a line below visible screen display it */
	if (w->last_line){
		if ((p = w->giveme_line(w->last_line + w->first_line + 1)))
			echo_line(w, p, w->last_line);
		else w->last_line--;
	}
			
	if (real_line_nr(line, w) < w->cursor_line){
		w->cursor_line--;
		wrefresh(w->descriptor);
		return;
	}
	if (real_line_nr(line, w) == w->cursor_line){
		cursor_on(w, w->cursor_line);
		p = w->giveme_line(line + 1);
		if (!p && (p = w->giveme_line(line - 1))){
			if(w->cursor_line){
				cursor_off(w, w->cursor_line);
				w->cursor_line--;
			}
			else{
				w->first_line--;
				echo_line(w, p, w->cursor_line);
			}
			cursor_on(w, w->cursor_line);
		}
	}
	wrefresh(w->descriptor);
}

/*
 * Update only window parameters - don't delete a line
 */
void virtual_delete_line(struct window *w, int line)
{
	char *p = 0;
	/* line is below visible screen */
	if (real_line_nr(line, w) > w->last_line) return;
	
	/* line is above visible screen */
	if (line < w->first_line){
		w->first_line--;
		return;
	}
	
	if (w->last_line)
		if (!(p = w->giveme_line(w->last_line + w->first_line + 1)))
			w->last_line--;
			
	if (real_line_nr(line, w) < w->cursor_line){
		w->cursor_line--;
		return;
	}
	if (real_line_nr(line, w) == w->cursor_line){
		p = w->giveme_line(line + 1);
		if (!p && (p = w->giveme_line(line - 1))){
			if(w->cursor_line) w->cursor_line--;
			else w->first_line--;
		}
	}
}

void cursor_down(struct window *w)
{
	char *buf;
	if (!w->has_cursor) return;
	if (w->cursor_line < w->rows - 1){
		if (w->cursor_line == w->last_line) return;
		move_cursor(w, w->cursor_line, w->cursor_line + 1);
		w->cursor_line++;
	}
/* cursor is at the bottom, check for lines, scroll screen, and print */
	else if ((buf = w->giveme_line(w->first_line + w->rows))){
		wscrl(w->descriptor, 1);
		echo_line(w, buf, w->cursor_line);
		w->first_line++; 
		move_cursor(w,w->cursor_line-1, w->cursor_line);
//		wrefresh(w->descriptor);
	}
}
void cursor_up(struct window *w)
{
	char *buf;
	if (w->cursor_line){
		move_cursor(w, w->cursor_line, w->cursor_line-1);
		w->cursor_line--;
	}
	/* cursor is at the top and there are more lines to show */
	else if (!w->cursor_line && w->first_line){
		buf = w->giveme_line(w->first_line - 1);
		if (!buf) return;
		wscrl(w->descriptor, -1);
		w->first_line--;
		echo_line(w, buf, w->cursor_line);
		move_cursor(w, w->cursor_line+1, w->cursor_line);
//		wrefresh(w->descriptor);
		if (w->last_line < w->rows - 1) w->last_line++;
	}
	return;
}

void page_down(struct window *w, void (*refresh)())
{
	int z;
	int i = w->d_lines - w->first_line - w->rows;
	
	/* no lines below visible screen */
	if(i <= 0 && w->cursor_line == w->last_line)
		return;
	if(i <= 0) { 
		move_cursor(w, w->cursor_line, w->last_line);
		w->cursor_line = w->last_line;
		return;
	}
	if(i >= w->rows) z = w->rows;
	else z = i;
	w->first_line += z;
	(*refresh)();
	wrefresh(w->descriptor);
}

void page_up(struct window *w, void (*refresh)())
{
	int z;
	int i = w->first_line;
	
	/* no lines above visible screen */
	if(!i && !w->cursor_line) return;
	if(!i) {
		move_cursor(w, w->cursor_line, w->first_line);
		w->cursor_line = w->first_line;
		return;
	}
	if(i >= w->rows) z = w->rows;
	else z = i;
	w->first_line -= z;
	(*refresh)();
	wrefresh(w->descriptor);
}

void key_home(struct window *w, void (*refresh)())
{
	int i = w->first_line;
	
	if(!i && !w->cursor_line) return; 
	if(!i) {
		move_cursor(w, w->cursor_line, w->first_line);
		w->cursor_line = w->first_line;
		return;
	}
	w->first_line = 0;
	(*refresh)();
	if(!w->cursor_line) return;
	else {
		move_cursor(w, w->cursor_line, w->first_line);
		w->cursor_line = w->first_line;
	}
}

void key_end(struct window *w, void (*refresh)())
{
	int i = w->d_lines - w->first_line - w->rows;
	if(i <= 0 && w->cursor_line == w->last_line)
		return;
	if(i <= 0) { 
		move_cursor(w, w->cursor_line, w->last_line);
		w->cursor_line = w->last_line;
		return;
	}
	w->first_line += i;
	(*refresh)();
	if(w->cursor_line == w->last_line) return;
	else {
		move_cursor(w, w->cursor_line, w->last_line);
		w->cursor_line = w->last_line;
	}
}
