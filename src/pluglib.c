#include "whowatch.h"

#define SUBWIN_COLS	256
extern WINDOW *exti_wd;
extern struct wdgt *exti;

void println(const char *t, ...)
{
	int n;
	char buf[SUBWIN_COLS];
        va_list ap;
        va_start(ap, t);
        n = vsnprintf(buf, sizeof buf, t, ap);
  	scr_addstr(exti, buf, n); 
	va_end(ap);
}

void plgn_out_start(void)
{
	scr_output_start(exti);
}

void print(const char *t, ...)
{
	int n;
	char buf[SUBWIN_COLS];
        va_list ap;
        va_start(ap, t);
        n = vsnprintf(buf, sizeof buf, t, ap);
	waddstr((WINDOW*)exti->wd, buf);
	va_end(ap);
}

void newln(void)
{
	scr_addstr(exti, "\n", 1);
}

void boldon(void)
{
	wattrset((WINDOW*)exti->wd, A_BOLD);
}

void boldoff(void)
{
	wattrset((WINDOW*)exti->wd, A_NORMAL);
}

void title(const char *t, ...)
{
	int n;
	char buf[SUBWIN_COLS];
        va_list ap;
	boldon();
        va_start(ap, t);
        n = vsnprintf(buf, sizeof buf, t, ap);
	waddstr((WINDOW*)exti->wd, buf);
	//scr_addstr(exti, buf, n);
        va_end(ap);
	boldoff();
}

