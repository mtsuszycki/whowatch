/*
 * Creates simple sub window that displays some message.
 * There is only one button available ("Cancel"). Default
 * action for any key is to destroy the sub window.
 */
 
#include "whowatch.h"
#include "subwin.h"

static struct pad_t _box;	/* ncurses pad for info box	*/
static char *info;		/* string to be displayed	*/
static char *title;		/* box title			*/

/*
 * It is a real mess here, because variable names
 * (size, cols, rows) are confusing. Needs to be changed. 
 */
static void set_size(void)
{
	_box.size_x = screen_cols/5;
	_box.size_y = screen_rows/3;
	_box.cols = screen_cols - 2*_box.size_x;
	_box.rows = screen_rows - 2*_box.size_y;
}

void info_refresh(void)
{
	if(!_box.wd) return;
	pnoutrefresh(_box.wd, 0, 0, _box.size_y, _box.size_x, 
		_box.rows+2*_box.size_y, _box.cols+2*_box.size_x);
}

static void mesg_print(void)
{
	int len = strlen(info);
	char *s;
	short row = -2;
	mvwaddstr(_box.wd, 0, _box.cols/2-strlen(title)/2, title);

	for(s = info; s < info + len - 1;) {
		mvwaddnstr(_box.wd, _box.rows/2 + row++, 2, s, _box.cols - 4);
		s += _box.cols - 4;
	}
	wattrset(_box.wd, A_REVERSE);
	mvwaddstr(_box.wd, _box.rows/2 + row + 2, _box.cols/2-5, "[ Cancel ]");
	wattrset(_box.wd, A_NORMAL);
}

/* 
 * Needed for SIGWINCH handling. Called only from whowatch.c
 */
void info_resize(void)
{
	if(!_box.wd) return;
	set_size();
	wresize(_box.wd, _box.rows, _box.cols);
	werase(_box.wd);
	box(_box.wd, ACS_VLINE, ACS_HLINE);
	mesg_print();
	info_refresh();
	redrawwin(main_win);	
}	
	
static void box_create(void)
{
	if(_box.wd) return;
	set_size();
	_box.wd = newpad(_box.rows, _box.cols);
	if(!_box.wd) prg_exit(__FUNCTION__": cannot allocate memory.");
	wbkgd(_box.wd, COLOR_PAIR(9));
	werase(_box.wd);
	box(_box.wd, ACS_VLINE, ACS_HLINE);
	mesg_print();
	info_refresh();
}

/*
 * Can be called within any place in the program to
 * display some message. First argument is a title.
 * Second is a message.
 */
void info_box(char *t, char *i)
{
	info = i;
	title = t;
	box_create();
}

int info_box_keys(int key)
{
	if(!_box.wd) return 0;
	delwin(_box.wd);
	redrawwin(main_win);
	_box.wd = 0;
	return 1;
}

