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
 * Copyright (c) 2018, Hangjie Mo
 * Copyright (c) 2018, Weizheng Xu
 * Copyright (c) 2018, Peter Dinda
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Hangjie Mo   <HangjieMo2019@u.northwestern.edu>
 *           Weizheng Xu  <WeizhengXu2019@u.northwestern.edu> 
 *           Peter Dinda  <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/*
  This is a wrapper for a port of LWIP to the Nautilus kernel framework.  
  The copyright claims in the above statement apply to this wrapper and
  the changes to the LWIP code made during the port.  

  No claims are made to the LWIP codebase, which is BSD licensed.  
  The license statement is repeated below, and is in the source files.

  Copyright (c) 2001-2004 Swedish Institute of Computer Science. 
  All rights reserved. 

  Redistribution and use in source and binary forms, with or without modification, 
  are permitted provided that the following conditions are met: 

  1. Redistributions of source code must retain the above copyright notice, 
  this list of conditions and the following disclaimer. 
  2. Redistributions in binary form must reproduce the above copyright notice, 
  this list of conditions and the following disclaimer in the documentation 
  and/or other materials provided with the distribution. 
  3. The name of the author may not be used to endorse or promote products 
  derived from this software without specific prior written permission. 

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS AND ANY EXPRESS OR IMPLIED 
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
  SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
  OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
  OF SUCH DAMAGE.
  
*/

/*
  In its current incarnation, the LWIP port to Nautilus operates in "OS Mode" to
  allow access to both the callback APIs and the sequential APIs (e.g., sockets). 

  Currently, only Ethernet interfaces (and loopback) are supported.  The implementation
  for Ethernet builds upon Nautilus's Ethernet low-level layer instead of directly on
  netdevs (e.g., device drivers).   This is because network drivers in Nautilus 
  are extremely minimalist, and assumptions, such as the driver keeping the device
  fed with receive buffers, do not hold.   

*/


#include <net/lwip/lwip.h>
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/dns.h"
#include "netif/ethernetif.h"

volatile static int done = 0;

static struct nk_net_lwip_config config;

static void donefunc(void *foo)
{
    done = 1;
}


int nk_net_lwip_start(struct nk_net_lwip_config *conf)
{

    config = *conf;
    
    tcpip_init(donefunc,0);

    while (!done) {}

    ip4_addr_t dns;

    dns.addr = htonl(config.dns_ip);
    
    dns_setserver(1,&dns);
    
    INFO("inited\n");

    return 0;

}

int nk_net_lwip_add_interface(struct nk_net_lwip_interface *intconf)
{
    ip4_addr_t ip, netmask, gw;
    ip.addr = htonl(intconf->ipaddr);
    netmask.addr = htonl(intconf->netmask);
    gw.addr = htonl(intconf->gateway);

    struct netif *inter = mem_malloc(sizeof(struct netif));
    if (!inter) {
	return -1;
    }

    memset(inter,0,sizeof(struct netif));

    netif_add(inter, &ip, &netmask, &gw, intconf->name, ethernetif_init, tcpip_input);
    netif_set_up(inter);

    INFO("interface %s added\n", intconf->name);
	
    return 0;
}


// no stopping for us...
//
// also no changing of configuration... 
