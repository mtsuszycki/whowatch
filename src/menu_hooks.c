/* 
 * All bindings to menu items.
 */
#include "whowatch.h"
#include "subwin.h"
#include "pluglib.h"

static char *prev_search;

void  m_exit(void)
{
	dolog("%s: entering\n", __FUNCTION__);
	prg_exit("");
}	
void m_details(void)
{
	sub_keys('d');
}

void m_kill(void)
{
	current->keys('K' | KBD_CTRL);
}

void m_hup(void)
{
	current->keys('U' | KBD_CTRL);
}

void m_term(void)
{
	current->keys('T' | KBD_CTRL);
}

void m_sysinfo(void)
{
	sub_keys('s');
}


void m_siglist(void)
{
	sub_keys('l');
}


void m_process(void)
{
	current->keys('t');
}

void m_owner(void)
{
	current->keys('o');
}

void m_long(void)
{
	full_cmd ^= 1;
	current->redraw();
}

void m_switch(void)
{
	current->keys(KBD_ENTER);
}

void m_idle(void)
{
	current->keys('i');
}

static void print_about(void *unused)
{
	println("Whowatch 1.6.0");
	println("Interactive process and users monitoring tool.");
	println("Author: Michal Suszycki <mike@wizard.ae.krakow.pl>");
	println("");
	println("BUILTIN PLUGINS:");
	println("- proccess plugin: show detailed information about process");
	println("- user plugin: show detailed information about user");
	println("- system plugin: system details");
}

void m_about(void)
{
	new_sub(print_about);
}

static void __load_plugin(char *s)
{
	char *err;
	if(!(err = plugin_load(s)))
		info_box(" Message ", "Plugin has been loaded successfully.");
	else if(*s) 
	info_box(" Error ", err);
}

void m_load_plugin(void)
{
	input_box(" Load plugin ", "Path ", 0, __load_plugin);
}

static void search(char *s)
{
	/* search functions are located in search.c */
	do_search(s);	
}

/*
void clear_search(void)
{
	prev_search = 0;
}
*/

void set_search(char *s)
{
	prev_search = s;
}

void m_search(void) 
{
	input_box(" Search ", "Pattern ", prev_search, search);
}
