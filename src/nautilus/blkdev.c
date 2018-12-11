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
#include <nautilus/blkdev.h>
#include <nautilus/shell.h>

#ifndef NAUT_CONFIG_DEBUG_BLKDEV
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("blkdev: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("blkdev: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("blkdev: " fmt, ##args)

#if 0
static spinlock_t state_lock;

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);

#endif

int nk_block_dev_init()
{
    INFO("init\n");
    return 0;
}

int nk_block_dev_deinit()
{
    INFO("deinit\n");
    return 0;
}


struct nk_block_dev * nk_block_dev_register(char *name, uint64_t flags, struct nk_block_dev_int *inter, void *state)
{
    INFO("register device %s\n",name);
    return (struct nk_block_dev *) nk_dev_register(name,NK_DEV_BLK,flags,(struct nk_dev_int *)inter,state);
}

int                   nk_block_dev_unregister(struct nk_block_dev *d)
{
    INFO("unregister device %s\n", d->dev.name);
    return nk_dev_unregister((struct nk_dev *)d);
}

struct nk_block_dev * nk_block_dev_find(char *name)
{
    DEBUG("find %s\n",name);
    struct nk_dev *d = nk_dev_find(name);
    if (d->type!=NK_DEV_BLK) {
	DEBUG("%s not found\n",name);
	return 0;
    } else {
	DEBUG("%s found\n",name);
	return (struct nk_block_dev*) d;
    }
}

int nk_block_dev_get_characteristics(struct nk_block_dev *dev, struct nk_block_dev_characteristics *c)
{
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
    struct nk_block_dev_int *di = (struct nk_block_dev_int *)(d->interface);

    DEBUG("get characteristics of %s\n",d->name);
    return di->get_characteristics(d->state,c);
}

struct op {
    int                 completed;
    nk_block_dev_status_t status;
    struct nk_block_dev   *dev;
};


static void generic_write_callback(nk_block_dev_status_t status, void *context)
{
    struct op *o = (struct op*) context;
    DEBUG("generic write callback (status = 0x%lx) for %p\n",status,context);
    o->status = status;
    o->completed = 1;
    nk_dev_signal((struct nk_dev *)o->dev);
}

static void generic_read_callback(nk_block_dev_status_t status, void *context)
{
    struct op *o = (struct op*) context;
    DEBUG("generic read callback (status = 0x%lx) for %p\n", status, context);
    o->status = status;
    o->completed = 1;
    nk_dev_signal((struct nk_dev *)o->dev);
}

static int generic_cond_check(void* state)
{
    struct op *o = (struct op*) state;
    return o->completed;
}


int nk_block_dev_read(struct nk_block_dev *dev, 
		      uint64_t blocknum, 
		      uint64_t count, 
		      void *dest, 
		      nk_dev_request_type_t type,
		      void (*callback)(nk_block_dev_status_t status, void *state),
		      void *state)
{
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
    struct nk_block_dev_int *di = (struct nk_block_dev_int *)(d->interface);
    DEBUG("read %s (start=%lu, count=%lu, type=%lx)\n", d->name,blocknum,count,type);
    switch (type) {
    case NK_DEV_REQ_CALLBACK:
	if (!di->read_blocks) { 
	    DEBUG("readblocks not possible\n");
	    return -1;
	} else {
	    return di->read_blocks(d->state,blocknum,count,dest,callback,state);
	}
	break;
    case NK_DEV_REQ_BLOCKING:
    case NK_DEV_REQ_NONBLOCKING: {
	if (!di->read_blocks) { 
	    DEBUG("readblocks is not possible\n");
	    return -1;
	} else {
	    volatile struct op o;

	    o.completed = 0;
	    o.status = 0;
	    o.dev = dev;
	    
	    if (type==NK_DEV_REQ_NONBLOCKING) {
		if (di->read_blocks(d->state,blocknum,count,dest,0,0)) {
		    ERROR("failed to start up readblocks\n");
		    return -1;
		} else {
		    DEBUG("readblocks started\n");
		    return 0;
		}
	    } else {
		if (di->read_blocks(d->state,blocknum,count,dest,generic_read_callback,(void*)&o)) {
		    ERROR("failed to start up readblocks\n");
		    return -1;
		} else {
		    DEBUG("readblocks started, waiting for completion\n");
		    while (!o.completed) { 
			nk_dev_wait((struct nk_dev *)d,generic_cond_check,(void*)&o);
		    }
		    return 0;
		}
	    }
	}
    }
	break;
    default:
	return -1;
    }
}


int nk_block_dev_write(struct nk_block_dev *dev, 
		       uint64_t blocknum, 
		       uint64_t count, 
		       void     *src,  
		       nk_dev_request_type_t type,
		       void (*callback)(nk_block_dev_status_t status, void *state),
		       void *state)
{
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
    struct nk_block_dev_int *di = (struct nk_block_dev_int *)(d->interface);
    DEBUG("write %s (start=%lu, count=%lu, type=%lx\n", d->name,blocknum,count,type);
    switch (type) {
    case NK_DEV_REQ_CALLBACK:
	if (!di->write_blocks) { 
	    DEBUG("writeblocks not possible\n");
	    return -1;
	} else {
	    return di->write_blocks(d->state,blocknum,count,src,callback,state);
	}
	break;
    case NK_DEV_REQ_BLOCKING:
    case NK_DEV_REQ_NONBLOCKING: {
	if (!di->write_blocks) { 
	    DEBUG("writeblocks is not possible\n");
	    return -1;
	} else {
	    volatile struct op o;

	    o.completed = 0;
	    o.status = 0;
	    o.dev = dev;
    
	    if (type==NK_DEV_REQ_NONBLOCKING) {
		if (di->write_blocks(d->state,blocknum,count,src,0,0)) {
		    ERROR("failed to start up writeblocks\n");
		    return -1;
		} else {
		    DEBUG("readblocks started\n");
		    return 0;
		}
	    } else {
		if (di->write_blocks(d->state,blocknum,count,src,generic_read_callback,(void*)&o)) {
		    ERROR("failed to start up writeblocks\n");
		    return -1;
 		} else {
		    DEBUG("writeblocks started, waiting for completion\n");
		    while (!o.completed) { 
			nk_dev_wait((struct nk_dev *)d, generic_cond_check, (void*)&o);
		    }
		    return 0;
		}
	    }
	}
    }
	break;
    default:
	return -1;
    }

}

static int 
handle_blktest (char * buf, void * priv)
{
    char name[32], rw[16];
    uint64_t start, count;
    struct nk_block_dev *d;
    struct nk_block_dev_characteristics c;

    if ((sscanf(buf,"blktest %s %s %lu %lu",name,rw,&start,&count)!=4)
            || (*rw!='r' && *rw!='w') ) { 
        nk_vc_printf("Don't understand %s\n",buf);
        return -1;
    }

    if (!(d=nk_block_dev_find(name))) { 
        nk_vc_printf("Can't find %s\n",name);
        return -1;
    }

    if (nk_block_dev_get_characteristics(d,&c)) { 
        nk_vc_printf("Can't get characteristics of %s\n",name);
        return -1;
    }

    char data[c.block_size+1];
    uint64_t i,j;


    for (i=start;i<start+count;i++) { 
        if (*rw == 'w') { 
            for (j=0;j<c.block_size;j++) { 
                data[j] = "abcdefghijklmnopqrstuvwxyz0123456789"[j%36];
            }
            if (nk_block_dev_write(d,i,1,data,NK_DEV_REQ_BLOCKING,0,0)) {
                nk_vc_printf("Failed to write block %lu\n",i);
                return -1;
            }
        } else if (*rw == 'r') {
            if (nk_block_dev_read(d,i,1,data,NK_DEV_REQ_BLOCKING,0,0)) {
                nk_vc_printf("Failed to read block %lu\n",i);
                return -1;
            }
            data[c.block_size] = 0;
            nk_vc_printf("%s\n",data);
        }
    }
    return 0;
}


static struct shell_cmd_impl blktest_impl = {
    .cmd      = "blktest",
    .help_str = "blktest dev r|w start count",
    .handler  = handle_blktest,
};
nk_register_shell_cmd(blktest_impl);
