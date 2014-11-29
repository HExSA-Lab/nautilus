#include <nautilus/nautilus.h>
#include <nautilus/paging.h>
#include <dev/ioapic.h>
#include <nautilus/irq.h>

#ifndef NAUT_CONFIG_DEBUG_IOAPIC
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif


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
    val = ioapic_read_reg(ioapic, IOAPIC_IRQ_ENTRY_LO(irq));
    ioapic_write_reg(ioapic, IOAPIC_IRQ_ENTRY_LO(irq), val | IOAPIC_MASK_IRQ);
}


void 
ioapic_unmask_irq (struct ioapic * ioapic, uint8_t irq)
{
    uint32_t val;
    val = ioapic_read_reg(ioapic, IOAPIC_IRQ_ENTRY_LO(irq));
    ioapic_write_reg(ioapic, IOAPIC_IRQ_ENTRY_LO(irq), val & ~IOAPIC_MASK_IRQ);
}


/* TODO: break this down more */
static void 
ioapic_assign_irq (struct ioapic * ioapic,
                   uint8_t irq, 
                   uint8_t vector,
                   uint8_t polarity, 
                   uint8_t trigger_mode)
{
    ioapic_write_irq_entry(ioapic, irq, 
                           vector                             |
                           //(DELMODE_LOWEST << DEL_MODE_SHIFT) |
                           (DELMODE_FIXED << DEL_MODE_SHIFT) |
                           (polarity << INTPOL_SHIFT)         | 
                           (trigger_mode << TRIG_MODE_SHIFT));


    DEBUG_PRINT("just wrote to IOAPIC reg %d (=0x%lx)\n", irq, ioapic_read_irq_entry(ioapic, irq));
}


static uint8_t 
ioapic_get_id (struct ioapic * ioapic)
{
    uint32_t ret;
    ret = ioapic_read_reg(ioapic, IOAPICID_REG);
    return (ret >> 23) & 0xf;
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

    if (nk_map_page_nocache(ioapic->base, PTE_PRESENT_BIT|PTE_WRITABLE_BIT) == -1) {
        panic("Could not map IOAPIC\n");
        return -1;
    }

    DEBUG_PRINT("Initializing IOAPIC, id: %d\n", ioapic_get_id(ioapic));
    DEBUG_PRINT("version: %x\n", ioapic_get_version(ioapic));
    DEBUG_PRINT("Mapping IOAPIC at %p\n", (void*)ioapic->base);

    DEBUG_PRINT("Assigning IO vectors\n");

    for (i = 0; i < MAX_IRQ_NUM; i++) {
        ioapic_assign_irq(ioapic, i, irq_to_vec(i), PIN_POLARITY_HI, TRIGGER_MODE_EDGE);  
    }

    ioapic_mask_irq(ioapic, 16);
    ioapic_mask_irq(ioapic, 17);
    ioapic_mask_irq(ioapic, 18);
    ioapic_mask_irq(ioapic, 19);

    /* disable IRQ 2 */
    //ioapic_mask_irq(ioapic, 2);

    /* disable IRQ 20..23 */
    ioapic_mask_irq(ioapic, 20);
    ioapic_mask_irq(ioapic, 21);
    ioapic_mask_irq(ioapic, 22);
    ioapic_mask_irq(ioapic, 23);


    return 0;
}


int 
ioapic_init (struct sys_info * sys)
{
    int i = 0;
    for (i = 0; i < sys->num_ioapics; i++) {
        if (__ioapic_init(&(sys->ioapics[i])) < 0) {
            ERROR_PRINT("Couldn't initialize IOAPIC\n");
            return -1;
        }
    }

    /* TODO: assign unique ids to IOAPICs */

    if (sys->pic_mode_enabled) {
        DEBUG_PRINT("Disabling PIC mode\n");
        imcr_begin_sym_io();
    }

    return 0;
}

