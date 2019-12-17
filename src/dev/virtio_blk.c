/*
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the
 * United States National Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2018  Gino Wang
 * Copyright (c) 2018  Jin Han 
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Gino Wang <sihengwang2019@u.northwestern.edu>
 *          Jin Han <jinhan2019@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/blkdev.h>
#include <nautilus/irq.h>

#include <dev/pci.h>
#include <dev/virtio_blk.h>

#ifndef NAUT_CONFIG_DEBUG_VIRTIO_BLK
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define INFO(fmt, args...) INFO_PRINT("virtio_blk: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("virtio_blk: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("virtio_blk: " fmt, ##args)

#define FBIT_ISSET(features, bit) ((features) & (0x01 << (bit)))
#define FBIT_SETIF(features_out, features_in, bit) if (FBIT_ISSET(features_in,bit)) { features_out |= (0x01 << (bit)) ; }
#define DEBUG_FBIT(features, bit) if (FBIT_ISSET(features, bit)) {	\
	DEBUG("feature bit set: %s\n", #bit);				\
    }


#define HACKED_LEGACY_VECTOR 0xe5
#define HACKED_LEGACY_IRQ    (HACKED_LEGACY_VECTOR - 32)


#define VIRTIO_BLK_OFF_CONFIG(v)     (virtio_pci_device_regs_start_legacy(v) + 0)

#define VIRTIO_BLK_REQUEST_QUEUE  0 // virtqueue index 

#define VIRTIO_BLK_T_IN           0 // read request
#define VIRTIO_BLK_T_OUT          1 // write request
#define VIRTIO_BLK_T_FLUSH        4 // flush request

#define VIRTIO_BLK_S_OK           0 // success
#define VIRTIO_BLK_S_IOERR        1 // host or guest error
#define VIRTIO_BLK_S_UNSUPP       2 // unsupported by host

#define HEADER_DESC_LEN           16  // header descriptor length
#define STATUS_DESC_LEN           1   // status descriptor length



/* Maximum size of any single segment is in "size_max" */
#define VIRTIO_BLK_F_SIZE_MAX		1

/* Maximum number of segments in a request in in "seg_max" */
#define VIRTIO_BLK_F_SEG_MAX		2

/* Disk-style geometry specified in "geometry" */
#define VIRTIO_BLK_F_GEOMETRY		4

/* Devie is read-only */
#define VIRTIO_BLK_F_RO				5

/* Block size of disk is in "blk_size" */
#define VIRTIO_BLK_F_BLK_SIZE   	6

/* Cache lush command support */
#define VIRTIO_BLK_F_FLUSH      	9    

/* Device exports information on optimal I/O alignment. */
#define VIRTIO_BLK_F_TOPOLOGY   	10

/* Device can toggle its cache between writeback andw ritethrough modes. */
#define VIRTIO_BLK_F_CONFIG_WCE  	11   

/* Legacy Interface: Feature bits */

/* Host supports request barriers */ 
#define VIRTIO_BLK_F_BARRIER		0 

/* Device supports a scsi packet commands */
#define VIRTIO_BLK_F_SCSI       	7


static uint64_t num_devs = 0;

struct virtio_blk_dev {
    struct nk_block_dev         *blk_dev;     // nautilus block device
    struct virtio_pci_dev       *virtio_dev;  // nautilus pci device
    struct virtio_blk_config    *blk_config;  // virtio blk configuration
    struct virtio_blk_callb     *blk_callb;   // virtio blk callbacks
};

struct virtio_blk_config {
    uint64_t capacity;  // device size
    uint32_t size_max;  // max size of any single descriptor 
    uint32_t seg_max;   // total number of descriptors 
    struct virtio_blk_geometry {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;         // device geometry 
    uint32_t blk_size;  // optimal sector size
};

struct virtio_blk_req {
    uint32_t type;      // read or write request
    uint32_t reserved;  // write back feature
    uint64_t sector;    // offset for read or write to occur
    uint8_t *data;      // ... not used
    uint8_t status;     // written by device
};

struct virtio_blk_callb {
    void *context;
    void (*callback)(nk_block_dev_status_t, void *);
};

/************************************************************
 ****************** block ops for kernel ********************
 ************************************************************/
static int get_characteristics(void *state, struct nk_block_dev_characteristics *c)
{
    if (!state || !c) {
	ERROR("unable to get characteristics of block device\n");
	return -1;
    }

    struct virtio_blk_dev *dev = (struct virtio_blk_dev *) state;
    c->block_size = dev->blk_config->blk_size;
    c->num_blocks = dev->blk_config->capacity; 

    return 0;
}

static void fill_hdr_desc(struct virtq *vq, struct virtio_blk_req *hdr, uint16_t hdr_index, uint16_t buf_index)
{
    struct virtq_desc *hdr_desc = &vq->desc[hdr_index];

    hdr_desc->addr = (uint64_t) hdr;
    hdr_desc->len = HEADER_DESC_LEN;
    hdr_desc->flags = 0;
    hdr_desc->next = buf_index;
    hdr_desc->flags |= VIRTQ_DESC_F_NEXT;
}

static void fill_buf_desc(struct virtq *vq, uint32_t size, uint64_t count, uint8_t *dest, uint16_t buf_index, uint16_t stat_index, uint8_t write)
{
    struct virtq_desc *buf_desc = &vq->desc[buf_index];

    buf_desc->addr = (uint64_t) dest;
    buf_desc->len = size * count;
    buf_desc->flags |= write ? 0 : VIRTQ_DESC_F_WRITE;
    buf_desc->next = stat_index;
    buf_desc->flags |= VIRTQ_DESC_F_NEXT;
}

static void fill_stat_desc(struct virtq *vq, uint8_t *status, uint16_t stat_index)
{
    struct virtq_desc *stat_desc = &vq->desc[stat_index];

    stat_desc->addr = (uint64_t) status;  
    stat_desc->flags = 0;
    stat_desc->flags |= VIRTQ_DESC_F_WRITE;
    stat_desc->len = STATUS_DESC_LEN;
    stat_desc->next = 0;
}

static int read_write_blocks(struct virtio_blk_dev *dev, uint64_t blocknum, uint64_t count, uint8_t *src_dest, void (*callback)(nk_block_dev_status_t, void *), void *context, uint8_t write) 
{
    DEBUG("%s blocknum = %lu count = %lu buf = %p callback = %p context = %p\n", write ? "write" : "read", blocknum, count, src_dest, callback, context);
    
    if (blocknum + count > dev->blk_config->capacity) {
        ERROR("request goes beyond device capacity\n");
        return -1;
    }

    if (write && (FBIT_ISSET(dev->virtio_dev->feat_accepted,VIRTIO_BLK_F_RO))) {
	ERROR("attempt to write read-only device\n");
	return -1;
    }

    DEBUG("[build request header]\n");

    struct virtio_blk_req *hdr = malloc(sizeof(struct virtio_blk_req));

    if (!hdr) {
	ERROR("Failed to allocate request header\n");
	return -1;
    }
    
    memset(hdr, 0, sizeof(struct virtio_blk_req));

    hdr->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    hdr->sector = blocknum;
    hdr->reserved = 0;
    hdr->status = 0;

    DEBUG("[allocate descriptors]\n");

    uint16_t desc[3];

    if (virtio_pci_desc_chain_alloc(dev->virtio_dev,VIRTIO_BLK_REQUEST_QUEUE,desc,3)) {
	ERROR("Failed to allocate descriptor chain\n");
	free(hdr);
	return -1;
    }
    
    uint16_t hdr_index = desc[0];
    uint16_t buf_index = desc[1];
    uint16_t stat_index = desc[2];

    struct virtq *vq = &dev->virtio_dev->virtq[VIRTIO_BLK_REQUEST_QUEUE].vq;

    DEBUG("[create descriptors]\n");

    DEBUG("[create header descriptor]\n");
    fill_hdr_desc(vq, hdr, hdr_index, buf_index);

    DEBUG("[create buffer descriptor]\n");
    fill_buf_desc(vq, dev->blk_config->blk_size, count, src_dest, buf_index, stat_index, write);

    DEBUG("[create status descriptor]\n");
    fill_stat_desc(vq, &hdr->status, stat_index);

    dev->blk_callb[hdr_index].callback = callback;
    dev->blk_callb[hdr_index].context = context;

    DEBUG("request in indexes: header = %d, buffer = %d, status = %d\n", hdr_index, buf_index, stat_index);

    // update avail ring
    vq->avail->ring[vq->avail->idx % vq->qsz] = hdr_index;
    mbarrier();
    vq->avail->idx++;
    mbarrier();
    
    DEBUG("available ring's hdr index = %d, at ring index %d\n", hdr_index, vq->avail->idx - 1);
    DEBUG("available ring's ring index for next hdr = %u\n", vq->avail->idx);

    DEBUG("[notify device]\n");
    virtio_pci_write_regw(dev->virtio_dev, QUEUE_NOTIFY, VIRTIO_BLK_REQUEST_QUEUE);
    
    return 0;
}

static int read_blocks(void *state, uint64_t blocknum, uint64_t count, uint8_t *dest, void (*callback)(nk_block_dev_status_t, void *), void *context)
{
    struct virtio_blk_dev *dev = (struct virtio_blk_dev *) state;

    return read_write_blocks(dev, blocknum, count, dest, callback, context, 0);
}

static int write_blocks(void *state, uint64_t blocknum, uint64_t count, uint8_t *src, void (*callback)(nk_block_dev_status_t,void *), void *context)
{
    struct virtio_blk_dev *dev = (struct virtio_blk_dev *) state;

    return read_write_blocks(dev, blocknum, count, src, callback, context, 1);
}

static struct nk_block_dev_int ops = {
    .get_characteristics = get_characteristics,
    .read_blocks = read_blocks,
    .write_blocks = write_blocks,
};

/************************************************************
 *************** interrupt handler & callback ***************
 ************************************************************/
static void teardown(struct virtio_pci_dev *dev) 
{
    // actually do frees... and reset device here...
    virtio_pci_virtqueue_deinit(dev);
}

static int process_used_ring(struct virtio_blk_dev *dev) 
{
    uint16_t hdr_desc_idx; 
    void (*callback)(nk_block_dev_status_t, void *);
    void *context;
    struct virtio_pci_virtq *virtq = &dev->virtio_dev->virtq[VIRTIO_BLK_REQUEST_QUEUE];
    struct virtq *vq = &dev->virtio_dev->virtq[VIRTIO_BLK_REQUEST_QUEUE].vq;
     
    DEBUG("[processing used ring]\n");
    DEBUG("current virtq used index = %d\n", virtq->vq.used->idx);
    DEBUG("last seen used index = %d\n", virtq->last_seen_used);
     
    for (; virtq->last_seen_used != virtq->vq.used->idx; virtq->last_seen_used++) {
	
	// grab the head of used descriptor chain
	hdr_desc_idx = vq->used->ring[virtq->last_seen_used % virtq->vq.qsz].id;
	 
	if (vq->desc[hdr_desc_idx].flags != VIRTQ_DESC_F_NEXT)  {
	    ERROR("Huh? head in the used ring is not a header descriptor\n");
	    return -1;
	}
	 
	struct virtq_desc *hdr_desc = &vq->desc[hdr_desc_idx];
	struct virtio_blk_req *hdr = (struct virtio_blk_req *)hdr_desc->addr;
	uint8_t status = hdr->status;
	 
	DEBUG("completion for descriptor at index %d with status: %d\n", hdr_desc_idx, status);
	 
	// grab corresponding callback
	callback = dev->blk_callb[hdr_desc_idx].callback;
	context = dev->blk_callb[hdr_desc_idx].context;
	 
	memset(&dev->blk_callb[hdr_desc_idx],0,sizeof(dev->blk_callb[hdr_desc_idx]));
	 
	free(hdr);

	DEBUG("descriptor hdr index = %u, callback = %p, context = %p\n", hdr_desc_idx, callback, context);
	 
	DEBUG("free used descriptors\n");

	if (virtio_pci_desc_chain_free(dev->virtio_dev, VIRTIO_BLK_REQUEST_QUEUE, hdr_desc_idx)) {
	    ERROR("error freeing descriptors\n");
	    return -1;
	}
	 
	if (callback) {
	    DEBUG("[issuing callback]\n");
	    callback(status ? NK_BLOCK_DEV_STATUS_ERROR : NK_BLOCK_DEV_STATUS_SUCCESS,context);
	}
    }
     
    return 0;
}

static int handler(excp_entry_t *exp, excp_vec_t vec, void *priv_data)
{
    DEBUG("[received an interrupt!]\n");
    struct virtio_blk_dev *dev = (struct virtio_blk_dev *) priv_data;
    
    // only for legacy style interrupt
    if (dev->virtio_dev->itype == VIRTIO_PCI_LEGACY_INTERRUPT) {
        DEBUG("using legacy style interrupt\n");
        // read the interrupt status register, which will reset it to zero
        uint8_t isr = virtio_pci_read_regb(dev->virtio_dev, ISR_STATUS);
	
        // if the lower bit is not set, not my interrupt
        if (!(isr & 0x1))  {
	    DEBUG("not my interrupt\n");
	    IRQ_HANDLER_END();
	    return 0;
        }
    }
    
    if (process_used_ring(dev)) {
	ERROR("failed to process used ring\n");
	IRQ_HANDLER_END();
	return -1;
    } 

    // print used for test
    //DEBUG("free count after = %d\n", dev->virtio_dev->virtq[VIRTIO_BLK_REQUEST_QUEUE].nfree);
    //uint8_t used_index = dev->virtio_dev->virtq[VIRTIO_BLK_REQUEST_QUEUE].vq.used->idx;
    //DEBUG("used ring next index = %d\n", used_index);

    DEBUG("[interrupt handler finished]\n");
    IRQ_HANDLER_END();
    return 0;
}

/*************************************************
 ******************** tests **********************
 *************************************************/

static volatile int done=0;

static void sudo_callback(nk_block_dev_status_t status, void *context)
{
    DEBUG("sudo_callback invoked: context = %s\n", (char *) context);
    done = 1;
}

static int test_read(struct virtio_blk_dev *dev) 
{
    DEBUG("testing read...\n");
    uint64_t blocknum = 0;
    uint64_t count = 1;
    uint32_t blk_size = dev->blk_config->blk_size;
    uint8_t *src = malloc(count * blk_size); 

    if (!src) {
	ERROR("Failed to allocate space for read test\n");
	return -1;
    }
    
    DEBUG("free count before = %d\n", dev->virtio_dev->virtq[VIRTIO_BLK_REQUEST_QUEUE].nfree);
    
    uint16_t i;
    for (i = 0; i < 16; i++) {
	DEBUG("before: %x\n", *(src + i));
    }

    done = 0;
    
    if (read_blocks(dev, blocknum, count, src, sudo_callback, "test read_blocks")) {
	ERROR("failed to test device\n");
	free(src);
	return -1;
    }

    while (!done) { }
    
    for (i = 0; i < 16; i++) {
	DEBUG("result: %x\n", *(src + i));
    }

    free(src);
    
    return 0;
}

static int test_write(struct virtio_blk_dev *dev) 
{
    DEBUG("testing write...\n");
    uint64_t blocknum = 512;
    uint64_t count = 1;
    uint32_t blk_size = dev->blk_config->blk_size;
    uint8_t *src = malloc(count * blk_size); 
    
    if (!src) {
	ERROR("Failed to allocate space for write test\n");
	return -1;
    }
    
    DEBUG("free count before = %d\n", dev->virtio_dev->virtq[VIRTIO_BLK_REQUEST_QUEUE].nfree);
    
    memset(src, 1, count * blk_size);

    uint16_t i;
    for (i = 0; i < 16; i++) {
	DEBUG("before: %x\n", *(src + i));
    }

    done = 0;
    
    if (write_blocks(dev, blocknum, count, src, sudo_callback, "test read_blocks")) {
	ERROR("failed to test device\n");
	return -1;
    }

    while (!done) {}
    
    for (i = 0; i < 16; i++) {
	DEBUG("result: %x\n", *(src+ i));
    }

    free(src);
    
    return 0;
}

/************************************************************
 ****************** device initialization *******************
 ************************************************************/
uint64_t select_features(uint64_t features) 
{
    DEBUG("device features: 0x%0lx\n",features);
    DEBUG_FBIT(features, VIRTIO_BLK_F_SIZE_MAX);
    DEBUG_FBIT(features, VIRTIO_BLK_F_SEG_MAX);
    DEBUG_FBIT(features, VIRTIO_BLK_F_GEOMETRY);
    DEBUG_FBIT(features, VIRTIO_BLK_F_RO);
    DEBUG_FBIT(features, VIRTIO_BLK_F_BLK_SIZE);
    DEBUG_FBIT(features, VIRTIO_BLK_F_FLUSH);
    DEBUG_FBIT(features, VIRTIO_BLK_F_TOPOLOGY);
    DEBUG_FBIT(features, VIRTIO_BLK_F_CONFIG_WCE);
    DEBUG_FBIT(features, VIRTIO_BLK_F_BARRIER);
    DEBUG_FBIT(features, VIRTIO_BLK_F_SCSI);
    DEBUG_FBIT(features, VIRTIO_F_NOTIFY_ON_EMPTY);
    DEBUG_FBIT(features, VIRTIO_F_ANY_LAYOUT);
    DEBUG_FBIT(features, VIRTIO_F_INDIRECT_DESC);
    DEBUG_FBIT(features, VIRTIO_F_EVENT_IDX);

    // choose accepted features
    uint64_t accepted = 0;

    FBIT_SETIF(accepted,features,VIRTIO_BLK_F_SIZE_MAX);
    FBIT_SETIF(accepted,features,VIRTIO_BLK_F_SEG_MAX);
    FBIT_SETIF(accepted,features,VIRTIO_BLK_F_GEOMETRY);
    FBIT_SETIF(accepted,features,VIRTIO_BLK_F_RO);
    FBIT_SETIF(accepted,features,VIRTIO_BLK_F_BLK_SIZE);
    
    DEBUG("features accepted: 0x%0lx\n", accepted);
    return accepted;
}

static void parse_config(struct virtio_blk_dev *d)
{
    struct virtio_pci_dev *dev = d->virtio_dev;
    
    memset(d->blk_config,0,sizeof(*(d->blk_config)));
    
    // must have capacity...
    d->blk_config->capacity = virtio_pci_read_regl(dev, VIRTIO_BLK_OFF_CONFIG(dev) + 0);
    if (dev->feat_accepted & VIRTIO_BLK_F_SIZE_MAX) { 
	d->blk_config->size_max = virtio_pci_read_regl(dev, VIRTIO_BLK_OFF_CONFIG(dev) + 8);
    }
    if (dev->feat_accepted & VIRTIO_BLK_F_SEG_MAX) { 
	d->blk_config->seg_max = virtio_pci_read_regl(dev, VIRTIO_BLK_OFF_CONFIG(dev) + 12);
    }
    if (dev->feat_accepted & VIRTIO_BLK_F_GEOMETRY) { 
	d->blk_config->geometry.cylinders = virtio_pci_read_regl(dev, VIRTIO_BLK_OFF_CONFIG(dev) + 16);
	d->blk_config->geometry.heads = virtio_pci_read_regb(dev, VIRTIO_BLK_OFF_CONFIG(dev) + 18);
	d->blk_config->geometry.sectors = virtio_pci_read_regb(dev, VIRTIO_BLK_OFF_CONFIG(dev) + 19);
    }
    if (dev->feat_accepted & VIRTIO_BLK_F_BLK_SIZE) { 
	d->blk_config->blk_size = virtio_pci_read_regl(dev, VIRTIO_BLK_OFF_CONFIG(dev) + 20);
    } else {
	d->blk_config->blk_size = 512; // presumably...
    }
    
    DEBUG("block device configuration layout\n");
    DEBUG("capacity           = %d\n", d->blk_config->capacity);
    DEBUG("size_max           = %d\n", d->blk_config->size_max);
    DEBUG("seg_max            = %d\n", d->blk_config->seg_max);
    DEBUG("geometry_cylinders = %d\n", d->blk_config->geometry.cylinders);
    DEBUG("geometry_heads     = %d\n", d->blk_config->geometry.heads);
    DEBUG("geometry_sectors   = %d\n", d->blk_config->geometry.sectors);
    DEBUG("blk_size           = %d\n", d->blk_config->blk_size);
}

int virtio_blk_init(struct virtio_pci_dev *dev)
{
    char buf[DEV_NAME_LEN];

    if (!dev->model==VIRTIO_PCI_LEGACY_MODEL) {
	ERROR("currently only supported with legacy model\n");
	return -1;
    }
    
    DEBUG("initialize device\n");
    
    // initialize block device
    struct virtio_blk_dev *d = malloc(sizeof(*d));
    if (!d) {
	ERROR("cannot allocate state\n");
	return -1;
    }
    memset(d,0,sizeof(*d));
    
    // acknowledge device
    if (virtio_pci_ack_device(dev)) {
        ERROR("Could not acknowledge device\n");
        free(d);
        return -1;
    }

    // read device feature bits
    if (virtio_pci_read_features(dev)) {
        ERROR("Unable to read device features\n");
        free(d);
        return -1;
    }

    // accept feature bits
    if (virtio_pci_write_features(dev, select_features(dev->feat_offered))) {
        ERROR("Unable to write device features\n");
        free(d);
        return -1;
    }
    
    // initilize device virtqueue
    if (virtio_pci_virtqueue_init(dev)) {
	ERROR("failed to initialize virtqueues\n");
	free(d);
	return -1;
    }
    
    dev->state = d;
    dev->teardown = teardown;
    d->virtio_dev = dev;
    
    // allocate callback array
    d->blk_callb = malloc(dev->virtq[0].vq.qsz * sizeof(struct virtio_blk_callb));
    
    DEBUG("allocated %d callbacks at %p\n", dev->virtq[0].vq.qsz, d->blk_callb);
    
    if (!d->blk_callb) {
	ERROR("failed to allocate callback array\n");
	virtio_pci_virtqueue_deinit(dev);
	free(d);
	return -1;
    }
    
    // allocate virtio block configuration
    d->blk_config = malloc(sizeof(struct virtio_blk_config));
    
    DEBUG("allocated virtio block config struct at %p for %hhx bytes\n", d->blk_callb, sizeof(struct virtio_blk_config));
    
    if (!d->blk_config) {
	ERROR("failed to allocate virtio block config struct\n");
	virtio_pci_virtqueue_deinit(dev);
	free(d->blk_callb);
	free(d);
	return -1;
    }
    
    parse_config(d);
    
    // register virtio block device
    snprintf(buf,DEV_NAME_LEN,"virtio-blk%u",__sync_fetch_and_add(&num_devs,1));
    d->blk_dev = nk_block_dev_register(buf,0,&ops,d);			       
    
    if (!d->blk_dev) {
	ERROR("failed to register block device\n");
	virtio_pci_virtqueue_deinit(dev);
	free(d->blk_config);
	free(d->blk_callb);
	free(d);
	return -1;
    }
    
    // We assume that interrupt allocations will not fail...
    // if we do fail, the rest of this code will leak
    
    struct pci_dev *p = dev->pci_dev;
    uint8_t i;
    ulong_t vec;
    
    if (dev->itype==VIRTIO_PCI_MSI_X_INTERRUPT) {
	// we assume MSI-X has been enabled on the device
	// already, that virtqueue setup is done, and
	// that queue i has been mapped to MSI-X table entry i
	// MSI-X is on but whole function is masked

	DEBUG("setting up interrupts via MSI-X\n");
	
	if (dev->num_virtqs != p->msix.size) {
	    ERROR("weird mismatch: numqueues=%u msixsize=%u\n", dev->num_virtqs, p->msix.size);
	    // continue for now...
	    // return -1;
	}
	
	// this should really go by virtqueue, not entry
	// and ideally pulled into a per-queue setup routine
	// in virtio_pci...
	uint16_t num_vec = p->msix.size;
        
	// now fill out the device's MSI-X table
	for (i=0;i<num_vec;i++) {
	    // find a free vector
	    // note that prioritization here is your problem
	    if (idt_find_and_reserve_range(1,0,&vec)) {
		ERROR("cannot get vector...\n");
		return -1;
	    }
	    // register your handler for that vector
	    if (register_int_handler(vec, handler, d)) {
		ERROR("failed to register int handler\n");
		return -1;
		// failed....
	    }
	    // set the table entry to point to your handler
	    if (pci_dev_set_msi_x_entry(p,i,vec,0)) {
		ERROR("failed to set MSI-X entry\n");
		return -1;
	    }
	    // and unmask it (device is still masked)
	    if (pci_dev_unmask_msi_x_entry(p,i)) {
		ERROR("failed to unmask entry\n");
		return -1;
	    }
	    DEBUG("finished setting up entry %d for vector %u on cpu 0\n",i,vec);
	}
	
	// unmask entire function
	if (pci_dev_unmask_msi_x_all(p)) {
	    ERROR("failed to unmask device\n");
	    return -1;
	}
	
    } else {
	
	DEBUG("setting up interrupts via legacy path at 0x%x\n",HACKED_LEGACY_VECTOR);
	INFO("THIS HACKED LEGACY INTERRUPT SETUP IS PROBABLY NOT WHAT YOU WANT\n");

        if (register_int_handler(HACKED_LEGACY_VECTOR, handler, d)) {
            ERROR("Failed to register int handler\n");
            return -1;
        }
	
	nk_unmask_irq(HACKED_LEGACY_IRQ);
	
	// for (i=0; i<256; i++) { nk_umask_irq(i); }
	
        // enable interrupts in PCI space
        uint16_t cmd = pci_dev_cfg_readw(p,0x4);
        cmd &= ~0x0400;
        pci_dev_cfg_writew(p,0x4,cmd);
	
    }
    
    DEBUG("device inited\n");
    
    /*************************************************
     ********************* TEST **********************
     *************************************************/
    // DEBUG("******* Running read_blocks or write_blocks *******\n");
    // test_read(d);
    // test_write(d);
    
    // DEBUG("******* Running descriptor chain test *******\n");
    // struct virtio_pci_virtq *virtq = &dev->virtq[0];
    // struct virtq *vq = &virtq->vq;
    // uint64_t qsz = vq->num;
    // for (i = 0; i < qsz; i++) {
    //   DEBUG("Next pointer of descriptor %d is %d\n", i, vq->desc[i].next);
    // }
   
    return 0;
}
