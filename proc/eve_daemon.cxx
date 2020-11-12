//和守护进程相关
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>     //errno
#include <sys/stat.h>
#include <fcntl.h>


#include "eve_func.h"
#include "eve_macro.h"
#include "eve_c_conf.h"


int eve_daemon()
{
    
    switch (fork())  
    {
    case -1:
       
        eve_log_error_core(EVE_LOG_EMERG,errno, "eve_daemon()中fork()失败!");
        return -1;
    case 0:
        
        break;
    default:
        
        return 1;  
    } //end switch

   eve_parent = eve_pid;     
    eve_pid = getpid();       
    
   
    if (setsid() == -1)  
    {
        eve_log_error_core(EVE_LOG_EMERG, errno,"eve_daemon()中setsid()失败!");
        return -1;
    }

   
    umask(0); 

   
    int fd = open("/dev/null", O_RDWR);
    if (fd == -1) 
    {
        eve_log_error_core(EVE_LOG_EMERG,errno,"eve_daemon()中open(\"/dev/null\")失败!");        
        return -1;
    }
    if (dup2(fd, STDIN_FILENO) == -1) 
    {
        eve_log_error_core(EVE_LOG_EMERG,errno,"eve_daemon()中dup2(STDIN)失败!");        
        return -1;
    }
    if (dup2(fd, STDOUT_FILENO) == -1) 
    {
        eve_log_error_core(EVE_LOG_EMERG,errno,"eve_daemon()中dup2(STDOUT)失败!");
        return -1;
    }
    if (fd > STDERR_FILENO)  
     {
        if (close(fd) == -1)  
        {
            eve_log_error_core(EVE_LOG_EMERG,errno, "eve_daemon()中close(fd)失败!");
            return -1;
        }
    }
    return 0; 
}

