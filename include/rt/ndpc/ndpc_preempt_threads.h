//
// Copyright (c) 2017 Peter Dinda  All Rights Reserved
//


#ifndef _ndpc_preempt_threads_
#define _ndpc_preempt_threads_

/*
  preemptive-threading computational model
  
  This is basically a pthread or a preemptive kernel thread
 
  - thread can be launched on any processor
  - thread can be pinned or mobile
  - mobile thread processor binding determined by RT
  - no locks, initially, instead
    local_crit_section - only me on this processor
    global critical section - only me on all processors

*/

// Opaque type for use by user
typedef void *thread_id_t;
// binding of thread to processor => -1 => not bound
typedef int proc_bind_t; 
// thread returns error code
typedef int (*thread_func_t)(void *input,
			     void **output);
// 0 = system decides stack size
typedef unsigned stack_size_t; 
// invoke init on all processors
int ndpc_init_preempt_threads();

int ndpc_create_preempt_thread(thread_func_t func,
			       void *input,
			       void **output,
			       proc_bind_t proc_bind,
			       stack_size_t stack_size,
			       thread_id_t *thread_id);

// zero => error
// positive => thread_id_t, 
//   if == ndpc_my_preempt_thead() then you are the child
//   if != then you are the parent, and this tid is the child tid
//
// when the child returns, it is over.  
//   If the child wants to return a value, it can call ndpc_set_result_of_forked_prempt_thread() first
//
thread_id_t ndpc_fork_preempt_thread();

//
// Yield to another thread explicitly
//
void ndpc_yield_preempt_thread();

// Get own thread id
thread_id_t ndpc_my_preempt_thread();

// Get parent's thread id
thread_id_t ndpc_my_parent_preempt_thread();

// called by a forked thread
int ndpc_set_result_of_forked_preempt_thread(void *result);

// Returns output
void *ndpc_join_preempt_thread(thread_id_t thread_id);


// Wait for any threads forked or launched by the caller
void ndpc_join_child_preempt_threads();


// Minimal support for locking since we expect this
// will be infrequent

// All threads barrier
int ndpc_barrier();

// no preemption on this core, other cores continue
int ndpc_enter_local_critical_section();
int ndpc_leave_local_critical_section();

// no preemption on this core, other cores stop
int ndpc_enter_global_critical_section();
int ndpc_leave_global_critical_section();


int ndpc_deinit_preempt_threads();


#endif
