/*

SPDX-License-Identifier: GPL-2.0+

Copyright 2006 Michal Suszycki <mt_suszycki@yahoo.com>

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

#ifndef UTMP_FILE
#define UTMP_FILE 	"/var/run/utmp"
#endif

#ifndef WTMP_FILE
#define WTMP_FILE 	"/var/log/wtmp"
#endif

#define LOGIN		1
#define LOGOUT		-1

#ifdef HAVE_UT_NAME
#define ut_user ut_name
#endif

struct user_t
{
	struct list_head head;
	char 	name[UT_NAMESIZE + 1];
	char 	tty[UT_LINESIZE + 1];
	int 	pid;
	char 	parent[16];
	char 	host[UT_HOSTSIZE + 1];
	int 	line;
};

static DECL_LIST_HEAD(users_l);
static int toggle;	/* if 0 show cmd line else show idle time 	*/
//static char *uhelp = "\001[F1]Help [F9]Menu [ENT]proc all[t]ree [i]dle/cmd "
//			" [c]md [d]etails [s]ysinfo";

#ifdef HAVE_PROCESS_SYSCTL
int get_login_pid(char *);
#endif

struct prot_t
{
	char	*s;
	short 	port;
	u32	nr;
};


