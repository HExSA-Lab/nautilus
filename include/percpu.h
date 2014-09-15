#ifndef __PER_CPU_H__
#define __PER_CPU_H__

#include <stddef.h>

struct cpu;

#define __movop_1 movb
#define __movop_2 movw
#define __movop_4 movl
#define __movop_8 movq
#define __cmpop_1 cmpxchgb
#define __cmpop_2 cmpxchgw
#define __cmpop_4 cmpxchgl
#define __cmpop_8 cmpxchgq

#define __areg_1 %%al
#define __areg_2 %%ax
#define __areg_4 %%eax
#define __areg_8 %%rax

#define __xpand_str(x) __stringify(x)
#define __stringify(x) #x
#define __percpu_seg %%gs


#define __per_cpu_get(var, n)                                        \
    ({                                                               \
    typeof(((struct cpu*)0)->var) __r;                             \
    asm volatile (__xpand_str(__movop_##n) "  " __xpand_str(__percpu_seg)":%p[_o], %[_r]"  \
                  : [_r] "=r" (__r)                                  \
                  : [_o] "n" (offsetof(struct cpu, var)));           \
    __r;                                                             \
    })
    

/* KCH NOTE: var needs to be in the cpu struct */
#define per_cpu_get(var)           \
    ({                      \
     typeof(((struct cpu*)0)->var) __r; \
     switch(sizeof(__r)) { \
        case 1: \
            __r = (typeof(__r)) __per_cpu_get(var,1); \
            break;\
        case 2:\
            __r = (typeof(__r)) __per_cpu_get(var,2); \
            break; \
        case 4: \
            __r = (typeof(__r)) __per_cpu_get(var,4); \
            break; \
        case 8: \
            __r = (typeof(__r)) __per_cpu_get(var,8);\
            break;\
        default: \
            ERROR_PRINT("undefined op size in per_cpu_var\n");\
        } \
        __r; })


#define __per_cpu_put(var, newval, n) \
do {\
     asm volatile (__xpand_str(__movop_##n) " "  __xpand_str(__percpu_seg)":%p[_o], " __xpand_str(__areg_##n) ";\n" \
                   "1:\n\t " __xpand_str(__cmpop_##n) " %[new],"  __xpand_str(__percpu_seg) ":%p[_o];\n"     \
                   "\tjnz 1b;\n" \
                   : /* no outputs */ \
                   : [_o] "n" (offsetof(struct cpu, var)), \
                     [new] "r" (newval) \
                   : "rax", "memory"); \
} while (0)

#define per_cpu_put(var, newval)                                      \
do { \
     typeof(&((struct cpu*)0)->var) __r; \
     switch (sizeof(__r)) { \
         case 1: \
            __per_cpu_put(var,newval,1); \
            break; \
         case 2: \
            __per_cpu_put(var,newval,2); \
            break; \
         case 4: \
            __per_cpu_put(var,newval,4); \
            break; \
         case 8: \
            __per_cpu_put(var,newval,8); \
            break; \
        default: \
            ERROR_PRINT("undefined op size in per_cpu_put\n"); \
    } \
     } while (0)

    
#define my_cpu_id() per_cpu_get(id)

/*
static inline struct cpu*
get_cpu (void)
{
    struct cpu * ret;
    asm volatile ("movq %%gs:0, %[ret]" : [ret] "=r" (ret));
    return ret;
}
*/

#endif /* !__PER_CPU_H__ */
