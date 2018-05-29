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
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/blkdev.h>
#include <dev/ramdisk.h>

#ifndef NAUT_CONFIG_DEBUG_RAMDISK
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("ramdisk: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("ramdisk: " fmt, ##args)
#define INFO(fmt, args...)  INFO_PRINT("ramdisk: " fmt, ##args)



#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK(state) _state_lock_flags = spin_lock_irq_save(&state->lock)
#define STATE_UNLOCK(state) spin_unlock_irq_restore(&(state->lock), _state_lock_flags)

struct ramdisk_state {
    struct nk_block_dev *blkdev;
    spinlock_t lock;
    uint64_t len;
    uint64_t block_size;
    uint64_t num_blocks;
    void     *data;
};


static int get_characteristics(void *state, struct nk_block_dev_characteristics *c)
{
    STATE_LOCK_CONF;
    struct ramdisk_state *s = (struct ramdisk_state *)state;
    
    STATE_LOCK(s);
    c->block_size = s->block_size;
    c->num_blocks = s->num_blocks;
    STATE_UNLOCK(s);
    return 0;
}

static int read_blocks(void *state, uint64_t blocknum, uint64_t count, uint8_t *dest,void (*callback)(nk_block_dev_status_t, void *), void *context)
{
    STATE_LOCK_CONF;
    struct ramdisk_state *s = (struct ramdisk_state *)state;

    DEBUG("read_blocks on device %s starting at %lu for %lu blocks\n",
	  s->blkdev->dev.name, blocknum, count);

    STATE_LOCK(s);
    if (blocknum+count >= s->num_blocks) { 
	STATE_UNLOCK(s);
	ERROR("Illegal access past end of disk\n");
	return -1;
    } else {
	memcpy(dest,s->data+blocknum*s->block_size,s->block_size*count);
	STATE_UNLOCK(s);
	//nk_dump_mem(dest,s->block_size*count);
	if (callback) {
	    callback(NK_BLOCK_DEV_STATUS_SUCCESS,context);
	}
	return 0;
    }
}

static int write_blocks(void *state, uint64_t blocknum, uint64_t count, uint8_t *src,void (*callback)(nk_block_dev_status_t, void *), void *context)
{
    STATE_LOCK_CONF;
    struct ramdisk_state *s = (struct ramdisk_state *)state;

    DEBUG("write_blocks on device %s starting at %lu for %lu blocks\n",
	  s->blkdev->dev.name, blocknum, count);

    STATE_LOCK(s);
    if (blocknum+count >= s->num_blocks) { 
	STATE_UNLOCK(s);
	ERROR("Illegal access past end of disk\n");
	return -1;
    } else {
	memcpy(s->data+blocknum*s->block_size,src,s->block_size*count);
	STATE_UNLOCK(s);
	if (callback) { 
	    callback(NK_BLOCK_DEV_STATUS_SUCCESS,context);
	}
	return 0;
    }
}



static struct nk_block_dev_int inter = 
{
    .get_characteristics = get_characteristics,
    .read_blocks = read_blocks,
    .write_blocks = write_blocks,
};

static int discover_ramdisks()
{
    int i=0;
    
    // this should do real discovery, but currently the only way to
    // include a ramdisk is with the embedded image
#if NAUT_CONFIG_RAMDISK_EMBED
    extern int __RAMDISK_START, __RAMDISK_END;

    struct ramdisk_state *s = malloc(sizeof(*s));
    
    if (!s) { 
	ERROR("Cannot allocate data structure for ramdisk\n");
	return -1;
    }

    memset(s,0,sizeof(*s));
    
    spinlock_init(&s->lock);
    
    s->block_size=RAMDISK_DEFAULT_BLOCK_SIZE;
    s->len = (((uint64_t)(&__RAMDISK_END)) - ((uint64_t)(&__RAMDISK_START)));
    s->num_blocks = s->len / s->block_size;
    s->data = &__RAMDISK_START;

    s->blkdev = nk_block_dev_register("ramdisk0", 0, &inter, s);

    if (!s->blkdev) {
	ERROR("Failed to register ramdisk\n");
	free(s);
	return -1;
    } 

    INFO("Added embedded image as ramdisk0, blocksize=%lu, numblocks=%lu, len=%lu\n",
	 s->block_size,s->num_blocks, s->len);

#endif
    return 0;
}

int nk_ramdisk_init(struct naut_info *naut)
{
    INFO("init\n");
    return discover_ramdisks();
}

int nk_ramdisk_deinit()
{
    INFO("deinit\n");
    return 0;
}




