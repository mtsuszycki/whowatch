/*
 * Print detailed information about user. This is a builtin plugin.
 */
#include "pluglib.h"
#include "whowatch.h"
#include <pwd.h>

#define MAILBOX_PATH	"/var/spool/mail"

/*
 * Argument is a username taken from current
 * cursor position.
 */
void euser(void *p)
{
	struct passwd *pw;
	if(!p) {
		println("Invalid user");
		return;
	}
	pw = getpwnam((char *)p);
	if(!pw) {
		println("Error - user not found.");
		return;
	}
	title("NAME: "); println("%s\n", pw->pw_name);
	title("HOME: "); println("%s\n", pw->pw_dir);
	title("UID: "); println("%d\n", pw->pw_uid);
	title("GID: "); println("%d\n", pw->pw_gid);
	title("SHELL: "); println("%s\n", pw->pw_shell);
	title("GECOS: "); println("%s\n", pw->pw_gecos);
}


