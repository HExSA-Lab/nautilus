#ifndef __MSR_H__
#define __MSR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/naut_types.h>


#define IA32_MSR_EFER 0xc0000080

#define EFER_SCE    1      // System Call Extensions (SYSCALL)
#define EFER_LME   (1<<8)  // Long Mode Enable
#define EFER_LMA   (1<<10) // Long Mode Active
#define EFER_NXE   (1<<11) // No-Execute Enable
#define EFER_SVME  (1<<12) // Secure Virtual Machine Enable
#define EFER_LMSLE (1<<13) // Long Mode Segment Limit Enable
#define EFER_FFXSR (1<<14) // Fast FXSAVE/FXRSTOR
#define EFER_TCE   (1<<15) // Translation Cache Extension


void msr_write (uint32_t msr, uint64_t data);
uint64_t msr_read (uint32_t msr);

#ifdef __cplusplus
}
#endif

#endif
