//和开启子进程相关
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>   //信号相关头文件 
#include <errno.h>    //errno
#include <unistd.h>

#include "eve_func.h"
#include "eve_macro.h"
#include "eve_c_conf.h"


void eve_process_events_and_timers()
{
    g_socket.eve_epoll_process_events(-1); 

    
    g_socket.printTDInfo();
    
    
}

