#include <stdarg.h>
#include "subwin.h"

void println(const char *t, ...)
{
	char buf[SUBWIN_COLS];
        va_list ap;
        va_start(ap, t);
        vsnprintf(buf, sizeof buf, t, ap);
	waddstr(main_pad->wd, buf);
        va_end(ap);
	waddstr(main_pad->wd, "\n");
	sub_current->lines++;
}

void print(const char *t, ...)
{
	char buf[SUBWIN_COLS];
        va_list ap;
        va_start(ap, t);
        vsnprintf(buf, sizeof buf, t, ap);
	waddstr(main_pad->wd, buf);
        va_end(ap);
}

void newln(void)
{
//	waddstr(main_pad->wd, "\n");
	sub_current->lines++;
}

void boldon(void)
{
	wattrset(main_pad->wd, A_BOLD);
}

void boldoff(void)
{
	wattrset(main_pad->wd, A_NORMAL);
}

void title(const char *t, ...)
{
	char buf[SUBWIN_COLS];
        va_list ap;
	boldon();
        va_start(ap, t);
        vsnprintf(buf, sizeof buf, t, ap);
	waddstr(main_pad->wd, buf);
        va_end(ap);
	boldoff();
}
