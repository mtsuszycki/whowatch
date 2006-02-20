
#define DU(B) typedef u_int ## B ## _t u ## B
DU(8); DU(16); DU(32); DU(64);
#undef DU

//def DEBUG
#if 1
#define DBG(f,a...)	dolog("%s %d: " f, __FUNCTION__, __LINE__ , ##a)
#else
#define DBG(f,a)	/* */
#endif

