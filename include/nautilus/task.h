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
 * Copyright (c) 2017, Peter Dinda
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __NK_TASK
#define __NK_TASK

// placed here in case we decide to move more of the
// task implementation into inline
#define TASK_INFO(fmt, args...) INFO_PRINT("task: " fmt, ##args)
#define TASK_ERROR(fmt, args...) ERROR_PRINT("task: " fmt, ##args)
#ifdef NAUT_CONFIG_DEBUG_TASKS
#define TASK_DEBUG(fmt, args...) DEBUG_PRINT("task: " fmt, ##args)
#else
#define TASK_DEBUG(fmt, args...)
#endif
#define TASK_WARN(fmt, args...)  WARN_PRINT("task: " fmt, ##args)


struct nk_task_stats {
    uint64_t    size_ns;       // a size of zero means the task has unknown size
    uint64_t    enqueue_time_ns;
    uint64_t    dequeue_time_ns;
    uint64_t    complete_time_ns;
    uint64_t    wait_start_ns;
    uint64_t    wait_end_ns;
};
    
struct nk_task {
    // flags are written only on completion, so this can be used
    uint64_t    flags;
#define NK_TASK_COMPLETED 1ULL // set when done
#define NK_TASK_DETACHED  2ULL // no one will wait on this task

    struct nk_task_stats stats;
    
    // a task is a deferred procedure call with
    // this function and this input
    void * (*func)(void *);
    void *input;
    void *output;

    // for managing tasks on queues
    struct list_head queue_node;
};


// create and queue a task
// cpu == -1 => any cpu
// size == 0 => unknown size, otherwise worst case run time in ns
// null return indicated the task cannot be queued
struct nk_task *nk_task_produce(int cpu, uint64_t size_ns, void * (*f)(void*), void *input, uint64_t flags);

// dequeue a task, typically used internally
// dequeuing a task does not execute it.
// cpu = -1 => any cpu
// size = 0 => unsized first, then sized
// size > 0 => search sized queue for up to search_limit steps
struct nk_task *nk_task_consume(int cpu, uint64_t size, uint64_t search_limit);

// same as above, but do not spin
struct nk_task *nk_task_try_consume(int cpu, uint64_t size, uint64_t search_limit);

// mark a task as completed
// this will delete the task if it's detached
int nk_task_complete(struct nk_task *task, void *output);

// wait for a task to complete and optionally get its output and performance stats
// detached tasks cannnot be waited on
// this will delete the task
int nk_task_wait(struct nk_task *task, void **output, struct nk_task_stats *stats);

// same as above, but will not spin
int nk_task_try_wait(struct nk_task *task, void **output, struct nk_task_stats *stats);


// Note that the implementation of tasks in scheduler.c

#endif
