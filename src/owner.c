/*
 * Functions needed for printing process owner in the tree.
 */
#include <pwd.h>
#include "whowatch.h"

#define HASHSIZE 32
#define NAME_SIZE	8

#define list_add(l,p) ({			\
	(p)->next = (l);			\
	(l) = (p);				\
})

struct owner{
	char name[NAME_SIZE + 1];
	int uid;
	struct owner *next;
};

/* remember each resolved uid->name in the hash table to save CPU time */
static struct owner *hash_table[HASHSIZE];

static inline int hash_fun(int n)
{
	return n&(HASHSIZE-1);
}

static inline struct owner* find_by_uid(int n)
{
	struct owner* p;

	p = hash_table[hash_fun(n)];
	while(p) {
		if(p->uid == n) break;
		p=p->next;
	}
	return p;
}

static inline struct owner* new_owner(int n)
{
	struct owner* p;
	struct passwd *u;
	p = (struct owner*) malloc(sizeof *p);
	if (!p) allocate_error();
	memset(p, 0, sizeof *p);
	u = getpwuid(n);
	if (!u) sprintf(p->name, "%d", n);
	else {
		strncpy(p->name, u->pw_name, NAME_SIZE);
		p->name[NAME_SIZE] = 0;
	}
	p->uid = n;
	list_add(hash_table[hash_fun(n)], p);
	return p;
}

char *get_owner_name(int u)
{
	struct owner *p;
	p = find_by_uid(u);
	if (!p) p = new_owner(u);
	return p->name;
}
