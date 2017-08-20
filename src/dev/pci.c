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
        PCI_ERROR("Could not allocate PCI device\n");
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
                

static void pci_fun_probe(struct pci_info * pci, uint8_t bus, uint8_t dev, uint8_t fun);


/*
 *
 * This function probes a particular device on a PCI bus,
 * first by checking if it is indeed a present device, then
 * by checking whether or not it has multiple functions.
 *
 */
static void
pci_dev_probe (struct pci_bus * bus, uint8_t dev)
{
    uint8_t fun = 0;
    uint16_t vendor_id = 0;
    uint8_t hdr_type;

    fun = 0;

    vendor_id = pci_get_vendor_id(bus->num, dev, fun);

    /* No device present */
    if (vendor_id == 0xffff) {
        return;
    }

    PCI_PRINT("%04d:%02d:%02d.%d %x:%x (rev %d)\n", 0, bus->num, dev, fun, vendor_id, 
            pci_get_dev_id(bus->num, dev, fun), 
            pci_get_rev_id(bus->num, dev, fun));

    /* create a logical representation of the device */
    if (pci_dev_create(dev, bus) == NULL) {
        PCI_ERROR("Could not create PCI device\n");
        return;
    }

    /* does it have multiple functions? */ 
    pci_fun_probe(bus->pci, bus->num, dev, fun);

    hdr_type = pci_get_hdr_type(bus->num, dev, fun);

    /* multi-function device */
    if ((hdr_type & 0x80) != 0) {

        PCI_PRINT("[%04d:%02d.%d] Multifunction Device detected\n", 
                bus->num, dev, vendor_id);

        // probe the other device functions 
        for (fun = 1; fun < PCI_MAX_FUN; fun++) {
            if (pci_get_vendor_id(bus->num, dev, fun) != 0xffff) {
                PCI_DEBUG("[%04d:%02d.%d] Probing function %02d\n", bus->num, dev, fun, fun);
                pci_fun_probe(bus->pci, bus->num, dev, fun);
            }
        }
    }
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
    }
}



/* 
 * 
 * This function checks a device's functions to see if it is
 * a PCI-PCI bridge. If so, we need to probe that secondary bus
 *
 */
static void
pci_fun_probe (struct pci_info * pci, uint8_t bus, uint8_t dev, uint8_t fun)
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
  nk_vc_printf("%x:%x.0 : %04x:%04x\n",
	       d->bus->num, d->num, d->cfg.vendor_id, d->cfg.device_id);
  return 0;
}


int pci_dump_device_list()
{
  return pci_map_over_devices(dump_dev_base,-1,-1,0);
}

struct device_query {
  uint8_t bus, slot, fun;
  struct pci_dev *dev;
};

static int find_dev(struct pci_dev *d, void *s)
{
  struct device_query *q = (struct device_query *)s;
  if (d->num==q->slot && d->bus->num==q->bus) { 
    q->dev = d;
  }
  return 0;
}


struct pci_dev *pci_find_device(uint8_t bus, uint8_t slot, uint8_t fun)
{
  struct device_query q = {.bus=bus, .slot=slot, .fun=fun, .dev=0 };
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
  nk_vc_printf("%-24s: 0x%x (%s)\n",
	       "hdr_type", d->cfg.hdr_type,
	       d->cfg.hdr_type==0 ? "device" :
	       d->cfg.hdr_type==1 ? "PCI<->PCI bridge" : "other");

  SHOWIT(bist);
  
  if (d->cfg.hdr_type==0) { 

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
#if 1
    nk_vc_printf("%-24s:   ","data-text");
    for (i=0;i<48*4;i++) {  
      nk_vc_printf("%c",isalnum(data[i]) ? data[i] : '.') ;
    }
    nk_vc_printf("\n");
#endif

    data = (uint8_t*)&d->cfg;

    if (d->cfg.status & (1<<4)) { 
      nk_vc_printf("%-24s: ","capabilities");
      uint8_t co = d->cfg.dev_cfg.cap_ptr & ~0x3;
      while (co) {
	uint8_t cap = data[co];
	co = data[co+1];
	nk_vc_printf("0x%02x (%s) ", cap,
		     cap==0x1 ? "PMI" :
		     cap==0x2 ? "AGP" :
		     cap==0x3 ? "VPD" :
		     cap==0x4 ? "SlotID" :
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
      }
      nk_vc_printf("\n");
    }
		     

  } else if (d->cfg.hdr_type==1) { 

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
#if 1
    nk_vc_printf("%-24s: ","data-text");
    for (i=0;i<48*4;i++) {  
      nk_vc_printf("%c",isalnum(data[i]) ? data[i] : '.') ;
    }
    nk_vc_printf("\n");
#endif
  } else {
    nk_vc_printf("No further info for this type\n");
  }

    
  return 0;
}  


static int dump_dev(struct pci_dev *d, void *s)
{
  nk_vc_printf("%-24s: %x:%x.0\n","LOCATION", d->bus->num, d->num);

  pci_dump_device(d);

  nk_vc_printf("\n");

  return 0;
}


int pci_dump_devices()
{
  return pci_map_over_devices(dump_dev,-1,-1,0);
}
