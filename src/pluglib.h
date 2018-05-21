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

#define	PERIODIC	0x1
#define INSERT		0x2
#define APPEND		0x4
#define OVERWRITE	0x8

#define PROC_PLUGIN     0
#define USER_PLUGIN     1
#define SYS_PLUGIN      2

extern int plugin_type;

void plgn_out_start(void);
void output_start(void);
void println(const char *, ...);
void print(const char *, ...);
void title(const char *, ...);
void newln(void);
int plugin_init(void *);
void plugin_draw(void *);
void plugin_clear(void);
void plugin_cleanup(void);
void boldon(void);
void boldoff(void);


