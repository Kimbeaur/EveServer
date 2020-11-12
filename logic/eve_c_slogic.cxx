
//和网络以及逻辑处理 有关的函数放这里
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>    
#include <stdarg.h>    
#include <unistd.h>    
#include <sys/time.h>  
#include <time.h>      
#include <fcntl.h>     
#include <errno.h>     
#include <sys/ioctl.h> 
#include <arpa/inet.h>
#include <pthread.h>   
#include "eve_c_conf.h"
#include "eve_macro.h"
#include "eve_global.h"
#include "eve_func.h"
//#include "eve_c_socket.h"
#include "eve_c_memory.h"
#include "eve_c_crc32.h"
#include "eve_c_slogic.h"  
#include "eve_logiccomm.h"  
#include "eve_c_lockmutex.h"  

//定义成员函数指针
typedef bool (CLogicSocket::*handler)(  eve_connection_t* pConn,      //连接池中连接的指针
                                        msg_head_t* pMsgHeader,  //消息头指针
                                        char *pPkgBody,                 //包体指针
                                        unsigned short iBodyLength);    //包体长度

//用来保存 成员函数指针 的这么个数组
static const handler statusHandler[] = 
{
    //数组前5个元素，保留，以备将来增加一些基本服务器功能
    &CLogicSocket::_HandlePing,                             //【0】：心跳包的实现
    NULL,                                                   //【1】：下标从0开始
    NULL,                                                   //【2】：下标从0开始
    NULL,                                                   //【3】：下标从0开始
    NULL,                                                   //【4】：下标从0开始
 
    //开始处理具体的业务逻辑
    &CLogicSocket::_HandleRegister,                         //【5】：实现具体的注册功能
    &CLogicSocket::_HandleLogIn,                            //【6】：实现具体的登录功能
    //......其他待扩展，


};
#define AUTH_TOTAL_COMMANDS sizeof(statusHandler)/sizeof(handler) 

//构造函数
CLogicSocket::CLogicSocket()
{

}
//析构函数
CLogicSocket::~CLogicSocket()
{

}
//初始化函数fork()子进程之前干这个事
//成功返回true，失败返回false
bool CLogicSocket::Initialize()
{
    bool bParentInit = CSocket::Initialize();  //调用父类的同名函数
    return bParentInit;
}

//处理收到的数据包，由线程池来调用本函数，本函数是一个单独的线程；
//pMsgBuf：消息头 + 包头 + 包体 ：自解释；
void CLogicSocket::threadRecvProcFunc(char *pMsgBuf)
{          
    msg_head_t* pMsgHeader = (msg_head_t*)pMsgBuf;                  //消息头
    comm_pkg_head_t*  pPkgHeader = (comm_pkg_head_t*)(pMsgBuf+m_iLenMsgHeader); //包头
    void  *pPkgBody;                                                              //指向包体的指针
    unsigned short pkglen = ntohs(pPkgHeader->pkgLen);                            //客户端指明的包宽度【包头+包体】

    if(m_iLenPkgHeader == pkglen)
    {
        //没有包体，只有包头
		if(pPkgHeader->crc32 != 0) //只有包头的crc值给0
		{
			return; //crc错，直接丢弃
		}
		pPkgBody = NULL;
    }
    else 
	{
        //有包体，走到这里
		pPkgHeader->crc32 = ntohl(pPkgHeader->crc32);		          //针对4字节的数据，网络序转主机序
		pPkgBody = (void *)(pMsgBuf+m_iLenMsgHeader+m_iLenPkgHeader); //跳过消息头 以及 包头 ，指向包体

        //eve_log_stderr(0,"CLogicSocket::threadRecvProcFunc()中收到包的crc值为%d!",pPkgHeader->crc32);

		//计算crc值判断包的完整性        
		int calccrc = CCRC32::GetInstance()->Get_CRC((unsigned char *)pPkgBody,pkglen-m_iLenPkgHeader); //计算纯包体的crc值
		if(calccrc != pPkgHeader->crc32) //服务器端根据包体计算crc值，和客户端传递过来的包头中的crc32信息比较
		{
            eve_log_stderr(0,"CLogicSocket::threadRecvProcFunc()中CRC错误[服务器:%d/客户端:%d]，丢弃数据!",calccrc,pPkgHeader->crc32);    //正式代码中可以干掉这个信息
			return; //crc错，直接丢弃
		}
       
	}

     	
    unsigned short imsgCode = ntohs(pPkgHeader->msgCode); //消息代码拿出来
    eve_connection_t* p_Conn = pMsgHeader->pConn;        //消息头中藏着连接池中连接的指针

    //我们要做一些判断
    //(1)如果从收到客户端发送来的包，到服务器释放一个线程池中的线程处理该包的过程中，客户端断开了，那显然，这种收到的包我们就不必处理了；    
    if(p_Conn->iCurrsequence != pMsgHeader->iCurrsequence)   //该连接池中连接以被其他tcp连接【其他socket】占用，这说明原来的 客户端和本服务器的连接断了，这种包直接丢弃不理
    {
        return; //丢弃不理这种包了【客户端断开了】
    }

    //(2)判断消息码是正确的，防止客户端恶意侵害我们服务器，发送一个不在我们服务器处理范围内的消息码
	if(imsgCode >= AUTH_TOTAL_COMMANDS) //无符号数不可能<0
    {
        eve_log_stderr(0,"CLogicSocket::threadRecvProcFunc()中imsgCode=%d消息码不对!",imsgCode); //这种有恶意倾向或者错误倾向的包，希望打印出来看看是谁干的
        return; //丢弃不理这种包【恶意包或者错误包】
    }

    
    //(3)有对应的消息处理函数吗
    if(statusHandler[imsgCode] == NULL) //这种用imsgCode的方式可以使查找要执行的成员函数效率特别高
    {
        eve_log_stderr(0,"CLogicSocket::threadRecvProcFunc()中imsgCode=%d消息码找不到对应的处理函数!",imsgCode); //这种有恶意倾向或者错误倾向的包，希望打印出来看看是谁干的
        return;  //没有相关的处理函数
    }

    //(4)调用消息码对应的成员函数来处理
    (this->*statusHandler[imsgCode])(p_Conn,pMsgHeader,(char *)pPkgBody,pkglen-m_iLenPkgHeader);
    return;	
}

//心跳包检测时间到，该去检测心跳包是否超时的事宜，本函数是子类函数，实现具体的判断动作
void CLogicSocket::procPingTimeOutChecking(msg_head_t* tmpmsg,time_t cur_time)
{
    CMemory *p_memory = CMemory::GetInstance();

    if(tmpmsg->iCurrsequence == tmpmsg->pConn->iCurrsequence) //此连接没断
    {
        eve_connection_t* p_Conn = tmpmsg->pConn;

        if(/*m_ifkickTimeCount == 1 && */m_ifTimeOutKick == 1)  //能调用到本函数第一个条件肯定成立，所以第一个条件加不加无所谓，主要是第二个条件
        {
            //到时间直接踢出去的需求
            zdClosesocketProc(p_Conn); 
        }            
        else if( (cur_time - p_Conn->lastPingTime ) > (m_iWaitTime*3+10) ) //超时踢的判断标准就是 每次检查的时间间隔*3，超过这个时间没发送心跳包，就踢【大家可以根据实际情况自由设定】
        {
            zdClosesocketProc(p_Conn); 
        }   
             
        p_memory->FreeMemory(tmpmsg);//内存要释放
    }
    else //此连接断了
    {
        p_memory->FreeMemory(tmpmsg);//内存要释放
    }
    return;
}

//发送没有包体的数据包给客户端
void CLogicSocket::SendNoBodyPkgToClient(msg_head_t* pMsgHeader,unsigned short iMsgCode)
{
    CMemory  *p_memory = CMemory::GetInstance();

    char *p_sendbuf = (char *)p_memory->AllocMemory(m_iLenMsgHeader+m_iLenPkgHeader,false);
    char *p_tmpbuf = p_sendbuf;
    
	memcpy(p_tmpbuf,pMsgHeader,m_iLenMsgHeader);
	p_tmpbuf += m_iLenMsgHeader;

    comm_pkg_head_t* pPkgHeader = (comm_pkg_head_t*)p_tmpbuf;	  //指向的是我要发送出去的包的包头	
    pPkgHeader->msgCode = htons(iMsgCode);	
    pPkgHeader->pkgLen = htons(m_iLenPkgHeader); 
	pPkgHeader->crc32 = 0;		
    msgSend(p_sendbuf);
    return;
}

//----------------------------------------------------------------------------------------------------------
//处理各种业务逻辑
bool CLogicSocket::_HandleRegister(eve_connection_t* pConn,msg_head_t* pMsgHeader,char *pPkgBody,unsigned short iBodyLength)
{
    //eve_log_stderr(0,"执行了CLogicSocket::_HandleRegister()!");
    
    //(1)首先判断包体的合法性
    if(pPkgBody == NULL) //具体看客户端服务器约定，如果约定这个命令[msgCode]必须带包体，那么如果不带包体，就认为是恶意包，直接不处理    
    {        
        return false;
    }
		    
    int iRecvLen = sizeof(Register_t); 
    if(iRecvLen != iBodyLength) //发送过来的结构大小不对，认为是恶意包，直接不处理
    {     
        return false; 
    }

    CLock lock(&pConn->logicPorcMutex); //凡是和本用户有关的访问都互斥
    
    //(3)取得了整个发送过来的数据
    Register_t* p_RecvInfo = (Register_t*)pPkgBody; 
    p_RecvInfo->iType = ntohl(p_RecvInfo->iType);          //所有数值型,short,int,long,uint64_t,int64_t这种大家都不要忘记传输之前主机网络序，收到后网络转主机序
    p_RecvInfo->username[sizeof(p_RecvInfo->username)-1]=0;//这非常关键，防止客户端发送过来畸形包，导致服务器直接使用这个数据出现错误。 
    p_RecvInfo->password[sizeof(p_RecvInfo->password)-1]=0;//这非常关键，防止客户端发送过来畸形包，导致服务器直接使用这个数据出现错误。 

	comm_pkg_head_t* pPkgHeader;	
	CMemory  *p_memory = CMemory::GetInstance();
	CCRC32   *p_crc32 = CCRC32::GetInstance();
    int iSendLen = sizeof(Register_t);  
    //a)分配要发送出去的包的内存

    //iSendLen = 65000; //unsigned最大也就是这个值
    char *p_sendbuf = (char *)p_memory->AllocMemory(m_iLenMsgHeader+m_iLenPkgHeader+iSendLen,false);//准备发送的格式，这里是 消息头+包头+包体
    //b)填充消息头
    memcpy(p_sendbuf,pMsgHeader,m_iLenMsgHeader);                   //消息头直接拷贝到这里来
    //c)填充包头
    pPkgHeader = (comm_pkg_head_t*)(p_sendbuf+m_iLenMsgHeader);    //指向包头
    pPkgHeader->msgCode = _CMD_REGISTER;	                        //消息代码，可以统一在eve_logiccomm.h中定义
    pPkgHeader->msgCode = htons(pPkgHeader->msgCode);	            //htons主机序转网络序 
    pPkgHeader->pkgLen  = htons(m_iLenPkgHeader + iSendLen);        //整个包的尺寸【包头+包体尺寸】 
    //d)填充包体
    Register_t* p_sendInfo = (Register_t*)(p_sendbuf+m_iLenMsgHeader+m_iLenPkgHeader);	//跳过消息头，跳过包头，就是包体了
    //。。。。。这里根据需要，填充要发回给客户端的内容,int类型要使用htonl()转，short类型要使用htons()转；
    
    //e)包体内容全部确定好后，计算包体的crc32值
    pPkgHeader->crc32   = p_crc32->Get_CRC((unsigned char *)p_sendInfo,iSendLen);
    pPkgHeader->crc32   = htonl(pPkgHeader->crc32);		

    //f)发送数据包
    msgSend(p_sendbuf);
  
    return true;
}
bool CLogicSocket::_HandleLogIn(eve_connection_t* pConn,msg_head_t* pMsgHeader,char *pPkgBody,unsigned short iBodyLength)
{    
    if(pPkgBody == NULL)
    {        
        return false;
    }		    
    int iRecvLen = sizeof(Login_t); 
    if(iRecvLen != iBodyLength) 
    {     
        return false; 
    }
    CLock lock(&pConn->logicPorcMutex);
        
    Login_t* p_RecvInfo = (Login_t*)pPkgBody;     
    p_RecvInfo->username[sizeof(p_RecvInfo->username)-1]=0;
    p_RecvInfo->password[sizeof(p_RecvInfo->password)-1]=0;

	comm_pkg_head_t* pPkgHeader;	
	CMemory  *p_memory = CMemory::GetInstance();
	CCRC32   *p_crc32 = CCRC32::GetInstance();

    int iSendLen = sizeof(Login_t);  
    char *p_sendbuf = (char *)p_memory->AllocMemory(m_iLenMsgHeader+m_iLenPkgHeader+iSendLen,false);    
    memcpy(p_sendbuf,pMsgHeader,m_iLenMsgHeader);    
    pPkgHeader = (comm_pkg_head_t*)(p_sendbuf+m_iLenMsgHeader);
    pPkgHeader->msgCode = _CMD_LOGIN;
    pPkgHeader->msgCode = htons(pPkgHeader->msgCode);
    pPkgHeader->pkgLen  = htons(m_iLenPkgHeader + iSendLen);    
    Login_t* p_sendInfo = (Login_t*)(p_sendbuf+m_iLenMsgHeader+m_iLenPkgHeader);
    pPkgHeader->crc32   = p_crc32->Get_CRC((unsigned char *)p_sendInfo,iSendLen);
    pPkgHeader->crc32   = htonl(pPkgHeader->crc32);		   
    //eve_log_stderr(0,"成功收到了登录并返回结果！");
    msgSend(p_sendbuf);
    return true;
}

//接收并处理客户端发送过来的ping包
bool CLogicSocket::_HandlePing(eve_connection_t* pConn,msg_head_t* pMsgHeader,char *pPkgBody,unsigned short iBodyLength)
{
    //心跳包要求没有包体；
    if(iBodyLength != 0)  //有包体则认为是 非法包
		return false; 

    CLock lock(&pConn->logicPorcMutex); //凡是和本用户有关的访问都考虑用互斥，以免该用户同时发送过来两个命令达到各种作弊目的
    pConn->lastPingTime = time(NULL);   //更新该变量

    //服务器也发送 一个只有包头的数据包给客户端，作为返回的数据
    SendNoBodyPkgToClient(pMsgHeader,_CMD_PING);

    //eve_log_stderr(0,"成功收到了心跳包并返回结果！");
    return true;
}