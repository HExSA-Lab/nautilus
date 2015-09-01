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
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __NEMO_H__
#define __NEMO_H__


#define NEMO_MAX_EVENTS 1024
#define NEMO_INT_VEC    0xe8


typedef void (*nemo_action_t)(void);

typedef int nemo_event_id_t;

typedef struct nemo_event {
	nemo_action_t   action;
	nemo_event_id_t id;
	void *          priv_data;
} nemo_event_t;


int nemo_init(void);
nemo_event_id_t nemo_register_event_action(void (*func)(void), void * priv_data);
void nemo_unregister_event_action(nemo_event_id_t eid);
void nemo_event_notify(nemo_event_id_t eid, int cpu);
void nemo_event_broadcast(nemo_event_id_t eid);
void nemo_event_await(void);




#endif /* !__NEMO_H__! */
