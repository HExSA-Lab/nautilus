#ifndef __BACKTRACE_H__
#define __BACKTRACE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define _TRACE_TAG printk("[----------------- Call Trace ------------------]\n")

#define backtrace_here() _TRACE_TAG; \
    __do_backtrace(__builtin_frame_address(0), 0)

#define backtrace(x) _TRACE_TAG; \
    __do_backtrace((void*)(x), 0)

    


struct nk_regs;
void __do_backtrace(void **, unsigned);
void nk_dump_mem(void *, ulong_t);
void nk_stack_dump(ulong_t);
void nk_print_regs(struct nk_regs * r);


#ifdef __cplusplus
}
#endif


#endif
