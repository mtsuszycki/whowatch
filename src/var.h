
#define DU(B) typedef u_int ## B ## _t u ## B
DU(8); DU(16); DU(32); DU(64);
#undef DU

#define MIN(x,y)	(x)<(y)?(x):(y)

//#if 1
#ifdef DEBUG
#define DBG(f,a...)	dolog("%s %d: " f, __FUNCTION__, __LINE__ , ##a)
#else
#define DBG(f,a...)	/* */
#endif

#define elemof(x)	(sizeof (x) / sizeof*(x))
#define endof(x)	((x) + elemof(x))

