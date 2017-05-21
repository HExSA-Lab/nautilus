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
#ifndef __IRQ_H__
#define __IRQ_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/nautilus.h>
#include <nautilus/cpu.h>
#include <nautilus/cpu_state.h>
#include <nautilus/list.h>

extern void apic_do_eoi();

#define IRQ_HANDLER_END() apic_do_eoi()

typedef enum { INT_TYPE_INT, INT_TYPE_NMI, INT_TYPE_SMI, INT_TYPE_EXT } int_type_t;
typedef enum { INT_POL_BUS, INT_POL_ACTHI, INT_POL_RSVD, INT_POL_ACTLO } int_pol_t;
typedef enum { INT_TRIG_BUS, INT_TRIG_EDGE, INT_TRIG_RSVD, INT_TRIG_LEVEL } int_trig_t;

struct nk_int_entry {
    int_trig_t trig_mode;
    int_pol_t  polarity;
    int_type_t type;
    uint8_t    src_bus_id;
    uint8_t    src_bus_irq;
    uint8_t    dst_ioapic_intin;
    uint8_t    dst_ioapic_id;

    struct list_head elm;
};

struct nk_bus_entry {
    uint8_t bus_id;
    char    bus_type[6];

    struct list_head elm;
};

void nk_mask_irq(uint8_t irq);
void nk_unmask_irq(uint8_t irq);
uint8_t nk_irq_is_assigned(uint8_t irq);

uint8_t irq_to_vec (uint8_t irq);
void irqmap_set_ioapic (uint8_t irq, struct ioapic * ioapic);
void disable_8259pic(void);
void imcr_begin_sym_io(void);
int register_irq_handler (uint16_t irq, 
                          int (*handler)(excp_entry_t *, excp_vec_t, void *priv_data),
                          void * priv_data);
int register_int_handler (uint16_t int_vec,
                          int (*handler)(excp_entry_t *, excp_vec_t, void *priv_data),
                          void * priv_data);

int nk_int_init(struct sys_info * sys);

uint8_t nk_int_matches_bus(struct nk_int_entry * entry, const char * bus_type, const uint8_t len);
void nk_add_bus_entry(const uint8_t bus_id, const char * bus_type);
void nk_add_int_entry (int_trig_t trig_mode,
                  int_pol_t  polarity,
                  int_type_t type,
                  uint8_t    src_bus_id,
                  uint8_t    src_bus_irq,
                  uint8_t    dst_ioapic_intin,
                  uint8_t    dst_ioapic_id);


#ifdef __cplusplus
}
#endif

#endif
