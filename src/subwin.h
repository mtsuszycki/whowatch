#include <ncurses.h>

#define SUBWIN_COLS     64              /* virtual size of a subwindow      */
#define SUBWIN_ROWS     128                                                                
#define PAD_X           screen_cols/5   /* coordinates of upper left corner */             
#define PAD_Y           screen_rows/4

#define MARG_X		screen_cols/5		/* vertical margin		 */	
#define MARG_Y		screen_rows/4		/* horizontal margin		 */
#define LR_X		(screen_cols-MARG_X)	/* x coord of lower right corner */
#define LR_Y		(screen_rows-MARG_Y)	/* y coord of lower right corner */

#define PAD_COLS	LR_X-MARG_X		/* number of visible columns	 */ 
#define PAD_ROWS	LR_Y-MARG_Y		/* number of visible rows	 */
#define BORDER_ROWS	PAD_ROWS+2		/* border size 			 */
#define BORDER_COLS	PAD_COLS+2

#define SUBWIN_NR	3

struct pad_t {
	WINDOW *wd;
	unsigned int size_x, size_y;
	unsigned int rows, cols;
};

struct subwin {
	unsigned int lines;
	unsigned int offset, xoffset;
	int arrow;
	unsigned int flags;
	void (*builtin_draw)(void *);	/* draw function in builtin plugin */
	void (*builtin_init)(void *);	/* init builtin plugin		   */
	void *handle;			/* handler for the dynamic library */
	int (*keys)(int key);		/* not used yet. 		   */
	int (*plugin_init)(void *);	
	void (*plugin_draw)(void *);
	void (*plugin_clear)(void);
	void (*plugin_cleanup)(void);
};


extern struct subwin sub_main, sub_proc, sub_user, sub_signal;
extern struct subwin *sub_current;
extern struct pad_t *main_pad;
