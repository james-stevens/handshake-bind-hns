/*
    #################################################################
    #    (c) Copyright 2009-2025 JRCS Ltd  - All Rights Reserved    #
    #################################################################
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>


#include "log_message.h"
#include "liball.h"

#define DEFAULT_LOG_FILE "/tmp/log.log"
static char log_file[PATH_MAX] = {0};
static FILE * logfp = NULL;


// #undef log_message

static int facility = LOG_LOCAL5;
loglvl_t reporting_level = 0;
static char application[1024];


void set_facility(int f) { facility = f; }


void init_log(char * argv0,loglvl_t level)
{
char *prog,*cp;

	if (((level&0xfffff)!=0)&&((level&0xf00000)==0)) {
		printf("ERROR: invalid log level -> 0x%08lX\n",level);
		log_exit(0);
		}

	if (argv0) {
		prog = argv0;
		if ((cp=strrchr(prog,'/'))!=NULL) prog=cp+1;
		// syslog(facility|LOG_INFO,"change program to '%s'",prog);

		if (prog[0]=='_') strcat(application,prog); else STRNCPY(application,prog,sizeof(application));
		application[sizeof(application)-1] = 0;
		}

	reporting_level = level;

	if (level & MSG_FACILITY) {
		int msg_local_vals[] = { 0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3,
			LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7, LOG_LOCAL0 };
		facility = msg_local_vals[(level & MSG_FACILITY) >> 16];
		}

	if (reporting_level&MSG_SYSLOG) openlog(application,LOG_PID|LOG_NDELAY,facility);
}


void set_log_file(char * file)
{
	STRCPY(log_file,file);
}





void do_log_message(char * file,int line,loglvl_t level,char * fmt,...)
{
#define MAX_MSG_LEN 32768
char msgbuf[MAX_MSG_LEN+10],buf[100],*end = msgbuf+MAX_MSG_LEN;
char * cp;
va_list ap;
int len;
time_t utm;
struct tm *stm;
int priority = 0;

	if (level & MSG_PRIORITY) {
		priority = (level & MSG_PRIORITY);
		level -= priority;
		priority = (priority >> 12) - 1;
		}

	if (!(level & reporting_level)) return;

	cp = msgbuf;
	len = 0;

	struct tm ts;

	utm = time(NULL);
	stm = localtime_r(&utm,&ts);
	strftime(buf,sizeof(buf),"%c",stm);

	len = snprintf(msgbuf,MAX_MSG_LEN,"%s: %s[%d]: ",buf,application,getpid());
	char * pre = cp = msgbuf + len;

	if (reporting_level&MSG_FILE_LINE) {
		int t = snprintf(cp,(end - cp),"[%s/%d] ",file,line);
		cp += t; len += t;
		}

	va_start(ap, fmt);
	len += vsnprintf(cp,(end - cp),fmt,ap);
	va_end(ap);

	if ((len>0)&&(msgbuf[len-1]!=10)) {
		char *p=msgbuf+len;
		*p++=10; *p++=0;
		len = p - msgbuf;
		}
	
	if (reporting_level&MSG_STDERR) { if (write(2,msgbuf,len)!=len) perror("write stderr"); }
	if (reporting_level&MSG_STDOUT) { if (write(1,msgbuf,len)!=len) perror("write stdout"); }
	if (reporting_level&MSG_SYSLOG) {
		if (!priority) {
			if (strncmp(pre,"ERROR: ",7)==0) priority = LOG_ERR;
			else if (strncmp(pre,"WARN: ",6)==0) priority = LOG_WARNING;
			else priority = LOG_INFO;
			}
		syslog(facility|priority,"%s",pre);
		}
	if (reporting_level&MSG_TOFILE) {
		if (!log_file[0]) STRCPY(log_file,DEFAULT_LOG_FILE);
		if (!logfp) logfp = fopen(log_file,"a+");
		fwrite(msgbuf,len,1,logfp);
		}
}


void log_exit(int ret)
{
	if (logfp) fclose(logfp);
	exit(ret);
}


void error_exit(int test,char * error_msg)
{
	log_message(MSG_DEBUG,"%s has test code %d\n",error_msg,test);
	if (test)
	    {
	    log_message(MSG_NORMAL,"FATAL: '%s' returned %d (%s)\n",error_msg,test,ERRMSG);
	    log_exit(1);
	    }
}



uint64_t long_time()
{
struct timeval tv;
uint64_t l_now;

	gettimeofday(&tv,NULL);
	l_now = tv.tv_sec;
	l_now *= 1000000;
	l_now += tv.tv_usec;
	return l_now;
}
