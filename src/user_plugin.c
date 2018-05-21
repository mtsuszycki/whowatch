/*

Copyright 2001, 2006 Michal Suszycki <mt_suszycki@yahoo.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

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


