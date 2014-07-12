#ifndef __APIC_H__
#define __APIC_H__

#include <types.h>

#define IA32_APIC_BASE_MSR        0x1b
#define IA32_APIC_BASE_MSR_BSP    0x100 
#define IA32_APIC_BASE_MSR_ENABLE 0x800

#define APIC_BASE_ADDR_MASK 0xfffff000
#define APIC_IS_BSP(x)      ((x) & (1 << 8))
#define APIC_VERSION(x)     ((x) & 0xffu)

#define APIC_IPI_SELF          0x40000
#define APIC_IPI_ALL           0x80000
#define APIC_IPI_OTHERS        0xC0000
#define APIC_GLOBAL_ENABLE     (1 << 11)
#define APIC_SPIV_SW_ENABLE   (1 << 8)

#define ICR_DEL_MODE_LOWEST  (1 << 8)
#define ICR_DEL_MODE_SMI     (2 << 8)
#define ICR_DEL_MODE_NMI     (4 << 8)
#define ICR_DEL_MODE_INIT    (5 << 8)
#define ICR_DEL_MODE_STARTUP (6 << 8)

#define ICR_DST_MODE_LOG      (1 << 11)
#define ICR_SEND_PENDING      (1 << 12)
#define ICR_LEVEL_ASSERT      (1 << 14)
#define ICR_TRIG_MODE_LEVEL   (1 << 15)



#define APIC_ID_SHIFT 24
#define APIC_ICR2_DST_SHIFT 24


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
#define APIC_REG_LVT_CMCI 0x2f0
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


struct apic_dev {
    ulong_t base_addr;
    uint8_t  version;
    uint_t  id;
};


int check_apic_avail(void);

static inline void
apic_write (struct apic_dev * apic, 
            uint_t reg, 
            uint32_t val)
{
    *((volatile uint32_t *)(apic->base_addr + reg)) = val;
}


static inline uint32_t
apic_read (struct apic_dev * apic, uint_t reg)
{
    return *((volatile uint32_t *)(apic->base_addr + reg));
}


struct naut_info;

void apic_do_eoi(void);
void apic_ipi(struct apic_dev * apic, uint_t remote_id, uint_t vector);
void apic_self_ipi (struct apic_dev * apic, uint_t vector);
void apic_send_iipi(struct apic_dev * apic, uint32_t remote_id);
void apic_deinit_iipi(struct apic_dev * apic, uint32_t remote_id);
void apic_send_sipi(struct apic_dev * apic, uint32_t remote_id, uint8_t target);
void apic_bcast_iipi(struct apic_dev * apic);
void apic_bcast_deinit_iipi(struct apic_dev * apic);
void apic_bcast_sipi(struct apic_dev * apic, uint8_t target);
void apic_init(struct naut_info * naut);

int apic_get_maxlvt(struct apic_dev * apic);
uint32_t apic_wait_for_send(struct apic_dev* apic);



#endif
