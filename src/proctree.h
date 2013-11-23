
#define TREE_DEPTH 32
#define TREE_STRING_SZ (2 + 2*TREE_DEPTH)

struct plist {
	struct proc_t* nx;
	struct proc_t** ppv;
};

struct proc_t {
	int pid;
	char   state;
	struct proc_t *parent;
	struct proc_t *child;
	struct plist mlist;
	struct plist broth;
	struct plist hash;
	void* priv;
};

int update_tree(void del(void*));
int update_tree();
struct proc_t* find_by_pid(int pid);
struct proc_t* tree_start(int root, int start);
struct proc_t* tree_next();
char *tree_string(int root, struct proc_t *proc);

extern int num_proc;

#ifdef HAVE_STRUCT_KINFO_PROC_KP_PROC
	#ifdef KERN_PROC2
		#define kinfo_pid p_pid
		#define kinfo_ppid p_ppid
		#define kinfo_tpgid p_tpgid
		#define kinfo_svuid p_svuid
		#define kinfo_stat p_stat
		#define kinfo_comm p_comm
		#define kinfo_tdev p_tdev
	#else
		#define kinfo_pid kp_proc.p_pid
		#define kinfo_ppid kp_eproc.e_ppid
		#define kinfo_tpgid kp_eproc.e_tpgid
		#define kinfo_svuid kp_eproc.e_pcred.p_svuid
		#define kinfo_stat kp_proc.p_stat
		#define kinfo_comm kp_proc.p_comm
		#define kinfo_tdev kp_eproc.e_tdev
	#endif
#else
	#define kinfo_pid ki_pid
	#define kinfo_ppid ki_ppid
	#define kinfo_tpgid ki_tpgid
	#define kinfo_svuid ki_svuid
	#define kinfo_stat ki_stat
	#define kinfo_comm ki_comm
	#define kinfo_tdev ki_tdev
#endif
