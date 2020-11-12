
//和网络 中 连接/连接池 有关的函数放这里
//
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
#include "eve_c_memory.h"
#include "eve_c_lockmutex.h"

//---------------------------------------------------------------
//连接池成员函数
eve_connection_s::eve_connection_s()//构造函数
{		
    iCurrsequence = 0;    
    pthread_mutex_init(&logicPorcMutex, NULL); //互斥量初始化
}
eve_connection_s::~eve_connection_s()//析构函数
{
    pthread_mutex_destroy(&logicPorcMutex);    //互斥量释放
}
//分配出去一个连接的时候初始化一些内容,原来内容放在 eve_get_connection()里，现在放在这里
void eve_connection_s::GetOneToUse()
{
    ++iCurrsequence;

    fd  = -1;                                         //开始先给-1
    curStat = _PKG_HD_INIT;                           //收包状态处于 初始状态，准备接收数据包头【状态机】
    precvbuf = dataHeadInfo;                          //收包我要先收到这里来，因为我要先收包头，所以收数据的buff直接就是dataHeadInfo
    irecvlen = sizeof(comm_pkg_head_t);               //这里指定收数据的长度，这里先要求收包头这么长字节的数据
    
    precvMemPointer   = NULL;                         //既然没new内存，那自然指向的内存地址先给NULL
    iThrowsendCount   = 0;                            //原子的
    psendMemPointer   = NULL;                         //发送数据头指针记录
    events            = 0;                            //epoll事件先给0 
    lastPingTime      = time(NULL);                   //上次ping的时间

    FloodkickLastTime = 0;                            //Flood攻击上次收到包的时间
	FloodAttackCount  = 0;	                          //Flood攻击在该时间内收到包的次数统计
    iSendCount        = 0;                            //发送队列中有的数据条目数，若client只发不收，则可能造成此数过大，依据此数做出踢出处理 
}

//回收回来一个连接的时候做一些事
void eve_connection_s::PutOneToFree()
{
    ++iCurrsequence;   
    if(precvMemPointer != NULL)//我们曾经给这个连接分配过接收数据的内存，则要释放内存
    {        
        CMemory::GetInstance()->FreeMemory(precvMemPointer);
        precvMemPointer = NULL;        
    }
    if(psendMemPointer != NULL) //如果发送数据的缓冲区里有内容，则要释放内存
    {
        CMemory::GetInstance()->FreeMemory(psendMemPointer);
        psendMemPointer = NULL;
    }

    iThrowsendCount = 0;                              //设置不设置感觉都行         
}

//---------------------------------------------------------------
//初始化连接池
void CSocket::initconnection()
{
    eve_connection_t* p_Conn;
    CMemory *p_memory = CMemory::GetInstance();   

    int ilenconnpool = sizeof(eve_connection_t);    
    for(int i = 0; i < m_worker_connections; ++i) 
    {
        p_Conn = (eve_connection_t*)p_memory->AllocMemory(ilenconnpool,true); 
        p_Conn = new(p_Conn) eve_connection_t();  	
        p_Conn->GetOneToUse();
        m_connectionList.push_back(p_Conn);     
        m_freeconnectionList.push_back(p_Conn); 
    } //end for
    m_free_connection_n = m_total_connection_n = m_connectionList.size(); 
    return;
}

//最终回收连接池，释放内存
void CSocket::clearconnection()
{
    eve_connection_t* p_Conn;
	CMemory *p_memory = CMemory::GetInstance();
	
	while(!m_connectionList.empty())
	{
		p_Conn = m_connectionList.front();
		m_connectionList.pop_front(); 
        p_Conn->~eve_connection_t();     //手工调用析构函数
		p_memory->FreeMemory(p_Conn);
	}
}

eve_connection_t* CSocket::eve_get_connection(int isock)
{
    CLock lock(&m_connectionMutex);  
    if(!m_freeconnectionList.empty())
    {
        //有空闲的，自然是从空闲的中摘取
        eve_connection_t* p_Conn = m_freeconnectionList.front(); 
        m_freeconnectionList.pop_front();                         
        p_Conn->GetOneToUse();
        --m_free_connection_n; 
        p_Conn->fd = isock;
        return p_Conn;
    }

    //走到这里，表示没空闲的连接了，那就考虑重新创建一个连接
    CMemory *p_memory = CMemory::GetInstance();
    eve_connection_t* p_Conn = (eve_connection_t*)p_memory->AllocMemory(sizeof(eve_connection_t),true);
    p_Conn = new(p_Conn) eve_connection_t();
    p_Conn->GetOneToUse();
    m_connectionList.push_back(p_Conn); 
    ++m_total_connection_n;             
    p_Conn->fd = isock;
    return p_Conn;
}

//归还参数pConn所代表的连接到到连接池中，注意参数类型是eve_connection_t*
void CSocket::eve_free_connection(eve_connection_t* pConn) 
{
   
    CLock lock(&m_connectionMutex);  
    pConn->PutOneToFree();   
    m_freeconnectionList.push_back(pConn);
    ++m_free_connection_n;

    return;
}


void CSocket::inRecyConnectQueue(eve_connection_t* pConn)
{
    std::list<eve_connection_t*>::iterator pos;
    bool iffind = false;
        
    CLock lock(&m_recyconnqueueMutex); //针对连接回收列表的互斥量，因为线程ServerRecyConnectionThread()也有要用到这个回收列表；

    //如下判断防止连接被多次扔到回收站中来
    for(pos = m_recyconnectionList.begin(); pos != m_recyconnectionList.end(); ++pos)
	{
		if((*pos) == pConn)		
		{	
			iffind = true;
			break;			
		}
	}
    if(iffind == true) //找到了，不必再入了
	{
        return;
    }

    pConn->inRecyTime = time(NULL);   
    ++pConn->iCurrsequence;
    m_recyconnectionList.push_back(pConn);  
    ++m_totol_recyconnection_n;            
    --m_onlineUserCount;                   
    return;
}

//处理连接回收的线程
void* CSocket::ServerRecyConnectionThread(void* threadData)
{
    ThreadItem *pThread = static_cast<ThreadItem*>(threadData);
    CSocket *pSocketObj = pThread->_pThis;
    
    time_t currtime;
    int err;
    std::list<eve_connection_t*>::iterator pos,posend;
    eve_connection_t* p_Conn;
    
    while(1)
    {
        
        usleep(200 * 1000);  
        
        if(pSocketObj->m_totol_recyconnection_n > 0)
        {
            currtime = time(NULL);
            err = pthread_mutex_lock(&pSocketObj->m_recyconnqueueMutex);  
            if(err != 0) eve_log_stderr(err,"CSocket::ServerRecyConnectionThread()中pthread_mutex_lock()失败，返回的错误码为%d!",err);

lblRRTD:
            pos    = pSocketObj->m_recyconnectionList.begin();
			posend = pSocketObj->m_recyconnectionList.end();
            for(; pos != posend; ++pos)
            {
                p_Conn = (*pos);
                if(
                    ( (p_Conn->inRecyTime + pSocketObj->m_RecyConnectionWaitTime) > currtime)  && (g_stopEvent == 0) //如果不是要整个系统退出，你可以continue，否则就得要强制释放
                    )
                {
                    continue; //没到释放的时间
                }    
  
                if(p_Conn->iThrowsendCount > 0)
                {
                    //这确实不应该，打印个日志吧；
                    eve_log_stderr(0,"CSocket::ServerRecyConnectionThread()中到释放时间却发现p_Conn.iThrowsendCount!=0，这个不该发生");
                   
                }

                //流程走到这里，表示可以释放，那我们就开始释放
                --pSocketObj->m_totol_recyconnection_n;          //待释放连接队列大小-1 
                pSocketObj->m_recyconnectionList.erase(pos);   //迭代器已经失效，但pos所指内容在p_Conn里保存着呢
                pSocketObj->eve_free_connection(p_Conn);	   //归还参数pConn所代表的连接到到连接池中
                goto lblRRTD; 
            } //end for
            err = pthread_mutex_unlock(&pSocketObj->m_recyconnqueueMutex); 
            if(err != 0)  eve_log_stderr(err,"CSocket::ServerRecyConnectionThread()pthread_mutex_unlock()失败，返回的错误码为%d!",err);
        } //end if

        if(g_stopEvent == 1) //要退出整个程序，那么肯定要先退出这个循环
        {
            if(pSocketObj->m_totol_recyconnection_n > 0)
            {
                //因为要退出，所以就得硬释放了【不管到没到时间，不管有没有其他不 允许释放的需求，都得硬释放】
                err = pthread_mutex_lock(&pSocketObj->m_recyconnqueueMutex);  
                if(err != 0) eve_log_stderr(err,"CSocket::ServerRecyConnectionThread()中pthread_mutex_lock2()失败，返回的错误码为%d!",err);

        lblRRTD2:
                pos    = pSocketObj->m_recyconnectionList.begin();
			    posend = pSocketObj->m_recyconnectionList.end();
                for(; pos != posend; ++pos)
                {
                    p_Conn = (*pos);
                    --pSocketObj->m_totol_recyconnection_n;        //待释放连接队列大小-1
                    pSocketObj->m_recyconnectionList.erase(pos);   //迭代器已经失效，但pos所指内容在p_Conn里保存着呢
                    pSocketObj->eve_free_connection(p_Conn);	   //归还参数pConn所代表的连接到到连接池中
                    goto lblRRTD2; 
                } //end for
                err = pthread_mutex_unlock(&pSocketObj->m_recyconnqueueMutex); 
                if(err != 0)  eve_log_stderr(err,"CSocket::ServerRecyConnectionThread()pthread_mutex_unlock2()失败，返回的错误码为%d!",err);
            } //end if
            break; //整个程序要退出了，所以break;
        }  //end if
    } //end while    
    
    return (void*)0;
}

void CSocket::eve_close_connection(eve_connection_t* pConn)
{    
    eve_free_connection(pConn); 
    if(pConn->fd != -1)
    {
        close(pConn->fd);
        pConn->fd = -1;
    }    
    return;
}
