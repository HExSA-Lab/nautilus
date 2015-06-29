#ifndef __HRT_H__
#define __HRT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/multiboot2.h>
#include <nautilus/nautilus.h>

#define HVM_HCALL_NUM 0xf000

static inline uint64_t 
hvm_hcall (uint64_t x)
{
    uint64_t rc;

    asm volatile ("movq $0xf000, %%rax; "            \
                 "pushq %%rbx; "                     \
                 "movq %1, %%rbx;"                   \
                 "vmmcall ;       "                  \
                 "movq %%rax, %0; "                  \
                 "popq %%rbx; "
                 : "=m"(rc)
                 : "m"(x)
                  : "%rax" );

    return rc;
}


int hrt_init_cpus (struct sys_info * sys);
int hrt_init_ioapics (struct sys_info * sys);
int hvm_hrt_init (void);


#ifdef __cplusplus
}
#endif

#endif
