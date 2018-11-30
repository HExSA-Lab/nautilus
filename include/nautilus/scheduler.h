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
 * Copyright (c) 2016, Chris Beauchene <ChristopherBeauchene2016@u.northwestern.edu>
 *                     Conor Hetland <ConorHetland2015@u.northwestern.edu>
 *                     Peter Dinda <pdinda@northwestern.edu
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Chris Beauchene  <ChristopherBeauchene2016@u.northwestern.edu>
 *          Conor Hetland <ConorHetland2015@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <nautilus/thread.h>

// All time units are in nanoseconds stored in a 64 bit word
// Time 0 is machine start time
// Absolute time is kept for about 585 years
//
// For a 2 GHz TSC, the cycle counter will roll over in 
// about 292 years

// Configuration occurs per-core
struct nk_sched_config {
    uint64_t util_limit;                 // utilization available in 10^-6 units
    uint64_t sporadic_reservation;       // utilization in 10^-6 units
    uint64_t aperiodic_reservation;      // utilization in 10^-6 units
    uint64_t aperiodic_quantum;          // in nanoseconds 
    uint64_t aperiodic_default_priority; 
};

struct nk_sched_periodic_constraints {
    uint64_t phase;  // time of first arrival relative to time of admission
    uint64_t period; // how frequently it arrives (arrival+period=deadline)
    uint64_t slice;  // how much RT computation when it arrives
};

struct nk_sched_sporadic_constraints {
    uint64_t phase;    // time of arrival relative to admission
    uint64_t size;     // length of RT computation
    uint64_t deadline; // deadline for RT computation
    uint64_t aperiodic_priority; // what priority once it it is complete
};

struct nk_sched_aperiodic_constraints {
    uint64_t priority;  // higher number = lower priority; essentially quantum
};

typedef enum { APERIODIC = 0, SPORADIC = 1, PERIODIC = 2} nk_sched_constraint_type_t;
struct nk_sched_constraints {
    nk_sched_constraint_type_t type;
    // the interrupt priority class that must be exceeded
    // in order for this task to be interrupted
    // default: 0x0 => all interrupts
    // 0xf => no interrupts
    // 0xe => interrupts 0xf? only
    // Note that in Nautilus, timer, kick, and all APIC emergency
    // interrupts are 0xf?
    // This does not affect NMIs, PMIs, or SMIs
    // Also note that this is ignored if interrupt threads
    // are enabled (NAUT_CONFIG_INTERRUPT_THREAD)
    uint8_t                    interrupt_priority_class; 
    union  {
	struct nk_sched_periodic_constraints     periodic;
	struct nk_sched_sporadic_constraints     sporadic;
	struct nk_sched_aperiodic_constraints    aperiodic;
    } ;
} ;

// Call this on the BSP at boot time, before the APs are running
int nk_sched_init(struct nk_sched_config *cfg);
// Call this on the APs at boot time
int nk_sched_init_ap(struct nk_sched_config *cfg);
// Call this on BSP and APs at boot time after the APs are running
// but interrupts are still off - it will turn interrupts on
void nk_sched_start();


// Initialize the scheduling state of a new thread
// It will have the defaults (aperiodic, medium priority) if
// the constraints argument is null
struct nk_sched_thread_state* nk_sched_thread_state_init(struct nk_thread *thread,
							 struct nk_sched_constraints *constraints);
void nk_sched_thread_state_deinit(struct nk_thread *thread);

// call at thread creation time to determine where the scheduler
// would like to put the thread, if it can choose
// returns a CPU id [0, num_cpus)
// negative number indicates error
int nk_sched_initial_placement();

// call after thread creation is complete, but before it is first run
// this will also select the initial cpu, setting current_cpu
int nk_sched_thread_post_create(struct nk_thread *thread);
// call before thread destruction is started
int nk_sched_thread_pre_destroy(struct nk_thread *thread);

// Change the scheduling state of the calling thread
// nonzero return => failed
int    nk_sched_thread_change_constraints(struct nk_sched_constraints *constraints);

// Move the thread to the new cpu
// a thread cannot move itself
// a running thread cannot be moved
// currently only aperiodic threads can be moved
// block=1 => will keep trying until successful
int    nk_sched_thread_move(struct nk_thread *thread, int new_cpu, int block);

//
// Have this CPU attempt to steal at most max threads from cpu
// Stealable threads are runnable aperiodic threads 
// cpu==-1 means the scheduler will select a cpu
// This makes a single pass, and there is no guarantee 
// any threads are stolen
int    nk_sched_cpu_mug(int cpu, uint64_t max, uint64_t *actual);

// Make the thread schedulable - generally only called by thread.c
// When the thread is first launched admit=1 is used to do admisson on the 
// designated CPU
// nonzero return => failed
int    nk_sched_make_runnable(struct nk_thread *thread, int cpu, int admit);

// force a scheduling event on the CPU
void   nk_sched_kick_cpu(int cpu);

// Put the thread to sleep / awaken it
// these signal the scheduler that the thread is now on a 
// non-scheduler queue (sleep) or is to be returned to a scheduler 
// queue (awaken)   
// the lock_to_release will be released once the scheduling pass is complete
// this allows the user code to manage races between its own abstractions
// (e.g., wait queues) and the scheduling process
// nk_sched_sleep will also renable preemption on the core before switching to
// the new thread
// nk_sched_sleep_extended expands this to allow for a general callback
// that will be invoked once the scheduling pass is complete. 
void    nk_sched_sleep(spinlock_t *lock_to_release);
void    nk_sched_sleep_extended(void (*release_callback)(void *), void *release_state);
#define nk_sched_awaken(thread,cpu) nk_sched_make_runnable(thread,cpu,0)

// Have the thread yield to another, if appropriate
void              nk_sched_yield(spinlock_t *lock_to_release);
#define           nk_sched_schedule(lock_to_release) nk_sched_yield(lock_to_release)

// Thread exit - will not return!
// does this have the same issue as sleep?
void    nk_sched_exit(spinlock_t *lock_to_release);

// clean up after detached threads
// normally will only execute if we have too many threads active
void    nk_sched_reap(int unconditional);

// find a dead thread that matches the criteria, if possible
// the caller can then avoid the cost of allocating a new
// thread, although it must still initialize it
struct nk_thread *nk_sched_reanimate(nk_stack_size_t min_stack_size,
				     int             placement_cpu);

// return ns
uint64_t nk_sched_get_realtime();

// Print out threads on cpu
// -1 => all CPUs
void nk_sched_dump_threads(int cpu);

// print out scheduler info on the cpu
// -1 => all CPUs
void nk_sched_dump_cores(int cpu);

// print out timer info on the cpu
// -1 => all CPUs
void nk_sched_dump_time(int cpu);

// map a functor over all threads on a cpu.
// cpu==-means all cpus
void nk_sched_map_threads(int cpu, void (func)(struct nk_thread *t, void *state), void *state);


// Provide ability to stop and start the world from the caller
// This forces all cores, except the caller out into an interrupt
// handler, where they will wait for the world to start again
// for the caller, interrupts will be disabled and the calling thread
// or interrupt handler will run uninterrupted until it 
// starts the world again
void nk_sched_stop_world();
struct nk_thread *nk_sched_get_cur_thread_on_cpu(int cpu);
void nk_sched_start_world();


// Invoked by interrupt handler wrapper and other code
// to cause thread context switches
// Do not call this unless you know what you are doing
struct nk_thread *nk_sched_need_resched(void);

// how much time has this thread spent executing?
// ns
uint64_t nk_sched_get_runtime(struct nk_thread *t);

// what are the threads scheduling constraints
int nk_sched_thread_get_constraints(struct nk_thread *t, struct nk_sched_constraints *c);

struct nk_thread *nk_find_thread_by_tid(uint64_t tid);
#endif /* _SCHEDULER_H */
