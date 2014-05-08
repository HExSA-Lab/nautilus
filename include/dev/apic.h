#ifndef __APIC_H__
#define __APIC_H__

#define IA32_APIC_BASE    0x0000001b

#define APIC_GLOBAL_ENABLE (IA32_APIC_BASE | (1<<11))

#define APIC_REG_ID       0x20
#define APIC_REG_LVR      0x30
#define APIC_REG_TPR      0x80
#define APIC_REG_APR      0x90
#define APIC_REG_PPR      0xa0
#define APIC_REG_EOR      0xb0
#define APIC_REG_RRR      0xc0
#define APIC_REG_LDR      0xd0
#define APIC_REG_DFR      0xe0
#define APIC_REG_SPIV     0xf0
#define APIC_REG_ISR      0x100
#define APIC_REG_TMR      0x180
#define APIC_REG_IRR      0x200
#define APIC_REG_ESR      0x280
#define APIC_REG_ICR      0x300
#define APIC_REG_ICR2     0x310
#define APIC_REG_LVTT     0x320
#define APIC_REG_LVTTHMR  0x330
#define APIC_REG_LVTPC    0x340
#define APIC_REG_LVT0     0x350
#define APIC_REG_LVT1     0x360
#define APIC_REG_LVTERR   0x370
#define APIC_REG_TMICT    0x380
#define APIC_REG_TMCCT    0x390
#define APIC_REG_TMDCR    0x3e0
#define APIC_REG_SELF_IPI 0x3f0

int check_apic_avail(void);


#endif
