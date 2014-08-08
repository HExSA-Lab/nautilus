#include <nautilus.h>
#include <smp.h>
#include <paging.h>
#include <acpi.h>
#include <irq.h>
#include <msr.h>
#include <cpu.h>
#include <dev/ioapic.h>
#include <dev/timer.h>
#include <lib/liballoc.h>

#ifndef NAUT_CONFIG_DEBUG_SMP
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

extern addr_t init_smp_boot;

/* TODO: compute checksum on MPTable */

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

    if (sys->num_cpus == MAX_CPUS) {
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

    sys->cpus[sys->num_cpus] = new_cpu;

    sys->num_cpus++;

    DEBUG_PRINT("sanity check: apic id b4:%u, after %u (ptr=%p)\n", cpu->lapic_id, new_cpu->lapic_id, (void*)new_cpu);
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
            case MP_TAB_TYPE_BUS:
            case MP_TAB_TYPE_LINT:
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
    uint32_t boot_stack_ptr = AP_BOOT_STACK_ADDR * core_num;
    if (create_page_mapping((addr_t)boot_stack_ptr, (addr_t)boot_stack_ptr, PTE_PRESENT_BIT|PTE_WRITABLE_BIT) < 0) {
        ERROR_PRINT("Couldn't create page mapping for core (%d) boot stack\n", core_num);
        return -1;
    }

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


int
smp_bringup_aps (struct naut_info * naut)
{
    addr_t boot_target     = (addr_t)&init_smp_boot;
    addr_t ap_trampoline   = AP_TRAMPOLINE_ADDR;
    uint8_t target_vec     = PADDR_TO_PAGE(ap_trampoline);
    struct apic_dev * apic = naut->sys.cpus[0]->apic;
    struct ap_init_area * ap_area;
    int status = 0; 
    int err = 0;
    int i, j, maxlvt;

    maxlvt = apic_get_maxlvt(apic);

    DEBUG_PRINT("passing target page num %x to SIPI\n", target_vec);

    /* clear APIC errors */
    if (maxlvt > 3) {
        apic_write(apic, APIC_REG_ESR, 0);
    }
    apic_read(apic, APIC_REG_ESR);

    /* copy our SMP boot code (shouldn't really need to do this) */
    DEBUG_PRINT("mapping in page for SMP boot code...\n");
    if (create_page_mapping(ap_trampoline, ap_trampoline, PTE_PRESENT_BIT|PTE_WRITABLE_BIT) < 0) {
        ERROR_PRINT("Couldn't create page mapping for SMP boot code\n");
        return -1;
    }

    memcpy((void*)ap_trampoline, (void*)boot_target, PAGE_SIZE);

    /* create an info area for APs */
    if (create_page_mapping((addr_t)AP_INFO_AREA, (addr_t)AP_INFO_AREA, PTE_PRESENT_BIT|PTE_WRITABLE_BIT) < 0) {
        ERROR_PRINT("Couldn't create page mapping for SMP AP init area\n");
        return -1;
    }

    /* initialize AP info area (stack pointer, GDT info, etc) */
    ap_area = (struct ap_init_area*)(AP_INFO_AREA);

    /* START BOOTING AP CORES */
    
    /* we, of course, skip the BSP (NOTE: assuming its 0...) */
    for (i = 1; i < naut->sys.num_cpus; i++) {
        int ret;

        printk("Booting secondary CPU %u\n", i);

        ret = init_ap_area(ap_area, naut, i);
        if (ret == -1) {
            ERROR_PRINT("Error initializing ap area\n");
            return -1;
        }

        /* Send the INIT sequence */
        DEBUG_PRINT("sending INIT to remote APIC\n");
        apic_send_iipi(apic, naut->sys.cpus[i]->lapic_id);

        /* wait for status to update */
        status = apic_wait_for_send(apic);
        DEBUG_PRINT("INIT send complete\n");

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

            DEBUG_PRINT("sending SIPI %u to core %u\n", j, i);

            /* send the startup signal */
            DEBUG_PRINT("\tremote LAPIC id: %u\n", naut->sys.cpus[i]->lapic_id);
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
            ERROR_PRINT("ERROR while delivering SIPI\n");
        }

        /* wait for AP to set its boot flag */
        while (naut->sys.cpus[i]->booted != 1) {
            asm volatile ("pause");
        }

        DEBUG_PRINT("Bringup for core %u done.\n", i);
    }

    return (status|err);

}


static int
smp_ap_setup (struct cpu * core)
{
    uint64_t core_addr = (uint64_t) &(core->system->cpus[core->id]);
    // set FS base
    msr_write(MSR_GS_BASE, (uint64_t)core_addr);

    struct cpu * core2 = get_cpu();
    printk("core %u testing my coreid: %u\n", core->id, core2->id);

    // setup IDT
    // setup GDT
    // per_processor GS/FS Base setup in GDT
    
    // init LAPIC

    return 0;
}


void 
smp_ap_entry (struct cpu * core) 
{ 
    core->booted = 1;
    printk("hello from core %u\n", core->id);

    if (smp_ap_setup(core) < 0) {
        panic("Error setting up AP!\n");
    }

    while (1) { halt(); }
        
}


