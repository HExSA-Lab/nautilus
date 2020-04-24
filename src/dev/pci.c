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
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <dev/pci.h>
#include <nautilus/cpu.h>
#include <nautilus/intrinsics.h>
#include <nautilus/mm.h>
#include <nautilus/shell.h>
#include <nautilus/dev.h>

#ifndef NAUT_CONFIG_DEBUG_PCI
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define PCI_PRINT(fmt, args...) printk("PCI: " fmt, ##args)
#define PCI_DEBUG(fmt, args...) DEBUG_PRINT("PCI: " fmt, ##args)
#define PCI_WARN(fmt, args...)  WARN_PRINT("PCI: " fmt, ##args)
#define PCI_ERROR(fmt, args...) ERROR_PRINT("PCI: " fmt, ##args)


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
pci_dev_valid (uint8_t bus, uint8_t slot, uint8_t fun)
{
    return (pci_cfg_readw(bus, slot, fun, 0) != 0xffff);
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


static inline uint16_t
pci_get_cmd (uint8_t bus, uint8_t dev, uint8_t fun)
{
    return (pci_cfg_readw(bus, dev, fun, 0x4) & 0xffff);
}


static inline void
pci_set_cmd (uint8_t bus, uint8_t dev, uint8_t fun, uint16_t val)
{
	pci_cfg_writew(bus, dev, fun, 0x6, val);
}


static inline uint16_t
pci_get_status (uint8_t bus, uint8_t dev, uint8_t fun)
{
    return (pci_cfg_readw(bus, dev, fun, 0x6) & 0xffff);
}


static void
pci_add_dev_to_bus (struct pci_dev * dev, 
                    struct pci_bus * bus)
{
    dev->bus = bus;
    list_add_tail(&(dev->dev_node), &(bus->dev_list));
}

static void
pci_copy_cfg_space(struct pci_dev *dev, struct pci_bus *bus)
{
  uint32_t i;
  // 4 bytes at a time
  for (i=0;i<sizeof(dev->cfg);i+=4) {
    ((uint32_t*)(&dev->cfg))[i/4] = pci_cfg_readl(bus->num,dev->num,dev->fun,i);
  }
}


void *pci_msi_x_get_bar_start(struct pci_dev *dev, uint8_t bar_num)
{
  uint32_t bar = pci_dev_cfg_readl(dev, 0x10 + bar_num*4);

  PCI_DEBUG("bar %d: 0x%0x\n",bar_num, bar);

  if (bar & 0x0) {
    PCI_DEBUG("Cannot be an I/O bar...\n");
    return 0;
  }

  uint8_t mem_bar_type = (bar & 0x6) >> 1;

  // 32 bit memory only at the moment
  if (mem_bar_type != 0) {
    PCI_DEBUG("Cannot handle memory bar type 0x%x\n", mem_bar_type);
    return 0;
  }

  return (void*) (uint64_t) (bar & 0xfffffff0);
}


#define CEIL_DIV(x,y) (((x)/(y)) + !!((x)%(y)))

//
// CTRL word format:  enable function_mask 3 bits reserved, 11 bits of table size
//
// enable          - self explanatory (can only have one of INTX, MSI, MSI-X)
// function mask   - global mask, if 0, vector mask bits are used
// reserved
// table size      - N-1


static void pci_msi_x_detect(struct pci_dev *d)
{
  uint8_t co = pci_dev_get_capability(d,0x11);

  if (!co) {
    PCI_DEBUG("device does not have MSI-X capability\n");
    return;
  }

  PCI_DEBUG("device has MSI-X capability\n");

  d->msix.co = co;

  uint16_t ctrl = pci_dev_cfg_readl(d,co) >> 16;

  d->msix.size = (ctrl & 0x7ff) + 1;  // N-1 encoded

  PCI_DEBUG("initial msi-x ctrl reg is %x\n",ctrl);

  uint32_t table = pci_dev_cfg_readl(d,co+4);
  uint8_t  table_bar_num = table & 0x7;
  uint32_t table_off = table & (~0x7); // quad word aligned
  uint32_t pba = pci_dev_cfg_readl(d,co+8);
  uint8_t  pba_bar_num = pba & 0x7;
  uint32_t pba_off = pba & (~0x7); // quad word aligned

  PCI_DEBUG("table_bar_num=%u pba_bar_num=%u\n",table_bar_num,pba_bar_num);
  PCI_DEBUG("table_off=0x%x pba_off=0x%x\n",table_off,pba_off);

  if (table_bar_num>5 || pba_bar_num>5) {
    PCI_ERROR("Insane bar number (table_bar=%u, pba_bar=%u)\n", table_bar_num, pba_bar_num);
    return;
  }
  
  void *table_start = pci_msi_x_get_bar_start(d,table_bar_num);
  void *pba_start = pci_msi_x_get_bar_start(d,pba_bar_num);

  PCI_DEBUG("table_start=%p pba_start=%p\n",table_start,pba_start);

  if (!table_start || !pba_start) {
    PCI_ERROR("relevant bars have invalid starting address (table_start=%p, pba_start=%p)\n", table_start,pba_start);
    return;
  }

  d->msix.table = table_start + table_off;  
  d->msix.pending = pba_start + pba_off;  

  PCI_DEBUG("table of size %u located at %p\n", d->msix.size, d->msix.table);
  PCI_DEBUG("pending of size %u located at %p\n", CEIL_DIV(d->msix.size,8), d->msix.pending);

  d->msix.type = PCI_MSI_X;
  
}

// this must run after copying the config space
// and adding the device to a bus
static void pci_msi_detect(struct pci_dev *d)
{
  uint8_t co = pci_dev_get_capability(d, 0x5);
  if (!co) { 
    //PCI_DEBUG("device does not have MSI capability\n");
    return;
  }

  //PCI_DEBUG("device has MSI capabilitity\n");

  d->msi.co = co;

  uint16_t ctrl = pci_dev_cfg_readw(d,co+2);

  int has_64=0, has_pervec=0, num_vec=0;
  
  //PCI_DEBUG("initial msi ctrl reg is %x\n", ctrl);
  if (ctrl & 0x80) { 
    has_64 = 1;
  }
  if (ctrl & 0x100) { 
    has_pervec = 1;
  }
  if (has_64) {
    if (has_pervec) { 
      d->msi.type = PCI_MSI_64_PER_VEC;
    } else {
      d->msi.type = PCI_MSI_64;
    }
  } else {
    if (has_pervec) { 
      d->msi.type = PCI_MSI_32_PER_VEC;
    } else {
      d->msi.type = PCI_MSI_32;
    }
  }

  d->msi.num_vectors_needed = 1 << ((ctrl >> 1) & 0x7);
  
  PCI_DEBUG("device has MSI of type \"%s\" num_vec=%d\n",
	    d->msi.type == PCI_MSI_32 ? "32 bit MSI" :
	    d->msi.type == PCI_MSI_32_PER_VEC ? "32 bit MSI with per-vector masking" :
	    d->msi.type == PCI_MSI_64 ? "64 bit MSI" :
	    d->msi.type == PCI_MSI_64_PER_VEC ? "64 bit MSI with per-vector masking" :
	    "UNKNOWN",
	    d->msi.num_vectors_needed);
}


static struct pci_dev*
pci_dev_create (struct pci_bus * bus, uint32_t num, uint8_t func ) 
{
    struct pci_dev * dev = NULL;
    dev = malloc(sizeof(struct pci_dev));
    if (!dev) {
        PCI_ERROR("Could not allocate PCI device\n");
        return NULL;
    }
    memset(dev, 0, sizeof(struct pci_dev));

    dev->num = num;
    dev->fun = func;

    pci_copy_cfg_space(dev,bus);

    pci_add_dev_to_bus(dev, bus);

    pci_msi_detect(dev);

    pci_msi_x_detect(dev);

    return dev;
}


static void
pci_add_bus (struct pci_bus * bus, struct pci_info * pci)
{
    pci->num_buses++;
    list_add_tail( &(bus->bus_node), &(pci->bus_list));
}


static struct pci_bus*
pci_bus_create (uint32_t num, struct pci_info * pci)
{
    struct pci_bus * bus = NULL;
    bus = malloc(sizeof(struct pci_bus));
    if (!bus) {
        PCI_ERROR("Could not allocate PCI bus\n");
        return NULL;
    }
    memset(bus, 0, sizeof(struct pci_bus));

    bus->num = num;
	bus->pci = pci;

    INIT_LIST_HEAD(&(bus->dev_list));

    pci_add_bus(bus, pci);
    return bus;
}
                

static void pci_pci_bridge_probe(struct pci_info * pci, uint8_t bus, uint8_t dev, uint8_t fun);


/*
 * This function probes a particular function on a PCI bus.
 */
static void pci_func_probe (struct pci_bus * bus, uint8_t dev, uint8_t fun, int *ismultifunc)
{
    uint16_t vendor_id = 0;
    uint8_t hdr_type;
    
    *ismultifunc = 0;

    //PCI_DEBUG("probing %x:%x:%x\n", bus->num, dev, fun);
  
    vendor_id = pci_get_vendor_id(bus->num, dev, fun);


    /* No device present */
    if (vendor_id == 0xffff) {
      //PCI_DEBUG("Skipping nonexistent device %x:%x:%x\n", bus->num, dev, fun);
      return;
    }

    PCI_PRINT("%04d:%02d:%02d.%d %x:%x (rev %d)\n", 0, bus->num, dev, fun, vendor_id, 
            pci_get_dev_id(bus->num, dev, fun), 
            pci_get_rev_id(bus->num, dev, fun));


    /* create a logical representation of the device */
    if (pci_dev_create(bus,dev,fun) == NULL) {
        PCI_ERROR("Could not create PCI device\n");
        return;
    }

    // If it's a PCI<->PCI bridge, we need to recurse into the child bus
    pci_pci_bridge_probe(bus->pci, bus->num, dev, fun);

    hdr_type = pci_get_hdr_type(bus->num, dev, fun);

    if ((hdr_type & 0x80) != 0) {
        PCI_PRINT("[%04d:%02d.%d] Multifunction Device detected\n", 
		  bus->num, dev, fun);
	*ismultifunc = 1;
    }

}

static void pci_dev_probe (struct pci_bus * bus, uint8_t dev)
{
  int mf;
  uint8_t fun;
  
  pci_func_probe(bus,dev,0,&mf);

  if (mf) { 
    for (fun = 1; fun < PCI_MAX_FUN; fun++) {
      pci_func_probe(bus,dev,fun,&mf);
    }
  }
}

static int pci_already_have_bus(struct pci_info *pci, uint8_t bus_num) 
{
  struct list_head *curbus;
  
  list_for_each(curbus,&(pci->bus_list)) { 
    struct pci_bus *bus = list_entry(curbus,struct pci_bus,bus_node);
    if (bus->num == bus_num) { 
      return 1;
    }
  }
  return 0;
}


/*
 *
 * This function checks to see if there are devices
 * on this PCI bus, and if there are, it probes 
 * all possible devices.
 *
 */
static void 
pci_bus_probe (struct pci_info * pci, uint8_t bus)
{
    uint8_t dev;
    uint8_t dev_found = 0;
    struct pci_bus * bus_ptr = NULL;

    //PCI_DEBUG("probing bus 0x%x\n",bus);

    // probe/scan bus only once
    if (pci_already_have_bus(pci,bus)) { 
      return;
    }

    /* make sure we actually have at least one device on this bus */
    for (dev = 0; dev < PCI_MAX_DEV; dev++) {
        if (pci_get_vendor_id(bus, dev, 0) != 0xffff) {
            dev_found = 1;
            break;
        }
    }

    /* create a logical bus if we found devices here */
    if (dev_found) {
        bus_ptr = pci_bus_create(bus, pci);
        if (!bus_ptr) {
            PCI_ERROR("Could not create PCI bus\n");
            return;
        }

        for (dev = 0; dev < PCI_MAX_DEV; dev++) {
            pci_dev_probe(bus_ptr, dev);
        }
    } else {
      //PCI_DEBUG("not adding bus since it has no devices\n");
    }
}



/* 
 * 
 * This function checks a device's functions to see if it is
 * a PCI-PCI bridge. If so, we need to probe that secondary bus
 *
 */
static void
pci_pci_bridge_probe (struct pci_info * pci, uint8_t bus, uint8_t dev, uint8_t fun)
{
    uint8_t base_class; 
    uint8_t sub_class; 
    uint8_t sec_bus;

    base_class = pci_get_base_class(bus, dev, fun);
    sub_class = pci_get_sub_class(bus, dev, fun);

    if (base_class == PCI_CLASS_BRIDGE && sub_class == PCI_SUBCLASS_BRIDGE_PCI) {
        PCI_DEBUG("[%04d.%02d.%02d] PCI-PCI bridge detected, probing secondary bus\n",
                bus, dev, fun);
        sec_bus = pci_get_sec_bus(bus, dev, fun);
        pci_bus_probe(pci, sec_bus);
    }
}



/*
 *
 * This function is the top-level PCI probe. This checks to see if we have one
 * or more PCI host controllers. We only currently support a single PCI host
 * controller. If there is indeed only one, we probe the single bus further for
 * devices by calling pci_bus_probe().
 *
 */
static void 
pci_bus_scan (struct pci_info * pci)
{
    uint8_t hdr_type;
    uint8_t fun;

    hdr_type = pci_get_hdr_type(0, 0, 0);

    // a multi-function host controller means there are more than one...
    if ((hdr_type & 0x80) == 0) {
        // only one PCI host controller 
        pci_bus_probe(pci, 0);

    } else {
        /* 
         * Multiple PCI host controllers present. this is likely due to
         * a multi-processor system, where each processor has it's own 
         * PCIe root complex, each rooting a hierarchy of PCIe switches
         * and devices.
         */
        for (fun = 0; fun < 8; fun++) {
            if (pci_get_vendor_id(0, 0, fun) != 0xffff) {
                PCI_DEBUG("Found new PCI host bridge, scanning bus %d\n", fun);
                pci_bus_probe(pci, fun);
            } 
        }
    }
}

static struct nk_dev_int ops = {
    .open=0,
    .close=0,
};


static inline int
get_membar_size (uint32_t baddr)
{
    // only applies to memory BARs
    if (baddr & 0x1) {
        return 0;
    }

    if (PCI_MBAR_IS_64(baddr)) {
        return 64;
    } else {
        return 32;
    }

    return 0;
}


uint64_t
pci_dev_get_bar_addr (struct pci_dev * d, uint8_t barnum)
{
    uint32_t offset = PCI_BAR_OFFSET + (barnum * 4);
    uint64_t bar    = pci_dev_cfg_readl(d, offset);

    if (!bar) {
        return 0;
    }

    //PCI_DEBUG("Read bar as 0x%x\n",bar);

    // we need to get the upper 32 bits of the address if this is a 64-bit BAR
    if (get_membar_size(bar) == 64) {
        bar |= ((uint64_t)pci_dev_cfg_readl(d, offset + 4)) << 32;
	//PCI_DEBUG("long bar addr = %lx\n",bar&PCI_BAR_MEM_MASK64);
        return bar & PCI_BAR_MEM_MASK64;
    } 
    
    uint64_t addr = pci_dev_get_bar_type(d, barnum) == PCI_BAR_IO ? (bar & PCI_BAR_IO_MASK) : (bar & PCI_BAR_MEM_MASK);
    
    //PCI_DEBUG("short bar addr = %lx (%s)\n",addr, pci_dev_get_bar_type(d,barnum)==PCI_BAR_IO ? "io" : "memory");

    return addr;
}

uint8_t
pci_dev_get_bar_next (struct pci_dev * d, uint8_t barnum)
{
    uint32_t offset = PCI_BAR_OFFSET + (barnum * 4);
    uint64_t bar    = pci_dev_cfg_readl(d, offset);

    if (barnum==5 || !bar) {
	return barnum;
    }

    switch (pci_dev_get_bar_type(d,barnum)) {
    case PCI_BAR_NONE:
	return barnum;
	break;
    case PCI_BAR_IO:
	return barnum+1;
	break;
    case PCI_BAR_MEM:
	if (get_membar_size(bar)==64) {
	    return barnum+2;
	} else {
	    return barnum+1;
	}
	break;
    default:
	return barnum;
    }
}

static uint32_t
__get_bar_size_32 (struct pci_dev * d, uint32_t offset)
{
    uint32_t baddr = pci_dev_cfg_readl(d, offset);
    uint32_t size  = 0;

    if (!baddr) {
        return 0;
    }

    pci_dev_cfg_writel(d, offset, PCI_BAR_SIZE_MAGIC);

    size = pci_dev_cfg_readl(d, offset);

    size &= (baddr & 0x1) ? PCI_BAR_IO_MASK : PCI_BAR_MEM_MASK;

    // restore original
    pci_dev_cfg_writel(d, offset, baddr);

    //PCI_DEBUG("short bar size = %lx\n",~size+1);

    return ~size + 1;
}


static uint32_t
__get_bar_size_64 (struct pci_dev * d, uint32_t offset)
{
    uint32_t baddr0 = pci_dev_cfg_readl(d, offset);
    uint32_t baddr1 = pci_dev_cfg_readl(d, offset + 4);
    uint64_t size   = 0;

    if (!baddr0) {
        return 0;
    }

    pci_dev_cfg_writel(d, offset, PCI_BAR_SIZE_MAGIC);

    size = pci_dev_cfg_readl(d, offset);

    pci_dev_cfg_writel(d, offset + 4, PCI_BAR_SIZE_MAGIC);

    size |= ((uint64_t)pci_dev_cfg_readl(d, offset + 4)) << 32;

    // I/O BARs cannot be 64-bit
    size &= PCI_BAR_MEM_MASK64;

    // restore originals
    pci_dev_cfg_writel(d, offset, baddr0);
    pci_dev_cfg_writel(d, offset + 4, baddr1);

    //PCI_DEBUG("long bar size = %lx\n",~size+1);
    
    return ~size + 1;
}


uint64_t
pci_dev_get_bar_size (struct pci_dev * d, uint8_t barnum)
{
    uint32_t offset = PCI_BAR_OFFSET + (barnum * 4);
    uint32_t bar    = pci_dev_cfg_readl(d, offset);

    if (get_membar_size(bar) == 64) {
        return __get_bar_size_64(d, offset);
    } else {
        return (uint64_t)__get_bar_size_32(d, offset);
    }

    return 0;
}


pci_bar_type_t 
pci_dev_get_bar_type (struct pci_dev * d, uint8_t barnum)
{
    uint32_t offset = PCI_BAR_OFFSET + (barnum * 4);
    uint32_t baddr  = pci_dev_cfg_readl(d, offset);
    uint32_t size   = 0;

    if (!baddr) {
        return PCI_BAR_NONE;
    } else if (baddr & 0x1) {
        return PCI_BAR_IO;
    } else {
        return PCI_BAR_MEM;
    }

    return PCI_BAR_NONE;
}


void 
pci_dev_disable_io (struct pci_dev * d)
{
	uint16_t cmd = pci_get_cmd(d->bus->num, d->num, d->fun);
    cmd &= 0xfffe;
    pci_set_cmd(d->bus->num, d->num, d->fun, cmd);
}


void
pci_dev_enable_io (struct pci_dev * d)
{
	uint16_t cmd = pci_get_cmd(d->bus->num, d->num, d->fun);
	cmd |= 1;
	pci_set_cmd(d->bus->num, d->num, d->fun, cmd);
}


void 
pci_dev_disable_mmio (struct pci_dev * d)
{
    uint16_t cmd = pci_get_cmd(d->bus->num, d->num, d->fun);
    cmd &= 0xfffd;
    pci_set_cmd(d->bus->num, d->num, d->fun, cmd);
}


void
pci_dev_enable_mmio (struct pci_dev * d)
{
    uint16_t cmd = pci_get_cmd(d->bus->num, d->num, d->fun);
    cmd |= 0x2;
    pci_set_cmd(d->bus->num, d->num, d->fun, cmd);
}


void 
pci_dev_disable_irq (struct pci_dev * d)
{
    uint16_t cmd = pci_get_cmd(d->bus->num, d->num, d->fun);
    cmd |= (1<<10);
    pci_set_cmd(d->bus->num, d->num, d->fun, cmd);
}


void
pci_dev_enable_irq (struct pci_dev * d)
{
	uint16_t cmd = pci_get_cmd(d->bus->num, d->num, d->fun);
    cmd &= ~(1<<10);
    pci_set_cmd(d->bus->num, d->num, d->fun, cmd);
}

void 
pci_dev_enable_mwi (struct pci_dev * d)
{
    uint16_t cmd = pci_get_cmd(d->bus->num, d->num, d->fun);
    cmd |= (1<<4);
    pci_set_cmd(d->bus->num, d->num, d->fun, cmd);
}


void
pci_dev_disable_mwi (struct pci_dev * d)
{
    uint16_t cmd = pci_get_cmd(d->bus->num, d->num, d->fun);
    cmd &= ~(1<<4);
    pci_set_cmd(d->bus->num, d->num, d->fun, cmd);
}


void 
pci_dev_enable_master (struct pci_dev * d)
{
	uint16_t cmd = pci_get_cmd(d->bus->num, d->num, d->fun);
	cmd |= 0x4;
	pci_set_cmd(d->bus->num, d->num, d->fun, cmd);
}


void
pci_dev_disable_master (struct pci_dev * d)
{
    uint16_t cmd = pci_get_cmd(d->bus->num, d->num, d->fun);
    cmd &= 0xfffb;
    pci_set_cmd(d->bus->num, d->num, d->fun, cmd);
}


/*
 *
 * This function initializes the PCI subsystem by scanning
 * for devices on the PCI bus.
 *
 */
int 
pci_init (struct naut_info * naut)
{
    struct pci_info * pci = NULL;

    pci = malloc(sizeof(struct pci_info));
    if (!pci) {
        PCI_ERROR("Could not allocate PCI struct\n");
        return -1;
    }
    memset(pci, 0, sizeof(struct pci_info));

    INIT_LIST_HEAD(&(pci->bus_list));

    PCI_PRINT("Probing PCI bus...\n");

    // this will miss PCI buses attached to other complexes...
    pci_bus_scan(pci);

    naut->sys.pci = pci;

    nk_dev_register("pci0", NK_DEV_BUS, 0, &ops, 0);

    // register ISA here for now
    nk_dev_register("isa", NK_DEV_BUS, 0, &ops, 0);

    return 0;
}

int pci_map_over_devices(int (*f)(struct pci_dev *d, void *s), uint16_t vendor, uint16_t device, void *state)
{
  struct pci_info *pci = nk_get_nautilus_info()->sys.pci;
  struct list_head *curbus, *curdev;
  int rc=0;
  
  if (!pci) { 
    PCI_ERROR("No PCI info available\n");
    return -1;
  }

  list_for_each(curbus,&(pci->bus_list)) { 

    struct pci_bus *bus = list_entry(curbus,struct pci_bus,bus_node);

    list_for_each(curdev, &(bus->dev_list)) { 
      struct pci_dev *pdev = list_entry(curdev,struct pci_dev,dev_node);

      if (vendor==0xffff || pdev->cfg.vendor_id==vendor) { 
	if (device==0xffff || pdev->cfg.device_id==device) { 
	  rc |= f(pdev,state);
	}
      }
    }
  }

  return rc;
}

static int dump_dev_base(struct pci_dev *d, void *s)
{
  uint16_t cmd, status, intr_pin, intr_line;

  cmd=pci_dev_cfg_readw(d,4);
  status=pci_dev_cfg_readw(d,6);

  nk_vc_printf("%x:%x.%x : %04x:%04x %04xc %04xs",
	       d->bus->num, d->num, d->fun, d->cfg.vendor_id, d->cfg.device_id, cmd, status);

  if ((d->cfg.hdr_type&~0x80) ==0 ) { 
    if (d->msi.co) { 
      struct pci_msi_info *m = &d->msi;
      
      nk_vc_printf(" MSI(%s,%s,nn=%d,bv=%d,nv=%d,tc=%d)",
		   m->enabled ? "on" : "off",
		   m->type == PCI_MSI_32 ? "MSI32" :
		   m->type == PCI_MSI_32_PER_VEC ? "MSI32wP" :
		   m->type == PCI_MSI_64 ? "MSI64" :
		   m->type == PCI_MSI_64_PER_VEC ? "MSI64wP" : "UNKNOWN",
		   m->num_vectors_needed,
		   m->base_vec,
		   m->num_vecs,
		   m->target_cpu);
    } 
    if (d->msix.co) {
      struct pci_msi_x_info *m = &d->msix;
      nk_vc_printf(" MSIX(%s,nt=%d)",
		   m->enabled ? "on" : "off",
		   m->size);
    }
    if (!d->cfg.dev_cfg.intr_pin) {
      nk_vc_printf(" nolegacy");
    } else {
      nk_vc_printf(" legacy(l=%d,p=%d)", d->cfg.dev_cfg.intr_line, d->cfg.dev_cfg.intr_pin);
    }
    nk_vc_printf("\n");
  } else {
    nk_vc_printf("not simple device\n");
  }
  return 0;
}


int pci_dump_device_list()
{
  return pci_map_over_devices(dump_dev_base,-1,-1,0);
}

struct device_query {
  uint8_t bus, num, fun;
  struct pci_dev *dev;
};

static int find_dev(struct pci_dev *d, void *s)
{
  struct device_query *q = (struct device_query *)s;
  if (d->num==q->num && d->bus->num==q->bus && d->fun==q->fun) { 
    q->dev = d;
  }
  return 0;
}


struct pci_dev *pci_find_device(uint8_t bus, uint8_t num, uint8_t fun)
{
  struct device_query q = {.bus=bus, .num=num, .fun=fun, .dev=0 };
  if (pci_map_over_devices(find_dev,-1,-1,&q)) { 
    return 0;
  } else {
    return q.dev;
  }
}


int pci_dump_device(struct pci_dev *d)
{
  int i;
  uint8_t *data;

  if (!d) { 
    return 0;
  }

#define SHOWIT(x) nk_vc_printf("%-24s: 0x%x\n", #x  , d->cfg.x)

  SHOWIT(vendor_id);
  SHOWIT(device_id);
  SHOWIT(cmd);
  SHOWIT(status);
  SHOWIT(rev_id);
  SHOWIT(prog_if);
  nk_vc_printf("%-24s: 0x%x (%s)\n",
	       "subclass", d->cfg.subclass,
	       d->cfg.class_code==PCI_SUBCLASS_BRIDGE_PCI ? "pci bridge" : "UNKNOWN");

  nk_vc_printf("%-24s: 0x%x (%s)\n",
	       "class_code", d->cfg.class_code,
	       d->cfg.class_code==PCI_CLASS_LEGACY ? "legacy" :
	       d->cfg.class_code==PCI_CLASS_STORAGE ? "storage" :
	       d->cfg.class_code==PCI_CLASS_NET ? "net" :
	       d->cfg.class_code==PCI_CLASS_DISPLAY ? "display" :
	       d->cfg.class_code==PCI_CLASS_MULTIM ? "multim" : // wtf?
	       d->cfg.class_code==PCI_CLASS_MEM ? "mem" :
	       d->cfg.class_code==PCI_CLASS_BRIDGE ? "bridge" :
	       d->cfg.class_code==PCI_CLASS_SIMPLE ? "simple" :
	       d->cfg.class_code==PCI_CLASS_BSP ? "bsp" :
	       d->cfg.class_code==PCI_CLASS_INPUT ? "input" :
	       d->cfg.class_code==PCI_CLASS_DOCK ? "dock" :
	       d->cfg.class_code==PCI_CLASS_PROC ? "proc" :
	       d->cfg.class_code==PCI_CLASS_SERIAL ? "serial" :
	       d->cfg.class_code==PCI_CLASS_WIRELESS ? "wireless" :
	       d->cfg.class_code==PCI_CLASS_INTIO ? "intio" :
	       d->cfg.class_code==PCI_CLASS_SAT ? "sat" :
	       d->cfg.class_code==PCI_CLASS_CRYPTO ? "crypto" :
	       d->cfg.class_code==PCI_CLASS_SIG ? "sig" :
	       d->cfg.class_code==PCI_CLASS_NOCLASS ? "noclass" : "UNKNOWN");
  SHOWIT(cl_size);
  SHOWIT(lat_timer);
  nk_vc_printf("%-24s: 0x%x (","hdr_type", d->cfg.hdr_type);
  if (d->cfg.hdr_type & 0x80) {
    nk_vc_printf("multifunction ");
  }
  nk_vc_printf("%s)\n",
	       (d->cfg.hdr_type&~0x80)==0 ? "device" :
	       (d->cfg.hdr_type&~0x80)==1 ? "PCI<->PCI bridge" : "other");

  SHOWIT(bist);
  
  if ((d->cfg.hdr_type&~0x80)==0) { 

#define SHOWITDEV(x) nk_vc_printf("%-24s: 0x%x\n", #x  , d->cfg.dev_cfg.x)
    SHOWITDEV(bars[0]);
    SHOWITDEV(bars[1]);
    SHOWITDEV(bars[2]);
    SHOWITDEV(bars[3]);
    SHOWITDEV(bars[4]);
    SHOWITDEV(bars[5]);
    SHOWITDEV(cardbus_cis_ptr);
    SHOWITDEV(subsys_vendor_id);
    SHOWITDEV(subsys_id);
    SHOWITDEV(exp_rom_bar);
    SHOWITDEV(cap_ptr);
    SHOWITDEV(rsvd[0]);
    SHOWITDEV(rsvd[1]);
    SHOWITDEV(rsvd[2]);
    SHOWITDEV(rsvd[3]);
    SHOWITDEV(rsvd[4]);
    SHOWITDEV(rsvd[5]);
    SHOWITDEV(rsvd[6]);
    SHOWITDEV(intr_line);
    SHOWITDEV(intr_pin);
    SHOWITDEV(min_grant);
    SHOWITDEV(max_latency);

    data = (uint8_t*)d->cfg.dev_cfg.data;
    nk_vc_printf("%-24s: 0x","data");
    for (i=0;i<48*4;i++) {  
      nk_vc_printf("%02x",data[i]);
    }
    nk_vc_printf("\n");

    data = (uint8_t*)&d->cfg;

    if (d->cfg.status & (1<<4)) { 
      nk_vc_printf("%-24s: ","capabilities");
      uint8_t co = d->cfg.dev_cfg.cap_ptr & ~0x3;
      while (co) {
	uint8_t cap = data[co];
	nk_vc_printf("0x%02x (%s) ", cap,
		     cap==0x1 ? "PMI" :
		     cap==0x2 ? "AGP" :
		     cap==0x3 ? "VPD" :
		     cap==0x4 ? "NumID" :
		     cap==0x5 ? "MSI" :
		     cap==0x6 ? "CompactHotSwap" :
		     cap==0x7 ? "PCI-X" :
		     cap==0x8 ? "HyperTransport" :
		     cap==0x9 ? "VendorSpecific" :
		     cap==0xa ? "Debug" :
		     cap==0xb ? "CompactCtrl" :
		     cap==0xc ? "HotPlug" :
		     cap==0xd ? "BridgeSubsysVendorID" :
		     cap==0xe ? "AGP8x" :
		     cap==0xf ? "SecureDev" : 
		     cap==0x10 ? "PCIExpress" : 
		     cap==0x11 ? "MSI-X" : 
		     cap==0x12 ? "SATADataIndex" :
		     cap==0x13 ? "PCIAdvancedFeatues" :
		     cap==0x14 ? "PCIEnhancedAlloc" : "UNKNOWN");
	co = data[co+1];
      }
      nk_vc_printf("\n");
    }
		     

  } else if ((d->cfg.hdr_type&~0x80)==1) { 

#define SHOWITP2P(x) nk_vc_printf("%-24s: 0x%x\n", #x  , d->cfg.pci_to_pci_bridge_cfg.x)

    SHOWITP2P(bars[0]);
    SHOWITP2P(bars[1]);
    SHOWITP2P(primary_bus_num);
    SHOWITP2P(secondary_bus_num);
    SHOWITP2P(sub_bus_num);
    SHOWITP2P(secondary_lat_timer);
    SHOWITP2P(io_base);
    SHOWITP2P(io_limit);
    SHOWITP2P(secondary_status);
    SHOWITP2P(mem_base);
    SHOWITP2P(mem_limit);
    SHOWITP2P(prefetch_mem_base);
    SHOWITP2P(prefetch_mem_limit);
    SHOWITP2P(prefetch_base_upper);
    SHOWITP2P(prefetch_limit_upper);
    SHOWITP2P(io_base_upper);
    SHOWITP2P(io_limit_upper);
    SHOWITP2P(cap_ptr);
    SHOWITP2P(rsvd[0]);
    SHOWITP2P(rsvd[1]);
    SHOWITP2P(rsvd[2]);
    SHOWITP2P(exp_rom_bar);
    SHOWITP2P(intr_line);
    SHOWITP2P(intr_pin);
    SHOWITP2P(bridge_ctrl);

    uint8_t *data = (uint8_t*)d->cfg.pci_to_pci_bridge_cfg.data;

    nk_vc_printf("%-24s: ","data");
    for (i=0;i<48*4;i++) {  
      nk_vc_printf("%02x",data[i]);
    }
    nk_vc_printf("\n");

  } else {
    nk_vc_printf("No further info for this type\n");
  }

    
  return 0;
}  


static int dump_dev(struct pci_dev *d, void *s)
{
  nk_vc_printf("%-24s: %x:%x.%x\n","LOCATION", d->bus->num, d->num, d->fun);

  pci_dump_device(d);

  nk_vc_printf("\n");

  return 0;
}


int pci_dump_devices()
{
  return pci_map_over_devices(dump_dev,-1,-1,0);
}
 
struct dev_match_q {
  struct pci_dev **list;
  uint32_t         max;
  uint32_t         cur;
};

static int collect_matching(struct pci_dev *d, void *s)
{
  struct dev_match_q *q = (struct dev_match_q *)s;

  if (q->cur<q->max) { 
    q->list[q->cur++] = d;
  }

  return 0;
}


int pci_find_matching_devices(uint16_t vendor_id, uint16_t device_id,
			      struct pci_dev *dev[], uint32_t *num)
{
  struct dev_match_q q;

  memset(&q,0,sizeof(q));

  q.list = dev;
  q.max = *num;
  q.cur = 0;

  if (pci_map_over_devices(collect_matching,vendor_id,device_id,&q)) { 
    return -1;
  } else {
    *num = q.cur;
    return 0;
  }
}

uint16_t pci_dev_cfg_readw(struct pci_dev *dev, uint8_t off)
{
  return pci_cfg_readw(dev->bus->num, dev->num, dev->fun, off);
}

uint32_t pci_dev_cfg_readl(struct pci_dev *dev, uint8_t off)
{
  return pci_cfg_readl(dev->bus->num, dev->num, dev->fun, off);
}

void     pci_dev_cfg_writew(struct pci_dev *dev, uint8_t off, uint16_t val)
{
  pci_cfg_writew(dev->bus->num,dev->num,dev->fun,off,val);
}

void     pci_dev_cfg_writel(struct pci_dev *dev, uint8_t off, uint32_t val)
{
  pci_cfg_writel(dev->bus->num,dev->num,dev->fun,off,val);
}

int      pci_dev_scan_capabilities(struct pci_dev *d,
				   void (*func)(void *state, void *indata),
				   void *state)
{
    if (!(d->cfg.status & (1<<4))) { 
	return -1;
    }

    uint8_t *data = (uint8_t*)&d->cfg;
    uint8_t co = d->cfg.dev_cfg.cap_ptr & ~0x3;
    
    // scan the cfg space snapshot
    while (co) {
	uint8_t cap = data[co];
	func(state,&data[co]);
	co = data[co+1];
    }
    
    return 0;
}

 
uint8_t  pci_dev_get_capability(struct pci_dev *d, uint8_t cap_id)
{
  if (!(d->cfg.status & (1<<4))) { 
    //PCI_DEBUG("device has no capabilities\n");
    return 0;
  }

  uint8_t *data = (uint8_t*)&d->cfg;
  uint8_t co = d->cfg.dev_cfg.cap_ptr & ~0x3;

  // scan the cfg space snapshot for the capability 
  while (co) {
    uint8_t cap = data[co];
    if (cap==cap_id) { 
      //PCI_DEBUG("capability 0x%x found\n", cap_id);
      return co;
    }
    co = data[co+1];
  }
  //PCI_DEBUG("capability 0x%x not found\n", cap_id);
  return co;
}



/*
  the structure of an x86 address register is:

  [32 bits - (presumably top half all zeros...) ]

  [12 bits]  [8 bits]  [8 bits] [1] [1] [2 bits]
   0xfee      dest_id    rsvd   RH  DM   XX 

  dest_id => equivalent to bits 63:56 of ioapic redirectionentry
             (physical or logical apic id)
  RH => redirection hint, 1=> send to lowest priority, 0=> send it
  DM => destination mode, 1=> interpret dest_id as logical
  XX => ? probably zero

  the structure of an x86 data register is:

  [32 bits reserved => 0}
  [16 bits]    [1] [1]  [3]   [3]   [8]
   rsvd        TM  LEV  rsvd  DM    VEC

   TM = trigger mode, 0=> edge, 1=> level
   LEV = level for trigger mode, 0=> don't care, else 0/1 = deassert/assert
   DM = delivery mode (fixed, lowest priority, smi, res, nmi, init, res, extint)

   Intel Volvume 3A, 10.11.1, 10.11.2
 */

int pci_dev_enable_msi(struct pci_dev *dev, int base_vec, int num_vecs, int target_cpu)
{
  uint32_t mar_low;
  uint16_t mdr;
  uint32_t ctrl;
  uint16_t cmd;
  uint8_t co;
  int log2_num_vecs;
  
  PCI_DEBUG("msi enable - base_vec=0x%x, num_vecs=0x%x, target_cpu=0x%x\n", 
	    base_vec, num_vecs, target_cpu);

  co = pci_dev_get_capability(dev,0x5);

  if (dev->msi.type==PCI_MSI_NONE || !co) { 
    PCI_ERROR("device does not support MSI\n");
    return -1;
  }

  if (num_vecs<1 || num_vecs>32 || 
      num_vecs>dev->msi.num_vectors_needed || 
      __builtin_popcount(num_vecs)!=1 || 
      base_vec & (num_vecs - 1)) {
    PCI_ERROR("Invalid MSI enable request\n");
  }


  // we need to disable the interrupt pin first
  
  cmd = pci_dev_cfg_readw(dev,0x4);
  cmd |= 0x400;
  pci_dev_cfg_writew(dev,0x4,cmd);

  PCI_DEBUG("legacy interrupt disabled (cmd=%x)\n",cmd);

  log2_num_vecs = 32-(__builtin_clz(num_vecs)+1);
  
  mar_low  = 0xfee00000;
  mar_low |= (target_cpu & 0xff) << 12;
  // we use RH=0 because we don't want lowest priority
  // we use DM=0 because we want physical delivery

  mdr = (base_vec & 0xff);
  // We use DM=0 to get fixed delivery
  // We use LEV=0 because we don't care and are using edge
  // We use TM=0 to get edge
  
  // this gets us the type and next ptr as well, but 
  // manipulating ctrl seems to require a 32 bit write...

  ctrl = pci_dev_cfg_readl(dev,co);

  PCI_DEBUG("initial ctrl: 0x%x\n",ctrl);

  ctrl &= 0xff8e0000;  // clear multiple message enable, clear msi-enable
  ctrl |= (log2_num_vecs & 0x7)<<4;  // set multiple message enable

  PCI_DEBUG("updated: mdr=0x%x, mar_low=0x%x, ctrl=0x%x\n",mdr,mar_low,ctrl);

  switch (dev->msi.type) { 
  case PCI_MSI_32:
    pci_dev_cfg_writel(dev,co+4,mar_low);
    pci_dev_cfg_writew(dev,co+8,mdr);
    pci_dev_cfg_writel(dev,co,ctrl);
    PCI_DEBUG("enabled as MSI32 (masked)\n");
    break;
  case PCI_MSI_32_PER_VEC:
    pci_dev_cfg_writel(dev,co+4,mar_low);
    pci_dev_cfg_writew(dev,co+8,mdr);
    pci_dev_cfg_writel(dev,co+12,0xffffffff); // mask all
    pci_dev_cfg_writel(dev,co,ctrl);
    PCI_DEBUG("enabled as MSI32 with per vec mask (all masked)\n");
    break;
  case PCI_MSI_64:
    pci_dev_cfg_writel(dev,co+8,0x0); // high addr
    pci_dev_cfg_writel(dev,co+4,mar_low);
    pci_dev_cfg_writew(dev,co+12,mdr);
    pci_dev_cfg_writel(dev,co,ctrl);
    PCI_DEBUG("enabled as MSI64 (masked)\n");
    break;
  case PCI_MSI_64_PER_VEC:
    pci_dev_cfg_writel(dev,co+8,0x0); // high addr
    pci_dev_cfg_writel(dev,co+4,mar_low);
    pci_dev_cfg_writew(dev,co+12,mdr);
    pci_dev_cfg_writel(dev,co+16,0xffffffff); // mask all
    pci_dev_cfg_writel(dev,co,ctrl);
    PCI_DEBUG("enabled as MSI64 with per vec mask (all masked)\n");
    break;
  default:
    PCI_ERROR("Unknown MSI type\n");
    return -1;
    break;
  }

  dev->msi.co = co;
  dev->msi.base_vec = base_vec;
  dev->msi.num_vecs = num_vecs;
  dev->msi.target_cpu=target_cpu;
  dev->msi.enabled=1;

  PCI_DEBUG("MSI enabled for device\n");

  return 0;
}

int pci_dev_mask_msi(struct pci_dev *dev, int vec)
{
  uint32_t mask;
  uint32_t ctrl;
  uint8_t  co;

  if (!dev->msi.enabled) { 
    PCI_DEBUG("device does not msi enabled\n");
    return -1;
  }

  co = dev->msi.co;

  ctrl = pci_dev_cfg_readl(dev,co);

  switch (dev->msi.type) { 
  case PCI_MSI_32:
  case PCI_MSI_64:
    // masking means to disable msi
    ctrl &= ~0x10000;
    pci_dev_cfg_writel(dev,co,ctrl);
    PCI_DEBUG("masked as a non-per-vec-masking device\n");
    break;
  case PCI_MSI_32_PER_VEC:
    // masking means to enable msi and mask the given vector
    ctrl |= 0x10000;
    mask=pci_dev_cfg_readl(dev,co+12);
    mask |= 0x1 << (vec-dev->msi.base_vec);
    pci_dev_cfg_writel(dev,co+12,mask);
    pci_dev_cfg_writel(dev,co,ctrl);
    PCI_DEBUG("masked as a 32 bit per-vec-masking device\n");
    break;
  case PCI_MSI_64_PER_VEC:
    // masking means to enable msi and mask the given vector
    ctrl |= 0x10000;
    mask=pci_dev_cfg_readl(dev,co+16);
    mask |= 0x1 << (vec-dev->msi.base_vec);
    pci_dev_cfg_writel(dev,co+16,mask);
    pci_dev_cfg_writel(dev,co,ctrl);
    PCI_DEBUG("masked as a 64 bit per-vec-masking device\n");
    break;
  default:
    PCI_ERROR("Unknown MSI type\n");
    return -1;
    break;
  }

  return 0;
}


int pci_dev_unmask_msi(struct pci_dev *dev, int vec)
{
  uint32_t mask;
  uint32_t ctrl;
  uint8_t co;

  if (!dev->msi.enabled) { 
    PCI_DEBUG("device does not msi enabled\n");
    return -1;
  }

  co = dev->msi.co;

  ctrl = pci_dev_cfg_readl(dev,co);


  switch (dev->msi.type) { 
  case PCI_MSI_32:
  case PCI_MSI_64:
    // unmasking means to enable msi
    ctrl |= 0x10000;
    pci_dev_cfg_writel(dev,co,ctrl);
    PCI_DEBUG("unmasked as a non-per-vec-masking device\n");
    break;
  case PCI_MSI_32_PER_VEC:
    // unmasking means to enable msi and unmask the given vector
    ctrl |= 0x10000;
    mask=pci_dev_cfg_readl(dev,co+12);
    mask &= ~(0x1 << (vec-dev->msi.base_vec));
    pci_dev_cfg_writel(dev,co+12,mask);
    pci_dev_cfg_writel(dev,co,ctrl);
    PCI_DEBUG("unmasked as a 32 bit per-vec-masking device\n");
    break;
  case PCI_MSI_64_PER_VEC:
    // unmasking means to enable msi and unmask the given vector
    ctrl |= 0x10000;
    mask=pci_dev_cfg_readl(dev,co+16);
    mask &= ~(0x1 << (vec-dev->msi.base_vec));
    pci_dev_cfg_writel(dev,co+16,mask);
    pci_dev_cfg_writel(dev,co,ctrl);
    PCI_DEBUG("unmasked as a 64 bit per-vec-masking device\n");
    break;
  default:
    PCI_ERROR("Unknown MSI type\n");
    return -1;
    break;
  }

  return 0;
}


int pci_dev_is_pending_msi(struct pci_dev *dev, int vec)
{
  uint32_t pending;
  uint8_t  co;

  if (!dev->msi.enabled) { 
    PCI_DEBUG("device does not msi enabled\n");
    return 0;
  }

  co = dev->msi.co;
  
  switch (dev->msi.type) {
  case PCI_MSI_32:
  case PCI_MSI_64:
    pending=0;
    PCI_DEBUG("pending is zero for a non-per-vector device\n");
    break;
  case PCI_MSI_32_PER_VEC:
    pending=pci_dev_cfg_readl(dev,co+16);
    break;
  case PCI_MSI_64_PER_VEC:
    pending=pci_dev_cfg_readl(dev,co+20);
    break;
  default:
    PCI_ERROR("Unknown MSI type - returning pending=0\n");
    pending=0;
    break;
  }

  pending = (pending >> (vec-dev->msi.base_vec)) & 0x1;

  PCI_DEBUG("pending of vector %d is %d\n", vec, pending);

  return pending;

}



#define WRITEL(p,v) __asm__ __volatile__ ("movl %0, (%1)" : : "r"(v), "r"(p) : "memory")
#define READL(p,v) __asm__ __volatile__ ("movl (%1), %0" : "=r"(v) :"r"(p) : "memory")
#define WRITEQ(p,v) __asm__ __volatile__ ("movq %0, (%1)" : : "r"(v), "r"(p) : "memory")
#define READQ(p,v) __asm__ __volatile__ ("movq (%1), %0" : "=r"(v) :"r"(p) : "memory")

int pci_dev_set_msi_x_entry(struct pci_dev *dev, int num, int vec, int target_cpu)
{
  struct pci_msi_x_info *m = &dev->msix;
  pci_msi_x_table_entry_t *t = m->table + num;

  if (dev->msix.type!=PCI_MSI_X) {
    PCI_DEBUG("MSI-X not available\n");
    return -1;
  }

  if (num<0 || num >= m->size) {
    PCI_DEBUG("entry %d is out of range\n",num);
    return -1;
  }

  uint32_t mar_low;
  uint32_t mdr;

  PCI_DEBUG("msix enable - num = %d vec = %d target = %d\n", num, vec, target_cpu);

  mar_low  = 0xfee00000;
  mar_low |= (target_cpu & 0xff) << 12;
  // we use RH=0 because we don't want lowest priority
  // we use DM=0 because we want physical delivery

  mdr = (vec & 0xff);
  // We use DM=0 to get fixed delivery
  // We use LEV=0 because we don't care and are using edge
  // We use TM=0 to get edge

  // now we write the entry
  WRITEL(&t->msg_addr_lo, mar_low);
  WRITEL(&t->msg_addr_hi,0);
  WRITEL(&t->msg_data,mdr);
  WRITEL(&t->vector_control,1); // masked

  PCI_DEBUG("entry %d at %p written as mar=%08x:%08x mdr=%08x vc=%x\n",
	    num, t, 0, mar_low, mdr, 1);

  return 0;
}


static int pci_dev_mask_unmask_msi_x_all(struct pci_dev *dev, int val)
{
  struct pci_msi_x_info *m = &dev->msix;
  uint32_t ctrl; 

  if (dev->msix.type!=PCI_MSI_X) {
    return -1;
  }


  // yes, ctrl is 16 bits, but the transaction must be
  // 32 bits...
  ctrl = pci_dev_cfg_readl(dev,m->co);

  if (val) {
    ctrl |= 1<<30;
  } else {
    ctrl &= ~(1<<30);
  }

  pci_dev_cfg_writel(dev,m->co,ctrl);
  
  PCI_DEBUG("function %smasked ctrl=%x\n", val ? "" : "un",ctrl);

  return 0;
}

static int pci_dev_mask_unmask_msi_x_entry(struct pci_dev *dev, int num, int val)
{
  struct pci_msi_x_info *m = &dev->msix;
  pci_msi_x_table_entry_t *t = m->table + num;
  uint32_t old;

  if (dev->msix.type!=PCI_MSI_X) {
    PCI_DEBUG("device does not support MSI X\n");
    return -1;
  }

  if (num<0 || num >= m->size) {
    PCI_DEBUG("num=%d is out of range\n",num);
    return -1;
  }


  READL(&t->vector_control,old);
  if (val) {
    old |= 1;
  } else {
    old &= ~1;
  }
  WRITEL(&t->vector_control,old);

  PCI_DEBUG("entry %d %x %s\n", num, old, val ? "masked" : "unmasked");

  return 0;
}
  
  
int pci_dev_mask_msi_x_entry(struct pci_dev *dev, int num)
{
  return pci_dev_mask_unmask_msi_x_entry(dev,num,1);
}

int pci_dev_unmask_msi_x_entry(struct pci_dev *dev, int num)
{
  return pci_dev_mask_unmask_msi_x_entry(dev,num,0);
}

int pci_dev_mask_msi_x_all(struct pci_dev *dev)
{
  return pci_dev_mask_unmask_msi_x_all(dev,1);
}

int pci_dev_unmask_msi_x_all(struct pci_dev *dev)
{
  return pci_dev_mask_unmask_msi_x_all(dev,0);
}


int pci_dev_enable_msi_x(struct pci_dev *dev)
{
  struct pci_msi_x_info *m = &dev->msix;
  uint16_t cmd;
  uint32_t ctrl; 

  if (dev->msix.type!=PCI_MSI_X) {
    PCI_DEBUG("device does not have MSI-X\n");
    return -1;
  }

  // we need to disable the interrupt pin first
  
  cmd = pci_dev_cfg_readw(dev,0x4);
  cmd |= 0x417;  // disable intr, set mem, io, busmaster, write+invalidate
  pci_dev_cfg_writew(dev,0x4,cmd);
  PCI_DEBUG("legacy interrupt disabled (cmd=%x)\n",cmd);

  // needs to be manipulated with dwords...
  ctrl = pci_dev_cfg_readl(dev,m->co);

  ctrl |= 3<<30;

  pci_dev_cfg_writel(dev,m->co,ctrl);

  m->enabled = 1;

  PCI_DEBUG("MSI-X enabled and masked\n");

  return 0;

}  

int pci_dev_disable_msi_x(struct pci_dev *dev)
{
  struct pci_msi_x_info *m = &dev->msix;
  uint32_t ctrl; 

  if (dev->msix.type!=PCI_MSI_X) {
    return -1;
  }

  ctrl = pci_dev_cfg_readl(dev,m->co);

  ctrl &= ~(1<<31);

  pci_dev_cfg_writel(dev,m->co,ctrl);

  m->enabled=0;

  PCI_DEBUG("MSI-X disabled\n");

  // note that legacy or MSI are NOT enabled automatically

  return 0;
}

int pci_dev_is_pending_msi_x(struct pci_dev *dev, int num)
{
  struct pci_msi_x_info *m = &dev->msix;
  uint64_t entry, bit, val;

  if (dev->msix.type!=PCI_MSI_X) {
    PCI_DEBUG("Not an MSI-X device\n");
    return -1;
  }

  if (num<0 || num >= m->size) {
    PCI_DEBUG("entry %d is out of range\n",num);
    return -1;
  }

  entry = num/64;
  bit = num%64;

  READQ(m->pending+entry,val);

  val = (val>>bit) & 0x1;

  PCI_DEBUG("entry %d %s\n", num, val ? "pending" : "not pending");
  
  return val;
}

void pci_dev_dump_msi_x(struct pci_dev *dev)
{
  struct pci_msi_x_info *m = &dev->msix;
  uint32_t temp;
  uint32_t i;

  nk_vc_printf("MSI-X state\n");
  
  temp = pci_dev_cfg_readl(dev,m->co) >> 16;
  nk_vc_printf(" ctrl  = 0x%0x\n", temp);

  temp = pci_dev_cfg_readl(dev,m->co+4);
  nk_vc_printf(" tobir = 0x%0x (decoded to %p)\n", temp,m->table);

  temp = pci_dev_cfg_readl(dev,m->co+8);
  nk_vc_printf(" pbbir = 0x%0x (decoded to %p)\n", temp,m->pending);

  nk_vc_printf(" Interrupt Table\n");
  for (i=0;i<m->size;i++) {
      uint32_t *t = (uint32_t*)(m->table + i);
      uint32_t entry[4];
      READL(t,entry[0]);
      READL(t+1,entry[1]);
      READL(t+2,entry[2]);
      READL(t+3,entry[3]);
      nk_vc_printf(" %d %08x %08x %08x %08x\n", i, entry[0], entry[1], entry[2], entry[3]);
  }

  nk_vc_printf(" Pending Table\n");
  for (i=0;i<m->size;i+=64) {
      uint64_t *t = m->pending + i/64;
      uint64_t bm;
      READQ(t,bm);
      nk_vc_printf(" %d %016lx\n", i/64, bm);
  }
  
}


static int
handle_pci (char * buf, void * priv)
{
    int bus, slot, func, off;
    uint64_t data;
    char bwdq; 

    if (strncmp(buf,"pci l",5)==0) { 
        pci_dump_device_list();
        return 0;
    }

    if (sscanf(buf,"pci raw %x %x %x", &bus, &slot, &func)==3) { 
        int i,j;
        uint32_t v;
        for (i=0;i<256;i+=32) {
            nk_vc_printf("%02x:", i);
            for (j=0;j<8;j++) {
                v = pci_cfg_readl(bus,slot,func,i+j*4);
                nk_vc_printf(" %08x",v);
            } 
            nk_vc_printf("\n");
        }
        return 0;
    }

    if (sscanf(buf,"pci dev %x %x %x", &bus, &slot, &func)==3) { 
        pci_dump_device(pci_find_device(bus,slot,func));
        return 0;
    }

    if (!strncmp(buf,"pci dev",7)) {
        pci_dump_devices();
        return 0;
    }

    if (((bwdq='w', sscanf(buf,"pci poke w %x %x %x %x %x", &bus, &slot, &func, &off,&data))==5) ||
            ((bwdq='d', sscanf(buf,"pci poke d %x %x %x %x %x", &bus, &slot, &func, &off,&data))==5) ||
            ((bwdq='d', sscanf(buf,"pci poke %x %x %x %x %x", &bus, &slot, &func, &off,&data))==5)) {
        if (bwdq=='w') { 
            pci_cfg_writew(bus,slot,func,off,(uint16_t)data);
            nk_vc_printf("PCI[%x:%x.%x:%x] = 0x%04x\n",bus,slot,func,off,(uint16_t)data);
        } else {
            pci_cfg_writel(bus,slot,func,off,(uint32_t)data);
            nk_vc_printf("PCI[%x:%x.%x:%x] = 0x%08x\n",bus,slot,func,off,(uint32_t)data);
        }
        return 0;
    }

    if (((bwdq='w', sscanf(buf,"pci peek w %x %x %x %x", &bus, &slot, &func, &off))==4) ||
            ((bwdq='d', sscanf(buf,"pci peek d %x %x %x %x", &bus, &slot, &func, &off))==4) ||
            ((bwdq='d', sscanf(buf,"pci peek %x %x %x %x", &bus, &slot, &func, &off))==4)) {
        if (bwdq=='w') { 
            data = pci_cfg_readw(bus,slot,func,off);
            nk_vc_printf("PCI[%x:%x.%x:%x] = 0x%04x\n",bus,slot,func,off,(uint16_t)data);
        } else {
            data = pci_cfg_readl(bus,slot,func,off);
            nk_vc_printf("PCI[%x:%x.%x:%x] = 0x%08x\n",bus,slot,func,off,(uint32_t)data);
        }
        return 0;
    }

    if (sscanf(buf,"pci cfg %x %x %x", &bus, &slot, &func)) {
        int i,j;
        uint8_t data[256];
        for (i=0;i<256;i+=4) {
            *(uint32_t*)(&data[i]) = pci_cfg_readl(bus,slot,func,i);
        }
        for (i=0;i<256;i+=16) {
            nk_vc_printf("PCI[%x:%x.%x].cfg[%02x] = ",bus,slot,func,i);
            for (j=i;j<(i+16);j++) {
                nk_vc_printf(" %02x",data[j]);
            }
            nk_vc_printf("\n");
        }
        return 0;
    }

    nk_vc_printf("unknown pci command\n");

    return 0;
}

static struct shell_cmd_impl pci_impl = {
    .cmd      = "pci",
    .help_str = "pci list | pci raw/dev bus slot func | pci dev [bus slot func]\n"
                "  pci peek|poke bus slot func off [val] | pci cfg bus slot func",
    .handler  = handle_pci,
};
nk_register_shell_cmd(pci_impl);
