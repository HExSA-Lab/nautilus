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
 * http://xstack.sandia.gov/hobbes
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
#include <nautilus/paging.h>
#include <nautilus/dev.h>
#include <dev/ioapic.h>
#include <nautilus/irq.h>
#include <nautilus/shell.h>
#include <nautilus/mm.h>

#ifndef NAUT_CONFIG_DEBUG_IOAPIC
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define IOAPIC_DEBUG(fmt, args...) DEBUG_PRINT("IOAPIC: " fmt, ##args)
#define IOAPIC_PRINT(fmt, args...) printk("IOAPIC: " fmt, ##args)


static uint64_t 
ioapic_read_irq_entry (struct ioapic * ioapic, uint8_t irq)
{
    uint32_t lo, hi;
    lo = ioapic_read_reg(ioapic, IOAPIC_IRQ_ENTRY_LO(irq));
    hi = ioapic_read_reg(ioapic, IOAPIC_IRQ_ENTRY_HI(irq));
    return (uint64_t)lo | ((uint64_t)hi << 32);
}



void
ioapic_write_irq_entry (struct ioapic * ioapic, uint8_t irq, uint64_t val)
{
    /* first disable it */
    ioapic_mask_irq(ioapic, irq);

    /* hi */
    ioapic_write_reg(ioapic, IOAPIC_IRQ_ENTRY_HI(irq), (uint32_t)(val >> 32));

    /* lo */
    ioapic_write_reg(ioapic, IOAPIC_IRQ_ENTRY_LO(irq), (uint32_t)(val & 0xffffffff));
}


void
ioapic_mask_irq (struct ioapic * ioapic, uint8_t irq)
{
    uint32_t val;
    ASSERT(irq < ioapic->num_entries);
    val = ioapic_read_reg(ioapic, IOAPIC_IRQ_ENTRY_LO(irq));
    ioapic_write_reg(ioapic, IOAPIC_IRQ_ENTRY_LO(irq), val | IOAPIC_MASK_IRQ);
    ioapic->entries[irq].enabled = 0;
}


static uint8_t
ioapic_get_max_entry (struct ioapic * ioapic)
{
    return ((ioapic_read_reg(ioapic, IOAPICVER_REG) >> 16) & 0xff);
}


void 
ioapic_unmask_irq (struct ioapic * ioapic, uint8_t irq)
{
    uint32_t val;
    ASSERT(irq < ioapic->num_entries);
    val = ioapic_read_reg(ioapic, IOAPIC_IRQ_ENTRY_LO(irq));
    ioapic_write_reg(ioapic, IOAPIC_IRQ_ENTRY_LO(irq), val & ~IOAPIC_MASK_IRQ);
    ioapic->entries[irq].enabled = 1;
}


static void 
ioapic_assign_irq (struct ioapic * ioapic,
                   uint8_t irq, 
                   uint8_t vector,
                   uint8_t polarity, 
                   uint8_t trigger_mode,
                   uint8_t mask_it)
{
    ASSERT(irq < ioapic->num_entries);
    ioapic_write_irq_entry(ioapic, irq, 
                           vector                             |
                           (mask_it ? IOAPIC_MASK_IRQ : 0)    |
                           (DELMODE_FIXED << DEL_MODE_SHIFT)  |
                           (polarity << INTPOL_SHIFT)         | 
                           (trigger_mode << TRIG_MODE_SHIFT));
}


static uint8_t 
ioapic_get_id (struct ioapic * ioapic)
{
    uint32_t ret;
    ret = ioapic_read_reg(ioapic, IOAPICID_REG);
    return (ret >> 24) & 0xf;
}

static uint8_t 
ioapic_get_version (struct ioapic * ioapic)
{
    uint32_t ret;
    ret = ioapic_read_reg(ioapic, IOAPICVER_REG);
    return ret & 0xff;
}


static void
ioapic_dump (struct ioapic * ioapic)
{
    uint64_t val = 0;
    unsigned pin;
    const char * del_modes[8] = {
        "Fixed",
        "Lowest",
        "SMI",
        "Rsvd",
        "NMI",
        "INIT",
        "Rsvd",
        "ExtINT"
    };

    IOAPIC_DEBUG("IOAPIC DUMP: \n");

    IOAPIC_DEBUG(
        "  ID:  0x%08x (id=%u)\n",
        ioapic_read_reg(ioapic, IOAPICID_REG),
        IOAPIC_GET_ID(ioapic_read_reg(ioapic, IOAPICID_REG))
    );

    IOAPIC_DEBUG(
        "  VER: 0x%08x (Max Red. Entry=%u, Version=0x%02x)\n",
        ioapic_read_reg(ioapic, IOAPICVER_REG),
        IOAPIC_GET_MAX_RED(ioapic_read_reg(ioapic, IOAPICVER_REG)),
        IOAPIC_GET_VER(ioapic_read_reg(ioapic, IOAPICVER_REG))
    );

    IOAPIC_DEBUG(
        "  BASE ADDR: %p\n",
        ioapic->base
    );

    IOAPIC_DEBUG(
        "  ARB: 0x%08x (Arb. ID=0x%01x)\n", 
        ioapic_read_reg(ioapic, IOAPICARB_REG),
        IOAPIC_GET_ARBID(ioapic_read_reg(ioapic, IOAPICARB_REG))
    );

    for (pin = 0; pin < ioapic->num_entries; pin++)  {
        val = ioapic_read_irq_entry(ioapic, pin);

        IOAPIC_DEBUG(
            "  IORED[%u]: (Dest ID=0x%02x, %s, Trig Mode=%s,\n",
            pin,
            IORED_GET_DEST(val),
            IORED_GET_MASK(val) ? "MASKED" : "ENABLED",
            IORED_GET_TRIG(val) ? "Level" : "Edge"
        );

        IOAPIC_DEBUG(
            "             Remote IRR=%u, Polarity=%s, Dest Mode=%s,\n",
            IORED_GET_RIRR(val),
            IORED_GET_POL(val) ? "ActiveLo" : "ActiveHi",
            IORED_GET_DST_MODE(val) ? "Logical" : "Physical"
        );

        IOAPIC_DEBUG(
            "             Delivery Mode=%s, IDT VECTOR %u\n", 
            del_modes[IORED_GET_DEL_MODE(val)],
            IORED_GET_VEC(val)
        );
    }
}


static int
__ioapic_init (struct ioapic * ioapic, uint8_t ioapic_id)
{
    int i;
    struct nk_int_entry * ioint = NULL;

    if (nk_map_page_nocache(ROUND_DOWN_TO_PAGE(ioapic->base), PTE_PRESENT_BIT|PTE_WRITABLE_BIT, PS_4K) == -1) {
        panic("Could not map IOAPIC\n");
        return -1;
    }

    ioapic_write_reg(ioapic, IOAPICID_REG, ioapic_id);

    /* be paranoid and mask everything right off the bat */
    for (i = 0; i < ioapic->num_entries; i++) {
        ioapic_mask_irq(ioapic, i);
    }

    /* get the last entry we can access for this IOAPIC */
    ioapic->num_entries = ioapic_get_max_entry(ioapic) + 1;

    ioapic->entries = malloc(sizeof(struct iored_entry)*ioapic->num_entries);
    if (!ioapic->entries) {
        ERROR_PRINT("Could not allocate IOAPIC %u INT entries\n");
        return -1;
    }
    memset(ioapic->entries, 0, sizeof(struct iored_entry)*ioapic->num_entries);

    IOAPIC_DEBUG("Initializing IOAPIC (ID=0x%x)\n", ioapic_get_id(ioapic));
    IOAPIC_DEBUG("\tVersion=0x%x\n", ioapic_get_version(ioapic));
    IOAPIC_DEBUG("\tMapping at %p\n", (void*)ioapic->base);
    IOAPIC_DEBUG("\tNum Entries: %u\n", ioapic->num_entries);

    /* we assign 0xf7 as our "bogus" vector. If we see this,
     * something is wrong because it doesn't correspond to an
     * assigned interrupt
     */
    for (i = 0; i < ioapic->num_entries; i++) {
        ioapic_assign_irq(ioapic,
                i,
                0xf7,
                0,
                0,
                1 /* mask it */);
    }

    /* now walk through the MP Table IO INT entries */
    list_for_each_entry(ioint, 
            &(nk_get_nautilus_info()->sys.int_info.int_list), 
            elm) {

        uint8_t pol;
        uint8_t trig;
        uint8_t newirq;

        if (ioint->dst_ioapic_id != ioapic->id) {
            continue;
        }

        /* PCI IRQs get their own IOAPIC entrires
         * we're not going to bother with dealing 
         * with PIC mode 
         */
        if (nk_int_matches_bus(ioint, "ISA", 3)) {
            pol     = 0;
            trig    = 0; 
            newirq  = ioint->src_bus_irq;
        } else if (nk_int_matches_bus(ioint, "PCI", 3)) {
            pol     = 1;
            trig    = 1;
            // INT A, B, C, and D -> IRQs 16,17,18,19
            // lower order 2 bits identify which PCI int, upper 3 identify the device
            newirq  = 16 + (ioint->src_bus_irq & 0x3);
        } else {
            pol     = 0;
            trig    = 0;
            newirq  = 20 + ioint->src_bus_irq;
        }
        

        /* TODO: this is not quite right. Here I'm making the assumption that 
         * we only assign PCI A, B, C, and D to one IORED entry each. Technically
         * we should be able to, e.g. assign Dev 1 PCI A and Dev 2 PCI A to different
         * IOAPIC IORED entries. The BIOS should, and does appear to, set things up
         * this way 
         */
        if (!nk_irq_is_assigned(newirq)) {

            IOAPIC_DEBUG("Unit %u assigning new IRQ 0x%x (src_bus=0x%x, src_bus_irq=0x%x, vector=0x%x) to IORED entry %u\n",
                    ioapic->id,
                    newirq,
                    ioint->src_bus_id,
                    ioint->src_bus_irq,
                    irq_to_vec(newirq),
                    ioint->dst_ioapic_intin);

            ioapic_assign_irq(ioapic, 
                    ioint->dst_ioapic_intin,
                    irq_to_vec(newirq),
                    pol,
                    trig,
                    1 /* mask it */);

            struct iored_entry * iored_entry = &(ioapic->entries[ioint->dst_ioapic_intin]);
            iored_entry->boot_info  = ioint;
            iored_entry->actual_irq = newirq;

            irqmap_set_ioapic(newirq, ioapic);
        }
    }


    IOAPIC_DEBUG("Masking all IORED entries\n");
    /* being paranoid */
    for (i = 0; i < ioapic->num_entries; i++) {
        ioapic_mask_irq(ioapic, i);
    }

    ioapic_dump(ioapic);

    return 0;
}

static struct nk_dev_int ops = {
    .open=0,
    .close=0,
};

int 
ioapic_init (struct sys_info * sys)
{
    int i = 0;
    for (i = 0; i < sys->num_ioapics; i++) {
        if (__ioapic_init(sys->ioapics[i], i) < 0) {
            ERROR_PRINT("Couldn't initialize IOAPIC\n");
            return -1;
        } else {
	    char n[32];
	    snprintf(n,32,"ioapic%u",i);
	    nk_dev_register(n,NK_DEV_INTR,0,&ops,&sys->ioapics[i]);
	}
    }

#ifndef NAUT_CONFIG_GEM5    // unsupported in Gem5, causes Gem5 to crash
    /* Enter Symmetric I/O mode */
    if (sys->pic_mode_enabled) {
        IOAPIC_PRINT("Disabling PIC mode\n");
        imcr_begin_sym_io();
    }
#endif
    
    return 0;
}


static int
handle_ioapic (char * buf, void * priv)
{
    uint32_t num, pin;
    struct sys_info *sys = &nk_get_nautilus_info()->sys;
    struct ioapic *io;
    char all[80];
    char what[80];
    int mask=0;

    if (sscanf(buf,"ioapic %u %s %s",&num,all,what)==3) { 
        if (num>=sys->num_ioapics) { 
            nk_vc_printf("unknown ioapic\n");
            return 0;
        }
        if (what[0]!='m' && what[0]!='u') { 
            nk_vc_printf("unknown ioapic request (mask|unmask)\n");
            return 0;
        }
        mask = what[0]=='m';

        if (all[0]!='a') { 
            if (sscanf(all,"%u",&pin)!=1) { 
                nk_vc_printf("unknown ioapic request (pin|all)\n");
                return 0;
            }
            if (mask) { 
                nk_vc_printf("masking ioapic %u pin %u\n",num,pin);
                ioapic_mask_irq(sys->ioapics[num], pin);
            } else {
                nk_vc_printf("unmasking ioapic %u pin %u\n",num,pin);
                ioapic_unmask_irq(sys->ioapics[num], pin);
            }
        } else {
            for (pin=0;pin<sys->ioapics[num]->num_entries;pin++) { 
                if (mask) { 
                    nk_vc_printf("masking ioapic %u pin %u\n",num,pin);
                    ioapic_mask_irq(sys->ioapics[num], pin);
                } else {
                    nk_vc_printf("unmasking ioapic %u pin %u\n",num,pin);
                    ioapic_unmask_irq(sys->ioapics[num], pin);
                }
            }
        }
        return 0;
    }

    if (sscanf(buf,"ioapic %s",what)==1) { 
        if (what[0]=='l' || what[0]=='d') { 
            for (num=0;num<sys->num_ioapics;num++) {
                io = sys->ioapics[num];
                nk_vc_printf("ioapic %u: id=%u %u pins address=%p\n",
                        num, io->id, io->num_entries, (void*)io->base);
                if (what[0]=='d') { 
                    uint64_t entry;
                    for (pin=0;pin<io->num_entries;pin++) { 
                        entry = (uint64_t) ioapic_read_reg(io, 0x10 + 2*pin);
                        entry |= ((uint64_t) ioapic_read_reg(io, 0x10 + 2*pin+1));

                        nk_vc_printf("  pin %2u -> %016lx (dest 0x%lx mask %lu vec 0x%lx%s)\n", 
                                pin, entry, (entry>>56)&0xffLU, (entry>>16)&0x1LU,
                                entry&0xffLU, (entry&0xffLU) == 0xf7 ? " panic" : "");
                    }
                }
            }
            return 0;
        } else {
            nk_vc_printf("Unknown ioapic request\n");
            return 0;
        }				       
    }

    nk_vc_printf("unknown ioapic request\n");
    return 0;
}


static struct shell_cmd_impl ioapic_impl = {
    .cmd      = "ioapic",
    .help_str = "ioapic <list|dump> | ioapic num <pin|all> <mask|unmask>",
    .handler  = handle_ioapic,
};
nk_register_shell_cmd(ioapic_impl);
