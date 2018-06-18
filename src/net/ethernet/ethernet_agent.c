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
#include <net/ethernet/ethernet_agent.h>

// right now this is a trivial implementation that just has a single pool
// in the future, this could expand to a per-cpu pool


#define MAX_AGENT_NAME 32

// number of received packets that match to store
#define MAX_DEV_RECEIVE_QUEUE 64

#ifndef NAUT_CONFIG_DEBUG_NET_ETHERNET_AGENT
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("ethernet_agent: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("ethernet_agent: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("ethernet_agent: " fmt, ##args)

#define AGENT_LIST_LOCK_CONF uint8_t _agent_list_lock_flags
#define AGENT_LIST_LOCK() _agent_list_lock_flags = spin_lock_irq_save(&agent_list_lock)
#define AGENT_LIST_UNLOCK() spin_unlock_irq_restore(&agent_list_lock, _agent_list_lock_flags)

#define AGENT_LOCK_CONF uint8_t _agent_lock_flags
#define AGENT_LOCK(a) _agent_lock_flags = spin_lock_irq_save(&a->lock)
#define AGENT_UNLOCK(a) spin_unlock_irq_restore(&a->lock, _agent_lock_flags)

#define DEV_LOCK_CONF uint8_t _dev_lock_flags
#define DEV_LOCK(d) _dev_lock_flags = spin_lock_irq_save(&d->lock)
#define DEV_UNLOCK(d) spin_unlock_irq_restore(&d->lock, _dev_lock_flags)

#define OP_FREE_LIST_LOCK_CONF uint8_t _op_free_list_lock_flags
#define OP_FREE_LIST_LOCK() _op_free_list_lock_flags = spin_lock_irq_save(&op_free_list_lock)
#define OP_FREE_LIST_UNLOCK() spin_unlock_irq_restore(&op_free_list_lock, _op_free_list_lock_flags)




struct nk_net_ethernet_agent {
    spinlock_t         lock;
    struct list_head   node; // for agent list

    enum { STOPPED=0, RUNNING} state;

    char               name[MAX_AGENT_NAME];
    struct nk_net_dev *netdev;

    struct list_head   dev_list;

    uint64_t           send_queue_size;
    uint64_t           recv_queue_size;

    // number of outstanding sends and receives handed to NIC
    uint64_t           send_queue_num;
    uint64_t           recv_queue_num;

    // eventually this will have a hash table for faster match by type
};

static spinlock_t       agent_list_lock;
static struct list_head agent_list;

struct netdev_op
{
    struct list_head      node;  // for a queue
    
    enum {BUFFER,PACKET}  interface;
    enum {SEND, RECEIVE}  type;

    struct nk_net_ethernet_agent_net_dev *dev;
    
    uint8_t              *buf;  // for interface=BUFFER
    uint64_t              len;
    
    nk_ethernet_packet_t *packet; // for interface=PACKET

    void                 (*callback)(nk_net_dev_status_t status, void *context);
    void                 (*callback_packet)(nk_net_dev_status_t status, nk_ethernet_packet_t *p, void *context);
    void                 *context;
    
};

static spinlock_t       op_free_list_lock;
static struct list_head op_free_list;


static inline void free_op(struct netdev_op *o)
{
    OP_FREE_LIST_LOCK_CONF;
    
    OP_FREE_LIST_LOCK();
    list_add(&o->node,&op_free_list);
    OP_FREE_LIST_UNLOCK();
}

static inline struct netdev_op *alloc_op()
{
    OP_FREE_LIST_LOCK_CONF;
    struct list_head *n;
    struct netdev_op *o = 0;
    
    OP_FREE_LIST_LOCK();
    if (!list_empty(&op_free_list)) {
	n = op_free_list.next;
	list_del_init(n);
	o = list_entry(n,struct netdev_op,node);
    }
    OP_FREE_LIST_UNLOCK();
    if (!o) {
	o = (struct netdev_op *) malloc(sizeof(*o));
	if (o) {
	    INIT_LIST_HEAD(&o->node);
	}
    }
    return o;
}


struct nk_net_ethernet_agent_net_dev {
    struct nk_net_dev           *netdev;

    spinlock_t                  lock;

    // received matching packets that did not have a matching op yet
    uint64_t                    receive_queue_count;
    struct list_head            receive_queue;

    // outstanding receive or send operations that did not have a matching received packet or send completion yet
    struct list_head            receive_op_queue;
    struct list_head            send_op_queue;
    
    // and also includes additional info
    // note that agent->netdev gives you the underlying network device
    struct nk_net_ethernet_agent *agent;
    struct list_head            devnode; // within the agent

    int                         (*filter)(nk_ethernet_packet_t *packet, void *state);
    void                        *filter_state;
};

struct nk_net_ethernet_agent *nk_net_ethernet_agent_create(struct nk_net_dev *dev, char *name, uint64_t send_queue_size, uint64_t receive_queue_size)
{
    AGENT_LIST_LOCK_CONF;

    if (!dev) {
	ERROR("Cannot find net device with name %s\n",name);
	return 0;
    }

    struct nk_net_ethernet_agent *a = malloc(sizeof(struct nk_net_ethernet_agent));

    if (!a) {
	ERROR("Cannot allocate an agent\n");
	return 0;
    }

    memset(a,0,sizeof(*a));

    spinlock_init(&a->lock);
    strncpy(a->name,name,MAX_AGENT_NAME); a->name[MAX_AGENT_NAME-1]=0;
    a->netdev = dev;

    INIT_LIST_HEAD(&a->dev_list);
    a->send_queue_size = send_queue_size;
    a->recv_queue_size = receive_queue_size;

    AGENT_LIST_LOCK();
    list_add(&a->node,&agent_list);
    AGENT_LIST_UNLOCK();
    
    return a;
    
}

// We can search for an agent by name
struct nk_net_ethernet_agent *nk_net_ethernet_agent_find(char *name)
{
    struct list_head *cur;
    struct nk_net_ethernet_agent *a;
    
    AGENT_LIST_LOCK_CONF;

    a=0;
    
    AGENT_LIST_LOCK();
    list_for_each(cur,&agent_list) {
	a = list_entry(cur,struct nk_net_ethernet_agent,node);
	if (a) {
	    break;
	}
    }
    AGENT_LIST_UNLOCK();

    return a;
}



static void recv_callback(nk_net_dev_status_t status, void *state);


// called with agent lock held
static void queue_receives(struct nk_net_ethernet_agent *a)
{
    while (a->recv_queue_num < a->recv_queue_size) {
	nk_ethernet_packet_t *p = nk_net_ethernet_alloc_packet(-1);
	if (!p) {
	    ERROR("Starting agent with fewer receives queued than desired\n");
	    break;
	}

	p->metadata = a;

	if (nk_net_dev_receive_packet(a->netdev,
				      p->raw,
				      MAX_ETHERNET_PACKET_LEN,
				      NK_DEV_REQ_CALLBACK,
				      recv_callback,
				      p)) {
	    ERROR("Failed to queue receive - agent started with fewer receives queued than desired..\n");
	    nk_net_ethernet_release_packet(p);
	    break;
	} else {
	    a->recv_queue_num++;
	}
    }
}

static int type_filter(nk_ethernet_packet_t *p, void *state)
{
    uint16_t type = (uint16_t)(uint64_t)state;

    return ntohs(p->header.type)==type;
}

// assumes agent is locked
static struct nk_net_ethernet_agent_net_dev *match_device(struct nk_net_ethernet_agent *a, nk_ethernet_packet_t *p)
{
    struct list_head *cur=0;
    struct nk_net_ethernet_agent_net_dev *d;

    list_for_each(cur,&a->dev_list) {
	d = list_entry(cur,struct nk_net_ethernet_agent_net_dev, devnode);
	if (d->filter && d->filter(p,d->filter_state)) {
	    return d;
	}
    }
    return 0;

}

static int complete_receive(struct nk_net_ethernet_agent_net_dev *d, nk_ethernet_packet_t *p)
{
    struct list_head *n;
    struct netdev_op *o=0;
    
    DEV_LOCK_CONF;

    DEV_LOCK(d);

    if (!list_empty(&d->receive_op_queue)) {
	n = d->receive_op_queue.next;
	o = list_entry(n,struct netdev_op, node);
	list_del_init(n);
    }
    DEV_UNLOCK(d);

    if (!o) {
	DEBUG("No matching receive\n");
	DEV_LOCK(d);
	if (d->receive_queue_count >= MAX_DEV_RECEIVE_QUEUE) {
	    DEV_UNLOCK(d);
	    DEBUG("No room to queue packet - discarding\n");
	    nk_net_ethernet_release_packet(p);
	    return 0;
	} else {
	    list_add_tail(&p->node,&d->receive_queue);
	    d->receive_queue_count++;
	    DEV_UNLOCK(d);
	    DEBUG("Queued packet for later\n");
	    return 0;
	}
    } else {
	if (o->interface==PACKET) {
	    if (o->callback_packet) { 
		o->callback_packet(NK_NET_DEV_STATUS_SUCCESS, p, o->context);
	    }
	    // receiver releases packet...
	} else { // BUFFER
	    memcpy(o->buf,p->raw,p->len);
	    nk_net_ethernet_release_packet(p);
	    if (o->callback) {
		o->callback(NK_NET_DEV_STATUS_SUCCESS, o->context);
	    }
	}
	free_op(o);
	return 0;
    }
}

// A receive callback is generic for all ethernet packets since
// we need to demux them to the caller.   Hence receives are handed
// the packet that was just received with its metadata being the
// agent
static void recv_callback(nk_net_dev_status_t status,
			  void *state)
{
    nk_ethernet_packet_t *p = (nk_ethernet_packet_t*)state;
    struct nk_net_ethernet_agent *a = (struct nk_net_ethernet_agent *) p->metadata;
    struct nk_net_ethernet_agent_net_dev *d = 0;
    AGENT_LOCK_CONF;
    
    if (status!=NK_NET_DEV_STATUS_SUCCESS) {
	// uhoh
	ERROR("Receive failure for packet %p\n", p);
    } else {

	AGENT_LOCK(a);
	if (a->state==RUNNING) {
	    d = match_device(a,p);
	} else {
	    d = 0;
	}
	AGENT_UNLOCK(a);

	if (!d) {
	    // no device found or we are not running, so just discard packet
	    DEBUG("dropping packet of type 0x%x\n",ntohs(p->header.type));
	    nk_net_ethernet_release_packet(p);
	} else {
	    DEBUG("matched packet of type 0x%x to device %s\n",ntohs(p->header.type),d->netdev->dev.name);
	    if (complete_receive(d,p)) {
		DEBUG("Failed to complete receive on matched packet... dropping packet\n");
	    }
	    // up to receiver to release packet when they are done wit it
	}
    }


    // and queue more receives
    AGENT_LOCK(a);
    a->recv_queue_num--;
    queue_receives(a);
    AGENT_UNLOCK(a);
}

// Sends are multiplexed before this, so a send callback is handed the original op 
static void send_callback(nk_net_dev_status_t status,
			  void *state)
{
    struct netdev_op *o = (struct netdev_op *) state;
    nk_ethernet_packet_t *p = (nk_ethernet_packet_t*) o->packet;
    struct nk_net_ethernet_agent_net_dev *d = (struct nk_net_ethernet_agent_net_dev *) o->dev;
    struct nk_net_ethernet_agent *a = d->agent;

    // The network op is not in any list at this point

    if (o->interface==BUFFER) {
	nk_net_ethernet_release_packet(p);
	if (o->callback) {
	    o->callback(status,o->context);
	}
    } else { // PACKET
	if (o->callback_packet) {
	    o->callback_packet(status, p, o->context);
	    // sender is responsible for releasing the packet
	} else {
	    // we are responsible for releasing the packet
	    nk_net_ethernet_release_packet(p);
	}
    }
}



static int get_characteristics(void *state, struct nk_net_dev_characteristics *c)
{
    struct nk_net_ethernet_agent_net_dev *d =  (struct nk_net_ethernet_agent_net_dev *) state;
    void *dev_state = d->agent->netdev->dev.state;
    struct nk_net_dev_int *dev_int = (struct nk_net_dev_int *) d->agent->netdev->dev.interface;
    return dev_int->get_characteristics(dev_state,c);
}


#define MIN(x,y) ((x)<(y) ? (x) : (y))

static inline int post_send_recv(void *state, uint8_t *buf, uint64_t len, void (*callback)(nk_net_dev_status_t status, void *context), void *context, int recv)
{
    DEV_LOCK_CONF;
    
    struct nk_net_ethernet_agent_net_dev *d =  (struct nk_net_ethernet_agent_net_dev *) state;
    struct netdev_op *o = alloc_op();

    if (!o) {
	return -1;
    }

    o->interface = BUFFER;
    o->type = recv ? RECEIVE : SEND;
    o->buf = buf;
    o->len = len;
    o->packet = 0;
    o->callback=callback;
    o->callback_packet=0;
    o->context = context;

    if (recv) {
	DEV_LOCK(d);
	if (d->receive_queue_count) {
	    // already have a packet, issue callback immediately
	    nk_ethernet_packet_t *p = list_entry(d->receive_queue.next,nk_ethernet_packet_t,node);
	    list_del_init(d->receive_queue.next);
	    d->receive_queue_count--;
	    DEV_UNLOCK(d);
	    free_op(o);
	    memcpy(buf,p->raw,MIN(len,p->len));
	    nk_net_ethernet_release_packet(p);
	    callback(NK_NET_DEV_STATUS_SUCCESS,context);
	} else {
	    // we need to queue the op for a future packet
	    // receiver releases packet
	    list_add_tail(&o->node,&d->receive_op_queue);
	    DEV_UNLOCK(d);
	}
	// the receive handler is responsible for queuing packets
	return 0;
    } else {
	// send goes straight through to the underlying device
	void *dev_state = d->agent->netdev->dev.state;
	struct nk_net_dev_int *dev_int = (struct nk_net_dev_int *) d->agent->netdev->dev.interface;

	o->packet = nk_net_ethernet_alloc_packet(-1);
	if (!o->packet) {
	    return -1;
	}
	memcpy(o->packet->raw,buf,len);
	o->packet->len = len;

	return dev_int->post_send(dev_state, o->packet->raw, o->packet->len, send_callback, o);
    }

    return 0;
      
}

static int post_send(void *state, uint8_t *src, uint64_t len, void (*callback)(nk_net_dev_status_t status, void *context), void *context)
{
    return post_send_recv(state,src,len,callback,context,0);
}

static int post_receive(void *state, uint8_t *dest, uint64_t len, void (*callback)(nk_net_dev_status_t status, void *context), void *context)
{
    return post_send_recv(state,dest,len,callback,context,1);
}


static inline int post_send_recv_packet(void *state, nk_ethernet_packet_t *packet, void (*callback)(nk_net_dev_status_t status, nk_ethernet_packet_t *packet, void *context), void *context, int recv)
{
    DEV_LOCK_CONF;
    
    struct nk_net_ethernet_agent_net_dev *d =  (struct nk_net_ethernet_agent_net_dev *) state;
    struct netdev_op *o = alloc_op();

    if (!o) {
	return -1;
    }

    o->interface = PACKET;
    o->type = recv ? RECEIVE : SEND;
    o->buf = 0;
    o->len = 0;
    o->packet = recv ? 0 : packet;
    o->callback=0;
    o->callback_packet=callback;
    o->context = context;

    if (recv) {
	DEV_LOCK(d);
	if (d->receive_queue_count) {
	    // already have a packet, issue callback immediately
	    nk_ethernet_packet_t *p = list_entry(d->receive_queue.next,nk_ethernet_packet_t,node);
	    list_del_init(d->receive_queue.next);
	    d->receive_queue_count--;
	    DEV_UNLOCK(d);
	    free_op(o);
	    callback(NK_NET_DEV_STATUS_SUCCESS,p,context);
	} else {
	    // we need to queue the op for a future packet
	    // receiver releases packet
	    list_add_tail(&o->node,&d->receive_op_queue);
	    DEV_UNLOCK(d);
	}
	// the receive handler is responsible for queuing packets
	return 0;
    } else {
	// send goes straight through to the underlying device
	void *dev_state = d->agent->netdev->dev.state;
	struct nk_net_dev_int *dev_int = (struct nk_net_dev_int *) d->agent->netdev->dev.interface;
	
	return dev_int->post_send(dev_state, o->packet->raw, o->packet->len, send_callback, o);
    }

    return 0;
      
}

static int post_send_packet(void *state, nk_ethernet_packet_t *packet, void (*callback)(nk_net_dev_status_t status, nk_ethernet_packet_t *packet, void *context), void *context)
{
    DEBUG("Post Send Packet state=%p, packet=%p callback=%p context=%p\n",
	  state,packet,callback,context);
	 
    return post_send_recv_packet(state, packet, callback, context, 0);
}

static int post_receive_packet(void *state,  void (*callback)(nk_net_dev_status_t status, nk_ethernet_packet_t *packet, void *context), void *context)
{
    return post_send_recv_packet(state, 0, callback, context, 1);
}



struct nk_net_ethernet_agent_net_dev_int {
    // this must be first so it derives cleanly
    // from nk_net_dev_int
    struct nk_net_dev_int netdev_int;

    // these extend it for packets
    int (*post_receive_packet)(void *state, void (*callback)(nk_net_dev_status_t status, nk_ethernet_packet_t *packet, void *context), void *context);
    int (*post_send_packet)(void *state, nk_ethernet_packet_t *packet, void (*callback)(nk_net_dev_status_t status, nk_ethernet_packet_t *packet, void *context), void *context);
};

// this needs to be an extension of the netdev interface
static struct nk_net_ethernet_agent_net_dev_int ops = {
    .netdev_int = {
	.get_characteristics = get_characteristics,
	.post_receive = post_receive,
	.post_send = post_send
    },
    .post_receive_packet = post_receive_packet,
    .post_send_packet = post_send_packet
};



struct nk_net_dev *nk_net_ethernet_agent_register_filter(struct nk_net_ethernet_agent *agent, int (*filter)(nk_ethernet_packet_t *packet, void *state), void *state)
{
    AGENT_LOCK_CONF;
    char name[DEV_NAME_LEN];

    struct nk_net_ethernet_agent_net_dev *d = malloc(sizeof(*d));

    if (!d) {
	ERROR("Failed to allocate device\n");
	return 0;
    }

    memset(d,0,sizeof(*d));

    spinlock_init(&d->lock);
    INIT_LIST_HEAD(&d->receive_queue);
    INIT_LIST_HEAD(&d->receive_op_queue);
    INIT_LIST_HEAD(&d->send_op_queue);
    d->agent = agent;
    INIT_LIST_HEAD(&d->devnode);
    d->filter = filter;
    d->filter_state = state;

    if (filter==type_filter) { 
	snprintf(name,DEV_NAME_LEN,"%s-type%04x",agent->name,(uint16_t)(uint64_t)state);
    } else {
	snprintf(name,DEV_NAME_LEN,"%s-filt%08x",agent->name,(uint32_t)(uint64_t)filter);
    }

    d->netdev = nk_net_dev_register(name,0,(struct nk_net_dev_int*)&ops,d);

    if (!d->netdev) {
	ERROR("Failed to register device %s\n",name);
	free(d);
	return 0;
    }
    
    AGENT_LOCK(agent);
    list_add(&d->devnode,&agent->dev_list);
    AGENT_UNLOCK(agent);

    return d->netdev;

}


struct nk_net_dev *nk_net_ethernet_agent_register_type(struct nk_net_ethernet_agent *agent, uint16_t type)
{
    return nk_net_ethernet_agent_register_filter(agent,type_filter,(void*)(uint64_t)type);
}


int                nk_net_ethernet_agent_unregister(struct nk_net_dev *dev)
{
    AGENT_LOCK_CONF;

    struct nk_net_ethernet_agent_net_dev *netdev = dev->dev.state;
    struct nk_net_ethernet_agent *agent = netdev->agent;

    AGENT_LOCK(agent);
    list_del_init(&netdev->devnode);
    AGENT_UNLOCK(agent);

    // now we have exclusive access to this device (no lock needed)
    // so clear out the queues
    struct list_head *cur, *temp;

    list_for_each_safe(cur,temp,&netdev->receive_queue) {
	nk_ethernet_packet_t *p = list_entry(cur,nk_ethernet_packet_t,node);
	list_del_init(cur);
	nk_net_ethernet_release_packet(p);
    }

    list_for_each_safe(cur,temp,&netdev->receive_op_queue) {
	struct netdev_op *op = list_entry(cur,struct netdev_op,node);
	list_del_init(cur);
	free(op);
    }

    list_for_each_safe(cur,temp,&netdev->send_op_queue) {
	struct netdev_op *op = list_entry(cur,struct netdev_op,node);
	list_del_init(cur);
	free(op);
    }

    if (nk_net_dev_unregister(netdev->netdev)) {
	ERROR("Failed to unregister underlying netdev\n");
	free(dev);
	return -1;
    } else {
	free(dev);
	return 0;
    }
}


struct op {
    int                 completed;
    nk_net_dev_status_t status;
    struct nk_net_dev   *dev;
    nk_ethernet_packet_t **packet_dest; // for receive
};


static void generic_send_callback(nk_net_dev_status_t status, nk_ethernet_packet_t *p, void *context)
{
    struct op *o = (struct op*) context;
    DEBUG("generic send callback (status = 0x%lx) for %p\n",status,context);
    // we can now release the packet
    nk_net_ethernet_release_packet(p);
    if (o) { 
	o->status = status;
	o->completed = 1;
	nk_dev_signal((struct nk_dev *)o->dev);
    }
}

static void generic_receive_callback(nk_net_dev_status_t status, nk_ethernet_packet_t *p, void *context)
{
    struct op *o = (struct op*) context;
    DEBUG("generic receive callback (status = 0x%lx) for %p\n", status, context);
    if (o) { 
	*o->packet_dest = p;
	o->status = status;
	o->completed = 1;
	nk_dev_signal((struct nk_dev *)o->dev);
    }
}

static int generic_cond_check(void* state)
{
    struct op *o = (struct op*) state;
    return o->completed;
}

// These two mirror the general interface of netdev.h

int nk_net_ethernet_agent_device_send_packet(struct nk_net_dev *dev,
					     nk_ethernet_packet_t *packet,
					     nk_dev_request_type_t type, 
					     void (*callback)(nk_net_dev_status_t status,
							      nk_ethernet_packet_t *packet,
							      void *state), // for callback reqs
					     void *state)
{
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
    struct nk_net_ethernet_agent_net_dev_int *di = (struct  nk_net_ethernet_agent_net_dev_int *)(d->interface);
    DEBUG("send packet %p on %s (len=%lu, type=%lx)\n", packet, d->name,packet->len,type);
    switch (type) {
    case NK_DEV_REQ_CALLBACK:
	if (!di->post_send_packet) { 
	    DEBUG("packet send not possible\n");
	    return -1;
	} else {
	    return di->post_send_packet(d->state,packet,callback,state);
	}
	break;
    case NK_DEV_REQ_BLOCKING:
    case NK_DEV_REQ_NONBLOCKING:
	if (!di->post_send_packet) { 
	    DEBUG("packet send not possible\n");
	    return -1;
	} else {
	    volatile struct op o;

	    o.completed = 0;
	    o.status = 0;
	    o.dev = dev;
	    o.packet_dest = 0;

	    if (type==NK_DEV_REQ_NONBLOCKING) { 
		if (di->post_send_packet(d->state,packet,0,0)) { 
		    ERROR("Failed to launch send\n");
		    return -1;
		} else {
		    DEBUG("Packet launch started\n");
		    return 0;
		}
	    } else {
		if (di->post_send_packet(d->state,packet,generic_send_callback,(void*)&o)) { 
		    ERROR("Failed to launch send\n");
		    return -1;
		} else {
		    DEBUG("Packet launch started, waiting for completion\n");
		    while (!o.completed) {
			nk_dev_wait((struct nk_dev *)dev, generic_cond_check, (void*)&o);
		    }
		    DEBUG("Packet launch completed\n");
		    return o.status;
		}
	    }
	
	}
	break;
    default:
	return -1;
    }
}

int nk_net_ethernet_agent_device_receive_packet(struct nk_net_dev *dev,
						nk_ethernet_packet_t **packet,
						nk_dev_request_type_t type, 
						void (*callback)(nk_net_dev_status_t status,
								 nk_ethernet_packet_t *packet,
								 void *state), // for callback reqs
						void *state)
{
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
    struct nk_net_ethernet_agent_net_dev_int *di = (struct  nk_net_ethernet_agent_net_dev_int *)(d->interface);
    DEBUG("receive packet on %s (type=%lx)\n", d->name,type);
    
    switch (type) {
    case NK_DEV_REQ_CALLBACK:
	if (!di->post_receive_packet) { 
	    DEBUG("packet receive not possible\n");
	    return -1;
	} else {
	    return di->post_receive_packet(d->state,callback,state);
	}
	break;
    case NK_DEV_REQ_BLOCKING:
    case NK_DEV_REQ_NONBLOCKING:
	if (!di->post_receive_packet) { 
	    DEBUG("packet receive not possible\n");
	    return -1;
	} else {
	    volatile struct op o;

	    o.completed = 0;
	    o.status = 0;
	    o.dev = dev;
	    o.packet_dest = packet;

	    if (type==NK_DEV_REQ_NONBLOCKING) { 
		if (di->post_receive_packet(d->state,0,0)) { 
		    ERROR("Failed to post receive\n");
		    return -1;
		} else {
		    DEBUG("Packet receive posted\n");
		    return 0;
		}
	    } else {
		if (di->post_receive_packet(d->state,generic_receive_callback,(void*)&o)) { 
		    ERROR("Failed to post receive\n");
		    return -1;
		} else {
		    DEBUG("Packet receive posted, waiting for completion\n");
		    while (!o.completed) {
			nk_dev_wait((struct nk_dev *)d, generic_cond_check, (void*)&o);
		    }
		    DEBUG("Packet receive completed\n");
		    return o.status;
		}
	    }
	}
	break;
    default:
	return -1;
    }
}


int nk_net_ethernet_agent_start(struct nk_net_ethernet_agent *a)
{
    AGENT_LOCK_CONF;

    AGENT_LOCK(a);

    if (a->state!=STOPPED) {
	ERROR("Agent can only be started from the stopped state\n");
	AGENT_UNLOCK(a);
	return -1;
    }

    queue_receives(a);

    a->state=RUNNING;

    AGENT_UNLOCK(a);

    return 0;
}

int nk_net_ethernet_agent_stop(struct nk_net_ethernet_agent *a)
{
    AGENT_LOCK_CONF;

    AGENT_LOCK(a);

    if (a->state!=RUNNING) {
	ERROR("Agent can only be stopped from the running state\n");
	AGENT_UNLOCK(a);
	return -1;
    }

    a->state=STOPPED;

    AGENT_UNLOCK(a);

    return 0;
}

struct nk_net_dev *nk_net_ethernet_agent_get_underlying_device(struct nk_net_ethernet_agent *agent)
{
    return agent->netdev;
}

int  nk_net_ethernet_agent_init()
{
    uint64_t i;

    spinlock_init(&agent_list_lock);
    INIT_LIST_HEAD(&agent_list);

    spinlock_init(&op_free_list_lock);
    INIT_LIST_HEAD(&op_free_list);
    
    INFO("inited\n");

    return 0;
}

void nk_net_ethernet_agent_deinit()
{
    INFO("deinited\n");
}


