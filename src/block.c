/*

SPDX-License-Identifier: GPL-2.0+

Copyright 2001 Michal Suszycki <mt_suszycki@yahoo.com>

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
 * To reduce malloc/free overheads linked list of _block_tbl_t
 * is used. Pre-allocated memory is managed by a bit map.
 */

#include "whowatch.h"

#define USED 		1
#define NOT_USED	0
#define TBL_SIZE	(sizeof(unsigned int)*8)

//static int nr_blocks;

void dolog(const char *t, ...)
{
	va_list ap;
	static FILE *logfile;
	char *c;

	time_t tm = time(0);
	if(!logfile) {
		logfile = fopen("/var/log/whowatch.log", "a");
		if(!logfile) return;
		fprintf(logfile, " #############\n");
	}
	c = ctime(&tm);
	*(c + strlen(c) - 1) = 0;
	fprintf(logfile, "%s: ", c);
	va_start(ap, t);
	vfprintf(logfile, t, ap);
	va_end(ap);
	fprintf(logfile, "\n");
	fflush(logfile);
}

struct _block_tbl_t {
	struct list_head head;
	void *_block_t;
	unsigned int map;
};

static struct _block_tbl_t *new_block(int size, struct list_head *h)
{
	struct _block_tbl_t *tmp;
	tmp = calloc(1, sizeof *tmp);
	if(!tmp) exit(0);
	tmp->_block_t = calloc(1, size * TBL_SIZE);
	if(!tmp->_block_t) exit(0);
	list_add(&tmp->head, h);
	return tmp;
}

/*
 * Returns address of unused memory space. If necessary
 * allocate new block.
 */
void *get_empty(int size, struct list_head *h)
{
	int i;
int nr = 0;
	struct _block_tbl_t *tmp;
	struct list_head *t;
	list_for_each(t, h) {
		tmp = list_entry(t, struct _block_tbl_t, head);
		if(tmp->map == ~0) continue;
		for(i = 0; i < TBL_SIZE; i++) {
			if((1U<<i & tmp->map) == NOT_USED) goto FOUND;
		}
nr++;
	}
	tmp = new_block(size, h);
	i = 0;
FOUND:
	tmp->map |= 1<<i;
	return tmp->_block_t + (size * i);
}

/*
 * Free all unused blocks of memory (map == 0)
static void rm_free_blocks(struct list_head *head)
{
	struct _block_tbl_t *tmp;
	struct list_head *t, *p;
	t = head->next;
	while(t != head) {
		tmp = list_entry(t, struct _block_tbl_t, head);
		p = t->next;
		if(!tmp->map) {
			free(tmp->_block_t);
			list_del(&tmp->head);
			free(tmp);
		}
		t = p;
	}
}
 */

/*
 * Find entry pointed by p and mark it unused.
 */
int free_entry(void *p, int size, struct list_head *h)
{
	struct _block_tbl_t *tmp;
	struct list_head *t;
int i = 0;
	list_for_each(t, h) {
		tmp = list_entry(t, struct _block_tbl_t, head);
		if(p >= tmp->_block_t && p < (tmp->_block_t + size * TBL_SIZE))
			goto FOUND;
		i++;
	}
	return -1;
FOUND:
	tmp->map &= ~(1<<(p - tmp->_block_t)/size);
	return 0;
}

