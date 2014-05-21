#ifndef __SMP_H__
#define __SMP_H__

#include <dev/apic.h>

#define MAX_CPUS 128

#define BASE_MEM_LAST_KILO 0x9fc00
#define BIOS_ROM_BASE      0xf0000
#define BIOS_ROM_END       0xfffff

struct naut_info;

int smp_init(struct naut_info * naut);

struct cpu {
    uint32_t id;
    uint8_t lapic_id;
    uint8_t enabled;
    uint8_t is_bsp;
    uint32_t cpu_sig;
    uint32_t feat_flags;

    uint8_t booted;

    struct apic_dev * apic;
};




#endif
