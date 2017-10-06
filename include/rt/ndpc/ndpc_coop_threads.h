//
// Copyright (c) 2017 Peter Dinda  All Rights Reserved
//


#ifndef _npdc_coop_threads_
#define _ndpc_coop_threads_

/*
  cooperative-threading computational model
  
  - thread can be launched on any processor
  - thread can be pinned or mobile
  - mobile thread processor binding determined by RT
  - binding changes only during a yield(), if at all
  - no locks, initially, instead
    local_crit_section - only me on this processor
    global critical section - only me on all processors
  - cooperative thread cannot do anything blocking, 
    except barriers and global sync

*/

// Opaque type for use by user
typedef void *thread_id_t;
// binding of thread to processor => -1 => not bound
typedef int proc_bind_t; 
// thread returns error code
typedef int (*thread_func_t)(void *input,
			     void *output);
// 0 = system decides stack size
typedef unsigned stack_size_t; 
// invoke init on all processors
int ndpc_init_coop_threads();

// The created thread will NOT begin running
// if it is created on the same processor
int ndpc_create_coop_thread(thread_func_t func,
			    void *input,
			    void *output,
			    proc_bind_t proc_bind,
			    stack_size_t stack_size,
			    thread_id_t *thread_id);

// Perhaps we should have an explicit thread fork... 
// but with the assumption that the child must not return? 
//int ndpc_fork_preempt_thread(proc_bind_t proc_bind,
//			     stack_size_t stack_size,
//			     thread_id_t *thread_id);

int ndpc_exit_coop_thread(int result);


int ndpc_join_coop_thread(thread_id_t *thread_id);


// Minimal support for locking since we expect this
// will be infrequent
// no preemption on this core, other cores stop
// on a yield
int ndpc_enter_global_critical_section();
int ndpc_leave_global_critical_section();

int ndpc_deinit_coop_threads();


#endif
