/*
 * To reduce malloc/free overheads linked list of _block_tbl_t
 * is used. Pre-allocated memory is managed by a bit map.
 */

#include "whowatch.h"

#define USED 		1
#define NOT_USED	0
#define TBL_SIZE	(sizeof(unsigned int)*8)

static FILE *logfile;
static int nr_blocks;

void dolog(const char *t, ...)
{
        va_list ap;
        char *c;
        time_t tm = time(0);
return;	
	if(!logfile) {
		logfile = fopen("/var/log/whowatch.log", "a");
		if(!logfile) exit(1);
		fprintf(logfile, " #############\n");
	}
        c = ctime(&tm);
        *(c + strlen(c) - 1) = 0;
        fprintf(logfile, "%s: ", c);
        va_start(ap, t);
        vfprintf(logfile, t, ap);
        va_end(ap);
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
	if(!tmp) prg_exit(__FUNCTION__ ": Cannot allocate memory.\n");
	tmp->_block_t = calloc(1, size * TBL_SIZE);
dolog(__FUNCTION__ ":new block(%d) - alloc size = %d\n", nr_blocks, size * TBL_SIZE);
	if(!tmp->_block_t) prg_exit(__FUNCTION__ ": Cannot allocate memory.\n");
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
			if((1<<i & tmp->map) == NOT_USED) goto FOUND;
		}
nr++;
	}
	dolog(__FUNCTION__": no empty space, getting new one.\n");
	tmp = new_block(size, h);
	i = 0;
FOUND:
	dolog(__FUNCTION__": empty in %d block at %d pos\n", nr, i);
	tmp->map |= 1<<i;
	return tmp->_block_t + (size * i);
}

/*
 * Free all unused blocks of memory (map == 0)
 */
static void rm_free_blocks(struct list_head *head)
{
	struct _block_tbl_t *tmp;
	struct list_head *t, *p;
	t = head->next;
dolog(__FUNCTION__": entering\n");
	while(t != head) {
		tmp = list_entry(t, struct _block_tbl_t, head);
		p = t->next;
		if(!tmp->map) {
			dolog(__FUNCTION__": empty block found %p\n", tmp);
			free(tmp->_block_t);
			list_del(&tmp->head);
			free(tmp);
		}
		t = p;
	}
}

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
	dolog(__FUNCTION__ ":entry not found - error\n");
	return -1;
FOUND:
	dolog(__FUNCTION__": %p pointer found in %d\n", p, i);
	tmp->map &= ~(1<<(p - tmp->_block_t)/size);
	dolog(__FUNCTION__": setting map pos %d to zero, map = %x\n",
		(p - tmp->_block_t)/size, tmp->map);
	return 0;
}		 	

