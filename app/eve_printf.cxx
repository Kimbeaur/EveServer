#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>   

#include "eve_global.h"
#include "eve_macro.h"
#include "eve_func.h"


static u_char *eve_sprintf_num(u_char *buf, u_char *last, uint64_t ui64,u_char zero, uintptr_t hexadecimal, uintptr_t width);

//----------------------------------------------------------------------------------------------------------------------

u_char *eve_slprintf(u_char *buf, u_char *last, const char *fmt, ...) 
{
    va_list   args;
    u_char   *p;

    va_start(args, fmt); 
    p = eve_vslprintf(buf, last, fmt, args);
    va_end(args);           
    return p;
}

//----------------------------------------------------------------------------------------------------------------------

u_char * eve_snprintf(u_char *buf, size_t max, const char *fmt, ...)  
{
    u_char   *p;
    va_list   args;

    va_start(args, fmt);
    p = eve_vslprintf(buf, buf + max, fmt, args);
    va_end(args);
    return p;
}

//----------------------------------------------------------------------------------------------------------------------

u_char *eve_vslprintf(u_char *buf, u_char *last,const char *fmt,va_list args)
{
 
    
    u_char     zero;
    uintptr_t  width,sign,hex,frac_width,scale,n;  

    int64_t    i64;   
    uint64_t   ui64;  
    u_char     *p;    
    double     f;     
    uint64_t   frac;  
    

    while (*fmt && buf < last) 
    {
        if (*fmt == '%')  
        {
           
            zero  = (u_char) ((*++fmt == '0') ? '0' : ' ');  
                                                                
            width = 0;                                       
			sign  = 1;    
			hex   = 0;                                       
			frac_width = 0;                                  
            i64 = 0;                                         
            ui64 = 0;                                        
            
            while (*fmt >= '0' && *fmt <= '9')     
            {
                width = width * 10 + (*fmt++ - '0');
            }

            for ( ;; ) 
            {
                switch (*fmt)  //处理一些%之后的特殊字符
                {
                case 'u':       //%u，这个u表示无符号
                    sign = 0;   //标记这是个无符号数
                    fmt++;      //往后走一个字符
                    continue;   //回到for继续判断

                case 'X':       //%X，X表示十六进制，并且十六进制中的A-F以大写字母显示，不要单独使用，一般是%Xd
                    hex = 2;    //标记以大写字母显示十六进制中的A-F
                    sign = 0;
                    fmt++;
                    continue;
                case 'x':       //%x，x表示十六进制，并且十六进制中的a-f以小写字母显示，不要单独使用，一般是%xd
                    hex = 1;    //标记以小写字母显示十六进制中的a-f
                    sign = 0;
                    fmt++;
                    continue;

                case '.':       //其后边必须跟个数字，必须与%f配合使用，形如 %.10f：表示转换浮点数时小数部分的位数，比如%.10f表示转换浮点数时，小数点后必须保证10位数字，不足10位则用0来填补；
                    fmt++;      //往后走一个字符，后边这个字符肯定是0-9之间，因为%.要求接个数字先 
                    while(*fmt >= '0' && *fmt <= '9')  //如果是数字，一直循环，这个循环最终就能把诸如%.10f中的10提取出来
                    {
                        frac_width = frac_width * 10 + (*fmt++ - '0'); 
                    } //end while(*fmt >= '0' && *fmt <= '9') 
                    break;

                default:
                    break;                
                } //end switch (*fmt) 
                break;
            } //end for ( ;; )

            switch (*fmt) 
            {
            case '%': 
                *buf++ = '%';
                fmt++;
                continue;
        
            case 'd': 
                if (sign)  
                {
                    i64 = (int64_t) va_arg(args, int);  
                }
                else 
                {
                    ui64 = (uint64_t) va_arg(args, u_int);    
                }
                break;  
             case 'i': 
                if (sign) 
                {
                    i64 = (int64_t) va_arg(args, intptr_t);
                } 
                else 
                {
                    ui64 = (uint64_t) va_arg(args, uintptr_t);
                }

                break;    

            case 'L':  //转换int64j型数据，如果用%uL，则转换的数据类型是uint64 t
                if (sign)
                {
                    i64 = va_arg(args, int64_t);
                } 
                else 
                {
                    ui64 = va_arg(args, uint64_t);
                }
                break;

            case 'p':  
                ui64 = (uintptr_t) va_arg(args, void *); 
                hex = 2;    //标记以大写字母显示十六进制中的A-F
                sign = 0;   //标记这是个无符号数
                zero = '0'; //前边0填充
                width = 2 * sizeof(void *);
                break;

            case 's': //一般用于显示字符串
                p = va_arg(args, u_char *); //va_arg():遍历可变参数，var_arg的第二个参数表示遍历的这个可变的参数的类型

                while (*p && buf < last)  //没遇到字符串结束标记，并且buf值够装得下这个参数
                {
                    *buf++ = *p++;  //那就装，比如  "%s"    ，   "abcdefg"，那abcdefg都被装进来
                }
                
                fmt++;
                continue; //重新从while开始执行 

            case 'P':  //转换一个pid_t类型
                i64 = (int64_t) va_arg(args, pid_t);
                sign = 1;
                break;

            case 'f': //一般 用于显示double类型数据，如果要显示小数部分，则要形如 %.5f  
                f = va_arg(args, double);  //va_arg():遍历可变参数，var_arg的第二个参数表示遍历的这个可变的参数的类型
                if (f < 0)  //负数的处理
                {
                    *buf++ = '-'; //单独搞个负号出来
                    f = -f; //那这里f应该是正数了!
                }
                //走到这里保证f肯定 >= 0【不为负数】
                ui64 = (int64_t) f; //正整数部分给到ui64里
                frac = 0;

                //如果要求小数点后显示多少位小数
                if (frac_width) //如果是%d.2f，那么frac_width就会是这里的2
                {
                    scale = 1;  //缩放从1开始
                    for (n = frac_width; n; n--) 
                    {
                        scale *= 10; //这可能溢出哦
                    }

                    frac = (uint64_t) ((f - (double) ui64) * scale + 0.5);   //取得保留的那些小数位数，【比如  %.2f   ，对应的参数是12.537，取得的就是小数点后的2位四舍五入，也就是54】
                                                                             //如果是"%.6f", 21.378，那么这里frac = 378000

                    if (frac == scale)   //进位，比如    %.2f ，对应的参数是12.999，那么  = (uint64_t) (0.999 * 100 + 0.5)  = (uint64_t) (99.9 + 0.5) = (uint64_t) (100.4) = 100
                                          //而此时scale == 100，两者正好相等
                    {
                        ui64++;    //正整数部分进位
                        frac = 0;  //小数部分归0
                    }
                } //end if (frac_width)

                //正整数部分，先显示出来
                buf = eve_sprintf_num(buf, last, ui64, zero, 0, width); //把一个数字 比如“1234567”弄到buffer中显示

                if (frac_width) //指定了显示多少位小数
                {
                    if (buf < last) 
                    {
                        *buf++ = '.'; //因为指定显示多少位小数，先把小数点增加进来
                    }
                    buf = eve_sprintf_num(buf, last, frac, '0', 0, frac_width); //frac这里是小数部分，显示出来，不够的，前边填充'0'字符
                }
                fmt++;
                continue;  //重新从while开始执行

            //..................................
            //................其他格式符，逐步完善
            //..................................

            default:
                *buf++ = *fmt++; //往下移动一个字符
                continue; //注意这里不break，而是continue;而这个continue其实是continue到外层的while去了，也就是流程重新从while开头开始执行;
            } //end switch (*fmt) 
            
            //显示%d的，会走下来，其他走下来的格式日后逐步完善......

            //统一把显示的数字都保存到 ui64 里去；
            if (sign) //显示的是有符号数
            {
                if (i64 < 0)  //这可能是和%d格式对应的要显示的数字
                {
                    *buf++ = '-';  //小于0，自然要把负号先显示出来
                    ui64 = (uint64_t) -i64; //变成无符号数（正数）
                }
                else //显示正数
                {
                    ui64 = (uint64_t) i64;
                }
            } //end if (sign) 

            //把一个数字 比如“1234567”弄到buffer中显示，如果是要求10位，则前边会填充3个空格比如“   1234567”
            //注意第5个参数hex，是否以16进制显示，比如如果你是想以16进制显示一个数字则可以%Xd或者%xd，此时hex = 2或者1
            buf = eve_sprintf_num(buf, last, ui64, zero, hex, width); 
            fmt++;
        }
        else  //当成正常字符，源【fmt】拷贝到目标【buf】里
        {
            //用fmt当前指向的字符赋给buf当前指向的位置，然后buf往前走一个字符位置，fmt当前走一个字符位置
            *buf++ = *fmt++;   //*和++优先级相同，结合性从右到左，所以先求的是buf++以及fmt++，但++是先用后加；
        } //end if (*fmt == '%') 
    }  //end while (*fmt && buf < last) 
    
    return buf;
}

static u_char * eve_sprintf_num(u_char *buf, u_char *last, uint64_t ui64, u_char zero, uintptr_t hexadecimal, uintptr_t width)
{
    //temp[21]
    u_char      *p, temp[EVE_INT64_LEN + 1];   //#define EVE_INT64_LEN   (sizeof("-9223372036854775808") - 1)     = 20   ，注意这里是sizeof是包括末尾的\0，不是strlen；             
    size_t      len;
    uint32_t    ui32;

    static u_char   hex[] = "0123456789abcdef";  //跟把一个10进制数显示成16进制有关，换句话说和  %xd格式符有关，显示的16进制数中a-f小写
    static u_char   HEX[] = "0123456789ABCDEF";  //跟把一个10进制数显示成16进制有关，换句话说和  %Xd格式符有关，显示的16进制数中A-F大写

    p = temp + EVE_INT64_LEN; //EVE_INT64_LEN = 20,所以 p指向的是temp[20]那个位置，也就是数组最后一个元素位置

    if (hexadecimal == 0)  
    {
        if (ui64 <= (uint64_t) EVE_MAX_UINT32_VALUE)   //EVE_MAX_UINT32_VALUE :最大的32位无符号数：十进制是‭4294967295‬
        {
            ui32 = (uint32_t) ui64; //能保存下
            do  //这个循环能够把诸如 7654321这个数字保存成：temp[13]=7,temp[14]=6,temp[15]=5,temp[16]=4,temp[17]=3,temp[18]=2,temp[19]=1
                  //而且的包括temp[0..12]以及temp[20]都是不确定的值
            {
                *--p = (u_char) (ui32 % 10 + '0');  //把屁股后边这个数字拿出来往数组里装，并且是倒着装：屁股后的也往数组下标大的位置装；
            }
            while (ui32 /= 10); //每次缩小10倍等于去掉屁股后边这个数字
        }
        else
        {
            do 
            {
                *--p = (u_char) (ui64 % 10 + '0');
            } while (ui64 /= 10); 
        }
    }
    else if (hexadecimal == 1)  
	{
       
        do 
        {            
           *--p = hex[(uint32_t) (ui64 & 0xf)];    
        } while (ui64 >>= 4);    
    } 
    else 
	{ 
        
        do 
        { 
            *--p = HEX[(uint32_t) (ui64 & 0xf)];
        } while (ui64 >>= 4);
    }

    len = (temp + EVE_INT64_LEN) - p;  

    while (len++ < width && buf < last)     {
        *buf++ = zero;  
    }
    
    len = (temp + EVE_INT64_LEN) - p; 
   
    if((buf + len) >= last)   
    {
        len = last - buf; 
    }

    return eve_cpymem(buf, p, len);
}

