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

#ifndef _LWIP_
#define _LWIP_

//
// This work consists of port of LWIP to Nautilus and
// this startup layer
//

#include <nautilus/netdev.h>

// 
// local loopback is done internally
// 

struct nk_net_lwip_interface {
    char     name[DEV_NAME_LEN];
    uint32_t ipaddr;
    uint32_t netmask;
    uint32_t gateway;
};

struct nk_net_lwip_config {
    uint32_t dns_ip;
};

// Call after ethernet agents, threads, and scheduling are available
// this brings up LWIP with the local loopback interface
// and registers the DNS server to user
int nk_net_lwip_start(struct nk_net_lwip_config *config);

// Add an interface with optional gateway
// the network device must not already have an agent, and
// currently is intended to be used ONLY for LWIP
int nk_net_lwip_add_interface(struct nk_net_lwip_interface *inter);

// currently it cannot stop

#endif
