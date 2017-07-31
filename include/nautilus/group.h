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
 * Copyright (c) 2017, Xiaoyang Wang <xiaoyangwang2018@u.northwestern.edu>
 *                     Jinghang Wang`<jinghangwang2018@u.northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Xiaoyang Wang <xiaoyangwang2018@u.northwestern.edu>
 *          Jinghang Wang <jinghangwang2018@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef _GROUP_H_
#define _GROUP_H_

#define MAX_GROUP_NAME 32

typedef struct nk_thread_group nk_thread_group_t;

// init of module
int nk_thread_group_init(void);

// deinit of module
int nk_thread_group_deinit(void);

// creating a thread group is done as easily as making a name
nk_thread_group_t *nk_thread_group_create(char *name);

// attach a state to the group
int nk_thread_group_attach_state(nk_thread_group_t *group, void *state);

// detach the state of a group
void *nk_thread_group_detach_state(nk_thread_group_t *group);

// recover the state of a group
void *nk_thread_group_get_state(nk_thread_group_t *group);

// search for a thread group by name
nk_thread_group_t *nk_thread_group_find(char *name);

// current thread joins a group
int nk_thread_group_join(nk_thread_group_t *group);

// current thread leaves a group
int nk_thread_group_leave(nk_thread_group_t *group);

// all threads in the group call to synchronize
int nk_thread_group_barrier(nk_thread_group_t *group);

// all threads in the group call to select one thread as leader
int nk_thread_group_election(nk_thread_group_t *group);

// reset leader
int nk_thread_group_reset_leader(nk_thread_group_t *group);

//  check if I'm the leader
int nk_thread_group_check_leader(nk_thread_group_t *group);

// broadcast a message to all members of the thread group
int nk_thread_group_broadcast(nk_thread_group_t *group, void *message, uint64_t tid, uint64_t src);

// terminate the bcast, then nobody will be waiting for sending or recieving
int nk_thread_group_broadcast_terminate(nk_thread_group_t *group);

// delete a group (should be empty)
int nk_thread_group_delete(nk_thread_group_t *group);

// return the size of a group
uint64_t nk_thread_group_get_size(nk_thread_group_t *group);

#endif /* _GROUP_H */
