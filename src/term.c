#include <fcntl.h>
#include <termios.h>

static struct termios tio;

void term_raw()
{
	struct termios t;
	tcgetattr(0, &tio);
	t = tio;
	t.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(0, TCSANOW, &t);
	fcntl(0, F_SETFL, O_NONBLOCK);
}

void term_rest()
{
	tcsetattr(0, TCSANOW, &tio);
}
