
//和网络 中 接受连接【accept】 有关的函数放这里
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>    //uintptr_t
#include <stdarg.h>    //va_start....
#include <unistd.h>    //STDERR_FILENO等
#include <sys/time.h>  //gettimeofday
#include <time.h>      //localtime_r
#include <fcntl.h>     //open
#include <errno.h>     //errno
//#include <sys/socket.h>
#include <sys/ioctl.h> //ioctl
#include <arpa/inet.h>

#include "eve_c_conf.h"
#include "eve_macro.h"
#include "eve_global.h"
#include "eve_func.h"
#include "eve_c_socket.h"

//建立新连接专用函数，当新连接进入时
void CSocket::eve_event_accept(eve_connection_t* oldc)
{
   
    struct sockaddr    mysockaddr;        //远端服务器的socket地址
    socklen_t          socklen;
    int                err;
    int                level;
    int                s;
    static int         use_accept4 = 1;   //我们先认为能够使用accept4()函数
    eve_connection_t* newc;              //代表连接池中的一个连接【注意这是指针】
    

    socklen = sizeof(mysockaddr);
    do   //用do，跳到while后边去方便
    {     
        if(use_accept4)
        {
            //以为listen套接字是非阻塞的，所以即便已完成连接队列为空，accept4()也不会卡在这里；
            s = accept4(oldc->fd, &mysockaddr, &socklen, SOCK_NONBLOCK); //从内核获取一个用户端连接，最后一个参数SOCK_NONBLOCK表示返回一个非阻塞的socket，节省一次ioctl【设置为非阻塞】调用
        }
        else
        {
            //以为listen套接字是非阻塞的，所以即便已完成连接队列为空，accept()也不会卡在这里；
            s = accept(oldc->fd, &mysockaddr, &socklen);
        }

        if(s == -1)
        {
            err = errno;

            //对accept、send和recv而言，事件未发生时errno通常被设置成EAGAIN（意为“再来一次”）或者EWOULDBLOCK（意为“期待阻塞”）
            if(err == EAGAIN) //accept()没准备好，这个EAGAIN错误EWOULDBLOCK是一样的
            {
                //除非你用一个循环不断的accept()取走所有的连接，不然一般不会有这个错误【我们这里只取一个连接，也就是accept()一次】
                return ;
            } 
            level = EVE_LOG_ALERT;
            if (err == ECONNABORTED)  //ECONNRESET错误则发生在对方意外关闭套接字后【您的主机中的软件放弃了一个已建立的连接--由于超时或者其它失败而中止接连(用户插拔网线就可能有这个错误出现)】
            {
                  level = EVE_LOG_ERR;
            } 
            else if (err == EMFILE || err == ENFILE) //EMFILE:进程的fd已用尽
            {
                level = EVE_LOG_CRIT;
            }
          
            if(use_accept4 && err == ENOSYS) //accept4()函数没实现，
            {
                use_accept4 = 0;  //标记不使用accept4()函数，改用accept()函数
                continue;         
            }

            if (err == ECONNABORTED)  //对方关闭套接字
            {
                //这个错误因为可以忽略，所以不用干啥
                //do nothing
            }
            
            if (err == EMFILE || err == ENFILE) 
            {
               //这个错误暂时不处理
            }            
            return;
        }  //end if(s == -1)

          
        if(m_onlineUserCount >= m_worker_connections)  //用户连接数过多，要关闭该用户socket，因为现在也没分配连接，所以直接关闭即可
        {
            close(s);
            return ;
        }
        //如果某些恶意用户连上来发了1条数据就断，不断连接，断开，就把其断掉，防止消耗服务器资源
        if(m_connectionList.size() > (m_worker_connections * 5))
        {
            if(m_freeconnectionList.size() < m_worker_connections)
            { 
                close(s);
                return ;   
            }
        }

        
        newc = eve_get_connection(s); //这是针对新连入用户的连接，和监听套接字 所对应的连接是两个不同的东西，不要搞混
        if(newc == NULL)
        {
            //连接池中连接不够用，那么就得把这个socekt直接关闭并返回了，因为在eve_get_connection()中已经写日志了，所以这里不需要写日志了
            if(close(s) == -1)
            {
                eve_log_error_core(EVE_LOG_ALERT,errno,"CSocket::eve_event_accept()中close(%d)失败!",s);                
            }
            return;
        }

        //成功的拿到了连接池中的一个连接
        memcpy(&newc->s_sockaddr,&mysockaddr,socklen);  

        if(!use_accept4)
        {
            
            if(setnonblocking(s) == false)
            {
                //设置非阻塞居然失败
                eve_close_connection(newc); 
                return; //直接返回
            }
        }

        newc->listening = oldc->listening;  //连接对象 和监听对象关联，方便通过连接对象找监听对象【关联到监听端口】
            
        newc->rhandler = &CSocket::eve_read_request_handler;  
        newc->whandler = &CSocket::eve_write_request_handler; 

        //客户端应该主动发送第一次的数据，这里将读事件加入epoll监控       
        if(eve_epoll_oper_event(s, EPOLL_CTL_ADD, EPOLLIN|EPOLLRDHUP, 0, newc) == -1)         
        {
            eve_close_connection(newc);
            return; 
        }
 

        if(m_ifkickTimeCount == 1)
        {
            AddToTimerQueue(newc);
        }
        ++m_onlineUserCount;  //连入用户数量+1        
        break;  //一般就是循环一次就跳出去
    } while (1);   

    return;
}

