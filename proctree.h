#define USE_PT_PRIV

struct plist {
	struct proc_t* nx;
	struct proc_t** ppv;
};

struct proc_t {
	int pid;
	struct proc_t *parent;
	struct proc_t *child;
	struct plist mlist;
	struct plist broth;
	struct plist hash;
#ifdef USE_PT_PRIV
	void* priv;
#endif
};

#ifdef USE_PT_PRIV
int update_tree(void del(void*));
#else
int update_tree();
#endif
struct proc_t* find_by_pid(int pid);
struct proc_t* tree_start(int root,int pid,char* buf);
struct proc_t* tree_next();

extern int num_proc;
