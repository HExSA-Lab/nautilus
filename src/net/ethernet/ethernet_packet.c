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
 * Copyright (c) 2018, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/list.h>
#include <nautilus/spinlock.h>
#include <nautilus/netdev.h>
#include <net/ethernet/ethernet_packet.h>

// right now this is a trivial implementation that just has a single pool
// in the future, this could expand to a per-cpu pool


// not currently a Kconfig option
#define NAUT_CONFIG_NET_ETHERNET_INIT_POOL_SIZE 256

#define NAUT_CONFIG_NET_ETHERNET_INIT_POOL_SIZE 256

#define POOL_INIT_START  (NAUT_CONFIG_NET_ETHERNET_INIT_POOL_SIZE)
#define POOL_INIT_LOW    (POOL_INIT_START/2)
#define POOL_INIT_HIGH   (POOL_INIT_LOW*3)

#ifndef NAUT_CONFIG_DEBUG_NET_ETHERNET_PACKET
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("ethernet_packet: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("ethernet_packet: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("ethernet_packet: " fmt, ##args)

static spinlock_t       lock;
static struct list_head free_list;
// the invariant here is that high=2*low
static uint64_t         free_list_len, free_list_low, free_list_high;

// called with lock held
static void try_grow_if_needed()
{
    if (free_list_len<free_list_low) {
	// We will set a new midpoint at double the old midpoint
	// and we will double the upper and lower control thresholds
	uint64_t needed = ((2*free_list_high-2*free_list_low)/2)-free_list_len;
	uint64_t i;

	for (i=0;i<needed;i++) {
	    nk_ethernet_packet_t *p = malloc(sizeof(nk_ethernet_packet_t));
	    if (!p) {
		break;
	    }
	    INIT_LIST_HEAD(&p->node);
	    list_add_tail(&p->node,&free_list);
	    p->alloc_cpu = -1;
	    p->refcount = 0;
	    free_list_len++;
	}

	// we may not be able to allocate that many...
	// i now contains the number we actually allocated, so now update
	// the control thresholds to match

	free_list_low = free_list_len/2;
	free_list_high = free_list_len*2;
    }
}
	
nk_ethernet_packet_t *nk_net_ethernet_alloc_packet(int cpu)
{
    struct list_head *cur=0;
    nk_ethernet_packet_t *p=0;
    int first=1;

 retry:
    spin_lock(&lock);
    if (!list_empty(&free_list)) {
	cur = free_list.next;
	list_del_init(cur);
	free_list_len--;
    }
    try_grow_if_needed();
    spin_unlock(&lock);
    
    if (!cur) {
	if (first) {
	    first=0;
	    goto retry;
	} else {
	    ERROR("Failed to grow packet pool?!\n");
	    p = (nk_ethernet_packet_t *)malloc_specific(sizeof(nk_ethernet_packet_t),cpu);
	}
    } else {
	p = list_entry(cur,nk_ethernet_packet_t, node);
    }

    if (!p) {
	ERROR("Failed to allocate packet!\n");
	return p;
    }

    INIT_LIST_HEAD(&p->node);
    p->refcount = 1;

    return p;
    
}

void nk_net_ethernet_acquire_packet(nk_ethernet_packet_t *p)
{
    __sync_fetch_and_add(&p->refcount,1);
}

void nk_net_ethernet_release_packet(nk_ethernet_packet_t *p)
{
    if (__sync_fetch_and_sub(&p->refcount,1)==1) {
	// the packet is now ready to be freed
	spin_lock(&lock);
	if (free_list_len<free_list_high) {
	    // add it to the front of the list since it's probably all in
	    // cache now, and so the next allocator will be able to take advantage
	    list_add(&p->node,&free_list);
	    free_list_len++;
	    spin_unlock(&lock);
	} else {
	    spin_unlock(&lock);
	    // just free it - we already have too many packets
	    free(p);
	    // we would possibly shrink the pool here...
	}
    }
}
	




int  nk_net_ethernet_packet_init()
{
    uint64_t i;

    spinlock_init(&lock);
    INIT_LIST_HEAD(&free_list);

    free_list_len = 0;
    free_list_low = POOL_INIT_LOW;
    free_list_high = POOL_INIT_HIGH;

    try_grow_if_needed();
    
    INFO("inited and seeded with %lu packets of size %lu (low=%lu, high=%lu)\n",free_list_len, MAX_ETHERNET_PACKET_LEN, free_list_low, free_list_high);

    return 0;
}

void nk_net_ethernet_packet_deinit()
{
    spin_lock(&lock);
    struct list_head *cur, *tmp;

    list_for_each_safe(cur,tmp,&free_list) {
	nk_ethernet_packet_t *p= list_entry(cur,nk_ethernet_packet_t,node);
	list_del_init(cur);
	free(p);
    }
    INFO("deinited\n");
}


