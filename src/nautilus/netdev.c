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
 * Copyright (c) 2016, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/dev.h>
#include <nautilus/netdev.h>

#ifndef NAUT_CONFIG_DEBUG_NETDEV
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("netdev: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("netdev: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("netdev: " fmt, ##args)

#if 0
static spinlock_t state_lock;

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);

#endif

int nk_net_dev_init()
{
    INFO("init\n");
    return 0;
}

int nk_net_dev_deinit()
{
    INFO("deinit\n");
    return 0;
}


struct nk_net_dev * nk_net_dev_register(char *name, uint64_t flags, struct nk_net_dev_int *inter, void *state)
{
    INFO("register device %s\n",name);
    return (struct nk_net_dev *) nk_dev_register(name,NK_DEV_NET,flags,(struct nk_dev_int *)inter,state);
}

int                   nk_net_dev_unregister(struct nk_net_dev *d)
{
    INFO("unregister device %s\n", d->dev.name);
    return nk_dev_unregister((struct nk_dev *)d);
}

struct nk_net_dev * nk_net_dev_find(char *name)
{
    DEBUG("find %s\n",name);
    struct nk_dev *d = nk_dev_find(name);
    if (d->type!=NK_DEV_NET) {
	DEBUG("%s not found\n",name);
	return 0;
    } else {
	DEBUG("%s found\n",name);
	return (struct nk_net_dev*) d;
    }
}

int nk_net_dev_get_characteristics(struct nk_net_dev *dev, struct nk_net_dev_characteristics *c)
{
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
    struct nk_net_dev_int *di = (struct nk_net_dev_int *)(d->interface);

    DEBUG("get characteristics of %s\n",d->name);
    return di->get_characteristics(d->state,c);
}


struct op {
    int                 completed;
    nk_net_dev_status_t status;
    struct nk_net_dev   *dev;
};


static void generic_send_callback(nk_net_dev_status_t status, void *context)
{
    struct op *o = (struct op*) context;
    DEBUG("generic send callback (status = 0x%lx) for %p\n",status,context);
    o->status = status;
    o->completed = 1;
    nk_dev_signal((struct nk_dev *)o->dev);
}

static void generic_receive_callback(nk_net_dev_status_t status, void *context)
{
    struct op *o = (struct op*) context;
    DEBUG("generic receive callback (status = 0x%lx) for %p\n", status, context);
    o->status = status;
    o->completed = 1;
    nk_dev_signal((struct nk_dev *)o->dev);
}

static int generic_cond_check(void* state)
{
    struct op *o = (struct op*) state;
    return o->completed;
}


int nk_net_dev_send_packet(struct nk_net_dev *dev, 
			   uint8_t *src, 
			   uint64_t len, 
			   nk_dev_request_type_t type,
			   void (*callback)(nk_net_dev_status_t status, void *state),
			   void *state)
{
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
    struct nk_net_dev_int *di = (struct nk_net_dev_int *)(d->interface);
    DEBUG("send packet on %s (len=%lu, type=%lx)\n", d->name,len,type);
    switch (type) {
    case NK_DEV_REQ_CALLBACK:
	if (!di->post_send) { 
	    DEBUG("packet send not possible\n");
	    return -1;
	} else {
	    return di->post_send(d->state,src,len,callback,state);
	}
	break;
    case NK_DEV_REQ_BLOCKING:
    case NK_DEV_REQ_NONBLOCKING:
	if (!di->post_send) { 
	    DEBUG("packet send not possible\n");
	    return -1;
	} else {
	    volatile struct op o;

	    o.completed = 0;
	    o.status = 0;
	    o.dev = dev;

	    if (type==NK_DEV_REQ_NONBLOCKING) { 
		if (di->post_send(d->state,src,len,0,0)) { 
		    ERROR("Failed to launch send\n");
		    return -1;
		} else {
		    DEBUG("Packet launch started\n");
		    return 0;
		}
	    } else {
		if (di->post_send(d->state,src,len,generic_send_callback,(void*)&o)) { 
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

int nk_net_dev_receive_packet(struct nk_net_dev *dev, 
			      uint8_t *dest, 
			      uint64_t len, 
			      nk_dev_request_type_t type,
			      void (*callback)(nk_net_dev_status_t status, void *state),
			      void *state)
{
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
    struct nk_net_dev_int *di = (struct nk_net_dev_int *)(d->interface);
    DEBUG("receive packet on %s (len=%lu, type=%lx)\n", d->name,len,type);
    
    switch (type) {
    case NK_DEV_REQ_CALLBACK:
	if (!di->post_receive) { 
	    DEBUG("packet receive not possible\n");
	    return -1;
	} else {
	    return di->post_receive(d->state,dest,len,callback,state);
	}
	break;
    case NK_DEV_REQ_BLOCKING:
    case NK_DEV_REQ_NONBLOCKING:
	if (!di->post_receive) { 
	    DEBUG("packet receive not possible\n");
	    return -1;
	} else {
	    volatile struct op o;

	    o.completed = 0;
	    o.status = 0;
	    o.dev = dev;

	    if (type==NK_DEV_REQ_NONBLOCKING) { 
		if (di->post_receive(d->state,dest,len,0,0)) { 
		    ERROR("Failed to post receive\n");
		    return -1;
		} else {
		    DEBUG("Packet receive posted\n");
		    return 0;
		}
	    } else {
		if (di->post_receive(d->state,dest,len,generic_receive_callback,(void*)&o)) { 
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
