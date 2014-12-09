#include <nautilus/cpu.h>
#include <nautilus/cpuid.h>
#include <nautilus/msr.h>
#include <nautilus/irq.h>
#include <nautilus/paging.h>
#include <nautilus/nautilus.h>
#include <nautilus/percpu.h>
#include <nautilus/intrinsics.h>
#include <dev/apic.h>
#include <dev/i8254.h>
#include <dev/timer.h>
#include <lib/liballoc.h>

#ifndef NAUT_CONFIG_DEBUG_APIC
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

static int
spur_int_handler (excp_entry_t * excp, excp_vec_t v)
{
    DEBUG_PRINT("Received Spurious Interrupt\n");

    struct apic_dev * a = per_cpu_get(apic);
    a->spur_int_cnt++;

    /* we don't need to EOI here */
    return 0;
}


static uint8_t
check_apic_avail (void)
{
    cpuid_ret_t cp;
    struct cpuid_feature_flags * flags;

    cpuid(CPUID_FEATURE_INFO, &cp);
    flags = (struct cpuid_feature_flags *)&cp.c;

    return flags->edx.apic;
}


static uint8_t
apic_is_bsp (struct apic_dev * apic)
{
    uint64_t data;
    data = msr_read(IA32_APIC_BASE_MSR);
    return APIC_IS_BSP(data);
}


static void
apic_sw_enable (struct apic_dev * apic)
{
    uint32_t val;
    uint8_t flags = irq_disable_save();
    val = apic_read(apic, APIC_REG_SPIV);
    apic_write(apic, APIC_REG_SPIV, val | APIC_SPIV_SW_ENABLE);
    irq_enable_restore(flags);
}


static void
apic_sw_disable (struct apic_dev * apic)
{
    uint32_t val;
    uint8_t flags = irq_disable_save();
    val = apic_read(apic, APIC_REG_SPIV);
    apic_write(apic, APIC_REG_SPIV, val & ~APIC_SPIV_SW_ENABLE);
    irq_enable_restore(flags);
}


static void
apic_assign_spiv (struct apic_dev * apic, uint8_t spiv_vec)
{
    apic_write(apic, 
            APIC_REG_SPIV,
            apic_read(apic, APIC_REG_SPIV) | spiv_vec);
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
    ASSERT(apic);
    apic_write(apic, APIC_REG_EOR, 0);
}


uint32_t
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

int
apic_read_timer (struct apic_dev * apic)
{
    return apic_read(apic, APIC_REG_TMCCT);
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
apic_timer_setup (struct apic_dev * apic, uint32_t quantum)
{
    uint32_t busfreq;
    uint32_t tmp;
    uint8_t  tmp2;
    cpuid_ret_t ret;

    printk("Setting up Local APIC timer for core %u\n", apic->id);

    cpuid(0x6, &ret);
    if (ret.a & 0x4) {
        printk("\t[APIC Supports Constant Tick Rate]\n");
    }

    if (register_int_handler(APIC_TIMER_INT_VEC,
            nk_timer_handler,
            NULL) != 0) {
        panic("Could not register APIC timer handler\n");
    }

    /* first we set up the APIC for one-shot  */
    apic_write(apic, APIC_REG_LVTT, APIC_TIMER_INT_VEC);

    /* set up the divider */
    apic_write(apic, APIC_REG_TMDCR, APIC_TIMER_DIVCODE);

    /* set PIT channel 2 to "out" mode */
    outb((inb(KB_CTRL_PORT_B) & 0xfd) | 0x1, 
          KB_CTRL_PORT_B);

    /* configure the PIT channel 2 for one-shot */
    outb(PIT_MODE(PIT_MODE_ONESHOT) |
         PIT_CHAN(PIT_CHAN_SEL_2)   |
         PIT_ACC_MODE(PIT_ACC_MODE_BOTH),
         PIT_CMD_REG);

    /* LSB  (PIT rate) */
    outb((PIT_RATE/NAUT_CONFIG_HZ) & 0xff,
         PIT_CHAN2_DATA);
    inb(KB_CTRL_DATA_OUT);

    /* MSB */
    outb((uint8_t)((PIT_RATE/NAUT_CONFIG_HZ)>>8),
                PIT_CHAN2_DATA);

    /* clear and reset bit 0 of kbd ctrl port to reload
     * current cnt on chan 2 with the new value */
    tmp2 = inb(KB_CTRL_PORT_B) & 0xfe;
    outb(tmp2, KB_CTRL_PORT_B);
    outb(tmp2 | 1, KB_CTRL_PORT_B);

    /* our count down value */
    apic_write(apic, APIC_REG_TMICT, 0xffffffff);

    while (!(inb(KB_CTRL_PORT_B) & 0x20));

    apic_write(apic, APIC_REG_LVTT, APIC_TIMER_DISABLE);

    busfreq = APIC_TIMER_DIV * NAUT_CONFIG_HZ*(0xffffffff - apic_read(apic, APIC_REG_TMCCT) + 1);
    printk("Detected CPU %u bus frequency as %u KHz\n", apic->id, busfreq/1000);
    tmp = busfreq/quantum/APIC_TIMER_DIV;

    DEBUG_PRINT("Writing APIC timer counter as %u\n", tmp);
    apic_write(apic, APIC_REG_TMICT, (tmp < APIC_TIMER_DIV) ? APIC_TIMER_DIV : tmp);
    apic_write(apic, APIC_REG_LVTT, APIC_TIMER_INT_VEC | APIC_TIMER_PERIODIC);
    apic_write(apic, APIC_REG_TMDCR, APIC_TIMER_DIVCODE);
}


#if 0
void
ap_apic_setup (struct cpu * core)
{
    struct apic_dev * a = malloc(sizeof(struct apic_dev));
    if (!a) {
        ERROR_PRINT("Could not allocate APIC struct for core %u\n", core->id);
        return;
    }
    memset(a, 0, sizeof(struct apic_dev));
    core->apic = a;

    core->apic->base_addr    = apic_get_base_addr();
    core->apic->version      = apic_get_version(core->apic);
    core->apic->id           = apic_get_id(core->apic);
    core->apic->spur_int_cnt = 0;

    if (core->apic->version < 0x10 || core->apic->version > 0x15) {
        panic("Unsupported APIC version (0x%x) in core %u\n", core->apic->version, core->id);
    }

    DEBUG_PRINT("Configuring CPU %u LAPIC (id=0x%x)\n", core->id, core->apic->id);
            
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

    /* set spurious interrupt vector to 0xff, disable CPU core focusing */
    apic_write(core->apic, 
               APIC_REG_SPIV,
               apic_read(core->apic, APIC_REG_SPIV) & APIC_DISABLE_FOCUS);

    apic_assign_spiv(core->apic, 0xffu);

    if (register_int_handler(0xff, spur_int_handler, core->apic) != 0) {
        ERROR_PRINT("Could not register handler for spurious interrupt\n");
    }

}


void
ap_apic_final_init(struct cpu * core)
{
    apic_enable(core->apic);

    apic_timer_setup(core->apic, (NAUT_CONFIG_HZ * 100 / 1000));
    apic_write(core->apic, APIC_REG_LVT0, 0x8700); /* BAM BAM */
}

#endif

void
apic_init (struct cpu * core)
{
    struct apic_dev * apic = NULL;

    apic = (struct apic_dev*)malloc(sizeof(struct apic_dev));
    if (!apic) {
        panic("Could not allocate apic struct\n");
    }
    memset(apic, 0, sizeof(struct apic_dev));

    if (!check_apic_avail()) {
        panic("No APIC found on core %u, dying\n", core->id);
    } 

    apic->base_addr = apic_get_base_addr();
    DEBUG_PRINT("APIC base addr: %p\n", apic->base_addr);

    if (core->is_bsp) {

        DEBUG_PRINT("Reserving APIC address space\n");
        if (nk_reserve_page(apic->base_addr) < 0) {
            DEBUG_PRINT("LAPIC mem region already reserved\n");
        }

        /* map in the lapic as uncacheable */
        if (nk_map_page_nocache(apic->base_addr, PTE_PRESENT_BIT|PTE_WRITABLE_BIT) == -1) {
            panic("Could not map APIC\n");
        }
    }

    apic->version   = apic_get_version(apic);
    apic->id        = apic_get_id(apic);

    DEBUG_PRINT("Found LAPIC (version=0x%x, id=0x%x)\n", apic->version, apic->id);

    if (apic->version < 0x10 || apic->version > 0x15) {
        panic("Unsupported APIC version (0x%1x)\n", (unsigned)apic->version);
    }

    apic_write(apic, APIC_REG_TPR, apic_read(apic, APIC_REG_TPR) & 0xfffffff00);        // accept all interrupts
    apic_write(apic, APIC_REG_LVTT, APIC_LVT_DISABLED);    // disable timer interrupts intially
    apic_write(apic, APIC_REG_LVTPC, APIC_LVT_DISABLED);   // disable perf cntr interrupts
    apic_write(apic, APIC_REG_LVTTHMR, APIC_LVT_DISABLED); // disable thermal interrupts

    /* Only the BSP takes External interrupts */
    if (core->is_bsp && !(apic_read(apic, APIC_REG_LVT0) & APIC_LVT_DISABLED)) {
        apic_write(apic, APIC_REG_LVT0, 0x700);
        printk("Enabling ExtInt on core %u\n", my_cpu_id());
    } else {
        apic_write(apic, APIC_REG_LVT0, 0x700 | APIC_LVT_DISABLED); 
        printk("Masking ExtInt on core %u\n", my_cpu_id());
    }

    /* only BSP takes NMI interrupts */
    if (core->is_bsp) {
        apic_write(apic, APIC_REG_LVT1, 0x400);
    } else {
        apic_write(apic, APIC_REG_LVT1, 0x400 | APIC_LVT_DISABLED);
    }

    apic_write(apic, APIC_REG_LVTERR, (apic_read(apic, APIC_REG_LVTERR) & 0xffffff00) | 0xfe);  // error interrupt maps to vector fe

    /* disable core focusing */
    apic_write(apic, 
               APIC_REG_SPIV,
               APIC_DISABLE_FOCUS);

    apic_assign_spiv(apic, APIC_SPUR_INT_VEC);

    if (register_int_handler(APIC_SPUR_INT_VEC, spur_int_handler, apic) != 0) {
        ERROR_PRINT("Could not register spurious interrupt handler\n");
    }

    apic_enable(apic); // This will also set S/W enable bit in SPIV

    apic_timer_setup(apic, (NAUT_CONFIG_HZ * 100 / 1000));

    core->apic = apic;
}

