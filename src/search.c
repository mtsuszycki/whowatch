#include "whowatch.h"
#include <regex.h>


static char prev_search[64];
//static struct window *prev;
static regex_t cur_reg;

/* 
 * Called by getprocbyname() and user_search(), instead of simple strncmp().
 */
inline int reg_match(const char *s)
{
	return !regexec(&cur_reg, s, 0, 0, REG_NOTEOL); 
}

void do_search(char *s)
{
	unsigned int p;
	int err;
	static char errbuf[64];
	
//	if(prev && current != prev) clear_search();
//	prev = current;
	err = regcomp(&cur_reg, s, REG_EXTENDED | REG_ICASE | REG_NOSUB);
	if(err) {
		regerror(err, &cur_reg, errbuf, sizeof errbuf);
		info_box(" Regex error ", errbuf);
		return;
	}
	if(current == &proc_win)
		p = getprocbyname(current->cursor + current->offset);
	else p = user_search(current->cursor + current->offset);
	
	snprintf(prev_search, sizeof prev_search, "%s", s);
	set_search(prev_search);
	regfree(&cur_reg);
	if(p == -1) return;
	/* move the cursor to appropriate line */
	to_line(p, current);
	pad_draw();
dolog(__FUNCTION__": %d\n", p);
}




