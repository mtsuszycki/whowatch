#define	PERIODIC	0x1
#define INSERT		0x2
#define APPEND		0x4
#define OVERWRITE	0x8

#define PROC_PLUGIN     0
#define USER_PLUGIN     1
#define SYS_PLUGIN      2

extern int plugin_type;

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


