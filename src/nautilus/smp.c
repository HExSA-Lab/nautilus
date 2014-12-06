#include <nautilus/nautilus.h>
#include <nautilus/smp.h>
#include <nautilus/paging.h>
#include <nautilus/acpi.h>
#include <nautilus/irq.h>
#include <nautilus/msr.h>
#include <nautilus/gdt.h>
#include <nautilus/cpu.h>
#include <nautilus/naut_assert.h>
#include <nautilus/thread.h>
#include <nautilus/queue.h>
#include <nautilus/idle.h>
#include <nautilus/atomic.h>
#include <nautilus/percpu.h>
#include <dev/ioapic.h>
#include <dev/apic.h>
#include <dev/timer.h>
#include <lib/liballoc.h>

#ifndef NAUT_CONFIG_DEBUG_SMP
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define SMP_PRINT(fmt, args...) printk("SMP: " fmt, ##args)
#define SMP_DEBUG(fmt, args...) DEBUG_PRINT("SMP: " fmt, ##args)


extern struct cpu * smp_ap_stack_switch(uint64_t, uint64_t, struct cpu*);

static volatile unsigned smp_core_count = 1; // assume BSP is booted

extern addr_t init_smp_boot;
extern addr_t end_smp_boot;

/* TODO: compute checksum on MPTable */
/* TODO: print out MP Table info (we'll eventually get some of this 
 * stuff from ACPI 
 */

static uint8_t mp_entry_lengths[5] = {
    MP_TAB_CPU_LEN,
    MP_TAB_BUS_LEN,
    MP_TAB_IOAPIC_LEN,
    MP_TAB_IO_INT_LEN,
    MP_TAB_LINT_LEN,
};


static void
parse_mptable_cpu (struct sys_info * sys, struct mp_table_entry_cpu * cpu)
{
    struct cpu * new_cpu = NULL;
    DEBUG_PRINT("parse_mptable_cpu found cpu\n");

    if (sys->num_cpus == NAUT_CONFIG_MAX_CPUS) {
        panic("too many CPUs!\n");
    }

    if(!(new_cpu = malloc(sizeof(struct cpu)))) {
        panic("Couldn't allocate CPU struct\n");
    } 
    DEBUG_PRINT("allocated new CPU struct at %p\n", (void*)new_cpu);
    memset(new_cpu, 0, sizeof(struct cpu));

    new_cpu->id         = sys->num_cpus;
    new_cpu->lapic_id   = cpu->lapic_id;
    new_cpu->enabled    = cpu->enabled;
    new_cpu->is_bsp     = cpu->is_bsp;
    new_cpu->cpu_sig    = cpu->sig;
    new_cpu->feat_flags = cpu->feat_flags;
    new_cpu->system     = sys;
    new_cpu->cpu_khz    = nk_detect_cpu_freq(new_cpu->id);

    spinlock_init(&new_cpu->lock);

    sys->cpus[sys->num_cpus] = new_cpu;

    sys->num_cpus++;
}


/* TODO: change ioapics to pointers! */
static void
parse_mptable_ioapic (struct sys_info * sys, struct mp_table_entry_ioapic * ioapic)
{
    DEBUG_PRINT("MPTABLE_PARSE: found IOAPIC %d (located at %p)\n", sys->num_ioapics, (void*)&sys->ioapics[sys->num_ioapics]);
    DEBUG_PRINT("IOAPIC INFO: id=%x, ver=%x, enabled=%x, addr=%lx\n", ioapic->id, ioapic->version, ioapic->enabled, ioapic->addr);


    sys->ioapics[sys->num_ioapics].id      = ioapic->id;
    sys->ioapics[sys->num_ioapics].version = ioapic->version;
    sys->ioapics[sys->num_ioapics].usable  = ioapic->enabled;
    sys->ioapics[sys->num_ioapics].base    = (addr_t)ioapic->addr;
    sys->num_ioapics++; 
}

static void
parse_mptable_lint (struct sys_info * sys, struct mp_table_entry_lint * lint)
{
    char * type_map[4] = {"[INT]", "[NMI]", "[SMI]", "[ExtINT]"};
    char * po_map[4] = {"[BUS]", "[ActHi]", "[Rsvd]", "[ActLo]"};
    char * el_map[4] = {"[BUS]", "[Edge]", "[Rsvd]", "[Level]"};
    DEBUG_PRINT("MPTABLE_PARSE: found LINT entry\n");
    DEBUG_PRINT("\tInt Type=%s\n", type_map[lint->int_type]);
    DEBUG_PRINT("\tPolarity=%s\n", po_map[lint->po]);
    DEBUG_PRINT("\tTrigger Mode=%s\n", el_map[lint->el]);
    DEBUG_PRINT("\tSrc Bus ID=%02x\n", lint->src_bus_id);
    DEBUG_PRINT("\tSrc Bus IRQ=%02x\n", lint->src_bus_irq);
    DEBUG_PRINT("\tDst LAPIC ID=%02x\n", lint->dst_lapic_id);
    DEBUG_PRINT("\tDst LAPIC LINTIN=%02x\n", lint->dst_lapic_lintin);
}

static void
parse_mptable_ioint (struct sys_info * sys, struct mp_table_entry_ioint * ioint)
{
    char * type_map[4] = {"[INT]", "[NMI]", "[SMI]", "[ExtINT]"};
    char * po_map[4] = {"[BUS]", "[ActHi]", "[Rsvd]", "[ActLo]"};
    char * el_map[4] = {"[BUS]", "[Edge]", "[Rsvd]", "[Level]"};
    DEBUG_PRINT("MPTABLE_PARSE: found IOINT entry\n");
    DEBUG_PRINT("\tType=%s\n", type_map[ioint->int_type]);
    DEBUG_PRINT("\tPolarity=%s\n", po_map[ioint->po]);
    DEBUG_PRINT("\tTrigger Mode=%s\n", el_map[ioint->el]);
    DEBUG_PRINT("\tSrc Bus ID=%02x\n", ioint->src_bus_id);
    DEBUG_PRINT("\tSrc Bus IRQ=%02x\n", ioint->src_bus_irq);
    DEBUG_PRINT("\tDst IOAPIC ID=%02x\n", ioint->dst_ioapic_id);
    DEBUG_PRINT("\tDst IOAPIC INT Pin=%02x\n", ioint->dst_ioapic_intin);
}

static void
parse_mptable_bus (struct sys_info * sys, struct mp_table_entry_bus * bus)
{
    DEBUG_PRINT("MPTABLE_PARSE: found BUS entry\n");
    DEBUG_PRINT("\tBus ID: %02x\n", bus->bus_id);
    DEBUG_PRINT("\tType: %s\n", bus->bus_type_string);
}


static int
parse_mp_table (struct sys_info * sys, struct mp_table * mp)
{
    int count = mp->entry_cnt;
    uint8_t * mp_entry;


    /* make sure everything is as expected */
    if (strncmp((char*)&mp->sig, "PCMP", 4) != 0) {
        ERROR_PRINT("MP Table unexpected format\n");
    }

    mp_entry = (uint8_t*)&mp->entries;
    DEBUG_PRINT("MP table entry count: %d\n", mp->entry_cnt);

    while (count--) {

        uint8_t type = *mp_entry;

        switch (type) {
            case MP_TAB_TYPE_CPU:
                parse_mptable_cpu(sys, (struct mp_table_entry_cpu*)mp_entry);
                break;
            case MP_TAB_TYPE_IOAPIC:
                parse_mptable_ioapic(sys, (struct mp_table_entry_ioapic*)mp_entry);
                break;
            case MP_TAB_TYPE_IO_INT:
                parse_mptable_ioint(sys, (struct mp_table_entry_ioint*)mp_entry);
                break;
            case MP_TAB_TYPE_BUS:
                parse_mptable_bus(sys, (struct mp_table_entry_bus*)mp_entry);
                break;
            case MP_TAB_TYPE_LINT:
                parse_mptable_lint(sys, (struct mp_table_entry_lint*)mp_entry);
                break;
            default:
                ERROR_PRINT("Unexpected MP Table Entry (type=%d)\n", type);
                return -1;
        }

        mp_entry += mp_entry_lengths[type];
    }

    return 0;
}


static struct mp_float_ptr_struct* 
find_mp_pointer (void)
{
    char * cursor = (char*)BASE_MEM_LAST_KILO;

    /* NOTE: these memory regions should already be mapped, 
     * if not, this will fail 
     */

    while (cursor != (char*)(BASE_MEM_LAST_KILO+PAGE_SIZE)) {

        if (strncmp(cursor, "_MP_", 4) == 0) {
            DEBUG_PRINT("\n");
            return (struct mp_float_ptr_struct*)cursor;
        }

        cursor += 4;
    }

    cursor = (char*)BIOS_ROM_BASE;

    while (cursor != (char*)BIOS_ROM_END) {

        if (strncmp(cursor, "_MP_", 4) == 0) {
            return (struct mp_float_ptr_struct*)cursor;
        }

        cursor += 4;
    }

    return 0;
}


int
smp_early_init (struct naut_info * naut)
{
    struct mp_float_ptr_struct * mp_ptr;

    mp_ptr = find_mp_pointer();

    if (!mp_ptr) {
        ERROR_PRINT("Could not find MP floating pointer struct\n");
        return -1;
    }

    naut->sys.pic_mode_enabled = mp_ptr->mp_feat2 & PIC_MODE_ON;

    DEBUG_PRINT("Parsing MP Table\n");
    parse_mp_table(&(naut->sys), (struct mp_table*)(uint64_t)mp_ptr->mp_cfg_ptr);

    printk("SMP: Detected %d CPUs\n", naut->sys.num_cpus);

    return 0;
}


static int
init_ap_area (struct ap_init_area * ap_area, 
              struct naut_info * naut,
              int core_num)
{
    memset((void*)ap_area, 0, sizeof(struct ap_init_area));

    /* setup pointer to this CPUs stack */
    uint32_t boot_stack_ptr = AP_BOOT_STACK_ADDR;

    ap_area->stack   = boot_stack_ptr;
    ap_area->cpu_ptr = naut->sys.cpus[core_num];

    /* protected mode temporary GDT */
    ap_area->gdt[2]      = 0x0000ffff;
    ap_area->gdt[3]      = 0x00cf9a00;
    ap_area->gdt[4]      = 0x0000ffff;
    ap_area->gdt[5]      = 0x00cf9200;

    /* long mode temporary GDT */
    ap_area->gdt64[1]    = 0x00a09a0000000000;
    ap_area->gdt64[2]    = 0x00a0920000000000;

    /* pointer to BSP's PML4 */
    ap_area->cr3         = read_cr3();

    /* pointer to our entry routine */
    ap_area->entry       = smp_ap_entry;

    return 0;
}


static inline void 
smp_wait_for_ap (struct naut_info * naut, unsigned int core_num)
{
    struct cpu * core = naut->sys.cpus[core_num];
    BARRIER_WHILE(!core->booted);

#if 0
    while (1) {
        uint8_t flags;
        flags = spin_lock_irq_save(&(naut->sys.cpus[core_num]->lock));
        if (*(volatile uint8_t *)&(naut->sys.cpus[core_num]->booted) == 1) {
            spin_unlock_irq_restore(&(naut->sys.cpus[core_num]->lock), flags);
            break;
        }
        spin_unlock_irq_restore(&(naut->sys.cpus[core_num]->lock), flags);
        asm volatile ("pause");
    }
#endif
}


int
smp_bringup_aps (struct naut_info * naut)
{
    struct ap_init_area * ap_area;

    addr_t boot_target     = (addr_t)&init_smp_boot;
    size_t smp_code_sz     = (addr_t)&end_smp_boot - boot_target;
    addr_t ap_trampoline   = (addr_t)AP_TRAMPOLINE_ADDR;
    uint8_t target_vec     = ap_trampoline >> 12U;
    struct apic_dev * apic = naut->sys.cpus[0]->apic;

    int status = 0; 
    int err = 0;
    int i, j, maxlvt;

    if (naut->sys.num_cpus == 1) {
        return 0;
    }

    maxlvt = apic_get_maxlvt(apic);

    DEBUG_PRINT("passing target page num %x to SIPI\n", target_vec);

    /* clear APIC errors */
    if (maxlvt > 3) {
        apic_write(apic, APIC_REG_ESR, 0);
    }
    apic_read(apic, APIC_REG_ESR);

    DEBUG_PRINT("copying in page for SMP boot code at (%p)...\n", (void*)ap_trampoline);
    memcpy((void*)ap_trampoline, (void*)boot_target, smp_code_sz);

    /* create an info area for APs */
    /* initialize AP info area (stack pointer, GDT info, etc) */
    ap_area = (struct ap_init_area*)AP_INFO_AREA;

    DEBUG_PRINT("passing ap area at %p\n", (void*)ap_area);

    /* START BOOTING AP CORES */
    
    /* we, of course, skip the BSP (NOTE: assuming it's 0...) */
    for (i = 1; i < naut->sys.num_cpus; i++) {
        int ret;

        DEBUG_PRINT("Booting secondary CPU %u\n", i);

        ret = init_ap_area(ap_area, naut, i);
        if (ret == -1) {
            ERROR_PRINT("Error initializing ap area\n");
            return -1;
        }


        /* Send the INIT sequence */
        DEBUG_PRINT("sending INIT to remote APIC (%u)\n", naut->sys.cpus[i]->lapic_id);
        apic_send_iipi(apic, naut->sys.cpus[i]->lapic_id);

        /* wait for status to update */
        status = apic_wait_for_send(apic);

        mbarrier();

        /* 10ms delay */
        udelay(10000);

        /* deassert INIT IPI (level-triggered) */
        apic_deinit_iipi(apic, naut->sys.cpus[i]->lapic_id);

        for (j = 1; j <= 2; j++) {
            if (maxlvt > 3) {
                apic_write(apic, APIC_REG_ESR, 0);
            }
            apic_read(apic, APIC_REG_ESR);

            DEBUG_PRINT("sending SIPI %u to core %u (vec=%x)\n", j, i, target_vec);

            /* send the startup signal */
            apic_send_sipi(apic, naut->sys.cpus[i]->lapic_id, target_vec);

            udelay(300);

            status = apic_wait_for_send(apic);

            udelay(200);

            err = apic_read(apic, APIC_REG_ESR) & 0xef;

            if (status || err) {
                break;
            }

        }

        if (status) {
            ERROR_PRINT("APIC wasn't delivered!\n");
        }

        if (err) {
            ERROR_PRINT("ERROR delivering SIPI\n");
        }

        /* wait for AP to set its boot flag */
        smp_wait_for_ap(naut, i);

        DEBUG_PRINT("Bringup for core %u done.\n", i);
    }

    BARRIER_WHILE(smp_core_count != naut->sys.num_cpus);

    return (status|err);
}


extern struct idt_desc idt_descriptor;
extern struct gdt_desc64 gdtr64;

static int xcall_handler(excp_entry_t * e, excp_vec_t v);


static int
smp_xcall_init_queue (struct cpu * core)
{
    core->xcall_q = nk_queue_create();
    if (!core->xcall_q) {
        ERROR_PRINT("Could not allocate xcall queue on cpu %u\n", core->id);
        return -1;
    }

    return 0;
}


int
smp_setup_xcall_bsp (struct cpu * core)
{
    printk("Setting up cross-core IPI event queue\n");
    smp_xcall_init_queue(core);

    if (register_int_handler(IPI_VEC_XCALL, xcall_handler, NULL) != 0) {
        ERROR_PRINT("Could not assign interrupt handler for XCALL on core %u\n", core->id);
        return -1;
    }

    return 0;
}


static int
smp_ap_setup (struct cpu * core)
{
    // setup IDT
    lidt(&idt_descriptor);

    // setup GDT
    lgdt64(&gdtr64);

    uint64_t core_addr = (uint64_t) core->system->cpus[core->id];

    // set GS base (for per-cpu state)
    msr_write(MSR_GS_BASE, (uint64_t)core_addr);
    
    apic_init(core);

    if (smp_xcall_init_queue(core) != 0) {
        ERROR_PRINT("Could not setup xcall for core %u\n", core->id);
        return -1;
    }

    if (nk_sched_init_ap() != 0) {
        ERROR_PRINT("Could not setup scheduling for core %u\n", core->id);
        return -1;
    }

    return 0;
}


extern void fpu_init(void);

static void
smp_ap_finish (struct cpu * core)
{
    //ap_apic_final_init(core);
    fpu_init();

    PAUSE_WHILE(atomic_cmpswap(core->booted, 0, 1) != 0);

    atomic_inc(smp_core_count);

    /* wait on all the other cores to boot up */
    BARRIER_WHILE(smp_core_count != core->system->num_cpus);

    DEBUG_PRINT("SMP: Core %u ready, enabling interrupts\n", core->id);
    sti();
}


void 
smp_ap_entry (struct cpu * core) 
{ 
    struct cpu * my_cpu;
    DEBUG_PRINT("smp: core %u starting up\n", core->id);
    if (smp_ap_setup(core) < 0) {
        panic("Error setting up AP!\n");
    }

    /* we should now be able to pull our CPU pointer out of GS
     * This is important, because the stack will be clobbered
     * for the next CPU boot! 
     */
    my_cpu = get_cpu();
    printk("SMP: CPU (AP) %u operational)\n", my_cpu->id);

    // switch from boot stack to my new stack (allocated in thread_init)
    nk_thread_t * cur = get_cur_thread();

    /* 
     * we have to call into assembly since GCC 
     * wont let us clobber rbp. Note how we reassign
     * my_cpu. This is so we don't lose it in the
     * switch (it's sitting on the stack!)
     */
    my_cpu = smp_ap_stack_switch(cur->rsp, cur->rsp, my_cpu);

    // wait for the other cores and turn on interrupts
    smp_ap_finish(my_cpu);

    ASSERT(irqs_enabled());
    while (1) {
        nk_yield();
    }
}


uint32_t
nk_get_num_cpus (void)
{
    struct sys_info * sys = per_cpu_get(system);
    return sys->num_cpus;
}

static void
init_xcall (struct nk_xcall * x, void * arg, nk_xcall_func_t fun)
{
    x->data       = arg;
    x->fun        = fun;
    x->xcall_done = 0;
}


static inline void
wait_xcall (struct nk_xcall * x)
{

    while (atomic_cmpswap(x->xcall_done, 1, 0) != 1) {
        asm volatile ("pause");
    }
}


static inline void
mark_xcall_done (struct nk_xcall * x) 
{
    atomic_cmpswap(x->xcall_done, 0, 1);
}


static int
xcall_handler (excp_entry_t * e, excp_vec_t v) 
{
    nk_queue_t * xcq = per_cpu_get(xcall_q); 
    struct nk_xcall * x = NULL;
    nk_queue_entry_t * elm = NULL;

    if (!xcq) {
        ERROR_PRINT("Badness: no xcall queue on core %u\n", my_cpu_id());
        goto out_err;
    }

    elm = nk_dequeue_first_atomic(xcq);
    x = container_of(elm, struct nk_xcall, xcall_node);
    if (!x) {
        ERROR_PRINT("No XCALL request found on core %u\n", my_cpu_id());
        goto out_err;
    }

    if (x->fun) {

        // we ack the IPI before calling the handler funciton,
        // because it may end up blocking (e.g. core barrier)
        IRQ_HANDLER_END(); 

        x->fun(x->data);

        /* we need to notify the waiter we're done */
        if (x->has_waiter) {
            mark_xcall_done(x);
        }

    } else {
        ERROR_PRINT("No XCALL function found on core %u\n", my_cpu_id());
        goto out_err;
    }


    return 0;

out_err:
    IRQ_HANDLER_END();
    return -1;
}


/* 
 * smp_xcall
 *
 * initiate cross-core call. 
 * 
 * @cpu_id: the cpu to execute the call on
 * @fun: the function to invoke
 * @arg: the argument to the function
 * @wait: this function should block until the reciever finishes
 *        executing the function
 *
 */
int
smp_xcall (cpu_id_t cpu_id, 
           nk_xcall_func_t fun,
           void * arg,
           uint8_t wait)
{
    struct sys_info * sys = per_cpu_get(system);
    nk_queue_t * xcq  = NULL;
    struct nk_xcall x;
    uint8_t flags;

    DEBUG_PRINT("Initiating SMP XCALL from core %u to core %u\n", my_cpu_id(), cpu_id);

    if (cpu_id > nk_get_num_cpus()) {
        ERROR_PRINT("Attempt to execute xcall on invalid cpu (%u)\n", cpu_id);
        return -1;
    }

    if (cpu_id == my_cpu_id()) {

        flags = irq_disable_save();
        fun(arg);
        irq_enable_restore(flags);

    } else {
        struct nk_xcall * xc = &x;

        if (!wait) {
            xc = &(sys->cpus[cpu_id]->xcall_nowait_info);
        }

        init_xcall(xc, arg, fun);

        xcq = sys->cpus[cpu_id]->xcall_q;
        if (!xcq) {
            ERROR_PRINT("Attempt by cpu %u to initiate xcall on invalid xcall queue (for cpu %u)\n", 
                        my_cpu_id(),
                        cpu_id);
            return -1;
        }

        flags = irq_disable_save();

        if (!nk_queue_empty_atomic(xcq)) {
            ERROR_PRINT("XCALL queue for core %u is not empty, bailing\n", cpu_id);
            irq_enable_restore(flags);
            return -1;
        }

        nk_enqueue_entry_atomic(xcq, &(xc->xcall_node));

        irq_enable_restore(flags);

        /* KCH: WARNING: assumes LAPIC_ID == OS ID  NOT GOOD! */
        struct apic_dev * apic = per_cpu_get(apic);

        apic_ipi(apic, cpu_id, IPI_VEC_XCALL);

        if (wait) {
            wait_xcall(xc);
        }

    }

    return 0;
}
