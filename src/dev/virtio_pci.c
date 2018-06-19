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
                    DEBUG("Unknown Device\n");
                    vdev->type = VIRTIO_PCI_UNKNOWN;
                    break;
                }

                if (pdev->msix.type==PCI_MSI_X) {
                    vdev->itype=VIRTIO_PCI_MSI_X;
                } else {
                    vdev->itype=VIRTIO_PCI_LEGACY;
                    // PCI Interrupt (A..D)
                    vdev->pci_intr = cfg->dev_cfg.intr_pin;
                    // Figure out mapping here or look at capabilities for MSI-X
                    // vdev->intr_vec = ...
                }
                    
                // BAR handling will eventually be done by common code in PCI
		    
                // we expect two bars exist, one for memory, one for i/o
                // and these will be bar 0 and 1
                // check to see if there are no others
                int foundmem=0;
                int foundio=0;
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
                        foundio=1;
                    } else {
                        vdev->mem_start = bar & 0xfffffff0;
                        vdev->mem_end = vdev->mem_start + size;
                        foundmem=1;
                    }

                }

                // for now, privilege the IO port interface, since we want
                // the legacy interface (4.1.4.8)
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

                vdev->pci_dev = pdev;

                INFO("Found virtio %s device: bus=%u dev=%u func=%u: pci_intr=%u intr_vec=%u ioport_start=%p ioport_end=%p mem_start=%p mem_end=%p access_method=%s\n",
                     vdev->type==VIRTIO_PCI_BLOCK ? "block" :
                     vdev->type==VIRTIO_PCI_NET ? "net" : "other",
                     bus->num, pdev->num, 0,
                     vdev->pci_intr, vdev->intr_vec,
                     vdev->ioport_start, vdev->ioport_end,
                     vdev->mem_start, vdev->mem_end,
                     vdev->method==IO ? "IO" : vdev->method==MEMORY ? "MEMORY" : "NONE");
                 

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



// this is all based on the legacy interface
int virtio_pci_virtqueue_init(struct virtio_pci_dev *dev)
{

    uint16_t i,j;
    uint64_t qsz;
    uint64_t qsz_numbytes;
    uint64_t alloc_size;


    DEBUG("Virtqueue init of %u:%u.%u\n",dev->pci_dev->bus->num,dev->pci_dev->num,dev->pci_dev->fun);

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

	if (dev->itype==VIRTIO_PCI_MSI_X) {
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

int virtio_pci_virtqueue_deinit(struct virtio_pci_dev *dev)
{
    uint16_t i;

    for (i=0;i<dev->num_virtqs;i++) {
        free(dev->virtq[i].data);
        dev->virtq[i].data=0;
    }
    return 0;
}


int virtio_pci_ack_device(struct virtio_pci_dev *dev)
{
    virtio_pci_write_regb(dev,DEVICE_STATUS,0x0); // driver resets device
    virtio_pci_write_regb(dev,DEVICE_STATUS,0b1); // driver acknowledges device
    virtio_pci_write_regb(dev,DEVICE_STATUS,0b11); // driver can drive device

    return 0;
}

int virtio_pci_read_features(struct virtio_pci_dev *dev)
{
    dev->feat_offered = virtio_pci_read_regl(dev,DEVICE_FEATURES);

    return 0;    
}

int virtio_pci_write_features(struct virtio_pci_dev *dev, uint32_t features)
{
    virtio_pci_write_regl(dev,DRIVER_FEATURES,features);
    dev->feat_accepted = features;

    return 0;
}

int virtio_pci_start_device(struct virtio_pci_dev *dev)
{
    virtio_pci_write_regb(dev,DEVICE_STATUS,0b111);

    return 0;
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

					 
static int bringup_device(struct virtio_pci_dev *dev)
{
    DEBUG("Bringing up device %u:%u.%u\n",dev->pci_dev->bus->num,dev->pci_dev->num,dev->pci_dev->fun);
    // switch on MSI-X here because it will change device layout
    if (dev->itype==VIRTIO_PCI_MSI_X) {
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



