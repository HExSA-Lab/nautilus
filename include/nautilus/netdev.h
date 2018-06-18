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

#ifndef __NET_DEV
#define __NET_DEV

#include <nautilus/dev.h>

#define ETHER_MAC_LEN 6

struct nk_net_dev_characteristics {
    uint8_t  mac[ETHER_MAC_LEN];
    uint64_t min_tu;
    uint64_t max_tu;
    uint64_t (*packet_size_to_buffer_size)(uint64_t packet_size);
};


typedef enum {
    NK_NET_DEV_STATUS_SUCCESS=0,
    NK_NET_DEV_STATUS_ERROR
} nk_net_dev_status_t;

struct nk_net_dev_int {
    // this must be first so it derives cleanly
    // from nk_dev_int
    struct nk_dev_int dev_int;
    
    // netdev-specific interface - set to zero if not available
    // an interface either succeeds (returns zero) or fails (returns -1)
    int (*get_characteristics)(void *state, struct nk_net_dev_characteristics *c);
    // send/receive are non-blocking always.  -1 on error, otherwise return 0
    // callback can be null
    int (*post_receive)(void *state, uint8_t *dest, uint64_t len, void (*callback)(nk_net_dev_status_t status, void *context), void *context);
    int (*post_send)(void *state, uint8_t *src, uint64_t len, void (*callback)(nk_net_dev_status_t status, void *context), void *context);
};


struct nk_net_dev {
    // must be first member 
    struct nk_dev dev;
};

int nk_net_dev_init();
int nk_net_dev_deinit();

struct nk_net_dev * nk_net_dev_register(char *name, uint64_t flags, struct nk_net_dev_int *inter, void *state); 
int                 nk_net_dev_unregister(struct nk_net_dev *);

struct nk_net_dev * nk_net_dev_find(char *name);


int nk_net_dev_get_characteristics(struct nk_net_dev *d, struct nk_net_dev_characteristics *c);

int nk_net_dev_receive_packet(struct nk_net_dev *dev, 
			      uint8_t *dest,
			      uint64_t len,
			      nk_dev_request_type_t type, 
			      void (*callback)(nk_net_dev_status_t status, 
					       void *state), // for callback reqs
			      void *state);                 // for callback reqs

int nk_net_dev_send_packet(struct nk_net_dev *dev, 
			   uint8_t *src,
			   uint64_t len,
			   nk_dev_request_type_t type,
			   void (*callback)(nk_net_dev_status_t status, 
                                            void *state),  // for callback reqs
			   void *state);                  // for callback reqs


#endif

