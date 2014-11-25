#include <cpu.h>
#include <cpuid.h>
#include <msr.h>
#include <irq.h>
#include <paging.h>
#include <nautilus.h>
#include <percpu.h>
#include <intrinsics.h>
#include <dev/apic.h>
#include <lib/liballoc.h>

#ifndef NAUT_CONFIG_DEBUG_APIC
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif


int 
check_apic_avail (void)
{
    cpuid_ret_t cp;
    struct cpuid_feature_flags * flags;

    cpuid(CPUID_FEATURE_INFO, &cp);
    flags = (struct cpuid_feature_flags *)&cp.c;

    return flags->edx.apic;
}


static int 
apic_is_bsp (struct apic_dev * apic)
{
    uint64_t data;
    data = msr_read(IA32_APIC_BASE_MSR);
    return APIC_IS_BSP(data);
}


static int
apic_sw_enable (struct apic_dev * apic)
{
    uint32_t val;
    uint8_t flags = irq_disable_save();
    val = apic_read(apic, APIC_REG_SPIV);
    apic_write(apic, APIC_REG_SPIV, val | APIC_SPIV_SW_ENABLE);
    irq_enable_restore(flags);
    return 0;
}


static int
apic_sw_disable (struct apic_dev * apic)
{
    uint32_t val;
    uint8_t flags = irq_disable_save();
    val = apic_read(apic, APIC_REG_SPIV);
    apic_write(apic, APIC_REG_SPIV, val & ~APIC_SPIV_SW_ENABLE);
    irq_enable_restore(flags);
    return 0;
}


static void
assign_spiv (uint8_t spiv_vec)
{
    /* TODO: fill in */

}


static void 
apic_enable (struct apic_dev * apic) 
{
    uint64_t data;
    uint8_t flags = irq_disable_save();
    data = msr_read(IA32_APIC_BASE_MSR);
    msr_write(IA32_APIC_BASE_MSR, data | APIC_GLOBAL_ENABLE);
    apic_sw_enable(apic);
    irq_enable_restore(flags);
}




static ulong_t 
apic_get_base_addr (void) 
{
    uint64_t data;
    data = msr_read(IA32_APIC_BASE_MSR);

    // we're assuming PAE is on
    return (addr_t)(data & APIC_BASE_ADDR_MASK);
}


static void
apic_set_base_addr (struct apic_dev * apic, addr_t addr)
{
    uint64_t data;
    data = msr_read(IA32_APIC_BASE_MSR);
    msr_write(IA32_APIC_BASE_MSR, (addr & APIC_BASE_ADDR_MASK) | (data & 0xfff));
}


void 
apic_do_eoi (void)
{
    struct apic_dev * apic = (struct apic_dev*)per_cpu_get(apic);
    apic_write(apic, APIC_REG_EOR, 0);
}


static uint32_t
apic_get_id (struct apic_dev * apic)
{
    return apic_read(apic, APIC_REG_ID) >> APIC_ID_SHIFT;
}


static inline uint8_t 
apic_get_version (struct apic_dev * apic)
{
    return APIC_VERSION(apic_read(apic, APIC_REG_LVR));
}


uint32_t 
apic_wait_for_send(struct apic_dev * apic)
{
    uint32_t res;
    int n = 0;

    do {
        if (!(res = apic_read(apic, APIC_REG_ICR) & ICR_SEND_PENDING)) {
            break;
        }
        udelay(100);
    } while (n++ < 1000);

    return res;
}


int 
apic_get_maxlvt (struct apic_dev * apic)
{
    uint_t v;

    v = apic_read(apic, APIC_REG_LVR);
    return ((v >> 16) & 0xffu);
}


inline void __always_inline
apic_ipi (struct apic_dev * apic, 
          uint_t remote_id,
          uint_t vector) 
{
    uint8_t flags = irq_disable_save();
    apic_write(apic, APIC_REG_ICR2, remote_id << APIC_ICR2_DST_SHIFT);
    apic_write(apic, APIC_REG_ICR, vector | ICR_LEVEL_ASSERT);
    irq_enable_restore(flags);
}


void
apic_self_ipi (struct apic_dev * apic, uint_t vector)
{
    uint8_t flags = irq_disable_save();
    apic_write(apic, APIC_IPI_SELF, vector);
    irq_enable_restore(flags);
}


void 
apic_send_iipi (struct apic_dev * apic, uint32_t remote_id) 
{
    uint8_t flags = irq_disable_save();
    apic_write(apic, APIC_REG_ICR2, remote_id << APIC_ICR2_DST_SHIFT);
    apic_write(apic, APIC_REG_ICR, ICR_TRIG_MODE_LEVEL| ICR_LEVEL_ASSERT | ICR_DEL_MODE_INIT);
    irq_enable_restore(flags);
}


void
apic_deinit_iipi (struct apic_dev * apic, uint32_t remote_id)
{
    uint8_t flags = irq_disable_save();
    apic_write(apic, APIC_REG_ICR2, remote_id << APIC_ICR2_DST_SHIFT);
    apic_write(apic, APIC_REG_ICR, ICR_TRIG_MODE_LEVEL| ICR_DEL_MODE_INIT);
    irq_enable_restore(flags);
}


void
apic_send_sipi (struct apic_dev * apic, uint32_t remote_id, uint8_t target)
{
    uint8_t flags = irq_disable_save();
    apic_write(apic, APIC_REG_ICR2, remote_id << APIC_ICR2_DST_SHIFT);
    apic_write(apic, APIC_REG_ICR, ICR_DEL_MODE_STARTUP | target);
    irq_enable_restore(flags);
}


void
apic_bcast_iipi (struct apic_dev * apic) 
{
    uint8_t flags = irq_disable_save();
    apic_write(apic, APIC_REG_ICR, APIC_IPI_OTHERS | ICR_LEVEL_ASSERT | ICR_TRIG_MODE_LEVEL | ICR_DEL_MODE_INIT);
    irq_enable_restore(flags);
}


void
apic_bcast_deinit_iipi (struct apic_dev * apic)
{
    uint8_t flags = irq_disable_save();
    apic_write(apic, APIC_REG_ICR, APIC_IPI_OTHERS | ICR_TRIG_MODE_LEVEL | ICR_DEL_MODE_INIT);
    irq_enable_restore(flags);
}


void
apic_bcast_sipi (struct apic_dev * apic, uint8_t target)
{
    uint8_t flags = irq_disable_save();
    apic_write(apic, APIC_REG_ICR, APIC_IPI_OTHERS | ICR_DEL_MODE_STARTUP | target);
    irq_enable_restore(flags);
}

/* TODO: add apic dump function */


void
ap_apic_setup (struct cpu * core)
{
    core->apic->base_addr = apic_get_base_addr();
    core->apic->version   = apic_get_version(core->apic);
    core->apic->id        = apic_get_id(core->apic);

    if (core->apic->version < 0x10 || core->apic->version > 0x15) {
        panic("Unsupported APIC version (0x%x) in core %u\n", core->apic->version, core->id);
    }

    DEBUG_PRINT("Configuring CPU %u LAPIC (id=0x%x)\n", core->id, core->apic->id);

    /* set spurious interrupt vector to 0xff, disable CPU core focusing */
    apic_write(core->apic, 
               APIC_REG_SPIV,
               ((apic_read(core->apic, APIC_REG_SPIV) & APIC_DISABLE_FOCUS) | 0xff));
            
    /* Setup TPR (Task-priority register) to disable softwareinterrupts */
    apic_write(core->apic, APIC_REG_TPR, 0x20);

    /* disable LAPIC timer interrupts */
    apic_write(core->apic, APIC_REG_LVTT, (1<<16)); 

    /* disable perf cntr interrupts */
    apic_write(core->apic, APIC_REG_LVTPC, (1<<16));

    /* disable thermal interrupts */
    apic_write(core->apic, APIC_REG_LVTTHMR, (1<<16));

    /* enable normal external interrupts */
    apic_write(core->apic, APIC_REG_LVT0, 0x8700);

    /* enable normal NMI processing */
    apic_write(core->apic, APIC_REG_LVT1, 0x400);
    
    /* disable error interrupts */
    apic_write(core->apic, APIC_REG_LVTERR, (1<<16));
}


void
ap_apic_final_init(struct cpu * core)
{
    apic_enable(core->apic);

    /* just to be safe */
    apic_write(core->apic, APIC_REG_LVT0, 0x8700);
    apic_write(core->apic, APIC_REG_SPIV, 0x3ff);
}


void
apic_init (struct naut_info * naut)
{
    struct apic_dev * apic = NULL;

    apic = (struct apic_dev*)malloc(sizeof(struct apic_dev));
    if (!apic) {
        panic("could not allocate apic struct\n");
    }
    memset(apic, 0, sizeof(struct apic_dev));

    if (!check_apic_avail()) {
        panic("no APIC found, dying\n");
    } 

    apic->base_addr = apic_get_base_addr();
    DEBUG_PRINT("apic base addr: %p\n", apic->base_addr);

    DEBUG_PRINT("Reserving APIC address space\n");
    if (nk_reserve_page(apic->base_addr) < 0) {
        DEBUG_PRINT("LAPIC mem region already reserved\n");
    }
    
    /* map in the lapic as uncacheable */
    if (nk_map_page_nocache(apic->base_addr, PTE_PRESENT_BIT|PTE_WRITABLE_BIT) == -1) {
        panic("Could not map APIC\n");
    }

    apic->version   = apic_get_version(apic);
    apic->id        = apic_get_id(apic);

    DEBUG_PRINT("Found LAPIC (version=0x%x, id=0x%x)\n", apic->version, apic->id);

    if (apic->version < 0x10 || apic->version > 0x15) {
        panic("Unsupported APIC version (0x%1x)\n", (unsigned)apic->version);
    }

    /* TODO: these shouldn't be hard-coded! */
    apic_write(apic, APIC_REG_TPR, 0x20);       // inhibit softint delivery
    apic_write(apic, APIC_REG_LVTT, 0x10000);   // disable timer interrupts
    apic_write(apic, APIC_REG_LVTPC, 0x10000);  // disable perf cntr interrupts
    apic_write(apic, APIC_REG_LVTTHMR, 0x10000); // disable thermal interrupts
    apic_write(apic, APIC_REG_LVT0, 0x08700);   // enable normal external interrupts
    apic_write(apic, APIC_REG_LVT1, 0x00400);   // enable normal NMI processing
    apic_write(apic, APIC_REG_LVTERR, 0x10000); // disable error interrupts
    apic_write(apic, APIC_REG_SPIV, 0x3ff);     // SPUR int is 0xff

    apic_enable(apic); // This will also set S/W enable bit in SPIV

    apic_write(apic, APIC_REG_LVT0, 0x08700);  // BAM BAM BAM
    apic_write(apic, APIC_REG_SPIV, 0x3ff); 

    naut->sys.cpus[0]->apic = apic;
}

