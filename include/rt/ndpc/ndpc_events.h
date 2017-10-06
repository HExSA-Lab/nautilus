//
// Copyright (c) 2017 Peter Dinda  All Rights Reserved
//


#ifndef _npdc_events_
#define _ndpc_events_

/*
  event-driven computational model
 
  - event callback can be invoked on any processor
  - a callback runs to completion without preemption on the processor
  - callback can register new event

*/

// Opaque type for use by user
typedef unsigned long long event_type_t;
// Deadline in absolute cycle count
typedef unsigned long long event_deadline_t
// callback function - returns nonzero if error
typedef int (*event_callback_t)(event_type_t et,
				event_deadline_t et,
				void *input,
				void *output);

// invoke init on all processors
int ndpc_init_events();

// Current time
event_deadline_t ndpc_now();

// queue an event either before starting
// the event system or in a callback
int ndpc_queue_event(event_type_t     et,
		     event_deadline_t ed,
		     event_callback_t cb);

// The event system main loop - invoke on every processor 
// after at least one  event is queued.  
// Returns after no more events are available
int npdc_run_events();

// Teardown - call on every processor
int ndpc_deinit_events();


#endif
