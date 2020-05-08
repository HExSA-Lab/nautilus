/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __CPU_H__
#define __CPU_H__

#ifdef __cplusplus 
extern "C" {
#endif

#include <nautilus/naut_types.h>

ulong_t nk_detect_cpu_freq(uint32_t);
uint8_t nk_is_amd(void);
uint8_t nk_is_intel(void);


#define RFLAGS_CF   (1 << 0)
#define RFLAGS_PF   (1 << 2)
#define RFLAGS_AF   (1 << 4)
#define RFLAGS_ZF   (1 << 6)
#define RFLAGS_SF   (1 << 7)
#define RFLAGS_TF   (1 << 8)
#define RFLAGS_IF   (1 << 9)
#define RFLAGS_DF   (1 << 10)
#define RFLAGS_OF   (1 << 11)
#define RFLAGS_IOPL (3 << 12)
#define RFLAGS_VM   ((1 << 17) | RFLAGS_IOPL)
#define RFLAGS_VIF  (1 << 19)
#define RFLAGS_VIP  (1 << 20)

#define CR0_PE 1
#define CR0_MP 2
#define CR0_EM (1<<2)
#define CR0_TS (1<<3)
#define CR0_NE (1<<5)
#define CR0_WP (1<<16)
#define CR0_AM (1<<18)
#define CR0_NW (1<<29)
#define CR0_CD (1<<30)
#define CR0_PG (1<<31)

#define CR4_VME        1
#define CR4_PVI        2
#define CR4_TSD        (1<<2)
#define CR4_DE         (1<<3)
#define CR4_PSE        (1<<4)
#define CR4_PAE        (1<<5)
#define CR4_MCE        (1<<6)
#define CR4_PGE        (1<<7)
#define CR4_PCE        (1<<8)
#define CR4_OSFXSR     (1<<9)
#define CR4_OSXMMEXCPT (1<<10)
#define CR4_VMXE       (1<<13)
#define CR4_XMXE       (1<<14)
#define CR4_FSGSBASE   (1<<16)
#define CR4_PCIDE      (1<<17)
#define CR4_OSXSAVE    (1<<18)
#define CR4_SMEP       (1<<20)

struct nk_regs {
    ulong_t r15;
    ulong_t r14;
    ulong_t r13;
    ulong_t r12;
    ulong_t r11;
    ulong_t r10;
    ulong_t r9;
    ulong_t r8;
    ulong_t rbp;
    ulong_t rdi;
    ulong_t rsi;
    ulong_t rdx;
    ulong_t rcx;
    ulong_t rbx;
    ulong_t rax;
    ulong_t vector;
    ulong_t err_code;
    ulong_t rip;
    ulong_t cs;
    ulong_t rflags;
    ulong_t rsp;
    ulong_t ss;
};


#define PAUSE_WHILE(x) \
    while ((x)) { \
        asm volatile("pause"); \
    } 

#ifndef NAUT_CONFIG_XEON_PHI
#define mbarrier()    asm volatile("mfence":::"memory")
#else
#define mbarrier()
#endif

#define BARRIER_WHILE(x) \
    while ((x)) { \
        mbarrier(); \
    }


static inline ulong_t 
read_cr0 (void)
{
    ulong_t ret;
    asm volatile ("mov %%cr0, %0" : "=r"(ret));
    return ret;
}


static inline void
write_cr0 (ulong_t data)
{
    asm volatile ("mov %0, %%cr0" : : "r"(data));
}


static inline ulong_t
read_cr2 (void)
{
    ulong_t ret;
    asm volatile ("mov %%cr2, %0" : "=r"(ret));
    return ret;
}


static inline void
write_cr2 (ulong_t data)
{
    asm volatile ("mov %0, %%cr2" : : "r"(data));
}


static inline ulong_t
read_cr3 (void)
{
    ulong_t ret;
    asm volatile ("mov %%cr3, %0" : "=r"(ret));
    return ret;
}


static inline void
write_cr3 (ulong_t data)
{
    asm volatile ("mov %0, %%cr3" : : "r"(data));
}


static inline ulong_t
read_cr4 (void)
{
    ulong_t ret;
    asm volatile ("mov %%cr4, %0" : "=r"(ret));
    return ret;
}


static inline void
write_cr4 (ulong_t data)
{
    asm volatile ("mov %0, %%cr4" : : "r"(data));
}


static inline ulong_t
read_cr8 (void)
{
    ulong_t ret;
    asm volatile ("mov %%cr8, %0" : "=r"(ret));
    return ret;
}


static inline void
write_cr8 (ulong_t data)
{
    asm volatile ("mov %0, %%cr8" : : "r"(data));
}


static inline uint8_t
inb (uint16_t port)
{
    uint8_t ret;
    asm volatile ("inb %1, %0":"=a" (ret):"dN" (port));
    return ret;
}


static inline uint16_t 
inw (uint16_t port)
{
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a" (ret):"dN" (port));
    return ret;
}


static inline uint32_t 
inl (uint16_t port)
{
    uint32_t ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "dN" (port));
    return ret;
}


static inline void 
outb (uint8_t val, uint16_t port)
{
    asm volatile ("outb %0, %1"::"a" (val), "dN" (port));
}


static inline void 
outw (uint16_t val, uint16_t port)
{
    asm volatile ("outw %0, %1"::"a" (val), "dN" (port));
}


static inline void 
outl (uint32_t val, uint16_t port)
{
    asm volatile ("outl %0, %1"::"a" (val), "dN" (port));
}


static inline void 
sti (void)
{
    asm volatile ("sti" : : : "memory");
}


static inline void 
cli (void)
{
    asm volatile ("cli" : : : "memory");
}


static inline uint64_t __attribute__((always_inline))
rdtsc (void)
{
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return lo | ((uint64_t)(hi) << 32);
}

#ifdef NAUT_CONFIG_XEON_PHI 
#define rdtscp() rdtsc
#else

static inline uint64_t
rdtscp (void)
{
    uint32_t lo, hi;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi));
    return lo | ((uint64_t)(hi) << 32);
}

#endif


static inline uint64_t 
read_rflags (void)
{
    uint64_t ret;
    asm volatile ("pushfq; popq %0" : "=a"(ret));
    return ret;
}

static inline void
halt (void) 
{
    asm volatile ("hlt");
}


static inline void 
invlpg (unsigned long addr)
{
    asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}


static inline void
wbinvd (void) 
{
    asm volatile("wbinvd" : : : "memory");
}


static inline void clflush(void *ptr)
{
    __asm__ __volatile__ ("clflush (%0); " 
			  : : "r"(ptr) : "memory");
    
}

static inline void clflush_unaligned(void *ptr, int size)
{
    clflush(ptr);
    if ((addr_t)ptr % size) { 
	// ptr is misaligned, so be paranoid since we 
	// may be spanning a cache line
	clflush((void*)((addr_t)ptr+size-1));
    }
}

/**
 * Flush all non-global entries in the calling CPU's TLB.
 *
 * Flushing non-global entries is the common-case since user-space
 * does not use global pages (i.e., pages mapped at the same virtual
 * address in *all* processes).
 *
 */
static inline void
tlb_flush (void)
{
    uint64_t tmpreg;

    asm volatile(
        "movq %%cr3, %0;  # flush TLB \n"
        "movq %0, %%cr3;              \n"
        : "=r" (tmpreg)
        :: "memory"
    );
}


static inline void io_delay(void)
{
    const uint16_t DELAY_PORT = 0x80;
    asm volatile("outb %%al,%0" : : "dN" (DELAY_PORT));
}


#ifdef NAUT_CONFIG_XEON_PHI
/* TODO: assuming 1.1GHz here... */
static void udelay (uint_t n) {
    uint32_t cycles = n * 1100;
    asm volatile ("movl %0, %%eax; delay %%eax;"
                : /* no output */
                : "r" (cycles)
                : "eax");
}
#else
static void udelay(uint_t n) {
    while (n--){
        io_delay();
    }
}
#endif

    
#ifdef __cplusplus 
}
#endif


#endif
