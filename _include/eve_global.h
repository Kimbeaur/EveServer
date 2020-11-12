
#ifndef __EVE_GBLDEF_H__
#define __EVE_GBLDEF_H__

#include <signal.h> 

#include "eve_c_slogic.h"
#include "eve_c_threadpool.h"

//一些比较通用的定义放在这里，比如typedef定义
//一些全局变量的外部声明也放在这里

//类型定义----------------

//结构定义
typedef struct _CConfItem
{
	char ItemName[50];
	char ItemContent[500];
}CConfItem;

//和运行日志相关 
typedef struct
{
	int    log_level;   //日志级别 或者日志类型，eve_macro.h里分0-8共9个级别
	int    fd;          //日志文件描述符

}eve_log_t;


//外部全局量声明
extern size_t        g_argvneedmem;
extern size_t        g_envneedmem; 
extern int           g_os_argc; 
extern char          **g_os_argv;
extern char          *gp_envmem; 
extern int           g_daemonized;
extern CLogicSocket  g_socket;  
extern CThreadPool   g_threadpool;

extern pid_t         eve_pid;
extern pid_t         eve_parent;
extern eve_log_t     eve_log;
extern int           eve_process;   
extern sig_atomic_t  eve_reap;   
extern int           g_stopEvent;

#endif
