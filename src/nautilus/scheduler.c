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
 * Copyright (c) 2016, Chris Beauchene <ChristopherBeauchene2016@u.northwestern.edu>
 *                     Conor Hetland <ConorHetland2015@u.northwestern.edu>
 *                     Peter Dinda <pdinda@northwestern.edu>
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

/*

  This is an implementation of a per-core hard real-time scheduler
  based on the model of Liu:

  Threads are one of:

  - aperiodic  (simple priority, not real-time)
  - periodic   (period, slice, first arrival)
  - sporadic   (size, arrival)

  On creation, a thread is aperiodic with medium priority.


  Tasks (deferred procedure calls/futures/...) are integrated as well.   

   - Tasks of known size can be optionally run by the scheduling pass
     itself, provided no real-time task is runnable.  
   - Tasks of known or unknown size can be optionally run by a per-cpu
     task thread.  
   - The idle loop also consumes tasks (albeit at lowest priority).

*/

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/waitqueue.h>
#include <nautilus/task.h>
#include <nautilus/timer.h>
#include <nautilus/scheduler.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/cpuid.h>
#include <nautilus/random.h>
#include <nautilus/backtrace.h>
#include <nautilus/shell.h>
#include <nautilus/topo.h>
#include <dev/apic.h>
#include <dev/gpio.h>

// enforce lower limits on period and slice / sporadic size
#define ENFORCE_LOWER_LIMITS 1
// period/slice/sporadic size can be no smaller than these (in ns)
// in the future these will be determined at boot time
// the slice lower limit is also the sporadic size lower limit
#define PERIOD_LOWER_LIMIT 20000
#define SLICE_LOWER_LIMIT  10000

// task and idle threads run tasks, and so their stacks
// need to be sized to be sensible for those tasks
#define TASK_THREAD_STACK_SIZE (PAGE_SIZE_2MB)
#define IDLE_THREAD_STACK_SIZE (PAGE_SIZE_2MB)

// interrupt and reaper threads are self-contained here, so
// their stack limits can be more carefully controlled
#define INTERRUPT_THREAD_STACK_SIZE (PAGE_SIZE_4KB*4)
#define REAPER_THREAD_STACK_SIZE (PAGE_SIZE_4KB*4)

// enforce granularity of period and slice / sporadic size
#define ENFORCE_GRANULARITY 1
// period/slice/sporadic size can be no smaller than these (in ns)
// in the future these will be determined at boot time
#define GRANULARITY 2000


#define INSTRUMENT    0

#define SANITY_CHECKS 0

#define STACK_CHECKS  0

#define DUMP_THREAD_RT_STATE 0
#define DUMP_SCHED_STATE     0

#define INFO(fmt, args...) INFO_PRINT("Scheduler: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("Scheduler: " fmt, ##args)


#define DEBUG(fmt, args...)
#define DEBUG_DUMP(rt,pre)
#ifdef NAUT_CONFIG_DEBUG_SCHED
#undef DEBUG
#undef DEBUG_DUMP
#define DEBUG(fmt, args...) DEBUG_PRINT("Scheduler: " fmt, ##args)
#if DUMP_THREAD_RT_STATE
#define DEBUG_DUMP(rt,pre) rt_thread_dump(rt,pre)
#else
#define DEBUG_DUMP(rt,pre) 
#endif
#endif


#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) (((x) >(y)) ? (x) : (y))
#endif

// representation of a utilization of one. 
#define UTIL_ONE 1000000ULL

// Maximum number of threads within a priority queue or queue
#define MAX_QUEUE (NAUT_CONFIG_MAX_THREADS)


#define GLOBAL_LOCK_CONF uint8_t _global_flags=0
#define GLOBAL_LOCK() _global_flags = spin_lock_irq_save(&global_sched_state.lock)
#define GLOBAL_UNLOCK() spin_unlock_irq_restore(&global_sched_state.lock,_global_flags)

#define LOCAL_LOCK_CONF uint8_t _local_flags=0
#define LOCAL_LOCK(s) _local_flags = spin_lock_irq_save(&((s)->lock))
#define LOCAL_UNLOCK(s) spin_unlock_irq_restore(&((s)->lock),_local_flags)

#define TASK_LOCK_CONF uint8_t _task_flags=0
#define TASK_LOCK(t) _task_flags = spin_lock_irq_save(&((t)->lock))
#define TASK_TRY_LOCK(t) spin_try_lock_irq_save(&((t)->lock),&_task_flags)
#define TASK_UNLOCK(t) spin_unlock_irq_restore(&((t)->lock),_task_flags)
//#define TASK_LOCK_CONF 
//#define TASK_LOCK(t) spin_lock(&((t)->lock))
//#define TASK_TRY_LOCK(t) spin_try_lock(&((t)->lock))
//#define TASK_UNLOCK(t) spin_unlock(&((t)->lock))

#if SANITY_CHECKS
#define PAD 0
#define MALLOC_SPECIFIC(x,c) ({ void *p  = malloc_specific((x)+2*PAD,c); if (!p) { panic("Failed to Allocate %d bytes\n",x); } memset(p,0,(x)+2*PAD); p+PAD; })
#define MALLOC(x) ({ void *p  = malloc((x)+2*PAD); if (!p) { panic("Failed to Allocate %d bytes\n",x); } memset(p,0,(x)+2*PAD); p+PAD; })
#define FREE(x) do {if (x) { free(((void*)x)-PAD); x=0;} } while (0)
#else // no sanity checks
#define MALLOC_SPECIFIC(x,c) malloc_specific(x,c)
#define MALLOC(x) malloc(x)
#define FREE(x) free(x)
#endif // sanity checks

#define ZERO(x) memset(x, 0, sizeof(*x))
//#define ZERO_QUEUE(x) memset(x, 0, sizeof(rt_priority_queue) + MAX_QUEUE * sizeof(rt_thread *))


// cause a GPF if this is ever followed as a pointer
#define SCHEDULER_POISON ((void*)0xdeadbeefb000b000ULL)

//
// Shared scheduler state
//
// Common to all cores
//
struct nk_sched_global_state {
    spinlock_t          lock;
    struct rt_list      *thread_list;
    uint64_t             num_threads;
    int                  reaping;
};

static volatile int scheduler_ready = 0;

static volatile uint64_t sync_count=0;
static volatile uint64_t tsc_start=-1ULL;

// only one core can do a world stop at a time
// this lock is also used to signal that we are starting
// or ending a world stop
// 0 => not stopping / no stopper
// k => stopping / stopper = k-1
static volatile uint64_t     stopping;
// all stopping cores synchronize via this barrier
static nk_counting_barrier_t stop_barrier;
// flags storage for the the core initiating the world stop
static volatile uint8_t      stop_flags;


static struct nk_sched_global_state global_sched_state;

//
// List implementation for use in scheduler
// Avoids changing thread structures
//
typedef struct nk_sched_thread_state rt_thread;
typedef struct rt_node {
    rt_thread      *thread;
    struct rt_node *next;
    struct rt_node *prev;
} rt_node;

static rt_node* rt_node_init(rt_thread *t);
static void     rt_node_deinit(rt_node *n);

typedef struct rt_list {
    rt_node *head;
    rt_node *tail;
} rt_list;

static rt_list*   rt_list_init();
static void       rt_list_deinit(rt_list *l);
static int        _rt_list_enqueue(rt_list *l, rt_thread *t, rt_node *newnode);
static int        rt_list_enqueue(rt_list *l, rt_thread *t);
static rt_thread* rt_list_dequeue(rt_list *l);
static rt_thread* rt_list_remove(rt_list *l, rt_node *n);
static rt_thread* rt_list_remove_search(rt_list *l, rt_thread *t);
static void       rt_list_map(rt_list *l, void (*func)(rt_thread *t, void *priv), void *priv);
static int        rt_list_empty(rt_list *l);



typedef enum { RUNNABLE_QUEUE = 0, 
	       PENDING_QUEUE = 1, 
	       APERIODIC_QUEUE = 2} queue_type;

//
// Queue specific to scheduler (circular buffer)
//
typedef struct rt_queue {
    queue_type type;
    uint64_t   size;        // number of elements currently in the queue
    uint64_t   head;        // index of newest element 
    uint64_t   tail;        // index of oldest element
    rt_thread *threads[MAX_QUEUE];
} rt_queue ;

static int        rt_queue_enqueue(rt_queue *queue, rt_thread *thread);
static rt_thread* rt_queue_dequeue(rt_queue *queue);
static rt_thread* rt_queue_peek(rt_queue *queue, uint64_t pos);
static rt_thread* rt_queue_remove(rt_queue *queue, rt_thread *thread);
static int        rt_queue_empty(rt_queue *queue);
static void       rt_queue_dump(rt_queue *queue, char *pre);

//
// Priority queues specific to scheduler
//
// Ordering is determined by thread->deadline, which means:
//   Runnable:  deadline (EDF queue)
//   Pending:   arrival time 
//   Aperiodic: priority 

typedef struct rt_priority_queue {
    queue_type type;
    uint64_t   size;
    rt_thread *threads[MAX_QUEUE];
} rt_priority_queue ;

static int        rt_priority_queue_enqueue(rt_priority_queue *queue, rt_thread *thread);
static rt_thread* rt_priority_queue_dequeue(rt_priority_queue *queue);
static rt_thread* rt_priority_queue_peek(rt_priority_queue *queue, uint64_t pos);
static rt_thread* rt_priority_queue_remove(rt_priority_queue *queue, rt_thread *thread);
static int        rt_priority_queue_empty(rt_priority_queue *queue);
static void       rt_priority_queue_dump(rt_priority_queue *queue, char *pre);

//
// Per-CPU scheduler state - hangs off off global cpu struct
//
//
typedef struct tsc_info {
    uint64_t sync_time;   // time at which this core finished synchronzing
    uint64_t sync_time_cycles; // sync_time in cycles
    uint64_t set_time;    // time when the next timer interrupt should occur
    uint64_t start_time;  // time from when the current thread starts running (exit from need_resched())
    uint64_t end_time;    // to when it stops (entry to need_resched())
} tsc_info;


typedef struct nk_sched_task_state {
    spinlock_t  lock;
    nk_wait_queue_t   *waitq;            // where the task thread blocks ultimately
    uint64_t           sized_enqueued;   // number of sized tasks enqueued
    uint64_t           sized_dequeued;   //   and dequeued (locally or remotely)
    struct list_head   sized_queue;      // tasks with known sizes
    uint64_t           unsized_enqueued; // number of unsized tasks enqueud
    uint64_t           unsized_dequeued; //   and dequeued (locally or remotely)
    struct list_head   unsized_queue;    // tasks with unknown sizes;
} task_info;

typedef struct nk_sched_percpu_state {
    spinlock_t             lock;
    struct nk_sched_config cfg; 
    rt_thread             *current;
    rt_priority_queue runnable;    // Periodic and sporadic threads that have arrived (and are runnable)
    rt_priority_queue pending;     // Periodic and sporadic threads that have not yet arrived
#if NAUT_CONFIG_APERIODIC_ROUND_ROBIN
    rt_queue          aperiodic;   // Aperiodic threads that are runnable
#endif
#if NAUT_CONFIG_APERIODIC_LOTTERY
    uint64_t          total_prob;  // total probability in the queue
    rt_queue          aperiodic;   // Aperiodic threads that are runnable
#endif
#if NAUT_CONFIG_APERIODIC_DYNAMIC_LIFETIME || NAUT_CONFIG_APERIODIC_DYNAMIC_QUANTUM
    rt_priority_queue aperiodic;   // Aperiodic threads that are runnable
#endif

    task_info tasks;       // tasks known to this local scheduler
    
    tsc_info tsc;

    uint64_t slack;        // allowed slop for scheduler execution itself

    uint64_t num_thefts;   // how many threads I've successfully stolen

    uint64_t reinject_count;  // how many timer/kick interrupts I've had to reinject

#if INSTRUMENT
    uint64_t resched_fast_num;
    uint64_t resched_fast_sum;
    uint64_t resched_fast_sum2;
    uint64_t resched_fast_min;
    uint64_t resched_fast_max;
    uint64_t resched_slow_num;
    uint64_t resched_slow_sum;
    uint64_t resched_slow_sum2;
    uint64_t resched_slow_min;
    uint64_t resched_slow_max;
    uint64_t resched_slow_noswitch_num;
    uint64_t resched_slow_noswitch_sum;
    uint64_t resched_slow_noswitch_sum2;
    uint64_t resched_slow_noswitch_min;
    uint64_t resched_slow_noswitch_max;
#endif

} rt_scheduler;

#if INSTRUMENT
#define INST_SCHED_IN(WHAT)  uint64_t _inst_start = rdtsc()
#define INST_SCHED_OUT(WHAT)						\
{									\
	uint64_t inst_diff = rdtsc() - _inst_start;			\
	scheduler->WHAT ## _num++;					\
	scheduler->WHAT##_sum+=inst_diff;				\
	scheduler->WHAT##_sum2+=inst_diff*inst_diff;			\
	if (scheduler->WHAT##_max < inst_diff) {			\
	    scheduler->WHAT##_max = inst_diff;				\
	}								\
	if (!scheduler->WHAT##_min || scheduler->WHAT##_min > inst_diff) { \
	    scheduler->WHAT##_min = inst_diff;				\
	}								\
    }

#define INST_DUMP(WHAT,BUF,LEN)						\
    snprintf(BUF,LEN,"+ rf:(n=%lu,a=%lu,v=%lu,m=%lu,M=%lu) rs:(n=%lu,a=%lu,v=%lu,m=%lu,M=%lu) rs-ns:(n=%lu,a=%lu,v=%lu,m=%lu,M=%lu)\n", \
	     WHAT->resched_fast_num,					\
	     WHAT->resched_fast_num>0 ? WHAT->resched_fast_sum/WHAT->resched_fast_num : 0, \
	     WHAT->resched_fast_num>0 ? ((WHAT->resched_fast_sum2)-(WHAT->resched_fast_sum*WHAT->resched_fast_sum)/WHAT->resched_fast_num)/WHAT->resched_fast_num : 0, \
	     WHAT->resched_fast_min,					\
	     WHAT->resched_fast_max,					\
	     WHAT->resched_slow_num,					\
	     WHAT->resched_slow_num>0 ? WHAT->resched_slow_sum/WHAT->resched_slow_num : 0, \
	     WHAT->resched_slow_num>0 ? ((WHAT->resched_slow_sum2)-(WHAT->resched_slow_sum*WHAT->resched_slow_sum)/WHAT->resched_slow_num)/WHAT->resched_slow_num : 0, \
	     WHAT->resched_slow_min,					\
	     WHAT->resched_slow_max,					\
	     WHAT->resched_slow_noswitch_num,				\
	     WHAT->resched_slow_noswitch_num>0 ? WHAT->resched_slow_noswitch_sum/WHAT->resched_slow_noswitch_num : 0, \
	     WHAT->resched_slow_noswitch_num>0 ? ((WHAT->resched_slow_noswitch_sum2)-(WHAT->resched_slow_noswitch_sum*WHAT->resched_slow_noswitch_sum)/WHAT->resched_slow_noswitch_num)/WHAT->resched_slow_noswitch_num : 0, \
	     WHAT->resched_slow_noswitch_min,				\
	     WHAT->resched_slow_noswitch_max);				\
    
#else
#define INST_SCHED_IN(s) 
#define INST_SCHED_OUT(s)						
#define INST_DUMP(s,b,n)
#endif


#if NAUT_CONFIG_APERIODIC_ROUND_ROBIN || NAUT_CONFIG_APERIODIC_LOTTERY
// This handles the special case where the idle thread is at the
// head of the queue, but there is another thread behind it
// in which case want to skip the idle thread.  
// Note that for the other aperiodic scheduling models, we
// handle idle by giving it the lowest possible priority, 
// allowing us to skip this kind of logic
#if NAUT_CONFIG_APERIODIC_ROUND_ROBIN 
#define GET_NEXT_APERIODIC(s) round_robin_get_next_aperiodic(s)
#define PUT_APERIODIC(s,t) round_robin_put_aperiodic(s,t)
#define REMOVE_APERIODIC(s,t) round_robin_remove_aperiodic(s,t)
#else  // NAUT_CONFIG_APERIODIC_LOTTERY
#define GET_NEXT_APERIODIC(s) lottery_get_next_aperiodic(s)
#define PUT_APERIODIC(s,t) lottery_put_aperiodic(s,t)
#define REMOVE_APERIODIC(s,t) lottery_remove_aperiodic(s,t)
#endif
#define HAVE_APERIODIC(s) (!rt_queue_empty(&(s)->aperiodic))
#ifdef NAUT_CONFIG_DEBUG_SCHED
#if DUMP_SCHED_STATE
#define DUMP_APERIODIC(s,p) rt_queue_dump(&(s)->aperiodic,p)
#else
#define DUMP_APERIODIC(s,p) 
#endif
#else
#define DUMP_APERIODIC(s,p) 
#endif
#define PEEK_APERIODIC(s,k) rt_queue_peek(&(s)->aperiodic,k)
#define SIZE_APERIODIC(s) ((s)->aperiodic.size)
#else
#define GET_NEXT_APERIODIC(s) rt_priority_queue_dequeue(&(s)->aperiodic)
#define PUT_APERIODIC(s,t) rt_priority_queue_enqueue(&(s)->aperiodic,t)
#define REMOVE_APERIODIC(s,t) rt_priority_queue_remove(&(s)->aperiodic,t)
#define PEEK_APERIODIC(s,k) rt_priority_queue_peek(&(s)->aperiodic,k)
#define SIZE_APERIODIC(s) ((s)->aperiodic.size)
#define HAVE_APERIODIC(s) (!rt_priority_queue_empty(&(s)->aperiodic))
#ifdef NAUT_CONFIG_DEBUG_SCHED
#if DUMP_SCHED_STATE
#define DUMP_APERIODIC(s,p) rt_priority_queue_dump(&(s)->aperiodic,p)
#else
#define DUMP_APERIODIC(s,p) 
#endif
#else
#define DUMP_APERIODIC(s,p) 
#endif
#endif

#define GET_NEXT_RT_PENDING(s)   rt_priority_queue_dequeue(&(s)->pending)
#define PUT_RT_PENDING(s,t) rt_priority_queue_enqueue(&(s)->pending,t)
#define REMOVE_RT_PENDING(s,t) rt_priority_queue_remove(&(s)->pending,t)
#define HAVE_RT_PENDING(s) (!rt_priority_queue_empty(&(s)->pending))
#define PEEK_RT_PENDING(s) (s->pending.threads[0])
#ifdef NAUT_CONFIG_DEBUG_SCHED
#if DUMP_SCHED_STATE
#define DUMP_RT_PENDING(s,p) rt_priority_queue_dump(&(s)->pending,p)
#else
#define DUMP_RT_PENDING(s,p)
#endif
#else
#define DUMP_RT_PENDING(s,p)
#endif

#define GET_NEXT_RT(s)   rt_priority_queue_dequeue(&(s)->runnable)
#define PUT_RT(s,t) rt_priority_queue_enqueue(&(s)->runnable,t)
#define REMOVE_RT(s,t) rt_priority_queue_remove(&(s)->runnable,t)
#define HAVE_RT(s) (!rt_priority_queue_empty(&(s)->runnable))
#define PEEK_RT(s) (s->runnable.threads[0])
#ifdef NAUT_CONFIG_DEBUG_SCHED
#if DUMP_SCHED_STATE
#define DUMP_RT(s,p) rt_priority_queue_dump(&(s)->runnable,p)
#else
#define DUMP_RT(s,p)
#endif
#else
#define DUMP_RT(s,p)
#endif

//
// Per-thread state (abstracted from overall thread context
// which remains in struct nk_thread).  thread.[ch] are responsible
// for wait queues and events.  From this sccheduler's perspective
// we only care about whether we are runnable or not

typedef struct nk_sched_constraints  rt_constraints;
typedef nk_sched_constraint_type_t rt_type;

typedef enum { ARRIVED=0,          // no admission control done
	       ADMITTED,           // admitted
	       CHANGING,           // admitted for new constraints, now changing to them
	       YIELDING,           // explit yield of slice
	       SLEEPING,           // being removed from RT and non-RT run/arrival queues
                                   // probably due to having been put into a wait queue
	       EXITING,            // being removed from RT and non-RT run/arrival queues
                                   // will not return
	       DENIED,             // not admitted
	       REAPABLE,           // it's OK for the reaper to destroy the thread
             } rt_status;

typedef struct nk_sched_thread_state {
    // how this thread is to be scheduled
    struct nk_sched_constraints constraints;
    // 
    rt_status status;
    // which queue the thread is currently on
    queue_type q_type;
    
    int      is_intr;      // this is an interrupt thread
    int      is_task;      // this is a task thread

    uint64_t start_time;   // when last started
    uint64_t cur_run_time; // how long it has run without being preempted
    uint64_t run_time;     // how long it has run so far
                           // full duration for aperiodic and sporadic
                           // current slice for periodic
    uint64_t deadline;     // current deadline / time of next arrival if pending
                           // for an aperiodic task, this is its current dynamic
                           // priority 
    uint64_t exit_time;    // time of actual completion / arrival

    // Statistics are reset when the constraints are changed
    uint64_t arrival_count;   // how many times it has arrived (1 for aperiodic/sporadic)
    uint64_t resched_count;   // how many times resched was invoked on this thread
    uint64_t resched_long_count; // how many times the long path was taken for the thread
    uint64_t switch_in_count; // number of times switched to
    uint64_t miss_count;      // number of deadline misses
    uint64_t miss_time_sum;   // sum of missed time
    uint64_t miss_time_sum2;  // sum of squares of missed time

    // the thread context itself
    struct nk_thread *thread;

    // the thread node in a thread list (the global thread list)
    struct rt_node   *list; 

} rt_thread ;

static void       rt_thread_dump(rt_thread *thread, char *prefix);
static int        rt_thread_admit(rt_scheduler *scheduler, rt_thread *thread, uint64_t now);
static int        rt_thread_check_deadlines(rt_thread *t, rt_scheduler *scheduler, uint64_t now);
static void       rt_thread_update_periodic(rt_thread *t, rt_scheduler *scheduler, uint64_t now);
static void       rt_thread_update_sporadic(rt_thread *t, rt_scheduler *scheduler, uint64_t now);
static void       rt_thread_update_aperiodic(rt_thread *thread, rt_scheduler *scheduler, uint64_t now);



// helpers
static uint64_t       cur_time();


static void           set_timer(rt_scheduler *scheduler, 
				rt_thread *thread, 
				uint64_t now);

static void           handle_special_switch(rt_status what, int have_lock, uint8_t flags, void (*release_callback)(void*), void *release_state);

static inline uint64_t get_min_per(rt_priority_queue *runnable, rt_priority_queue *queue, rt_thread *thread);
static inline uint64_t get_avg_per(rt_priority_queue *runnable, rt_priority_queue *pending, rt_thread *thread);
static inline uint64_t get_periodic_util_rms_limit(uint64_t count);
static inline void     get_periodic_util(rt_scheduler *sched, uint64_t *util, uint64_t *count);
static inline void     get_sporadic_util(rt_scheduler *sched, uint64_t now, uint64_t *util, uint64_t *count);
static inline uint64_t get_random();



static void print_thread(rt_thread *r, void *priv)
{
    ASSERT(r);
    ASSERT(r->thread);

    nk_thread_t  * t = r->thread;
    int cpu = (int)(uint64_t)priv;


#define US(ns) ((ns)/1000ULL)
#define MS(ns) ((ns)/1000000ULL)

#define CO(ns) MS(ns)

    if (cpu==t->current_cpu || cpu<0) { 

	nk_vc_printf("%llut %lur %dc%s %s %s %s %llus %lluc %llur %llud %llue", 
		     t->tid, 
		     t->refcount,
		     t->current_cpu,
		     t->bound_cpu>=0 ? "b" : "",
		     t->is_idle ? "(idle)" : t->name[0]==0 ? "(noname)" : t->name,
		     t->status==NK_THR_INIT ? "ini" :
		     t->status==NK_THR_RUNNING ? "RUN" :
		     t->status==NK_THR_WAITING ? "wai" :
		     t->status==NK_THR_SUSPENDED ? "sus" :
		     t->status==NK_THR_EXITED ? "exi" : "UNK",
		     r->status==ARRIVED ? "arr" :
		     r->status==ADMITTED ? "adm" :
		     r->status==CHANGING ? "cha" :
		     r->status==YIELDING ? "yie" :
		     r->status==EXITING ? "exi" :
		     r->status==SLEEPING ? "sle" :
		     r->status==DENIED ? "den" :
		     r->status==REAPABLE ? "rea" : "UNK",
		     CO(r->start_time),
		     CO(r->cur_run_time),
		     CO(r->run_time),
		     CO(r->deadline),
		     CO(r->exit_time));
	
	switch (r->constraints.type) {
	case APERIODIC:
	    nk_vc_printf(" aperiodic(%utp, %llu)", r->constraints.interrupt_priority_class,CO(r->constraints.aperiodic.priority));
	    break;
	case SPORADIC:
	    nk_vc_printf(" sporadic(%utp, %llu)", r->constraints.interrupt_priority_class,CO(r->constraints.sporadic.size));
	    break;
	case PERIODIC:
	    nk_vc_printf(" periodic(%utp, %llu,%llu)", r->constraints.interrupt_priority_class,CO(r->constraints.periodic.period), CO(r->constraints.periodic.slice));
	    break;
	}

	nk_vc_printf(" stats: %llua %llure %llurl %llusw %llum",
		     r->arrival_count,
		     r->resched_count,
		     r->resched_long_count,
		     r->switch_in_count,
		     r->miss_count);

	nk_vc_printf(" [%s]", r->thread->aspace ? r->thread->aspace->name : "default");
	
	nk_vc_printf("\n");

    }
}


static uint64_t   reap_count;
static rt_thread *reap_pool[MAX_QUEUE];

static void pre_reap_thread(rt_thread *r, void *priv)
{
    ASSERT(r);
    ASSERT(r->thread);

    nk_thread_t  * t = r->thread;
    rt_node      * temp;

    if (reap_count<MAX_QUEUE && !t->refcount && t->status==NK_THR_EXITED && r->status==REAPABLE) {
	DEBUG("Reaping tid %llu (%s)\n",t->tid,t->name);
	reap_pool[reap_count++] = r;
    }
}


void nk_sched_dump_cores(int cpu_arg)
{
    LOCAL_LOCK_CONF;

    int cpu;

    char *intr_model="UNK";

#ifdef NAUT_CONFIG_INTERRUPT_THREAD
#ifdef NAUT_CONFIG_INTERRUPT_THREAD_ALLOW_IDLE
    intr_model = "it+ii";
#else
    intr_model = "it";
#endif
#else
    intr_model = "ip";
#endif

    struct sys_info * sys = per_cpu_get(system);
    rt_scheduler *s;

    for (cpu=0;cpu<sys->num_cpus;cpu++) { 
	if (cpu_arg<0 || cpu_arg==cpu) {
	    char buf[256];
	    struct apic_dev *apic = sys->cpus[cpu]->apic;
	    struct nk_aspace *aspace = sys->cpus[cpu]->cur_aspace;

	    s = sys->cpus[cpu]->sched_state;
	    LOCAL_LOCK(s);
	    snprintf(buf,256,"%dc %s %unl %luin %luex %luri %lut %s %utp %lup %lur %lua %lum (%s) (%luul %lusp %luap %luaq %luadp) (%luste %lustd %luute %luutd) (%luapic) [%s]\n",
		     cpu, 
		     intr_model,
		     sys->cpus[cpu]->interrupt_nesting_level,
		     sys->cpus[cpu]->interrupt_count,
		     sys->cpus[cpu]->exception_count,
		     sys->cpus[cpu]->sched_state->reinject_count,
		     s->current->thread->tid, 
		     s->current->thread->is_idle ? "(idle)" : s->current->thread->name[0] ? s->current->thread->name : "(noname)",
		     s->current->thread->sched_state->constraints.interrupt_priority_class,
		     s->pending.size, s->runnable.size, s->aperiodic.size,
		     s->num_thefts,
		     
#if NAUT_CONFIG_APERIODIC_ROUND_ROBIN
		     "RR",
#endif
		     
#if NAUT_CONFIG_APERIODIC_DYNAMIC_QUANTUM 
		     "DQ",
#endif
		     
#if NAUT_CONFIG_APERIODIC_DYNAMIC_LIFETIME
		     "DL",
#endif
		     
#if NAUT_CONFIG_APERIODIC_LOTTERY
		     "LO",
#endif
		     s->cfg.util_limit,
		     s->cfg.sporadic_reservation, s->cfg.aperiodic_reservation, 
		     s->cfg.aperiodic_quantum, s->cfg.aperiodic_default_priority,
		     s->tasks.sized_enqueued, s->tasks.sized_dequeued,
		     s->tasks.unsized_enqueued, s->tasks.unsized_dequeued,
		     apic->timer_count,
		     aspace ? aspace->name : "default");
#if INSTRUMENT
	    char buf2[256];
            INST_DUMP(s,buf2,256);
#endif
	    LOCAL_UNLOCK(s);

	    nk_vc_printf(buf);
#if INSTRUMENT
	    nk_vc_printf(buf2);
#endif
	}
    }
}

void nk_sched_dump_time(int cpu_arg)
{
    int cpu;

    struct sys_info * sys = per_cpu_get(system);
    struct tsc_info *tsc0 = &sys->cpus[0]->sched_state->tsc;

    for (cpu=0;cpu<sys->num_cpus;cpu++) { 
	if (cpu_arg<0 || cpu_arg==cpu) {

	    struct apic_dev *apic = sys->cpus[cpu]->apic;
	    struct tsc_info *tsc = &sys->cpus[cpu]->sched_state->tsc;
			 
            nk_vc_printf("%dc %luhz %luppt %lucpu %lucpt %uts %uct %lutc %lust %lustc %ldstr %ldstrc\n",
			 cpu, apic->bus_freq_hz, apic->ps_per_tick,
			 apic->cycles_per_us, apic->cycles_per_tick,
			 apic->timer_set, apic->current_ticks, apic->timer_count,
			 tsc->sync_time, tsc->sync_time_cycles,
			 tsc->sync_time - tsc0->sync_time,
			 tsc->sync_time_cycles - tsc0->sync_time_cycles);
	}
    }
}

void nk_sched_dump_threads(int cpu)
{
    GLOBAL_LOCK_CONF;
    struct sys_info *sys = per_cpu_get(system);
    struct apic_dev *apic = sys->cpus[my_cpu_id()]->apic;

    GLOBAL_LOCK();

    rt_list_map(global_sched_state.thread_list,print_thread,(void*)(long)cpu);

    GLOBAL_UNLOCK();
}

void nk_sched_map_threads(int cpu, void (func)(struct nk_thread *t, void *state), void *state)
{
    GLOBAL_LOCK_CONF;
   
    GLOBAL_LOCK();

    rt_node *n = global_sched_state.thread_list->head;
    while (n != NULL) {
	if (cpu==-1 || n->thread->thread->current_cpu==cpu) { 
	    func(n->thread->thread,state);
	}
        n = n->next;
    }

    GLOBAL_UNLOCK();
}

/* KCH NOTE: The following helper functions *currently* assume that they will
 * be called only from the map_sibling* functions below, thus locking is left out.
 * DO NOT use them individually, otherwise you'll see race conditions.
 */
uint8_t nk_topo_threads_share_hwthread (struct nk_thread * a, struct nk_thread * b)
{
    uint8_t res;
    res = a->current_cpu == b->current_cpu;
    return res;
}

uint8_t nk_topo_thread_shares_hwthread_with_me (struct nk_thread * other)
{
    return nk_topo_threads_share_hwthread(get_cur_thread(), other);
}

uint8_t nk_topo_threads_share_core (struct nk_thread * a, struct nk_thread * b)
{
    uint8_t res;
    struct sys_info *sys = per_cpu_get(system);
    res = nk_topo_cpus_share_phys_core(sys->cpus[a->current_cpu], sys->cpus[b->current_cpu]);
    return res;
}

uint8_t nk_topo_thread_shares_core_with_me (struct nk_thread * other)
{
    return nk_topo_threads_share_core(get_cur_thread(), other);
}

uint8_t 
nk_topo_threads_share_socket (struct nk_thread * a, struct nk_thread * b)
{
    uint8_t res;
    struct sys_info *sys = per_cpu_get(system);
    res = nk_topo_cpus_share_socket(sys->cpus[a->current_cpu], sys->cpus[b->current_cpu]);
    return res;
}

uint8_t 
nk_topo_thread_shares_socket_with_me (struct nk_thread * other)
{
    return nk_topo_threads_share_socket(get_cur_thread(), other);
}


static uint8_t (*const thread_filter_funcs[])(struct nk_thread*, struct nk_thread*) =
{ 
    [NK_TOPO_HW_THREAD_FILT] = nk_topo_threads_share_hwthread,
    [NK_TOPO_PHYS_CORE_FILT] = nk_topo_threads_share_core,
    [NK_TOPO_SOCKET_FILT]    = nk_topo_threads_share_socket,
};

// Map a function to all other threads on the same X, where X can be hwthread, physical core, or socket
void nk_topo_map_sibling_threads(void (func)(struct nk_thread *t, void *state), nk_topo_filt_t filter, void *state)
{
    GLOBAL_LOCK_CONF;
    GLOBAL_LOCK();

    rt_node *n = global_sched_state.thread_list->head;

    while (n != NULL) {
        if (n->thread->thread != get_cur_thread()) { // skip myself
            if (filter == NK_TOPO_ALL_FILT ||
                    thread_filter_funcs[filter](get_cur_thread(), n->thread->thread)) {
                func(n->thread->thread, state);
            }
        }

        n = n->next;
    }

    GLOBAL_UNLOCK();
}

void nk_topo_map_hwthread_sibling_threads(void (func)(struct nk_thread *t, void *state), void *state)
{
    nk_topo_map_sibling_threads(func, NK_TOPO_HW_THREAD_FILT, state);
}

void nk_topo_map_core_sibling_threads(void (func)(struct nk_thread *t, void *state), void *state)
{
    nk_topo_map_sibling_threads(func, NK_TOPO_PHYS_CORE_FILT, state);
}

void nk_topo_map_socket_sibling_threads(void (func)(struct nk_thread *t, void *state), void *state)
{
    nk_topo_map_sibling_threads(func, NK_TOPO_SOCKET_FILT, state);
}

static void
topo_test (struct nk_thread * t, void * state)
{
    nk_vc_printf("THREADMAPPER: Applying func to thread %x\n", t->tid);
}

static int
handle_threadtopotest (char * buf, void * priv)
{
    char aps;
    if (((aps='a', strcmp(buf,"threadtopotest a"))==0) ||
			((aps='l', strcmp(buf,"threadtopotest l"))==0) ||
			((aps='p', strcmp(buf,"threadtopotest p"))==0) ||
			((aps='s', strcmp(buf,"threadtopotest s"))==0)) {
        switch (aps) { 
            case 'a': 
				nk_vc_printf("Mapping func to all siblings\n");
                nk_topo_map_sibling_threads(topo_test,  NK_TOPO_ALL_FILT, NULL);
                break;
            case 'l': 
				nk_vc_printf("Mapping func to logical core siblings\n");
                nk_topo_map_hwthread_sibling_threads(topo_test,  NULL);
                break;
            case 'p': 
				nk_vc_printf("Mapping func to core siblings\n");
                nk_topo_map_core_sibling_threads(topo_test, NULL);
                break;
            case 's': 
				nk_vc_printf("Mapping func to socket siblings\n");
                nk_topo_map_socket_sibling_threads(topo_test, NULL);
                break;
            default:
                nk_vc_printf("Unknown threadtopotest command requested\n");
                return -1;
        }
    }

    return 0;
}

static struct shell_cmd_impl threadtopo_impl = {
    .cmd      = "threadtopotest",
    .help_str = "threadtopotest <a|s|p|l>",
    .handler  = handle_threadtopotest,
};
nk_register_shell_cmd(threadtopo_impl);


void nk_sched_stop_world()
{
    // this must happen before any of the lookups...
    if (!scheduler_ready) {
        return;
    }

    
    uint64_t num_cpus = nk_get_num_cpus();
    uint64_t my_cpu_id = my_cpu_id();
    uint64_t stopper = my_cpu_id+1;
    uint64_t i;


    
    // scheduler cannot stop us
    // note that an interrupt will still land us in the
    // stop-world test in need_resched
    preempt_disable();
    // wait until we are the sole world stopper
    // perhaps participating in other world stops along the way
    PAUSE_WHILE(!__sync_bool_compare_and_swap(&stopping,0,stopper));
    
    // Now we want to make sure nothing can interrupt us
    // and we might as well reset the scheduler now as well
    stop_flags = irq_disable_save();
    preempt_enable();  // interrupts are still off - scheduler is not going to preempt us
    
    // kick everyone else to get them to stop
    for (i=0;i<num_cpus;i++) { 
	if (i!=my_cpu_id) {
	    nk_sched_kick_cpu(i);
	}
    }

    // wait for them all to stop
    nk_counting_barrier(&stop_barrier);

}

void nk_sched_start_world()
{
    if (!scheduler_ready) {
      return;
    }

    // indicate that we are restarting the world
    __sync_fetch_and_and(&stopping,0);

    // wait for them to notice 
    nk_counting_barrier(&stop_barrier);
    
    // now allow interrupts again locally
    // so the scheduler can preempt us
    irq_enable_restore(stop_flags);
    
}


struct thread_query {
    uint64_t     tid;
    nk_thread_t *thread;
};

static void find_thread(rt_thread *r, void *priv)
{
    nk_thread_t  * t = r->thread;
    struct thread_query *q = (struct thread_query *)priv;

    if (t->tid == q->tid) { 
	q->thread = t;
    }
}


struct nk_thread *nk_find_thread_by_tid(uint64_t tid)
{
    GLOBAL_LOCK_CONF;
    struct thread_query q;
    
    q.tid=tid;
    q.thread=0;

    GLOBAL_LOCK();

    rt_list_map(global_sched_state.thread_list,find_thread,(void*)&q);

    GLOBAL_UNLOCK();

    return q.thread;
}

void nk_sched_reap(int uncond)
{
    DEBUG("Executing Reap (%s)\n", uncond? "UNCOND": "cond");
    
    GLOBAL_LOCK_CONF;
    uint64_t i;

    if (in_interrupt_context()) {
	// never reap in interrupt context, even unconditionally
	// if this means a memory allocation or thread creation occuring
	// from an interrupt handler fails, so be it
	DEBUG("Ignoring %sconditional reap while in interrupt context\n", uncond ? "un" : "");
	return;
    }

    if (!uncond && global_sched_state.num_threads < ((NAUT_CONFIG_MAX_THREADS * 95)/100)) {
	// unless we are unconditionally reaping, do not reap if we still
	// have a lot of threads left....
	DEBUG("Ignoring conditional reap with %lu threads\n", global_sched_state.num_threads);
	return;
    }

    if (!__sync_bool_compare_and_swap(&global_sched_state.reaping,0,1)) {
	// reaping pass is already in progress elsewhere
	// we will wait for it to  inish.  In this way, our caller
	// will also see the benefit of that pass
	DEBUG("%sconditional reap waiting on previous reap to complete\n", uncond ? "un" : "");
	while (__sync_fetch_and_or(&global_sched_state.reaping,0)) {
	    // wait for the reapping pass to finish
	}
	DEBUG("%sconditional reap - previous reap now complete, ignoring this reap\n", uncond ? "un" : "");
	return;
    }

    DEBUG("Reap begins (%lu threads)\n", global_sched_state.num_threads);

    GLOBAL_LOCK();

    reap_count = 0;

    // We need to do this in two phases since
    // destroy thread also needs the global lock
    // first phase, collect
    rt_list_map(global_sched_state.thread_list,pre_reap_thread,0);

    GLOBAL_UNLOCK();

    // Now reap
    // We are still holding global_sched_state.reaping, so
    // we must have exclusive access to the reap_pool built
    // in the previous step
    if (reap_count!=0) { 
	// reverse order to potentially improve frees
	for (i=reap_count;i>0;i--) { 
	    DEBUG("Reaping thread %lu\n", reap_pool[i-1]->thread->tid);
	    // thread destruction calls back to pre_destory, which
	    // will acquire the global lock when it removes the thread from
	    // the global thread list, hence we do not need to hold
	    // global lock here.
	    nk_thread_destroy(reap_pool[i-1]->thread);
	}
    }

    DEBUG("%sconditional reap ends (%lu threads)\n", uncond ? "un" : "", global_sched_state.num_threads);
    
    // done with reaping - another core can now go
    __sync_fetch_and_and(&global_sched_state.reaping,0);
}

//
// The current implementation is essentially a specialized reaping pass,
// looking in reverse order on the logic that recent threads will be similar
// to the requested thread
//
// Eventually, we will move threads to separate per-cpu queues and perhaps
// do a smarter search for ones with the required constraints
//
//
struct nk_thread *nk_sched_reanimate(nk_stack_size_t min_stack_size,
				     int             placement_cpu)
{
    DEBUG("Reanimation request for a thread of stack minimum size %lu for CPU %d\n",
	 min_stack_size, placement_cpu);
    
    GLOBAL_LOCK_CONF;

    if (in_interrupt_context()) {
	DEBUG("Reanimation request while in interrupt context ignored\n");
	return 0;
    }

    // We are currently overloaded with reaping, so we will wait until
    // we are the sole "reaper"
    while (!__sync_bool_compare_and_swap(&global_sched_state.reaping,0,1)) {
    }
    DEBUG("Reanimation search begins (%lu threads)\n", global_sched_state.num_threads);

    GLOBAL_LOCK();

    // start search from the oldest thread
    rt_node *cur = global_sched_state.thread_list->head;

    uint64_t count = 0;
    
    while (cur &&
	   !(cur->thread->status == REAPABLE &&
	     cur->thread->thread->status==NK_THR_EXITED &&
	     !cur->thread->thread->refcount &&
	     cur->thread->thread->stack_size >= min_stack_size &&
	     (placement_cpu<0 || cur->thread->thread->placement_cpu==placement_cpu))) {
	
	//DEBUG("Skipping thread %p (%lu) \"%s\"\n", cur->thread->thread, cur->thread->thread->tid,
	//      cur->thread->thread->is_idle ? "*idle*" ::q!
	//      cur->thread->thread->name[0] ? cur->thread->thread->name : "(no name)");
	cur=cur->next;
	count++;
    }
    
    
    rt_thread *rt = 0;
    
    
    if (cur) {
	rt = rt_list_remove(global_sched_state.thread_list, cur);
	global_sched_state.num_threads--;
	DEBUG("Examined %lu threads before finding a matching one\n", count+1); 
    } else {
	DEBUG("Examined %lu threads without finding a matching one\n", count);
    }
    
    GLOBAL_UNLOCK();

    // done with "reaping" - another core can now go
    __sync_fetch_and_and(&global_sched_state.reaping,0);
    
    if (rt) {
	DEBUG("Reanimation successful - returning thread %p (sched state %p name \"%s\")\n", rt->thread, rt, rt->thread->name);
	return rt->thread;
    } else {
	DEBUG("Reanimation attempt failed\n");
	return 0;
    }
}

void nk_sched_thread_state_deinit(struct nk_thread *thread)
{
    FREE(thread->sched_state);
    thread->sched_state=0;
}

struct nk_thread *nk_sched_get_cur_thread_on_cpu(int cpu)
{
    if (!scheduler_ready) {
        return 0;
    }
    
    if (cpu>=0 && cpu<nk_get_num_cpus()) {
        struct sys_info *sys = per_cpu_get(system);
        return sys->cpus[cpu]->sched_state->current->thread;
    } else {
        return 0;
    }
}

struct nk_sched_thread_state *nk_sched_thread_state_init(struct nk_thread *thread,
							 struct nk_sched_constraints *constraints)
{
    struct nk_sched_thread_state *t;
    
    if (thread->sched_state) {
	// this is a reanimated thread, so no allocation is done
	t = (struct nk_sched_thread_state *)thread->sched_state;
    } else {
	t = (struct nk_sched_thread_state *)MALLOC_SPECIFIC(sizeof(struct nk_sched_thread_state),thread->current_cpu);
    }
    
    struct sys_info *sys = per_cpu_get(system);
    rt_scheduler *sched = sys->cpus[my_cpu_id()]->sched_state;
    struct nk_sched_constraints *c;
    struct nk_sched_constraints default_constraints = 
	{ .type = APERIODIC, 
	  .aperiodic.priority = sched->cfg.aperiodic_default_priority };

    if (!t) { 
	ERROR("Cannot allocate scheduler thread state\n");
	return NULL;
    }

    ZERO(t);

    if (!constraints) { 
	constraints = &default_constraints;
    }

    t->constraints = *constraints; 

    c = &t->constraints;

    t->status = ARRIVED;

    t->start_time = 0;
    t->run_time = 0;
    t->deadline = 0;


    if (c->type == PERIODIC) {
	t->deadline = cur_time() + c->periodic.period;
    } else if (c->type == SPORADIC) {
        t->deadline = cur_time() + c->sporadic.deadline;
    }
    
    t->thread = thread;

    return t;
}

int nk_sched_initial_placement()
{
    struct sys_info * sys = per_cpu_get(system);
    return (int)(get_random() % sys->num_cpus);
}

int nk_sched_thread_post_create(nk_thread_t * t)
{
    GLOBAL_LOCK_CONF;

    nk_sched_reap(0); // conditional reap to make room for new thread

    // the caller is expected to have already set current_cpu!

    // we allocate the new thread node here because a malloc inside
    // of the global list enqueue could itself trigger a reap
    // in which case we would deadlock, since reaping also acquires the
    // global scheduler lock

    rt_node *n = rt_node_init(t->sched_state); 
    
    DEBUG("create: node %p for thread %p (%lu)\n",n,t->sched_state,t->sched_state->thread->tid);
    
    if (!n) {
    	ERROR("Failed to allocate rt_node in post create\n");
    	return 0;
    }
    
    // now we can safely acquire the lock and put the new thread
    // on the global thread list
    
    GLOBAL_LOCK();

    if (global_sched_state.num_threads >= MAX_QUEUE) {
	DEBUG("Scheduler vetos thread creation as there are %lu active threads in system\n", global_sched_state.num_threads);
	DEBUG("You can increase the maximum of %lu active threads using NAUT_CONFIG_MAX_THREADS\n", NAUT_CONFIG_MAX_THREADS);
	GLOBAL_UNLOCK();
	rt_node_deinit(n);
	return -1;
    }
    
    // this enqueue avoids any malloc and thus no reap should be
    // triggered by it
    if (_rt_list_enqueue(global_sched_state.thread_list, t->sched_state, n)) {
	ERROR("Failed to add new thread to global thread list\n");
	GLOBAL_UNLOCK();
	return -1;
    }
    global_sched_state.num_threads++;

    DEBUG("Post Create of thread %p (%d) [numthreads=%d]\n",
	  t, t->tid, global_sched_state.num_threads);
    
    GLOBAL_UNLOCK();

    return 0;
}



int nk_sched_thread_pre_destroy(nk_thread_t * t)
{
    rt_thread *r;
    GLOBAL_LOCK_CONF;
    
    GLOBAL_LOCK();

    if (!(r=rt_list_remove(global_sched_state.thread_list,t->sched_state->list))) {
	ERROR("Failed to remove thread from global list....\n");
	GLOBAL_UNLOCK();
	return -1;
    }

    global_sched_state.num_threads--;
    
    GLOBAL_UNLOCK();
    
    return 0;
}

#if NAUT_CONFIG_APERIODIC_ROUND_ROBIN

static inline int round_robin_put_aperiodic(rt_scheduler *s, rt_thread *t)
{
    return rt_queue_enqueue(&s->aperiodic,t);
}

static inline rt_thread *round_robin_get_next_aperiodic(rt_scheduler *s)
{
    rt_thread *r = rt_queue_dequeue(&s->aperiodic);

    // skip idle thread if possible
    if (r->thread->is_idle && s->aperiodic.size>=1) {
	rt_queue_enqueue(&s->aperiodic,r); 
	r = rt_queue_dequeue(&s->aperiodic);
    }

    return r; 
}

static inline rt_thread *round_robin_remove_aperiodic(rt_scheduler *s, rt_thread *t) 
{
    return rt_queue_remove(&s->aperiodic,t);
}

#endif

static inline uint64_t get_random()
{
    uint64_t t;
    nk_get_rand_bytes((uint8_t *)&t,sizeof(t));
    return t;
}

#if NAUT_CONFIG_APERIODIC_LOTTERY


static inline int lottery_put_aperiodic(rt_scheduler *s, rt_thread *t)
{
    int rc = rt_queue_enqueue(&s->aperiodic,t);

    if (!rc) { 
	s->total_prob += t->constraints.aperiodic.priority;
    }
    
    //    printk("LO: put %p (%s), now with total prob=%lu\n",t->thread->tid,t->thread->name,s->total_prob);
    return rc;
    
}


static inline rt_thread *lottery_get_next_aperiodic(rt_scheduler *s)
{
    // this is the dumb, obvious algorithm (linear in # threads in queue)
    rt_thread *t=NULL;
    uint64_t cur;
    uint64_t pos;
    rt_queue *q=&s->aperiodic;
    uint64_t target_prob;
    uint64_t cum_prob;

    ASSERT(s->total_prob);
    ASSERT(q->size);

    target_prob = get_random() % s->total_prob;

    for (cur=0, cum_prob=0;
	 cur<q->size;
	 cur++) {
	pos = (q->tail + cur) % MAX_QUEUE;
	cum_prob += q->threads[pos]->constraints.aperiodic.priority;
	if (cum_prob>=target_prob) {
	    // don't pick the idle thread if it can be avoided
	    if (q->threads[pos]->thread->is_idle && q->size>1) { 
		// there is at least one other thread
		if (cur<(q->size-1)) {
		    // pick the very next one if possible
		    cur++;
		} else {
		    // pick the previous one if not
		    // cur must be >0
		    cur--;
		}
		// update position
		pos = (q->tail + cur) % MAX_QUEUE;
	    }
	    break;
	}
    }

    if (cur==q->size) { 
	panic("Cannot find thread in lottery scheduler\n");
	return 0;
    }

    // grab the thread we will return
    t = q->threads[pos];


    // cur => position within the queue (0=oldest, ...)
    // pos => position within the array
    // We now need to "copy down" everything above this position

    uint64_t i, p, n;
    for (i=cur;
	 i<(q->size-1);
	 i++) {
	
	p = (q->tail + i) % MAX_QUEUE;
	n = (q->tail + i + 1 ) % MAX_QUEUE;
	q->threads[p] = q->threads[n];
    }

    // decrement head
    q->head = (q->head + MAX_QUEUE - 1) % MAX_QUEUE;
    
    q->size--;

    s->total_prob -= t->constraints.aperiodic.priority;


    return t;
}

static inline rt_thread *lottery_remove_aperiodic(rt_scheduler *s, rt_thread *t)
{
    rt_thread *r = rt_queue_remove(&s->aperiodic,t);

    if (r) { 
	s->total_prob -= t->constraints.aperiodic.priority;
    }

    return r;
}

#endif

static int    _sched_make_runnable(struct nk_thread *thread, int cpu, int admit, int have_lock)
{
    LOCAL_LOCK_CONF;
    struct sys_info * sys = per_cpu_get(system);
    rt_thread *t = thread->sched_state;
    rt_scheduler *s;

    if (unlikely(cpu <= CPU_ANY || 
        cpu >= sys->num_cpus)) {
        s = per_cpu_get(sched_state);
    } else {
        s = sys->cpus[cpu]->sched_state;
    }

    if (!s) {
	return -1;
    }

    if (!have_lock) {
	LOCAL_LOCK(s);
    }

    if (admit) {
	if (rt_thread_admit(s,t,cur_time())) { 
	    DEBUG("Failed to admit thread\n");
	    goto out_bad;
	} else {
	    DEBUG("Admitted thread %p (tid=%d)\n",thread,thread->tid);
	}
    }

    // Admission will have reset the thread state and stats

    switch (t->constraints.type) {
    case APERIODIC:
	if (PUT_APERIODIC(s,t)) { 
	    ERROR("Failed to make non-RT thread runnable (%llu)\n",s->aperiodic.size);
	    //	    ERROR("queue has %llu entries\n", s->aperiodic->size);
	    goto out_bad;
	} else {
	    thread->status = NK_THR_SUSPENDED;
	    thread->sched_state->status = ADMITTED;	    

	    DUMP_APERIODIC(s, "aperiodic after make runnable");
	    goto out_good;
	}
	break;
    case SPORADIC:
    case PERIODIC:
	if (PUT_RT_PENDING(s,t)) {
	    ERROR("Failed to make RT thread pending\n");
	    goto out_bad;
	} else {
	    thread->status = NK_THR_SUSPENDED;
	    thread->sched_state->status = ADMITTED;
	    DUMP_RT_PENDING(s, "pending after make runnable");
	    goto out_good;
	}
	break;
    }

 out_bad:
    if (!have_lock) {
	LOCAL_UNLOCK(s);
    }
    return -1;
 out_good:
    if (!have_lock) { 
	LOCAL_UNLOCK(s);
    }
    return 0;
}

int nk_sched_make_runnable(struct nk_thread *thread, int cpu, int admit)
{
    return _sched_make_runnable(thread,cpu,admit,0);
}


void nk_sched_exit(spinlock_t *lock_to_release)
{
    handle_special_switch(EXITING,0,0,lock_to_release ? (void (*)(void*))spin_unlock : 0 ,(void*)lock_to_release);
    // we should not come back!
    panic("Returned to finished thread!\n");
}


static rt_list* rt_list_init() 
{
    rt_list *list = (rt_list *)MALLOC(sizeof(rt_list));
    if (list) { 
	list->head = NULL;
	list->tail = NULL;
    }
    return list;
}

static void rt_list_deinit(rt_list *l) 
{
    rt_node *cur = l->head;
    rt_node *next = l->head;

    while (cur) { 
	next = cur->next;
	rt_node_deinit(cur);
	cur = next;
    }
    FREE(l);
}


 
// node setup without allocation
static void _rt_node_init(rt_node *node, rt_thread *t)
{
    ASSERT(t);
    node->thread = t;
    node->next = NULL;
    node->prev = NULL;
}

// node setup with allocation
static rt_node* rt_node_init(rt_thread *t) 
{
    rt_node *node = (rt_node *)MALLOC_SPECIFIC(sizeof(rt_node),t->thread->current_cpu);
    if (node) { 
	_rt_node_init(node,t);
    }
    return node;
}

static void rt_node_deinit(rt_node *n)
{
    n->next = SCHEDULER_POISON;
    n->prev = SCHEDULER_POISON;
    n->thread = SCHEDULER_POISON;
    FREE(n);
}

static int rt_list_empty(rt_list *l) 
{
    return (l->head == NULL);
}

// insert with pre-allocated node structure
// no memory allocation
static int _rt_list_enqueue(rt_list *l, rt_thread *t, rt_node *newnode)
{
    if (l == NULL) {
        ERROR("RT_LIST IS UNINITIALIZED.\n");
        return -1;
    }

    _rt_node_init(newnode,t);

    if (l->head == NULL) {
        l->head = newnode;
	l->tail = l->head;
	t->list = l->head;
	return 0;
    }

    rt_node *n = l->tail;

    l->tail = newnode;
    l->tail->prev = n;
    n->next = l->tail;
    t->list = l->tail;

    return 0;
}


static int rt_list_enqueue(rt_list *l, rt_thread *t)
{
    rt_node *n = rt_node_init(t);
    
    if (!n) {
	ERROR("Failed to allocate node for rt list enqueue\n");
	return -1;
    } else {
	return _rt_list_enqueue(l,t,n);
    }
}


static rt_thread* rt_list_dequeue(rt_list *l) 
{
    if (l == NULL) {
        ERROR("RT_LIST IS UNINITIALIZED.\n");
        return NULL;
    }

    if (l->head == NULL) {
        return NULL;
    }

    rt_node *n = l->head;
    rt_thread *t= n->thread;

    l->head = n->next;
    l->head->prev = NULL;
    n->next = NULL;
    n->prev = NULL;
    rt_node_deinit(n);
    t->list = SCHEDULER_POISON;
    return t;
}

static void rt_list_map(rt_list *l, void (func)(rt_thread *t, void *priv), void *priv)  
{
    ASSERT(l);
    rt_node *n = l->head;
    while (n != NULL) {
	//DEBUG("scan: node %p for thread %p\n",n, n->thread);
	//DEBUG("      (%lu)\n", n->thread->thread->tid);
	func(n->thread,priv);
        n = n->next;
    }
}

static rt_thread* rt_list_remove(rt_list *l, rt_node *n) 
{
    rt_node *tmp = n->next;
    rt_thread *f;

    if (n->next != NULL) {
	n->next->prev = n->prev;
    } else {
	l->tail = n->prev;
    }
    if (n->prev != NULL) {
	n->prev->next = tmp;
    } else {
	l->head = tmp;
    }
    n->next = NULL;
    n->prev = NULL;
    f = n->thread;
    rt_node_deinit(n);
    return f;
}


static rt_thread* rt_list_remove_search(rt_list *l, rt_thread *t) 
{
    rt_node *n = l->head;
    while (n != NULL) {
        if (n->thread == t) {
	    return rt_list_remove(l,n);
        }
        n = n->next;
    }
    return NULL;
}




static int        rt_queue_enqueue(rt_queue *queue, rt_thread *thread)
{
    if (queue->size==MAX_QUEUE) {
	return -1;
    } else {
	queue->threads[queue->head] = thread;
	queue->head = (queue->head + 1 ) % MAX_QUEUE;
	queue->size++;
	return 0;
    }
}
	
static rt_thread* rt_queue_dequeue(rt_queue *queue)
{
    if (queue->size==0) { 
	return 0;
    } else {
	rt_thread *r = queue->threads[queue->tail];
	queue->tail = (queue->tail+1) % MAX_QUEUE;
	queue->size--;
	return r;
    }
}

static rt_thread* rt_queue_remove(rt_queue *queue, rt_thread *thread)
{
    if (queue->size==0) { 
	return 0;
    } else {
	uint64_t now, cur, next;

	for (now=0;now<queue->size;now++) { 
	    cur = (queue->tail + now) % MAX_QUEUE;
	    if (queue->threads[cur] == thread) { 
		break;
	    }
	}

	if (now==queue->size) {
	    // not found
	    return 0;
	}

	// copy down
	for (;now<queue->size-1;now++) {
	    cur = (queue->tail + now) % MAX_QUEUE;
	    next = (queue->tail + now + 1) % MAX_QUEUE;
	    queue->threads[cur] = queue->threads[next];
	}
	
	// decrement head
	queue->head = (queue->head + MAX_QUEUE - 1) % MAX_QUEUE;
    
	queue->size--;
	
	return thread;
    }
}

static rt_thread *rt_queue_peek(rt_queue *queue, uint64_t pos)
{
    if (pos>=queue->size) { 
	return 0;
    } else {
	return queue->threads[(queue->tail+pos)%MAX_QUEUE];
    }
}

static int        rt_queue_empty(rt_queue *queue)
{
    return queue->size==0;
}

static void rt_queue_dump(rt_queue *queue, char *pre)
{
    int now;
    int cur;
    DEBUG("======%s==BEGIN=====\n",pre);
    for (now=0;now<queue->size;now++) { 
	cur = (queue->tail + now) % MAX_QUEUE;
	DEBUG("   %llu %s (%llu)\n",queue->threads[cur]->thread->tid,
	      queue->threads[cur]->thread->is_idle ? "*idle*" : 
	      queue->threads[cur]->thread->name[0] ? queue->threads[cur]->thread->name : "(no name)" ,queue->threads[cur]->deadline);
    }
    DEBUG("======%s==END=====\n",pre);
}

#if SANITY_CHECKS
#define parent(i) ({ uint64_t _t = ((i) ? (((i) - 1) >> 1) : 0); if (_t>=MAX_QUEUE) panic("parent too big\n"); _t; })
#define left_child(i) ({ uint64_t _t = (((i) << 1) + 1); if (_t>=MAX_QUEUE) panic("left too big\n"); _t; })
#define right_child(i) ({ uint64_t _t = (((i) << 1) + 2); if (_t>=MAX_QUEUE) panic("right too big\n"); _t; })
#else // no sanity checks
#define parent(i)      ((i) ? (((i) - 1) >> 1) : 0)
#define left_child(i)  (((i) << 1) + 1)
#define right_child(i) (((i) << 1) + 2)
#endif // sanity checks

static void rt_priority_queue_dump(rt_priority_queue *queue, char *pre)
{
    int now;
    DEBUG("======%s==BEGIN=====\n",pre);
    for (now=0;now<queue->size;now++) { 
	DEBUG("   %llu %s (%llu)\n",queue->threads[now]->thread->tid,
	      queue->threads[now]->thread->is_idle ? "*idle*" : 
	      queue->threads[now]->thread->name[0] ? queue->threads[now]->thread->name : "(no name)" ,queue->threads[now]->deadline);
    }
    DEBUG("======%s==END=====\n",pre);
}

static int rt_priority_queue_enqueue(rt_priority_queue *queue, rt_thread *thread)
{
    if (queue->size == MAX_QUEUE)        {
	ERROR("Too many threads for priority queue %s\n", 
	      queue->type==RUNNABLE_QUEUE ? "Runnable" :
	      queue->type==PENDING_QUEUE ? "Pending" :
	      queue->type==APERIODIC_QUEUE ? "Aperiodic Runnable" : "UNKNOWN");
	      
	return -1;
    }
        
    uint64_t pos = queue->size++;
    queue->threads[pos] = thread;

    // update heap
    while (queue->threads[parent(pos)]->deadline > thread->deadline 
	   && pos != parent(pos))  {
	queue->threads[pos] = queue->threads[parent(pos)];
	pos = parent(pos);
    }

    thread->q_type = queue->type;
    queue->threads[pos] = thread;


    return 0;
}


//
// Get highest priority thread from the queue
//
static rt_thread* rt_priority_queue_dequeue(rt_priority_queue *queue)
{
    char *qstr;

    if (queue->type == RUNNABLE_QUEUE) {
	qstr= "Runnable";
    } else if (queue->type == PENDING_QUEUE) { 
	qstr = "Pending";
    } else if (queue->type == APERIODIC_QUEUE) { 
	qstr = "Aperiodic Runnable";
    } else {
	ERROR("Unknown Queue\n");
	return NULL;
    }

    if (queue->size < 1)  {
	ERROR("%s QUEUE EMPTY! CAN'T DEQUEUE!\n", qstr);
	return NULL;
    }
    
    rt_thread *min, *last;
    int now, child;
    
    // Get the entry we are about to remove (min)
    min = queue->threads[0];
    last = queue->threads[--queue->size];
        
    // update the heap
    for (now = 0; left_child(now) < queue->size; now = child) {
	
	child = left_child(now);

	if (child < queue->size && 
	    queue->threads[right_child(now)]->deadline < queue->threads[left_child(now)]->deadline)  {
	    
	    child = right_child(now);
	}
            
	if (last->deadline > queue->threads[child]->deadline) {
	    queue->threads[now] = queue->threads[child];
	} else {
	    break;
	}
    }
        
    queue->threads[now] = last;
        
    return min;

}

static rt_thread* rt_priority_queue_remove(rt_priority_queue *queue, rt_thread *thread)
{
    // this is currently hideous beyond belief

    rt_priority_queue temp = *queue;
    rt_thread *cur;
    int found=0;

    queue->size=0;

    while (!rt_priority_queue_empty(&temp)) {
	cur=rt_priority_queue_dequeue(&temp);
	if (cur && cur!=thread) {
	    if (rt_priority_queue_enqueue(queue,cur)) { 
		ERROR("Failed to re-enqueue in removal process...\n");
	    } 
	} else {
	    found = 1;
	}
    }
    if (!found) { 
	return 0;
    } else {
	return thread;
    }
}

static rt_thread *rt_priority_queue_peek(rt_priority_queue *queue, uint64_t pos)
{
    if (pos>=queue->size) { 
	return 0;
    } else {
	return queue->threads[pos];
    }
}

static int rt_priority_queue_empty(rt_priority_queue *queue)
{
    return queue->size==0;
}

static void rt_thread_dump(rt_thread *thread, char *pre)
{
    
    if (thread->constraints.type == PERIODIC)    {
	DEBUG("%s: Thread %llu \"%s\" %s: PERIODIC (%llu, %llu): START TIME: %llu RUN TIME: %llu EXIT TIME: %llu DEADLINE: %llu CURRENT TIME: %llu\n", pre, thread->thread->tid, thread->thread->name,thread->thread->is_idle ? "*idle*" : "", thread->constraints.periodic.period, thread->constraints.periodic.slice, thread->start_time, thread->run_time, thread->exit_time, thread->deadline, cur_time() );
    } else if (thread->constraints.type == SPORADIC)    {
	DEBUG("%s: Thread %llu \"%s\" %s: SPORADIC (%llu): START TIME: %llu RUN TIME: %llu EXIT TIME: %llu DEADLINE: %llu CURRENT TIME: %llu\n", pre, thread->thread->tid, thread->thread->name, thread->thread->is_idle ? "*idle*" : "",thread->constraints.sporadic.size,thread->start_time, thread->run_time, thread->exit_time, thread->deadline, cur_time() );
    } else {
        DEBUG("%s: Thread %llu \"%s\" %s: APERIODIC (%llu): START TIME: %llu RUN TIME: %llu EXIT TIME: %llu DEADLINE: %llu CURRENT TIME: %llu\n", pre, thread->thread->tid,  thread->thread->name, thread->thread->is_idle ? "*idle*" : "", thread->constraints.aperiodic.priority, thread->start_time, thread->run_time, thread->exit_time, thread->deadline, cur_time() );

    }
	
}


static void set_timer(rt_scheduler *scheduler, rt_thread *thread, uint64_t now)
{
    struct sys_info *sys = per_cpu_get(system);
    struct apic_dev *apic = sys->cpus[my_cpu_id()]->apic;

    uint64_t next_arrival = -1; //big num
    uint64_t next_preempt = -1; //big num

    if (HAVE_RT_PENDING(scheduler)) { 
	// the current deadline is the next arrival
	next_arrival = PEEK_RT_PENDING(scheduler)->deadline;
    }

    if (thread) { 
	uint64_t remaining_time;
	switch (thread->constraints.type) { 
	case APERIODIC:
	    next_preempt = now + scheduler->cfg.aperiodic_quantum;
	    break;
	case SPORADIC:
	    ASSERT(thread->constraints.sporadic.size >= thread->run_time);
	    remaining_time = thread->constraints.sporadic.size - thread->run_time;
	    next_preempt = now + remaining_time;
	    break;
	case PERIODIC:
	    ASSERT(thread->constraints.periodic.slice >= thread->run_time);
	    remaining_time = thread->constraints.periodic.slice - thread->run_time;
	    next_preempt = now + remaining_time;
	    break;
	}
	thread->start_time = now;
    }


    // set timer to the minimum of the next arrival and the timeout
    // of the current thread, adding slack for scheduler overhead

    scheduler->tsc.start_time = now;
    scheduler->tsc.set_time = MIN(next_arrival,next_preempt);
    
  
    // the set time has been computed based on the "now" argument
    // which is the start of the scheduling pass.   We need to set
    // the cycle counter delay based on the set time relative 
    // to the *current time*
    uint32_t ticks = apic_realtime_to_ticks(apic,  
					    scheduler->tsc.set_time - cur_time() + scheduler->slack);

    
    if (cur_time() >= scheduler->tsc.set_time) {
	DEBUG("Time of next clock has already passed (cur_time=%llu, set_time=%llu)\n",
	      cur_time(), scheduler->tsc.set_time);
	ticks = 1;
    }

    if (ticks & 0x80000000) { 
	ERROR("Ticks is unlikely, probably overflow\n");
    }

    //    DEBUG("Setting timer to at most %llu ns (%llu ticks)\n",scheduler->tsc.set_time - now + scheduler->slack,
    //	  apic_realtime_to_ticks(apic, scheduler->tsc.set_time - now + scheduler->slack));

    apic_update_oneshot_timer(apic, 
			      ticks,
			      IF_EARLIER);
			      

}

static inline void set_interrupt_priority(rt_thread *t)
{    
#ifdef NAUT_CONFIG_INTERRUPT_THREAD
    // if we are using the interrupt thread model
    // then if we are the interrupt thread (or an interrupt-enabled idle thread)
    if (t->is_intr) {
	// then allow any interrupt
	write_cr8(0x0);
    } else {
	// block all but scheduling-related interrupts
	write_cr8(0xe);
    }
#else
    // if we are not using the interrupt thread model, then use the 
    // interrupt priority class selected by the thread itself
    write_cr8((uint64_t)t->constraints.interrupt_priority_class);
#endif
}

#if STACK_CHECKS || defined(NAUT_CONFIG_ENABLE_STACK_CHECK)	
#define stack_check(rt,p)						\
{									\
    struct nk_thread *t = rt->thread;					\
    if ((t->rsp < (uint64_t)(t->stack)) || t->rsp >= (uint64_t)(t->stack+t->stack_size)) { \
	ERROR("STACK OVERRUN : thread %p (%u, \"%s\") rsp=%p  stack=[%p, %p)\n", \
	      t, t->tid, t->is_idle ? "*idle*" : !t->name[0] ? "*unnamed*" : t->name, \
	      t->rsp, t->stack, t->stack+t->stack_size);		\
	BACKTRACE(ERROR,4); \
	if (p) {							\
	   panic("This thread (%p, tid=%u) has run off the end (or beginning) of its stack! (start=%p, rsp=%p, start size=%lx)\n", \
	      (void*)t,							\
	      t->tid,							\
	      t->stack,							\
	      (void*)t->rsp,						\
	      t->stack_size);						\
          return 0;							\
	}								\
    }                                                                   \
}
#else
#define stack_check(x,p)
#endif



#ifdef NAUT_CONFIG_TASK_IN_SCHED

#define TASK_SLOP  50    // out of 100, percentage of available time to consume with tasks
#define TASK_MIN   50000 // ns of time needed to even consider running a task
#define TASK_LIMIT 10    // max number of tasks to scan

// This should be invoked ONLY in need_resched after the time
static int pump_sized_tasks(rt_scheduler *scheduler, rt_thread *next)
{
    if (!next || next->constraints.type!=APERIODIC) {
	// never delay a real-time task that we are committed to running
	return 0;
    }

    if (next->is_task ||  next->is_intr || next->thread->is_idle) {
	// task and idle threads will operate their own task handling loops
	// interrupt thread should never be delayed, even if for some reason
	// it's being run as an aperiodic thread
	return 0;
    }

    // we are committing to a "safe" aperiodic, so we can delay it until the next
    // scheduling event with sized tasks
    
    // next time the scheduler will be invoked for any reason, including RT arrivals
    uint64_t next_time = scheduler->tsc.set_time;
    uint64_t current_time;
    uint64_t avail_time;
    uint64_t count=0;
    
    struct nk_task *task;

    while (1) {
	current_time = cur_time();
	if (current_time+TASK_MIN > next_time) {
	    // not enough time left to do anything
	    return 0;
	}

	// consider a fraction of the available time
	avail_time = ((next_time - current_time)*TASK_SLOP)/100;

	// find a sized task, on this cpu, that will fit, but don't search for too long
	// and do not spin on any locks
	task = nk_task_try_consume(my_cpu_id(), avail_time, TASK_LIMIT);

	if (task) {
	    // if we found one, run it
	    void *output = task->func(task->input);
	    nk_task_complete(task,output);
	    count++;
	    // and then continue to see if we can fit in another one
	} else {
	    // we are out of suitable, easy to find tasks
	    //TASK_DEBUG("Handled %lu sized tasks\n",count);
	    return 0;
	}
    }
	 
}

#else
#define pump_sized_tasks(s,n)
#endif


#define DUMP_ENTRY_CONTEXT()				\
    ERROR("ENTRY CONTEXT\n");				\
    ERROR(" have_lock = %d\n",have_lock);               \
    ERROR(" going_to_sleep = %d\n",going_to_sleep);	\
    ERROR(" going_to_exit = %d\n",going_to_exit);	\
    ERROR(" changing = %d\n",changing);			\
    ERROR(" yielding = %d\n",yielding);			\
    ERROR(" idle = %d\n",idle);				\
    ERROR(" timed_out = %d\n",timed_out);		\
    ERROR(" apic_timer = %d\n",apic_timer);		\
    ERROR(" apic_kick = %d\n",apic_kick);					
     
//
// Invoked in interrupt context by the timer interrupt or
// some other interrupt
//
// Invoked in non-interrupt context by handle_special_switch
//
// Returns NULL if there is no need to change the current thread
// Returns pointer to the thread to switch to otherwise
//
// In both cases updates the timer to reflect the thread
// that should be running
//
struct nk_thread *_sched_need_resched(int have_lock, int force_resched)
{
    LOCAL_LOCK_CONF;
    
    NK_GPIO_OUTPUT_MASK(0x4,GPIO_OR);

    // even before we check for preemptability, we 
    // need to handle a world stop, which we always do
    if (stopping) { 
	// if we are the world stopper, we got here via 
	// an interrupt, and we need to resume without
	if (stopping==(my_cpu_id()+1)) { 
	    DEBUG("Stopping self interrupted - resuming\n");
	    NK_GPIO_OUTPUT_MASK(~0x4,GPIO_AND);
	    return 0;
	} else {
	    uint64_t num_cpus = nk_get_num_cpus();
	    DEBUG("World stop signalled\n");
	    // We now wait for everyone else to stop
	    nk_counting_barrier(&stop_barrier);
	    // everyone's stopped... we are now waiting for
	    // the world stopper to restart us all
	    PAUSE_WHILE(stopping);
	    // we've been restarted - we'll now wait for everyone
	    nk_counting_barrier(&stop_barrier);
	    // everyone's now restarted
	    // if we got here due to the world stopper's kick
	    // we should avoid running the scheduler
	    if (!force_resched && !per_cpu_get(system)->cpus[my_cpu_id()]->apic->in_timer_interrupt) {
		DEBUG("Resuming from world stop without scheduling pass\n");
		NK_GPIO_OUTPUT_MASK(~0x4,GPIO_AND);
		return 0;
	    } else {
		// Otherwise, we will continue with running the scheduler
		DEBUG("Resuming from world stop with scheduling pass\n");
	    }
	}
    }

    if (preempt_is_disabled())  {
	if (force_resched) {
	    DEBUG("Forced reschedule with preemption off\n");
	} else {
	    struct apic_dev *a = per_cpu_get(system)->cpus[my_cpu_id()]->apic;
	    DEBUG("Preemption disabled, avoiding rescheduling pass and staying with current thread\n");

	    if (a->in_timer_interrupt || a->in_kick_interrupt) {
		// We are about to lose a timer interrupt
		// and we need to reinject it so that it shows up again
		DEBUG("Reinjecting timer: in_timer=%d, in_kick=%d\n", 
		      a->in_timer_interrupt, a->in_kick_interrupt);
		//BACKTRACE(DEBUG,3);
		apic_update_oneshot_timer(a, 
					  apic_realtime_to_ticks(a, NAUT_CONFIG_INTERRUPT_REINJECTION_DELAY_NS),
					  IF_EARLIER);
		per_cpu_get(system)->cpus[my_cpu_id()]->sched_state->reinject_count++;
	    }
	    // do not context switch
	    NK_GPIO_OUTPUT_MASK(~0x4,GPIO_AND);
	    return 0;
	}
    }

    INST_SCHED_IN();

    uint64_t now = cur_time();

    struct sys_info *sys = per_cpu_get(system);
    rt_scheduler *scheduler = sys->cpus[my_cpu_id()]->sched_state;
    struct apic_dev *apic = sys->cpus[my_cpu_id()]->apic;
    struct nk_thread *c = get_cur_thread();
    rt_thread *rt_c = c->sched_state;

    // optional
    stack_check(rt_c,1);

    if (!have_lock) {
	LOCAL_LOCK(scheduler);
    }

    scheduler->tsc.end_time = now;

    rt_c->run_time += now - rt_c->start_time;

    rt_c->cur_run_time += now - rt_c->start_time;
    
    rt_thread *rt_n;

    int going_to_sleep = rt_c->status==SLEEPING;
    int going_to_exit = rt_c->status==EXITING;
    int changing = rt_c->status==CHANGING;
    int yielding = rt_c->status==YIELDING;
    int idle = rt_c->thread->is_idle;
    int timed_out = scheduler->tsc.set_time < now;  
    int apic_timer = apic->in_timer_interrupt;
    int apic_kick = apic->in_kick_interrupt;

    // "SPECIAL" means the current task is not to be enqueued
#define CUR_IS_SPECIAL (going_to_sleep || going_to_exit || changing)
#define CUR_IS_NOT_SPECIAL (!(CUR_IS_SPECIAL))
#define CUR_SPECIAL_STR (going_to_sleep ? "Sleeping" : going_to_exit ? "Exiting" : changing ? "Changing" : "NotSpecial")
#define CUR_NOT_SPECIAL_STR (yielding ? "Yielding" : "Preempting")


    // We might be switching away from a thread that is 
    // attempting a special case, for example going to sleep

    DEBUG("need_resched (cur=%d, sleep=%d, exit=%d, changing=%d)\n", c->tid, going_to_sleep,going_to_exit, changing);

    rt_c->resched_count++;

    if (!timed_out && !apic_timer && !apic_kick
	&& CUR_IS_NOT_SPECIAL 
	&& !yielding 
	&& !idle) { 
	// we got here either due to some non-timer interrupt or
	// by a direct call on a thread, and the thread is not
	// trying to do anything special, nor has it timed out 
	// hence we will not change anything, but wait for the next invocation
	DEBUG("Out Early:  now=%lx set_time=%lx\n",now,scheduler->tsc.set_time); 
	goto out_good_early;
    }

    // We have either timed out the current task, or we are being
    // kicked, or it is in the middle of a state transation, or it is
    // yielding, or it is the idle task, which we will always
    // re-examine

    DEBUG("need_resched (now=%llu cur=%llu, idle=%d, sleep=%d, exit=%d, changing=%d yielding=%d status=%d rt_status=%d)\n", now, c->tid, idle, going_to_sleep,going_to_exit, changing, yielding, rt_c->thread->status, rt_c->status);
    //DEBUG_DUMP(rt_c,"Current");

    DUMP_RT_PENDING(scheduler,"pending before handling");
    DUMP_RT(scheduler,"runnable before handling");
    DUMP_APERIODIC(scheduler,"aperiodic before handling");

    // First, move any RT threads that have now arrived 
    // from the pending queue to the runnable queue
    while (HAVE_RT_PENDING(scheduler) && 
	   PEEK_RT_PENDING(scheduler)->deadline <= now) {
	rt_thread *rt_a = GET_NEXT_RT_PENDING(scheduler);
	if (!rt_a) {
	    ERROR("Race in arrivals\n");
	    continue;
	}
	rt_a->arrival_count++;
	if (rt_a->constraints.type==PERIODIC) { 
	    // the deadline is updated to be the end of the period, 
	    // relative to this arrival time (not the current time)
	    rt_a->deadline = rt_a->deadline + rt_a->constraints.periodic.period;
	    rt_a->run_time = 0;
	} else { // SPORADIC
	    // the deadline is absolutely the deadline given in the constraints
	    rt_a->deadline = rt_a->constraints.sporadic.deadline;
	}
	DEBUG_DUMP(rt_a,"Arrival");
	// we do not manipulate the threads status or rt status
	// becaase this would have been done before it was placed into
	// the pending queue
	if (PUT_RT(scheduler, rt_a)) { 
	    goto panic_queue;
	}
    }

    DEBUG("Finished handling pending\n");


    // Now consider the currently running thread
    switch (rt_c->constraints.type) {

    case APERIODIC:


	//DEBUG("current is aperiodic\n");
	// If aperiodic, update its dynamic priority
	rt_thread_update_aperiodic(rt_c,scheduler,now);
	
	if (CUR_IS_NOT_SPECIAL) {
	    // current aperiodic thread has run out of time or is yielding
	    // keep it on the aperiodic run queue
	    //DEBUG_DUMP(rt_c,CUR_NOT_SPECIAL_STR);
	    DEBUG("Set current aperiodic thread (%lu) to suspended\n",rt_c->thread->tid);
	    rt_c->thread->status=NK_THR_SUSPENDED;
	    if (PUT_APERIODIC(scheduler, rt_c)) { 
		goto panic_queue;
	    }
	} else {
	    // current aperiodic thread has initiated something special
	    // it doesn't go back to the run queue until awoken
	    DEBUG_DUMP(rt_c,CUR_SPECIAL_STR);
	}
	
	// if there is any runnable realtime task
	if (HAVE_RT(scheduler)) {
	    // if so, we will switch to the one with
	    // the earliest deadline
	    DEBUG("RT threads available\n");
	    rt_n = GET_NEXT_RT(scheduler);
	    if (rt_n != NULL) {
		DEBUG_DUMP(rt_n,"Next (Aperiodic->RT)");
		goto out_good;
	    } else {
		ERROR("RACE CONDITION DETECTED: No RT threads found on switch from aperiodic\n");
		// continue on to aperiodic, since this is salvageable
	    }
	} 

	//DEBUG("No RT threads available\n");
	// if there is no runnable realtime task, choose
	// the highest priority aperiodic thread that is runnable
	rt_n = GET_NEXT_APERIODIC(scheduler);
	if (rt_n == NULL) {
	    goto panic_no_aperiodic;
	}
	//DEBUG("Found aperiodic thread %llu (priority %llu)\n",rt_n->thread->tid,rt_n->constraints.aperiodic.priority);
	DEBUG_DUMP(rt_n,"Next (Aperiodic->Aperiodic)");
	goto out_good;
	break;
            
    case SPORADIC:

	if (changing &&
	    HAVE_RT(scheduler) &&
	    PEEK_RT(scheduler) == rt_c) {
	    // If I have just been converted to an RT thread, it means that
	    // I was put into the RT pending queue.   By this point in 
	    // the resched, I may have been pumped to the RT run queue
	    // and I may be at the head of the queue.   If so, I need to
	    // remove myself and keep running
	    rt_n = GET_NEXT_RT(scheduler);
	    // I also need to mark myself suspended as if I had previously
	    // been descheduled into that queue
	    rt_n->thread->status=NK_THR_SUSPENDED;
	    // our deadline has already been updated by the arrival process
	    DEBUG_DUMP(rt_n,"ChangingFast (Aperiodic->Periodic)");
	    goto out_good;
	}

	// If the sporadic task has run long enough...
	if (rt_c->run_time >= rt_c->constraints.sporadic.size) {
	    // sanity check whether it met deadlines here
	    // it does not matter for scheduling whether the deadline
	    // was met or not as it will not arrive again
	    rt_thread_check_deadlines(rt_c,scheduler,now);

	    // The task's life is not over at this point, however,
	    // as it now becomes an aperiodic (after we handle any
	    // special transitions)

	    if (CUR_IS_NOT_SPECIAL) {
		// we demote it to aperiodic at this point
		rt_c->constraints.type=APERIODIC;
		rt_c->constraints.aperiodic.priority=rt_c->constraints.sporadic.aperiodic_priority;
		rt_c->thread->status=NK_THR_SUSPENDED;
		if (PUT_APERIODIC(scheduler, rt_c)) { 
		    goto panic_queue;
		}
	    } else {
		// It's trying to do something special
		// so it has already been queued appropriately
		DEBUG_DUMP(rt_c,CUR_SPECIAL_STR);
	    }

	    // if we have any runnable RT threads, we 
	    // will pick the one with earliest deadline
	    if (HAVE_RT(scheduler)) {
		rt_n = GET_NEXT_RT(scheduler);
		if (rt_n != NULL) {
		    DEBUG_DUMP(rt_n,"Next (Aperiodic->RT)");
		    goto out_good;
		} else {
		    ERROR("RACE CONDITION DETECTED: No RT threads found on switch from sporadic on timeout\n");
		    // continue on to aperiodic, since this is salvageable
		}
	    }

	    DEBUG("No RT tasks available\n");

	    // If nothing RT to do, then go aperiodic	    
	    rt_n = GET_NEXT_APERIODIC(scheduler);
	    
	    if (rt_n == NULL) {
		goto panic_no_aperiodic;
	    }

	    DEBUG_DUMP(rt_n,"Next (Sporadic->Aperiodic)");

	    goto out_good;

	} else {
	    // the current sporadic thread is not done yet or has gone to sleep/exit/etc
	    DEBUG("Sporadic task not done yet\n");

	    // are there other RT tasks?
	    if (HAVE_RT(scheduler)) {
		// are we special or is there a higher priority task?
		if (CUR_IS_SPECIAL ||
		    (rt_c->deadline > PEEK_RT(scheduler)->deadline)) {
		    // if so, we need to preempt this one
		    rt_n = GET_NEXT_RT(scheduler);
		    if (rt_n != NULL) {
			// put the current one back on the EDF queue
			// if we're not special
			DEBUG("Higher priority RT task found\n");
			if (CUR_IS_NOT_SPECIAL) { 
			    DEBUG("Putting self back on RT run queue\n");
			    rt_c->thread->status=NK_THR_SUSPENDED;
			    if (PUT_RT(scheduler, rt_c)) {
				goto panic_queue;
			    }
			} else {
			    // We are not enqueueing current since
			    // in special mode, it will have been queued
			    // elsewhere already
			}
			DEBUG_DUMP(rt_n,"Next (Sporadic->RT)");
			goto out_good;
		    } else {
			ERROR("RACE CONDITION DETECTED: No RT threads found on switch from sporadic on preemption\n");
			// continue on... we will continue to run the current thread
		    }
		} else {
		    // No high priority RT threads - we remain the RT thread of interest or we are special
		}
	    } else {
		// No other RT threads - We remain the RT thread of interest or we are special
	    }
	}

	// At this point, there is no RT thread better than us.  
	// But if we are special, we need to go away regardless,
	// which means we need to find an aperiodic
	if (CUR_IS_SPECIAL) {
	    // We are not enqueuing ourselves here since 
	    // someone else already has dropped us into the right queue
	    // in which case we need to find an aperiodic
	    rt_n = GET_NEXT_APERIODIC(scheduler);
	    if (rt_n == NULL) {
		goto panic_no_aperiodic;
	    }

	    DEBUG_DUMP(rt_n,"Next (Sporadic->Aperiodic)");
	    goto out_good;
	} else {
	    // We are sticking with the current thread
	    DEBUG("Sticking with current sporadic task\n");
	    rt_n = rt_c;
	    // we are still NK_THR_RUNNING at this point
	    // we mark it "suspended" to meet expectation of 
	    // the outbound code.   This is a common expectation
	    // of aperiodic, periodic, and sporadic tasks
	    rt_n->thread->status = NK_THR_SUSPENDED;
	    goto out_good;
	}

	break;
        
    case PERIODIC:

	if (changing &&
	    HAVE_RT(scheduler) &&
	    PEEK_RT(scheduler) == rt_c) {
	    // If I have just been converted to an RT thread, it means that
	    // I was put into the RT pending queue.   By this point in 
	    // the resched, I may have been pumped to the RT run queue
	    // and I may be at the head of the queue.   If so, I need to
	    // remove myself and keep running
	    rt_n = GET_NEXT_RT(scheduler);
	    // I also need to mark myself suspended as if I had previously
	    // been descheduled into that queue
	    rt_n->thread->status=NK_THR_SUSPENDED;
	    // our deadline has already been updated by the arrival process
	    DEBUG_DUMP(rt_n,"ChangingFast (Aperiodic->Periodic)");
	    goto out_good;
	}
	// Check to see if we are done with this period of the task
	if (rt_c->run_time >= rt_c->constraints.periodic.slice) {
	    DEBUG("Current task complete (slice=%llu, run_time=%llu)\n",
		  rt_c->constraints.periodic.slice, rt_c->run_time);
	    // if we are done with this period, sanity check deadlines
	    if (rt_thread_check_deadlines(rt_c,scheduler,now)) {
		DEBUG("Missed Deadline - immediate re-arrival\n");
		// deadline update here is relative to when this task
		// SHOULD have completed, not relative to the current time
		rt_c->deadline = rt_c->deadline + rt_c->constraints.periodic.period;
		rt_c->run_time = 0;
		rt_c->arrival_count++;
		// and it has immediately arrived again, so stash it
		// into the EDF queue
		if (CUR_IS_NOT_SPECIAL) {
		    // We are being switched away from, so stash ourselves away
		    DEBUG("Missed Deadline, immediately re-enqueuing\n");
		    rt_c->thread->status=NK_THR_SUSPENDED;
		    if (PUT_RT(scheduler, rt_c)) {
			goto panic_queue;
		    }
		} else {
		    // special - we have already been placed into an appropriate queue
		    // so we are no longer runnable or pending
		    DEBUG_DUMP(rt_c,CUR_SPECIAL_STR);
		}
	    } else {
		// we met the deadline, so we need to set it up
		// to arrive again in the future for the next period
		DEBUG("Deadline met\n");
		// Note that the current deadline is in fact the arrival 
		// deadline for the next arrival
		if (CUR_IS_NOT_SPECIAL) {
		    DEBUG("Deadline met - enqueuing to pending\n");
		    rt_c->thread->status=NK_THR_SUSPENDED;
		    if (PUT_RT_PENDING(scheduler, rt_c)) {
			goto panic_queue;
		    }
		} else {
		    // we have already been queued
		    DEBUG_DUMP(rt_c,CUR_SPECIAL_STR);
		}
	    }

	    DEBUG("Looking for RT task\n");
	    // do we have an RT task available to switch to?
	    if (HAVE_RT(scheduler)) {
		// if so, pick the one with the earliest deadline
		rt_n = GET_NEXT_RT(scheduler);
		if (rt_n != NULL) {
		    DEBUG_DUMP(rt_n,"Next (Periodic->RT)");
		    goto out_good;
		} else {
		    ERROR("RACE CONDITION DETECTED: No RT threads found on switch from periodic on timeout\n");
		    // continue on... 
		}
	    }

	    DEBUG("No RT tasks available\n");

	    // if we get here, there are only aperiodics, which will be handled later

	} else {
	    // we have not run out of time, but
	    // we could have gone into a special state or there could be a higher
	    // priority task
	    DEBUG("Periodic task not done yet\n");
	    // is another RT task available?
	    if (HAVE_RT(scheduler)) {
		// if we're going to sleep/exit or it has an earlier deadline, we 
		// will switch to it
		// if we just changed constraints, then we could now be on
		// the run queue since we pumped the pending queue
		if (CUR_IS_SPECIAL ||
		    (rt_c->deadline > PEEK_RT(scheduler)->deadline)) {
		    rt_n = GET_NEXT_RT(scheduler);
		    if (rt_n != NULL) {
			// only make us runnable again if we are being prempted
			// by a higher priority task, not if we are switching
			// due to a special case
			DEBUG("Higher priority RT task found\n");
			if (CUR_IS_NOT_SPECIAL) {
			    DEBUG("Putting self back on RT run queue\n");
			    // our deadline and runtime does not change here
			    rt_c->thread->status=NK_THR_SUSPENDED;
			    if (PUT_RT(scheduler, rt_c)) { 
				goto panic_queue;
			    }
			} else {
			    // current is not enqueued since this is 
			    // a special case, and it as already been queued
			    // appropriately
			}
			DEBUG_DUMP(rt_n,"Next (Periodic->RT)");
			goto out_good;
		    } else {
			ERROR("RACE CONDITION DETECTED: No RT threads found on switch from periodic on preemption\n");
			// continue
                    }
		} else {
		    // not special and still highest priority, handled below
		} 
            } else {
		// no RT threads, possibly special, handled below
	    }
	} 

	// We are special, but there are no RT threads
	// or we are being suspended an there are no RT threads
	if (CUR_IS_SPECIAL || rt_c->thread->status==NK_THR_SUSPENDED) {
	    // in which case we need to find an aperiodic
	    rt_n = GET_NEXT_APERIODIC(scheduler);
	    if (rt_n == NULL) {
		goto panic_no_aperiodic;
	    }
	    DEBUG_DUMP(rt_n,"Next (Periodic->Aperiodic)");
	    goto out_good;
	} else {	    
	    // We are sticking with the current thread
	    DEBUG("Sticking with current periodic task\n");
	    rt_n = rt_c;
	    // we are still NK_THR_RUNNING at this point
	    // we mark it "suspended" to meet expectation of 
	    // the outbound code.   This is a common expectation
	    // of aperiodic, periodic, and sporadic tasks
	    rt_n->thread->status = NK_THR_SUSPENDED;
	    goto out_good;
	}
	
	break;
	
    default:
	ERROR("Unknown current task type %d... just letting it run\n",rt_c->constraints.type);
	goto out_good_early;
	break;
    }    
    
 panic_no_aperiodic:
    // This cannot happen since there must at least be an idle thread available
    ERROR("APERIODIC QUEUE IS EMPTY.\n THE WORLD IS GOVERNED BY MADNESS.\n");
    panic("ATTEMPTING TO RUN A NULL RT_THREAD.\n");
    return 0;
    
 panic_queue:
    ERROR("UNEXPECTED QUEUE OVERFLOW\n");
    panic("UNEXPECTED QUEUE OVERFLOW IN nk_sched_need_resched()\n");
    return 0;


 out_good_early:
    //   DEBUG("Have not timed out yet (set_time=%llu now=%llu caller=%p) - early exit\n",scheduler->tsc.set_time,now,__builtin_return_address(0));
    if (!have_lock) {
	LOCAL_UNLOCK(scheduler);
    }

    // we do not need the scheduler lock to pump tasks since we have interrupts off
    pump_sized_tasks(scheduler,rt_c);

    INST_SCHED_OUT(resched_fast);

    NK_GPIO_OUTPUT_MASK(~0x4,GPIO_AND);

    return 0;

 out_good:
    //DEBUG("out good\n");
    //DEBUG("out good new tid = %llu (%s) \n",rt_n->thread->tid,rt_n->thread->name);}
    
    rt_c->resched_long_count++;

    if (changing) {
	DEBUG("Thread %llu constraint change complete\n",rt_c->thread->tid);
	rt_c->status=ADMITTED;
    }

    if (going_to_exit) { 
	//DEBUG("Thread %llu exit complete\n", rt_c->thread->tid);
	rt_c->exit_time = now;
    }
    
    if (going_to_sleep) {
	//	DEBUG("%llu sleep complete\n", rt_c->thread->tid);
    }

    if (yielding) { 
	//DEBUG("Thread %llu yield complete\n", rt_c->thread->tid);
    }

    scheduler->current = rt_n;

    // set timer according to nature of thread
    set_timer(scheduler, rt_n, now);
    if (rt_n!=rt_c) {
	//if (!rt_n->is_intr) {
	//    INFO("Switching to non-interrupt thread (%lu, %s)\n",rt_n->thread->tid,rt_n->thread->name);
	//}  else {
	//    INFO("Switching to interrupt thread (%lu, %s)\n",rt_n->thread->tid,rt_n->thread->name);
	//}
	if (rt_n->thread->status==NK_THR_RUNNING) { 
	    ERROR("Switching to new thread that is already running (old tid=%llu (%s), new tid=%llu (%s))\n",rt_c->status,rt_c->thread->tid,rt_c->thread->name,rt_n->thread->tid,rt_n->thread->name);
	    DUMP_ENTRY_CONTEXT();
	}
	// We may have switched away from a thread attempting to sleep
	// before we were able to do so, in which case, don't reset its status
	// we need to try again
	if (rt_n->thread->status != NK_THR_WAITING) { 
	    DEBUG("Switching new thread (%lu) from state %u to state %u\n",rt_n->thread->tid, rt_n->thread->status, NK_THR_RUNNING);
	    rt_n->thread->status=NK_THR_RUNNING;
	}
	// The currently running thread has either been marked suspended
	// at this point, or it is trying to exit or sleep
	// The purpose of this test is to catch any potential
	// races we might have between the scheduler running
	// in thread context and in interrupt context
	if (rt_c->thread->status==NK_THR_RUNNING &&
	    !going_to_sleep && !going_to_exit) {
	    ERROR("Switching to new thread, but old thread (which is not sleeping or exiting) is still marked running (rt status is %u, oldtid=%llu (%s), newtid=%llu (%s))\n",rt_c->status,rt_c->thread->tid,rt_c->thread->name,rt_n->thread->tid,rt_n->thread->name);
	    DUMP_ENTRY_CONTEXT();
	}

	DEBUG("Switching from %llu (%s) to %llu (%s) on cpu %llu\n", 
	      rt_c->thread->tid, rt_c->thread->name,
	      rt_n->thread->tid, rt_n->thread->name,
	      my_cpu_id());

	rt_n->switch_in_count++;
	      
	// we are switching threads, start accounting for the new one
	rt_n->cur_run_time=0;

	// instantiate our interrupt priority class
	set_interrupt_priority(rt_n);

	if (!have_lock) {
	    LOCAL_UNLOCK(scheduler);
	}

	stack_check(rt_n,1);

	// we do not need the scheduler lock to pump tasks since we have interrupts off
	pump_sized_tasks(scheduler,rt_n);
	
	INST_SCHED_OUT(resched_slow);
	NK_GPIO_OUTPUT_MASK(~0x4,GPIO_AND);
	return rt_n->thread;
    } else {
	// we are not switching threads
	// the thread may be marked waiting as we may have been preempted in the 
	// middle of going to sleep
	if (rt_c->thread->status!=NK_THR_SUSPENDED && rt_c->thread->status!=NK_THR_WAITING && !yielding) { 
	    ERROR("Staying with current thread, but it is not marked suspended or waiting or yielding (thread status is %u rt thread status is %u tid=%llu (%s))\n",rt_c->thread->status,rt_c->status,rt_c->thread->tid,rt_c->thread->name);
	    DUMP_ENTRY_CONTEXT();
	}
	if (rt_c->thread->status!= NK_THR_WAITING) { 
	    DEBUG("Switching current thread (%lu) from state %u to state %u\n",rt_c->thread->tid,rt_c->thread->status, NK_THR_RUNNING);
	    rt_c->thread->status=NK_THR_RUNNING;
	}

	DEBUG("Staying with current task %llu (%s)\n", rt_c->thread->tid, rt_c->thread->name);

	if (!have_lock) {
	    LOCAL_UNLOCK(scheduler);
	}
	
	// we do not need the scheduler lock to pump tasks since we have interrupts off
	pump_sized_tasks(scheduler,rt_c);

	INST_SCHED_OUT(resched_slow_noswitch);
	NK_GPIO_OUTPUT_MASK(~0x4,GPIO_AND);
	return 0;
    }
}


struct nk_thread *nk_sched_need_resched()
{
    return _sched_need_resched(0,0);
}

int nk_sched_thread_change_constraints(struct nk_sched_constraints *constraints)
{
    LOCAL_LOCK_CONF;
    struct sys_info *sys = per_cpu_get(system);
    rt_scheduler *scheduler = sys->cpus[my_cpu_id()]->sched_state;
    struct nk_thread *t = get_cur_thread();
    rt_thread *r = t->sched_state;
    struct nk_sched_constraints old;


    DEBUG("Changing constraints of %llu \"%s\"\n", t->tid,t->name);

    LOCAL_LOCK(scheduler);

    if (r->constraints.type != APERIODIC && 
	constraints->type != APERIODIC) {

	DEBUG("Transitioning %llu \"%s\" temporarily to aperiodic\n" , t->tid,t->name);

	struct nk_sched_constraints temp = { .type=APERIODIC, 
					     .aperiodic.priority=scheduler->cfg.aperiodic_default_priority };

	old = r->constraints;
	r->constraints = temp;

	// although this is an admission, it's an admission for
	// aperiodic, which is always yes, and fast
	if (_sched_make_runnable(t,t->current_cpu,1,1)) {
	    ERROR("Failed to re-admit %llu \"%s\" as aperiodic\n" , t->tid,t->name);
	    panic("Unable to change thread's constraints to aperiodic!\n");
	    goto out_bad;
	}
	// we are now on the aperiodic run queue
	// so we need to get running again with our new constraints
	handle_special_switch(CHANGING,1,_local_flags,0,0);
	// we've now released the lock, so reacquire
	LOCAL_LOCK(scheduler);
    }


    // now we are aperiodic and the scheduler is locked/interrupts off

    old = r->constraints;
    r->constraints = *constraints;

    // we assume from here that we are aperiodic changing to other

    if (_sched_make_runnable(t,t->current_cpu,1,1)) {
	DEBUG("Failed to re-admit %llu \"%s\" with new constraints\n" , t->tid,t->name);
	// failed to admit task, bring it back up as aperiodic
	// again.   This should just work
	r->constraints = old;
	if (_sched_make_runnable(t,t->current_cpu,1,1)) {
	    // very bad...
	    panic("Failed to recover to aperiodic when changing constraints\n");
	    goto out_bad;
	}
	DEBUG("Readmitted %llu \"%s\" with old constraints\n" , t->tid,t->name);
	// we are now aperioidic
	// since we are again on the run queue
	// we need to kick ourselves off the cp
	handle_special_switch(CHANGING,1,_local_flags,0,0);
	// when we come back, we note that we have failed
	// we also have no lock
	goto out_bad_no_unlock;
    } else {
	// we were admitted and are now on some queue
	// we now need to kick ourselves off the CPU
	DEBUG("Succeeded in admitting %llu \"%s\" with new constraints\n",t->tid,t->name);
	DUMP_RT(scheduler,"runnable before handle special switch");
	DUMP_RT_PENDING(scheduler,"pending before handle special switch");
	DUMP_APERIODIC(scheduler,"aperiodic before handle special switch");

	handle_special_switch(CHANGING,1,_local_flags,0,0);

	// we now have released lock and interrupts are back to prior

	DEBUG("Thread is now state %d / %d\n",t->status, t->sched_state->status);
	DUMP_RT(scheduler,"runnable after handle special switch");
	DUMP_RT_PENDING(scheduler,"pending after handle special switch");
	DUMP_APERIODIC(scheduler,"aperiodic after handle special switch");
	// when we are back, we note success
	goto out_good_no_unlock;
    }

 out_bad:
    LOCAL_UNLOCK(scheduler);
 out_bad_no_unlock:
    return -1;

 out_good_no_unlock:
    return 0;
}

int nk_sched_thread_move(struct nk_thread *t, int new_cpu, int block)
{
    LOCAL_LOCK_CONF;
    struct sys_info *sys = per_cpu_get(system);
    int old_cpu = t->current_cpu;
    rt_thread *rt = t->sched_state;
    rt_scheduler *os = sys->cpus[old_cpu]->sched_state;
    int rc=-1;

    if (t->bound_cpu>=0) { 
	ERROR("Cannot move a bound thread\n");
	return -1;
    }
    
    if (new_cpu<0 || new_cpu>=sys->num_cpus) { 
	ERROR("Impossible migration to %d\n",new_cpu);
	return -1;
    }
    
    if (t == get_cur_thread()) { 
	ERROR("Cannot currently migrate self\n");
	return -1;
    }
    
    if (old_cpu == new_cpu) { 
	return 0;
    }
    
    if (rt->constraints.type!=APERIODIC) { 
	ERROR("Currently only non-RT threads can be migrated\n");
	return -1;
    }
    
 retry:

    // I now need to grab it from the old scheduler, so own it
    LOCAL_LOCK(os);
    
    if (*((volatile int *)&(t->current_cpu)) != old_cpu) {
	// we raced with someone and it has already been moved
	// should never happen
	ERROR("Race to move thread\n");
	rc = -1;
	goto out_fail;
    }
    
    if (t->status!=NK_THR_SUSPENDED || rt->status!=ADMITTED) {
	// it is not in a workable state for migration
	DEBUG("Thread cannot be migrated as it is not suspended\n");
	rc = -1;
	goto out_good_or_retry_if_blocking;
    }
    
    // now, if it's in the old cpu's aperiodic queue, we need to 
    // remove it.  
    if (!REMOVE_APERIODIC(os,rt)) { 
	DEBUG("Thread cannot be migrated as it is not in the aperiodic ready queue\n");
	rc = -1;
	goto out_good_or_retry_if_blocking;
    }

    // switch its cpu... 
    t->current_cpu = new_cpu;
    // we will make it runnable after we drop the lock
    rc = 0;

 out_good_or_retry_if_blocking:
    
    LOCAL_UNLOCK(os);

    if (rc) {
	if (block) { 
	    // we will wait a quantum and then try again
	    DEBUG("Going to sleep before retry\n");
	    nk_sleep(1000000000ULL/NAUT_CONFIG_HZ);
	    goto retry;
	} else {
	    return -1;
	}
    } else {
	// it is already admitted, and is aperiodic
	// so all we need to do is get it on the destination's
	// queue
	DEBUG("Making thread runnable on new CPU\n");
	if (_sched_make_runnable(t,t->current_cpu,0,0)) {
	    ERROR("Failed to make thread runnable on destination - attempting fallback\n");
	    t->current_cpu = old_cpu;
	    if (_sched_make_runnable(t,t->current_cpu,0,0)) { 
		ERROR("Cannot move thread back to original cpu\n");
		// very bad...
		panic("Failed to make migrated task runnable on destination or source\n");
		return -1;
	    } else {
		return -1;
	    }
	} else {
	    return 0;
	}
    }

 out_fail:
    LOCAL_UNLOCK(os);
    return -1;
}


static int select_victim(int new_cpu)
{
    int a,b;
    struct sys_info *sys = per_cpu_get(system);
 
    // power of two random choices selection

    // pick two, return the one with the most threads

    do {
	a = (int)(get_random() % sys->num_cpus);
	b = (int)(get_random() % sys->num_cpus);
    } while (a==new_cpu || b==new_cpu);

    return (sys->cpus[a]->sched_state->aperiodic.size  >
	    sys->cpus[b]->sched_state->aperiodic.size) ? a : b;

}

uint64_t nk_sched_get_runtime(struct nk_thread *t)
{
    return t->sched_state->run_time;
}

int nk_sched_cpu_mug(int old_cpu, uint64_t maxcount, uint64_t *actualcount)
{
    LOCAL_LOCK_CONF;
    struct sys_info *sys = per_cpu_get(system);
    int new_cpu = my_cpu_id();
    rt_scheduler *os;
    rt_scheduler *ns = sys->cpus[new_cpu]->sched_state;
    rt_thread *prosp[maxcount];
    uint64_t count=0;
    uint64_t cur, pos;
    int rc=-1;


    *actualcount = 0;

    if (old_cpu==-1) { 
	old_cpu = select_victim(new_cpu);
    }

    if (old_cpu==new_cpu) {
	ERROR("Cannot steal from self!\n");
	return -1;
    }
	

    os = sys->cpus[old_cpu]->sched_state;
 
    DEBUG("Work stealing: selected victim is %d\n",old_cpu);

    if (old_cpu>=sys->num_cpus) { 
	ERROR("Cannot steal from cpu %d (out of range)\n", old_cpu);
	return -1;
    }

    if (SIZE_APERIODIC(os) <= SIZE_APERIODIC(ns)) { 
	DEBUG("Avoiding theft from insufficiently rich CPU\n");
	return 0;
    }

    // phase one - grab control of the remote scheduler
    // and examine it for prospective threads
    LOCAL_LOCK(os);

    count=0;

    for (cur=0;cur<SIZE_APERIODIC(os);cur++) {
	rt_thread *t = PEEK_APERIODIC(os,cur);
	// do not steal the idle thread, interrupt thread, task thread, or any bound thread
	if (t && !t->thread->is_idle && !t->is_intr && !t->is_task && t->thread->bound_cpu<0 ) { 
	    DEBUG("Found thread %llu %s\n",t->thread->tid,t->thread->name);
	    prosp[count++] = t;
	    if (count>=maxcount) { 
		break;
	    }
	}
    }
    
    LOCAL_UNLOCK(os);

    // phase 2, attempt to move those threads to me
    
    // note that these moves can fail as the remote scheduler
    // is racing with us.  These failures are OK
    
    *actualcount=0;

    for (cur=0;cur<count;cur++) { 
	DEBUG("Attempting to move thread %llu %s from cpu %d to cpu %d\n",
	     prosp[cur]->thread->tid, prosp[cur]->thread->name,old_cpu,new_cpu);
	if (nk_sched_thread_move(prosp[cur]->thread,new_cpu,0)) {
	    DEBUG("Could not steal thread %llu %s\n",prosp[cur]->thread->tid,prosp[cur]->thread->name);
	} else {
	    DEBUG("Stole thread %llu %s\n",prosp[cur]->thread->tid,prosp[cur]->thread->name);
	    (*actualcount)++;
	}
    }
    
    ns->num_thefts += *actualcount;
    
    DEBUG("Thread theft complete\n");

    return 0;

}


void    nk_sched_kick_cpu(int cpu)
{
#ifdef NAUT_CONFIG_KICK_SCHEDULE
    if (cpu != my_cpu_id()) {
        apic_ipi(per_cpu_get(apic),
		 nk_get_nautilus_info()->sys.cpus[cpu]->lapic_id,
		 APIC_NULL_KICK_VEC);
    } else {
	// we do not reschedule here since
	// we do not know if it is safe to do so 
    }
#endif
}

extern void nk_thread_switch(nk_thread_t *new);
extern void nk_thread_switch_exit_helper(nk_thread_t *new, rt_status *statusp, rt_status newval);

// support a special state transition ("what") by doing a pass of the
// local scheduler
//   - have_lock implies we have the local scheduler lock
//     and we have disabled interrupts
//   - if have_lock is true, flags is the interrupt state that will be restored
//   - if release_callback exists, then it will be invoked just prior to
//     thread switch
// handle_special_switch will in all cases before return:
//   - release the local scheduler lock and restore the interrupt flags, either to
//     the state held at entry, or to the have_lock/flags parameters
// On a context switch:
//   - local scheduler lock is released
//   - interrupts are off in the thread we are switch from, and will be restored
//     to the state of the thread we are switching to
static void handle_special_switch(rt_status what, int have_lock, uint8_t flags, void (*release_callback)(void*), void *release_state)
{
    int did_preempt_disable = 0;
    int no_switch=0;

    if (!preempt_is_disabled()) {
	preempt_disable();
	did_preempt_disable = 1;
    }

    if (!have_lock) { 
	// we always want a local critical section here
	flags = irq_disable_save();
    }

    nk_thread_t * c    = get_cur_thread();
    rt_thread   * rt_c = c->sched_state;
    rt_status last_status;
    nk_thread_t * n = NULL;
    struct sys_info *sys = per_cpu_get(system);
    rt_scheduler *s = sys->cpus[my_cpu_id()]->sched_state;


    ASSERT(what==SLEEPING || what==YIELDING || what==EXITING || what==CHANGING);

    DEBUG("%sing %llu \"%s\"\n",
	  what == SLEEPING ? "Sleep" : 
	  what == YIELDING ? "Yield" : 
	  what == EXITING ? "Exit" : 
	  what == CHANGING ? "Chang" : "WTF",
	  c->tid, c->name);


    last_status = c->sched_state->status;

    c->sched_state->status = what;

    // force a rescheduling pass even though preemption is off
    n = _sched_need_resched(have_lock,1);

    // at this point, the scheduler has been updated and so we can
    // invoke the release callback
    if (release_callback) {
	release_callback(release_state);
    }

    if (!n) {
	switch (what) {
	case SLEEPING:
	    ERROR("Attempt to sleep resulted in no context switch\n");
	    break;
	case EXITING:
	    ERROR("Attempt to exit resulted in no context switch\n");
	    break;
	case CHANGING:
	    // may not have resulted in a context switch
	    DEBUG("Constraint change completed - no switch\n");
	    break;
	case YIELDING:
	    DEBUG("Yield complete - no switch\n");
	    break;
	default:
	    ERROR("Huh - unknown request %d in handle_special_switch()\n",what);
	    break;
	}
	no_switch = 1;
	goto out_good;
    }



    DEBUG("Switching to %llu \"%s\"\n",
	  n->tid, n->name);

    if (have_lock) {
	// release the lock if we had it on entry
	// but keep interrupts off since we want to continue
	// to have a local critical section
	spin_unlock(&s->lock);
	have_lock = 0;
    }

    // Now preemption is enabled, but interrupts are 
    // still off, so we will continue to run to completion
    preempt_reset();

    if (what==EXITING) {
	// We need to make the exit transition extremely cleanly as we can
	// race with the reaper - this invokes an assembly snippet that does this
	// correctly, and also avoids any state save costs for this now dead
	// thread.   The helper also needs to reset the preemption and
	// and interrupt nesting level
	nk_thread_switch_exit_helper(n,&c->sched_state->status,REAPABLE);
	ERROR("Returned from an exit context switch!\n");
	panic("Returned from an exit context switch\n");
	return;
    }
    

    // at this point, we have interrupts off
    // whatever we switch to will turn them back on
    // our context will indicate interrupts off
    // when we switch away, we will leave rflags.if=0 on
    // the stack and preemption enabled
    nk_thread_switch(n);
    
    DEBUG("After return from switch (back in %llu \"%s\")\n", c->tid, c->name);

 out_good:
    c->sched_state->status = last_status;
    if (have_lock) {
	spin_unlock(&s->lock);
    }
    if (no_switch && did_preempt_disable) { 
	// we are not doing a context switch, so we need to revert
	// preemption state to what we had on entry
	// if we had done a context switch, the preemption state
	// would have been reset before the switch back to us
	preempt_enable();
    }
    // and now we restore the interrupt state to 
    // what we had on entry
    irq_enable_restore(flags);
}

/*
 * yield
 *
 * schedule some other thread if appropriate
 * a thread yields only if it wants to remain runnable
 *
 */
void nk_sched_yield(spinlock_t *lock_to_release)
{
    handle_special_switch(YIELDING,0,0,lock_to_release ? (void (*)(void *))spin_unlock : 0, (void*)lock_to_release);
}


/*
 * sleep_extended
 *
 * unconditionally schedule some other thread
 * a thread sleeps only if it wants to stop being runnable
 * the thread can pass in callback that will be invoked once
 * the scheduling pass is complete
 */
void nk_sched_sleep_extended(void (*release_callback)(void *), void *release_state)
{
    handle_special_switch(SLEEPING,0,0,release_callback,release_state);
}

// Basic sleep
void nk_sched_sleep(spinlock_t *lock_to_release)
{
    nk_sched_sleep_extended(lock_to_release ? (void (*)(void*))spin_unlock : 0, (void*)lock_to_release);
}



static int rt_thread_check_deadlines(rt_thread *t, rt_scheduler *s, uint64_t now)
{
    if (now > t->deadline) {
        DEBUG("Missed Deadline = %llu\t\t Current Timer = %llu\n", t->deadline, now);
        DEBUG("Difference =  %llu\n", now - t->deadline);
        rt_thread_dump(t,"Missed Deadline");
	t->miss_count++;
	t->miss_time_sum += now - t->deadline;
	t->miss_time_sum2 += (now - t->deadline)*(now - t->deadline);
	return 1;
    }
    return 0;
}

static inline void rt_thread_update_periodic(rt_thread *t, rt_scheduler *scheduler, uint64_t now)
{
    if (t->constraints.type == PERIODIC) {
        t->deadline = now + t->constraints.periodic.period;
        t->run_time = 0;
    }
}

static inline void rt_thread_update_sporadic(rt_thread *t, rt_scheduler *scheduler, uint64_t now)
{
    if (t->constraints.type == SPORADIC) {
        t->deadline = t->constraints.sporadic.deadline;
        t->run_time = 0;
    }
}

//
// We set the dynamic priority of the thread here
// 
// - idle thread has lowest possible priority
// - other threads have their priority set to 
//   their baseline priority + the amount of time
//   they have run since last switched in (limited by quantum) + a 
//   random factor to keep switching among similarly
//   behaving threads
static inline void rt_thread_update_aperiodic(rt_thread *t, rt_scheduler *scheduler, uint64_t now)
{
    if (t->constraints.type == APERIODIC) {
#if NAUT_CONFIG_APERIODIC_ROUND_ROBIN || NAUT_CONFIG_APERIODIC_LOTTERY
	// nothing is done, as there is no notion of priority
	// and the PUT_APERIODIC() will put it at the end of the queue
	// and update probability if needed
	return;
#endif

#if NAUT_CONFIG_APERIODIC_DYNAMIC_QUANTUM 
	if (t->thread->is_idle) { 
	    t->deadline = -1ULL; // lowest priority possible;
	} else {
	    t->deadline = t->constraints.aperiodic.priority 
		+ MIN(t->cur_run_time,scheduler->cfg.aperiodic_quantum);
	    // the priority must be bounded from below by the 
	    // baseline priority and from above by the idle
	    // priority
	    if ((t->deadline < t->constraints.aperiodic.priority)
		|| (t->deadline > (-1ULL - 2048ULL))) {
		// overflowed or could hit the idle priority
		// when we add our random fuz
		t->deadline = -1ULL - 2048ULL;
	    }
	    t->deadline += now & 0xfff; // now cannot overflow nor hit idle priority
	}
	return;
#endif

#if NAUT_CONFIG_APERIODIC_DYNAMIC_LIFETIME
	if (t->thread->is_idle) { 
	    t->deadline = -1ULL; // lowest priority possible;
	} else {
	    t->deadline = t->constraints.aperiodic.priority + t->run_time;
	    // the priority must be bounded from below by the 
	    // baseline priority and from above by the idle
	    // priority
	    if ((t->deadline < t->constraints.aperiodic.priority)
		|| (t->deadline > (-1ULL - 2048ULL))) {
		// overflowed or could hit the idle priority
		// when we add our random fuz
		t->deadline = -1ULL - 2048ULL;
	    }
	    t->deadline += now & 0xfff; // now cannot overflow nor hit idle priority
	}
	return;
#endif

	panic("NO APERIODIC SCHEDULER IS SELECTED (IMPOSSIBLE)\n");
	return;

    }
}


// in nanoseconds
static uint64_t cur_time()
{
    struct sys_info *sys = per_cpu_get(system);
    struct apic_dev *apic = sys->cpus[my_cpu_id()]->apic;
    uint64_t c = rdtsc();
    uint64_t t = apic_cycles_to_realtime(apic, c);
    return t;
}

uint64_t nk_sched_get_realtime() 
{ 
    return cur_time();
}

static void reset_state(rt_thread *thread)
{
    thread->start_time = 0;
    thread->cur_run_time = 0;
    thread->run_time = 0;
    thread->deadline = 0;
    thread->exit_time = 0;
}

static void reset_stats(rt_thread *thread)
{
    if (thread->constraints.type==APERIODIC) {
	thread->arrival_count = 1;
    } else {
	thread->arrival_count = 0;
    }
    
    thread->resched_count=0;
    thread->resched_long_count=0;
    thread->switch_in_count=0;
    thread->miss_count=0;
    thread->miss_time_sum=0;
    thread->miss_time_sum2=0;
}

// RMS schedulability test for a non-harmonic task set
// for a harmonic task set, this is 1.0
static inline uint64_t get_periodic_util_rms_limit(uint64_t count)
{
    // n*(2^{1/n} - 1) = n*2^{1/n} - n
    static uint64_t limit = 693147; // ln 2
    static uint64_t levels[16] = { 1000000,
				   828427,
				   779763,
				   756828,
				   743491,
				   734772,
				   728626,
				   724061,
				   720537,
				   717734,
				   715451,
				   713557,
				   711958,
				   710592,
				   709411,
				   708380,
    };

    if (count>16) { 
	return limit;
    } else {
	return levels[count-1];
    }
}

static inline void get_periodic_util(rt_scheduler *sched, uint64_t *util, uint64_t *count)
{
    rt_priority_queue *pending = &sched->pending;
    rt_priority_queue *runnable = &sched->runnable;
    int i;

    *util=0;
    *count=0;

    for (i = 0; i < runnable->size; i++) {
        rt_thread *thread = runnable->threads[i];
        if (thread->constraints.type == PERIODIC) {
	    (*count)++;
            *util += (thread->constraints.periodic.slice * UTIL_ONE) / thread->constraints.periodic.period;
        }
    }
    
    for (i = 0; i < pending->size; i++) {
        rt_thread *thread = pending->threads[i];
        if (thread->constraints.type == PERIODIC) {
	    (*count)++;
            *util += (thread->constraints.periodic.slice * UTIL_ONE) / thread->constraints.periodic.period;
        }
    }
    
}

static inline void get_sporadic_util(rt_scheduler *sched, uint64_t now, uint64_t *util, uint64_t *count)
{
    rt_priority_queue *pending = &sched->pending;
    rt_priority_queue *runnable = &sched->runnable;
    int i;

    *util=0;
    *count=0;

    for (i = 0; i < runnable->size; i++) {
        rt_thread *thread = runnable->threads[i];
        if (thread->constraints.type == SPORADIC) {
	    (*count)++;
	    // runnable task measured based on its remaining time
	    // and its current deadline (phase is now zero since
	    // has arrived)
            *util += ((thread->constraints.sporadic.size - thread->run_time) * UTIL_ONE) / (thread->constraints.sporadic.deadline - now);
        }
    }
    
    for (i = 0; i < pending->size; i++) {
        rt_thread *thread = pending->threads[i];
        if (thread->constraints.type == SPORADIC) {
	    (*count)++;
	    // runnable task measured based on its total size
	    // and expected deadline relative to now
            *util += (thread->constraints.sporadic.size * UTIL_ONE) / (thread->constraints.sporadic.deadline - now - thread->constraints.sporadic.phase);
        }
    }
    
}



//
// This assumes the local lock is held
//
// This is also a work in progress, particularly the sporadic/periodic
// integration...
//
static int rt_thread_admit(rt_scheduler *scheduler, rt_thread *thread, uint64_t now)
{

    uint64_t util_limit = scheduler->cfg.util_limit;
    uint64_t aper_res = scheduler->cfg.aperiodic_reservation;
    uint64_t spor_res = scheduler->cfg.sporadic_reservation;
    uint64_t per_res = util_limit - aper_res - spor_res;

    DEBUG("Admission: %s tpr=%u util_limit=%llu aper_res=%llu spor_res=%llu per_res=%llu\n",
	  thread->constraints.type==APERIODIC ? "Aperiodic" :
	  thread->constraints.type==PERIODIC ? "Periodic" :
	  thread->constraints.type==SPORADIC ? "Sporadic" : "Unknown",
	  thread->constraints.interrupt_priority_class,
	  util_limit,aper_res,spor_res,per_res);

    if (thread->constraints.interrupt_priority_class > 0xe) {
	DEBUG("Rejecting thread with too high of an interrupt priority class (%u)\n", thread->constraints.interrupt_priority_class);
	return -1;
    }

    
    switch (thread->constraints.type) {
    case APERIODIC:
	// APERIODIC always admitted
	reset_state(thread);
	reset_stats(thread);
	thread->deadline = thread->constraints.aperiodic.priority;
	DEBUG("Admitting APERIODIC thread\n");
	return 0;
	break;
    case PERIODIC: {
	uint64_t this_util = (thread->constraints.periodic.slice*UTIL_ONE)/thread->constraints.periodic.period;
	uint64_t cur_util, cur_count;
	uint64_t rms_limit;
	uint64_t our_limit;

	if (ENFORCE_GRANULARITY && 
	    ((thread->constraints.periodic.period % GRANULARITY) || 
	     (thread->constraints.periodic.slice % GRANULARITY))) {
	    DEBUG("Rejecting thread because period and/or slice do not meet granularity requirement of %lu ns\n", GRANULARITY);
	    return -1;
	}
	
	if (ENFORCE_LOWER_LIMITS &&
	    ((thread->constraints.periodic.period < PERIOD_LOWER_LIMIT) ||
	     (thread->constraints.periodic.slice < SLICE_LOWER_LIMIT))) {
	    DEBUG("Rejecting thread because period and/or slice are below the lower limits of %lu / %lu ns\n", PERIOD_LOWER_LIMIT,SLICE_LOWER_LIMIT);
	    return -1;
	}

	get_periodic_util(scheduler,&cur_util,&cur_count);
	rms_limit = get_periodic_util_rms_limit(cur_count+1);
	our_limit = MIN(rms_limit,per_res);

	DEBUG("Periodic admission:  this_util=%llu cur_util=%llu rms_limit=%llu our_limit=%llu\n",this_util,cur_util,rms_limit,our_limit);



	if (cur_util+this_util < our_limit) { 
	    // admit task
	    reset_state(thread);
	    reset_stats(thread);
	    // the next arrival of this thread will be at this time
	    thread->deadline = now + thread->constraints.periodic.phase;
	    DEBUG("Admitting PERIODIC thread\n");
	    return 0;
	} else {
	    DEBUG("Rejected PERIODIC thread\n");
	    return -1;
	}
    }
	break;

    case SPORADIC: {
	uint64_t this_util;
	uint64_t time_left;
	uint64_t cur_util, cur_count;
	uint64_t our_limit;

	if (ENFORCE_GRANULARITY && 
	    (thread->constraints.sporadic.size % GRANULARITY)) {
	    DEBUG("Rejecting thread because sporadic size does not meet granularity requirement of %lu ns\n", GRANULARITY);
	    return -1;
	}

	if (ENFORCE_LOWER_LIMITS &&
	    (thread->constraints.sporadic.size < SLICE_LOWER_LIMIT)) {
	    DEBUG("Rejecting thread because sporadic size is below the lower slice limit of %lu\n", SLICE_LOWER_LIMIT);
	    return -1;
	}

	if ((now + 
	     thread->constraints.sporadic.phase + 
	     thread->constraints.sporadic.size) >= 
	    thread->constraints.sporadic.deadline) { 
	    // immediate reject
	    DEBUG("Rejected impossible SPORADIC thread\n");
	    return -1;
	}
	
	time_left = (thread->constraints.sporadic.deadline - (now + thread->constraints.periodic.phase));
	this_util = (thread->constraints.sporadic.size*UTIL_ONE)/time_left;

	get_sporadic_util(scheduler,now,&cur_util,&cur_count);
	our_limit = spor_res;

	DEBUG("Sporadic admission:  this_util=%llu cur_util=%llu our_limit=%llu\n",this_util,cur_util,our_limit);

	if ((cur_util+this_util) < our_limit) { 
	    // admit task
	    reset_state(thread);
	    reset_stats(thread);
	    // the next arrival of this thread will be at this time
	    thread->deadline = now + thread->constraints.sporadic.phase;
	    DEBUG("Admitting SPORADIC thread\n");
	    return 0;
	} else {
	    DEBUG("Rejected SPORADIC thread\n");
	    return -1;
	}
    }
	break;
    default:
	ERROR("Attempt to admit unknown kind of thread\n");
	return -1;
	break;
    }
}

int nk_sched_thread_get_constraints(struct nk_thread *t, struct nk_sched_constraints *c)
{
    rt_thread *r = t->sched_state;
    *c = r->constraints;
    return 0;
}

static inline uint64_t get_avg_per(rt_priority_queue *runnable, rt_priority_queue *pending, rt_thread *new_thread)
{
    uint64_t sum_period = 0;
    uint64_t num_periodic = 0;
    int i;
    
    for (i = 0; i < runnable->size; i++)
    {
        rt_thread *thread = runnable->threads[i];
        if (thread->constraints.type == PERIODIC) {
            sum_period += thread->constraints.periodic.period;
            num_periodic++;
        }
    }
    
    for (i = 0; i < pending->size; i++)
    {
        rt_thread *thread = pending->threads[i];
        if (thread->constraints.type == PERIODIC) {
            sum_period += thread->constraints.periodic.period;
            num_periodic++;
        }
    }
    
    if (new_thread->constraints.type == PERIODIC)
    {
        sum_period += new_thread->constraints.periodic.period;
        num_periodic++;
    }
    
    struct sys_info *sys = per_cpu_get(system);
    sum_period += sys->cpus[my_cpu_id()]->sched_state->cfg.aperiodic_quantum;
    num_periodic++;
    return (sum_period / num_periodic);
}

static inline uint64_t get_min_per(rt_priority_queue *runnable, rt_priority_queue *pending, rt_thread *thread)
{
    uint64_t min_period = 0xFFFFFFFFFFFFFFFF;
    int i;
    for (i = 0; i < runnable->size; i++)
    {
        rt_thread *thread = runnable->threads[i];
        if (thread->constraints.type == PERIODIC)
        {
            min_period = MIN(thread->constraints.periodic.period, min_period);
        }
    }
    
    for (i = 0; i < pending->size; i++)
    {
        rt_thread *thread = pending->threads[i];
        if (thread->constraints.type == PERIODIC)
        {
            min_period = MIN(thread->constraints.periodic.period, min_period);
        }
    }
    return min_period;
}

static int task_initial_placement()
{
    struct sys_info * sys = per_cpu_get(system);
    return (int)(get_random() % sys->num_cpus);
}


struct nk_task *nk_task_produce(int cpu, uint64_t size_ns, void *(*f)(void*), void *input, uint64_t flags)
{
    TASK_LOCK_CONF;
    
    int placement_cpu = cpu>=0 ? cpu : task_initial_placement();
    uint64_t start = cur_time();
    
    struct nk_task *t = MALLOC_SPECIFIC(sizeof(struct nk_task),placement_cpu);

    if (!t) {
	TASK_ERROR("Failed to allocate a task\n");
	return 0;
    }

    memset(t,0,sizeof(*t));

    t->stats.size_ns = size_ns;
    t->stats.enqueue_time_ns = start;
    
    t->flags = flags & ~NK_TASK_COMPLETED;
    t->func = f;
    t->input = input;

    INIT_LIST_HEAD(&t->queue_node);

    struct sys_info * sys = per_cpu_get(system);
    task_info *ti = &sys->cpus[placement_cpu]->sched_state->tasks;

    // own the target scheduler's task queue
    TASK_LOCK(ti);
    if (t->stats.size_ns) {
	list_add_tail(&t->queue_node, &ti->sized_queue);
	ti->sized_enqueued++;
    } else {
	list_add_tail(&t->queue_node, &ti->unsized_queue);
	ti->unsized_enqueued++;
    }
    TASK_UNLOCK(ti);

    // kick any waitqueue
    nk_wait_queue_wake_all(ti->waitq);

    return t;
}

static int task_cpu_selection()
{
    struct sys_info * sys = per_cpu_get(system);
    return (int)(get_random() % sys->num_cpus);
}

// dequeue a task, typically used internally
// dequeuing a task does not execute it.
static struct nk_task *_nk_task_consume(int cpu, uint64_t size_ns, uint64_t search_limit, int try)
{
    TASK_LOCK_CONF;
    
    int source_cpu = cpu>=0 ? cpu : task_cpu_selection();

    struct sys_info * sys = per_cpu_get(system);
    task_info *ti = &sys->cpus[source_cpu]->sched_state->tasks;
    struct nk_task *t = 0;
    struct list_head *cur;

    if (try) {
	if (TASK_TRY_LOCK(ti)) {
	    // failed, so just leave
	    return 0;
	}
    } else {
	TASK_LOCK(ti);
    }
    
    if (size_ns) {
	uint64_t count=0;
	// search for a task that is smaller than the given size
	// this could of course be much smarter...
	list_for_each(cur, &ti->sized_queue) {
	    struct nk_task *test = list_entry(cur,struct nk_task, queue_node);
	    if (test->stats.size_ns <= size_ns) {
		t = test;
		list_del_init(cur);
		ti->sized_dequeued++;
		break;
	    }
	    count++;
	    if (count >= search_limit) {
		break;
	    }
	}
    } else {
	// try unsized queue first
	if (!list_empty(&ti->unsized_queue)) {
	    cur = ti->unsized_queue.next;
	    t = list_entry(cur,struct nk_task, queue_node);
	    list_del_init(cur);
	    ti->unsized_dequeued++;
	} else if (!list_empty(&ti->sized_queue)) {
	    cur = ti->sized_queue.next;
	    t = list_entry(cur,struct nk_task, queue_node);
	    list_del_init(cur);
	    ti->sized_dequeued++;
	} else {
	    // we got nuthin
	}
    }

    TASK_UNLOCK(ti);

    if (t) {
	t->stats.dequeue_time_ns = cur_time();
    }
	
    return t;
}    


struct nk_task *nk_task_consume(int cpu, uint64_t size_ns, uint64_t search_limit)
{
    return _nk_task_consume(cpu,size_ns,search_limit,0);
}

struct nk_task *nk_task_try_consume(int cpu, uint64_t size_ns, uint64_t search_limit)
{
    return _nk_task_consume(cpu,size_ns,search_limit,1);
}

// mark a task as completed
// this will delete the task if it's detached
int nk_task_complete(struct nk_task *task, void *output)
{
    task->output = output;
    // setting the flag must occur *after* setting the output
    __sync_fetch_and_or(&task->flags,NK_TASK_COMPLETED);
    task->stats.complete_time_ns = cur_time();
    if (task->flags & NK_TASK_DETACHED) {
	free(task);
    }
    return 0;
}

// wait for a task to complete and optionally get its output
// detached tasks cannnot be waited on
// this will delete the task
static int _nk_task_wait(struct nk_task *task, void **output, struct nk_task_stats *stats, int try)
{
    if (task->flags & NK_TASK_DETACHED) {
	TASK_ERROR("Cannot wait on detached task; also probable race...\n");
	return -1;
    }

    task->stats.wait_start_ns = cur_time();
    
    volatile uint64_t *test = &task->flags;

    if (try) {
	if (!(*test & NK_TASK_COMPLETED)) {
	    // not done yet, return immediately
	    return 1;
	}
    } else {
	// pump tasks here while we are waiting
	// this assures we can make forward progress
	while (!(*test & NK_TASK_COMPLETED)) {
	    // first look to my own queue
	    struct nk_task *t = nk_task_try_consume(my_cpu_id(),0,0);
	    if (!t) {
		// then other queues
		t = nk_task_try_consume(-1,0,0);
	    }
	    if (t) {
		// found task; run it and complete it
		output = t->func(t->input);
		nk_task_complete(t,output);
	    }
	}
    }

    if (output) {
	*output = task->output;
    }

    task->stats.wait_end_ns = cur_time();

    if (stats) {
	*stats = task->stats;
    }

    free(task);

    return 0;
}

int nk_task_wait(struct nk_task *task, void **output, struct nk_task_stats *stats)
{
    return _nk_task_wait(task,output,stats,0);
}

int nk_task_try_wait(struct nk_task *task, void **output, struct nk_task_stats *stats)
{
    return _nk_task_wait(task,output,stats,1);
}



static struct nk_sched_percpu_state *init_local_state(struct nk_sched_config *cfg)
{
    struct nk_sched_percpu_state *state = (struct nk_sched_percpu_state*)MALLOC_SPECIFIC(sizeof(struct nk_sched_percpu_state),my_cpu_id());
    char buf[NK_WAIT_QUEUE_NAME_LEN];
    
    if (!state) {
        ERROR("Could not allocate rt state\n");
	goto fail_free;
    } else {
	
	ZERO(state);

	state->cfg = *cfg;

	state->runnable.type = RUNNABLE_QUEUE;
        state->pending.type = PENDING_QUEUE;
        state->aperiodic.type = APERIODIC_QUEUE;

    }
    
    spinlock_init(&state->lock);

    spinlock_init(&state->tasks.lock);
    INIT_LIST_HEAD(&state->tasks.sized_queue);
    INIT_LIST_HEAD(&state->tasks.unsized_queue);

    snprintf(buf,NK_WAIT_QUEUE_NAME_LEN,"sched%d-task-wait",my_cpu_id());
    state->tasks.waitq = nk_wait_queue_create(buf);
    if (!state->tasks.waitq) {
	ERROR("Could not allocate task state\n");
	goto fail_free;
    }
    
    return state;

 fail_free:
    FREE(state); 

    return 0;
}

#if NAUT_CONFIG_INTERRUPT_THREAD
static void interrupt(void *in, void **out)
{
    if (nk_thread_name(get_cur_thread(),"(intr)")) { 
	ERROR("Failed to name interrupt thread\n");
	return;
    }

    struct nk_sched_constraints c = { .type=PERIODIC,
				      .interrupt_priority_class=0xe, 
				      .periodic.phase=0,
				      .periodic.period=NAUT_CONFIG_INTERRUPT_THREAD_PERIOD_US*1000ULL,
				      .periodic.slice=NAUT_CONFIG_INTERRUPT_THREAD_SLICE_US*1000ULL};

    if (nk_sched_thread_change_constraints(&c)) { 
	ERROR("Unable to set constraints for interrupt thread\n");
	panic("Unable to set constraints for interrupt thread\n");
	return;
    }

    // promote to interrupt thread
    get_cur_thread()->sched_state->is_intr=1;

    while (1) {
	if (!irqs_enabled()) { 
	    panic("Interrupt thread running with interrupts off!");
	}
	DEBUG("Interrupt thread halting\n");
	// we will be woken from this halt at least by the 
	// timer interrupt at the end of our current slice
	__asm__ __volatile__ ("hlt");
	DEBUG("Interrupt thread awoke from halt (interrupt occurred)\n");
    }
}

static int start_interrupt_thread_for_this_cpu()
{
  nk_thread_id_t tid;
  
  // 2 MB stack since we'll be running interrupt handlers on it
  if (nk_thread_start(interrupt, 0, 0, 1, INTERRUPT_THREAD_STACK_SIZE, &tid, my_cpu_id())) {
      ERROR("Failed to start interrupt thread\n");
      return -1;
  }

  DEBUG("Interrupt thread launched on cpu %d as %p\n", my_cpu_id(), tid);

  return 0;

}

#endif


#if NAUT_CONFIG_TASK_THREAD

static int await_task(void *p)
{
    task_info *ti = (task_info *) p;

    return (ti->sized_enqueued > ti->sized_dequeued) || (ti->unsized_enqueued > ti->unsized_dequeued);
}

static void task(void *in, void **out)
{
    if (nk_thread_name(get_cur_thread(),"(task)")) { 
	TASK_ERROR("Failed to name task thread\n");
	return;
    }

    struct nk_sched_constraints c = { .type=APERIODIC,
				      .interrupt_priority_class=0x0, 
				      .aperiodic.priority=NAUT_CONFIG_TASK_THREAD_PRIORITY };
    
    if (nk_sched_thread_change_constraints(&c)) { 
	TASK_ERROR("Unable to set constraints for task thread\n");
	panic("Unable to set constraints for task thread\n");
	return;
    }

    // promote to task thread
    get_cur_thread()->sched_state->is_task=1;

    struct nk_task *t;
    void *output;

    while (1) {
	// first look to my own queue
	t = nk_task_try_consume(my_cpu_id(),0,0);
	if (!t) {
	    // then other queues
	    t = nk_task_try_consume(-1,0,0);
	}
	if (t) {
	    // found task; run it and complete it
	    output = t->func(t->input);
	    nk_task_complete(t,output);
	} else {
	    // no task, let's put ourselves to sleep on our own cpu's task queues
	    struct sys_info * sys = per_cpu_get(system);
	    task_info *ti = &sys->cpus[my_cpu_id()]->sched_state->tasks;
	    nk_wait_queue_sleep_extended(ti->waitq, await_task, ti);
	    // when we wake up, we will try again
	}
    }
}

static int start_task_thread_for_this_cpu()
{
  nk_thread_id_t tid;
  
  if (nk_thread_start(task, 0, 0, 1, TASK_THREAD_STACK_SIZE, &tid, my_cpu_id())) {
      TASK_ERROR("Failed to start task thread\n");
      return -1;
  }

  TASK_DEBUG("Task thread launched on cpu %d as %p\n", my_cpu_id(), tid);

  return 0;

}

#endif



static int shared_init(struct cpu *my_cpu, struct nk_sched_config *cfg)
{
    nk_thread_t * main = NULL;
    void * my_stack = NULL;
    int flags;

    struct nk_sched_constraints default_constraints = 
	{ .type = APERIODIC, 
	  .aperiodic.priority = cfg->aperiodic_default_priority };



    my_cpu->sched_state = init_local_state(cfg);

    if (!my_cpu->sched_state) {
	ERROR("Failed to allocate local state\n");
	return -1;
    }

    flags = irq_disable_save();

    // first we need to add our current thread as the current thread
    main  = MALLOC_SPECIFIC(sizeof(nk_thread_t),my_cpu_id());
    if (!main) {
        ERROR_PRINT("Could not allocate main thread\n");
	goto fail_free;
    }

    ZERO(main);


    // need to be sure this is aligned, hence direct use of malloc here
    my_stack = malloc_specific(IDLE_THREAD_STACK_SIZE,my_cpu_id());

    if (!my_stack) {
        ERROR("Couldn't allocate stack\n");
        goto fail_free;
    }
    memset(my_stack, 0, IDLE_THREAD_STACK_SIZE);

    main->stack_size = IDLE_THREAD_STACK_SIZE;

    if (_nk_thread_init(main, my_stack, 1, my_cpu->id, my_cpu->id, NULL)) {
	ERROR("Failed to init thread\n");
	goto fail_free;
    }

    // Since this is an already running "thread", it already has a stack.
    // When we return to either init() or to smp_ap_entry(), a stack
    // switch will be performed to the stack we just allocated.  
    // It is possible that init() or smp_ap_entry()
    // will end up popping stuff off of what the compiler thinks is the 
    // *same* stack throughout.  The result is tha we could underrun the new stack
    // and corrupt adjacent memory at a higher address.
    // We guard against this by padding the stack pointer 
    main->rsp -= 1024;

    main->bound_cpu = my_cpu_id(); // idle threads cannot move
    main->current_cpu = main->bound_cpu;
    main->status = NK_THR_RUNNING;
    main->sched_state->status = ADMITTED;

    // this will become an idle thread, so we will make it aperiodic with defaults
    main->sched_state->constraints = default_constraints;
    // let it continue to be high periority for now
    // when it is first de-scheduled, it will be reset to the idle priority
    main->sched_state->deadline = default_constraints.aperiodic.priority;

    reset_stats(main->sched_state);

    put_cur_thread(main);

    my_cpu->sched_state->current = main->sched_state;

    nk_sched_thread_post_create(main);

#ifdef NAUT_CONFIG_INTERRUPT_THREAD_ALLOW_IDLE
    // make the idle thread an interrupt thread as well
    main->sched_state->is_intr=1;
#endif

    // reset local cycle count - this will be synchronized later
    msr_write(IA32_TIME_STAMP_COUNTER,0);

    irq_enable_restore(flags);

    // we will continue to launch main (idle) within
    // nk_sched_start because we want to have coordinated time
    // before doing so, so we need to wait all the CPUs have
    // done the shared init

    return 0;

 fail_free:
    // Note I will leak any internal thread stuff here
    if (my_stack) { 
	free(my_stack);
    }
    FREE(main);

    sti();

    return -1;
}

static int init_global_state()
{
    ZERO(&global_sched_state);
    global_sched_state.thread_list = rt_list_init();
    if (!global_sched_state.thread_list) { 
	ERROR("Cannot allocate global thread list\n");
	return -1;
    }

    spinlock_init(&global_sched_state.lock);

    nk_counting_barrier_init(&stop_barrier,nk_get_num_cpus());

    return 0;

}
								  


/*
 * nk_sched_init_ap
 *
 * scheduler init routine for APs once they
 * have booted up
 *
 */
int
nk_sched_init_ap (struct nk_sched_config *cfg)
{
    cpu_id_t id = my_cpu_id();
    struct cpu * my_cpu = get_cpu();

    DEBUG("Initializing scheduler on AP %u (%p)\n",id,my_cpu);

    if (shared_init(my_cpu,cfg)) { 
	ERROR("Could not intialize scheduler\n");
	return -1;
    }

    return 0;

}


#if NAUT_CONFIG_AUTO_REAP
static void reaper(void *in, void **out)
{
    if (nk_thread_name(get_cur_thread(),"(reaper)")) { 
	ERROR("Failed to name reaper\n");
	return;
    }

    struct nk_sched_constraints c = { .type=APERIODIC,
				      .aperiodic.priority=-1 }; // lowest priority

    if (nk_sched_thread_change_constraints(&c)) { 
	ERROR("Unable to set constraints for reaper thread\n");
	return;
    }
    
    while (1) {
	DEBUG("Reaper sleeping\n");
	nk_sleep(NAUT_CONFIG_AUTO_REAP_PERIOD_MS*1000000ULL);	
	DEBUG("Reaping threads\n");
	nk_sched_reap(1); // unconditional reap
    }
}

static int start_reaper()
{
  nk_thread_id_t tid;

  if (nk_thread_start(reaper, 0, 0, 1, REAPER_THREAD_STACK_SIZE, &tid, 0)) {
      ERROR("Failed to start reaper thread\n");
      return -1;
  }

  DEBUG("Reaper launched on cpu 0 as %p\n",tid);
  return 0;

}

#endif


void nk_sched_start()
{
    uint64_t num_cpus = nk_get_num_cpus();
    struct cpu *my_cpu = get_cpu();
    struct sys_info *sys = per_cpu_get(system);
    struct apic_dev *apic = sys->cpus[my_cpu_id()]->apic;
    uint64_t cur_cycles;

    DEBUG("Scheduler startup - %s\n", my_cpu->is_bsp ? "bsp" : "ap");

    // barrier for all the schedulers
    __sync_fetch_and_add(&sync_count,1);
    while (sync_count < num_cpus) {
	// spin
    }
    cur_cycles = rdtsc();
    if (my_cpu->is_bsp) { 
	// everyone has started their local tsc at 0
	// we will now use the BSP tsc, which has advanced
	// the furthest, pick a point in the future to advance
	// every one to. The + 1 here offsets the -1 init value
	tsc_start = cur_cycles * 16ULL + 1ULL;
    }  else {
	while (tsc_start==-1ULL) { 
	    // spin;
	}
    }

    msr_write(IA32_TIME_STAMP_COUNTER,tsc_start);

    cur_cycles = rdtsc();

    my_cpu->sched_state->tsc.sync_time_cycles = cur_cycles;

    my_cpu->sched_state->tsc.sync_time = apic_cycles_to_realtime(apic,cur_cycles);

    DEBUG("Time restarted at %lu cycles (currently %lu cycles / %lu ns)\n", tsc_start, cur_cycles, my_cpu->sched_state->tsc.sync_time);

    // with the schedulers now synchronized and running, we launch the 
    // ancilary threads if needed
    // note that we are still running with interrupts off
    // so we will not switch here unless we yield, which we
    // must not do in creating any of the ancilary threads

#ifdef NAUT_CONFIG_AUTO_REAP
    if (my_cpu->is_bsp) { 
	DEBUG("Starting reaper thread\n");
	if (start_reaper()) { 
	    ERROR("Cannot start reaper thread\n");
	    panic("Cannot start reaper thread\n");
	    return;
	}
    }
#endif

#ifdef NAUT_CONFIG_INTERRUPT_THREAD
    DEBUG("Starting interrupt thread for CPU %d\n",my_cpu->id);
    if (start_interrupt_thread_for_this_cpu()) {
	ERROR("Cannot start interrupt thread for CPU!\n");
	panic("Cannot start interrupt thread for CPU!\n");
	return;
    }
#endif	

#ifdef NAUT_CONFIG_TASK_THREAD
    DEBUG("Starting task thread for CPU %d\n",my_cpu->id);
    if (start_task_thread_for_this_cpu()) {
	ERROR("Cannot start task thread for CPU!\n");
	panic("Cannot start task thread for CPU!\n");
	return;
    }
#endif	

    // this is the thread set up by the nk_sched_init/nk_sched_init_ap
    // it's the boot thread and will become the idle thread
    // We are now ready to make it the thread that the scheduler
    // begins in
    struct nk_thread *main = get_cur_thread();

    uint64_t now = cur_time();

    // it just started
    main->sched_state->start_time = now;

    my_cpu->sched_state->tsc.start_time = now;
    my_cpu->sched_state->tsc.set_time = now + my_cpu->sched_state->cfg.aperiodic_quantum;
    
    set_timer(my_cpu->sched_state, main->sched_state, now);

    DEBUG("Startup done main tid=%lu\n",main->tid);

    if (my_cpu->is_bsp) { 
      scheduler_ready = 1;
    }

}
    

static void timing_test(uint64_t N, uint64_t M, int print);

/* 
 * nk_sched_init
 *
 * entry point into the scheduler at bootup on the boot core
 *
 */
int
nk_sched_init(struct nk_sched_config *cfg) 
{
    struct cpu * my_cpu = nk_get_nautilus_info()->sys.cpus[nk_get_nautilus_info()->sys.bsp_id];

    INFO("Initializing scheduler on BSP\n");

    //timing_test(1000000,1000000,1);
    //INFO("Hanging\n");
    //while (1) { asm("hlt"); }

    if (init_global_state()) { 
	ERROR("Could not initialize global scheduler state\n");
	return -1;
    }

    if (shared_init(my_cpu,cfg)) { 
	ERROR("Could not intialize scheduler\n");
	return -1;
    }

    return 0;
}




extern void nk_simple_timing_loop(uint64_t);

static void timing_test(uint64_t N, uint64_t M, int print)
{
  uint64_t max, min, sum, sum2;
  uint64_t i;
  uint64_t start, dur;
  uint64_t begin, totaldur;

  sum=sum2=0;
  max=0; 
  min=-1;

  if (print) {
    INFO("Beginning timing test (%lu calls to loop of %lu iterations)\n",M,N);
  }

  begin = rdtsc();
  for (i=0;i<M;i++) { 
    //    INFO("Loop %lu\n", i);
    start = rdtsc();
    nk_simple_timing_loop(N);
    dur = rdtsc() - start;
    if (dur>max) { max=dur; }
    if (dur<min) { min=dur; }
    sum += dur;
    sum2 += dur*dur;
  }
  totaldur = rdtsc()-begin;

  if (print) {
    INFO("Timing test done - stats follow\n");
    INFO("%lu executions of a loop with %lu iterations\n", M,N);
    INFO("Measured duration: %lu cycles\n",sum);
    INFO("Total duration: %lu cycles\n",totaldur);
    INFO("Overhead: %lu cycles (%lu cycles/call)\n",totaldur-sum,(totaldur-sum)/M);
    INFO("min  = %lu cycles\n",min);
    INFO("max  = %lu cycles\n",max);
    INFO("avg  = %lu cycles\n",sum/M);
    INFO("var  = %lu cycles\n",((sum2)-(sum*sum)/M)/M);
    INFO("sum  = %lu cycles\n",sum);
    INFO("sum2 = %lu cycles\n",sum2);
  }

}


static int
handle_cores (char * buf, void * priv)
{
    int cpu;

    if (sscanf(buf, "cores %d", &cpu) != 1) {
      cpu = -1; 
    }

    nk_sched_dump_cores(cpu);

    return 0;
}


static struct shell_cmd_impl cores_impl = {
    .cmd      = "cores",
    .help_str = "cores [n]",
    .handler  = handle_cores,
};
nk_register_shell_cmd(cores_impl);


static int
handle_threads (char * buf, void * priv)
{
    int cpu;

    if (sscanf(buf, "threads %d", &cpu) != 1) {
        cpu = -1; 
    }

    nk_sched_dump_threads(cpu);

    return 0;
}

static struct shell_cmd_impl threads_impl = {
    .cmd      = "threads",
    .help_str = "threads [n]",
    .handler  = handle_threads,
};
nk_register_shell_cmd(threads_impl);

static int
handle_reap (char * buf, void * priv)
{
    nk_sched_reap(1); // unconditional reap
    return 0;
}


static struct shell_cmd_impl reap_impl = {
    .cmd      = "reap",
    .help_str = "reap",
    .handler  = handle_reap,
};
nk_register_shell_cmd(reap_impl);


static int
handle_time (char * buf, void * priv)
{
    int cpu;

    if (sscanf(buf, "time %d", &cpu) != 1) {
        cpu = -1; 
    }

    nk_sched_dump_time(cpu);

    return 0;
}


static struct shell_cmd_impl time_impl = {
    .cmd      = "time",
    .help_str = "time [n]",
    .handler  = handle_time,
};
nk_register_shell_cmd(time_impl);

struct burner_args {
    struct nk_virtual_console *vc;
    char     name[SHELL_MAX_CMD];
    uint64_t size_ns; 
    struct nk_sched_constraints constraints;
} ;


// enable this to flip a GPIO periodically within
// the main loop of test thread
#define GPIO_OUTPUT 0

#if GPIO_OUPUT
#define GET_OUT() inb(0xe010)
#define SET_OUT(x) outb(x,0xe010)
#else
#define GET_OUT() 
#define SET_OUT(x) 
#endif

#define SWITCH() SET_OUT(~GET_OUT())
#define LOOP() {SWITCH(); udelay(1000); }


static void 
burner (void * in, void ** out)
{
    uint64_t start, end, dur;
    struct burner_args *a = (struct burner_args *)in;

    nk_thread_name(get_cur_thread(),a->name);

    if (nk_bind_vc(get_cur_thread(), a->vc)) { 
        ERROR_PRINT("Cannot bind virtual console for burner %s\n",a->name);
        return;
    }

    nk_vc_printf("%s (tid %llu) attempting to promote itself\n", a->name, get_cur_thread()->tid);
#if 1
    if (nk_sched_thread_change_constraints(&a->constraints)) { 
        nk_vc_printf("%s (tid %llu) rejected - exiting\n", a->name, get_cur_thread()->tid);
        return;
    }
#endif

    nk_vc_printf("%s (tid %llu) promotion complete - spinning for %lu ns\n", a->name, get_cur_thread()->tid, a->size_ns);

    while (1) {
        NK_GPIO_OUTPUT_MASK(0x1, GPIO_XOR);
        start = nk_sched_get_realtime();
        //LOOP();
        udelay(10);
        end = nk_sched_get_realtime();
        dur = end - start;
        //	nk_vc_printf("%s (tid %llu) start=%llu, end=%llu left=%llu\n",a->name,get_cur_thread()->tid, start, end,a->size_ns);
        if (dur >= a->size_ns) { 
            nk_vc_printf("%s (tid %llu) done - exiting\n",a->name,get_cur_thread()->tid);
            free(in);
            return;
        } else {
            a->size_ns -= dur;
        }
    }
}


static int 
launch_aperiodic_burner (char * name, 
                         uint64_t size_ns, 
                         uint32_t tpr, 
                         uint64_t priority)
{
    nk_thread_id_t tid;
    struct burner_args *a;

    a = malloc(sizeof(struct burner_args));

    if (!a) { 
        return -1;
    }
    
    strncpy(a->name,name,SHELL_MAX_CMD); a->name[SHELL_MAX_CMD-1]=0;

    a->vc                                   = get_cur_thread()->vc;
    a->size_ns                              = size_ns;
    a->constraints.type                     = APERIODIC;
    a->constraints.interrupt_priority_class = (uint8_t) tpr;
    a->constraints.aperiodic.priority       = priority;

    if (nk_thread_start(burner, (void*)a , NULL, 1, PAGE_SIZE_4KB, &tid, 1)) { 
        free(a);
        return -1;
    } else {
        return 0;
    }
}

static int 
launch_sporadic_burner (char * name, 
                        uint64_t size_ns, 
                        uint32_t tpr, 
                        uint64_t phase, 
                        uint64_t size, 
                        uint64_t deadline, 
                        uint64_t aperiodic_priority)
{
    nk_thread_id_t tid;
    struct burner_args *a;

    a = malloc(sizeof(struct burner_args));
    if (!a) { 
        return -1;
    }
    
    strncpy(a->name,name,SHELL_MAX_CMD); a->name[SHELL_MAX_CMD-1]=0;

    a->vc                                      = get_cur_thread()->vc;
    a->size_ns                                 = size_ns;
    a->constraints.type                        = SPORADIC;
    a->constraints.interrupt_priority_class    = (uint8_t) tpr;
    a->constraints.sporadic.phase              = phase;
    a->constraints.sporadic.size               = size;
    a->constraints.sporadic.deadline           = deadline;
    a->constraints.sporadic.aperiodic_priority = aperiodic_priority;

    if (nk_thread_start(burner, (void*)a , NULL, 1, PAGE_SIZE_4KB, &tid, 1)) {
        free(a);
        return -1;
    } else {
        return 0;
    }
}

static int 
launch_periodic_burner (char * name, 
                        uint64_t size_ns, 
                        uint32_t tpr, 
                        uint64_t phase, 
                        uint64_t period, 
                        uint64_t slice)
{
    nk_thread_id_t tid;
    struct burner_args *a;

    a = malloc(sizeof(struct burner_args));
    if (!a) { 
        return -1;
    }
    
    strncpy(a->name,name,SHELL_MAX_CMD); a->name[SHELL_MAX_CMD-1]=0;

    a->vc                                   = get_cur_thread()->vc;
    a->size_ns                              = size_ns;
    a->constraints.type                     = PERIODIC;
    a->constraints.interrupt_priority_class = (uint8_t) tpr;
    a->constraints.periodic.phase           = phase;
    a->constraints.periodic.period          = period;
    a->constraints.periodic.slice           = slice;

    if (nk_thread_start(burner, (void*)a , NULL, 1, PAGE_SIZE_4KB, &tid, 1)) {
        free(a);
        return -1;
    } else {
        return 0;
    }
}

static int
handle_burn (char * buf, void * priv)
{
    uint64_t size_ns;
    uint32_t tpr;
    uint64_t priority, phase;
    uint64_t period, slice;
    uint64_t size, deadline;
    char name[SHELL_MAX_CMD];

    if (sscanf(buf, "burn a %s %llu %u %llu", name, &size_ns, &tpr, &priority) == 4) { 
        nk_vc_printf("Starting aperiodic burner %s with tpr %u, size %llu ms.and priority %llu\n", name, tpr, size_ns, priority);
        size_ns *= 1000000;
        launch_aperiodic_burner(name,size_ns,tpr,priority);
        return 0;
    }

    if (sscanf(buf,"burn s %s %llu %u %llu %llu %llu %llu", name, &size_ns, &tpr, &phase, &size, &deadline, &priority) == 7) { 

        nk_vc_printf("Starting sporadic burner %s with size %llu ms tpr %u phase %llu from now size %llu ms deadline %llu ms from now and priority %lu\n",name,size_ns,tpr,phase,size,deadline,priority);
        
        size_ns  *= 1000000;
        phase    *= 1000000;
        size     *= 1000000;
        deadline *= 1000000;

        deadline += nk_sched_get_realtime();

        launch_sporadic_burner(name,size_ns,tpr,phase,size,deadline,priority);

        return 0;
    }

    if (sscanf(buf,"burn p %s %llu %u %llu %llu %llu", name, &size_ns, &tpr, &phase, &period, &slice)==6) { 
        nk_vc_printf("Starting periodic burner %s with size %llu ms tpr %u phase %llu from now period %llu ms slice %llu ms\n",name,size_ns,tpr,phase,period,slice);
        size_ns *= 1000000;
        phase   *= 1000000; 
        period  *= 1000000;
        slice   *= 1000000;

        launch_periodic_burner(name,size_ns,tpr,phase,period,slice);
        return 0;
    }

    if (sscanf(buf, "burn up %s %llu %u %llu %llu %llu", name, &size_ns, &tpr, &phase, &period, &slice)==6) { 
        nk_vc_printf("Starting periodic burner %s with size %llu ms tpr %u phase %llu from now period %llu us slice %llu us\n",name,size_ns,tpr,phase,period,slice);
        size_ns *= 1000000;
        phase   *= 1000; 
        period  *= 1000;
        slice   *= 1000;

        launch_periodic_burner(name,size_ns,tpr,phase,period,slice);
        return 0;
    }

    nk_vc_printf("Unknown burner command (%s)\n", buf);

    return 0;
}

static struct shell_cmd_impl burn_impl = {
    .cmd      = "burn",
    .help_str = "burn a name size_ms tpr priority\n" 
                "  burn s name size_ms tpr phase size deadline priority\n" 
                "  burn p name size_ms tpr phase period slice",
    .handler  = handle_burn,
};
nk_register_shell_cmd(burn_impl);


static int 
test_stop (char * buf, void * priv)
{
    int i;

#define N 16
    
    for (i = 0; i < N; i++) { 
        nk_vc_printf("Stopping world iteration %d\n", i);
        nk_sched_stop_world();
        nk_vc_printf("Executing during stopped world iteration %d\n", i);
        nk_sched_start_world();
    }

    return 0;
}


static struct shell_cmd_impl stop_impl = {
    .cmd      = "stop",
    .help_str = "stop",
    .handler  = test_stop,
};
nk_register_shell_cmd(stop_impl);
