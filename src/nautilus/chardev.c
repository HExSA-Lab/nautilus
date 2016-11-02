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
 * http://xtack.sandia.gov/hobbes
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
#include <nautilus/chardev.h>

#ifndef NAUT_CONFIG_DEBUG_CHARDEV
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("chardev: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("chardev: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("chardev: " fmt, ##args)

#if 0
static spinlock_t state_lock;

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);

#endif

int nk_char_dev_init()
{
    INFO("init\n");
    return 0;
}

int nk_char_dev_deinit()
{
    INFO("deinit\n");
    return 0;
}


struct nk_char_dev * nk_char_dev_register(char *name, uint64_t flags, struct nk_char_dev_int *inter, void *state)
{
    INFO("register device %s\n",name);
    return (struct nk_char_dev *) nk_dev_register(name,NK_DEV_CHAR,flags,(struct nk_dev_int *)inter,state);
}

int                   nk_char_dev_unregister(struct nk_char_dev *d)
{
    INFO("unregister device %s\n", d->dev.name);
    return nk_dev_unregister((struct nk_dev *)d);
}

struct nk_char_dev * nk_char_dev_find(char *name)
{
    DEBUG("find %s\n",name);
    struct nk_dev *d = nk_dev_find(name);
    if (d->type!=NK_DEV_CHAR) {
	DEBUG("%s not found\n",name);
	return 0;
    } else {
	DEBUG("%s found\n",name);
	return (struct nk_char_dev*) d;
    }
}

int nk_char_dev_get_characteristics(struct nk_char_dev *dev, struct nk_char_dev_characteristics *c)
{
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
    struct nk_char_dev_int *di = (struct nk_char_dev_int *)(d->interface);

    DEBUG("get characteristics of %s\n",d->name);
    return di->get_characteristics(d->state,c);
}


uint64_t nk_char_dev_read(struct nk_char_dev *dev, 
			  uint64_t count, 
			  uint8_t *dest, 
			  nk_dev_request_type_t type)
{
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
    struct nk_char_dev_int *di = (struct nk_char_dev_int *)(d->interface);
    DEBUG("read %s (count=%lu, type=%lx)\n", d->name,count,type);
    switch (type) {
    case NK_DEV_REQ_BLOCKING:
    case NK_DEV_REQ_NONBLOCKING:
	if (!di->read) { 
	    DEBUG("read not possible\n");
	    return -1;
	} else {
	    uint64_t num=0;
	    int err;
	    while (num<count) {
		err = di->read(d->state,dest);
		if (err < 0) { 
		    return -1;
		} else if (err==0) { 
		    if (type==NK_DEV_REQ_NONBLOCKING) { 
			return num;
		    } else {
			continue;
		    }
		} else {
		    num++;
		    dest++;
		}
	    }
	    return num;
	} 
	break;
    default:
	return -1;
    }
}


uint64_t nk_char_dev_write(struct nk_char_dev *dev, 
			   uint64_t count, 
			   uint8_t *src, 
			   nk_dev_request_type_t type)
{
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
    struct nk_char_dev_int *di = (struct nk_char_dev_int *)(d->interface);
    DEBUG("write %s (count=%lu, type=%lx)\n", d->name,count,type);
    switch (type) {
    case NK_DEV_REQ_BLOCKING:
    case NK_DEV_REQ_NONBLOCKING:
	if (!di->write) { 
	    DEBUG("write not possible\n");
	    return -1;
	} else {
	    uint64_t num=0;
	    int err;
	    while (num<count) {
		err = di->write(d->state,src);
		if (err < 0) { 
		    return -1;
		} else if (err==0) { 
		    if (type==NK_DEV_REQ_NONBLOCKING) { 
			return num;
		    } else {
			continue;
		    }
		} else {
		    num++;
		    src++;
		}
	    }
	    return num;
	} 
	break;
    default:
	return -1;
    }
}

