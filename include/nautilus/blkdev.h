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

#ifndef __BLK_DEV
#define __BLK_DEV

#include <nautilus/dev.h>

struct nk_block_dev_characteristics {
    uint64_t block_size;
    uint64_t num_blocks;
};


typedef enum {
    NK_BLOCK_DEV_STATUS_SUCCESS=0,
    NK_BLOCK_DEV_STATUS_ERROR
} nk_block_dev_status_t;

struct nk_block_dev_int {
    // this must be first so it derives cleanly
    // from nk_dev_int
    struct nk_dev_int dev_int;
    
    // blockdev-specific interface - set to zero if not available
    // an interface either succeeds (returns zero) or fails (returns -1) 
    // in any case, it returns immediately
    int (*get_characteristics)(void *state, struct nk_block_dev_characteristics *c);
    int (*read_blocks)(void *state, uint64_t blocknum, uint64_t count, uint8_t *dest, void (*callback)(nk_block_dev_status_t status, void *context), void *context);
    int (*write_blocks)(void *state, uint64_t blocknum, uint64_t count, uint8_t *src, void (*callback)(nk_block_dev_status_t status, void *context), void *context);
};


struct nk_block_dev {
    // must be first member 
    struct nk_dev dev;
};

int nk_block_dev_init();
int nk_block_dev_deinit();

struct nk_block_dev * nk_block_dev_register(char *name, uint64_t flags, struct nk_block_dev_int *inter, void *state); 
int                   nk_block_dev_unregister(struct nk_block_dev *);

struct nk_block_dev * nk_block_dev_find(char *name);


int nk_block_dev_get_characteristics(struct nk_block_dev *d, struct nk_block_dev_characteristics *c);

int nk_block_dev_read(struct nk_block_dev *dev, 
		      uint64_t blocknum, 
		      uint64_t count, 
		      void   *dest, 
		      nk_dev_request_type_t type,
		      void (*callback)(nk_block_dev_status_t status, void *state), 
		      void *state);

int nk_block_dev_write(struct nk_block_dev *dev, 
		       uint64_t blocknum, 
		       uint64_t count, 
		       void   *src,  
		       nk_dev_request_type_t type,
		       void (*callback)(nk_block_dev_status_t status, void *state), 
		       void *state);



#endif

