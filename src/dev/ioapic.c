#include <nautilus/nautilus.h>
#include <nautilus/paging.h>
#include <dev/ioapic.h>
#include <nautilus/irq.h>
#include <lib/liballoc.h>

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



static void
ioapic_write_irq_entry (struct ioapic * ioapic, uint8_t irq, uint64_t val)
{
    /* first disable it */
    ioapic_mask_irq(ioapic, irq);

    /* hi */
    ioapic_write_reg(ioapic, IOAPIC_IRQ_ENTRY_HI(irq), (uint32_t)(val >> 32));

    /* lo */
    ioapic_write_reg(ioapic, IOAPIC_IRQ_ENTRY_LO(irq), (uint32_t)(val & 0xffffffff));

    /* re-enable it */
    ioapic_unmask_irq(ioapic, irq);
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
                   uint8_t trigger_mode)
{
    ASSERT(irq < ioapic->num_entries);
    ioapic_write_irq_entry(ioapic, irq, 
                           vector                             |
                           (DELMODE_FIXED << DEL_MODE_SHIFT) |
                           (polarity << INTPOL_SHIFT)         | 
                           (trigger_mode << TRIG_MODE_SHIFT));


    IOAPIC_PRINT("Wrote IOAPIC reg %d (=0x%lx)\n", irq, ioapic_read_irq_entry(ioapic, irq));
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


static int
__ioapic_init (struct ioapic * ioapic)
{
    int i;
    struct nk_int_entry * ioint = NULL;

    if (nk_map_page_nocache(PAGE_MASK(ioapic->base), PTE_PRESENT_BIT|PTE_WRITABLE_BIT) == -1) {
        panic("Could not map IOAPIC\n");
        return -1;
    }

    /* get the last entry we can access for this IOAPIC */
    ioapic->num_entries = ioapic_get_max_entry(ioapic) + 1;

    ioapic->entries = malloc(sizeof(struct iored_entry)*ioapic->num_entries);
    if (!ioapic->entries) {
        ERROR_PRINT("Could not allocate IOAPIC %u INT entries\n");
        return -1;
    }
    memset(ioapic->entries, 0, sizeof(struct iored_entry)*ioapic->num_entries);

    IOAPIC_PRINT("Initializing IOAPIC (ID=0x%x)\n", ioapic_get_id(ioapic));
    IOAPIC_PRINT("\tVersion=0x%x\n", ioapic_get_version(ioapic));
    IOAPIC_PRINT("\tMapping at %p\n", (void*)ioapic->base);
    IOAPIC_PRINT("\tNum Entries: %u\n", ioapic->num_entries);


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
                    trig);

            struct iored_entry * iored_entry = &(ioapic->entries[ioint->dst_ioapic_intin]);
            iored_entry->boot_info  = ioint;
            iored_entry->actual_irq = newirq;

            irqmap_set_ioapic(newirq, ioapic);
        }
    }


    IOAPIC_DEBUG("Masking all IORED entries\n");
    for (i = 0; i < ioapic->num_entries; i++) {
        ioapic_mask_irq(ioapic, i);
    }

    /* we come out of this with all IOAPIC entries masked 
     * we can later enable them using the more general
     * IRQ functions 
     */

    return 0;
}


int 
ioapic_init (struct sys_info * sys)
{
    int i = 0;
    for (i = 0; i < sys->num_ioapics; i++) {
        if (__ioapic_init(sys->ioapics[i]) < 0) {
            ERROR_PRINT("Couldn't initialize IOAPIC\n");
            return -1;
        }
    }

    if (sys->pic_mode_enabled) {
        IOAPIC_PRINT("Disabling PIC mode\n");
        imcr_begin_sym_io();
    }

    return 0;
}

