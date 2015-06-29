#include <nautilus/multiboot2.h>
#include <nautilus/mb_utils.h>
#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/hrt.h>
#include <lib/liballoc.h>


int 
hrt_init_cpus (struct sys_info * sys)
{
    int i;
    struct multiboot_tag_hrt * hrt = sys->mb_info->hrt_info;
    unsigned core2apic_offset;

    if (!hrt) {
        panic("No HRT information found!\n");
    }

    sys->bsp_id   = 0;
    sys->num_cpus = hrt->num_apics;

    core2apic_offset = hrt->first_hrt_apic_id;

    if (sys->num_cpus > NAUT_CONFIG_MAX_CPUS) {
        panic("Nautilus kernel currently compiled for a maximum CPU count of %u, attempting to use %u\n", NAUT_CONFIG_MAX_CPUS, sys->num_cpus);
    }

    printk("HRT detected %u CPUs\n", sys->num_cpus);

    for (i = 0; i < hrt->num_apics; i++) {
        struct cpu * new_cpu = malloc(sizeof(struct cpu));

        if (!new_cpu) {
            ERROR_PRINT("Could not allocate CPU struct\n");
            return -1;
        }
        memset(new_cpu, 0, sizeof(struct cpu));

        if (i == sys->bsp_id) {
            new_cpu->is_bsp = 1;
        }

        new_cpu->id         = i;
        new_cpu->lapic_id   = i + core2apic_offset;
        new_cpu->enabled    = 1;
        new_cpu->cpu_sig    = 0;
        new_cpu->feat_flags = 0;
        new_cpu->system     = sys;
        new_cpu->cpu_khz    = 0;

        spinlock_init(&(new_cpu->lock));

        sys->cpus[i] = new_cpu;
        
    }

    return 0;
}


int
hrt_init_ioapics (struct sys_info * sys)
{
    struct multiboot_tag_hrt * hrt = sys->mb_info->hrt_info;

    if (!hrt) {
        panic("No HRT information found!\n");
    }

    if (hrt->have_hrt_ioapic) {

        sys->num_ioapics = 1;

        struct ioapic * ioa = malloc(sizeof(struct ioapic));
        if (!ioa) {
            ERROR_PRINT("Could not allocate IOAPIC\n");
            return -1;
        }
        memset(ioa, 0, sizeof(struct ioapic));

        ioa->id      = 0;
        ioa->version = 0;
        ioa->usable  = 1;

        // KCH TODO: maybe the bases should be communicated
        ioa->base            = 0xfec00000;
        ioa->first_hrt_entry = hrt->first_hrt_ioapic_entry;

        sys->ioapics[0] = ioa;

            
    } else {
        sys->num_ioapics = 0;
    }

    return 0;
}


int
hvm_hrt_init (void)
{
    hvm_hcall(0);
    return 0;
}
