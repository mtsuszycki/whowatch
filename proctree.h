

struct plist {
	struct proc* nx;
	struct proc** ppv;
};

struct proc {
	int pid;
	struct proc *parent;
	struct proc *child;
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
struct proc* find_by_pid(int pid);
struct proc* tree_start(int pid,char* buf);
struct proc* tree_next();

extern int num_proc;
