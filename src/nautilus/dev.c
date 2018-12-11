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
#include <nautilus/spinlock.h>
#include <nautilus/dev.h>
#include <nautilus/thread.h>
#include <nautilus/waitqueue.h>
#include <nautilus/shell.h>

#ifndef NAUT_CONFIG_DEBUG_DEV
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("dev: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("dev: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("dev: " fmt, ##args)

static spinlock_t state_lock;

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);

static struct list_head dev_list;


int nk_dev_init()
{
    INIT_LIST_HEAD(&dev_list);
    spinlock_init(&state_lock);
    INFO("devices inited\n");
    return 0;
}

int nk_dev_deinit()
{
    if (!list_empty(&dev_list)) { 
	ERROR("Extant devices on deinit\n");
	return -1;
    }
    spinlock_deinit(&state_lock);
    INFO("device deinit\n");
    return 0;
}


struct nk_dev *nk_dev_register(char *name, nk_dev_type_t type, uint64_t flags, struct nk_dev_int *inter, void *state)
{
    STATE_LOCK_CONF;
    struct nk_dev *d = malloc(sizeof(*d));
    char buf[NK_WAIT_QUEUE_NAME_LEN];
    
    if (!d) {
	ERROR("Failed to allocate device\n");
	return 0;
    }
    
    memset(d,0,sizeof(*d));

    snprintf(buf,NK_WAIT_QUEUE_NAME_LEN,"%s-wait", name);
    d->waiting_threads = nk_wait_queue_create(buf);
    
    if (!d->waiting_threads) { 
	ERROR("Failed to allocate wait queue\n");
	free(d);
	return 0;
    }

    strncpy(d->name,name,DEV_NAME_LEN); d->name[DEV_NAME_LEN-1] = 0;
    d->type = type;
    d->flags = flags;
    d->state = state;
    d->interface = inter;

    STATE_LOCK();
    list_add(&d->dev_list_node,&dev_list);
    STATE_UNLOCK();
    
    INFO("Added device with name %s, type %lu, flags 0x%lx\n", d->name, d->type,d->flags);
    
    return d;
}

int            nk_dev_unregister(struct nk_dev *d)
{
    STATE_LOCK_CONF;
    
    STATE_LOCK();
    list_del(&d->dev_list_node);
    STATE_UNLOCK();

    nk_wait_queue_wake_all(d->waiting_threads);
    nk_wait_queue_destroy(d->waiting_threads);
    INFO("Unregistered device %s\n",d->name);
    free(d);
    return 0;
}

struct nk_dev *nk_dev_find(char *name)
{
    struct list_head *cur;
    struct nk_dev *target=0;
    STATE_LOCK_CONF;
    STATE_LOCK();
    list_for_each(cur,&dev_list) {
	if (!strncasecmp(list_entry(cur,struct nk_dev,dev_list_node)->name,name,DEV_NAME_LEN)) { 
	    target = list_entry(cur,struct nk_dev, dev_list_node);
	    break;
	}
    }
    STATE_UNLOCK();
    return target;
}



void nk_dev_wait(struct nk_dev *d,
		 int (*cond_check)(void *state),
		 void *state)
{
    if (get_cpu()->interrupt_nesting_level) { 
	// We are in an interrupt context, and 
	// we cannot wait...
	return;
    } else {
	// We are in a thread context and we will
	// put ourselves to sleep
	nk_wait_queue_sleep_extended(d->waiting_threads, cond_check, state);
    }
}

void nk_dev_signal(struct nk_dev *d)
{
    //    ERROR_PRINT("dev signal d->waiting_threads->lock=%d\n",d->waiting_threads->lock);
    nk_wait_queue_wake_all(d->waiting_threads);
}

void nk_dev_dump_devices()
{
    struct list_head *cur;
    STATE_LOCK_CONF;
    STATE_LOCK();
    list_for_each(cur,&dev_list) {
	struct nk_dev *d = list_entry(cur,struct nk_dev, dev_list_node);
	nk_vc_printf("%s: %s flags=0x%lx interface=%p state=%p\n",
		     d->name, 
		     d->type==NK_DEV_GENERIC ? "generic" : 
		     d->type==NK_DEV_BUS ? "bus" : 
		     d->type==NK_DEV_TIMER ? "timer" : 
		     d->type==NK_DEV_INTR ? "interrupt" : 
		     d->type==NK_DEV_CHAR ? "char" : 
		     d->type==NK_DEV_BLK ? "block" :
		     d->type==NK_DEV_NET ? "net" : "unknown", 
		     d->flags,
		     d->interface,
		     d->state);
		     
    }
    STATE_UNLOCK();
}


static int
handle_devs (char * buf, void * priv)
{
    nk_dev_dump_devices();
    return 0;
}


static struct shell_cmd_impl devs_impl = {
    .cmd      = "devs",
    .help_str = "devs",
    .handler  = handle_devs,
};
nk_register_shell_cmd(devs_impl);
