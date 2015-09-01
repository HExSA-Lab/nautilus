/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/acpi.h>
#include <nautilus/smp.h>
#include <nautilus/sfi.h>
#include <nautilus/irq.h>
#include <nautilus/mm.h>
#include <nautilus/percpu.h>
#include <nautilus/numa.h>
#include <nautilus/cpu.h>

#ifndef NAUT_CONFIG_DEBUG_SMP
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define SMP_PRINT(fmt, args...) printk("SMP: " fmt, ##args)
#define SMP_DEBUG(fmt, args...) DEBUG_PRINT("SMP: " fmt, ##args)


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

    if (sys->num_cpus == NAUT_CONFIG_MAX_CPUS) {
        panic("CPU count exceeded max (check your .config)\n");
    }

    if(!(new_cpu = mm_boot_alloc(sizeof(struct cpu)))) {
        panic("Couldn't allocate CPU struct\n");
    } 
    memset(new_cpu, 0, sizeof(struct cpu));

    new_cpu->id         = sys->num_cpus;
    new_cpu->lapic_id   = cpu->lapic_id;

    new_cpu->enabled    = cpu->enabled;
    new_cpu->is_bsp     = cpu->is_bsp;
    new_cpu->cpu_sig    = cpu->sig;
    new_cpu->feat_flags = cpu->feat_flags;
    new_cpu->system     = sys;
    new_cpu->cpu_khz    = nk_detect_cpu_freq(new_cpu->id);

    SMP_DEBUG("CPU %u -- LAPIC ID 0x%x -- Features: ", new_cpu->id, new_cpu->lapic_id);
#ifdef NAUT_CONFIG_DEBUG_SMP
    printk("%s", (new_cpu->feat_flags & 1) ? "fpu " : "");
    printk("%s", (new_cpu->feat_flags & (1<<7)) ? "mce " : "");
    printk("%s", (new_cpu->feat_flags & (1<<8)) ? "cx8 " : "");
    printk("%s", (new_cpu->feat_flags & (1<<9)) ? "apic " : "");
    printk("\n");
#endif
    SMP_DEBUG("\tEnabled?=%01d\n", new_cpu->enabled);
    SMP_DEBUG("\tBSP?=%01d\n", new_cpu->is_bsp);
    SMP_DEBUG("\tSignature=0x%x\n", new_cpu->cpu_sig);
    SMP_DEBUG("\tFreq=%lu.%03lu MHz\n", new_cpu->cpu_khz/1000, new_cpu->cpu_khz%1000);

    spinlock_init(&new_cpu->lock);

    sys->cpus[sys->num_cpus] = new_cpu;

    sys->num_cpus++;
}


static void
parse_mptable_ioapic (struct sys_info * sys, struct mp_table_entry_ioapic * ioapic)
{
    struct ioapic * ioa = NULL;
    if (sys->num_ioapics == NAUT_CONFIG_MAX_IOAPICS) {
        panic("IOAPIC count exceeded max (change it in .config)\n");
    }

    if (!(ioa = mm_boot_alloc(sizeof(struct ioapic)))) {
        panic("Couldn't allocate IOAPIC struct\n");
    }
    memset(ioa, 0, sizeof(struct ioapic));

    SMP_DEBUG("IOAPIC entry:\n");
    SMP_DEBUG("\tID=0x%x\n", ioapic->id);
    SMP_DEBUG("\tVersion=0x%x\n", ioapic->version);
    SMP_DEBUG("\tEnabled?=%01d\n", ioapic->enabled);
    SMP_DEBUG("\tBase Addr=0x%lx\n", ioapic->addr);

    ioa->id      = ioapic->id;
    ioa->version = ioapic->version;
    ioa->usable  = ioapic->enabled;
    ioa->base    = (addr_t)ioapic->addr;

    sys->ioapics[sys->num_ioapics] = ioa;
    sys->num_ioapics++; 
}


static void
parse_mptable_lint (struct sys_info * sys, struct mp_table_entry_lint * lint)
{
    char * type_map[4] = {"[INT]", "[NMI]", "[SMI]", "[ExtINT]"};
    char * po_map[4] = {"[BUS]", "[ActHi]", "[Rsvd]", "[ActLo]"};
    char * el_map[4] = {"[BUS]", "[Edge]", "[Rsvd]", "[Level]"};
    uint8_t polarity = lint->int_flags & 0x3;
    uint8_t trig_mode = (lint->int_flags >> 2) & 0x3;
    SMP_DEBUG("LINT entry\n");
    SMP_DEBUG("\tInt Type=%s\n", type_map[lint->int_type]);
    SMP_DEBUG("\tPolarity=%s\n", po_map[polarity]);
    SMP_DEBUG("\tTrigger Mode=%s\n", el_map[trig_mode]);
    SMP_DEBUG("\tSrc Bus ID=0x%02x\n", lint->src_bus_id);
    SMP_DEBUG("\tSrc Bus IRQ=0x%02x\n", lint->src_bus_irq);
    SMP_DEBUG("\tDst LAPIC ID=0x%02x\n", lint->dst_lapic_id);
    SMP_DEBUG("\tDst LAPIC LINTIN=0x%02x\n", lint->dst_lapic_lintin);
}

static void
parse_mptable_ioint (struct sys_info * sys, struct mp_table_entry_ioint * ioint)
{
    char * type_map[4] = {"[INT]", "[NMI]", "[SMI]", "[ExtINT]"};
    char * po_map[4] = {"[BUS]", "[ActHi]", "[Rsvd]", "[ActLo]"};
    char * el_map[4] = {"[BUS]", "[Edge]", "[Rsvd]", "[Level]"};
    uint8_t polarity = ioint->int_flags & 0x3;
    uint8_t trig_mode = (ioint->int_flags >> 2) & 0x3;
    SMP_DEBUG("IOINT entry\n");
    SMP_DEBUG("\tType=%s\n", type_map[ioint->int_type]);
    SMP_DEBUG("\tPolarity=%s\n", po_map[polarity]);
    SMP_DEBUG("\tTrigger Mode=%s\n", el_map[trig_mode]);
    SMP_DEBUG("\tSrc Bus ID=0x%02x\n", ioint->src_bus_id);
    SMP_DEBUG("\tSrc Bus IRQ=0x%02x\n", ioint->src_bus_irq);
    SMP_DEBUG("\tDst IOAPIC ID=0x%02x\n", ioint->dst_ioapic_id);
    SMP_DEBUG("\tDst IOAPIC INT Pin=0x%02x\n", ioint->dst_ioapic_intin);

    nk_add_int_entry(trig_mode,
                     polarity,
                     ioint->int_type,
                     ioint->src_bus_id,
                     ioint->src_bus_irq,
                     ioint->dst_ioapic_intin,
                     ioint->dst_ioapic_id);
}

static void
parse_mptable_bus (struct sys_info * sys, struct mp_table_entry_bus * bus)
{
    SMP_DEBUG("Bus entry\n");
    SMP_DEBUG("\tBus ID: 0x%02x\n", bus->bus_id);
    SMP_DEBUG("\tType: %.6s\n", bus->bus_type_string);
    nk_add_bus_entry(bus->bus_id, bus->bus_type_string);
}

static uint8_t 
blk_cksum_ok (uint8_t * mp, unsigned len)
{
    unsigned sum = 0;

    while (len--) {
        sum += *mp++;
    }

    return ((sum & 0xff) == 0);
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
    SMP_PRINT("Parsing MP Table (entry count=%u)\n", mp->entry_cnt);


    SMP_PRINT("Verifying MP Table integrity...");
    if (!blk_cksum_ok((uint8_t*)mp, mp->len)) {
        printk("FAIL\n");
        ERROR_PRINT("Corrupt MP Table detected\n");
    } else {
        printk("OK\n");
    }

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

    while (cursor <= (char*)(BASE_MEM_LAST_KILO+PAGE_SIZE)) {

        if (strncmp(cursor, "_MP_", 4) == 0) {
            return (struct mp_float_ptr_struct*)cursor;
        }

        cursor += 4;
    }

    cursor = (char*)BIOS_ROM_BASE;

    while (cursor <= (char*)BIOS_ROM_END) {

        if (strncmp(cursor, "_MP_", 4) == 0) {
            return (struct mp_float_ptr_struct*)cursor;
        }

        cursor += 4;
    }
    return 0;
}


static int
__early_init_mp (struct naut_info * naut)
{
    struct mp_float_ptr_struct * mp_ptr;

    mp_ptr = find_mp_pointer();

    if (!mp_ptr) {
        ERROR_PRINT("Could not find MP floating pointer struct\n");
        return -1;
    }

    naut->sys.pic_mode_enabled = !!(mp_ptr->mp_feat2 & PIC_MODE_ON);

    SMP_PRINT("Verifying MP Floating Ptr Struct integrity...");
    if (!blk_cksum_ok((uint8_t*)mp_ptr, 16)) {
        printk("FAIL\n");
        ERROR_PRINT("Corrupt MP Floating Ptr Struct detected\n");
    } else {
        printk("OK\n");
    }

    parse_mp_table(&(naut->sys), (struct mp_table*)(uint64_t)mp_ptr->mp_cfg_ptr);

    SMP_PRINT("Detected %u CPUs\n", naut->sys.num_cpus);

    return 0;
}


int 
arch_early_init (struct naut_info * naut)
{
    return __early_init_mp(naut);
}
