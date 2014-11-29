#include <nautilus/msr.h>


inline void 
msr_write (uint32_t msr, uint64_t data)
{
    uint32_t lo = data;
    uint32_t hi = data >> 32;
    asm volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}


inline uint64_t 
msr_read (uint32_t msr) 
{
    uint32_t lo, hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}


