#ifndef __BACKTRACE_H__
#define __BACKTRACE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define backtrace() printk("+++ BEGIN BACKTRACE +++\n"); __do_backtrace(__builtin_frame_address(0), 0)

void __do_backtrace(void **, unsigned);
void print_gprs(void);


#ifdef __cplusplus
}
#endif


#endif
