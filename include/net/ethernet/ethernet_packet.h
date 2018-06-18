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

#ifndef __NETCOL__
#define __NETCOL__

#include <nautilus/naut_types.h>
#include <nautilus/netdev.h>
#include <nautilus/list.h>

typedef uint8_t ethernet_mac_addr_t[6];

#ifdef NAUT_CONFIG_NET_ETHERNET_HUGE_PACKET
#define MAX_ETHERNET_PACKET_DATA_LEN 9000
#else
#define MAX_ETHERNET_PACKET_DATA_LEN 1500
#endif

#define ETHERNET_HEADER_LEN 14

//#define MAX_ETHERNET_PACKET_LEN (ETHERNET_HEADER_LEN + MAX_ETHERNET_PACKET_DATA_LEN)

// gruesome hack
#define MAX_ETHERNET_PACKET_LEN 2048

typedef struct nk_ethernet_packet {
    struct list_head node;   // used internally for free packets, can be used externally for allocated packets

    uint64_t         refcount;

    int              alloc_cpu;      // the cpu this packet was allocated for

    uint32_t         len;            // how many bytes of the raw data are in use

    void             *metadata;      // for external use

    union {
	uint8_t raw[MAX_ETHERNET_PACKET_LEN];
	struct {
	    struct {
		ethernet_mac_addr_t dst;
		ethernet_mac_addr_t src;
		uint16_t            type;
	    } __packed          header;
	    uint8_t             data[MAX_ETHERNET_PACKET_DATA_LEN];
	} __packed;
    } __packed;  // probably should also align to a cache line
} nk_ethernet_packet_t;


#define ntohs(x) ((((x)<<8)&0xff00)|(((x)>>8)&0xff))
#define ntohl(x) ((((x)<<24)&0xff000000)|(((x)<<8)&0xff0000)|(((x)>>8)&0xff00)|(((x)>>24)&0xff))
#define htons(x) ntohs(x)
#define htonl(x) ntohl(x)

// allocate a packet with an affinity for the given cpu (-1 => any)
// allocating a packet will also acquire it (refcount => 1 ).
// until the packet is released for the final time, the node field can be used
// by the caller
nk_ethernet_packet_t *nk_net_ethernet_alloc_packet(int cpu);

void nk_net_ethernet_acquire_packet(nk_ethernet_packet_t *packet);

// the final release will free the packet
void nk_net_ethernet_release_packet(nk_ethernet_packet_t *packet);

// called by BSP at bootstrap *after* kmem setup is done
int  nk_net_ethernet_packet_init();

void nk_net_ethernet_packet_deinit();

#endif
