#ifndef __HRT_H__
#define __HRT_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLER__

#include <nautilus/multiboot2.h>
#include <nautilus/nautilus.h>


#define UPCALL_MAGIC_ADDRESS 0x0000800df00df00dULL
#define UPCALL_MAGIC_ERROR   0xf00df00d

#define HVM_HCALL_NUM 0xf000

/*
  Calling convention:

64 bit:
  rax = hcall number
  rbx = 0x6464646464646464...
  rcx = 1st arg
  rdx = 2nd arg
  rsi = 3rd arg
  rdi = 4th arg
  r8  = 5th arg
  r9  = 6th arg
  r10 = 7th arg
  r11 = 8th arg

32 bit:
  eax = hcall number
  ebx = 0x32323232
  arguments on stack in C order (first argument is TOS)
     arguments are also 32 bit
*/
#define HCALL64(rc,id,a,b,c,d,e,f,g,h)            \
  asm volatile ("movq %1, %%rax; "            \
        "pushq %%rbx; "               \
        "movq $0x6464646464646464, %%rbx; "   \
        "movq %2, %%rcx; "            \
        "movq %3, %%rdx; "            \
        "movq %4, %%rsi; "            \
        "movq %5, %%rdi; "            \
        "movq %6, %%r8 ; "            \
        "movq %7, %%r9 ; "            \
        "movq %8, %%r10; "            \
        "movq %9, %%r11; "            \
        "vmmcall ;       "            \
        "movq %%rax, %0; "            \
        "popq %%rbx; "                \
        : "=m"(rc)                \
        : "m"(id),                \
                  "m"(a), "m"(b), "m"(c), "m"(d),     \
          "m"(e), "m"(f), "m"(g), "m"(h)      \
        : "%rax","%rcx","%rdx","%rsi","%rdi", \
          "%r8","%r9","%r10","%r11"       \
        )


static inline uint64_t
hvm_hcall (uint64_t cmd, uint64_t a1, uint64_t a2, uint64_t a3,
       uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7)
{
    uint64_t rc;
    uint64_t id=0xf00d;

    HCALL64(rc,id,cmd,a1,a2,a3,a4,a5,a6,a7);

    return rc;
}

int nautilus_hrt_upcall_handler (excp_entry_t * excp, excp_vec_t vec);

int __early_init_hrt (struct naut_info * naut);
int hrt_init_cpus (struct sys_info * sys);
int hrt_init_ioapics (struct sys_info * sys);
int hvm_hrt_init (void);


#endif /* !__ASSEMBLER__! */

#ifdef NAUT_CONFIG_HRT_PS_512G
#define HRT_FLAGS 0x800
#elif defined(NAUT_CONFIG_HRT_PS_1G)
#define HRT_FLAGS 0x400
#elif defined(NAUT_CONFIG_HRT_PS_2M)
#define HRT_FLAGS 0x200
#elif defined(NAUT_CONFIG_HRT_PS_4K)
#define HRT_FLAGS 0x100
#else
#error "Undefined HRT Page Size"
#endif

#ifdef __cplusplus
}
#endif

#endif
