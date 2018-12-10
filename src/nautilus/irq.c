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
#include <nautilus/idt.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/mm.h>

#define PIC_MASTER_CMD_PORT  0x20
#define PIC_MASTER_DATA_PORT 0x21
#define PIC_SLAVE_CMD_PORT   0xa0
#define PIC_SLAVE_DATA_PORT  0xa1

#define MAX_IRQ_NUM    15     // this is really PIC-specific


/* NOTE: the APIC organizes interrupt priorities as follows:
 * class 0: interrupt vectors 0-15
 * class 1: interrupt vectors 16-31
 * .
 * .
 * .
 * class 15: interrupt vectors 0x1f - 0xff
 *
 * The upper 4 bits indicate the priority class
 * and the lower 4 represent the int number within
 * that class
 *
 * higher class = higher priority
 *
 * higher vector = higher priority within the class
 *
 * Because x86 needs the vectors 0-31, the first
 * two classes are reserved
 *
 * In short, we use vectors above 31, with increasing
 * priority with increasing vector number
 *
 */


uint8_t
irq_to_vec (uint8_t irq)
{
    return nk_get_nautilus_info()->sys.int_info.irq_map[irq].vector;
}

void
irqmap_set_ioapic (uint8_t irq, struct ioapic * ioapic)
{
    struct naut_info * naut = nk_get_nautilus_info();
    naut->sys.int_info.irq_map[irq].ioapic = ioapic;
    naut->sys.int_info.irq_map[irq].assigned = 1;
}

static inline void 
set_irq_vector (uint8_t irq, uint8_t vector)
{
    nk_get_nautilus_info()->sys.int_info.irq_map[irq].vector = vector;
}


void 
nk_mask_irq (uint8_t irq)
{
    struct naut_info * naut = nk_get_nautilus_info();
	if (nk_irq_is_assigned(irq)) {
		ioapic_mask_irq(naut->sys.int_info.irq_map[irq].ioapic, irq);
	}
}


void
nk_unmask_irq (uint8_t irq)
{
    struct naut_info * naut = nk_get_nautilus_info();
	if (nk_irq_is_assigned(irq)) {
		ioapic_unmask_irq(naut->sys.int_info.irq_map[irq].ioapic, irq);
	}
}


uint8_t
nk_irq_is_assigned (uint8_t irq)
{
    struct naut_info * naut = nk_get_nautilus_info();
    return naut->sys.int_info.irq_map[irq].assigned;
}


/* 
 * this should only be used when the OS interrupt vector
 * is known ahead of time, that is, *not* in the case
 * of external interrupts. It's much more likely you
 * should be using register_irq_handler
 */
int 
register_int_handler (uint16_t int_vec, 
                      int (*handler)(excp_entry_t *, excp_vec_t, void *),
                      void * priv_data)
{

    if (!handler) {
        ERROR_PRINT("Attempt to register interrupt %d with invalid handler\n", int_vec);
        return -1;
    }

    if (int_vec > 0xff) {
        ERROR_PRINT("Attempt to register invalid interrupt(0x%x)\n", int_vec);
        return -1;
    }

    idt_assign_entry(int_vec, (ulong_t)handler, (ulong_t)priv_data);

    return 0;
}


int 
register_irq_handler (uint16_t irq, 
                      int (*handler)(excp_entry_t *, excp_vec_t, void *),
                      void * priv_data)
{
    uint8_t int_vector;

    if (!handler) {
        ERROR_PRINT("Attempt to register IRQ %d with invalid handler\n", irq);
        return -1;
    }

    if (irq > MAX_IRQ_NUM) {
        ERROR_PRINT("Attempt to register invalid IRQ (0x%x)\n", irq);
        return -1;
    }

    int_vector = irq_to_vec(irq);

    idt_assign_entry(int_vector, (ulong_t)handler, (ulong_t)priv_data);

    return 0;
}


void 
disable_8259pic (void)
{
    printk("Disabling legacy 8259 PIC\n");
    outb(0xff, PIC_MASTER_DATA_PORT);
    outb(0xff, PIC_SLAVE_DATA_PORT);
}

void 
imcr_begin_sym_io (void)
{
    /* ICMR */
    outb(0x70, 0x22);
    outb(0x01, 0x23);
}


uint8_t 
nk_int_matches_bus (struct nk_int_entry * entry, const char * bus_type, const uint8_t len)
{
    uint8_t src_bus;
    struct nk_bus_entry * bus;

    /* find the src bus id */
    src_bus = entry->src_bus_id;

    list_for_each_entry(bus, 
            &(nk_get_nautilus_info()->sys.int_info.bus_list),
            elm) {

        if (bus->bus_id == entry->src_bus_id) {
            return !strncmp(bus->bus_type, bus_type, len);
        }

    }

    return 0;
}


void 
nk_add_bus_entry (const uint8_t bus_id, const char * bus_type)
{
    struct nk_bus_entry * bus = NULL;
    struct naut_info * naut = nk_get_nautilus_info();

    bus = mm_boot_alloc(sizeof(struct nk_bus_entry));
    if (!bus) {
        ERROR_PRINT("Could not allocate bus entry\n");
        return;
    }
    memset(bus, 0, sizeof(struct nk_bus_entry));

    bus->bus_id  = bus_id;
    memcpy((void*)bus->bus_type, bus_type, 6);

    list_add(&(bus->elm), &(naut->sys.int_info.bus_list));

}

void
nk_add_int_entry (int_trig_t trig_mode,
                  int_pol_t  polarity,
                  int_type_t type,
                  uint8_t    src_bus_id,
                  uint8_t    src_bus_irq,
                  uint8_t    dst_ioapic_intin,
                  uint8_t    dst_ioapic_id)
{
    struct nk_int_entry * new = NULL;
    struct naut_info * naut = nk_get_nautilus_info();

    new = mm_boot_alloc(sizeof(struct nk_int_entry));
    if (!new) {
        ERROR_PRINT("Could not allocate IRQ entry\n");
        return;
    }
    memset(new, 0, sizeof(struct nk_int_entry));

    new->trig_mode        = trig_mode;
    new->polarity         = polarity;
    new->type             = type;
    new->src_bus_id       = src_bus_id;
    new->src_bus_irq      = src_bus_irq;
    new->dst_ioapic_intin = dst_ioapic_intin;
    new->dst_ioapic_id    = dst_ioapic_id;

    list_add(&(new->elm), &(naut->sys.int_info.int_list));
}


int 
nk_int_init (struct sys_info * sys)
{
    struct nk_int_info * info = &(sys->int_info);
    int i;
    uint8_t vector;

    /* set it up so we get an illegal vector if we don't
     * assign IRQs properly. 0xff is reserved for APIC 
     * suprious interrupts */
    for (i = 0; i < 256; i++) {
        set_irq_vector(i, 0xfe);
    }

    /* we're going to count down in decreasing priority 
     * when we run out of vectors, we'll stop */
    for (i = 0, vector = 0xef; vector > 0x1f; vector--, i++) {
        set_irq_vector(i, vector);
    }

    INIT_LIST_HEAD(&(info->int_list));
    INIT_LIST_HEAD(&(info->bus_list));

    return 0;
}


