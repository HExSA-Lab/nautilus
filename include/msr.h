#ifndef __MSR_H__
#define __MSR_H__

#include <types.h>

void msr_write (uint32_t msr, uint64_t data);
uint64_t msr_read (uint32_t msr);

#endif
