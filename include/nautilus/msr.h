#ifndef __MSR_H__
#define __MSR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/naut_types.h>


#define IA32_MSR_EFER 0xc0000080
#define IA32_MISC_ENABLES 0x1a0


#define MSR_FS_BASE 0xc0000100
#define MSR_GS_BASE 0xc0000101
#define MSR_KERNEL_GS_BASE 0xc0000102

#define EFER_SCE    1      // System Call Extensions (SYSCALL)
#define EFER_LME   (1<<8)  // Long Mode Enable
#define EFER_LMA   (1<<10) // Long Mode Active
#define EFER_NXE   (1<<11) // No-Execute Enable
#define EFER_SVME  (1<<12) // Secure Virtual Machine Enable
#define EFER_LMSLE (1<<13) // Long Mode Segment Limit Enable
#define EFER_FFXSR (1<<14) // Fast FXSAVE/FXRSTOR
#define EFER_TCE   (1<<15) // Translation Cache Extension


#define AMD_MSR_TSC           0x00000010
#define AMD_MSR_APIC_BASE     0x0000001b
#define AMD_MSR_MPERF         0x000000e7
#define AMD_MSR_APERF         0x000000e8
#define AMD_MSR_MTRRCAP       0x000000fe
#define AMD_MSR_SYSENTER_CS   0x00000174
#define AMD_MSR_SYSENTER_ESP  0x00000175
#define AMD_MSR_SYSENTER_EIP  0x00000176
#define AMD_MSR_MCG_CAP       0x00000179
#define AMD_MSR_MCG_STATUS    0x0000017a
#define AMD_MSR_MCG_CTL       0x0000017b
#define AMD_MSR_DEBUG_CTL     0x000001d9

#define AMD_MSR_PAT           0x00000277
#define AMD_MSR_STAR          0xc0000081
#define AMD_MSR_LSTAR         0xc0000082
#define AMD_MSR_CSTAR         0xc0000083
#define AMD_MSR_SFMASK        0xc0000084
#define AMD_MSR_FSBASE        0xc0000100
#define AMD_MSR_GSBASE        0xc0000101
#define AMD_MSR_KERN_GSBASE   0xc0000102
#define AMD_MSR_TSC_AUX       0xc0000103

#define AMD_MSR_SYSCFG        0xc0010010

#define AMD_MSR_VM_CR         0xc0010114

/* Control over Northbridge in multi-core chips */
#define AMD_MSR_NBRIDGE_CTL   0xc001001f


void msr_write (uint32_t msr, uint64_t data);
uint64_t msr_read (uint32_t msr);

#ifdef __cplusplus
}
#endif

#endif
