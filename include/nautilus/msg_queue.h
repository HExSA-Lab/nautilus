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
 * Copyright (c) 2018, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __MSG_QUEUE_H__
#define __MSG_QUEUE_H__

// Message queus are intended for threads
// Interrupts should only use "try_push" and "try_pull"
// And you probably do not want to use message queues at all
// in interrupt context unless you know what you are doing

typedef enum { NK_MSG_QUEUE_DEFAULT=0 } nk_msg_queue_type_t;

#define NK_MSG_QUEUE_NAME_LEN 32

// name is optional, currently there is only one mq type and no characteristics
struct nk_msg_queue *nk_msg_queue_create(char *name,
					 uint64_t size,
					 nk_msg_queue_type_t type,
					 void *type_characteristics);

// msg queues are refcounted - creates, finds, and acquire/release
// change refcounts, release deletes the message queue when the refcount
// goes to zero

struct nk_msg_queue *nk_msg_queue_find(char *name);
void                 nk_msg_queue_attach(struct nk_msg_queue *queue);

void                 nk_msg_queue_release(struct nk_msg_queue *queue);

//
int   nk_msg_queue_full(struct nk_msg_queue *queue);
int   nk_msg_queue_empty(struct nk_msg_queue *queue);

// blocks
void  nk_msg_queue_push(struct nk_msg_queue *queue, void *msg);
void  nk_msg_queue_pull(struct nk_msg_queue *queue, void **msg);

// returns 0 on success - does not block
int  nk_msg_queue_try_push(struct nk_msg_queue *queue, void *msg);
int  nk_msg_queue_try_pull(struct nk_msg_queue *queue, void **msg);

// returns 0 on success, >0 on timeout
// blocks for up to timeout_ns
int  nk_msg_queue_push_timeout(struct nk_msg_queue *queue, void *msg, uint64_t timeout_ns);
int  nk_msg_queue_pull_timeout(struct nk_msg_queue *queue, void **msg, uint64_t timeout_ns);

int  nk_msg_queue_init();
void nk_msg_queue_deinit();

void nk_msg_queue_dump_queues();

#endif
