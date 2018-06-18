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
#include <nautilus/random.h>
#include <nautilus/list.h>
#include <nautilus/spinlock.h>
#include <nautilus/netdev.h>

#include <net/ethernet/ethernet_arp.h>

#ifndef NAUT_CONFIG_DEBUG_NET_ETHERNET_ARP
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("ethernet_arp: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("ethernet_arp: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("ethernet_arp: " fmt, ##args)

#define ARPER_LIST_LOCK_CONF uint8_t _arper_list_lock_flags
#define ARPER_LIST_LOCK() _arper_list_lock_flags = spin_lock_irq_save(&arper_list_lock)
#define ARPER_LIST_UNLOCK() spin_unlock_irq_restore(&arper_list_lock, _arper_list_lock_flags)

#define ARP_CACHE_LOCK_CONF uint8_t _arp_cache_lock_flags
#define ARP_CACHE_LOCK() _arp_cache_lock_flags = spin_lock_irq_save(&arp_cache_lock)
#define ARP_CACHE_UNLOCK() spin_unlock_irq_restore(&arp_cache_lock, _arp_cache_lock_flags)

#define ARPER_LOCK_CONF uint8_t _arper_lock_flags
#define ARPER_LOCK(a) _arper_lock_flags = spin_lock_irq_save(&a->lock)
#define ARPER_UNLOCK(a) spin_unlock_irq_restore(&a->lock, _arper_lock_flags)

static spinlock_t arper_list_lock;
static struct list_head  arper_list;

static spinlock_t arp_cache_lock;

struct addr_pair {
    uint64_t            time_ns; // when this entry was made (for the ARP cache)
    int                 state;
#define FREE    0
#define ALLOCED 1
#define WAITING 2       // for ARP cache, outstanding request
#define INUSE   3
    ipv4_addr_t         ip_addr;
    ethernet_mac_addr_t mac_addr;
};

// number of entries in the ARP cache
#define MAX_ARP_CACHE_PAIRS 256

// number of entries per interface (for multiple addresses)
#define MAX_PAIRS 16


#define ARP_ENTRY_TIMEOUT_S (14400ULL)
#define ARP_ENTRY_TIMEOUT_NS (ARP_ENTRY_TIMEOUT_S * 1000000000ULL)


struct nk_net_ethernet_arper {
    spinlock_t        lock;
    struct list_head  node;     // for inclusion in arper list

    struct nk_net_dev *netdev;  // the filtered network device

    struct addr_pair  pairs[MAX_PAIRS];
};


// lock must be held
static struct addr_pair *find_pair_by_mac_addr(struct nk_net_ethernet_arper *arper, ethernet_mac_addr_t mac)
{
    int i;

    for (i=0;i<MAX_PAIRS;i++) {
	if (arper->pairs[i].state==INUSE && !memcmp(&arper->pairs[i].mac_addr,mac,ETHER_MAC_LEN)) {
	    return &arper->pairs[i];
	}
    }
    return 0;
}

// lock must be held
static struct addr_pair *find_pair_by_ip_addr(struct nk_net_ethernet_arper *arper, ipv4_addr_t ip)
{
    int i;

    for (i=0;i<MAX_PAIRS;i++) {
	if (arper->pairs[i].state==INUSE && arper->pairs[i].ip_addr==ip) {
	    return &arper->pairs[i];
	}
    }
    return 0;
}

// lock must be held
static struct addr_pair *alloc_pair(struct nk_net_ethernet_arper *arper)
{
    int i;

    for (i=0;i<MAX_PAIRS;i++) {
	if (arper->pairs[i].state==FREE) {
	    arper->pairs[i].state=ALLOCED;
	    return &arper->pairs[i];
	}
    }
    return 0;
}

// lock must be held
static void free_pair(struct addr_pair *pair)
{
    pair->state=FREE;
}


#define IPSTR "%u.%u.%u.%u"
#define IPLIST(x) ((x)>>24)&0xff, ((x)>>16)&0xff, ((x)>>8)&0xff, (x)&0xff

#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MACLIST(x) (x)[0],(x)[1],(x)[2],(x)[3],(x)[4],(x)[5]

static struct addr_pair *find_waiting_pair_in_cache_by_ip_addr(ipv4_addr_t ip);


void recv_callback(nk_net_dev_status_t status,
		   nk_ethernet_packet_t *packet,
		   void *state)
{
    struct nk_net_ethernet_arper *a = (struct nk_net_ethernet_arper *)state;

    DEBUG("Receive Callback\n");
    
    if (status) {
	ERROR("Bad packet receive - reissuing a receive\n");
	goto launch_receive;
    }


    nk_net_ethernet_arp_reqresp_t *r = (nk_net_ethernet_arp_reqresp_t *)packet->data;

    if (r->htype!=htons(1) || r->ptype!=htons(0x0800)) {
	DEBUG("Unsupported ARP (htype=%x ptype=%x)\n",r->htype,r->ptype);
	goto launch_receive;
    }

    if (r->oper==htons(1)) {  // ARP REQUEST
	struct addr_pair *p;
	
	ARPER_LOCK_CONF;
	
	ARPER_LOCK(a);
	
	p = find_pair_by_ip_addr(a,ntohl(r->tpa));

	ARPER_UNLOCK(a);
	
	if (!p) {
	    DEBUG("ARP reguest for " IPSTR " is ignored\n", IPLIST(ntohl(r->tpa)));
	    goto launch_receive;
	}

	DEBUG("ARP request for " IPSTR " mapped to " MACSTR "\n", IPLIST(p->ip_addr),MACLIST(p->mac_addr));

	struct nk_net_dev_characteristics c;

	nk_net_dev_get_characteristics(a->netdev,&c);

	memcpy(packet->header.dst,r->sha,ETHER_MAC_LEN);
	memcpy(packet->header.src,c.mac,ETHER_MAC_LEN);
	// type remains the same
	
	// now we will convert the arp request to a response
	r->oper=htons(2);  // response
	// target addresses are from the request (copy sender->target)
	memcpy(r->tha,r->sha,ETHER_MAC_LEN);
	r->tpa = r->spa;
	// sender addresses are the response
	memcpy(r->sha,p->mac_addr,ETHER_MAC_LEN); 
	r->spa = htonl(p->ip_addr);

	packet->len = 14 + 28; 

	// and then launch it back (non-blocking)
	if (nk_net_ethernet_agent_device_send_packet(a->netdev,
						     packet,
						     NK_DEV_REQ_NONBLOCKING,
						     0, 0)) {
	    ERROR("Failed to launch ARP response\n");
	    // sender will just have to retry
	}
	// send side will delete packet
	goto launch_receive;
	
    } else if (r->oper==htons(2)) {  // ARP RESPONSE

	struct addr_pair *p;
	
	DEBUG("ARP response for " IPSTR " mapped to " MACSTR "\n", IPLIST(ntohl(r->spa)),MACLIST(r->sha));
	ARP_CACHE_LOCK_CONF;
	
	ARP_CACHE_LOCK();
	
	p = find_waiting_pair_in_cache_by_ip_addr(ntohl(r->spa));

	if (p) {
	    DEBUG("Pair found, current state %d\n",p->state);
	    memcpy(p->mac_addr,r->sha,ETHER_MAC_LEN);
	    p->time_ns = nk_sched_get_realtime();
	    p->state = INUSE;
	} else {
	    DEBUG("Pair not found, avoiding cache update\n");
	}

	ARP_CACHE_UNLOCK();
	
	goto launch_receive;
    } else {
	DEBUG("Unknown ARP operation\n");
	goto launch_receive;
    }



 launch_receive:
    // now we issue a receive to get the ball rolling
    if (nk_net_ethernet_agent_device_receive_packet(a->netdev,
						    0,
						    NK_DEV_REQ_CALLBACK,
						    recv_callback,
						    a)) {
	ERROR("Failed to launch recurring receive - we are probably dead now\n");
    }

 
	

}


struct nk_net_ethernet_arper * nk_net_ethernet_arper_create(struct nk_net_ethernet_agent *agent)
{
    struct nk_net_ethernet_arper *a = malloc(sizeof(*a));

    if (!a) {
	DEBUG("Cannot allocate space for arper\n");
	return 0;
    }

    memset(a,0,sizeof(*a));

    INIT_LIST_HEAD(&a->node);
    spinlock_init(&a->lock);

    a->netdev = nk_net_ethernet_agent_register_type(agent,0x0806); // ARP type

    if (!a->netdev) {
	ERROR("Failed to register for ARP type\n");
	free(a);
	return 0;
    }

    ARPER_LIST_LOCK_CONF;
    
    ARPER_LIST_LOCK();
    list_add(&a->node,&arper_list);
    ARPER_LIST_UNLOCK();

    // now we issue a receive to get the ball rolling
    if (nk_net_ethernet_agent_device_receive_packet(a->netdev,
						    0,
						    NK_DEV_REQ_CALLBACK,
						    recv_callback,
						    a)) {
	ERROR("Failed to launch initial receive\n");
	free(a);
	return 0;
    }


    return a;
}


int                            nk_net_ethernet_arper_destroy(struct nk_net_ethernet_arper *arper)
{
    ARPER_LIST_LOCK_CONF;

    ARPER_LIST_LOCK();
    list_del_init(&arper->node);
    ARPER_LIST_UNLOCK();

    nk_net_ethernet_agent_unregister(arper->netdev);

    free(arper);

    return 0;
    
}

int nk_net_ethernet_arper_add(struct nk_net_ethernet_arper *a, ipv4_addr_t ip_addr, ethernet_mac_addr_t mac_addr)
{
    ARPER_LOCK_CONF;

    ARPER_LOCK(a);
    
    struct addr_pair *p = alloc_pair(a);

    if (!p) {
	ARPER_UNLOCK(a);
	ERROR("Cannot allocate address pair\n");
	return -1;
    }

    p->ip_addr = ip_addr;
    memcpy(p->mac_addr, mac_addr, ETHER_MAC_LEN);

    p->state = INUSE;

    ARPER_UNLOCK(a);

    return 0;
}




int nk_net_ethernet_arper_remove(struct nk_net_ethernet_arper *a, uint32_t ip_addr)
{
    ARPER_LOCK_CONF;

    ARPER_LOCK(a);
    
    struct addr_pair *p = find_pair_by_ip_addr(a,ip_addr);

    if (!p) {
	ARPER_UNLOCK(a);
	ERROR("IP address not found\n");
	return -1;
    }

    memset(p,0,sizeof(*p));

    ARPER_UNLOCK(a);

    return 0;

}


static struct addr_pair arp_cache[MAX_ARP_CACHE_PAIRS];

// lock must be held
static struct addr_pair *find_pair_in_cache_by_mac_addr(ethernet_mac_addr_t mac)
{
    int i;

    for (i=0;i<MAX_ARP_CACHE_PAIRS;i++) {
	if (arp_cache[i].state==INUSE && !memcmp(&arp_cache[i].mac_addr,mac,ETHER_MAC_LEN)) {
	    return &arp_cache[i];
	}
    }
    return 0;
}

// lock must be held
static struct addr_pair *find_oldest_pair_in_cache()
{
    int i;
    struct addr_pair *cur = 0;

    for (i=0;i<MAX_ARP_CACHE_PAIRS;i++) {
	if (arp_cache[i].state==INUSE) {
	    if (!cur) {
		cur = &arp_cache[i];
	    } else {
		if (cur->time_ns < arp_cache[i].time_ns) {
		    cur = &arp_cache[i];
		}
	    }
	}
    }
    return cur;
}

// lock must be held
static struct addr_pair *find_pair_in_cache_by_ip_addr(ipv4_addr_t ip)
{
    int i;

    for (i=0;i<MAX_ARP_CACHE_PAIRS;i++) {
	if (arp_cache[i].state==INUSE && arp_cache[i].ip_addr==ip) {
	    return &arp_cache[i];
	}
    }
    return 0;
}

// lock must be held
static struct addr_pair *find_waiting_pair_in_cache_by_ip_addr(ipv4_addr_t ip)
{
    int i;

    for (i=0;i<MAX_ARP_CACHE_PAIRS;i++) {
	if (arp_cache[i].state==WAITING && arp_cache[i].ip_addr==ip) {
	    return &arp_cache[i];
	}
    }
    return 0;
}

// lock must be held
static struct addr_pair *alloc_pair_in_cache()
{
    int i;

    for (i=0;i<MAX_ARP_CACHE_PAIRS;i++) {
	if (arp_cache[i].state==FREE) {
	    arp_cache[i].state=ALLOCED;
	    return &arp_cache[i];
	}
    }
    return 0;
}

// lock must be held
static void free_pair_in_cache(struct addr_pair *pair)
{
    pair->state=FREE;
}

static void time_out_cache()
{
    int i;

    uint64_t now = nk_sched_get_realtime();

    if (now<ARP_ENTRY_TIMEOUT_NS) {
	return;
    }

    for (i=0;i<MAX_ARP_CACHE_PAIRS;i++) {
	if (arp_cache[i].time_ns<(now - ARP_ENTRY_TIMEOUT_NS)) {
	    arp_cache[i].state=FREE;
	}
    }
}

static struct addr_pair *select_victim()
{
    struct addr_pair *p;

    p = alloc_pair_in_cache();

    if (!p) {
	time_out_cache();
	p = alloc_pair_in_cache();
	if (!p) {
	    p = find_oldest_pair_in_cache();
	    if (!p) { 
		  uint64_t r;
		  nk_get_rand_bytes((uint8_t*)&r,sizeof(r));
		  p = &arp_cache[r % MAX_ARP_CACHE_PAIRS];
	    }
	}
    }

    p->state = ALLOCED;

    return p;
}

int nk_net_ethernet_arp_lookup(struct nk_net_ethernet_arper *arper, ipv4_addr_t ip_addr, ethernet_mac_addr_t mac_addr)
{
    if (!arper) {
	int count=0;
	int rc=0;
	struct list_head *cur;

	ARPER_LIST_LOCK_CONF;
	ARPER_LIST_LOCK();

	list_for_each(cur,&arper_list) {
	    count++;
	    struct nk_net_ethernet_arper *a = list_entry(cur,struct nk_net_ethernet_arper, node);
	    int thisrc;
	    thisrc = nk_net_ethernet_arp_lookup(a,ip_addr,mac_addr);
	    if (!thisrc) {
		rc = 0;
		goto out;
	    } else if (thisrc<0) {
		rc = -1;
		goto out;
	    } else {
		rc|=thisrc;
	    }
	}

    out:
	ARPER_LIST_UNLOCK();
	
	return count==0 ? -1 : rc;
    }
	

    uint64_t now = nk_sched_get_realtime();
    
    ARP_CACHE_LOCK_CONF;
    
    ARP_CACHE_LOCK();

    struct addr_pair *p = find_pair_in_cache_by_ip_addr(ip_addr);

    
    if (p) {
	if (now <= ARP_ENTRY_TIMEOUT_NS || (p->time_ns > (now - ARP_ENTRY_TIMEOUT_NS))) {
	    // we have an entry and it is new enough
	    memcpy(mac_addr,p->mac_addr,ETHER_MAC_LEN);
	    ARP_CACHE_UNLOCK();
	    DEBUG("ARP request found in cache\n");
	    return 0;
	} else {
	    DEBUG("ARP request found in cache but is too old\n");
	    // continue to launch packet
	}
    } else {
	DEBUG("ARP request not found in cache - selecting victim\n");
	p = select_victim();
    }

    p->ip_addr = ip_addr;
    p->state = WAITING;
    
    ARP_CACHE_UNLOCK();

    // We now need to launch a request

    
    nk_ethernet_packet_t *pk = nk_net_ethernet_alloc_packet(-1);
    
    if (!pk) {
	ERROR("Failed to allocate packet\n");
	return -1;
    }

    memset(pk->header.dst,0xff,ETHER_MAC_LEN);

    struct nk_net_dev_characteristics c;
    
    nk_net_dev_get_characteristics(arper->netdev,&c);

    ARPER_LOCK_CONF;

    ARPER_LOCK(arper);
    struct addr_pair *my_pair = find_pair_by_mac_addr(arper,c.mac);
    ipv4_addr_t my_addr = my_pair ? my_pair->ip_addr : 0;
    ARPER_UNLOCK(arper);
    
    memcpy(pk->header.src,c.mac,ETHER_MAC_LEN);

    pk->header.type = htons(0x0806);

    nk_net_ethernet_arp_reqresp_t *r = (nk_net_ethernet_arp_reqresp_t*)pk->data;

    DEBUG("Constructing request for ip address 0x%x\n",ip_addr);
    
    r->htype = htons(1);
    r->ptype = htons(0x0800);
    r->hlen = 6;
    r->plen = 4;
    r->oper = htons(1); // request
    memcpy(r->sha,c.mac,ETHER_MAC_LEN);
    r->spa = htonl(my_addr);
    memset(r->tha,0,ETHER_MAC_LEN);
    r->tpa = htonl(ip_addr);

    pk->len = 14 + 28;

    // now launch packet - we don't need to wait or be informed
    // eventually, we may get back a response, and that
    // response will be via the receive callback
    if (nk_net_ethernet_agent_device_send_packet(arper->netdev,
						 pk,
						 NK_DEV_REQ_NONBLOCKING,
						 0, 0)) {
	    ERROR("Failed to launch ARP request\n");
	    return -1;
    }

    DEBUG("ARP Request launched\n");

    return 1; // try again
}


int  nk_net_ethernet_arp_init()
{
    spinlock_init(&arper_list_lock);
    INIT_LIST_HEAD(&arper_list);

    INFO("inited\n");

    return 0;
}

void nk_net_ethernet_arp_deinit()
{
    ARPER_LIST_LOCK_CONF;

    ARPER_LIST_LOCK();

    struct list_head *cur, *tmp;
    
    list_for_each_safe(cur,tmp, &arper_list) {
	struct nk_net_ethernet_arper *a = list_entry(cur,struct nk_net_ethernet_arper, node);
	list_del_init(&a->node);
	nk_net_ethernet_agent_unregister(a->netdev);
	free(a);
    }
	
    ARPER_LIST_UNLOCK();

    INFO("deinited\n");
}
	


