#include <nautilus/nautilus.h>
#include <dev/pci.h>
#include <dev/virtio_pci.h>

#ifndef NAUT_CONFIG_DEBUG_VIRTIO_PCI
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif 

#define INFO(fmt, args...) printk("VIRTIO_PCI: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("VIRTIO_PCI: DEBUG: " fmt, ##args)
#define ERROR(fmt, args...) printk("VIRTIO_PCI: ERROR: " fmt, ##args)

// list of virtio devices
static struct list_head dev_list;


int virtio_pci_init(struct naut_info * naut)
{
  struct pci_info *pci = naut->sys.pci;
  struct list_head *curbus, *curdev;

  INFO("init\n");

  INIT_LIST_HEAD(&dev_list);

  if (!pci) { 
    ERROR("No PCI info\n");
    return -1;
  }

  DEBUG("Finding virtio devices\n");

  list_for_each(curbus,&(pci->bus_list)) { 
    struct pci_bus *bus = list_entry(curbus,struct pci_bus,bus_node);

    DEBUG("Searching PCI bus %u for Virtio devices\n", bus->num);

    list_for_each(curdev, &(bus->dev_list)) { 
      struct pci_dev *pdev = list_entry(curdev,struct pci_dev,dev_node);
      struct pci_cfg_space *cfg = &pdev->cfg;

      DEBUG("Device %u is a %x:%x\n", pdev->num, cfg->vendor_id, cfg->device_id);

      if (cfg->vendor_id==0x1af4) {
	DEBUG("Virtio Device Found\n");
	struct virtio_pci_dev *vdev;

	vdev = malloc(sizeof(struct virtio_pci_dev));
	if (!vdev) {
	  ERROR("Cannot allocate device\n");
	  return -1;
	}

	memset(vdev,0,sizeof(*vdev));
	
	vdev->pci_dev = pdev;

	switch (cfg->device_id) { 
	case 0x1000:
	  DEBUG("Net Device\n");
	  vdev->type = VIRTIO_PCI_NET;
	  break;
	case 0x1001:
	  DEBUG("Block Device\n");
	  vdev->type = VIRTIO_PCI_BLOCK;
	  break;
	default:
	  DEBUG("Other Device\n");
	  vdev->type = VIRTIO_PCI_OTHER;
	  break;
	}

	// PCI Interrupt (A..D)
	vdev->pci_intr = cfg->dev_cfg.intr_pin;
	// Figure out mapping here or look at capabilities for MSI-X
	// vdev->intr_vec = ...

	// we expect two bars exist, one for memory, one for i/o
	// and these will be bar 0 and 1
	// check to see if there are no others
	for (int i=0;i<6;i++) { 
	  uint32_t bar = pci_cfg_readl(bus->num,pdev->num, 0, 0x10 + i*4);
	  uint32_t size;
	  DEBUG("bar %d: 0x%0x\n",i, bar);
	  if (i>=2 && bar!=0) { 
	    DEBUG("Not expecting this to be a non-empty bar...\n");
	  }
	  if (!(bar & 0x0)) { 
	    uint8_t mem_bar_type = (bar & 0x6) >> 1;
	    if (mem_bar_type != 0) { 
	      ERROR("Cannot handle memory bar type 0x%x\n", mem_bar_type);
	      return -1;
	    }
	  }

	  // determine size
	  // write all 1s, get back the size mask
	  pci_cfg_writel(bus->num,pdev->num,0,0x10 + i*4, 0xffffffff);
	  // size mask comes back + info bits
	  size = pci_cfg_readl(bus->num,pdev->num,0,0x10 + i*4);

	  // mask all but size mask
	  if (bar & 0x1) { 
	    // I/O
	    size &= 0xfffffffc;
	  } else {
	    // memory
	    size &= 0xfffffff0;
	  }
	  size = ~size;
	  size++; 

	  // now we have to put back the original bar
	  pci_cfg_writel(bus->num,pdev->num,0,0x10 + i*4, bar);

	  if (!size) { 
	    // non-existent bar, skip to next one
	    continue;
	  }

	  if (size>0 && i>=2) { 
	    ERROR("unexpected virtio pci bar with size>0!\n");
	    return -1;
	  }
	  
	  if (bar & 0x1) { 
	    vdev->ioport_start = bar & 0xffffffc0;
	    vdev->ioport_end = vdev->ioport_start + size;
	  } else {
	    vdev->mem_start = bar & 0xfffffff0;
	    vdev->mem_end = vdev->mem_start + size;
	  }

	}

	INFO("Adding virtio %s device: bus=%u dev=%u func=%u: pci_intr=%u intr_vec=%u ioport_start=%p ioport_end=%p mem_start=%p mem_end=%p\n",
	     vdev->type==VIRTIO_PCI_BLOCK ? "block" :
	     vdev->type==VIRTIO_PCI_NET ? "net" : "other",
	     bus->num, pdev->num, 0,
	     vdev->pci_intr, vdev->intr_vec,
	     vdev->ioport_start, vdev->ioport_end,
	     vdev->mem_start, vdev->mem_end);
	     

	list_add(&dev_list, &vdev->virtio_node);
      }
      
    }
  }
      

  
  return 0;
}

int virtio_pci_deinit()
{
  INFO("deinited\n");
  return 0;
}



