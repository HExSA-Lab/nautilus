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
 * Copyright (c) 2019, Michael A. Cuevas <cuevas@u.northwestern.edu>
 * Copyright (c) 2019, Enrico Deiana <ead@u.northwestern.edu>
 * Copyright (c) 2019, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Michael A. Cuevas <cuevas@u.northwestern.edu>
 *          Enrico Deiana <ead@u.northwestern.edu>
 *          Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/cpu.h>
#include <nautilus/naut_assert.h>
#include <nautilus/irq.h>
#include <nautilus/idle.h>
#include <nautilus/paging.h>
#include <nautilus/fiber.h>
#include <nautilus/waitqueue.h>
#include <nautilus/timer.h>
#include <nautilus/percpu.h>
#include <nautilus/atomic.h>
#include <nautilus/list.h>
#include <nautilus/errno.h>
#include <nautilus/mm.h>
#include <nautilus/random.h>
#include <nautilus/scheduler.h>
#include <nautilus/cpu_state.h>

#ifndef NAUT_CONFIG_DEBUG_FIBERS
#undef  DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define FIBER_INFO(fmt, args...) INFO_PRINT("fiber: " fmt, ##args)
#define FIBER_ERROR(fmt, args...) ERROR_PRINT("fiber: " fmt, ##args)
#define FIBER_DEBUG(fmt, args...) DEBUG_PRINT("fiber: " fmt, ##args)
#define FIBER_WARN(fmt, args...)  WARN_PRINT("fiber: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("fiber: " fmt, ##args)

/* Constants used for fiber thread setup and fiber fork */
#define FIBER_THREAD_STACK_SIZE NAUT_CONFIG_FIBER_THREAD_STACK_SIZE
#define LAUNCHPAD 16
#define STACK_CLONE_DEPTH 2
#define GPR_RAX_OFFSET 0x70

/* Macros for accessing parts of the fiber state */
#define _GET_FIBER_STATE() get_cpu()->f_state
#define _NK_IDLE_FIBER() get_cpu()->f_state->idle_fiber
#define _GET_FIBER_THREAD() get_cpu()->f_state->fiber_thread
#define _GET_SCHED_HEAD() &(get_cpu()->f_state->f_sched_queue)
 
/* Macros for locking and unlocking fibers */
#define _LOCK_SCHED_QUEUE(state) spin_lock(&(state->lock)) 
#define _UNLOCK_SCHED_QUEUE(state) spin_unlock(&(state->lock)) 
#define _LOCK_FIBER(f) spin_lock(&(f->lock))
#define _UNLOCK_FIBER(f) spin_unlock(&(f->lock))

/* Each CPU has a fiber state associated with it */
typedef struct nk_fiber_percpu_state {
    spinlock_t  lock; /* lock for the entire fiber percpu state */
    nk_thread_t *fiber_thread; /* Points to the CPU's Fiber thread which is created at bootup */
    nk_fiber_t *curr_fiber; /* points to the fiber currently running on this CPU */
    nk_fiber_t *idle_fiber; /* points to this CPU's idle fiber */
    struct list_head f_sched_queue; /* sched queue for fibers on this CPU (can be accessed by other CPUs) */
    struct nk_wait_queue *waitq; /* Wait queue that the fiber thread can sleep on */
    int fork_cpu; /* Determines which CPU forked fibers will be placed on. Default => curr CPU */
} fiber_state;

/* These functions are implemented in assembly. Can be found in src/asm/fiber_lowlevel.S */
extern void _nk_fiber_context_switch(nk_fiber_t *f_to);
extern void _nk_fiber_context_switch_early(nk_fiber_t* f_to);
extern void _nk_exit_switch(nk_fiber_t *next);
extern nk_fiber_t *nk_fiber_fork();
extern int _nk_fiber_fork_exit(nk_fiber_t *curr);
extern int _nk_fiber_join_yield();

/* Assembly stubs for yield functions. Users use these functions to perform context switches */
extern int nk_fiber_yield();
extern int nk_fiber_yield_to(nk_fiber_t* f_to, int earlyRetFlag);

#if NAUT_CONFIG_FIBER_FSAVE
extern void _nk_fiber_fp_save(nk_fiber_t* f);
#endif

/******** INTERNAL FUNCTIONS **********/

// returns the fiber state for the current CPU
static fiber_state* _get_fiber_state()
{
  return get_cpu()->f_state;
}

// returns the fiber currently running on the current CPU
nk_fiber_t* nk_fiber_current()
{
  return _get_fiber_state()->curr_fiber;
}

// returns the current CPU's idle fiber
static nk_fiber_t* _nk_idle_fiber()
{
  return _get_fiber_state()->idle_fiber;
}

// returns the current CPU's fiber thread
static nk_thread_t *_get_fiber_thread()
{
  return _get_fiber_state()->fiber_thread;
}

// returns the current CPU's sched queue head
static struct list_head* _get_sched_head()
{
  return &(_get_fiber_state()->f_sched_queue); 
}

// returns the current CPU's fiber sched queue lock
static spinlock_t *_get_sched_queue_lock()
{
  return &(get_cpu()->f_state->lock); 
}

// utility function for setting up  a fiber's stack 
static void _fiber_push(nk_fiber_t * f, uint64_t x)
{
    f->rsp -= 8;
    *(uint64_t*)(f->rsp) = x;
}

// Round Robin policy for fibers. Returns the first fiber in the curr CPU's sched queue
// Returns NULL if no fiber is available in the curr CPU's sched queue
static nk_fiber_t* _rr_policy()
{
  // Grab the first fiber from the sched queue
  struct list_head *f_queue = _GET_SCHED_HEAD(); 
  nk_fiber_t *fiber_to_schedule = list_first_entry(f_queue, nk_fiber_t, sched_node); 
  
  // If sched queue not empty, fiber_to_schedule != NULL
  if (fiber_to_schedule){
    // Remove the fiber from the sched queue
    list_del_init(&(fiber_to_schedule->sched_node));
  }

  //DEBUG: prints the fiber that was just dequeued and indicates current and idle fiber
  FIBER_DEBUG("_rr_policy() : just dequeued a fiber : %p\n", fiber_to_schedule);
  FIBER_DEBUG("_rr_policy() : current fiber is %p and idle fiber is %p\n", _GET_FIBER_STATE()->curr_fiber,_GET_FIBER_STATE()->idle_fiber); 

  // Returns the fiber to schedule (or NULL if no fiber to schedule)
  return fiber_to_schedule;
}

// Cleans up an exiting fiber. Frees fiber struct and fiber's stack, cleans up fiber's wait queue
// Exiting fiber must be running when this is called because a context switch is performed at the end
static void _nk_fiber_exit(nk_fiber_t *f)
{
  // Acquire the exiting fiber's lock
  _LOCK_FIBER(f);

  // Set status of fiber to exiting
  f->f_status = EXIT;

  // Grab the fiber state of the current cpu
  fiber_state *state = _GET_FIBER_STATE();

  // next will be the fiber we switch to (might be idle fiber)
  nk_fiber_t *next = NULL;
  
  // DEBUG: Prints out the exiting fiber's wait queue size
  FIBER_DEBUG("_nk_fiber_exit() : queue size is %d\n", f->num_wait);
  
  // On exit, go through each fiber in wait queue
  nk_fiber_t *temp;
  struct list_head *waitq = &(f->wait_queue); 
  // DEBUG: Prints out wait queue address, head, and tail.
  FIBER_DEBUG("_nk_fiber_exit() : The wait queue of size %d is %p on fiber %p\n", f->num_wait, waitq, f);

  while (f->num_wait > 0) {
    // Dequeue a fiber from the waitq
    temp = list_first_entry(waitq, nk_fiber_t, wait_node);
    list_del_init(&(temp->wait_node));
    f->num_wait--;

    // DEBUG: Prints out what fibers are in waitq and what the waitq size is
    //FIBER_DEBUG("_nk_fiber_exit() : In waitq loop. Temp is %p and size is %d\n", temp, waitq->size);
    
    // if temp is a valid fiber, add it to a random sched queue
    if (temp){
      nk_fiber_run(temp, F_RAND_CPU);

      // DEBUG: prints the number of fibers that temp is waiting on
      FIBER_DEBUG("_nk_fiber_exit() : restarting fiber %p on wait_queue %p\n", temp, waitq);
    }
  }

  // Mark the current fiber as done (since we are exiting)
  f->is_done = 1;

  // Picks fiber to switch to and updates fiber state
  _LOCK_SCHED_QUEUE(state);
  next = _rr_policy();
  if (!(next)) {
    next = state->idle_fiber;
  }
  state->curr_fiber = next;
  _UNLOCK_SCHED_QUEUE(state);
  
  // Unlock the fiber before free (in case we implement reaping)
  _UNLOCK_FIBER(f);

  // Free the current fiber's memory (stack and fiber structure)
  free(f->stack);
  free(f);
  
  // Switch back to the idle fiber using special exit function
  // Jumps to exit switch so we avoid pushing return addr to freed stack
  __asm__ __volatile__ ("movq %0, %%rdi;"
                        "jmp _nk_exit_switch;" : : "r"(next) : "memory");
}

// Wrapper used to execute a fiber's routine
static void _fiber_wrapper(nk_fiber_t* f_to)
{
  FIBER_DEBUG("_nk_fiber_wrapper() : executing fiber routine from fiber %p\n", f_to);

  // Execute fiber function
  f_to->fun(f_to->input, f_to->output);

  FIBER_DEBUG("_nk_fiber_exit() : exiting from fiber %p\n", f_to);
  
  // Exit when fiber function ends
  // Starts each fiber on f's wait queue and switches stacks to idle fiber
  _nk_fiber_exit(f_to);

  return;
}

/*
 * _nk_fiber_init
 *
 * Utility function that sets up the given fiber's stack
 *
 * The stack will look like:
 *
 * ________________________
 *|                        |
 *|  Ptr to Fiber Wrapper  |
 *|________________________|
 *|                        |
 *|    Dummy GPR VALUES    |
 *|           .            |
 *|           .            |
 *|           .            |
 *| ptr to f in rdi's spot |
 *|           .            |
 *|           .            |
 *|           .            |
 *|  Remaining dummy GPRs  |
 *|________________________|
 *|                        |
 *|   Initial FPR Values   |
 *| *only included if FPR  |
 *|   saving is enabled*   |
 *|________________________|
 *
 * Order of GPRs can be found in include/nautilus/fiber.h
 * All values except %rdi are dummy values when debug enabled.
 * All values except %rdi are 0 when debugging not enabled.
 * %rdi's stack position will contain ptr to fiber (stack ptr).
 * We use this setup in context switch assembly code
 * to help us switch stacks and start the fiber.
 *
 * We pop these values off the stack and ptr to fiber
 * will go into %rdi which is first argument register. We
 * can then retq to Fiber Wrapper and the first argument will
 * be the fiber in %rdi. This allows us to run the routine
 * held in the fiber structure with the proper input and output.
 *
 */

#if NAUT_CONFIG_DEBUG_FIBERS

// If debugging is enabled, sentinent values are placed in registers
static void _nk_fiber_init(nk_fiber_t *f)
{
  f->rsp = (uint64_t) f->stack + f->stack_size;
  _fiber_push(f, (uint64_t) _fiber_wrapper);
  _fiber_push(f, 0xdeadbeef12345670ul);
  _fiber_push(f, 0xdeadbeef12345671ul);
  _fiber_push(f, 0xdeadbeef12345672ul);
  _fiber_push(f, 0xdeadbeef12345673ul);
  _fiber_push(f, 0xdeadbeef12345674ul);
  _fiber_push(f, (uint64_t) f);
  _fiber_push(f, 0x0ul); /* RBP, do not want this to be sentinent value */
  _fiber_push(f, 0xdeadbeef12345676ul);
  _fiber_push(f, 0xdeadbeef12345677ul);
  _fiber_push(f, 0xdeadbeef12345678ul);
  _fiber_push(f, 0xdeadbeef12345679ul);
  _fiber_push(f, 0xdeadbeef1234567aul);
  _fiber_push(f, 0xdeadbeef1234567bul);
  _fiber_push(f, 0xdeadbeef1234567cul);
  _fiber_push(f, 0xdeadbeef1234567dul);
  #if NAUT_CONFIG_FIBER_FSAVE
  _nk_fiber_fp_save(f);
  #endif

  return;
}

#else 

static void _nk_fiber_init(nk_fiber_t *f)
{
  f->rsp = (uint64_t) f->stack + f->stack_size;
  _fiber_push(f, (uint64_t) _fiber_wrapper);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, (uint64_t) f);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0x0ul);
  #if NAUT_CONFIG_FIBER_FSAVE
  _nk_fiber_fp_save(f);
  #endif
  return;
}

#endif

// Helper function called by C portions of nk_fiber_yield and nk_fiber_yield_to.
// Sets up the context switch between f_from and f_to
__attribute__((noreturn)) static void _nk_fiber_yield_helper(nk_fiber_t *f_to, fiber_state *state, nk_fiber_t* f_from)
{
  // If a fiber is not waiting or exiting, change its status to yielding
  if (f_from->f_status == READY && !(f_from->is_idle)) {
    f_from->f_status = YIELD;
  }
  
  // Update fiber state and f_to's curr_cpu/status
  _LOCK_FIBER(f_to);
  state->curr_fiber = f_to;
  f_to->curr_cpu = my_cpu_id();
  f_to->f_status = RUN;
  
  // Unlock fibers and perform context switch
  _UNLOCK_FIBER(f_to);

  // Change the vc of the current thread if we aren't switching away from the idle fiber
  // TODO: MAC: Might not do what I think it does
  if (!f_from->is_idle){ 
    nk_fiber_set_vc(f_from->vc);
  }
  
  // DEBUG: Shows when we are switching to idle fiber. We want to minimize this!
  /*if (f_to->is_idle) {
    FIBER_INFO("_nk_fiber_yield_helper() : Switched to idle fiber on CPU %d\n", my_cpu_id());
  }*/
  
  // Enqueue the current fiber (if it is not the idle fiber)
  if (!(f_from->is_idle)) {
    // Gets the sched queue for the current CPU
    _LOCK_SCHED_QUEUE(state);
    struct list_head *fiber_sched_queue = &(state->f_sched_queue);
     
    // DEBUG: Prints the fiber that's about to be enqueued
    FIBER_DEBUG("_nk_fiber_yield_helper() : About to enqueue fiber: %p \n", f_from);
    
    _LOCK_FIBER(f_from);
    f_from->f_status = READY;
    f_from->curr_cpu = my_cpu_id();
    _UNLOCK_FIBER(f_from);

    // Adds fiber we're switching away from to the current CPU's fiber queue
    list_add_tail(&(f_from->sched_node), fiber_sched_queue);
    _UNLOCK_SCHED_QUEUE(state);
  }
  // Begin context switch (register saving and stack switch)
  _nk_fiber_context_switch(f_to);

  // Tells compiler this point is unreachable, stops compiler warning
  __builtin_unreachable();
}

// C portion of function sed to switch away from a fiber that was just placed on a wait queue
// This function is called from an assembly stub, _nk_fiber_join_yield, that is implemented
// in src/asm/fiber_lowlevel.S. This is similar to _nk_fiber_yield and _nk_fiber_yield_to, 
// except the user will never call this function (or _nk_fiber_join_yield) on their own.
__attribute__((noreturn)) void __nk_fiber_join_yield(uint64_t rsp, uint64_t offset)
{
  // Gets current state that will be needed throughout function
  fiber_state *state = _GET_FIBER_STATE();
  nk_fiber_t *f_from = state->curr_fiber;
  
  // Adjust f_from's stack ptr
  f_from->rsp = rsp;

  // get next fiber to yield to
  _LOCK_SCHED_QUEUE(state);
  nk_fiber_t *f_to = _rr_policy();
  _UNLOCK_SCHED_QUEUE(state);
  if (!(f_to)) { 
    if (f_from->is_idle) {
      // Should never come from the idle fiber
      panic("Attempted to call yield_to from idle fiber. Should never happen!\n");
      *(uint64_t*)(rsp+GPR_RAX_OFFSET) = -1;
      //_nk_fiber_context_switch_early(f_from);
      _nk_fiber_context_switch(f_from);
    } else {
        f_to = state->idle_fiber;
    }
  } 
  state->curr_fiber = f_to;
  
  // Change the vc of the current thread if we aren't switching away from the idle fiber
  if (!f_from->is_idle){ 
    nk_fiber_set_vc(f_from->vc);
  }
 
  // Update f_to info and status 
  _LOCK_FIBER(f_to);
  f_to->curr_cpu = my_cpu_id();
  f_to->f_status = RUN;
  _UNLOCK_FIBER(f_to);

  // Begin context switch (register saving and stack change)
  *(uint64_t*)(rsp+GPR_RAX_OFFSET) = 0;
  _nk_fiber_context_switch(f_to);
  
  // Tells compiler this point is unreachable, stops compiler warning
  __builtin_unreachable();
} 

// Cleans up forked fibers since they do not get cleaned up by _fiber_wrapper
static void _nk_fiber_cleanup()
{
  // Figure out what the current fiber is and exit from it
  nk_fiber_t *curr = nk_fiber_current();
  FIBER_DEBUG("_nk_fiber_cleanup() : starting fiber cleanup on %p\n", curr);
  _nk_fiber_exit(curr);
}

// returns a random number
static inline uint64_t _get_random()
{
    uint64_t t;
    nk_get_rand_bytes((uint8_t *)&t,sizeof(t));
    return t;
}

// If needed, wakes up the fiber thread so fiber routines can be executed
// Called in nk_fiber_run to ensure fibers placed on queues will be run ASAP.
static int _wake_fiber_thread(fiber_state *state)
{
  #if NAUT_CONFIG_FIBER_ENABLE_SLEEP
  nk_thread_t *curr_thread = state->fiber_thread;
  // Wake up fiber thread if it is sleeping (so it can schedule & run fibers)
  if (curr_thread->timer) {
    //DEBUG: Prints info whenever a fiber thread is awakened
    FIBER_DEBUG("nk_fiber_run() : waking fiber thread\n");
    FIBER_DEBUG("nk_fiber_run() : curr_thread = %p named %s, timer = %p named %s, cpu = %d \n", curr_thread, curr_thread->name, curr_thread->timer, curr_thread->timer->name, curr_thread->current_cpu);

    // Cancel the timer for the desired fiber thread
    nk_timer_cancel(curr_thread->timer);
  }
  #endif

  #if NAUT_CONFIG_FIBER_ENABLE_WAIT 
  if (!(list_empty_careful(&(state->waitq->list)))) {
    nk_wait_queue_wake_one_extended(state->waitq, 1);
  }
  #endif
  // NAUT_CONFIG_FIBER_ENABLE_SPIN case: No need to wake, so just return 0
  return 0;
}

// Returns a random CPU's fiber thread
static nk_thread_t *_get_random_fiber_thread()
{
  // Picks a random CPU and returns that CPU's fiber thread
  struct sys_info * sys = per_cpu_get(system);
  int random_cpu = (int)(_get_random() % sys->num_cpus);
  return sys->cpus[random_cpu]->f_state->fiber_thread;
}

// Returns a random CPU's fiber state
static fiber_state *_get_random_fiber_state()
{
  // Picks a random CPU and returns that CPU's fiber thread
  struct sys_info * sys = per_cpu_get(system);
  int random_cpu = (int)(_get_random() % sys->num_cpus);
  return sys->cpus[random_cpu]->f_state;
}

// Checks if to_del is on a sched queue (ready to be switched to)
// returns -EINVAL if not ready, otherwise returns 0
static int _check_yield_to(nk_fiber_t *to_del) {
  _LOCK_FIBER(to_del);
  // If the fiber isn't ready to switch to, indicate failure.
  if (to_del->f_status != READY) {
     FIBER_DEBUG("_check_yield_to() : to_del's status is %s\n", to_del->f_status);
     _UNLOCK_FIBER(to_del);
     return -EINVAL;
  } else { /* The fiber is ready, so we will take it from its queue so we can use it */
      // Gets the fiber state of the CPU of the target fiber
      fiber_state *state = per_cpu_get(system)->cpus[to_del->curr_cpu]->f_state;
      
      // Prevents deadlock by avoiding lock if to_del is on own queue (would spin forever)
      if (state == _GET_FIBER_STATE()) {
        list_del_init(&(to_del->sched_node));
        _UNLOCK_FIBER(to_del);
      } else { 
      _LOCK_SCHED_QUEUE(state);
      list_del_init(&(to_del->sched_node));
      _UNLOCK_FIBER(to_del);
      _UNLOCK_SCHED_QUEUE(state);
      }
      return 0;
  }
}

// sets up fiber state for current CPU
static struct nk_fiber_percpu_state *init_local_fiber_state()
{
    struct nk_fiber_percpu_state *state = (struct nk_fiber_percpu_state*)malloc_specific(sizeof(struct nk_fiber_percpu_state), my_cpu_id());
    
    if (!state) {
        ERROR("Could not allocate fiber state\n");
	    goto fail_free;
    }
	
	memset(state, 0, sizeof(struct nk_fiber_percpu_state));
    
    spinlock_init(&(state->lock));
     
    INIT_LIST_HEAD(&(state->f_sched_queue));
    
    state->waitq = nk_wait_queue_create("fib");
    
    state->fork_cpu = F_CURR_CPU;
 
    return state;

 fail_free:
    free(state); 

    return 0;
}

// Sets up fiber state on all APs
int nk_fiber_init_ap ()
{
    cpu_id_t id = my_cpu_id();
    struct cpu * my_cpu = get_cpu();

    FIBER_DEBUG("Initializing fibers on AP %u (%p)\n",id,my_cpu);
    
    my_cpu->f_state = init_local_fiber_state();
    if (!(my_cpu->f_state)) { 
	    ERROR("Could not intialize fiber thread\n");
	    return -1;
    }

    return 0;
}

// Sets up fiber state on first CPU to boot
int nk_fiber_init() 
{
    struct cpu * my_cpu = nk_get_nautilus_info()->sys.cpus[nk_get_nautilus_info()->sys.bsp_id];

    FIBER_INFO("Initializing fibers on BSP\n");

    my_cpu->f_state = init_local_fiber_state();
    if (!(my_cpu->f_state)) { 
	    ERROR("Could not intialize fiber thread\n");
	    return -1;
    }

    return 0;
}

// Utility function used to determine if fiber thread should sleep or not
static int _check_empty(void *s) 
{
  fiber_state *state = (fiber_state*)s;
  return (!(list_empty(&(state->f_sched_queue)) && state->curr_fiber->is_idle));
}

// The idle fiber has different behavior depending on those chosen Kconfig option.
// SPIN: yields continuously (even if there are no fibers to yield to)
// SLEEP: yields continuously, but will force fiber thread to sleep for set amt. of time if no fibers are avail.
    // nk_fiber_run will wake up fiber_thread if a fiber is added to queue when it is sleeping
// WAIT: yields continuously, but will put fiber thread onto wait queue if no fibers are available
    // nk_fiber_run will wake up fiber_thread when a fiber is added to the queue
static void __nk_fiber_idle(void *in, void **out)
{
  while (1) {
    // If we have fiber thread spin enabled
    #ifdef NAUT_CONFIG_FIBER_ENABLE_SPIN
    nk_fiber_yield();
    #endif

    // If we have fiber thread sleep enabled
    #ifdef NAUT_CONFIG_FIBER_ENABLE_SLEEP  
    nk_fiber_yield();
    if (list_empty_careful(_GET_SCHED_HEAD())){
      FIBER_DEBUG("nk_fiber_idle() : fiber thread going to sleep\n");
      nk_sleep(NAUT_CONFIG_FIBER_THREAD_SLEEP_TIME);
      FIBER_DEBUG("nk_fiber-idle() : fiber thread waking up\n");
    }
    #endif
    
    // If we have fiber thread wait enabled
    // Instead of sleep, have thread sleep on per cpu fiber state wait queue
    // wakes up when nk_fiber_run is called
    #ifdef NAUT_CONFIG_FIBER_ENABLE_WAIT
    nk_fiber_yield();
    fiber_state *state = _GET_FIBER_STATE();
    if (!(_check_empty((void*)state))){
      FIBER_DEBUG("nk_fiber_idle() : fiber thread waiting on more fibers\n");
      nk_wait_queue_sleep_extended(state->waitq, _check_empty, state);
      FIBER_DEBUG("nk_fiber-idle() : fiber thread waking up\n");
    }
    #endif 
  }
}

// Started at bootup, responsible for running all fiber routines on the CPU it is located on
static void __fiber_thread(void *in, void **out)
{
  if (nk_thread_name(get_cur_thread(),"(fiber)")) { 
    ERROR("Failed to name fiber thread\n");
    return;
  }

  //TODO: Make these options configurable with Kconfig
  struct nk_sched_constraints c = { .type=APERIODIC,
            .interrupt_priority_class=0x0, 
            .aperiodic.priority=NAUT_CONFIG_FIBER_THREAD_PRIORITY };
  
  if (nk_sched_thread_change_constraints(&c)) { 
    ERROR("Unable to set constraints for fiber thread\n");
    panic("Unable to set constraints for fiber thread\n");
    return;
  }

  
  // TODO: Associate fiber thread to console thread (somehow) 

  // Fetch and update fiber state 
  fiber_state *state = _GET_FIBER_STATE();
  if (!(state)) {
    ERROR("Failed to get current fiber state\n");
    panic("Failed to get current fiber state\n");
  }
  state->fiber_thread = get_cur_thread();
 
  // Starting the idle fiber
  nk_fiber_t *idle_fiber_ptr;
  if (nk_fiber_create(__nk_fiber_idle, 0, 0, 0, &idle_fiber_ptr) < 0) {
    ERROR("Unable to create idle fiber\n");
    panic("Unable to create idle fiber\n");
  }

  // Updating fiber state with new fiber information
  state->curr_fiber = idle_fiber_ptr;
  state->idle_fiber = idle_fiber_ptr;   
  
  // Updating idle field of idle fiber
  idle_fiber_ptr->is_idle = true;

  // Updating current cpu info
  idle_fiber_ptr->curr_cpu = my_cpu_id();

  // For FPU Debugging, prints Xsave configuration
  #if (0 && defined(NAUT_CONFIG_DEBUG_FPU))
  uint64_t eax;
  uint64_t edx;

  asm volatile ("pushq %%rcx ;"
                "pushq %%rdx ;"
                "pushq %%rax ;"
                "xor %%rcx, %%rcx ;"
                "xgetbv ;"
                "movl %%eax, %[result1] ;"
                "movl %%edx, %[result2] ;"
                "popq %%rax ;"
                "popq %%rdx ;"
                "popq %%rcx ;"
                 : [result1]"=m" (eax), [result2] "=m" (edx) : : "memory");
      
  FIBER_INFO("The cr0 features are eax: %d and edx: %d\n", eax, edx);

  #endif

  // Starting the idle fiber with fiber wrapper (so it isn't added to the queue)
  _fiber_wrapper(idle_fiber_ptr);

  return;
}

// Starts the fiber thread for this CPU
static int __start_fiber_thread_for_this_cpu()
{
  nk_thread_id_t tid;
  if (nk_thread_start(__fiber_thread, 0, 0, 1, FIBER_THREAD_STACK_SIZE, &tid, my_cpu_id())) {
      ERROR("Failed to start fiber thread\n");
      return -1;
  }
  FIBER_INFO("Fiber thread launched on cpu %d as %p\n", my_cpu_id(), tid);
  return 0;
}

// Starts fiber thread, will panic if unable to start fiber thread
void nk_fiber_startup()
{
    struct cpu *my_cpu = get_cpu();
    FIBER_INFO("Starting fiber thread for CPU %d\n",my_cpu->id);
    if (__start_fiber_thread_for_this_cpu()){
	    ERROR("Cannot start fiber thread for CPU!\n");
	    panic("Cannot start fiber thread for CPU!\n");
	    return;
    }
}

#if NAUT_CONFIG_DEBUG_FIBERS
// Debug function, will print out fiber queue for the current CPU
static void _debug_yield(nk_fiber_t *f_to)
{
  if (f_to) {
    //DEBUG: Indicates what fiber was picked to schedule
    FIBER_DEBUG("nk_fiber_yield() : The fiber picked to schedule is %p\n", f_to); 
  
    //DEBUG: Will print out the fiber queue for this CPU's fiber thread
    nk_fiber_t *f_iter = NULL;
    struct list_head *f_sched = _GET_SCHED_HEAD();
    list_for_each_entry(f_iter, f_sched, sched_node){
      FIBER_DEBUG("nk_fiber_yield() : The fiber queue contains fiber: %p\n", f_iter);
    }
    //DEBUG: Will indicate when fiber queue is done printing (to indicate whether queue is finite)
    FIBER_DEBUG("nk_fiber_yield() : Done printing out the fiber queue.\n");
  }
}
#endif

/****** WRAPPER NK FIBER YIELD *******/
static uint64_t rdtsc_time = 0;
static uint64_t data[10000];
static int a = 0;

int _wrapper_nk_fiber_yield()
{
// nk_vc_printf("wrapper_nk_fiber_yield : running\n");
  uint64_t curr_time = rdtsc();
  // nk_vc_printf("Interval time : %d\n", curr_time - rdtsc_time);
  data[a] = curr_time - rdtsc_time;
  rdtsc_time = curr_time;
  a++; 
  return nk_fiber_yield();
}
void _nk_fiber_print_data()
{
  int i;
  for (i = 0; i < a; i++) {
    nk_vc_printf("%d interval: %d\n", i, data[i]);
  } 
    /*
     * // Variance calculation
     * uint64_t N = a;
     * uint64_t sumOfSquares = 0;
     * uint64_t total = 0;
     * uint64_t mean = 0;
     *  for (i = 0; i < a; i++) {
     *    total += data[i]; 
     *  }
     *  mean = total / N; 
     *  for (i = 0; i < a; i++) {
     *    uint64_t diff = data[i] - mean;
     *    sumOfSquares += (diff * diff);
     *  } 
     *  long double variance = sumOfSquares / N;
     *  nk_vc_printf("VARIANCE OF DATA SET: %d\n", variance);
     */ 
    memset(data, 0, sizeof(data));
    a = 0;
    rdtsc_time = 0;
    return;
}  

/******** EXTERNAL FUNCTIONS **********/

/* 
 * nk_fiber_create
 *
 * creates a fiber and sets up its stack
 *
 * @fun: the fiber's routine
 * @input: the argument(s) for the fiber's routine
 * @output: where the fiber should store its output
 * @stack_size: size of the fiber's stack. 0 => let us decide
 * @fiber_output: Holds the pointer to the fiber pointer once it is created 
 *
 *
 * on error, returns -EINVAL, otherwise 0
 */
int nk_fiber_create(nk_fiber_fun_t fun,
                    void *input,
                    void **output,
                    nk_stack_size_t stack_size,
                    nk_fiber_t **fiber_output)
{
  // Create pointer to fiber, initialize to NULL
  nk_fiber_t *fiber = NULL;

  // Get stack size
  nk_stack_size_t required_stack_size = stack_size ? stack_size: FSTACK_16KB;

  // Allocate space for a fiber
  fiber = malloc(sizeof(nk_fiber_t));

  // Check if malloc for nk_fiber_t struct failed
  if (!fiber) {
    // Print error here
    return -EINVAL;
  }

  // Initialize the whole struct with zeros
  memset(fiber, 0, sizeof(nk_fiber_t));
  
  // Set fiber status to init
  fiber->f_status = INIT;
 
  // Set stack size
  fiber->stack_size = required_stack_size;
 
  // Allocate stack space
  fiber->stack = (void*) malloc(required_stack_size);

  // Check if malloc for the stack failed
  if (!fiber->stack){
    // Free the previously allocated nk_fiber_t
    free(fiber);
    return -EINVAL;
  }

  // Initialize function, input, and output related to the fiber
  fiber->fun = fun;
  fiber->input = input;
  fiber->output = output;

  // Initialize the fiber's stack
  _nk_fiber_init(fiber);

  // Return the fiber 
  if (fiber_output){
    *fiber_output = fiber;
  }

  fiber->vc = get_cur_thread()->vc;

  // Initialize the fiber's spinlock
  spinlock_init(&(fiber->lock));

  // Initializes the fiber's list field
  INIT_LIST_HEAD(&(fiber->sched_node));

  // Initializes wait queue
  INIT_LIST_HEAD(&(fiber->wait_queue)); 
  INIT_LIST_HEAD(&(fiber->wait_node)); 
 
  //TODO: Not used yet 
  // Initializes child fiber list 
  INIT_LIST_HEAD(&(fiber->fiber_children));
  INIT_LIST_HEAD(&(fiber->child_node));

  return 0;
}

/* 
 * nk_fiber_run
 *
 * takes a fiber and a CPU argument and queues the fiber on the desired CPU
 *
 * @f: the fiber to add to the sched queue
 * @target_cpu: which CPU to start the fiber on. F_CURR_CPU => run on current CPU,
 *              F_RAND_CPU => run on random CPU. 
 *
 * on error (invalid target_cpu), returns -EINVAL, otherwise 0.
 */
int nk_fiber_run(nk_fiber_t *f, int target_cpu)
{ 
  // system info gathered
  struct sys_info * sys = per_cpu_get(system);
  int num_cpus = sys->num_cpus;
  int t_cpu = 0;
 
  // by default, the state is set to the current cpu's fiber state
  // This means we only have to update state if target_cpu != F_CURR_CPU (3 cases)
  fiber_state *state = _GET_FIBER_STATE();

  // Check to see if target_cpu is sane 
  if (target_cpu < F_RAND_CPU || target_cpu > num_cpus) {
    // If target_cpu is insane, return an error
    return  -EINVAL;
  } else if (target_cpu > F_CURR_CPU) { /* if value is greater than -1, must be specific CPU */
      //state is is set to the f_state of target_cpu
      state = sys->cpus[target_cpu]->f_state;
  } else if(target_cpu == F_RAND_CPU) { /* RAND and CURR are only choices left */
      // Random fiber state selected
      state = _get_random_fiber_state();
  } /* if target_cpu != RAND, then it must be CURR. Fiber state is already set to CURR CPU by default */

  // t_cpu is updated to the cpu number of the selected state 
  t_cpu = state->fiber_thread->current_cpu;
 
  //DEBUG: Prints the fiber that is about to be enqueued and the CPU it will be enqueued on
  FIBER_DEBUG("nk_fiber_run() : about to enqueue a fiber: %p on cpu: %d\n", f, state->fiber_thread->current_cpu); 
  
  // Lock the fiber, change it's curr cpu, and change status to ready (since we are about to queue it)
  _LOCK_FIBER(f);
  f->curr_cpu = t_cpu;
  f->f_status = READY;
  
  // Lock the CPU fiber state and enqueue the fiber into sched queue 
  _LOCK_SCHED_QUEUE(state);
  list_add_tail(&(f->sched_node), &(state->f_sched_queue));
  
  // Unlock CPU fiber state and f
  _UNLOCK_SCHED_QUEUE(state);
  _UNLOCK_FIBER(f);
 
  // Wake up fiber thread for selected CPU (or do nothing if it is already awake)
  _wake_fiber_thread(state); 

  return 0;
}

/* 
 * nk_fiber_start
 *
 * creates a fiber and puts it on specified CPU's sched queue
 *
 * @fun: the fiber's routine
 * @input: the argument(s) for the fiber's routine
 * @output: where the fiber should store its output
 * @stack_size: size of the fiber's stack. 0 => let us decide
 * @target_cpu: which CPU to start the fiber on. F_CURR_CPU => run on current CPU,
 *              F_RAND_CPU => run on random CPU. 
 * @fiber_output: Holds the pointer to the fiber pointer once it is created 
 *
 *
 * on create error, returns -1; on run error, returns -2; otherwise returns 0
 */
int nk_fiber_start(nk_fiber_fun_t fun,
                   void *input, void **output,
                   nk_stack_size_t stack_size,
                   int target_cpu,
                   nk_fiber_t **fiber_output)
{
  // Creates fiber and puts it into scheduling queue
  if (nk_fiber_create(fun, input, output, stack_size, fiber_output) < 0) {
    return -1;
  }
  if (nk_fiber_run(*fiber_output, target_cpu) < 0) {
    return -2;
  }
  return 0;
}

/* 
 * _nk_fiber_yield (NOTE THAT THIS ISN'T CALLED DIRECTLY! Users call nk_fiber_yield())
 *
 * C portion of general purpose yield function. Yields execution to a randomly selected fiber.
 * Calls to yield will first enter through assembly stub found in src/asm/fiber_lowlevel.S. They will
 * then jump into this C code to perform part of the yield and call a yield _nk_fiber_yield_helper().
 *
 * ***** ALL PASSED FROM ASSEMBLY STUB *****
 * @rsp: New stack pointer to be saved into curr_fiber.
 * @offset: Placement of FP state on stack.
 * 
 *
 * ***** "Returns" by overwriting RAX on the fiber's stack *****
 * on error (called outside fiber thread), returns -1.
 * on special case (idle fiber yield and no fiber available), returns 1
 * otherwise, returns 0.
 */
__attribute__((noreturn)) void _nk_fiber_yield(uint64_t rsp, uint64_t offset)
{ 
  // Checks if the current thread is the fiber thread (if not, error)
  fiber_state *state = _GET_FIBER_STATE();
  
  // Saves current stack pointer into fiber struct
  nk_fiber_t *curr_fiber = state->curr_fiber;
  curr_fiber->rsp = rsp;

  #if NAUT_CONFIG_FIBER_FSAVE
  curr_fiber->fpu_state_offset = offset;
  #endif

  if (state->fiber_thread != get_cur_thread()) {
    // Abort yield somehow. Subtract from RSP and retq?
    *(uint64_t*)(rsp+GPR_RAX_OFFSET) = -1; 
    _nk_fiber_context_switch(curr_fiber);
  }
  
  // Pick a random fiber to yield to (NULL if no fiber in queue)

  _LOCK_SCHED_QUEUE(state);
  nk_fiber_t *f_to = _rr_policy();
  _UNLOCK_SCHED_QUEUE(state);
  
  #if NAUT_CONFIG_DEBUG_FIBERS
  //_debug_yield(f_to);
  #endif

  // If f_to is NULL, there are no fibers in the queue
  // We can either exit early and sleep (if curr_fiber is idle)
  // Or we can switch to the idle fiber  
  if (!(f_to)) { 
    if (curr_fiber->is_idle) {
      //Abort yield somehow? Subtract from RSP and retq?
      *(uint64_t*)(rsp+GPR_RAX_OFFSET) = 1; 
      _nk_fiber_context_switch(curr_fiber);
    } else {
        f_to = state->idle_fiber;
    }
  }
  // Utility function to perform enqueue and other yield housekeeping
  *(uint64_t*)(rsp+GPR_RAX_OFFSET) = 0; 
  _nk_fiber_yield_helper(f_to, state, curr_fiber);
}

/* 
 * _nk_fiber_yield_to (NOTE THAT THIS ISN'T CALLED DIRECTLY! Users call nk_fiber_yield_to())
 *
 * C portion of yield_to funciton. Will yield execution from current fiber to f_to. Calls
 * to nk_fiber_yield_to will first enter through assembly stub found in src/asm/fiber_lowlevel.S.
 * Stub will pass rsp and offset to this C code to be saved into fiber struct. This function then
 * performs part of yield and then calls _nk_fiber_yield_to() to help perform context switch. 
 *
 * ***** ALL PASSED FROM ASSEMBLY STUB *****
 * @f_to: the fiber to yield execution to
 * @earlyRetFlag: whether to return early if f_to is not available. 
 *                YIELD_TO_EARLY_RET => don't yield to rand fiber if f_to unavailable
 *                Any other int => yield to random fiber fiber if f_to unavailable
 * @offset: Location of floating point state saved on stack.
 * @rsp: Stack pointer of fiber stack.
 *
 * ***** "Returns" by overwriting RAX on fiber's stack *****
 * on early return case, returns -1.
 * if f_to could not be yielded to (rand fiber was yielded to instead), returns 1.
 * on successful yield to f_to, returns 0.
 */
__attribute__((noreturn)) void _nk_fiber_yield_to(nk_fiber_t *f_to, int earlyRetFlag, uint64_t offset, uint64_t rsp)
{
  // Updates f_to's stack info
  fiber_state *state = _GET_FIBER_STATE();
  nk_fiber_t *curr_fiber = state->curr_fiber;
  curr_fiber->rsp = rsp;
  
  #if NAUT_CONFIG_FIBER_FSAVE
  curr_fiber->fpu_state_offset = offset;
  #endif

  // Locks fiber state
  _LOCK_SCHED_QUEUE(state);
  // Remove f_to from its respective fiber queue (need to check all CPUs)
  // This is currently not safe, fiber may be running and therefore not in sched queue
  if (_check_yield_to(f_to) < 0){
    //DEBUG: Will indicate whether the fiber we're attempting to yield to was not found
    FIBER_DEBUG("nk_fiber_yield_to() : Failed to find fiber in queues :(\n");
    
    // If early ret flag is set, we will indicate failure instead of yielding to random fiber
    if (earlyRetFlag) {
      _UNLOCK_SCHED_QUEUE(state);
      *(uint64_t*)(rsp+GPR_RAX_OFFSET) = -1;
      _nk_fiber_context_switch(curr_fiber);
      FIBER_DEBUG("nk_fiber_yield_to() : early ret flag set, returning early\n");
    }
    
    // early ret flag not set, so we find a random fiber to yield to instead
    nk_fiber_t *new_to = _rr_policy();
    _UNLOCK_SCHED_QUEUE(state);
    
    // Checks to see if we received a valid fiber from _rr_policy (NULL = no fibers to schedule)
    if (!(new_to)) { 
      if (curr_fiber->is_idle) { /* if no fiber to sched and curr idle, no reason to switch */
        *(uint64_t*)(rsp+GPR_RAX_OFFSET) = 0;
        _nk_fiber_context_switch_early(curr_fiber);
      } else { /* if no fiber to sched and not currenty in idle fiber, switch to idle fiber */
          new_to = state->idle_fiber;
      }
    }
 
    // If the fiber could not be found, we yield to a random fiber instead
    *(uint64_t*)(rsp+GPR_RAX_OFFSET) = 1;
    _nk_fiber_yield_helper(new_to, state, curr_fiber);
  }

  // Use utility function to perform rest of yield 
  _UNLOCK_SCHED_QUEUE(state);
  *(uint64_t*)(rsp+GPR_RAX_OFFSET) = 0;
  _nk_fiber_yield_helper(f_to, state, curr_fiber);
}

/* 
 * nk_fiber_conditional_yield
 *
 * Will yield execution to random fiber if condition holds
 *
 * @cond_function: conditional function for checking whether or not to yield. 
 *                  cond_function returns true (any val but 0) => yield
 * @state: The state passed to cond_function. 
 *
 * on failed condition (no yield), returns 1.
 * on successful yield, returns the return value of nk_fiber_yield.
 */
// TODO MAC: Talk with Prof. Dinda about race condition
int nk_fiber_conditional_yield(uint8_t (*cond_function)(void *param), void *state)
{
  if (cond_function(state)){
    return nk_fiber_yield();
  }
  return 1;
}

/* 
 * nk_fiber_conditional_yield_to
 *
 * Will yield execution to specified fiber if condition holds
 *
 * @fib: fiber to yield to
 * @earlyRetFlag: whether to return early if f_to is not available to yield to. 
 *                YIELD_TO_EARLY_RET => don't yield to rand fiber if f_to unavailable
 *                Any other int => yield to random fiber fiber if f_to unavailable
 * @cond_function: conditional function for checking whether or not to yield. 
 *                  cond_function returns true (any val but 0) => yield
 * @state: The state passed to cond_function. 
 *
 * on failed condition (no yield), returns 1.
 * on true condition, returns the return value of nk_fiber_yield_to.
 */
//TODO MAC: Talk with Prof. Dinda about race condition in this, too.
int nk_fiber_conditional_yield_to(nk_fiber_t *f_to, int earlyRetFlag, uint8_t (*cond_function)(void *param), void *state)
{
  if (cond_function(state)){
    return nk_fiber_yield_to(f_to, earlyRetFlag);
  }
  return 1;
}

/* 
 * nk_fiber_join
 *
 * Puts the curr_fiber onto wait_on's wait queue
 *
 * @wait_on: the fiber whose wait queue we will be placed onto 
 *
 * on failure (wait_on is NULL or is exiting/done), returns -1
 * on success, returns the return value of _nk_fiber_join_yield
 */
int nk_fiber_join(nk_fiber_t *wait_on)
{
  // Check if wait_on is NULL
  if (!wait_on){
    return -1;
  }
  // Fetches current fiber
  nk_fiber_t *curr_fiber = nk_fiber_current();
  
  // DEBUG: Prints out our intent to add curr_fiber to wait_on's wait queue
  FIBER_DEBUG("nk_fiber_join() : about to enqueue fiber %p on the wait queue %p\n", curr_fiber, &(wait_on->wait_queue));

  // Adds curr_fiber to wait_on's wait queue
  _LOCK_FIBER(wait_on);
  if (wait_on->is_done || wait_on->f_status == EXIT){
    FIBER_INFO("nk_fiber_join() : tried to join a thread which is finshed or exiting\n");
    _UNLOCK_FIBER(wait_on);
    return -1;
  }
  // Change wait_on's curr CPU since it will no longer be associated with one
  wait_on->curr_cpu = -1;

  // Adding curr_fiber to wait_on's wait queue
  struct list_head *wait_q = &(wait_on->wait_queue);
  list_add_tail(&(curr_fiber->wait_node), wait_q);
  wait_on->num_wait++;

  // Update status of curr_fiber and yield
  curr_fiber->f_status = WAIT;
  _UNLOCK_FIBER(wait_on);
  return _nk_fiber_join_yield();
}

/* 
 * __nk_fiber_fork
 *
 * Creates a copy of the current fiber with modifications made to its stack
 *
 * 
 * Note that this isn't called directly. It is vectored
 * into from an assembly stub. This is so the parent's 
 * can be saved before any modifications can be made to them.
 *
 * On success, parent gets child's fiber ptr, child gets 0
 * On failure, parent gets (nk_fiber_t*)-1;
 *
 */
nk_fiber_t *__nk_fiber_fork(uint64_t rsp0, uint64_t offset)
{
  // Fetch current fiber and fiber state
  fiber_state *state = _GET_FIBER_STATE();
  nk_fiber_t *curr = state->curr_fiber;

  // Variables needed to hold stack frame information
  uint64_t size, alloc_size;
  uint64_t     rbp1_offset_from_ret0_addr;
  uint64_t     rbp_stash_offset_from_ret0_addr;
  uint64_t     rbp_offset_from_ret0_addr;
  void         *child_stack;
  uint64_t     rsp;
  uint64_t     fp_state_offset;

  // Store the value of %rsp into var rsp and clobber memory
  __asm__ __volatile__ ( "movq %%rsp, %0" : "=r"(rsp) : : "memory");

  void *rbp0      = __builtin_frame_address(0);                   // current rbp, *rbp0 = rbp1
  void *rbp1      = __builtin_frame_address(1);                   // caller rbp, *rbp1 = rbp2  (forker's frame)
  void *rbp_tos   = __builtin_frame_address(STACK_CLONE_DEPTH);   // should scan backward to avoid having this be zero or crazy
  void *ret0_addr = rbp0 + 0x8;
  FIBER_DEBUG("__nk_fiber_fork() : rbp0: %p, rbp1: %p, rbp_tos: %p, ret0_addr: %p\n", rbp0, rbp1, rbp_tos, ret0_addr);
  if ((uint64_t)rbp_tos <= (uint64_t)curr->stack ||
	(uint64_t)rbp_tos >= (uint64_t)(curr->stack + curr->stack_size)) { 
	FIBER_DEBUG("__nk_fiber_fork() : Cannot resolve %lu stack frames on fork, using just one\n", STACK_CLONE_DEPTH);
        rbp_tos = rbp1;
  }

  curr->rsp = rsp0;

  fp_state_offset = 0;  
  #if NAUT_CONFIG_FIBER_FSAVE
  curr->fpu_state_offset = offset;
  fp_state_offset = rsp0 - offset;
  #endif

  // this is the address at which the fork wrapper (nk_fiber_fork) stashed
  // the current value of rbp - this must conform to the GPR SAVE model
  // in fiber.h
  void *rbp_stash_addr = ret0_addr + 9*8 /*+ FIBER_FPR_SAVE_SIZE*/ + fp_state_offset; 
  
  // from last byte of tos_rbp to the last byte of the stack on return from this function 
  // (return address of wrapper)
  // the "launch pad" is added so that in the case where there is no stack frame above the caller
  // we still have the space to fake one.
  size = (rbp_tos + 8) - ret0_addr + LAUNCHPAD;
  
  rbp1_offset_from_ret0_addr = rbp1 - ret0_addr;

  rbp_stash_offset_from_ret0_addr = rbp_stash_addr - ret0_addr;
  rbp_offset_from_ret0_addr = (*(void**)rbp_stash_addr) - ret0_addr;
  alloc_size = curr->stack_size;
 
  FIBER_DEBUG("__nk_fiber_fork() : rbp_stash_addr: %p, rbp1_offset_from_ret0: %p, rbp_stash_offset: %p, rbp_offset_from: %p\n", rbp_stash_addr, rbp1_offset_from_ret0_addr, rbp_stash_offset_from_ret0_addr, rbp_offset_from_ret0_addr);
   
  // Allocate new fiber struct using current fiber's data
  nk_fiber_t *new;
  nk_fiber_create(NULL, NULL, 0, alloc_size, &new);
  if (!new) {
    //panic("__nk_fiber_fork() : could not allocate new fiber. Fork failed.\n");
    return (nk_fiber_t*)-1;
  }
  child_stack = new->stack;

  //TODO MAC: Figure out how to do this correctly
  #if NAUT_CONFIG_FIBER_FSAVE
  uint64_t new_start_of_stack = new->fpu_state_offset - fp_state_offset;
  child_stack = new->stack + new_start_of_stack;
  #endif

  // current fiber's stack is copied to new fiber
  _fiber_push(new, (uint64_t)&_nk_fiber_cleanup);  

 
  FIBER_DEBUG("__nk_fiber_fork() : child_stack: %p, alloc_size: %p, size: %p\n", child_stack, alloc_size, size);
  
  memcpy(child_stack + alloc_size - size - 0x0, ret0_addr - 0x0, size - LAUNCHPAD + 0x0);
  void **rbp_stash_ptr;
  void **rbp2_ptr;
  void **ret2_ptr; 
  if (rbp_tos != rbp1) {
    new->rsp = (uint64_t)(child_stack + alloc_size - size + 0x8);
    FIBER_DEBUG("__nk_fiber_fork() : new->rsp is %p\n", new->rsp); 

    // Update the child's snapshot of rbp on its stack (that was done
    // by nk_fiber_fork()) with the corresponding position in the child's stack
    // when nk_fiber_fork() unwinds the GPRs, it will end up with rbp pointing
    // into the cloned stack instead of the old stack
    rbp_stash_ptr = (void**)(new->rsp + rbp_stash_offset_from_ret0_addr - 0x08);
  
    FIBER_DEBUG("__nk_fiber_fork() : rsp: %p, at rsp: %p, rbp_stash_offset_from_ret0_addr: %p\n", new->rsp, *(void**)new->rsp, rbp_stash_offset_from_ret0_addr);
    FIBER_DEBUG("__nk_fiber_fork() : rsp + 0x78: %p, rbp_stash_offset_from_ret0_addr: %p\n", *(void**)(new->rsp + 0x78), *(void**)(new->rsp+rbp_stash_offset_from_ret0_addr - 0x8));
  
    *rbp_stash_ptr = (void*)(new->rsp + rbp_offset_from_ret0_addr);
  
    FIBER_DEBUG("__nk_fiber_fork() : rbp_stash_ptr: %p\n", *rbp_stash_ptr); 
  
    // Determine caller's rbp copy and return address in the child stack
    rbp2_ptr = (void**)(new->rsp + rbp1_offset_from_ret0_addr - 0x8);
    ret2_ptr = rbp2_ptr + 1;
  } else {
      new->rsp = (uint64_t)(child_stack + alloc_size - size + 0x8);
      rbp_stash_ptr = (void**)(new->rsp + rbp_stash_offset_from_ret0_addr - 0x8);
      *rbp_stash_ptr = (void*)(new->rsp + rbp_offset_from_ret0_addr);
      rbp2_ptr = (void**)(new->rsp + rbp1_offset_from_ret0_addr);
      ret2_ptr = rbp2_ptr + 1;
  } 
  FIBER_DEBUG("__nk_fiber_fork() : rbp1_offset_from_ret0_addr: %p, rbp2_ptr: %p, ret2_ptr: %p\n", rbp1_offset_from_ret0_addr, rbp2_ptr, ret2_ptr);
  FIBER_DEBUG("__nk_fiber_fork() : rbp2_ptr - 0x8: %p, rbp2_ptr: %p, ret2_ptr: %p\n", *(void**)(rbp2_ptr - 0x8), *rbp2_ptr, *ret2_ptr);
  
  #if NAUT_CONFIG_FIBER_FSAVE
  new->fpu_state_offset = (new->rsp - fp_state_offset);
  #endif

  FIBER_DEBUG("__nk_fiber_fork() : fp_state_offset %p and new->fpu_state_offset %p\n", fp_state_offset, new->fpu_state_offset);

  // rbp2 we don't care about since we will not not
  // return from the caller in the child, but rather go into the fiber cleanup
  *rbp2_ptr = 0x0ULL;

  // fix up the return address to point to our fiber cleanup function
  // so when caller returns, the fiber exists
  *ret2_ptr = &_nk_fiber_cleanup;
 
  /* 
  FIBER_DEBUG("__nk_fiber_fork() : rbp2_ptr - 0x8: %p, rbp2_ptr: %p, ret2_ptr: %p\n", *(void**)(rbp2_ptr - 0x8), *rbp2_ptr, *ret2_ptr);
  FIBER_DEBUG("__nk_fiber_fork() : at rsp + 0x78: %p, rsp + 0x80: %p, rsp + 0x88: %p\n", (void**)(new->rsp + 0x80), *(void**)(new->rsp) + 0x70, *(void**)(new->rsp+0x88));
  */

  //DEBUG: Printing the fibers data
  FIBER_DEBUG("nk_fiber_fork() : printing fiber data for curr fiber. ptr %p, stack ptr %p\n", curr, curr->rsp);
  FIBER_DEBUG("nk_fiber_fork() : printing fiber data for new fiber. ptr %p, stack ptr %p\n", new, new->rsp); 

  // Set forked fiber's %rax to 0. Will not restore %rax when we exit fork function
  *(uint64_t*)(new->rsp+GPR_RAX_OFFSET) = 0x0;

  // Add the forked fiber to the sched queue
  if (nk_fiber_run(new, state->fork_cpu) < 0) {
    free(new->stack);
    free(new);
    return (nk_fiber_t*)-1;
  } 

  return new;
}

/* 
 * nk_fiber_set_vc
 *
 * Changes the current fiber's vc to the specified vc
 *
 * @vc: the virtual console to change to
 *
 */
void nk_fiber_set_vc(struct nk_virtual_console *vc)
{
  // Fetches current fiber
  nk_fiber_t* curr_fiber = nk_fiber_current();

  // Changes the vc of current fiber and the current thread
  curr_fiber->vc = vc;
  get_cur_thread()->vc = vc;
}

/* 
 * nk_fiber_set_fork_cpu
 *
 * Changes the default fork cpu for the current CPU
 *
 * @target_cpu: the desired CPU to set as default
 *
 * returns -1 on failure, 0 otherwise
 */
int nk_fiber_set_fork_cpu(int target_cpu)
{
  // Gets the current fiber state
  fiber_state *state = _GET_FIBER_STATE();

  // Gets System info (CPU num)
  struct sys_info * sys = per_cpu_get(system);
  int num_cpus = sys->num_cpus;

  // Determines whether target_cpu is sane
  if (target_cpu >= 0 && target_cpu <= num_cpus) {
    _LOCK_SCHED_QUEUE(state);
    state->fork_cpu = target_cpu;
    _UNLOCK_SCHED_QUEUE(state);
    return 0;
  }

  // Getting this far indicates failure to change fork's CPU
  return -1;
}
