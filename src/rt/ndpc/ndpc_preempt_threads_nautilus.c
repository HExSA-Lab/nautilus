//
// Copyright (c) 2017 Peter Dinda  All Rights Reserved
//

#include <nautilus/nautilus.h>
#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/barrier.h>
#include <nautilus/scheduler.h>

#include <rt/ndpc/ndpc_preempt_threads.h>


#ifndef NAUT_CONFIG_NDPC_RT_DEBUG
#define DEBUG(fmt, args...)
#else
#define DEBUG(fmt, args...) DEBUG_PRINT("ndpc: " fmt, ##args)
#endif

#define ERROR(fmt, args...) ERROR_PRINT("ndpc: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("ndpc: " fmt, ##args)

/*
  This is glue code to interface the ndpc interface for 
  preemptive threads to the Nautilus interface

*/


static struct global_state {
    nk_barrier_t barrier;
} global;

int ndpc_init_preempt_threads()
{
    DEBUG("Init preempt threads\n");
    nk_barrier_init(&global.barrier,nk_get_num_cpus());
    return 0;
}

int ndpc_create_preempt_thread(thread_func_t func,
			       void *input,
			       void **output,
			       proc_bind_t proc_bind,
			       stack_size_t stack_size,
			       thread_id_t *thread_id)

{
    DEBUG("create_preempt_thread\n");
    return nk_thread_start((nk_thread_fun_t) func,
			   input,
			   output,
			   0, // is not detached ?
			   stack_size,
			   thread_id,
			   proc_bind);
}

// ndpc_fork_preempt_thread() implemented in the lowlevel file

void ndpc_yield_preempt_thread()
{
    DEBUG("yield_preempt_thread\n");
    nk_yield();
}

// Get own thread id
thread_id_t ndpc_my_preempt_thread()
{
    DEBUG("my_preempt_thread()=%lu\n",nk_get_tid());
    return nk_get_tid();
}

// Get parent's thread id
thread_id_t ndpc_my_parent_preempt_thread()
{
    DEBUG("my_parent_preeempt_thread()=%lu\n",nk_get_parent_tid());
    return nk_get_parent_tid();
}

// called by a forked thread
int ndpc_set_result_of_forked_preempt_thread(void *result)
{
    DEBUG("set_result_of_forked_preeempt_thread(%p)\n",result);
    nk_set_thread_output(result);
    return 0;
}

// Returns output
void *ndpc_join_preempt_thread(thread_id_t thread_id)    
{
    void *out;

    DEBUG("join_preempt_thread(%lu)\n", thread_id);
    if (nk_join(thread_id, &out)) {
	DEBUG("join_preempt_thread(%lu) failed, returning null\n", thread_id);
	return 0;
    } else {
	DEBUG("join_preempt_thread(%lu) succeeded, returning %p\n", thread_id,out);
	return out;
    }
}

static int _fork_join_cb(void *out)
{
    return 0;
}

// Wait for any threads forked or launched by the caller
void ndpc_join_child_preempt_threads()
{
    DEBUG("join_child_preempt_threads() start\n");
    nk_join_all_children(_fork_join_cb);
    DEBUG("join_child_preempt_threads() end\n");
}

// Minimal support for locking since we expect this
// will be infrequent

// All threads barrier
int ndpc_barrier()
{
    DEBUG("barrier start\n");
    nk_barrier_wait(&global.barrier);
    DEBUG("barrier end\n");
    return 0;
}

// no preemption on this core, other cores continue
int ndpc_enter_local_critical_section()
{
    ERROR("ndpc_enter_local_critical_section NOT IMPLEMENTED\n");
    return -1; // need to implement

}
int ndpc_leave_local_critical_section()
{
    ERROR("ndpc_leave_local_critical_section NOT IMPLEMENTED\n");
    return -1; // need to implement

}

// no preemption on this core, other cores stop
int ndpc_enter_global_critical_section()
{
    ERROR("ndpc_enter_global_critical_section NOT IMPLEMENTED\n");
    return -1; // need to implement
}

int ndpc_leave_global_critical_section() 
{
    ERROR("ndpc_leave_global_critical_section NOT IMPLEMENTED\n");
    return -1; // need to implement
}


int ndpc_deinit_preempt_threads()
{
    DEBUG("Deinit preempt threads\n");
    nk_barrier_destroy(&global.barrier);
    return 0;
}
