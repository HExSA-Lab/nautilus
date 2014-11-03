#ifndef __MSR_H__
#define __MSR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <naut_types.h>

void msr_write (uint32_t msr, uint64_t data);
uint64_t msr_read (uint32_t msr);

#ifdef __cplusplus
}
#endif

#endif
