#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pluglib.h"

#define MAILBOX_PATH	"/var/spool/mail"

int plugin_type = USER_PLUGIN;

int plugin_init(void *unused) 
{
	println("Initializing plugin...");
	return APPEND | PERIODIC;
}

void plugin_draw(void *p)
{
	char *name = (char*)p;
	char buf[128];
	struct stat st;
	
	snprintf(buf, sizeof buf, "%s/%s", MAILBOX_PATH, name);
	title("MAILBOX SIZE: ");
	if(stat(buf, &st) == -1) {
		println("%s %s", buf, strerror(errno));
		return;
	}
	println("%ld", st.st_size);
}


