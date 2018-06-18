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
#include <nautilus/scheduler.h>
#include <net/collective/ethernet/ethernet_collective.h>

#ifndef NAUT_CONFIG_DEBUG_NET_COLLECTIVE_ETHERNET
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("ether_col: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("ether_col: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("ether_col: " fmt, ##args)

// Fast L2 collective communication, currently just barriers and rings
// Also currently assumes no packet loss
// Also currently assumes synchrony barrier

// This is a work in progress

typedef enum {COLLECTIVE_IDLE=0, COLLECTIVE_BARRIER, COLLECTIVE_RING} mode_t;
typedef enum {BARRIER_IDLE=0, BARRIER_UP, BARRIER_DOWN} barrier_state_t;
typedef enum {RING_IDLE=0, RING_RECEIVE_LEFT} ring_state_t;

// collective message types
#define COLLECTIVE_RING_TYPE    0x1
#define COLLECTIVE_BARRIER_TYPE 0x2


struct nk_net_ethernet_collective {
    mode_t   current_mode;            // what operation we are handling
    
    // for barrier operations
    barrier_state_t barrier_state;
    uint32_t curlevel;
    uint32_t maxlevel;
    
    // for ring operations
    ring_state_t ring_state;
    int          initiator;
    void        *token;
    uint64_t     token_len;


    // the network device to use - this is supplied by an ethernet agent
    // it must support the ethernet agent send/receive packet interface
    struct nk_net_dev                          *netdev;
    struct nk_net_dev_characteristics           netchar;

    //  general metadata for collective operations
    uint32_t   num_nodes;  // size of the collective
    uint32_t   my_node;    // my own index in the mac array (e.g., my rank)
    uint16_t   type;       // ethernet type to use for collective operations
    ethernet_mac_addr_t macs[0];    // members of the collective - extended as per number requested
};



static inline void encode_packet(nk_ethernet_packet_t *p, ethernet_mac_addr_t dest, ethernet_mac_addr_t src, uint16_t basetype, uint16_t subtype, void *data, uint16_t len)
{
    memcpy(p->header.dst,dest,6);
    memcpy(p->header.src,src,6);
    p->header.type = htons(basetype);

    memcpy(p->data,&subtype,2);
    memcpy(p->data+2,&len,2);
    memcpy(p->data+4,data,len);

    p->len = 14 + 2 + 2 + len;
}

// we match against basetype, and subtype, and return -1 if this is not a match
// otherwise we deconstruct the packet
// len is an i/o argument, in: size of buffer, out: amount of data
static inline int decode_packet(nk_ethernet_packet_t *p, ethernet_mac_addr_t dest, ethernet_mac_addr_t src, uint16_t basetype, uint16_t subtype, void *data, uint16_t *len)
{
    uint16_t t;
    
    t = ntohs(p->header.type);

    if (t!=basetype) {
	return -1;
    }

    memcpy(&t,p->data,2);

    if (t!=subtype) {
	return -1;
    }

    
    // matching packet, so decode
    memcpy(dest,p->header.dst,6);
    memcpy(src,p->header.src,6);

    memcpy(&t,p->data+2,2);

    if (t < *len) {
	*len = t;
    }
    
    memcpy(data, p->data+4, *len);

    return 0;
}


// we match against basetype, and subtype, and return -1 if this is not a match
// otherwise we deconstruct the packet
// len is an i/o argument, in: size of buffer, out: amount of data
static inline int decode_packet_in_place(nk_ethernet_packet_t *p, ethernet_mac_addr_t **dest, ethernet_mac_addr_t **src, uint16_t basetype, uint16_t subtype, void **data, uint16_t *len)
{
    uint16_t t;
    
    t = ntohs(p->header.type);

    if (t!=basetype) {
	return -1;
    }

    memcpy(&t,p->data,2);

    if (t!=subtype) {
	return -1;
    }

    *dest = &p->header.dst;
    *src = &p->header.src;
    memcpy(&t,p->data+2,2);
    *len = t;
    *data = p->data+4;

    return 0;
}

#if 0

typedef struct barrier_data {
    uint8_t  dir;  // 0=UP, 1=DOWN
    uint32_t level;
} __packed barrier_data_t;

uint32_t log2_ceil(uint32_t n)
{
    return 32-(__builtin_clz(n)+1);
}

uint32_t pow2(uint32_t n)
{
    return 2<<n;
}

static void barrier_recv_callback(nk_net_ethernet_dev_status_t status,
				  nk_ethernet_packet_t *packet,
				  void *state)
{

    struct nk_net_ethernet_collective *col = (struct nk_net_ethernet_collective *)state;

    ethernet_mac_addr_t *dest, *src;
    uint16_t subtype;
    char buf[5];
    uint32_t level;
    uint8_t dir;
    barrier_data_t *bar;
    uint16_t barlen;
    
    if (status!=NK_NET_ETHERNET_DEV_STATUS_SUCCESS) {
	ERROR("Receive failure - the barrier may be dead..\n");
	return;
    }

    if (decode_packet_in_place(packet,
			       &dest,
			       &src,
			       col->type,
			       COLLECTIVE_BARRIER_TYPE,
			       &bar,
			       &barlen)) {
	DEBUG("Packet does not match current operation\n");
	return;
    }

    if (col->barrier_state == BARRIER_UP) {
	if (bar->dir!=0) {
	    DEBUG("Discarding barrier packet for wrong direction...\n");
	    nk_net_ethernet_release_packet(p);
	    return;
	}

	if (bar->level != (col->curlevel+1)) {
	    DEBUG("Discarding barrier packet for wrong level...\n");
	    nk_net_ethernet_release_packet(p);
	    return;
	}

	col->curlevel = bar->level;

	if (col->curlevel == col->maxlevel) {
	    // We are at the top of the tree and need to send the first packet down
	    // we will reuse
	    bar->dir=1;
	    bar->
    
	subtype!=COLLECTIVE_BARRIER_TYPE) {
	// failed or not a packet for me, do another receive
	if (nk_net_ethernet_dev_receive_packet(col->netdev,
				      col->recv_buf,
				      col->token_len+PACKET_HEADER_LEN,
				      NK_DEV_REQ_CALLBACK,
				      barrier_recv_callback,
				      col)) {
	    ERROR("Cannot re-initiate receive\n");
	    return;
	}
	return;
    }

    level = *(uint32_t*)buf;
    dir = (uint8_t) buf[4];
    
    if ((level != col->curlevel) ||
	(dir==0 && col->barrier_state!=BARRIER_UP) ||
	(dir!=0 && col->barrier_state!=BARRIER_DOWN)) {
	// uh oh
	    ERROR("Received level %u %s message, but we are currently at level %u %s\n",
		  level, dir==0 ? "UP" : "DOWN",
		  col->curlevel,
		  col->barrier_state==BARRIER_UP ? "UP" : "DOWN");
	    return;
    }

    if (col->barrier_state==BARRIER_UP) {
	if ((0x1UL << level) >= col->num_nodes
	
	if ((col->my_node >> (level+1))&0x1) {
	    // we are a sender in this round
	
    
		  
    if (!col->initiator) {
	// if we aren't the source of the token, we need to send it to our neighbor
	int len = encode_packet(col->send_buf,
				col->macs[(col->my_node + 1) % col->num_nodes],
				col->macs[col->my_node],
				COLLECTIVE_RING_TYPE,
				col->token,
				col->token_len);
	if (nk_net_ethernet_dev_send_packet(col->netdev,
				   col->send_buf,
				   len,
				   NK_DEV_REQ_NONBLOCKING,
				   0, 0)) {
	    ERROR("Cannot launch packet mid ring\n");
	    return;
	}
    }

    // now we are done

    col->ring_state = RING_IDLE;
    col->current_mode = COLLECTIVE_IDLE;
}


// The actual barrier can be executed as many times as desired
int nk_net_ethernet_collective_barrier(struct nk_net_ethernet_collective *col)
{

    if (col->num_nodes == 1) {
	return 0;
    }

    if (!__sync_bool_compare_and_swap(&col->current_mode,COLLECTIVE_IDLE,COLLECTIVE_BARRIER)) {
	DEBUG("Collective operation already in progress\n");
	return -1;
    }

    

    col->curlevel = 0;  // leaves
    col->maxlevel = log2_ceil(col->num_nodes); 
    col->barrier_state = BARRIER_UP;
    
    if (!(col->my_node % pow2(col->curlevel+1))) {
	// even nodes receive (from right odd node)
	col->curlevel = 1;
	if (nk_net_ethernet_agent_device_receive_packet(col->netdev,
							0,
							NK_DEV_REQ_CALLBACK,
							barrier_recv_callback,
							col)) {
	    ERROR("Cannot initiate receive\n");
	    return -1;
	}
    } else {
	// odd nodes
	// will send to left shark
	nk_ethernet_packet_t *p = nk_net_ethernet_alloc_packet(-1);

	if (!p) {
	    ERROR("Cannot allocate packet\n");
	    return -1;
	}

	barrier_data_t b = {.dir=0, .level=col->curlevel+1 };
	
	encode_packet(p,
		      col->macs[col->my_node - pow2(col->curlevel)],
		      col->macs[col->my_node],
		      col->type,
		      COLLECTIVE_BARRIER_TYPE,
		      &b,
		      sizeof(b));
	
	if (nk_net_ethernet_agent_device_send_packet(col->netdev,
						     p,
						     NK_DEV_REQ_NONBLOCKING,
						     0, 0)) {
	    DEBUG("Initiator cannot launch packet\n");
	    return -1;
	}
    }

    uint64_t start = nk_sched_get_realtime();
    
    // wait for completion
    while (__sync_fetch_and_or(&col->current_mode,0)==COLLECTIVE_BARRIER) {
	// barriers do not regenerate...
    }

    return 0;
}

#else
	    
int nk_net_ethernet_collective_barrier(struct nk_net_ethernet_collective *col)
{
    return -1;
}

#endif
	    
// Data in a ring packet:   token
//

static void ring_recv_callback(nk_net_dev_status_t status,
			       nk_ethernet_packet_t *packet,
			       void *state)
{
    struct nk_net_ethernet_collective *col = (struct nk_net_ethernet_collective *)state;

    ethernet_mac_addr_t *dest, *src;
    uint16_t subtype;
    void *token;
    uint16_t token_len;

    
    if (status!=NK_NET_DEV_STATUS_SUCCESS) {
	ERROR("Receive failure - the token will have to be regenerated\n");
	return;
    }

    if (decode_packet_in_place(packet,
			       &dest,
			       &src,
			       col->type,
			       COLLECTIVE_RING_TYPE,
			       &token,
			       &token_len)) {
	DEBUG("Packet does not match current operation\n");
	return;
    }

    DEBUG("Received token packet\n");

    if (token_len<col->token_len) {
	token_len = col->token_len;
    }
    
    memcpy(col->token,token,token_len);
    
    if (!col->initiator) {
	// we will send the same packet to our neighbor;

	memcpy(packet->header.dst, col->macs[(col->my_node + 1) % col->num_nodes], 6);

	memcpy(packet->header.src,col->macs[col->my_node], 6);

	// all else is the same

	if (nk_net_ethernet_agent_device_send_packet(col->netdev,
						     packet,
						     NK_DEV_REQ_NONBLOCKING,
						     0, 0)) {
	    ERROR("Cannot launch packet mid ring\n");
	    return;
	}
    }

    // now we are done

    col->ring_state = RING_IDLE;
    col->current_mode = COLLECTIVE_IDLE;
}


#define REGEN_DELAY_NS 10000000
// circulate a token among the nodes in the barrier
// for two nodes, this is a ping-pong
int nk_net_ethernet_collective_ring(struct nk_net_ethernet_collective *col, void *token, uint64_t token_len, int initiate)
{
    if (token_len>NET_ETHERNET_COLLECTIVE_MAX_TOKEN_LEN) {
	DEBUG("token length unsupported\n");
	return -1;
    }

    if (col->num_nodes == 1) {
	return 0;
    }

    if (!__sync_bool_compare_and_swap(&col->current_mode,COLLECTIVE_IDLE,COLLECTIVE_RING)) {
	DEBUG("Collective operation already in progress\n");
	return -1;
    }

    col->initiator = initiate;
    col->token = token;
    col->token_len = token_len;


    col->ring_state = RING_RECEIVE_LEFT;
    
    // everyone posts receive for left
    if (nk_net_ethernet_agent_device_receive_packet(col->netdev,
						    0,
						    NK_DEV_REQ_CALLBACK,
						    ring_recv_callback,
						    col)) {
	ERROR("Cannot initiate receive\n");
	return -1;
    }

 do_initiate:
    // initiator also launches the first packet
    if (initiate) {
	nk_ethernet_packet_t *p = nk_net_ethernet_alloc_packet(-1);

	if (!p) {
	    ERROR("Cannot allocate packet\n");
	    return -1;
	}
	
	encode_packet(p,
		      col->macs[(col->my_node + 1) % col->num_nodes],
		      col->macs[col->my_node],
		      col->type,
		      COLLECTIVE_RING_TYPE,
		      token,
		      token_len);
	
	if (nk_net_ethernet_agent_device_send_packet(col->netdev,
						     p,
						     NK_DEV_REQ_NONBLOCKING,
						     0, 0)) {
	    DEBUG("Initiator cannot launch packet\n");
	    return -1;
	}
    }

    uint64_t start = nk_sched_get_realtime();
    
    // wait for completion
    while (__sync_fetch_and_or(&col->current_mode,0)==COLLECTIVE_RING) {
	if ((nk_sched_get_realtime()-start) > REGEN_DELAY_NS) {
	    // relaunch the packet if we have been waiting too long
	    goto do_initiate;
	}
    }

    return 0;
}

struct nk_net_ethernet_collective *nk_net_ethernet_collective_create(struct nk_net_ethernet_agent *agent,
								     uint16_t    type,
								     uint32_t    num_nodes,
								     ethernet_mac_addr_t  macs[])
{
    uint64_t size = sizeof(struct nk_net_ethernet_collective)+sizeof(ethernet_mac_addr_t)*num_nodes;
    uint32_t i;
    
    struct nk_net_ethernet_collective *col = (struct nk_net_ethernet_collective *)malloc(size);

    if (!col) {
	ERROR("Failed to allocate collective structure for %u nodes\n",num_nodes);
	return 0;
    }

    memset(col,0,sizeof(*col));
    
    col->num_nodes = num_nodes;
    col->type = type;

    if (!(col->netdev = nk_net_ethernet_agent_register_type(agent, type))) {
	ERROR("Cannot register with agent for type %x\n",type);
	free(col);
	return 0;
    }

    if (nk_net_dev_get_characteristics(col->netdev, &col->netchar)) {
	ERROR("Failed to get network characterstics\n");
	free(col);
	return 0;
    }

    col->my_node = -1;
    for (i=0;i<num_nodes;i++) {
	memcpy(col->macs[i],macs[i],6);
	if (!memcmp(col->macs[i],col->netchar.mac,ETHER_MAC_LEN)) {
	    col->my_node = i;
	}
    }
    
    if (col->my_node == -1 ) {
	ERROR("I can't find myself among given mac addresses\n");
	nk_net_ethernet_agent_unregister(col->netdev);
	free(col);
	return 0;
    }

    return col;
}
	      


int nk_net_ethernet_collective_destroy(struct nk_net_ethernet_collective *col)
{
    if (col->current_mode != COLLECTIVE_IDLE) {
	return -1;
    } else {
	nk_net_ethernet_agent_unregister(col->netdev);
	free(col);
	return 0;
    }
}


int nk_net_ethernet_collective_init()
{
    INFO("inited\n");
    return 0;
}

void nk_net_ethernet_collective_deinit()
{
    INFO("deinited\n");
}


