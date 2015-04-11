#ifndef __XEON_PHI_H__
#define __XEON_PHI_H__

#include <nautilus/naut_types.h>

#define PHI_SBOX_BASE   0x8007D0000ULL
#define PHI_BOOT_OK_REG 0xAB28


void phi_card_is_up(void);

static inline uint32_t
phi_sbox_read (uint32_t offset)
{
    return *(volatile uint32_t*)((uint32_t*) PHI_SBOX_BASE + offset);
}

static inline void
phi_sbox_write (uint32_t offset, uint32_t val)
{
    *(volatile uint32_t*)((uint32_t*)PHI_SBOX_BASE + offset) = val;
}



#endif
