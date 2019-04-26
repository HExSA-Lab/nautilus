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

#ifndef NAUT_CONFIG_DEBUG_FIBERS
#undef  DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define FIBER_INFO(fmt, args...) INFO_PRINT("fiber: " fmt, ##args)
#define FIBER_ERROR(fmt, args...) ERROR_PRINT("fiber: " fmt, ##args)
#define FIBER_DEBUG(fmt, args...) DEBUG_PRINT("fiber: " fmt, ##args)
#define FIBER_WARN(fmt, args...)  WARN_PRINT("fiber: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("fiber: " fmt, ##args)

#define FIBER_THREAD_STACK_SIZE (PAGE_SIZE_2MB)
#define LAUNCHPAD 16
#define STACK_CLONE_DEPTH 2
#define GPR_RAX_OFFSET 0x70

typedef struct nk_fiber_percpu_state {
    spinlock_t  lock;
    nk_thread_t *fiber_thread;
    nk_fiber_t *curr_fiber;
    nk_fiber_t *idle_fiber;
    struct list_head f_sched_queue;
} fiber_state;
  
extern void nk_fiber_context_switch(nk_fiber_t *cur, nk_fiber_t *next);
extern void _nk_exit_switch(nk_fiber_t *next);
extern nk_fiber_t *nk_fiber_fork();
extern void _nk_fiber_fork_exit();

/******** INTERNAL INTERFACE **********/

// returns the fiber state for the current CPU
fiber_state* _get_fiber_state()
{
  return get_cpu()->f_state;
}


// returns the current CPU's current fiber
nk_fiber_t* nk_fiber_current()
{
  return _get_fiber_state()->curr_fiber;
}


// returns the idle fiber of the current CPU
nk_fiber_t* _nk_idle_fiber()
{
  return _get_fiber_state()->idle_fiber;
}

// returns the current CPU's fiber thread
nk_thread_t *_get_fiber_thread()
{
  return _get_fiber_state()->fiber_thread;
}


// returns the idle fiber of the current CPU
struct list_head* _get_sched_head()
{
  return &(_get_fiber_state()->f_sched_queue); 
}


/*
 * utility function for setting up
 * a fiber's stack 
 */
void _fiber_push(nk_fiber_t * f, uint64_t x)
{
    f->rsp -= 8;
    *(uint64_t*)(f->rsp) = x;
}

void _nk_fiber_exit(nk_fiber_t *f)
{
  // Set status of fiber to exiting
  f->f_status = EXIT;
  // Get the idle fiber for the current CPU
  nk_fiber_t *idle = _nk_idle_fiber();
  
  // DEBUG: Prints out the exiting fiber's wait queue size
  FIBER_DEBUG("_nk_fiber_exit() : queue size is %d\n", f->num_wait);
  
  // On exit, go through each fiber in wait queue
  nk_fiber_t *temp;
  struct list_head *waitq = &(f->wait_queue); 
  // DEBUG: Prints out wait queue address, head, and tail.
  FIBER_DEBUG("_nk_fiber_exit() : The wait queue of size %d is %p on fiber %p\n", f->num_wait, waitq, f);

  while(f->num_wait > 0){
    
    // Dequeue a fiber from the waitq
    temp = list_first_entry(waitq, nk_fiber_t, wait_node);
    list_del_init(&(temp->wait_node));
    f->num_wait--;
    // DEBUG: Prints out what fibers are in waitq and what the waitq size is
    //FIBER_DEBUG("_nk_fiber_exit() : In waitq loop. Temp is %p and size is %d\n", temp, waitq->size);
    if (temp != 0){
      // DEBUG: prints the number of fibers that temp is waiting on
      FIBER_DEBUG("_nk_fiber_exit() : restarting fiber %p on wait_queue %p\n", temp, waitq);
      nk_fiber_run(temp, 1);
    }
  }
  // Mark the current fiber as done (since we are exiting)
  f->is_done = 1;

  //TODO: PROBABLY WANT TO DO THESE ATOMICALLY (Frees and changing curr fiber)
 
  _get_fiber_state()->curr_fiber = idle;
    
  // Removes the idle fiber from the queue
  list_del_init(&(_get_fiber_state()->curr_fiber->sched_node));  

  // Free the current fiber's memory (stack, stack ptr, and wait queue)
  free(f->stack);
  free(f);
  
   // Switch back to the idle fiber using special exit function
  _nk_exit_switch(idle);

  return;
}

void _fiber_wrapper(nk_fiber_t* f_to)
{
  // Execute fiber function
  FIBER_DEBUG("_fiber_wrapper BEGIN\n");
  f_to->fun(f_to->input, f_to->output);
  FIBER_DEBUG("_fiber_wrapper END\n");

  // Exit when fiber function ends
  // Starts each fiber on f's wait queue and switches stacks to idle fiber
  //FIBER_DEBUG("_nk_fiber_exit() : exiting from fiber %p\n", f_to);
  _nk_fiber_exit(f_to);

  return;
}


/* Utility function that sets up the given fiber's stack
 *
 * The stack will look like:
 *
 * ________________________
 *|                        |
 *|  Ptr to Fiber Wrapper  |
 *|________________________|
 *|                        |
  |    Dummy GPR VALUES    |
 *|           .            |
 *|           .            |
 *|           .            |
 *| ptr to f in rdi's spot |
 *|           .            |
 *|           .            |
 *|           .            |
 *|  Remaining dummy GPRs  |
 *|________________________|
 *
 * Order of GPRs can be found in include/nautilus/fiber.h
 * All values except %rdi are dummy values.
 * %rdi's stack position will contain ptr to fiber (stack).
 * We use this setup in context switch assembly code
 * to help us switch stacks.
 * We pop these values off the stack and ptr to fiber
 * will go into %rdi which is first argument register. We
 * can then retq to Fiber Wrapper and the first argument will
 * be the fiber in  %rdi. This allows us to run the routine
 * held in the fiber structure.
 *
 */

#if NAUT_CONFIG_DEBUG_FIBERS

void _nk_fiber_init(nk_fiber_t *f)
{
  f->rsp = (uint64_t) f->stack + f->stack_size;
  _fiber_push(f, (uint64_t) _fiber_wrapper);
  _fiber_push(f, 0xdeadbeef12345670ul);
  _fiber_push(f, 0xdeadbeef12345671ul);
  _fiber_push(f, 0xdeadbeef12345672ul);
  _fiber_push(f, 0xdeadbeef12345673ul);
  _fiber_push(f, 0xdeadbeef12345674ul);
  _fiber_push(f, (uint64_t) f);
  _fiber_push(f, 0xdeadbeef12345675ul);
  _fiber_push(f, 0xdeadbeef12345676ul);
  _fiber_push(f, 0xdeadbeef12345677ul);
  _fiber_push(f, 0xdeadbeef12345678ul);
  _fiber_push(f, 0xdeadbeef12345679ul);
  _fiber_push(f, 0x0ul);
  _fiber_push(f, 0xdeadbeef1234567bul);
  _fiber_push(f, 0xdeadbeef1234567cul);
  _fiber_push(f, 0xdeadbeef1234567dul);

  return;
}

#else 

void _nk_fiber_init(nk_fiber_t *f)
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

  return;
}

#endif

nk_fiber_t* _rr_policy()
{
  // Get the sched queue from the fiber thread on the current CPU
  nk_thread_t *cur_thread = _get_fiber_thread();
  struct list_head *fiber_sched_queue = _get_sched_head();
  
  
  // Pick the fiber at the front of the queue and return it if the queue is not empty
  nk_fiber_t *fiber_to_schedule = NULL;
  if (!(list_empty(fiber_sched_queue))){
    // Grab the first fiber from the sched queue
    fiber_to_schedule = list_first_entry(fiber_sched_queue, nk_fiber_t, sched_node);
    
    // Remove the fiber from the sched queue
    list_del_init(&(fiber_to_schedule->sched_node));
  } else {
    // If empty, reset the fiber sched queue
    // MAC: might be unecessary (?)
    list_del_init(fiber_sched_queue);
    }

  //DEBUG: prints the fiber that was just dequeued and indicates current and idle fiber
  FIBER_DEBUG("_rr_policy() : just dequeued a fiber : %p\n", fiber_to_schedule);
  FIBER_DEBUG("_rr_policy() : current fiber is %p and idle fiber is %p\n", _get_fiber_state()->curr_fiber,_get_fiber_state()->idle_fiber); 

  // Returns the fiber to schedule (or NULL if no fiber to schedule)
  return fiber_to_schedule;
}

int _nk_fiber_yield_to(nk_fiber_t *f_to)
{
  // Get the current fiber
  nk_fiber_t *f_from = nk_fiber_current();
 
  // If a fiber is not waiting or exiting, change its status to yielding
  if (f_from->f_status != WAIT && f_from->f_status != EXIT) {
    f_from->f_status = YIELD;
  }
  
  // Enqueue the current fiber (if not on wait queue)
  if (f_to != f_from && f_from->f_status != WAIT) {
    // Gets the sched queue for the current CPU
    struct list_head *fiber_sched_queue = _get_sched_head();
    
    // DEBUG: Prints the fiber that's about to be enqueued
    FIBER_DEBUG("nk_fiber_yield() : About to enqueue fiber: %p \n", f_from);
    
    // Adds fiber we're switching away from to the current CPU's fiber queue
    list_add_tail(&(f_from->sched_node), fiber_sched_queue);
  }
  
  // Change the vc of the current thread if we aren't switching away from the idle fiber
  // TODO: MAC: Might not do what I think it does
  if (!f_from->is_idle){ 
    nk_fiber_set_vc(f_from->vc);
  }
  
  // Context switch (register saving and stack change)
  _get_fiber_state()->curr_fiber = f_to;
  f_from->f_status = READY;
  f_to->f_status = RUN;
  nk_fiber_context_switch(f_from, f_to);

  return 0;
}

void _nk_fiber_cleanup()
{
  nk_fiber_t *curr = nk_fiber_current();
  FIBER_DEBUG("_nk_fiber_cleanup() : starting fiber cleanup on %p\n", curr);
  _nk_fiber_exit(curr);
}

uint8_t _is_idle_fiber(nk_fiber_t *f)
{
  nk_fiber_t *idle_fiber = _nk_idle_fiber();

  // if the argument fiber is the idle fiber, return 1
  uint8_t result = 0;
  if (idle_fiber == f){
    result = 1;
  }
 
  return result; 
}

static inline uint64_t _get_random()
{
    uint64_t t;
    nk_get_rand_bytes((uint8_t *)&t,sizeof(t));
    return t;
}

static int _nk_initial_placement()
{
    // Picks a random number between 0 and the number of CPUs
    struct sys_info * sys = per_cpu_get(system);
    return (int)(_get_random() % sys->num_cpus);
}

nk_thread_t *_get_random_fiber_thread()
{
  // Picks a random CPU and returns that CPU's fiber thread
  int random_cpu = _nk_initial_placement();
  struct sys_info * sys = per_cpu_get(system);
  return sys->cpus[random_cpu]->f_state->fiber_thread;
}


// TODO MAC: Completely unecessary, put lock on fibers and then
// check state (running, idle, waiting) and act accordingly

// Checks all queues for the specified fiber and removes it from the queue if found
int _check_all_queues_remove(nk_fiber_t *to_del)
{
  // Gets number of CPUs on system
  struct sys_info *sys = per_cpu_get(system);
  int cpu_iter = sys->num_cpus;

  // Iterates through sched queues on each CPU
  struct list_head *temp;
  nk_fiber_t *test;
  for (int iter = 0; iter < cpu_iter; iter++){
    // Checks each sched queue to see if it contains to_del
    temp = &(sys->cpus[iter]->f_state->f_sched_queue);
    list_for_each_entry(test, temp, sched_node){
      //DEBUG: Prints all the fibers (and queues) that are being checked
      FIBER_DEBUG("_check_all_queues_remove() : %d outer loop and %p is the temp fiber\n", iter, test);
      
      // If the fiber is found, remove it from the sched queue and return 0 (to indicate success)
      if (test == to_del){
        list_del_init(&(to_del->sched_node));
        return  0;
      }
    }
  }
  // Fiber was not found, return 1 to indicate failure
  return 1;
}

static struct nk_fiber_percpu_state *init_local_fiber_state()
{
    struct nk_fiber_percpu_state *state = (struct nk_fiber_percpu_state*)malloc_specific(sizeof(struct nk_fiber_percpu_state), my_cpu_id());
    
    if (!state) {
        ERROR("Could not allocate fiber state\n");
	goto fail_free;
    }
	
	memset(state, 0, sizeof(struct nk_fiber_percpu_state));
    
    spinlock_init(&state->lock);
   
    // Promote to fiber thread
    INIT_LIST_HEAD(&(state->f_sched_queue));
    
    return state;

 fail_free:
    free(state); 

    return 0;
}

int
nk_fiber_init_ap ()
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

int
nk_fiber_init() 
{
    struct cpu * my_cpu = nk_get_nautilus_info()->sys.cpus[nk_get_nautilus_info()->sys.bsp_id];

    FIBER_INFO("Initializing fibers on BSP\n");

    //timing_test(1000000,1000000,1);
    //INFO("Hanging\n");
    //while (1) { asm("hlt"); }

    my_cpu->f_state = init_local_fiber_state();
    if (!(my_cpu->f_state)) { 
	ERROR("Could not intialize fiber thread\n");
	return -1;
    }

    return 0;
}

// TODO MAC: Introduce multiple options for idle fiber behavior:
    // Idle only spins
      // NAUT_CONFIG_ENABLE_SPIN
    // Idle does a sleep
      // NAUT_CONFIG_ENABLE_SLEEP
        // NAUT_CONFIG_FIBER_THREAD_SLEEP_TIME
    // Idle puts thread to sleep on a wait queue w/ nk_fiber_run doing wakeup of target thread
      // NAUT_CONFIG_ENABLE_WAIT
static void __nk_fiber_idle(void *in, void **out)
{
  while (1) {
    // If we have fiber thread spin enabled
    #ifdef NAUT_CONFIG_ENABLE_SPIN
    nk_fiber_yield();
    #endif

    // If we have fiber thread sleep enabled
    #ifdef NAUT_CONFIG_ENABLE_SLEEP  
    nk_fiber_yield();
    if (list_empty_careful(_get_sched_head())){
      FIBER_DEBUG("nk_fiber_idle() : fiber thread going to sleep\n");
      nk_sleep(NAUT_CONFIG_FIBER_THREAD_SLEEP_TIME);
      FIBER_DEBUG("nk_fiber-idle() : fiber thread waking up\n");
    }
    #endif
    
    // If we have fiber thread wait unabled
    // Instead of sleep, add thread to wait queue and get woken
    // up when nk_fiber_run is called
    #ifdef NAUT_CONFIG_ENABLE_WAIT
    nk_fiber_yield();
    if (list_empty_careful(_get_sched_head())){
      FIBER_DEBUG("nk_fiber_idle() : fiber thread waiting on more fibers\n");
      nk_sleep(NAUT_CONFIG_FIBER_THREAD_SLEEP_TIME);
      FIBER_DEBUG("nk_fiber-idle() : fiber thread waking up\n");
    }
    #endif 
  }
}

void __fiber_thread(void *in, void **out)
{

  if (nk_thread_name(get_cur_thread(),"(fiber)")) { 
    ERROR("Failed to name fiber thread\n");
    return;
  }

  //TODO: Figure out if these constraints are right for fibers
  struct nk_sched_constraints c = { .type=APERIODIC,
            .interrupt_priority_class=0x0, 
            .aperiodic.priority=NAUT_CONFIG_FIBER_THREAD_PRIORITY };
  
  if (nk_sched_thread_change_constraints(&c)) { 
    ERROR("Unable to set constraints for fiber thread\n");
    panic("Unable to set constraints for fiber thread\n");
    return;
  }

  // TODO: Associate fiber thread to console thread (somehow) 
  //get_cur_thread()->vc = get_cur_thread()->parent->vc;

  // Fetch and update fiber state 
  fiber_state *state = _get_fiber_state();
  if (!(state)) {
    ERROR("Failed to get current fiber state\n");
  }
  state->fiber_thread = get_cur_thread();

  // Starting the idle fiber
  nk_fiber_t *idle_fiber_ptr;
  if (nk_fiber_create(__nk_fiber_idle, 0, 0, 0, &idle_fiber_ptr) < 0) {
    ERROR("Unable to create idle fiber\n");
  }

  // Updating fiber state with new fiber information
  state->curr_fiber = idle_fiber_ptr;
  state->idle_fiber = idle_fiber_ptr;   
  
  // Updating idle field of idle fiber
  idle_fiber_ptr->is_idle = true;
  
  // Starting the idle fiber with fiber wrapper (so it isn't added to the queue)
  _fiber_wrapper(idle_fiber_ptr);

  return;
}

static int __start_fiber_thread_for_this_cpu()
{
  nk_thread_id_t tid;
  if (nk_thread_start(__fiber_thread, 0, 0, 1, FIBER_THREAD_STACK_SIZE, &tid, my_cpu_id())) {
      ERROR("Failed to start fiber thread\n");
      return -1;
  }
  //INFO("Fiber thread launched on cpu %d as %p\n", my_cpu_id(), tid);
  return 0;
}

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
void _debug_yield(nk_fiber_t *f_to)
{
  if (f_to) {
    //DEBUG: Indicates what fiber was picked to schedule
    FIBER_DEBUG("nk_fiber_yield() : The fiber picked to schedule is %p\n", f_to); 
  
    //DEBUG: Will print out the fiber queue for this CPU's fiber thread
    nk_fiber_t *f_iter = NULL;
    struct list_head *f_sched = _get_sched_head();
    list_for_each_entry(f_iter, f_sched, sched_node){
      FIBER_DEBUG("nk_fiber_yield() : The fiber queue contains fiber: %p\n", f_iter);
    }
    //DEBUG: Will indicate when fiber queue is done printing (to indicate whether queue is finite)
    FIBER_DEBUG("nk_fiber_yield() : Done printing out the fiber queue.\n");
  }
}
#endif

/******** EXTERNAL INTERFACE **********/

int nk_fiber_create(nk_fiber_fun_t fun, void *input, void **output, nk_stack_size_t stack_size, nk_fiber_t **fiber_output)
{
  // Create pointer to fiber, initialize to NULL
  nk_fiber_t *fiber = NULL;

  // Get stack size
  nk_stack_size_t required_stack_size = stack_size ? stack_size: PAGE_SIZE;

  // Allocate space for a fiber
  fiber = malloc(sizeof(nk_fiber_t));

  // Check if an error happened when allocating space for a nk_fiber_t
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

  // Check if an error happened when allocating space for the stack
  if (!fiber->stack){
    // Print error here
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

  // Initializes the fiber's list field
  INIT_LIST_HEAD(&(fiber->sched_node));

  // Sets wait queue count to 0
  fiber->num_wait = 0;
  INIT_LIST_HEAD(&(fiber->wait_queue)); 
  INIT_LIST_HEAD(&(fiber->wait_node)); 

  return 0;
}

int nk_fiber_run(nk_fiber_t *f, uint8_t random_cpu_flag)
{
  //by default, the curr_thread is is set to the fiber thread
  nk_thread_t *curr_thread = _get_fiber_thread();
  
  //if the random cpu flag is set, the fiber will be placed on a random fiber thread's queue
  if (random_cpu_flag){
    curr_thread = _get_random_fiber_thread();
  }
  
  // Enqueues the fiber into the chosen fiber thread's queue.
  struct list_head *fiber_sched_queue = _get_sched_head();
  
  //DEBUG: Prints the fiber that is about to be enqueued and the CPU it will be enqueued on
  FIBER_DEBUG("nk_fiber_run() : about to enqueue a fiber: %p on cpu: %d\n", f, curr_thread->current_cpu); 
  f->f_status = READY; 
  list_add_tail(&(f->sched_node), fiber_sched_queue);

  // TODO MAC: Factor out waking of fiber thread into separate function
  // Wake up fiber thread if it is sleeping (so it can schedule & run fibers)
  if (curr_thread->timer){
    //DEBUG: Prints info whenever a fiber thread is awakened
    FIBER_DEBUG("nk_fiber_run() : waking fiber thread\n");
    FIBER_DEBUG("nk_fiber_run() : curr_thread = %p named %s, timer = %p named %s, cpu = %d \n", curr_thread, curr_thread->name, curr_thread->timer, curr_thread->timer->name, curr_thread->current_cpu);
    nk_timer_cancel(curr_thread->timer);
  }

  return 0;
}

int nk_fiber_start(nk_fiber_fun_t fun, void *input, void **output, nk_stack_size_t stack_size, uint8_t random_cpu_flag, nk_fiber_t **fiber_output){
  // Creates fiber and puts it into scheduling queue
  nk_fiber_create(fun, input, output, stack_size, fiber_output);
  nk_fiber_run(*fiber_output, random_cpu_flag);

  return 0;
}

// TODO MAC: check if we're running in the fiber thread before we allow yield to take place
int nk_fiber_yield()
{
  // Pick a random fiber to yield to (NULL if no fiber in queue)
  nk_fiber_t *f_to = _rr_policy();

  // TODO MAC: Move into separate function for debugging 
  #if NAUT_CONFIG_DEBUG_FIBERS
  _debug_yield(f_to);
  #endif

  // If f_to is NULL, there are no fibers in the queue
  // We can then exit early and sleep
  if (f_to == NULL){
   return 0;
   FIBER_INFO("nk_fiber_yield() : yield aborted. Returning 0\n");
  }
  // Utility function to perform enqueue and other yield housekeeping
  return _nk_fiber_yield_to(f_to);
}

int nk_fiber_yield_to(nk_fiber_t *f_to)
{
  // Remove f_to from its respective fiber queue (need to check all CPUs)
  // This is currently not safe, fiber may be running and therefore not in sched queue
  if (_check_all_queues_remove(f_to)){
    //DEBUG: Will indicate whether the fiber we're attempting to yield to was not found
    FIBER_DEBUG("nk_fiber_yield_to() : Failed to find fiber in queues :(\n");
    
    // If the fiber could not be found, we need to yield to a random fiber instead
    _nk_fiber_yield_to(_rr_policy());
    // Return 1 to indicate failure
    return 1;
  }

  // Use utility function to perform rest of yield
  _nk_fiber_yield_to(f_to);
  
  // returns 0 to indicate we successfully yielded to f_to
  return 0;
}

int nk_fiber_conditional_yield(nk_fiber_t *fib, uint8_t (*cond_function)(void *param), void *state)
{
  if (cond_function(state)){
    nk_fiber_yield();
    return 0;
  }
  return 1;
}

nk_fiber_t *__nk_fiber_fork()
{
  // Fetch current fiber
  nk_fiber_t *curr = nk_fiber_current();

  // Variables needed to hold stack frame information
  uint64_t size, alloc_size;
  uint64_t     rbp1_offset_from_ret0_addr;
  uint64_t     rbp_stash_offset_from_ret0_addr;
  uint64_t     rbp_offset_from_ret0_addr;
  void         *child_stack;
  uint64_t     rsp;

  // Store the value of %rsp into var rsp and clobber memory
  __asm__ __volatile__ ( "movq %%rsp, %0" : "=r"(rsp) : : "memory");
 
  void *rbp0      = __builtin_frame_address(0);                   // current rbp, *rbp0 = rbp1
  void *rbp1      = __builtin_frame_address(1);                   // caller rbp, *rbp1 = rbp2  (forker's frame)
  void *rbp_tos   = __builtin_frame_address(STACK_CLONE_DEPTH);   // should scan backward to avoid having this be zero or crazy
  void *ret0_addr = rbp0 + 8;
  FIBER_DEBUG("__nk_fiber_fork() : rbp0: %p, rbp1: %p, rbp_tos: %p, ret0_addr: %p\n", rbp0, rbp1, rbp_tos, ret0_addr);
  if ((uint64_t)rbp_tos <= (uint64_t)curr->stack ||
	(uint64_t)rbp_tos >= (uint64_t)(curr->stack + curr->stack_size)) { 
	FIBER_DEBUG("__nk_fiber_fork() : Cannot resolve %lu stack frames on fork, using just one\n", STACK_CLONE_DEPTH);
        rbp_tos = rbp1;
    }


    // this is the address at which the fork wrapper (nk_fiber_fork) stashed
    // the current value of rbp - this must conform to the GPR SAVE model
    // in fiber.h
  void *rbp_stash_addr = ret0_addr + 9*8; 
  
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
  
  // current fiber's stack is copied to new fiber
  _fiber_push(new, (uint64_t)&_nk_fiber_cleanup);  
  
  child_stack = new->stack;
  FIBER_DEBUG("__nk_fiber_fork() : child_stack: %p, alloc_size: %p, size: %p\n", child_stack, alloc_size, size);
  memcpy(child_stack + alloc_size - size, ret0_addr, size - LAUNCHPAD);
  new->rsp = (uint64_t)(child_stack + alloc_size - size + 0x8);
  FIBER_DEBUG("__nk_fiber_fork() : new->rsp is %p\n", new->rsp); 

  // Update the child's snapshot of rbp on its stack (that was done
   // by nk_fiber_fork()) with the corresponding position in the child's stack
   // when nk_fiber_fork() unwinds the GPRs, it will end up with rbp pointing
   // into the cloned stack instead of the old stack
  void **rbp_stash_ptr = (void**)(new->rsp + rbp_stash_offset_from_ret0_addr - 0x8);
  *rbp_stash_ptr = (void*)(new->rsp + rbp_offset_from_ret0_addr);
  
  // Determine caller's rbp copy and return address in the child stack
  void **rbp2_ptr = (void**)(new->rsp + rbp1_offset_from_ret0_addr + 0x8);
  void **ret2_ptr = rbp2_ptr-1;
   
  FIBER_DEBUG("__nk_fiber_fork() : rbp_stash_ptr: %p, rbp2_ptr: %p, ret2_ptr: %p\n", rbp_stash_ptr, rbp2_ptr, ret2_ptr);
  // rbp2 we don't care about since we will not not
  // return from the caller in the child, but rather go into the fiber cleanup
  *rbp2_ptr = 0x0ULL;

  // fix up the return address to point to our fiber cleanup function
  // so when caller returns, the fiber exists
  *ret2_ptr = &_nk_fiber_cleanup;
  
  //DEBUG: Printing the fibers data
  FIBER_DEBUG("nk_fiber_fork() : printing fiber data for curr fiber. ptr %p, stack ptr %p\n", curr, curr->rsp);
  FIBER_DEBUG("nk_fiber_fork() : printing fiber data for new fiber. ptr %p, stack ptr %p\n", new, new->rsp); 

  // Set forked fiber's %rax to 0. Will not restore %rax when we exit fork function
  *(uint64_t*)(new->rsp+GPR_RAX_OFFSET) = 0x0ul;

  // Add the forked fiber to the sched queue
  nk_fiber_run(new, 0); 
  
  return new;
}

// TODO MAC: Should return an int because this can error
int nk_fiber_join(nk_fiber_t *wait_on)
{
  // Check if wait_on is NULL
  // TODO: Should we make join return an int so we can indicate early abort?
  // TODO MAC: don't sleep before checking is_done
  // Also, there is a race condition (is_done being set on other CPU)
  if (!wait_on){
    return -1;
  }
  // Fetches current fiber
  nk_fiber_t *curr_fiber = nk_fiber_current();
  
  // DEBUG: Prints out our intent to add curr_fiber to wait_on's wait queue
  FIBER_DEBUG("nk_fiber_join() : about to enqueue fiber %p on the wait queue %p\n", curr_fiber, &(wait_on->wait_queue));

  // MAC TODO: Should check if wait_on is a valid fiber? But how?

  // Adds curr_fiber to wait_on's wait queue
  // Not safe, need to lock before we enqueue
  struct list_head *wait_q = &(wait_on->wait_queue);
  list_add_tail(&(curr_fiber->wait_node), wait_q);
  wait_on->num_wait++;
  curr_fiber->f_status = WAIT;
  
  // Handled by special case in fiber yield
  // TODO: Could also attempt to yield directy to wait_on (might speed up wait time)?
  return nk_fiber_yield();
}

void nk_fiber_set_vc(struct nk_virtual_console *vc)
{
  // Fetches current fiber
  nk_fiber_t* curr_fiber = nk_fiber_current();

  // Changes the vc of current fiber and the current thread
  curr_fiber->vc = vc;
  get_cur_thread()->vc = vc;
}

