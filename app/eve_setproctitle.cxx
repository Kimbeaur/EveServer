//和设置课执行程序标题（名称）相关的放这里 
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  //env
#include <string.h>

#include "eve_global.h"

//设置可执行程序标题相关函数：分配内存，并且把环境变量拷贝到新内存中来
void eve_init_setproctitle()
{   
    //这里无需判断penvmen == NULL,有些编译器new会返回NULL，有些会报异常，但不管怎样，如果在重要的地方new失败了，你无法收场，让程序失控崩溃，助你发现问题为好； 
    gp_envmem = new char[g_envneedmem]; 
    memset(gp_envmem,0,g_envneedmem);  //内存要清空防止出现问题

    char *ptmp = gp_envmem;
    //把原来的内存内容搬到新地方来
    for (int i = 0; environ[i]; i++) 
    {
        size_t size = strlen(environ[i])+1 ; //不要拉下+1，否则内存全乱套了，因为strlen是不包括字符串末尾的\0的
        strcpy(ptmp,environ[i]);      //把原环境变量内容拷贝到新地方【新内存】
        environ[i] = ptmp;            //然后还要让新环境变量指向这段新内存
        ptmp += size;
    }
    return;
}

//设置可执行程序标题
void eve_setproctitle(const char *title)
{

    size_t ititlelen = strlen(title); 

  
    size_t esy = g_argvneedmem + g_envneedmem; //argv和environ内存总和
    if( esy <= ititlelen)
    {
        
        return;
    }

    g_os_argv[1] = NULL;  
    char *ptmp = g_os_argv[0]; //让ptmp指向g_os_argv所指向的内存
    strcpy(ptmp,title);
    ptmp += ititlelen; //跳过标题

    
    size_t cha = esy - ititlelen;  
    memset(ptmp,0,cha);
    return;
}