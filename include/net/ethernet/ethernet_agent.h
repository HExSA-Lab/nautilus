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

#ifndef __ETHERNET_AGENT__
#define __ETHERNET_AGENT__

#include <nautilus/naut_types.h>
#include <nautilus/netdev.h>
#include <net/ethernet/ethernet_packet.h>

   
struct nk_net_ethernet_agent;

// Creating an agent for a network device
// the agent will operate the send and receive queues
// as well as multiplexing and demultiplexing the device
// it is undefined to have more more than one agent per device
struct nk_net_ethernet_agent *nk_net_ethernet_agent_create(struct nk_net_dev *netdev, char *agent_name, uint64_t send_queue_size, uint64_t receive_queue_size);

// We can search for an agent by name
struct nk_net_ethernet_agent *nk_net_ethernet_agent_find(char *name);

// We can register for a given type with the agent, this returns a new, enahanced network device
// and it also registers the net device with agentname+<type>
struct nk_net_dev *nk_net_ethernet_agent_register_type(struct nk_net_ethernet_agent *agent, uint16_t type);
struct nk_net_dev *nk_net_ethernet_agent_register_filter(struct nk_net_ethernet_agent *agent, int (*filter)(nk_ethernet_packet_t *packet, void *state), void *state);
int                nk_net_ethernet_agent_unregister(struct nk_net_dev *dev);

int nk_net_ethernet_agent_start(struct nk_net_ethernet_agent *agent);
int nk_net_ethernet_agent_stop(struct nk_net_ethernet_agent *agent);


// the user can find the agented network device for a given type
// by its device name, which will give him the basic
// interface, which replicated here:
#define nk_net_ethernet_agent_device_receive_buffer(dev,dest,len,type,callback,state) nk_net_dev_receive_packet(dev,dest,len,type,callback,state)
#define nk_net_ethernet_agent_device_send_buffert(dev,src,len,type,callback,state) nk_net_dev_send_packet(dev,src,len,type,callback,state)

// the user can also the following calls avoid copying packets
// Note that packets can be allocated via the interface in ethernet_packet.h

int nk_net_ethernet_agent_device_send_packet(struct nk_net_dev *dev,
					     nk_ethernet_packet_t *packet,
					     nk_dev_request_type_t type, 
					     void (*callback)(nk_net_dev_status_t status,
							      nk_ethernet_packet_t *packet,
							      void *state), // for callback reqs
					     void *state);

int nk_net_ethernet_agent_device_receive_packet(struct nk_net_dev *dev,
						nk_ethernet_packet_t **packet,
						nk_dev_request_type_t type, 
						void (*callback)(nk_net_dev_status_t status,
								 nk_ethernet_packet_t *packet,
								 void *state), // for callback reqs
						void *state);

struct nk_net_dev *nk_net_ethernet_agent_get_underlying_device(struct nk_net_ethernet_agent *agent);


int  nk_net_ethernet_agent_init();
void nk_net_ethernet_agent_deinit();


#endif
