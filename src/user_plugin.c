/* 
 * Print detailed information about user. This is a builtin plugin.
 */
#include "pluglib.h"
#include <pwd.h>

#define MAILBOX_PATH	"/var/spool/mail"

/* 
 * Argument is a username taken from current
 * cursor position.
 */
void builtin_user_draw(void *n)
{
	struct passwd *pw;
	pw = getpwnam((char*)n);
	if(!pw) {
		println("Error - user not found.");
		return;
	}
	title("NAME: "); println("%s", pw->pw_name);
	title("HOME: "); println("%s", pw->pw_dir);
	title("UID: "); println("%d", pw->pw_uid);
	title("GID: "); println("%d", pw->pw_gid);
	title("SHELL: "); println("%s", pw->pw_shell);
	title("GECOS: "); println("%s", pw->pw_gecos);
}

	