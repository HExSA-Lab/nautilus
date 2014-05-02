#ifndef __CPU_H__
#define __CPU_H__

#include <types.h>

typedef struct cpuid_ret {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
} cpuid_ret_t; 


static inline cpuid_ret_t 
cpuid (uint32_t func)
{
    cpuid_ret_t ret;

    asm volatile ("cpuid" : 
                  "=a"(ret.a), "=b"(ret.b), "=c"(ret.c), "=d"(ret.d) : 
                  "0"(func));
    return ret;
}


static inline cpuid_ret_t
cpuid_sub (uint32_t func, uint32_t sub_func)
{
    cpuid_ret_t ret;
    asm volatile ("cpuid" : 
                  "=a"(ret.a), "=b"(ret.b), "=c"(ret.c), "=d"(ret.d) : 
                  "0"(func), "2"(sub_func));
    return ret;
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


static inline uint64_t
rdtsc (void)
{
    uint32_t lo, hi;
    asm ("rdtsc" : "=a"(lo), "=d"(hi));
    return lo | ((uint64_t)(hi) << 32);
}


#endif
