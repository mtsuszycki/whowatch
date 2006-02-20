/* 
 * In fact it is a special plugin because it uses plugin API.
 * It implements context sensitive help messages.
 */
#include "whowatch.h"
#include "pluglib.h"

static void general(void)
{
	title("GENERAL KEYS:\n"); newln();
	println("cursor movement:");
	println("- cursor up, down, Home, End");
	println("- PageUp, PageDown");
	println("");
	title("F9"); println(" - menu");
	title("ESC"); println(" - close window/menu or quit");
	title("d"); println(" - user or process details");
	title("s"); println(" - system information");
	title("t"); println(" - tree of all processes");
	title("/"); println(" - search");
	println("");
}

static void userwin_help(void)
{
	title("USERS LIST:\n"); newln();
	title("ENTER"); println(" - selected user processes");
	title("i"); println(" - toggle users idle time");
	title("c"); println(" - toggle long command line");
}

static void procwin_help(void)
{
	title("PROCESS TREE:\n"); newln();
	title("ENTER"); println(" - go back to user list");
	title("l"); println(" - choose from signal list");
	title("o"); println(" - toggle process owner");
	title("c"); println(" - toggle long command line");
	title("^K"); println(" - send KILL signal");
}
	
void sub_help(void)
{
	println("");
        title("DETAILS WINDOW:\n"); newln();
        title("a"); println(" - up");
        title("z"); println(" - down");
        title("left cursor"); println(" - left");
        title("right cursor"); println(" - right");
}

static void show_help(void *unused)
{
//dolog(__FUNCTION__":printing help\n");
	general();
	if(current == &users_list) userwin_help();
	if(current == &proc_win) procwin_help();	
	sub_help();
}

void help(void)
{
	new_sub(show_help);
}
