#include <ctype.h>
#include "whowatch.h"
#include "subwin.h"

#define MAX_INPUT	128
#define INPUT_LINE	0
#define	OK_BUTTON	1
#define CANCEL_BUTTON	2

static char in[MAX_INPUT];	/* buffer for a input string 		*/
static int pos;			/* current cursor position 		*/
void (*call_back)(char *in);
static unsigned short offset;
static struct pad_t _box, _in;
static char *descr, *title;
static short cur_button;

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

static void in_set_size(int l)
{
	_in.cols = screen_cols - 2*_box.size_x - 3;
	_in.size_x = _box.size_x + l + 1;
	_in.size_y =  _box.size_y + _box.size_y/2 - 1;
}

static inline void in_refresh(void)
{
	pnoutrefresh(_in.wd, 0, offset, _in.size_y,
		 _in.size_x, _in.size_y, _in.cols + _box.size_x);
}

void box_refresh(void)
{
	if(!_box.wd) return;
	pnoutrefresh(_box.wd, 0, 0, _box.size_y, _box.size_x, 
		_box.rows+2*_box.size_y, _box.cols+2*_box.size_x);
	in_refresh();
}

/*
 * Fills input line with a given string. Needed only
 * at input box creation.
 */
static void fill_input(char *s)
{
	int len;
	if(!s) return;
	len = strlen(s);
	if(len > _in.cols) offset += len - _in.cols; 
	mvwaddnstr(_in.wd, 0, 0, s + offset, _in.cols);
	pos = strlen(s + offset);
} 
 
/*
 * Prints OK and Cancel buttons. One of them is highlighted.
 * It depends on 'type' argument.
 */
static void buttons(short type)
{
	static short coord_y, ok_x, cancel_x;
	
	if(type == INPUT_LINE) curs_set(1);
	else curs_set(0);	
	coord_y = _box.rows*2/3;
	ok_x = _box.cols/2-8;
	cancel_x = _box.cols/2+1;
	if(type == OK_BUTTON) 	wattrset(_box.wd, A_REVERSE);
	mvwaddstr(_box.wd, coord_y, ok_x, "[ OK ]");
	wattrset(_box.wd, A_NORMAL);
	if(type == CANCEL_BUTTON) wattrset(_box.wd, A_REVERSE);
	mvwaddstr(_box.wd, coord_y, cancel_x, "[ Cancel ]");
	wattrset(_box.wd, A_NORMAL);
}

static void title_print(void)
{
assert(title);
assert(descr);
	mvwaddstr(_box.wd, 0, _box.cols/2-strlen(title)/2, title);
	mvwaddstr(_box.wd, _in.size_y-_box.size_y, 1, descr);
	buttons(cur_button);
}

void box_resize(void)
{
	if(!_box.wd) return;
	set_size();
	in_set_size(strlen(descr));
	wresize(_box.wd, _box.rows, _box.cols);
	werase(_box.wd);
	box(_box.wd, ACS_VLINE, ACS_HLINE);
	title_print();
	fill_input(in);
	redrawwin(main_win);	
	box_refresh();
}	

	
static int box_create(void)
{
	if(_box.wd) return 0;
	set_size();
	in_set_size(strlen(descr));
	_box.wd = newpad(_box.rows, _box.cols);
	_in.wd = newpad(1, MAX_INPUT);
	wbkgd(_in.wd, COLOR_PAIR(3));
	wbkgd(_box.wd, COLOR_PAIR(9));
	werase(_box.wd);
	werase(_in.wd);
	box(_box.wd, ACS_VLINE, ACS_HLINE);
	title_print();
	fill_input(in);
	box_refresh();
	return 1;
}

void clb(char *s)
{
	dolog(__FUNCTION__": in: %s\n", s);
}

/* 
 * Create new input box. Callback function will be called
 * when ENTER is pressed with inbox input data as an argument.
 */
void input_box(char *t, char *d, char *p, void (*callback)(char *in))
{
	descr = d;
	title = t;
	if(p) snprintf(in, sizeof in, "%s", p);
	echo();
	curs_set(2);
	box_create();
	call_back = callback;
}

static void in_keys(int key)
{
	int size = _in.cols + _box.size_x - _in.size_x;
	
	switch(key) {
	case KBD_ENTER:
	case KBD_ESC:
		delwin(_box.wd);
		delwin(_in.wd);
		redrawwin(main_win);
		_box.wd = 0;
		curs_set(0);
		if(key == KBD_ENTER && call_back && 
			cur_button != CANCEL_BUTTON && pos) call_back(in);
		pos = offset = 0;
		bzero(in, sizeof in);
		cur_button = INPUT_LINE;
		break;
	case KBD_BS:
		if(!pos) break;
		in[--pos] = 0;
		mvwdelch(_in.wd, 0, pos);
		if(offset) offset--;
		break;
	case KBD_TAB:
		cur_button++;
		if(cur_button > CANCEL_BUTTON) cur_button = INPUT_LINE;
		buttons(cur_button);
		break;
	default:
		if(cur_button != INPUT_LINE || !isascii(key) || pos == MAX_INPUT-1) break;
		if(pos >= size) offset++;
		in[pos++] = key;
		waddch(_in.wd, key);
		break;
	}
}

	
int box_keys(int key)
{
	if(!_box.wd) return 0;	
	in_keys(key);
	return 1;	
}

	



