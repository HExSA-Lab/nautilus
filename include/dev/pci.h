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
#ifndef __PCI_H__
#define __PCI_H__

#include <nautilus/list.h>

#define PCI_CFG_ADDR_PORT 0xcf8
#define PCI_CFG_DATA_PORT 0xcfc

#define PCI_SLOT_SHIFT 11
#define PCI_BUS_SHIFT  16
#define PCI_FUN_SHIFT  8

#define PCI_REG_MASK(x) ((x) & 0xfc)

#define PCI_ENABLE_BIT 0x80000000UL

#define PCI_MAX_BUS 256
#define PCI_MAX_DEV 32
#define PCI_MAX_FUN 8

#define PCI_CLASS_LEGACY  0x0
#define PCI_CLASS_STORAGE 0x1
#define PCI_CLASS_NET     0x2
#define PCI_CLASS_DISPLAY 0x3
#define PCI_CLASS_MULTIM  0x4
#define PCI_CLASS_MEM     0x5
#define PCI_CLASS_BRIDGE  0x6
#define PCI_CLASS_SIMPLE  0x7
#define PCI_CLASS_BSP     0x8
#define PCI_CLASS_INPUT   0x9
#define PCI_CLASS_DOCK    0xa
#define PCI_CLASS_PROC    0xb
#define PCI_CLASS_SERIAL  0xc
#define PCI_CLASS_WIRELESS 0xd
#define PCI_CLASS_INTIO    0xe
#define PCI_CLASS_SAT      0xf
#define PCI_CLASS_CRYPTO   0x10
#define PCI_CLASS_SIG      0x11
#define PCI_CLASS_NOCLASS  0xff


#define PCI_SUBCLASS_BRIDGE_PCI 0x4


struct naut_info; 

struct pci_bus {
    uint32_t num;
    struct list_head bus_node;
    struct list_head dev_list;
    struct pci_info * pci;
};


struct pci_cfg_space {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t cmd;
    uint16_t status;
    uint8_t  rev_id;
    uint8_t  prog_if;
    uint8_t  subclass;
    uint8_t  class_code;
    uint8_t  cl_size;
    uint8_t  lat_timer;
    uint8_t  hdr_type;
    uint8_t  bist;

    union {
        // type = 00h (device)
        struct {
            uint32_t bars[6];
            uint32_t cardbus_cis_ptr;
            uint16_t subsys_vendor_id;
            uint16_t subsys_id;
            uint32_t exp_rom_bar;
            uint8_t  cap_ptr;
            uint8_t  rsvd[7];
            uint8_t  intr_line;
            uint8_t  intr_pin;
            uint8_t  min_grant;
            uint8_t  max_latency;
            uint32_t data[48];
        } __packed dev_cfg;

        // type = 01h (PCI-to-PCI bridge)
        struct {
            uint32_t bars[2];
            uint8_t  primary_bus_num;
            uint8_t  secondary_bus_num;
            uint8_t  sub_bus_num;
            uint8_t  secondary_lat_timer;
            uint8_t  io_base;
            uint8_t  io_limit;
            uint16_t secondary_status;
            uint16_t mem_base;
            uint16_t mem_limit;
            uint16_t prefetch_mem_base;
            uint16_t prefetch_mem_limit;
            uint32_t prefetch_base_upper;
            uint32_t prefetch_limit_upper;
            uint16_t io_base_upper;
            uint16_t io_limit_upper;
            uint8_t  cap_ptr;
            uint8_t  rsvd[3];
            uint32_t exp_rom_bar;
            uint8_t  intr_line;
            uint8_t  intr_pin;
            uint16_t bridge_ctrl;
            uint32_t data[48];
        } __packed pci_to_pci_bridge_cfg;

        struct {
            uint32_t cardbus_socket_bar;
            uint8_t  cap_list_offset;
            uint8_t  rsvd;
            uint16_t secondary_status;
            uint8_t  pci_bus_num;
            uint8_t  cardbus_bus_num;
            uint8_t  sub_bus_num;
            uint8_t  cardbus_lat_timer;
            uint32_t mem_base0;
            uint32_t mem_limit0;
            uint32_t mem_base1;
            uint32_t mem_limit1;
            uint32_t io_base0;
            uint32_t io_limit0;
            uint32_t io_base1;
            uint32_t io_limit1;
            uint8_t  intr_line;
            uint8_t  intr_pin;
            uint16_t bridge_ctrl;
            uint16_t subsys_dev_id;
            uint16_t subsys_vendor_id;
            uint32_t legacy_base;
            uint32_t rsvd1[14];
            uint32_t data[32];
        } __packed cardbus_bridge_cfg;

    } __packed;

} __packed;


struct pci_dev {
    uint32_t num;
    struct pci_bus * bus;
    struct list_head dev_node;
    struct pci_cfg_space cfg;
};


struct pci_info {
    uint32_t num_buses;
    struct list_head bus_list;
};



uint16_t pci_cfg_readw(uint8_t bus, uint8_t slot, uint8_t fun, uint8_t off);
uint32_t pci_cfg_readl(uint8_t bus, uint8_t slot, uint8_t fun, uint8_t off);

void pci_cfg_writew(uint8_t bus, uint8_t slot, uint8_t fun, uint8_t off, uint16_t val);
void pci_cfg_writel(uint8_t bus, uint8_t slot, uint8_t fun, uint8_t off, uint32_t val);

int pci_init (struct naut_info * naut);


#endif
