//函数声明放在这个头文件里-------------------------------------------

#ifndef __EVE_FUNC_H__
#define __EVE_FUNC_H__

//字符串相关函数
void   Rtrim(char *string);
void   Ltrim(char *string);

//设置可执行程序标题相关函数
void   eve_init_setproctitle();
void   eve_setproctitle(const char *title);

//和日志，打印输出有关
void   eve_log_init();
void   eve_log_stderr(int err, const char *fmt, ...);
void   eve_log_error_core(int level,  int err, const char *fmt, ...);
u_char *eve_log_errno(u_char *buf, u_char *last, int err);
u_char *eve_snprintf(u_char *buf, size_t max, const char *fmt, ...);
u_char *eve_slprintf(u_char *buf, u_char *last, const char *fmt, ...);
u_char *eve_vslprintf(u_char *buf, u_char *last,const char *fmt,va_list args);

//和信号/主流程相关相关
int    eve_init_signals();
void   eve_master_process_cycle();
int    eve_daemon();
void   eve_process_events_and_timers();


#endif  