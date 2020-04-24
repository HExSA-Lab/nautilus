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
 * Copyright (c) 2018, Nathan Lindquist
 * Copyright (c) 2018, Clay Kauzlaric
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Nathan Lindquist <nallink01@gmail.com>
 *          Clay Kauzlaric <clay.kauzlaric@yahoo.com>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/netdev.h>
#include <nautilus/irq.h>
#include <nautilus/backtrace.h>

#include <dev/pci.h>
#include <dev/virtio_net.h>

#ifndef NAUT_CONFIG_DEBUG_VIRTIO_NET
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define INFO(fmt, args...) INFO_PRINT("virtio_net: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("virtio_net: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("virtio_net: " fmt, ##args)

#define FBIT_ISSET(features, bit) ((features) & (0x01 << (bit)))
#define FBIT_SETIF(features_out, features_in, bit) if (FBIT_ISSET(features_in,bit)) { features_out |= (0x01 << (bit)) ; }
#define DEBUG_FBIT(features, bit) if (FBIT_ISSET(features, bit)) {\
                                      DEBUG("feature bit set: %s\n", #bit);\
                                  }

// constants

#define HACKED_LEGACY_VECTOR 0xe4
#define HACKED_LEGACY_IRQ    (HACKED_LEGACY_VECTOR - 32)

#define MIN_TU 48
#define MAX_TU 1522

// virtqueue indices
#define VIRTIO_NET_RECVQ_IDX  0
#define VIRTIO_NET_SENDQ_IDX  1
#define VIRTIO_NET_CTRLQ_IDX  2

// legacy register offsets
#define VIRTIO_NET_OFF_MAC(v)     (virtio_pci_device_regs_start_legacy(v) + 0)
#define VIRTIO_NET_OFF_STATUS(v)  (virtio_pci_device_regs_start_legacy(v) + 6)

// feature bits

// device handles packets with partial checksum
#define VIRTIO_NET_F_CSUM                 0

// guest handles packets with partial checksum
#define VIRTIO_NET_F_GUEST_CSUM           1

// control channel offloads reconfiguration support
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS  2

// device has the MAC address given in config space
#define VIRTIO_NET_F_MAC                  5

// device handles packets with any GSO type (legacy, deprecated)
#define VIRTIO_NET_F_GSO                  6

// guest can receive TSOv4
#define VIRTIO_NET_F_GUEST_TSO4           7

// guest can receive TSOv6
#define VIRTIO_NET_F_GUEST_TSO6           8

// guest can receive TSO with ECN
#define VIRTIO_NET_F_GUEST_ECN            9

// guest can receive UFO
#define VIRTIO_NET_F_GUEST_UFO            10

// device can receive TSOv4
#define VIRTIO_NET_F_HOST_TSO4            11

// device can receive TSOv6
#define VIRTIO_NET_F_HOST_TSO6            12

// device can receive TSO with ECN
#define VIRTIO_NET_F_HOST_ECN             13

// device can receive UFO
#define VIRTIO_NET_F_HOST_UFO             14

// guest can merge receive buffers
#define VIRTIO_NET_F_MRG_RXBUF            15

// configuration status field is available
#define VIRTIO_NET_F_STATUS               16

// control virtqueue is available
#define VIRTIO_NET_F_CTRL_VQ              17

// control channel RX mode supported
#define VIRTIO_NET_F_CTRL_RX              18

// control channel VLAN filtering
#define VIRTIO_NET_F_CTRL_VLAN            19

// guest can send gratuitous packets
#define VIRTIO_NET_F_GUEST_ANNOUNCE       21

// device supports multiqueue
#define VIRTIO_NET_F_MQ                   22

// set MAC address through control channel
#define VIRTIO_NET_F_CTRL_MAC_ADDR        23


// device data types

struct virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __packed;


// our state

static uint64_t num_devs=0;

struct callback_info {
    void *context;
    void (*callback)(nk_net_dev_status_t status, void *context);
};

struct virtio_net_dev {
    struct nk_net_dev     *net_dev;
    struct virtio_pci_dev *virtio_dev;

    uint8_t mac[ETHER_MAC_LEN];
    struct callback_info *callbacks[2];
};


// interface to kernel

static uint64_t packet_size_to_buffer_size(uint64_t sz)
{
    return sz;
}

static int get_characteristics(void *state, struct nk_net_dev_characteristics *c)
{
    if (!state) {
        ERROR("state is null\n");
        return -1;
    }

    if (!c) {
        ERROR("characteristics pointer is null\n");
        return -1;
    }

    struct virtio_net_dev *d = (struct virtio_net_dev *) state;

    memcpy(c->mac, d->mac, ETHER_MAC_LEN);

    c->min_tu = MIN_TU;
    c->max_tu = MAX_TU;
    c->packet_size_to_buffer_size = packet_size_to_buffer_size;

    return 0;
}

static int post(void *state, uint8_t *buf, uint64_t len, void (*callback)(nk_net_dev_status_t status, void *context), void *context, int send)
{
    uint16_t qidx = send ? VIRTIO_NET_SENDQ_IDX : VIRTIO_NET_RECVQ_IDX;
    struct virtio_net_dev *d = (struct virtio_net_dev *) state;
    struct virtq *vq = &d->virtio_dev->virtq[qidx].vq;

    // alloc descriptor for header
    uint16_t header_idx;
    if (virtio_pci_desc_alloc(d->virtio_dev, qidx, &header_idx)) {
        ERROR("descriptor alloc failed\n");
        return -1;
    }
    DEBUG("allocated descriptor %d\n", header_idx);

    // alloc descriptor for packet
    uint16_t packet_idx;
    if (virtio_pci_desc_alloc(d->virtio_dev, qidx, &packet_idx)) {
        ERROR("descriptor alloc failed\n");
        virtio_pci_desc_free(d->virtio_dev, qidx, header_idx);
        return -1;
    }
    DEBUG("allocated descriptor %d\n", packet_idx);

    // create buffer for header
    uint8_t *header = malloc(sizeof(struct virtio_net_hdr));
    if (!header) {
        virtio_pci_desc_free(d->virtio_dev, qidx, header_idx);
        virtio_pci_desc_free(d->virtio_dev, qidx, packet_idx);
        ERROR("couldn't allocate buffer for virtio-net header\n");
        return -1;
    }
    memset(header, 0, sizeof(struct virtio_net_hdr));
    DEBUG("malloc header %x\n", (uint64_t) header);

    // setup header descriptor
    struct virtq_desc *header_desc = &vq->desc[header_idx];
    header_desc->addr = (uint64_t) header;
    header_desc->len = sizeof(struct virtio_net_hdr);
    header_desc->flags = VIRTQ_DESC_F_NEXT;
    if (!send) {
        header_desc->flags |= VIRTQ_DESC_F_WRITE;
    }
    header_desc->next = packet_idx;

    // setup packet descriptor
    struct virtq_desc *packet_desc = &vq->desc[packet_idx];
    packet_desc->addr = (uint64_t) buf;
    packet_desc->len = len;
    if (!send) {
        packet_desc->flags = VIRTQ_DESC_F_WRITE;
    }
    packet_desc->next = 0;

    // stash the callback and context
    d->callbacks[qidx][header_idx].callback = callback;
    d->callbacks[qidx][header_idx].context = context;
    
    // put header descriptor in virtq
    vq->avail->ring[vq->avail->idx % vq->qsz] = header_idx;
    mbarrier();
    vq->avail->idx++;
    mbarrier();

    // notify device (needs to be turned off for recv?)
    virtio_pci_write_regw(d->virtio_dev, QUEUE_NOTIFY, qidx);

    return 0;
}

static int post_receive(void *state, uint8_t *dest, uint64_t len, void (*callback)(nk_net_dev_status_t status, void *context), void *context)
{
    DEBUG("post_receive\n");

    if (post(state, dest, len, callback, context, 0)) {
        return -1;
    }
    
    return 0;
}

static int post_send(void *state, uint8_t *src, uint64_t len, void (*callback)(nk_net_dev_status_t status, void *context), void *context)
{
    DEBUG("post_send\n");

    if (post(state, src, len, callback, context, 1)) {
        return -1;
    }

    return 0;
}

static struct nk_net_dev_int ops =  {
    .get_characteristics = get_characteristics,
    .post_receive = post_receive,
    .post_send = post_send,
};


// interrupt handling

static int process_used_ring(struct virtio_net_dev *d, int qidx)
{
    uint16_t curr_idx, desc_idx, len;
    uint32_t i;
    uint8_t *header;
    struct virtq_desc *head, *body;
    struct virtio_pci_virtq *virtq = &d->virtio_dev->virtq[qidx];

    mbarrier();

    DEBUG("processing used ring for virtq %d\n", qidx);
    DEBUG("used idx = %d\n", virtq->vq.used->idx);
    DEBUG("last seen used = %d\n", virtq->last_seen_used);

    for (; virtq->last_seen_used != virtq->vq.used->idx; virtq->last_seen_used++) {
        curr_idx = virtq->last_seen_used % virtq->vq.qsz;
        desc_idx = (uint16_t) virtq->vq.used->ring[curr_idx].id;
        len = (uint16_t) virtq->vq.used->ring[curr_idx].len;

        head = &virtq->vq.desc[desc_idx];
        header = (uint8_t *) head->addr;
        if (!(head->flags & VIRTQ_DESC_F_NEXT)) {
            ERROR("head in used ring does not have next flag\n");
            return -1;
        }
        body = &virtq->vq.desc[head->next];

        DEBUG("head = %d\n", desc_idx);
        DEBUG("body = %d\n", head->next);
        DEBUG("len = %d\n", len);
#ifdef NAUT_CONFIG_DEBUG_VIRTIO_NET
        //nk_dump_mem((uint8_t *) body->addr, len - sizeof(struct virtio_net_hdr));
#endif
        // free the header buffer (this is the only buffer we allocated)
        free(header);

	// grab the callback info
        void (*callback)(nk_net_dev_status_t, void *) = d->callbacks[qidx][desc_idx].callback;
        void *context = d->callbacks[qidx][desc_idx].context;

        memset(&d->callbacks[qidx][desc_idx], 0, sizeof(struct callback_info));

        // free the descriptor chain
        if (virtio_pci_desc_chain_free(d->virtio_dev, qidx, desc_idx)) {
            ERROR("error freeing descriptors\n");
            return -1;
        }

        // call the corresponding callback
        if (callback) {
            callback(NK_NET_DEV_STATUS_SUCCESS, context);
        }
    }

    return 0;
}

static int handler(excp_entry_t *exp, excp_vec_t vec, void *priv_data)
{
    int rc = 0;
    
    DEBUG("interrupt\n");

    struct virtio_net_dev *d = (struct virtio_net_dev *) priv_data;

    if (d->virtio_dev->itype == VIRTIO_PCI_LEGACY_INTERRUPT) {
        // read ISR status field
        uint8_t isr = virtio_pci_read_regb(d->virtio_dev, ISR_STATUS);

        // if bit 0 not set, ignore
        if (!(isr & 0x01)) {
            DEBUG("interrupt not for me\n");
            IRQ_HANDLER_END();
            return 0;
        }

        // need to check bit 1 for config change
    }

    // scan used rings
    if (process_used_ring(d, VIRTIO_NET_RECVQ_IDX)) {
        ERROR("error processing used ring for recvq\n");
	rc = -1;
    }
    if (process_used_ring(d, VIRTIO_NET_SENDQ_IDX)) {
        ERROR("error processing used ring for sendq\n");
	rc = -1;
    }

    DEBUG("interrupt done\n");
    IRQ_HANDLER_END();
    return rc;
}


// initialization code

void teardown(struct virtio_pci_dev *dev)
{
    // reset device?
    // actually do frees..
    virtio_pci_virtqueue_deinit(dev);
}

static uint64_t select_features(uint64_t features)
{
    DEBUG("device features: 0x%0lx\n",features);
    DEBUG_FBIT(features, VIRTIO_NET_F_CSUM);
    DEBUG_FBIT(features, VIRTIO_NET_F_GUEST_CSUM);
    DEBUG_FBIT(features, VIRTIO_NET_F_CTRL_GUEST_OFFLOADS);
    DEBUG_FBIT(features, VIRTIO_NET_F_MAC);
    DEBUG_FBIT(features, VIRTIO_NET_F_GSO);
    DEBUG_FBIT(features, VIRTIO_NET_F_GUEST_TSO4);
    DEBUG_FBIT(features, VIRTIO_NET_F_GUEST_TSO6);
    DEBUG_FBIT(features, VIRTIO_NET_F_GUEST_ECN);
    DEBUG_FBIT(features, VIRTIO_NET_F_GUEST_UFO);
    DEBUG_FBIT(features, VIRTIO_NET_F_HOST_TSO4);
    DEBUG_FBIT(features, VIRTIO_NET_F_HOST_TSO6);
    DEBUG_FBIT(features, VIRTIO_NET_F_HOST_ECN);
    DEBUG_FBIT(features, VIRTIO_NET_F_HOST_UFO);
    DEBUG_FBIT(features, VIRTIO_NET_F_MRG_RXBUF);
    DEBUG_FBIT(features, VIRTIO_NET_F_STATUS);
    DEBUG_FBIT(features, VIRTIO_NET_F_CTRL_VQ);
    DEBUG_FBIT(features, VIRTIO_NET_F_CTRL_RX);
    DEBUG_FBIT(features, VIRTIO_NET_F_CTRL_VLAN);
    DEBUG_FBIT(features, VIRTIO_NET_F_GUEST_ANNOUNCE);
    DEBUG_FBIT(features, VIRTIO_NET_F_MQ);
    DEBUG_FBIT(features, VIRTIO_NET_F_CTRL_MAC_ADDR);
    DEBUG_FBIT(features, VIRTIO_F_NOTIFY_ON_EMPTY);
    DEBUG_FBIT(features, VIRTIO_F_ANY_LAYOUT);
    DEBUG_FBIT(features, VIRTIO_F_INDIRECT_DESC);
    DEBUG_FBIT(features, VIRTIO_F_EVENT_IDX);
    //DEBUG_FBIT(features, VIRTIO_F_VERSION_1);

    // choose accepted features
    uint64_t accepted = 0;

    FBIT_SETIF(accepted,features,VIRTIO_NET_F_MAC);

    DEBUG("features accepted: 0x%0lx\n", accepted);

    return accepted;
}

static void test_callback(nk_net_dev_status_t status, void *context)
{
    DEBUG("callback called: status=%d, context=%s\n", status, (const char *) context);
}

static int test_send(struct virtio_net_dev *d)
{
    // this leaks but who cares - it is not used normally

    // allocate buffer for packet
    uint32_t packetlen = 18 + 46;
    uint8_t *packet = malloc(packetlen);
    DEBUG("malloc send buffer %x\n", (uint64_t) packet);
    memset(packet, 0, packetlen);

    // create packet
    packet[0] = 0xff;
    packet[1] = 0xff;
    packet[2] = 0xff;
    packet[3] = 0xff;
    packet[4] = 0xff;
    packet[5] = 0xff;

    int i;
    for (i = 0; i < ETHER_MAC_LEN; i++) {
        packet[i+6] = d->mac[i];
    }

    *((uint16_t *) &packet[12]) = 46;

    for (i = 0; i < 46; i++) {
        packet[i+14] = i;
    }

    for (i = 0; i < 4; i++) {
        packet[i+60] = 0;
    }

    // send packet
    if (post_send(d, packet, packetlen, test_callback, "test_send")) {
        ERROR("couldn't send test packet\n");
        return -1;
    }

    DEBUG("packet launch started\n");

    return 0;
}

static int test_recv(struct virtio_net_dev *d)
{
    // this leaks but who cares

    // allocate buffer for packet
    uint32_t packetlen = 2048;
    uint8_t *packet = malloc(packetlen);
    DEBUG("malloc recv buffer %x\n", (uint64_t) packet);
    memset(packet, 0, packetlen);

    if (post_receive(d, packet, packetlen, test_callback, "test_recv")) {
        ERROR("couldn't set up recv buffer\n");
        return -1;
    }

    DEBUG("packet recv started\n");

    return 0;
}

static int check_int(struct virtio_net_dev *d)
{
    DEBUG("checking interrupts...\n");

    struct virtio_pci_dev *vdev = d->virtio_dev;
    struct pci_dev *p = vdev->pci_dev;

    // check MSI-X status
    uint16_t n = p->msix.size;
    uint16_t i;
    int pending;

    for (i = 0; i < n; i++) {
        pending = pci_dev_is_pending_msi_x(p,i);
        if (pending < 0) {
            DEBUG("error reading int status for slot %d\n", i);
        }
        else {
            DEBUG("slot %d %s\n", i, pending ? "pending" : "not pending");
        }
    }

    // check MSI-X vectors on device
    for (i = 0; i < n; i++) {
        virtio_pci_write_regw(vdev,QUEUE_SEL,i);
        uint16_t queue_vec = virtio_pci_read_regw(vdev,QUEUE_VEC);
        DEBUG("queue vec for %d = 0x%x\n",i,queue_vec);
    }
    uint16_t config_vec = virtio_pci_read_regw(vdev,CONFIG_VEC);
    DEBUG("config vec = 0x%x\n", config_vec);

    // check registers
    uint16_t status = pci_dev_cfg_readw(p,0x6);
    uint16_t cmd = pci_dev_cfg_readw(p,0x4);
    DEBUG("status snapshot = 0x%x\n",p->cfg.status);
    DEBUG("status = 0x%x\n",status);
    DEBUG("cmd = 0x%x\n",cmd);

    return 0;
}

int virtio_net_init(struct virtio_pci_dev *dev)
{
    char buf[DEV_NAME_LEN];

    if (!dev->model==VIRTIO_PCI_LEGACY_MODEL) {
	ERROR("currently only supported with legacy model\n");
	return -1;
    }
    
    DEBUG("init device\n");

    // allocate memory for state
    struct virtio_net_dev *d = malloc(sizeof(*d));
    if (!d) {
        ERROR("can't allocate state\n");
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

    // create virtqueues for device
    if (virtio_pci_virtqueue_init(dev)) {
        ERROR("Failed to initialize virtqueues\n");
        free(d);
        return -1;
    }

    // allocate memory for callbacks (this memory leaks, needs to be freed)
    uint32_t send_callbacks_size = sizeof(struct callback_info) * dev->virtq[VIRTIO_NET_SENDQ_IDX].vq.qsz;
    uint32_t recv_callbacks_size = sizeof(struct callback_info) * dev->virtq[VIRTIO_NET_RECVQ_IDX].vq.qsz;

    d->callbacks[VIRTIO_NET_SENDQ_IDX] = malloc(send_callbacks_size);
    if (!d->callbacks[VIRTIO_NET_SENDQ_IDX]) {
        ERROR("can't allocate send callbacks\n");
	virtio_pci_virtqueue_deinit(dev);
        free(d);
        return -1;
    }

    d->callbacks[VIRTIO_NET_RECVQ_IDX] = malloc(recv_callbacks_size);
    if (!d->callbacks[VIRTIO_NET_RECVQ_IDX]) {
        ERROR("can't allocate recv callbacks\n");
	virtio_pci_virtqueue_deinit(dev);
        free(d->callbacks[VIRTIO_NET_SENDQ_IDX]);
        free(d);
        return -1;
    }

    memset(d->callbacks[VIRTIO_NET_SENDQ_IDX], 0, send_callbacks_size);
    memset(d->callbacks[VIRTIO_NET_RECVQ_IDX], 0, recv_callbacks_size);

    // fill out pci dev state
    dev->state = d;
    dev->teardown = teardown;

    d->virtio_dev = dev;

    // register net dev
    snprintf(buf,DEV_NAME_LEN,"virtio-net%u",__sync_fetch_and_add(&num_devs,1));
    d->net_dev = nk_net_dev_register(buf,0,&ops,d);

    if (!d->net_dev) {
        ERROR("Failed to register network device\n");
        virtio_pci_virtqueue_deinit(dev);
	free(d->callbacks[VIRTIO_NET_RECVQ_IDX]);
	free(d->callbacks[VIRTIO_NET_SENDQ_IDX]);
        free(d);
        return -1;
    }

    // We assume that interrupt allocations will not fail...
    // if we do fail, the rest of this code will leak

    struct pci_dev *p = dev->pci_dev;
    uint16_t num_vec = p->msix.size;
    ulong_t vec;
    uint16_t i;

    // now set up interrupts
    if (dev->itype==VIRTIO_PCI_MSI_X_INTERRUPT) {
        // we assume MSI-X has been enabled on the device
        // already, that virtqueue setup is done, and
        // that queue i has been mapped to MSI-X table entry i
        // MSI-X is on but whole function is masked

        DEBUG("setting up interrupts via MSI-X\n");

        if (dev->num_virtqs != num_vec) {
            ERROR("weird mismatch: numqueues=%u msixsize=%u\n",
                dev->num_virtqs, p->msix.size);
            //continue for now...
	    //return -1;
	    
        }

        // now fill out the device's MSI-X table
        for (i=0;i<num_vec;i++) {
            // find a free vector
            // note that prioritization here is your problem
            if (idt_find_and_reserve_range(1,0,&vec)) {
                ERROR("Cannot get vector...\n");
                return -1;
            }
            // register your handler for that vector
            if (register_int_handler(vec, handler, d)) {
                ERROR("Failed to register int handler\n");
                return -1;
                // failed....
            }
            // set the table entry to point to your handler
            if (pci_dev_set_msi_x_entry(p,i,vec,0)) {
                ERROR("Failed to set MSI-X entry\n");
                return -1;
            }
            // and unmask it (device is still masked)
            if (pci_dev_unmask_msi_x_entry(p,i)) {
                ERROR("Failed to unmask entry\n");
                return -1;
            }
            DEBUG("Finished setting up entry %d for vector %u on cpu 0\n",i,vec);
        }

        // unmask entire function
        if (pci_dev_unmask_msi_x_all(p)) {
            ERROR("Failed to unmask device\n");
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

    // get MAC address
    if (FBIT_ISSET(dev->feat_accepted, VIRTIO_NET_F_MAC)) {
        for (i = 0; i < ETHER_MAC_LEN; i++) {
            d->mac[i] = virtio_pci_read_regb(dev,VIRTIO_NET_OFF_MAC(dev)+i);
        }
    }
    
    DEBUG("device mac address: %x:%x:%x:%x:%x:%x\n",
        d->mac[0], d->mac[1], d->mac[2],
        d->mac[3], d->mac[4], d->mac[5]);

    // start device
    if (virtio_pci_start_device(dev)) {
        ERROR("Failed to start device\n");
        return -1;
    }

    // now try to poke device
    // test_send(d);
    // for (i = 0; i < 128; i++) {
    //     test_recv(d);
    // }
    // check_int(d);

    DEBUG("device inited\n");    

    return 0;
}
