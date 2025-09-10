/*
    #################################################################
    #    (c) Copyright 2009-2025 JRCS Ltd  - All Rights Reserved    #
    #################################################################
*/
#ifndef _INCLUDE_LOG_MESSAGE
#define _INCLUDE_LOG_MESSAGE

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>

#define MSG_NONE				0x000000
#define MSG_NORMAL				0x000001
#define MSG_HIGH				0x000002
#define MSG_ALLOC				0x000004
#define MSG_SQL 				0x000008
#define MSG_REGISTRAR			0x000010
#define MSG_DEBUG				0x000020
#define MSG_DEBUG_HIGH			0x000040
#define MSG_DEBUG_VERY_HIGH		0x000080

#define MSG_REASON				0x000100
#define MSG_FILE_LINE			0x000200
#define MSG_SQL_UPDATE			0x000400

#define MSG_PRIORITY			0x00F000
#define LOG_CONV(A) ((A+1)<<12)
#define MSG_PRI_EMERG           LOG_CONV(LOG_EMERG)
#define MSG_PRI_ALERT           LOG_CONV(LOG_ALERT)
#define MSG_PRI_CRIT            LOG_CONV(LOG_CRIT)
#define MSG_PRI_ERR             LOG_CONV(LOG_ERR)
#define MSG_PRI_WARNING         LOG_CONV(LOG_WARNING)
#define MSG_PRI_NOTICE          LOG_CONV(LOG_NOTICE)
#define MSG_PRI_INFO            LOG_CONV(LOG_INFO)
#define MSG_PRI_DEBUG           LOG_CONV(LOG_DEBUG)

#define MSG_ERROR				(MSG_PRI_ERR|MSG_NORMAL)
#define MSG_WARNING				(MSG_PRI_WARNING|MSG_NORMAL)

#define MSG_FACILITY			0x0F0000
#define MSG_LOCAL1				0x010000
#define MSG_LOCAL2				0x020000
#define MSG_LOCAL3				0x030000
#define MSG_LOCAL4				0x040000
#define MSG_LOCAL5				0x050000
#define MSG_LOCAL6				0x060000
#define MSG_LOCAL7				0x070000
#define MSG_LOCAL0				0x080000

#define MSG_STDOUT				0x100000
#define MSG_STDERR				0x200000
#define MSG_SYSLOG				0x400000
#define MSG_TOFILE				0x800000

#define MSG_EXTRA 				MSG_NORMAL

typedef uint64_t loglvl_t;
extern loglvl_t reporting_level;
extern void do_log_message(char * file, int line, loglvl_t level,char * fmt,...);
extern void error_exit(int test,char * error_msg);
extern void init_log(char * app,loglvl_t level);
extern void set_facility(int f);
extern void log_exit(int ret);

#define LEVEL(A) ((A[0]=='x')?xtoi(A+1):strtoul(A,NULL,10))

extern uint64_t long_time();

#define ERRMSG  strerror(errno)

#ifdef NO_LOGGING
#define log_message(...)
#define logmsg(...)
#define log_level(A) (0)
#warning NO_LOGGING
#else
#define log_level(A)	(reporting_level&(A))
#define log_message(A,args...) do { if (log_level(A)) do_log_message(__FILE__,__LINE__,A,## args); } while(0)
#define logmsg(A,args...) do { if (log_level(A)) do_log_message(__FILE__,__LINE__,A,## args); } while(0)
#endif

#ifdef DEBUG_LOG
#undef log_level
#undef log_message
#undef logmsg
#define log_level(A)	(reporting_level&(A))
#define log_message(A,args...) do { if (log_level(A)) fprintf(stdout,## args); } while(0)
#define logmsg(A,args...) do { if (log_level(A)) fprintf(stdout,## args); } while(0)
#endif


#endif // _INCLUDE_LOG_MESSAGE
