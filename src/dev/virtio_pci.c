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
 * Copyright (c) 2017, Peter Dinda
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 *          Nathan Lindquist <nallink01@gmail.com>
 *          Clay Kauzlaric <clay.kauzlaric@yahoo.com>
 *          Gino Wang <sihengwang2019@u.northwestern.edu>
 *          Jin Han <jinhan2019@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <dev/pci.h>
#include <dev/virtio_pci.h>

#ifdef NAUT_CONFIG_VIRTIO_NET
#include <dev/virtio_net.h>
#endif

#ifdef NAUT_CONFIG_VIRTIO_BLK
#include <dev/virtio_blk.h>
#endif

#ifdef NAUT_CONFIG_VIRTIO_GPU
#include <dev/virtio_gpu.h>
#endif

#ifndef NAUT_CONFIG_DEBUG_VIRTIO_PCI
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif 

#define INFO(fmt, args...) INFO_PRINT("virtio_pci: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("virtio_pci: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("virtio_pci: " fmt, ##args)

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK(state) _state_lock_flags = spin_lock_irq_save(&(state->lock))
#define STATE_UNLOCK(state) spin_unlock_irq_restore(&(state->lock), _state_lock_flags)


// this global state has no lock since we do not expect concurrent uses
// list of virtio devices
static struct list_head dev_list;

#define u8 uint8_t
#define le32 uint32_t


// DEVICE STATUS masks and values
#define DEV_STATUS_RESET              0
#define DEV_STATUS_ACKNOWLEDGE        1
#define DEV_STATUS_DRIVER             2
#define DEV_STATUS_DRIVER_OK          4
#define DEV_STATUS_FEATURES_OK        8
#define DEV_STATUS_DEVICE_NEEDS_RESET 64
#define DEV_STATUS_FAILED             128


// FIX FIX
// PAD these should be using atomic write/read
//

uint32_t virtio_pci_read_regl(struct virtio_pci_dev *dev, uint32_t offset)
{
    uint32_t result;
    if (dev->model!=VIRTIO_PCI_LEGACY_MODEL) {
	ERROR("supported only for legacy model\n");
	return -1;
    }
    if (dev->method==MEMORY) {
        uint64_t addr = dev->mem_start + offset;
        __asm__ __volatile__ ("movl (%1), %0" : "=r"(result) : "r"(addr) : "memory");
    } else {
        result = inl(dev->ioport_start+offset);
    }
    return result;
}

uint16_t virtio_pci_read_regw(struct virtio_pci_dev *dev, uint32_t offset)
{
    uint16_t result;
    if (dev->model!=VIRTIO_PCI_LEGACY_MODEL) {
	ERROR("supported only for legacy model\n");
	return -1;
    }
    if (dev->method==MEMORY) {
        uint64_t addr = dev->mem_start + offset;
        __asm__ __volatile__ ("movw (%1), %0" : "=r"(result) : "r"(addr) : "memory");
    } else {
        result = inw(dev->ioport_start+offset);
    }
    return result;
}

uint8_t virtio_pci_read_regb(struct virtio_pci_dev *dev, uint32_t offset)
{
    uint8_t result;
    if (dev->model!=VIRTIO_PCI_LEGACY_MODEL) {
	ERROR("supported only for legacy model\n");
	return -1;
    }
    if (dev->method==MEMORY) {
        uint64_t addr = dev->mem_start + offset;
        __asm__ __volatile__ ("movb (%1), %0" : "=r"(result) : "r"(addr) : "memory");
    } else {
        result = inb(dev->ioport_start+offset);
    }
    return result;
}

void virtio_pci_write_regl(struct virtio_pci_dev *dev, uint32_t offset, uint32_t data)
{
    if (dev->model!=VIRTIO_PCI_LEGACY_MODEL) {
	ERROR("supported only for legacy model\n");
	return;
    }
    if (dev->method==MEMORY) { 
        uint64_t addr = dev->mem_start + offset;
	__asm__ __volatile__ ("movl %1, (%0)" : "=r"(addr): "r"(data) : "memory");
    } else {
        outl(data,dev->ioport_start+offset);
    }
}

void virtio_pci_write_regw(struct virtio_pci_dev *dev, uint32_t offset, uint16_t data)
{
    if (dev->model!=VIRTIO_PCI_LEGACY_MODEL) {
	ERROR("supported only for legacy model\n");
	return;
    }
    if (dev->method==MEMORY) { 
        uint64_t addr = dev->mem_start + offset;
	__asm__ __volatile__ ("movw %1, (%0)" : "=r"(addr): "r"(data) : "memory");
    } else {
        outw(data,dev->ioport_start+offset);
    }
}

void virtio_pci_write_regb(struct virtio_pci_dev *dev, uint32_t offset, uint8_t data)
{
    if (dev->model!=VIRTIO_PCI_LEGACY_MODEL) {
	ERROR("supported only for legacy model\n");
	return;
    }
    if (dev->method==MEMORY) { 
        uint64_t addr = dev->mem_start + offset;
        __asm__ __volatile__ ("movb %1, (%0)" : "=r"(addr): "r"(data) : "memory");
    } else {
        outb(data,dev->ioport_start+offset);
    }
}



struct virtio_pci_cap {
    u8 cap_vndr;    /* Generic PCI field: PCI_CAP_ID_VNDR */
    u8 cap_next;    /* Generic PCI field: next ptr. */
    u8 cap_len;     /* Generic PCI field: capability length */
    u8 cfg_type;    /* Identifies the structure. */
    u8 bar;         /* Where to find it. */
    u8 padding[3];  /* Pad to full dword. */
    le32 offset;    /* Offset within bar. */
    le32 length;    /* Length of the structure, in bytes. */
};

/* Common configuration */
#define VIRTIO_PCI_CAP_COMMON_CFG        1
/* Notifications */
#define VIRTIO_PCI_CAP_NOTIFY_CFG        2
/* ISR Status */
#define VIRTIO_PCI_CAP_ISR_CFG           3
/* Device specific configuration */
#define VIRTIO_PCI_CAP_DEVICE_CFG        4
/* PCI configuration access */
#define VIRTIO_PCI_CAP_PCI_CFG           5

// for notify cap
struct virtio_pci_notify_cap { 
    struct virtio_pci_cap cap; 
    le32 notify_off_multiplier; /* Multiplier for queue_notify_off. */ 
};




static int is_legacy(struct virtio_pci_dev *vdev)
{
    struct pci_dev *pdev = vdev->pci_dev;
    struct pci_cfg_space *cfg = &pdev->cfg;

    return (cfg->device_id>=0x1000 && cfg->device_id<=0x1009) || (cfg->rev_id==0) || (cfg->dev_cfg.subsys_id<0x40);
}

static int is_modern(struct virtio_pci_dev *vdev)
{
    return !is_legacy(vdev);
}
    

static void modern_config_cap(void *state, void *data)
{
    struct virtio_pci_dev *vdev = (struct virtio_pci_dev *)state;
    struct virtio_pci_cap *cap = (struct virtio_pci_cap *)data;
    struct pci_dev *pdev = vdev->pci_dev;

    if (cap->cap_vndr==0x9) {
	// vendor cap on a virtio pci device
	// encodes important stuff
	DEBUG("pci cap cfg_type 0x%x bar %u offset 0x%x length 0x%x\n",
	      cap->cfg_type, cap->bar, cap->offset, cap->length);

	switch (cap->cfg_type) {
	case VIRTIO_PCI_CAP_COMMON_CFG: {
	    DEBUG("common config - bar=%d, baroffset=0x%x, len=0x%x\n",
		  cap->bar, cap->offset, cap->length);
	    vdev->common = (struct virtio_pci_common_cfg*)(pci_dev_get_bar_addr(pdev,cap->bar) + cap->offset);
	    DEBUG("common register block is at %p\n",vdev->common);
	}
	    break;
	case VIRTIO_PCI_CAP_NOTIFY_CFG: {
	    struct virtio_pci_notify_cap * ncap = (struct virtio_pci_notify_cap *)cap;
	    vdev->notify_off_multiplier = ncap->notify_off_multiplier;
        vdev->notify_base_addr = (uint32_t *)(pci_dev_get_bar_addr(pdev,cap->bar) + cap->offset);
	    DEBUG("notify config: multiplier set to %u\n",vdev->notify_off_multiplier);
        DEBUG("notify config: base addr set to %p\n",vdev->notify_base_addr);
	}
	    break;
	case VIRTIO_PCI_CAP_ISR_CFG:  {
	    DEBUG("isr config - skipped since we will use MSI-X\n");
	}
	    break;
	case VIRTIO_PCI_CAP_DEVICE_CFG: {
	    DEBUG("device-specific config - bar=%d, baroffset=0x%x, len=0x%x\n",
		  cap->bar, cap->offset, cap->length);
	    vdev->device_specific = (uint8_t *)(pci_dev_get_bar_addr(pdev,cap->bar) + cap->offset);
	    DEBUG("device-specific data %p 8 bytes: %x %x %x %x %x %x %x %x\n",vdev->device_specific,
		  vdev->device_specific[0],
		  vdev->device_specific[1],
		  vdev->device_specific[2],
		  vdev->device_specific[3],
		  vdev->device_specific[4],
		  vdev->device_specific[5],
		  vdev->device_specific[6],
		  vdev->device_specific[7]);
	}
	    break;
	case VIRTIO_PCI_CAP_PCI_CFG: {
	    DEBUG("pci config - skipped - we are using normal PCI config\n");
	}
	    break;
	default:
	    ERROR("unknown config type %d skipped\n", cap->cfg_type);
	    break;
	}
    }
}

static int modern_discover(struct virtio_pci_dev *vdev)
{
    struct pci_dev *pdev = vdev->pci_dev;
    struct pci_cfg_space *cfg = &pdev->cfg;

    vdev->model = VIRTIO_PCI_MODERN_MODEL;
    
    switch (cfg->device_id) {
    case 0x1040+16 :
	DEBUG("GPU Device\n");
	vdev->type = VIRTIO_PCI_GPU;
	break;
    default:
	DEBUG("Unknown device\n");
	vdev->type = VIRTIO_PCI_UNKNOWN;
	return -1;
    }

    // scan capabiliities
    pci_dev_scan_capabilities(pdev,modern_config_cap,vdev);


    return 0;

}

static int legacy_discover(struct virtio_pci_dev *vdev)
{
    struct pci_dev *pdev = vdev->pci_dev;
    struct pci_cfg_space *cfg = &pdev->cfg;
    struct pci_bus *bus = pdev->bus;

    vdev->model = VIRTIO_PCI_LEGACY_MODEL;
    
    switch (cfg->dev_cfg.subsys_id) { 
    case 1:
	DEBUG("Net Device\n");
	vdev->type = VIRTIO_PCI_NET;
	break;
    case 2:
	DEBUG("Block Device\n");
	vdev->type = VIRTIO_PCI_BLOCK;
	break;
    case 3:
	DEBUG("Console Device\n");
	vdev->type = VIRTIO_PCI_CONSOLE;
	break;
    case 4:
	DEBUG("Entropy Device\n");
	vdev->type = VIRTIO_PCI_ENTROPY;
	break;
    case 5:
	DEBUG("Balloon Device\n");
	vdev->type = VIRTIO_PCI_BALLOON;
	break;
    case 6:
	DEBUG("IOMemory Device\n");
	vdev->type = VIRTIO_PCI_IOMEM;
	break;
    case 7:
	DEBUG("rpmsg Device\n");
	vdev->type = VIRTIO_PCI_RPMSG;
	break;
    case 8:
	DEBUG("SCSI Host Device\n");
	vdev->type = VIRTIO_PCI_SCSI_HOST;
	break;
    case 9:
	DEBUG("9P Transport Device\n");
	vdev->type = VIRTIO_PCI_9P;
	break;
    case 10:
	DEBUG("WIFI Device\n");
	vdev->type = VIRTIO_PCI_WIFI;
	break;
    case 11:
	DEBUG("rproc serial Device\n");
	vdev->type = VIRTIO_PCI_RPROC_SERIAL;
	break;
    case 12:
	DEBUG("CAIF Device\n");
	vdev->type = VIRTIO_PCI_CAIF;
	break;
    case 13:
	DEBUG("Fancier Balloon Device\n");
	vdev->type = VIRTIO_PCI_FANCIER_BALLOON;
	break;
    case 16:
	DEBUG("GPU Device\n");
	vdev->type = VIRTIO_PCI_GPU;
	break;
    case 17:
	DEBUG("Timer Device\n");
	vdev->type = VIRTIO_PCI_TIMER;
	break;
    case 18:
	DEBUG("Input Device\n");
	vdev->type = VIRTIO_PCI_INPUT;
	break;
    default:
	DEBUG("Unknown Device (%d)\n",cfg->dev_cfg.subsys_id);
	vdev->type = VIRTIO_PCI_UNKNOWN;
	break;
    }
    
    // we expect two bars exist, one for memory, one for i/o
    // and these will be bar 0 and 1
    // check to see if there are no others
    int foundmem=0;
    int foundio=0;
    int i;
    for (i=0;i<6;i=pci_dev_get_bar_next(pdev,i)) {
	pci_bar_type_t type = pci_dev_get_bar_type(pdev,i);

	if (type==PCI_BAR_NONE) {
	    break;
	}

	uint64_t     addr = pci_dev_get_bar_addr(pdev,i);
	uint64_t     size = pci_dev_get_bar_size(pdev,i);
	
	if (i>=2) { 
	    DEBUG("Not expecting this to be a non-empty bar...\n");
	}

	if (type==PCI_BAR_IO) {
	    vdev->ioport_start = addr;
	    vdev->ioport_end = addr+size;
	    foundio=1;
	} else {
	    vdev->mem_start = addr;
	    vdev->mem_end = addr+size;
	    foundmem=1;
	}
    }

    if (foundio) {
	vdev->method = IO;
    } else if (foundmem) {
	vdev->method = MEMORY;
    } else {
	vdev->method = NONE;
	ERROR("Device has no register access method... Impossible...\n");
	panic("Device has no register access method... Impossible...\n");
	return -1;
    }

    return 0;
    
}

int discover_devices(struct pci_info *pci)
{
    struct list_head *curbus, *curdev;

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
		
		if (is_legacy(vdev)) {
		    DEBUG("Using legacy discovery\n");
		    if (legacy_discover(vdev)) {
			ERROR("Failed legacy discover\n");
			return -1;
		    }
		} else {
		    DEBUG("Using modern discovery\n");
		    if (modern_discover(vdev)) {
			ERROR("Failed modern discover\n");
			return -1;
		    }
		}
		
		if (pdev->msix.type==PCI_MSI_X) {
		    vdev->itype=VIRTIO_PCI_MSI_X_INTERRUPT;
		} else {
		    vdev->itype=VIRTIO_PCI_LEGACY_INTERRUPT;
		    // PCI Interrupt (A..D)
		    vdev->pci_intr = cfg->dev_cfg.intr_pin;
		    // Figure out mapping here or look at capabilities for MSI-X
		    // vdev->intr_vec = ...
		}
    
 		if (vdev->model==VIRTIO_PCI_LEGACY_MODEL) {
		    INFO("Found legacy virtio %s device: bus=%u dev=%u func=%u: pci_intr=%u intr_vec=%u ioport_start=%p ioport_end=%p mem_start=%p mem_end=%p access_method=%s\n",
			 vdev->type==VIRTIO_PCI_BLOCK ? "block" :
			 vdev->type==VIRTIO_PCI_NET ? "net" :
			 vdev->type==VIRTIO_PCI_GPU ? "gpu" : "other",
			 bus->num, pdev->num, 0,
			 vdev->pci_intr, vdev->intr_vec,
			 vdev->ioport_start, vdev->ioport_end,
			 vdev->mem_start, vdev->mem_end,
			 vdev->method==IO ? "IO" : vdev->method==MEMORY ? "MEMORY" : "NONE");
		}

 		if (vdev->model==VIRTIO_PCI_MODERN_MODEL) {
		    INFO("Found modern virtio %s device: bus=%u dev=%u func=%u: common=%p dev_spec=%p notif_mult=%u\n",
			 vdev->type==VIRTIO_PCI_BLOCK ? "block" :
			 vdev->type==VIRTIO_PCI_NET ? "net" :
			 vdev->type==VIRTIO_PCI_GPU ? "gpu" : "other",
			 bus->num, pdev->num, 0,
			 vdev->common, vdev->device_specific, vdev->notify_off_multiplier);
		}
		    
		
                list_add(&vdev->virtio_node,&dev_list);
            }
	    
        }
    }
    
    return 0;
}

#define ALIGN(x) (((x) + 4095UL) & ~4095UL) 

#define NUM_PAGES(x) ((x)/4096 + !!((x)%4096))

static inline unsigned compute_size(unsigned int qsz) 
{ 
    return ALIGN(sizeof(struct virtq_desc)*qsz + sizeof(uint16_t)*(3 + qsz)) 
        + ALIGN(sizeof(uint16_t)*3 + sizeof(struct virtq_used_elem)*qsz); 
}



static int virtqueue_init_legacy(struct virtio_pci_dev *dev)
{

    uint16_t i,j;
    uint64_t qsz;
    uint64_t qsz_numbytes;
    uint64_t alloc_size;


    DEBUG("Virtqueue legacy init of %u:%u.%u\n",dev->pci_dev->bus->num,dev->pci_dev->num,dev->pci_dev->fun);

    // now let's figure out the sizes

    dev->num_virtqs = 0;

    for (i=0;i<MAX_VIRTQS;i++) {
        virtio_pci_write_regw(dev,QUEUE_SEL,i);
        qsz = virtio_pci_read_regw(dev,QUEUE_SIZE);

        if (qsz==0) {
            // out of queues to support
            break;
        }

        DEBUG("Virtqueue %u has 0x%lx slots\n", i, qsz);

        qsz_numbytes = compute_size(qsz);

        DEBUG("Virtqueue %u has size 0x%lx bytes\n", i, qsz_numbytes);

        dev->virtq[i].size_bytes = qsz_numbytes;
        alloc_size = 4096 * (NUM_PAGES(qsz_numbytes) + 1);

        if (!(dev->virtq[i].data = malloc(alloc_size))) {
            ERROR("Cannot allocate ring\n");
            return -1;
        }

        memset(dev->virtq[i].data,0,alloc_size);

        dev->virtq[i].aligned_data = (uint8_t *)ALIGN((uint64_t)(dev->virtq[i].data));

        // init the standardized virtqueue structure's pointers
        dev->virtq[i].vq.qsz = qsz;
        dev->virtq[i].vq.desc = (struct virtq_desc *) (dev->virtq[i].aligned_data);
        dev->virtq[i].vq.avail = (struct virtq_avail *) 
            (dev->virtq[i].aligned_data 
                + sizeof(struct virtq_desc)*qsz);
        dev->virtq[i].vq.used = (struct virtq_used *) 
            (dev->virtq[i].aligned_data
                +  ALIGN(sizeof(struct virtq_desc)*qsz + sizeof(uint16_t)*(3 + qsz)));

        // init free list
        // desc j points to desc j+1
	// a value >=qsz indicates null
        dev->virtq[i].nfree = qsz;
        dev->virtq[i].head = 0;
        for (j = 0; j < qsz-1; j++) {
            dev->virtq[i].vq.desc[j].next = j+1;
        }
        spinlock_init(&dev->virtq[i].lock);
        
        // init last seen used
        dev->virtq[i].last_seen_used = 0;

        DEBUG("virtq allocation at %p for 0x%lx bytes\n", dev->virtq[i].data,alloc_size);
        DEBUG("virtq data at %p\n", dev->virtq[i].aligned_data);
        DEBUG("virtq qsz  = 0x%lx\n",dev->virtq[i].vq.qsz);
        DEBUG("virtq desc at %p\n", dev->virtq[i].vq.desc);
        DEBUG("virtq avail at %p\n", dev->virtq[i].vq.avail);
        DEBUG("virtq used at %p\n", dev->virtq[i].vq.used);

        // now tell device about the virtq
        // note it's a 32 bit xegister, but the address is a page address
        // so it really represents a 44 bit address (32 bits * 4096)
        virtio_pci_write_regl(dev,QUEUE_ADDR,(uint32_t)(((uint64_t)(dev->virtq[i].aligned_data))/4096));

	if (dev->itype==VIRTIO_PCI_MSI_X_INTERRUPT) {
	    // we still have the queue selected -
	    // we map it to obvious table entry
	    virtio_pci_write_regw(dev,QUEUE_VEC,i);
	}

        dev->num_virtqs++;
  }
  
  if (i==MAX_VIRTQS) { 
      ERROR("Device needs too many virtqueues!\n");
      return -1;
  }
  
  return 0;

}

static int virtqueue_init_modern(struct virtio_pci_dev *dev)
{
    uint16_t num;
    uint16_t i,j;
    uint64_t qsz;
    uint64_t qsz_numbytes;
    uint64_t alloc_size;


    DEBUG("Virtqueue modern init of %u:%u.%u\n",dev->pci_dev->bus->num,dev->pci_dev->num,dev->pci_dev->fun);
    num = virtio_pci_atomic_load(&dev->common->num_queues);

    DEBUG("device has %u virtqueues\n",num);

    // now let's figure out the sizes

    dev->num_virtqs = 0;

    for (i=0;i<num;i++) {
	
	virtio_pci_atomic_store(&dev->common->queue_select,i);
	qsz = virtio_pci_atomic_load(&dev->common->queue_size);

        if (qsz==0) {
            // out of queues to support
            break;
        }

        DEBUG("Virtqueue %u has 0x%lx slots\n", i, qsz);

        qsz_numbytes = compute_size(qsz);

        DEBUG("Virtqueue %u has size 0x%lx bytes\n", i, qsz_numbytes);

        dev->virtq[i].size_bytes = qsz_numbytes;
        alloc_size = 4096 * (NUM_PAGES(qsz_numbytes) + 1);

        if (!(dev->virtq[i].data = malloc(alloc_size))) {
            ERROR("Cannot allocate ring\n");
            return -1;
        }

        memset(dev->virtq[i].data,0,alloc_size);

        dev->virtq[i].aligned_data = (uint8_t *)ALIGN((uint64_t)(dev->virtq[i].data));

        // init the standardized virtqueue structure's pointers
        dev->virtq[i].vq.qsz = qsz;
        dev->virtq[i].vq.desc = (struct virtq_desc *) (dev->virtq[i].aligned_data);
        dev->virtq[i].vq.avail = (struct virtq_avail *) 
            (dev->virtq[i].aligned_data 
                + sizeof(struct virtq_desc)*qsz);
        dev->virtq[i].vq.used = (struct virtq_used *) 
            (dev->virtq[i].aligned_data
                +  ALIGN(sizeof(struct virtq_desc)*qsz + sizeof(uint16_t)*(3 + qsz)));

        // init free list
        // desc j points to desc j+1
	// a value >=qsz indicates null
        dev->virtq[i].nfree = qsz;
        dev->virtq[i].head = 0;
        for (j = 0; j < qsz-1; j++) {
            dev->virtq[i].vq.desc[j].next = j+1;
        }
        spinlock_init(&dev->virtq[i].lock);

        // init last seen used
        dev->virtq[i].last_seen_used = 0;

        DEBUG("virtq allocation at %p for 0x%lx bytes\n", dev->virtq[i].data,alloc_size);
        DEBUG("virtq data at %p\n", dev->virtq[i].aligned_data);
        DEBUG("virtq qsz  = 0x%lx\n",dev->virtq[i].vq.qsz);
        DEBUG("virtq desc at %p\n", dev->virtq[i].vq.desc);
        DEBUG("virtq avail at %p\n", dev->virtq[i].vq.avail);
        DEBUG("virtq used at %p\n", dev->virtq[i].vq.used);

	// tell the device where our virtqueue is
	virtio_pci_atomic_store(&dev->common->queue_desc, dev->virtq[i].vq.desc);
	virtio_pci_atomic_store(&dev->common->queue_driver, dev->virtq[i].vq.avail);
	virtio_pci_atomic_store(&dev->common->queue_device, dev->virtq[i].vq.used);
				

	if (dev->itype==VIRTIO_PCI_MSI_X_INTERRUPT) {
	    // we still have the queue selected -
	    // we map it to obvious table entry
	    // IS THIS RIGHT?
	    virtio_pci_atomic_store(&dev->common->queue_msix_vector,i);
	}

        dev->num_virtqs++;
  }
  
  if (i==MAX_VIRTQS) { 
      ERROR("Device needs too many virtqueues!\n");
      return -1;
  }
  
  return 0;

}


static int virtqueue_deinit_legacy(struct virtio_pci_dev *dev)
{
    uint16_t i;

    for (i=0;i<dev->num_virtqs;i++) {
        free(dev->virtq[i].data);
        dev->virtq[i].data=0;
    }
    return 0;
}


static int virtqueue_deinit_modern(struct virtio_pci_dev *dev)
{
    ERROR("unimplemented\n");
    return -1;
}

static int ack_device_legacy(struct virtio_pci_dev *dev)
{
    virtio_pci_write_regb(dev,DEVICE_STATUS,DEV_STATUS_RESET); 
    virtio_pci_write_regb(dev,DEVICE_STATUS,DEV_STATUS_ACKNOWLEDGE); 
    virtio_pci_write_regb(dev,DEVICE_STATUS,DEV_STATUS_ACKNOWLEDGE | DEV_STATUS_DRIVER);

    return 0;
}

static int ack_device_modern(struct virtio_pci_dev *dev)
{
    virtio_pci_atomic_store(&dev->common->device_status,DEV_STATUS_RESET);
    virtio_pci_atomic_store(&dev->common->device_status,DEV_STATUS_ACKNOWLEDGE);
    virtio_pci_atomic_store(&dev->common->device_status,DEV_STATUS_ACKNOWLEDGE | DEV_STATUS_DRIVER);
    
    return 0;
}

static int read_features_legacy(struct virtio_pci_dev *dev)
{
    dev->feat_offered = virtio_pci_read_regl(dev,DEVICE_FEATURES);
    
    return 0;    
}

static int read_features_modern(struct virtio_pci_dev *dev)
{
    // get MSB
    virtio_pci_atomic_store(&dev->common->device_feature_select,1);
    dev->feat_offered = virtio_pci_atomic_load(&dev->common->device_feature);
    dev->feat_offered <<= 32;
    // add LSB
    virtio_pci_atomic_store(&dev->common->device_feature_select,0);
    dev->feat_offered |= virtio_pci_atomic_load(&dev->common->device_feature);
    
    return 0;
}

static int write_features_legacy(struct virtio_pci_dev *dev, uint32_t features)
{
    virtio_pci_write_regl(dev,DRIVER_FEATURES,features);
    dev->feat_accepted = features;

    return 0;
}

static int write_features_modern(struct virtio_pci_dev *dev, uint64_t features)
{
    uint32_t temp;
    // set MSB
    temp = features>>32;
    virtio_pci_atomic_store(&dev->common->driver_feature_select,1);
    virtio_pci_atomic_store(&dev->common->driver_feature,temp);
    // set LSB
    temp = features;
    virtio_pci_atomic_store(&dev->common->driver_feature_select,0);
    virtio_pci_atomic_store(&dev->common->driver_feature,temp);

    // now indicate we are done
    uint8_t ds  = virtio_pci_atomic_load(&dev->common->device_status);
    ds |= DEV_STATUS_FEATURES_OK;
    virtio_pci_atomic_store(&dev->common->device_status,ds);

    // now reread to see if it bought it
    ds  = virtio_pci_atomic_load(&dev->common->device_status);

    if (ds & DEV_STATUS_FEATURES_OK) {
	// features accepted
	dev->feat_accepted = features;
	DEBUG("negotiated features accepted by device \n");
	return 0;
    } else {
	ERROR("negotiated features not accepted by device\n");
	return -1;
    }
}



static int start_device_legacy(struct virtio_pci_dev *dev)
{
    uint8_t r = virtio_pci_read_regb(dev,DEVICE_STATUS);

    r |= DEV_STATUS_DRIVER_OK;
    
    virtio_pci_write_regb(dev,DEVICE_STATUS,r);

    return 0;
}

static int start_device_modern(struct virtio_pci_dev *dev)
{
    ERROR("unimplemented\n");
    return -1;
}



#define DISPATCH_RET(dev,func,args...)		\
    switch ((dev)->model) {			\
    case VIRTIO_PCI_LEGACY_MODEL:		\
        return func##_legacy(dev, ##args);	\
	break;					\
    case VIRTIO_PCI_MODERN_MODEL:		\
        return func##_modern(dev, ##args);	\
	break;					\
    default:					\
        ERROR("unsupported model\n");		\
        return -1;                              \
    }                                           \


int virtio_pci_virtqueue_init(struct virtio_pci_dev *dev)
{
    DISPATCH_RET(dev,virtqueue_init);
}

int virtio_pci_virtqueue_deinit(struct virtio_pci_dev *dev)
{
    DISPATCH_RET(dev,virtqueue_deinit);
}

int virtio_pci_ack_device(struct virtio_pci_dev *dev)
{
    DISPATCH_RET(dev,ack_device);
}


int virtio_pci_start_device(struct virtio_pci_dev *dev)
{
    DISPATCH_RET(dev,start_device);
}	
     
int virtio_pci_read_features(struct virtio_pci_dev *dev)
{
    DISPATCH_RET(dev,read_features);
}

int virtio_pci_write_features(struct virtio_pci_dev *dev, uint64_t features)
{
    DISPATCH_RET(dev,write_features,features);
}



int virtio_pci_desc_chain_alloc(struct virtio_pci_dev *dev, uint16_t qidx, uint16_t *desc_idx, uint16_t count)
{
    STATE_LOCK_CONF;
    struct virtio_pci_virtq *virtq = &dev->virtq[qidx];
    uint16_t i;

    //DEBUG("chain alloc (%u) for virtq %d\n", count, qidx);

    STATE_LOCK(virtq);

    if (virtq->nfree < count) {
        STATE_UNLOCK(virtq);
        return -1;
    }

    uint16_t prev = 0;
    
    for (i=0;i<count;i++) { 
	uint16_t desc_head = virtq->head;
	uint16_t desc_next = virtq->vq.desc[desc_head].next;

	if (desc_head >= virtq->vq.qsz) {
	    int j;
	    STATE_UNLOCK(virtq);
	    ERROR("Erroneous allocation (%u)...\n",desc_head);
	    for (j=0;j<i;j++) {
		virtio_pci_desc_free(dev,qidx,desc_idx[j]);
	    }
	    return -1;
	}
	
	struct virtq_desc *desc = &virtq->vq.desc[desc_head];

	// setup new descriptor
	desc->flags = 0;
	desc->next = desc_head; // may be overwritten later in a chain;

	// update free list
	virtq->head = desc_next;
	virtq->nfree--;

	if (i>0) {
	    // stitch into chain
	    // a single allocation will not be stitched
	    struct virtq_desc *prev_desc = &virtq->vq.desc[prev];
	    prev_desc->flags = VIRTQ_DESC_F_NEXT;
	    prev_desc->next = desc_head;
	}
	    
	desc_idx[i] = desc_head;
	prev = desc_head;

	//DEBUG("Allocated chain entry %u for virtq %d is descriptor %u\n",i,qidx,desc_head);
    }

    STATE_UNLOCK(virtq);

   
    return 0;
}

// Allocates a single descriptor
int virtio_pci_desc_alloc(struct virtio_pci_dev *dev, uint16_t qidx, uint16_t *desc_idx)
{
    return virtio_pci_desc_chain_alloc(dev,qidx,desc_idx,1);
}

static int virtio_pci_desc_free_internal(struct virtio_pci_dev *dev, uint16_t qidx, uint16_t desc_idx, int chain)
{
    uint16_t i=0;
    
    STATE_LOCK_CONF;

    struct virtio_pci_virtq *virtq = &dev->virtq[qidx];

    
    STATE_LOCK(virtq);

    if (desc_idx >= virtq->vq.qsz) {
	STATE_UNLOCK(virtq);
	ERROR("Erroneous free (%u)...\n",desc_idx);
	return -1;
    }

    for (i=0;;i++) {
	//DEBUG("Free chain entry %u for virtq %d is descriptor %u\n", i, qidx, desc_idx);

        struct virtq_desc *desc = &virtq->vq.desc[desc_idx];

        if (virtq->nfree == virtq->vq.qsz) {
	    ERROR("Impossible free\n");
            STATE_UNLOCK(virtq);
            return -1;
        }

        uint16_t prev_head = virtq->head;
        uint16_t next = desc->next;
        uint16_t flags = desc->flags;

        memset(desc, 0, sizeof(*desc));
        desc->next = prev_head;

        virtq->head = desc_idx;
        virtq->nfree++;

        if (chain && (flags & VIRTQ_DESC_F_NEXT)) {
            desc_idx = next;
        } else {
            break;
        }
    }

    STATE_UNLOCK(virtq);

    return 0;
}


int virtio_pci_desc_free(struct virtio_pci_dev *dev, uint16_t qidx, uint16_t desc_idx)
{
    return virtio_pci_desc_free_internal(dev,qidx,desc_idx,0);
}

int virtio_pci_desc_chain_free(struct virtio_pci_dev *dev, uint16_t qidx, uint16_t desc_idx)
{
    return virtio_pci_desc_free_internal(dev,qidx,desc_idx,1);
}




int virtio_pci_virtqueue_notify(struct virtio_pci_dev *dev, uint16_t qidx) 
{  
    if (dev->model == VIRTIO_PCI_LEGACY_MODEL) {
	virtio_pci_write_regw(dev,QUEUE_NOTIFY,qidx);
	return 0;
    } else if (dev->model == VIRTIO_PCI_MODERN_MODEL) {
	
	virtio_pci_atomic_store(&dev->common->queue_select, qidx);
	
	uint64_t offset = virtio_pci_atomic_load(&dev->common->queue_notify_off);
	uint64_t mult_offset = offset * dev->notify_off_multiplier;
	uint32_t *addr = (uint32_t*)(((addr_t)dev->notify_base_addr)+mult_offset);

	virtio_pci_atomic_store(addr, 0xFFFFFFFF);

	return 0;
    } else {
	ERROR("Unknown model\n");
	
	return -1;
    }
}


					 
static int bringup_device(struct virtio_pci_dev *dev)
{
    DEBUG("Bringing up device %u:%u.%u\n",dev->pci_dev->bus->num,dev->pci_dev->num,dev->pci_dev->fun);
    // switch on MSI-X here because it will change device layout
    if (dev->itype==VIRTIO_PCI_MSI_X_INTERRUPT) {
	if (pci_dev_enable_msi_x(dev->pci_dev)) {
	    ERROR("Failed to enable MSI-X on device...\n");
	    return -1;
	}
    }
    switch (dev->type) {
#ifdef NAUT_CONFIG_VIRTIO_NET
    case VIRTIO_PCI_NET:
	return virtio_net_init(dev);
	break;
#endif
#ifdef NAUT_CONFIG_VIRTIO_BLK
    case VIRTIO_PCI_BLOCK:
	return virtio_blk_init(dev);
	break;
#endif
#ifdef NAUT_CONFIG_VIRTIO_GPU
    case VIRTIO_PCI_GPU:
	return virtio_gpu_init(dev);
	break;
#endif
    default:
        INFO("Skipping unsupported device type\n");
        return 0;
        break;
    }
}


static int bringup_devices()
{
    struct list_head *curdev, tx_node;
    int rc;

    rc = 0;

    DEBUG("Bringing up virtio devices\n");
    int num = 0;

    list_for_each(curdev,&(dev_list)) { 
        struct virtio_pci_dev *dev = list_entry(curdev,struct virtio_pci_dev,virtio_node);
        int ret = bringup_device(dev);
        if (ret) { 
            ERROR("Bringup of virtio device failed\n");
            rc = -1;
        }
    }

    return rc;
  
}

int virtio_pci_init(struct naut_info * naut)
{
    
    if (discover_devices(naut->sys.pci)) {
        ERROR("Discovery failed\n");
        return -1;
    }

    return bringup_devices();
}
    

int virtio_pci_deinit()
{
    // should really scan list of devices and tear down...
    INFO("deinited\n");
    return 0;
}



