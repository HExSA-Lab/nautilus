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
#include <dev/pci.h>
#include <nautilus/cpu.h>
#include <nautilus/intrinsics.h>
#include <nautilus/mm.h>

#ifndef NAUT_CONFIG_DEBUG_PCI
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define PCI_PRINT(fmt, args...) printk("PCI: " fmt, ##args)


uint16_t 
pci_cfg_readw (uint8_t bus, 
               uint8_t slot,
               uint8_t fun,
               uint8_t off)
{
    uint32_t addr;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfun  = (uint32_t)fun;
    uint32_t ret;

    addr = (lbus  << PCI_BUS_SHIFT) | 
           (lslot << PCI_SLOT_SHIFT) | 
           (lfun  << PCI_FUN_SHIFT) |
           PCI_REG_MASK(off) | 
           PCI_ENABLE_BIT;

    outl(addr, PCI_CFG_ADDR_PORT);
    ret = inl(PCI_CFG_DATA_PORT);
    return (ret >> ((off & 0x2) * 8)) & 0xffff;
}


uint32_t 
pci_cfg_readl (uint8_t bus, 
               uint8_t slot,
               uint8_t fun,
               uint8_t off)
{
    uint32_t addr;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfun  = (uint32_t)fun;

    addr = (lbus  << PCI_BUS_SHIFT) | 
           (lslot << PCI_SLOT_SHIFT) | 
           (lfun  << PCI_FUN_SHIFT) |
           PCI_REG_MASK(off) | 
           PCI_ENABLE_BIT;

    outl(addr, PCI_CFG_ADDR_PORT);
    return inl(PCI_CFG_DATA_PORT);
}

void
pci_cfg_writew (uint8_t bus, 
		uint8_t slot,
		uint8_t fun,
		uint8_t off,
		uint16_t val)
{
    uint32_t addr;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfun  = (uint32_t)fun;
    uint32_t ret;

    addr = (lbus  << PCI_BUS_SHIFT) | 
           (lslot << PCI_SLOT_SHIFT) | 
           (lfun  << PCI_FUN_SHIFT) |
           PCI_REG_MASK(off) | 
           PCI_ENABLE_BIT;

    outl(addr, PCI_CFG_ADDR_PORT);
    outw(val,PCI_CFG_DATA_PORT);
}


void
pci_cfg_writel (uint8_t bus, 
		uint8_t slot,
		uint8_t fun,
		uint8_t off,
		uint32_t val)
{
    uint32_t addr;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfun  = (uint32_t)fun;

    addr = (lbus  << PCI_BUS_SHIFT) | 
           (lslot << PCI_SLOT_SHIFT) | 
           (lfun  << PCI_FUN_SHIFT) |
           PCI_REG_MASK(off) | 
           PCI_ENABLE_BIT;

    outl(addr, PCI_CFG_ADDR_PORT);
    outl(val, PCI_CFG_DATA_PORT);
}


static inline uint16_t
pci_dev_valid (uint8_t bus, uint8_t slot)
{
    return (pci_cfg_readw(bus, slot, 0, 0) != 0xffff);
}


static inline uint8_t 
pci_get_base_class (uint8_t bus, uint8_t dev, uint8_t fun)
{
    return (pci_cfg_readl(bus, dev, fun, 0x8) >> 24) & 0xff;
}


static inline uint8_t
pci_get_sub_class (uint8_t bus, uint8_t dev, uint8_t fun)
{
    return (pci_cfg_readl(bus, dev, fun, 0x8) >> 16) & 0xff;
}


static inline uint8_t 
pci_get_rev_id (uint8_t bus, uint8_t dev, uint8_t fun)
{
    return (pci_cfg_readw(bus, dev, fun, 0x8) & 0xff);
}

static inline uint8_t
pci_get_hdr_type (uint8_t bus, uint8_t dev, uint8_t fun)
{
    return (pci_cfg_readl(bus, dev, fun, 0xc) >> 16)& 0xff;
}


static inline uint16_t
pci_get_vendor_id (uint8_t bus, uint8_t dev, uint8_t fun)
{
    return pci_cfg_readw(bus, dev, fun, 0x0);
}


static inline uint16_t
pci_get_dev_id (uint8_t bus, uint8_t dev, uint8_t fun)
{
    return (pci_cfg_readw(bus, dev, fun, 0x2) & 0xffff);
}


static inline uint8_t 
pci_get_sec_bus (uint8_t bus, uint8_t dev, uint8_t fun)
{
    return (pci_cfg_readl(bus, dev, fun, 0x18) >> 0x8) & 0xff;
}


static void
pci_add_dev_to_bus (struct pci_dev * dev, 
                    struct pci_bus * bus)
{
    dev->bus = bus;
    list_add(&(dev->dev_node), &(bus->dev_list));
}

static void
pci_copy_cfg_space(struct pci_dev *dev, struct pci_bus *bus)
{
  uint32_t i;
  // 4 bytes at a time
  for (i=0;i<sizeof(dev->cfg);i+=4) {
    ((uint32_t*)(&dev->cfg))[i/4] = pci_cfg_readl(bus->num,dev->num,0,i);
  }
}


static struct pci_dev*
pci_dev_create (uint32_t num, struct pci_bus * bus)
{
    struct pci_dev * dev = NULL;
    dev = malloc(sizeof(struct pci_dev));
    if (!dev) {
        ERROR_PRINT("Could not allocate PCI device\n");
        return NULL;
    }
    memset(dev, 0, sizeof(struct pci_dev));

    dev->num = num;

    pci_copy_cfg_space(dev,bus);

    pci_add_dev_to_bus(dev, bus);

    return dev;
}


static void
pci_add_bus (struct pci_bus * bus, struct pci_info * pci)
{
    pci->num_buses++;
    list_add( &(bus->bus_node), &(pci->bus_list));
}


static struct pci_bus*
pci_bus_create (uint32_t num, struct pci_info * pci)
{
    struct pci_bus * bus = NULL;
    bus = malloc(sizeof(struct pci_bus));
    if (!bus) {
        ERROR_PRINT("Could not allocate PCI bus\n");
        return NULL;
    }
    memset(bus, 0, sizeof(struct pci_bus));

    bus->num = num;
    INIT_LIST_HEAD(&(bus->dev_list));

    pci_add_bus(bus, pci);
    return bus;
}
                

static void pci_fun_probe(struct pci_info * pci, uint8_t bus, uint8_t dev, uint8_t fun);

static void
pci_dev_probe (struct pci_bus * bus, uint8_t dev)
{
    uint8_t fun;
    uint16_t vendor_id;
    uint8_t hdr_type;

    fun = 0;

    vendor_id = pci_get_vendor_id(bus->num, dev, fun);
    if (vendor_id == 0xffff) {
        return;
    }

    PCI_PRINT("%04d:%02d:%02d.%d %x:%x (rev %d)\n", 0, bus->num, dev, fun, vendor_id, 
            pci_get_dev_id(bus->num, dev, fun), 
            pci_get_rev_id(bus->num, dev, fun));

    if (pci_dev_create(dev, bus) == NULL) {
        ERROR_PRINT("Could not create PCI device\n");
        return;
    }

    pci_fun_probe(bus->pci, bus->num, dev, fun);

    hdr_type = pci_get_hdr_type(bus->num, dev, fun);

    /* multi-function device */
    if ((hdr_type & 0x80) != 0) {
      PCI_PRINT("Multifunction Device\n");
        for (fun = 1; fun < PCI_MAX_FUN; fun++) {
            if (pci_get_vendor_id(bus->num, dev, fun) != 0xffff) {
                pci_fun_probe(bus->pci, bus->num, dev, fun);
            }
        }
    }
}


static void 
pci_bus_probe (struct pci_info * pci, uint8_t bus)
{
    uint8_t dev;
    uint8_t dev_found = 0;
    struct pci_bus * bus_ptr = NULL;


    for (dev = 0; dev < PCI_MAX_DEV; dev++) {
        if (pci_get_vendor_id(bus, dev, 0) != 0xffff) {
            dev_found = 1;
            break;
        }
    }

    if (dev_found) {
        bus_ptr = pci_bus_create(bus, pci);
        if (!bus_ptr) {
            ERROR_PRINT("Could not create PCI bus\n");
            return;
        }

        for (dev = 0; dev < PCI_MAX_DEV; dev++) {
            pci_dev_probe(bus_ptr, dev);
        }
    }
}



static void
pci_fun_probe (struct pci_info * pci, uint8_t bus, uint8_t dev, uint8_t fun)
{
    uint8_t base_class; 
    uint8_t sub_class; 
    uint8_t sec_bus;

    base_class = pci_get_base_class(bus, dev, fun);
    sub_class = pci_get_sub_class(bus, dev, fun);

    if (base_class == PCI_CLASS_BRIDGE && sub_class == PCI_SUBCLASS_BRIDGE_PCI) {
        sec_bus = pci_get_sec_bus(bus, dev, fun);
        pci_bus_probe(pci, sec_bus);
    }
}


static void 
pci_bus_scan (struct pci_info * pci)
{
    uint8_t hdr_type;

    hdr_type = pci_get_hdr_type(0, 0, 0);
    if ((hdr_type & 0x80) == 0) {
        /* only one PCI host controller */
        pci_bus_probe(pci, 0);

    } else {
        ERROR_PRINT("Host with multiple PCI host controllers not supported\n");
    }
}


int 
pci_init (struct naut_info * naut)
{
    struct pci_info * pci = NULL;

    pci = malloc(sizeof(struct pci_info));
    if (!pci) {
        ERROR_PRINT("Could not allocate PCI struct\n");
        return -1;
    }
    memset(pci, 0, sizeof(struct pci_info));

    INIT_LIST_HEAD(&(pci->bus_list));

    PCI_PRINT("Probing PCI bus...\n");
    pci_bus_scan(pci);

    naut->sys.pci = pci;

    return 0;
}

