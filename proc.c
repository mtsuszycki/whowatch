#include "whowatch.h"

/*
 * Find parent pid (in /proc/pid/stat)
 */
int get_ppid(int pid)
{
        static int ppid;
        char buff[256];
        FILE *f;
        sprintf(buff,"/proc/%d/stat",pid);
        if (!(f =  fopen(buff,"rt")))
		return -1;
        fscanf(f,"%*d %*s %*c %d",&ppid);
        fclose(f);
        return ppid;
}
	
/*
 * Get name of the process
 */														
char *get_name(int pid)
{
        static char buf[256];
        FILE *f;
        sprintf(buf,"/proc/%d/stat",pid);
        if (!(f = fopen(buf,"rt")))
               	return "can't read";
        fscanf(f,"%*d %s",buf);
	fclose(f);
	return buf;
}

char get_state(int pid)
{
	static char buf[256];
	char state;
        FILE *f;
        sprintf(buf,"/proc/%d/stat",pid);
        if (!(f = fopen(buf,"rt")))
               	return '?';
        fscanf(f,"%*d %*s %c",&state);
	fclose(f);
	return state;
}
/*
 * Get process' command line
 */
char *get_cmdline(int pid)
{
        static char buff[512];
        FILE *f;
        int i = 0;
        sprintf(buff,"/proc/%d/cmdline",pid);
        if (!(f = fopen(buff,"rt")))
               	return "-";
	while (fread(buff+i,1,1,f) == 1){
		if (buff[i] == '\0') buff[i] = ' ';
		if (i == sizeof buff - 1) break;
		i++;
	}
	fclose(f);
	buff[i] = '\0';
	return i?buff:get_name(pid);
}

char *get_w(int pid)
{
	static char buf[256];
	FILE *f;
	static int tpgid;
	
	sprintf(buf,"/proc/%d/stat",pid);
	if (!(f = fopen(buf,"r"))) return "-";
	fscanf(f,"%*d %*s %*c %*d %*d %*d %*d %d",&tpgid);
	fclose(f);
	return get_cmdline(tpgid);
}

/* it should not be here in proc.c */
char *count_idle(char *tty)
{
	struct stat st;
	static char buf[256];
	time_t idle_time;
	
	sprintf(buf,"/dev/%s",tty);
	
	if (stat(buf,&st) == -1) return "?";
	idle_time = time(0) - st.st_atime;	
	
	if (idle_time >= 3600 * 24) 
		sprintf(buf,"%ldd",idle_time/(3600 * 24) );
	else if (idle_time >= 3600){
		time_t min = (idle_time % 3600) / 60;
		if (min < 10)
			sprintf(buf,"%ld:0%ld", idle_time/3600, min);
		else
			sprintf(buf,"%ld:%ld", idle_time/3600, min);
	}
	else if (idle_time >= 60)
		sprintf(buf,"%ld",idle_time/60);
	else
		sprintf(buf," ");
	
	return buf;
}
