#ifndef __CPU_H__
#define __CPU_H__

#include <types.h>

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


#define PAUSE_WHILE(x) \
    while ((x)) { \
        asm volatile("pause"); \
    } 

#define mbarrier()    asm volatile("mfence":::"memory")


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


static inline uint64_t
rdtsc (void)
{
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return lo | ((uint64_t)(hi) << 32);
}


static inline uint64_t
rdtscp (void)
{
    uint32_t lo, hi;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi));
    return lo | ((uint64_t)(hi) << 32);
}


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


static inline void io_delay(void)
{
    const uint16_t DELAY_PORT = 0x80;
    asm volatile("outb %%al,%0" : : "dN" (DELAY_PORT));
}


static void udelay(uint_t n) {
    while (n--){
        io_delay();
    }
}

    


#endif
