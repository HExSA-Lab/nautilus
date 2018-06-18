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

#ifndef __ETHERNET_ARP__
#define __ETHERNET_ARP__

#include <net/ethernet/ethernet_packet.h>
#include <net/ethernet/ethernet_agent.h>

typedef uint32_t ipv4_addr_t; 

typedef struct nk_net_ethernet_arp_reqresp {
    uint16_t        htype;            // 1 = ethernet
    uint16_t        ptype;            // 0x0800 = IPv4
    uint8_t         hlen;             // should be 6
    uint8_t         plen;             // should be 4
    uint16_t        oper;             // 1 = req, 2 = resp
    ethernet_mac_addr_t sha;          // sender's hardware address (MAC)
    ipv4_addr_t         spa;          // sender's protocol address (IP)
    ethernet_mac_addr_t tha;          // target's mac address
    ipv4_addr_t         tpa;          // target's ip address
} __packed nk_net_ethernet_arp_reqresp_t;


// create and start an arper
struct nk_net_ethernet_arper * nk_net_ethernet_arper_create(struct nk_net_ethernet_agent *agent);
int                            nk_net_ethernet_arper_destroy(struct nk_net_ethernet_arper *arper);

int nk_net_ethernet_arper_add(struct nk_net_ethernet_arper *arper, ipv4_addr_t ip_addr, ethernet_mac_addr_t mac_addr);
int nk_net_ethernet_arper_remove(struct nk_net_ethernet_arper *arper, ipv4_addr_t ip_addr);

// Attempt a lookup, launching a request if needed
// if arper=null, we will ask all the arpers
// if it is not in the cache, this will fail with a positive return value (try again)
// if it is a hard failure, a negative return value is given
int nk_net_ethernet_arp_lookup(struct nk_net_ethernet_arper *arper, ipv4_addr_t ip_addr, ethernet_mac_addr_t mac_addr);

int  nk_net_ethernet_arp_init();
void nk_net_ethernet_arp_deinit();


#endif
