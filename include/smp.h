#ifndef __SMP_H__
#define __SMP_H__

#define MAX_CPUS 128

#define AP_TRAMPOLINE_ADDR 0xf000 
#define AP_BOOT_STACK_ADDR 0x1000
#define AP_INFO_AREA       0x2000

#define BASE_MEM_LAST_KILO 0x9fc00
#define BIOS_ROM_BASE      0xf0000
#define BIOS_ROM_END       0xfffff

#define MSR_FS_BASE 0xc0000100
#define MSR_GS_BASE 0xc0000101

#ifndef __ASSEMBLER__ 

#include <dev/apic.h>
#include <spinlock.h>
struct naut_info;

struct cpu {
    uint32_t id;
    uint8_t lapic_id;
    uint8_t enabled;
    uint8_t is_bsp;
    uint32_t cpu_sig;
    uint32_t feat_flags;

    uint8_t booted;

    struct apic_dev * apic;

    struct sys_info * system;

    spinlock_t lock;

};

struct ap_init_area {
    uint32_t stack;  // 0
    uint32_t rsvd; // to align the GDT on 8-byte boundary // 4
    uint32_t gdt[6] ; // 8
    uint16_t gdt_limit; // 32
    uint32_t gdt_base; // 34
    uint16_t rsvd1; // 38
    uint64_t gdt64[3]; // 40
    uint16_t gdt64_limit; // 64
    uint64_t gdt64_base; // 66
    uint64_t cr3; // 74
    struct cpu * cpu_ptr; // 82

    void (*entry)(struct cpu * core); // 90

} __packed;


int smp_early_init(struct naut_info * naut);
int smp_bringup_aps(struct naut_info * naut);
void smp_ap_entry (struct cpu * core);


static inline struct cpu*
get_cpu (void)
{
    struct cpu * ret;
    asm volatile ("movq %%gs:0x0, %0" : "=r"(ret));
    return ret;
}

#endif /* !__ASSEMBLER__ */
#endif
