
#define PROC_DEPTH 8

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
	void* priv;
};

int update_tree(void del(void*));
struct proc_t* find_by_pid(int pid);
struct proc_t* tree_start(int root, int start);
struct proc_t* tree_next();
char *tree_string(int root, struct proc_t *proc);

extern int num_proc;
