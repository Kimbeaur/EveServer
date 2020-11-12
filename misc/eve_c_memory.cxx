
//和 内存分配 有关的函数放这里
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eve_c_memory.h"

//类静态成员赋值
CMemory *CMemory::m_instance = NULL;


void *CMemory::AllocMemory(int memCount,bool ifmemset)
{	    
	void *tmpData = (void *)new char[memCount]; 
    if(ifmemset) //要求内存清0
    {
	    memset(tmpData,0,memCount);
    }
	return tmpData;
}

//内存释放函数
void CMemory::FreeMemory(void *point)
{		
    delete [] ((char *)point);
} 

