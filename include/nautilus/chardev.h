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

#ifndef __CHAR_DEV
#define __CHAR_DEV

#include <nautilus/dev.h>

struct nk_char_dev_characteristics {
    // currently none
};

struct nk_char_dev_int {
    // this must be first so it derives cleanly
    // from nk_dev_int
    struct nk_dev_int dev_int;
    
    // chardev-specific interface - set to zero if not available
    // either succeeds (returns zero) or fails (returns -1)
    int (*get_characteristics)(void *state, struct nk_char_dev_characteristics *c); 
    // returns 1 on success, 0 for would block, -1 for error
    // must be non-blocking
    int (*read)(void *state, uint8_t *dest);
    int (*write)(void *state, uint8_t *src);
    // returns whether device is currently readable or writeable or both
    // or in error state
#define NK_CHARDEV_READABLE  1
#define NK_CHARDEV_WRITEABLE 2
#define NK_CHARDEV_ERROR     4
    int (*status)(void *state);
};


struct nk_char_dev {
    // must be first member 
    struct nk_dev dev;
};

int nk_char_dev_init();
int nk_char_dev_deinit();

struct nk_char_dev * nk_char_dev_register(char *name, uint64_t flags, struct nk_char_dev_int *inter, void *state); 
int                  nk_char_dev_unregister(struct nk_char_dev *);

struct nk_char_dev * nk_char_dev_find(char *name);


int nk_char_dev_get_characteristics(struct nk_char_dev *d, struct nk_char_dev_characteristics *c);

// returns number read/written with -1 signifying error
uint64_t nk_char_dev_read(struct nk_char_dev *dev, 
			  uint64_t count,
			  uint8_t *dest,
			  nk_dev_request_type_t type);

// returns number read/written with -1 signifying error
uint64_t nk_char_dev_write(struct nk_char_dev *dev, 
			   uint64_t count,
			   uint8_t *src, 
			   nk_dev_request_type_t type);

// returns combination of readable/writeable/error
// no guarantee that this status will not change
int nk_char_dev_status(struct nk_char_dev *dev);

#endif

