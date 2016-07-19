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

/*

  This is an implementation of a per-core hard real-time scheduler
  based on the model of Liu:

  Threads are one of:

  - aperiodic  (simple priority, not real-time)
  - periodic   (period, slice, first arrival)
  - sporadic   (size, arrival)

  On creation, a thread is aperiodic with medium priority.

 */

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/scheduler.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/cpuid.h>
#include <dev/apic.h>
#include <dev/timer.h>


#define INFO(fmt, args...) printk("RT SCHED: " fmt, ##args)
#define RT_SCHED_PRINT(fmt, args...) printk("RT SCHED: " fmt, ##args)
#define RT_SCHED_ERROR(fmt, args...) printk("RT SCHED ERROR: " fmt, ##args)

#define RT_SCHED_DEBUG(fmt, args...)
#ifdef NAUT_CONFIG_DEBUG_RT_SCHEDULER
#undef RT_SCHED_DEBUG
#define RT_SCHED_DEBUG(fmt, args...) printk("RT SCHED: " fmt, ##args)
#endif

#define parent(i) ((i) ? (((i) - 1) >> 1) : 0)
#define left_child(i) (((i) << 1) + 1)
#define right_child(i) (((i) << 1) + 2)

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) (((x) >(y)) ? (x) : (y))
#endif

#define APERIODIC 0
#define SPORADIC 1
#define PERIODIC 2

#define PERIODIC_UTIL 55000
#define SPORADIC_UTIL 18000
#define APERIODIC_UTIL 9000

#define ARRIVED 0
#define ADMITTED 1
#define TOBE_REMOVED 2
#define REMOVED 3
#define SLEEPING 4
#define DENIED 5

#define RUNNABLE_QUEUE 0
#define PENDING_QUEUE 1
#define APERIODIC_QUEUE 2

#define MAX_QUEUE 256

#define QUANTUM 10000000


#define PAD 8

#define MALLOC(x) ({ void *p  = malloc((x)+2*PAD); if (!p) { panic("Failed to Allocate %d bytes\n",x); } memset(p,0,(x)+2*PAD); p+PAD; })
#define FREE(x) do {free(x-PAD); x=0; } while (0)

typedef struct rt_thread_sim {
    rt_type type;
    queue_type q_type;
    rt_status status;
    rt_constraints *constraints;

    uint64_t start_time; 
    uint64_t run_time;
    uint64_t deadline;
    uint64_t exit_time;

} rt_thread_sim;

typedef struct rt_queue_sim {
    queue_type type;
    uint64_t size, head, tail;
    rt_thread_sim *threads[0];
} rt_queue_sim;

typedef struct rt_simulator {
    rt_queue_sim *runnable;
    rt_queue_sim *pending;
    rt_queue_sim *aperiodic;
} rt_simulator;

static rt_simulator* init_simulator();

static int check_deadlines(rt_thread *t);
static inline void update_periodic(rt_thread *t);
static void set_timer(rt_scheduler *scheduler, rt_thread *thread, uint64_t end_time, uint64_t slack);

static rt_thread_sim* rt_need_resched_logic(rt_simulator *simulator, rt_thread_sim *thread, uint64_t time, int *failed, int *finished_max, rt_thread_sim *max);

static uint64_t set_timer_logic(rt_simulator *simulator, rt_thread_sim *thread, uint64_t time);
static void enqueue_thread_logic(rt_queue_sim *queue, rt_thread_sim *thread);
static rt_thread_sim* dequeue_thread_logic(rt_queue_sim *queue);
static inline void update_exit_logic(rt_thread_sim *t, uint64_t time);
static inline void update_enter_logic(rt_thread_sim *t, uint64_t time);
static int check_deadlines_logic(rt_thread_sim *t, uint64_t time);
static inline void update_periodic_logic(rt_thread_sim *t, uint64_t time);
static void copy_threads_sim(rt_simulator *simulator, rt_scheduler *scheduler, rt_thread *new, rt_thread *this);
static void free_threads_sim(rt_simulator *simulator);

static rt_thread_sim* max_periodic(rt_simulator *simulator);
static rt_thread_sim* min_periodic(rt_simulator *simulator);

static inline uint64_t get_min_per(rt_queue *runnable, rt_queue *queue, rt_thread *thread);
static inline uint64_t get_avg_per(rt_queue *runnable, rt_queue *pending, rt_thread *thread);
static inline uint64_t get_per_util(rt_queue *runnable, rt_queue *pending);
static inline uint64_t get_spor_util(rt_queue *runnable);
static inline uint64_t umin(uint64_t x, uint64_t y);

static rt_list* rt_list_init();
static rt_node* rt_node_init(rt_thread *t);

static void sched_sim(void *scheduler);
static int test_sum(void);

rt_thread* rt_thread_init(int type,
                          rt_constraints *constraints,
                          uint64_t deadline,
                          struct nk_thread *thread
                          )
{
    rt_thread *t = (rt_thread *)MALLOC(sizeof(rt_thread));
    t->type = type;
    t->status = ARRIVED;
    t->constraints = constraints;
    t->start_time = 0;
    t->run_time = 0;
    t->deadline = 0;
    t->parent = NULL;


    t->holding = rt_list_init();
    t->waiting = rt_list_init();
    t->children = rt_list_init();

    if (type == PERIODIC)
    {
        t->deadline = cur_time() + constraints->periodic.period;
    } else if (type == SPORADIC)
    {
        t->deadline = cur_time() + deadline;
    }
    
    thread->rt_thread = t;
    t->thread = thread;
    return t;
}

static rt_list* rt_list_init() {
    rt_list *list = (rt_list *)MALLOC(sizeof(rt_list));
    list->head = NULL;
    list->tail = NULL;
    return list;
}

static rt_node* rt_node_init(rt_thread *t) {
    rt_node *node = (rt_node *)MALLOC(sizeof(rt_node));
    node->thread = t;
    node->next = NULL;
    node->prev = NULL;
    return node;
}

int rt_list_empty(rt_list *l) {
    return (l->head == NULL);
}

void list_enqueue(rt_list *l, rt_thread *t) {
    if (l == NULL) {
        RT_SCHED_ERROR("RT_LIST IS UNINITIALIZED.\n");
        return;
    }

    if (l->head == NULL) {
        l->head = rt_node_init(t);
        l->tail = l->head;
        return;
    }

    rt_node *n = l->tail;
    l->tail = rt_node_init(t);
    l->tail->prev = n;
    n->next = l->tail;
}
rt_thread* list_dequeue(rt_list *l) {
    if (l == NULL) {
        RT_SCHED_ERROR("RT_LIST IS UNINITIALIZED.\n");
        return NULL;
    }

    if (l->head == NULL) {
        return NULL;
    }

    rt_node *n = l->head;
    l->head = n->next;
    l->head->prev = NULL;
    n->next = NULL;
    n->prev = NULL;
    return n->thread;
}

rt_thread* list_remove(rt_list *l, rt_thread *t) {
    rt_node *n = l->head;
    while (n != NULL) {
        if (n->thread == t) {
            rt_node *tmp = n->next;
            if (n->next != NULL) {
                n->next->prev = n->prev;
            }
            if (n->prev != NULL) {
                n->prev->next = tmp;
            }
            n->next = NULL;
            n->prev = NULL;
            n->thread->status = REMOVED;
            return n->thread;
        }
        n = n->next;
    }
    return NULL;
}


// B goes on A's waiting Q
// A goes on B's holding Q

void wait_on(rt_thread *A, rt_thread *B) {
    list_enqueue(A->waiting, B);
    list_enqueue(B->holding, A);
}

// Remove B from A's waiting Q
// Remove A from B's holding Q

void wake_up(rt_thread *A, rt_thread *B) {
    list_remove(A->waiting, B);
    list_remove(B->holding, A);

    if (rt_list_empty(A->waiting)) {
        struct sys_info *sys = per_cpu_get(system);
        rt_scheduler *sched = sys->cpus[my_cpu_id()]->rt_sched;
        if (A->status == TOBE_REMOVED || A->status == REMOVED) return;

        list_remove(sched->sleeping, A);
        if (A->status != TOBO_REMOVED && A->status != REMOVED) {
            A->status = ARRIVED;
            list_enqueue(sched->arrival, A);
        }
    }
}

void wake_up_all(rt_thread *A) {
    rt_thread *woke = list_dequeue(A->holding);

    while (woke != NULL) {
        wake_up(woke, A);
        woke = list_dequeue(A->holding);
    }
}

rt_scheduler* rt_scheduler_init(rt_thread *main_thread)
{
    rt_scheduler* scheduler = (rt_scheduler *)MALLOC(sizeof(rt_scheduler));
    rt_queue *runnable = (rt_queue *)MALLOC(sizeof(rt_queue) + MAX_QUEUE * sizeof(rt_thread *));
    rt_queue *pending = (rt_queue *)MALLOC(sizeof(rt_queue) + MAX_QUEUE * sizeof(rt_thread *));
    rt_queue *aperiodic = (rt_queue *)MALLOC(sizeof(rt_queue) + MAX_QUEUE * sizeof(rt_thread *));

    tsc_info *info = (tsc_info *)MALLOC(sizeof(tsc_info));

	scheduler->main_thread = main_thread;

    if (!scheduler || !runnable || ! pending || !aperiodic || !info) {
        RT_SCHED_ERROR("Could not allocate rt scheduler\n");
        return NULL;
    } else {
#define ZERO(x) memset(x, 0, sizeof(*x))
#define ZERO_QUEUE(x) memset(x, 0, sizeof(rt_queue) + MAX_QUEUE * sizeof(rt_thread *))

		ZERO(scheduler);
		ZERO(info);
		ZERO_QUEUE(runnable);
		ZERO_QUEUE(pending);
		ZERO_QUEUE(aperiodic);
		
        runnable->type = RUNNABLE_QUEUE;
        runnable->size = 0;
        scheduler->runnable = runnable;
        
        pending->type = PENDING_QUEUE;
        pending->size = 0;
        scheduler->pending = pending;
        
        aperiodic->type = APERIODIC_QUEUE;
        aperiodic->size = 0;
        scheduler->aperiodic = aperiodic;
		
        rt_list *arrival = rt_list_init();
        rt_list *exited = rt_list_init();
        rt_list *sleeping = rt_list_init();

        if (!arrival || !exited || !sleeping) {
            RT_SCHED_ERROR("Could not allocate rt scheduler's list.\n");
            return NULL;
        } 

		scheduler->arrival = arrival;
        scheduler->exited = exited;
        scheduler->sleeping = sleeping;
        scheduler->tsc = info;

    }
    return scheduler;
}

static rt_simulator* init_simulator() {
    rt_simulator* simulator = (rt_simulator *)MALLOC(sizeof(rt_simulator));
    rt_queue_sim *runnable = (rt_queue_sim *)MALLOC(sizeof(rt_queue_sim) + MAX_QUEUE * sizeof(rt_thread_sim *));
    rt_queue_sim *pending = (rt_queue_sim *)MALLOC(sizeof(rt_queue_sim) + MAX_QUEUE * sizeof(rt_thread_sim *));
    rt_queue_sim *aperiodic = (rt_queue_sim *)MALLOC(sizeof(rt_queue_sim) + MAX_QUEUE * sizeof(rt_thread_sim *));

    if (!simulator || !runnable || ! pending || !aperiodic) {
        RT_SCHED_ERROR("Could not allocate rt simulator\n");
        return NULL;
    } else {
        runnable->type = RUNNABLE_QUEUE;
        runnable->size = 0;
        simulator->runnable = runnable;
        
        pending->type = PENDING_QUEUE;
        pending->size = 0;
        simulator->pending = pending;
        
        aperiodic->type = APERIODIC_QUEUE;
        aperiodic->size = 0;
        simulator->aperiodic = aperiodic;
    }
    return simulator;
}


void enqueue_thread(rt_queue *queue, rt_thread *thread)
{
    if (queue->type == RUNNABLE_QUEUE)
    {
        if (queue->size == MAX_QUEUE)
        {
            RT_SCHED_ERROR("RUN QUEUE IS FULL!");
            return;
        }
        
        uint64_t pos = queue->size++;
        queue->threads[pos] = thread;
        while (queue->threads[parent(pos)]->deadline > thread->deadline && pos != parent(pos))
        {
            queue->threads[pos] = queue->threads[parent(pos)];
            pos = parent(pos);
        }
        thread->q_type = RUNNABLE_QUEUE;
        queue->threads[pos] = thread;
        
    } else if (queue->type == PENDING_QUEUE)
    {
        if (queue->size == MAX_QUEUE)
        {
            RT_SCHED_ERROR("PENDING QUEUE IS FULL!");
            return;
        }
        
        uint64_t pos = queue->size++;
        queue->threads[pos] = thread;
        while (queue->threads[parent(pos)]->deadline > thread->deadline && pos != parent(pos))
        {
            queue->threads[pos] = queue->threads[parent(pos)];
            pos = parent(pos);
        }
        thread->q_type = PENDING_QUEUE;
        queue->threads[pos] = thread;
        
    } else if (queue->type == APERIODIC_QUEUE)
    {
        if (queue->size == MAX_QUEUE) {
            return;
        }
        
        uint64_t pos = queue->size++;
        queue->threads[pos] = thread;
        while (queue->threads[parent(pos)]->constraints->aperiodic.priority > thread->constraints->aperiodic.priority && pos != parent(pos))
        {
            queue->threads[pos] = queue->threads[parent(pos)];
            pos = parent(pos);
        }
        thread->q_type = APERIODIC_QUEUE;
        queue->threads[pos] = thread;
    } 
}

rt_thread* remove_thread(rt_thread *thread) {
    rt_queue *queue = NULL;
    queue_type type = thread->q_type;
    struct sys_info *sys = per_cpu_get(system);
    rt_scheduler *scheduler = sys->cpus[my_cpu_id()]->rt_sched;

    if (type == RUNNABLE_QUEUE) {
        queue = scheduler->runnable;

        if (queue == NULL) {
            RT_SCHED_ERROR("RUNNABLE QUEUE NOT FOUND\n");
            return NULL;
        }

        if (queue->size < 1) {
            RT_SCHED_ERROR("RUNNABLE QUEUE IS EMPTY. CAN'T REMOVE.\n");
            return NULL;
        }

        rt_thread *target_thread, *last;
        int i = 0, target_index = queue->size, now, child;
        for (i = 0; i < queue->size; i++) {
            if (thread == queue->threads[i]) {
                target_index = i;
                break;
            }
        }

        if (target_index == queue->size) {
            RT_SCHED_ERROR("THREAD NOT FOUND ON QUEUE\n");
            return NULL;
        }
        target_thread = queue->threads[target_index];
        last = queue->threads[--queue->size];

        for (now = target_index; left_child(now) < queue->size; now = child)
        {
            child = left_child(now);
            if (child < queue->size && queue->threads[right_child(now)]->deadline < queue->threads[left_child(now)]->deadline)
            {
                child = right_child(now);
            }
            
            if (last->deadline > queue->threads[child]->deadline)
            {
                queue->threads[now] = queue->threads[child];
            } else {
                break;
            }
        }
        
        queue->threads[now] = last;
        return thread;
    } else if (type == PENDING_QUEUE) {
        queue = scheduler->pending;

        if (queue == NULL) {
            RT_SCHED_ERROR("PENDING QUEUE NOT FOUND\n");
            return NULL;
        }

        if (queue->size < 1) {
            RT_SCHED_ERROR("PENDING QUEUE IS EMPTY. CAN'T REMOVE.\n");
            return NULL;
        }

        rt_thread *target_thread, *last;
        int i = 0, target_index = queue->size, now, child;
        for (i = 0; i < queue->size; i++) {
            if (thread == queue->threads[i]) {
                target_index = i;
                break;
            }
        }

        if (target_index == queue->size) {
            RT_SCHED_ERROR("THREAD NOT FOUND ON QUEUE\n");
            return NULL;
        }
        target_thread = queue->threads[target_index];
        last = queue->threads[--queue->size];

        for (now = target_index; left_child(now) < queue->size; now = child)
        {
            child = left_child(now);
            if (child < queue->size && queue->threads[right_child(now)]->deadline < queue->threads[left_child(now)]->deadline)
            {
                child = right_child(now);
            }
            
            if (last->deadline > queue->threads[child]->deadline)
            {
                queue->threads[now] = queue->threads[child];
            } else {
                break;
            }
        }
        
        queue->threads[now] = last;
        return thread;
    }  else if (type == APERIODIC_QUEUE) {
        queue = scheduler->aperiodic;

        if (queue == NULL) {
            RT_SCHED_ERROR("APERIODIC QUEUE NOT FOUND\n");
            return NULL;
        }

        if (queue->size < 1) {
            RT_SCHED_ERROR("APERIODIC QUEUE IS EMPTY. CAN'T REMOVE.\n");
            return NULL;
        }

        rt_thread *target_thread, *last;
        int i = 0, target_index = queue->size, now, child;
        for (i = 0; i < queue->size; i++) {
            if (thread == queue->threads[i]) {
                target_index = i;
                break;
            }
        }

        if (target_index == queue->size) {
            RT_SCHED_ERROR("THREAD NOT FOUND ON QUEUE\n");
            return NULL;
        }
        target_thread = queue->threads[target_index];
        last = queue->threads[--queue->size];

        for (now = target_index; left_child(now) < queue->size; now = child)
        {
            child = left_child(now);
            if (child < queue->size && queue->threads[right_child(now)]->constraints->aperiodic.priority < queue->threads[left_child(now)]->constraints->aperiodic.priority)
            {
                child = right_child(now);
            }
            
            if (last->constraints->aperiodic.priority > queue->threads[child]->constraints->aperiodic.priority)
            {
                queue->threads[now] = queue->threads[child];
            } else {
                break;
            }
        }
        
        queue->threads[now] = last;
        return thread;
    }
    return NULL;
}

rt_thread* dequeue_thread(rt_queue *queue)
{
    if (queue->type == RUNNABLE_QUEUE)
    {
        if (queue->size < 1)
        {
            RT_SCHED_ERROR("RUNNABLE QUEUE EMPTY! CAN'T DEQUEUE!\n");
            return NULL;
        }
        
        rt_thread *min, *last;
        int now, child;
        
        min = queue->threads[0];
        last = queue->threads[--queue->size];
        
        for (now = 0; left_child(now) < queue->size; now = child)
        {
            child = left_child(now);
            if (child < queue->size && queue->threads[right_child(now)]->deadline < queue->threads[left_child(now)]->deadline)
            {
                child = right_child(now);
            }
            
            if (last->deadline > queue->threads[child]->deadline)
            {
                queue->threads[now] = queue->threads[child];
            } else {
                break;
            }
        }
        
        queue->threads[now] = last;

        if (min->status == TOBE_REMOVED) {
            min->status = REMOVED;
            return dequeue_thread(queue);
        }
        
        return min;
    } else if (queue->type == PENDING_QUEUE)
    {
        if (queue->size < 1)
        {
            RT_SCHED_ERROR("PENDING QUEUE EMPTY! CAN'T DEQUEUE!\n");
            return NULL;
        }
        rt_thread *min, *last;
        int now, child;
        
        min = queue->threads[0];
        last = queue->threads[--queue->size];
        
        for (now = 0; left_child(now) < queue->size; now = child)
        {
            child = left_child(now);
            if (child < queue->size && queue->threads[right_child(now)]->deadline < queue->threads[left_child(now)]->deadline)
            {
                child = right_child(now);
            }
            
            if (last->deadline > queue->threads[child]->deadline)
            {
                queue->threads[now] = queue->threads[child];
            } else {
                break;
            }
        }
        
        queue->threads[now] = last;

        if (min->status == TOBE_REMOVED) {
            min->status = REMOVED;
            return dequeue_thread(queue);
        }
        
        return min;
    } else if (queue->type == APERIODIC_QUEUE)
    {
        if (queue->size < 1)
        {
            RT_SCHED_ERROR("APERIODIC QUEUE EMPTY! CAN'T DEQUEUE!\n");
            return NULL;
        }
        rt_thread *min, *last;
        int now, child;
        
        min = queue->threads[0];
        last = queue->threads[--queue->size];
        
        for (now = 0; left_child(now) < queue->size; now = child)
        {
            child = left_child(now);
            if (child < queue->size && queue->threads[right_child(now)]->constraints->aperiodic.priority < queue->threads[left_child(now)]->constraints->aperiodic.priority)
            {
                child = right_child(now);
            }
            
            if (last->constraints->aperiodic.priority > queue->threads[child]->constraints->aperiodic.priority)
            {
                queue->threads[now] = queue->threads[child];
            } else {
                break;
            }
        }
        
        queue->threads[now] = last;

        if (min->status == TOBE_REMOVED) {
            min->status = REMOVED;
            return dequeue_thread(queue);
        }
        
        return min;
    }
    return NULL;
}

static void enqueue_thread_logic(rt_queue_sim *queue, rt_thread_sim *thread)
{
    if (queue->type == RUNNABLE_QUEUE)
    {
        uint64_t pos = queue->size++;
        queue->threads[pos] = thread;
        while (queue->threads[parent(pos)]->deadline > thread->deadline && pos != parent(pos))
        {
            queue->threads[pos] = queue->threads[parent(pos)];
            pos = parent(pos);
        }
        thread->q_type = RUNNABLE_QUEUE;
        queue->threads[pos] = thread;
        
    } else if (queue->type == PENDING_QUEUE)
    {
        uint64_t pos = queue->size++;
        queue->threads[pos] = thread;
        while (queue->threads[parent(pos)]->deadline > thread->deadline && pos != parent(pos))
        {
            queue->threads[pos] = queue->threads[parent(pos)];
            pos = parent(pos);
        }
        thread->q_type = PENDING_QUEUE;
        queue->threads[pos] = thread;
        
    } else if (queue->type == APERIODIC_QUEUE)
    {
        uint64_t pos = queue->size++;
        queue->threads[pos] = thread;
        while (queue->threads[parent(pos)]->constraints->aperiodic.priority > thread->constraints->aperiodic.priority && pos != parent(pos))
        {
            queue->threads[pos] = queue->threads[parent(pos)];
            pos = parent(pos);
        }
        thread->q_type = APERIODIC_QUEUE;
        queue->threads[pos] = thread;
    } 
}

static rt_thread_sim* dequeue_thread_logic(rt_queue_sim *queue)
{
    if (queue->type == RUNNABLE_QUEUE)
    {
        if (queue->size < 1)
        {
            RT_SCHED_ERROR("RUNNABLE QUEUE EMPTY! CAN'T DEQUEUE!\n");
            return NULL;
        }
        
        rt_thread_sim *min, *last;
        int now, child;
        
        min = queue->threads[0];
        last = queue->threads[--queue->size];
        
        for (now = 0; left_child(now) < queue->size; now = child)
        {
            child = left_child(now);
            if (child < queue->size && queue->threads[right_child(now)]->deadline < queue->threads[left_child(now)]->deadline)
            {
                child = right_child(now);
            }
            
            if (last->deadline > queue->threads[child]->deadline)
            {
                queue->threads[now] = queue->threads[child];
            } else {
                break;
            }
        }
        
        queue->threads[now] = last;
        
        return min;
    } else if (queue->type == PENDING_QUEUE)
    {
        if (queue->size < 1)
        {
            RT_SCHED_ERROR("PENDING QUEUE EMPTY! CAN'T DEQUEUE!\n");
            return NULL;
        }
        rt_thread_sim *min, *last;
        int now, child;
        
        min = queue->threads[0];
        last = queue->threads[--queue->size];
        
        for (now = 0; left_child(now) < queue->size; now = child)
        {
            child = left_child(now);
            if (child < queue->size && queue->threads[right_child(now)]->deadline < queue->threads[left_child(now)]->deadline)
            {
                child = right_child(now);
            }
            
            if (last->deadline > queue->threads[child]->deadline)
            {
                queue->threads[now] = queue->threads[child];
            } else {
                break;
            }
        }
        
        queue->threads[now] = last;
        
        return min;
    } else if (queue->type == APERIODIC_QUEUE)
    {
        if (queue->size < 1)
        {
            RT_SCHED_ERROR("APERIODIC QUEUE EMPTY! CAN'T DEQUEUE!\n");
            return NULL;
        }

        rt_thread_sim *min, *last;
        int now, child;
        
        min = queue->threads[0];
        last = queue->threads[--queue->size];
        
        for (now = 0; left_child(now) < queue->size; now = child)
        {
            child = left_child(now);
            if (child < queue->size && queue->threads[right_child(now)]->constraints->aperiodic.priority < queue->threads[left_child(now)]->constraints->aperiodic.priority)
            {
                child = right_child(now);
            }
            
            if (last->constraints->aperiodic.priority > queue->threads[child]->constraints->aperiodic.priority)
            {
                queue->threads[now] = queue->threads[child];
            } else {
                break;
            }
        }
        
        queue->threads[now] = last;
        
        return min;
    } 
    return NULL;
}

void rt_thread_dump(rt_thread *thread)
{
    
    if (thread->type == PERIODIC)
    {
		printk("START TIME: %llu\t\tRUN TIME: %llu\t\tEXIT TIME: %llu\nDEADLINE: %llu\t\tCURRENT TIME: %llu\n", thread->start_time, thread->run_time, thread->exit_time, thread->deadline, cur_time() );
    } else if (thread->type == SPORADIC)
    {
        RT_SCHED_DEBUG("Work: %llu\t\t", thread->constraints->sporadic.work);
    }
}


static void set_timer(rt_scheduler *scheduler, rt_thread *current_thread, uint64_t end_time, uint64_t slack)
{
    scheduler->tsc->start_time = cur_time();
    end_time = cur_time();
    struct sys_info *sys = per_cpu_get(system);
    struct apic_dev *apic = sys->cpus[my_cpu_id()]->apic;
    if (scheduler->pending->size > 0 && current_thread) {
        rt_thread *thread = scheduler->pending->threads[0];
        uint64_t completion_time = 0;
        if (current_thread->type == PERIODIC)
        {
            apic_oneshot_write(apic, umin(thread->deadline - end_time, (current_thread->constraints->periodic.slice - current_thread->run_time) + slack));
            scheduler->tsc->set_time = umin(thread->deadline - end_time, (current_thread->constraints->periodic.slice - current_thread->run_time));
        } else if (current_thread->type == SPORADIC)
        {
            apic_oneshot_write(apic, umin(thread->deadline - end_time, (current_thread->constraints->sporadic.work - current_thread->run_time) + slack));
            scheduler->tsc->set_time = umin(thread->deadline - end_time, (current_thread->constraints->sporadic.work - current_thread->run_time) + slack);
        } else
        {
            apic_oneshot_write(apic, umin(thread->deadline - end_time, QUANTUM));
            scheduler->tsc->set_time = umin(thread->deadline - end_time, QUANTUM);
        }
    } else if (scheduler->pending->size == 0 && current_thread) {
        if (current_thread->type == PERIODIC)
        {
            apic_oneshot_write(apic, (current_thread->constraints->periodic.slice - current_thread->run_time) + slack);
            scheduler->tsc->set_time = (current_thread->constraints->periodic.slice - current_thread->run_time) + slack;
        } else if (current_thread->type == SPORADIC)
        {
            apic_oneshot_write(apic, (current_thread->constraints->sporadic.work - current_thread->run_time) + slack);
            scheduler->tsc->set_time = (current_thread->constraints->sporadic.work - current_thread->run_time) + slack;
        }
        else {
            apic_oneshot_write(apic, QUANTUM);
            scheduler->tsc->set_time = QUANTUM;
        }
    } else {
        apic_oneshot_write(apic, QUANTUM);
        scheduler->tsc->set_time = QUANTUM;
    }
	scheduler->tsc->end_time = end_time;
}


struct nk_thread *rt_need_resched()
{
    struct sys_info *sys = per_cpu_get(system);
    rt_scheduler *scheduler = sys->cpus[my_cpu_id()]->rt_sched;
    
    struct nk_thread *c = get_cur_thread();
    rt_thread *rt_c = c->rt_thread;
    
    
    uint64_t end_time = scheduler->run_time + cur_time();
    uint64_t slack = 0;
    scheduler->tsc->end_time = cur_time();
    
    rt_thread *rt_n;
    
    while (scheduler->pending->size > 0)
    {
        if (scheduler->pending->threads[0]->deadline < end_time)
        {
            rt_thread *arrived_thread = dequeue_thread(scheduler->pending);
            if (arrived_thread == NULL) {
                continue;
            }
            update_periodic(arrived_thread);
            enqueue_thread(scheduler->runnable, arrived_thread);
            continue;
        } else
        {
            break;
        }
    }
    
    switch (rt_c->type) {
        case APERIODIC:
            rt_c->constraints->aperiodic.priority = rt_c->run_time;
            enqueue_thread(scheduler->aperiodic, rt_c);

            if (scheduler->runnable->size > 0)
            {
                rt_n = dequeue_thread(scheduler->runnable);
                if (rt_n != NULL) {
                    set_timer(scheduler, rt_n, end_time, slack);
                    return rt_n->thread;
                }
            }

            rt_n = dequeue_thread(scheduler->aperiodic);
            if (rt_n == NULL) {
                    RT_SCHED_ERROR("APERIODIC QUEUE IS EMPTY.\n THE WORLD IS GOVERNED BY MADNESS.\n");
                    panic("ATTEMPTING TO RUN A NULL RT_THREAD.\n");
            }
            set_timer(scheduler, rt_n, end_time, slack);
            return rt_n->thread;
            break;
            
        case SPORADIC:
            if (rt_c->run_time >= rt_c->constraints->sporadic.work) {
                check_deadlines(rt_c);

                if (scheduler->runnable->size > 0) {
                    rt_n = dequeue_thread(scheduler->runnable);
                    if (rt_n != NULL) {
                        set_timer(scheduler, rt_n, end_time, slack);
                        return rt_n->thread;
                    }
                }
                rt_n = dequeue_thread(scheduler->aperiodic);
                if (rt_n == NULL) {
                    RT_SCHED_ERROR("APERIODIC QUEUE IS EMPTY.\n THE WORLD IS GOVERNED BY MADNESS.\n");
                    panic("ATTEMPTING TO RUN A NULL RT_THREAD.\n");
                }
                set_timer(scheduler, rt_n, end_time, slack);
                return rt_n->thread;
            } else {
                if (scheduler->runnable->size > 0)
                {
                    if (rt_c->deadline > scheduler->runnable->threads[0]->deadline) {
                        rt_n = dequeue_thread(scheduler->runnable);
                        if (rt_n != NULL) {
                            enqueue_thread(scheduler->runnable, rt_c);
                            set_timer(scheduler, rt_n, end_time, slack);
                            return rt_n->thread;
                        }
                    }
                }
            }
            set_timer(scheduler, rt_c, end_time, slack);
            return rt_c->thread;
            break;
            
        case PERIODIC:
            if (rt_c->run_time >= rt_c->constraints->periodic.slice) {
                if (check_deadlines(rt_c)) {
                    update_periodic(rt_c);
                    enqueue_thread(scheduler->runnable, rt_c);
                } else {
                    enqueue_thread(scheduler->pending, rt_c);
                }

                if (scheduler->runnable->size > 0) {
                    rt_n = dequeue_thread(scheduler->runnable);
                    if (rt_n != NULL) {
                        set_timer(scheduler, rt_n, end_time, slack);
                        return rt_n->thread;
                    }
                }
                rt_n = dequeue_thread(scheduler->aperiodic);
                if (rt_n == NULL) {
                    RT_SCHED_ERROR("APERIODIC QUEUE IS EMPTY.\n THE WORLD IS GOVERNED BY MADNESS.\n");
                    panic("ATTEMPTING TO RUN A NULL RT_THREAD.\n");
                }
                set_timer(scheduler, rt_n, end_time, slack);
                return rt_n->thread;
            } else {
                if (scheduler->runnable->size > 0)
                {
                    if (rt_c->deadline > scheduler->runnable->threads[0]->deadline) {
                        rt_n = dequeue_thread(scheduler->runnable);
                        if (rt_n != NULL) {
                            enqueue_thread(scheduler->runnable, rt_c);
                            set_timer(scheduler, rt_n, end_time, slack);
                            return rt_n->thread;
                        }
                    }
                }
            }
            set_timer(scheduler, rt_c, end_time, slack);
            return rt_c->thread;
            break;
        default:
            set_timer(scheduler, rt_c, end_time, slack);
            return c;
    }    
}



static int check_deadlines(rt_thread *t)
{
	
    if (t->exit_time > t->deadline) {
        RT_SCHED_ERROR("Missed Deadline = %llu\t\t Current Timer = %llu\n", t->deadline, t->exit_time);
        RT_SCHED_ERROR("Difference =  %llu\n", t->exit_time - t->deadline);
        rt_thread_dump(t);
		return 1;
    }
    return 0;
}

static inline void update_periodic(rt_thread *t)
{
    if (t->type == PERIODIC)
    {
        t->deadline  = cur_time() + t->constraints->periodic.period;
        t->run_time = 0;
    }
}

uint64_t cur_time()
{
    return rdtsc();
}


int rt_admit(rt_scheduler *scheduler, rt_thread *thread)
{
    if (thread->type == PERIODIC)
    {
        uint64_t per_util = get_per_util(scheduler->runnable, scheduler->pending);

        printk("UTIL FACTOR =  \t%llu\n", per_util);
        printk("NEW UTIL FACTOR =  \t%llu\n", per_util + ((thread->constraints->periodic.slice * 100000) / thread->constraints->periodic.period));
        
        
        if ((per_util + (thread->constraints->periodic.slice * 100000) / thread->constraints->periodic.period) > PERIODIC_UTIL) {
            RT_SCHED_ERROR("PERIODIC: Admission denied utilization factor overflow!\n");
            return 0;
        }
    } else if (thread->type == SPORADIC)
    {
        uint64_t spor_util = get_spor_util(scheduler->runnable);
        
        if (spor_util > SPORADIC_UTIL) {
            RT_SCHED_DEBUG("SPORADIC: Admission denied utilization factor overflow!\n");
            return 0;
        }
    }

    return 1;
}

static inline uint64_t get_avg_per(rt_queue *runnable, rt_queue *pending, rt_thread *new_thread)
{
    uint64_t sum_period = 0;
    uint64_t num_periodic = 0;
    int i;
    
    for (i = 0; i < runnable->size; i++)
    {
        rt_thread *thread = runnable->threads[i];
        if (thread->type == PERIODIC) {
            sum_period += thread->constraints->periodic.period;
            num_periodic++;
        }
    }
    
    for (i = 0; i < pending->size; i++)
    {
        rt_thread *thread = pending->threads[i];
        if (thread->type == PERIODIC) {
            sum_period += thread->constraints->periodic.period;
            num_periodic++;
        }
    }
    
    if (new_thread->type == PERIODIC)
    {
        sum_period += new_thread->constraints->periodic.period;
        num_periodic++;
    }
    
    sum_period += QUANTUM;
    num_periodic++;
    return (sum_period / num_periodic);
}

static inline uint64_t get_min_per(rt_queue *runnable, rt_queue *pending, rt_thread *thread)
{
    uint64_t min_period = 0xFFFFFFFFFFFFFFFF;
    int i;
    for (i = 0; i < runnable->size; i++)
    {
        rt_thread *thread = runnable->threads[i];
        if (thread->type == PERIODIC)
        {
            min_period = MIN(thread->constraints->periodic.period, min_period);
        }
    }
    
    for (i = 0; i < pending->size; i++)
    {
        rt_thread *thread = pending->threads[i];
        if (thread->type == PERIODIC)
        {
            min_period = MIN(thread->constraints->periodic.period, min_period);
        }
    }
    return min_period;
}

static inline uint64_t get_per_util(rt_queue *runnable, rt_queue *pending)
{
    uint64_t util = 0;
    
    int i;
    for (i = 0; i < runnable->size; i++)
    {
        rt_thread *thread = runnable->threads[i];
        if (thread->type == PERIODIC) {
            util += (thread->constraints->periodic.slice * 100000) / thread->constraints->periodic.period;
        }
    }
    
    for (i = 0; i < pending->size; i++)
    {
        rt_thread *thread = pending->threads[i];
        if (thread->type == PERIODIC) {
            util += (thread->constraints->periodic.slice * 100000) / thread->constraints->periodic.period;
        }
    }
    
    return util;
}

static inline uint64_t get_spor_util(rt_queue *runnable)
{
    uint64_t util = 0;
    
    int i;
    for (i = 0; i < runnable->size; i++)
    {
        rt_thread *thread = runnable->threads[i];
        if (thread->type == SPORADIC)
        {
            util += (thread->constraints->sporadic.work * 100000) / (thread->deadline - cur_time());
        }
    }
    return util;
}

static void test_real_time(void *in)
{
    while (1)
    {
      printk("Inside thread %d\n", (int)(uint64_t)in);
        udelay(10000);
    }
}

void rt_start(uint64_t sched_slice_time, uint64_t sched_period) {

    nk_thread_id_t sched;

    rt_constraints *c = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints p = {sched_period, sched_slice_time};
    c->periodic = p;

    nk_thread_start_sim((nk_thread_fun_t)sched_sim, NULL, NULL, 0, 0, &sched, my_cpu_id(), PERIODIC, c, 0);

    nk_thread_id_t r;
    nk_thread_id_t s;
    nk_thread_id_t t;
    nk_thread_id_t u;
    nk_thread_id_t v;
    nk_thread_id_t w;
    nk_thread_id_t x;
    nk_thread_id_t y;
    
    rt_constraints *constraints_first = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_first = {(100000000), (10000000)};
    constraints_first->periodic = per_constr_first;
    
    rt_constraints *constraints_second = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_second = {(50000000), (5000000)};
    constraints_second->periodic = per_constr_second;
    
    rt_constraints *constraints_third = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_third = {(2500000), (250000)};
    constraints_third->periodic = per_constr_third;
    
    rt_constraints *constraints_fifth = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_fifth = {(50000000), (5000000)};
    constraints_fifth->periodic = per_constr_fifth;
    
    rt_constraints *constraints_six = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_six = {(50000000), (5000000)};
    constraints_six->periodic = per_constr_six;
    
    rt_constraints *constraints_seven = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_seven = {(500000000), (5000000)};
    constraints_seven->periodic = per_constr_seven;
    
    rt_constraints *constraints_fourth = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct aperiodic_constraints aper_constr = {2};
    constraints_fourth->aperiodic = aper_constr;
    
    rt_constraints *constraints_eighth = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_eighth = {(500000000000), (500000000)};
    constraints_eighth->periodic = per_constr_eighth;

    uint64_t first = 1, second = 2, third = 3, fourth = 4, five = 5, six = 6, seven = 7, eight = 8;
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)first, NULL, 0, 0, &r, my_cpu_id(), PERIODIC, constraints_first, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)second, NULL, 0, 0, &s, my_cpu_id(), PERIODIC, constraints_second, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)third, NULL, 0, 0, &t, my_cpu_id(), PERIODIC, constraints_third, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)five, NULL, 0, 0, &v, my_cpu_id(), PERIODIC, constraints_fifth, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)six, NULL, 0, 0, &w, my_cpu_id(), PERIODIC, constraints_six, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)seven, NULL, 0, 0, &x, my_cpu_id(), PERIODIC, constraints_seven, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)fourth, NULL, 0, 0, &u, my_cpu_id(), APERIODIC, constraints_fourth, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)eight, NULL, 0, 0, &y, my_cpu_id(), PERIODIC, constraints_eighth, 0);  
  
    printk("Joined test thread.\n");

}

static int test_sum(void) {
    const int size = 100;
    int sum = 0, i = 0;
    nk_thread_t *me = get_cur_thread();
    for (i = 0; i < size; i++) {
        sum += i;
        udelay(10000);
        printk("INSIDE FUNCTION %x.\n", me->tid);
    }

    return sum;
}


static void sched_sim(void *scheduler) {

	rt_simulator *sim = init_simulator();

    struct sys_info *sys = per_cpu_get(system);
    rt_scheduler *sched = sys->cpus[my_cpu_id()]->rt_sched;

    while (1) {
        rt_thread *new = list_dequeue(sched->arrival);
        if (new != NULL) {
            if (new->type == APERIODIC) {
                enqueue_thread(sched->aperiodic, new);
            } else {
                uint64_t context_time = 1000;
                uint64_t sched_time = sched->run_time;
                printk("Sched time is %llu\n", sched_time);
                uint64_t current_time = 0;

                int finished_max = 0;
                int failed = 0;

                int admission_check = rt_admit(sched, new);
                if (admission_check) {
                    nk_thread_t *me = get_cur_thread();
                    rt_thread *this = me->rt_thread;
                    copy_threads_sim(sim, sched, new, this);

                    rt_thread_sim *next = min_periodic(sim);
                    rt_thread_sim *max = max_periodic(sim);
                    update_enter_logic(next, current_time);
                    current_time += set_timer_logic(sim, next, current_time);
                    

                    while (finished_max <= 1) {
                        update_exit_logic(next, current_time);
                        next = rt_need_resched_logic(sim, next, current_time, &failed, &finished_max, max);
                        if (failed) break;
                        current_time += (context_time + sched_time);
                        update_enter_logic(next, current_time);
                        current_time += set_timer_logic(sim, next, current_time);
                    }

                    if (failed) {
                         RT_SCHED_ERROR("THREAD DENIED ENTRY.\n");
                         new->status = DENIED;
                    } else {
                        printk("THREAD ADMITTED\n");
                        new->status = ADMITTED;
			new->deadline = cur_time() + new->constraints->periodic.period;
                        enqueue_thread(sched->runnable, new);
                    }

                    free_threads_sim(sim);
                }
            }
        }


        rt_thread *d = list_dequeue(sched->exited);
        while (d != NULL) {
            rt_thread *e = NULL;

            if (d->status == TOBE_REMOVED) {
                e = remove_thread(d); 
            } 

            if (d->status == SLEEPING) {
                e = list_remove(sched->sleeping, d);
            }

            if (d->status != REMOVED && e == NULL) {
                RT_SCHED_ERROR("REMOVING THREAD INCORRECTLY.\n");
            } else {
                d->status = REMOVED;
            }
        }   
    }
}

static rt_thread_sim* max_periodic(rt_simulator *simulator) {
    rt_queue_sim *runnable = simulator->runnable;
    rt_queue_sim *pending = simulator->pending;
    rt_thread_sim *max_thread = NULL;  
    uint64_t max_period = 0;
    int i = 0;
    for (i = 0; i < runnable->size; i++) {
        rt_thread_sim *thread = runnable->threads[i];
        if (thread->type == PERIODIC) {
            if (thread->constraints->periodic.period > max_period) {
                max_period = thread->constraints->periodic.period;
                max_thread = thread;
            }
        }
    }
    
    for (i = 0; i < pending->size; i++) {
        rt_thread_sim *thread = pending->threads[i];
        if (thread->type == PERIODIC) {
            if (thread->constraints->periodic.period > max_period) {
                max_period = thread->constraints->periodic.period;
                max_thread = thread;
            }
        }
    }
    return max_thread;
}

static rt_thread_sim* min_periodic(rt_simulator *simulator) {
    rt_queue_sim *runnable = simulator->runnable;
    rt_queue_sim *pending = simulator->pending;
    rt_thread_sim *min_thread = NULL;  
    uint64_t min_period = 0xFFFFFFFFFFFFFFFF;
    int i = 0;
    for (i = 0; i < runnable->size; i++) {
        rt_thread_sim *thread = runnable->threads[i];
        if (thread->type == PERIODIC) {
            if (thread->constraints->periodic.period < min_period) {
                min_period = thread->constraints->periodic.period;
                min_thread = thread;
            }
        }
    }
    
    for (i = 0; i < pending->size; i++) {
        rt_thread_sim *thread = pending->threads[i];
        if (thread->type == PERIODIC) {
            if (thread->constraints->periodic.period < min_period) {
                min_period = thread->constraints->periodic.period;
                min_thread = thread;
            }
        }
    }
    return min_thread;
}

static void copy_threads_sim(rt_simulator *simulator, rt_scheduler *scheduler, rt_thread *new, rt_thread *this) {
    int i;

    for (i = 0; i < scheduler->runnable->size; i++) {
        rt_thread *s = scheduler->runnable->threads[i];
        rt_thread_sim *d = (rt_thread_sim *)MALLOC(sizeof(rt_thread_sim));
        d->type = s->type;
        d->q_type = s->q_type;
        d->status = ADMITTED;

        rt_constraints *constraints = (rt_constraints *)MALLOC(sizeof(rt_constraints));
        if (d->type == PERIODIC) {
            struct periodic_constraints constr = {(s->constraints->periodic.period), (s->constraints->periodic.slice)};
            constraints->periodic = constr;
        } else if (d->type == SPORADIC) {
            struct sporadic_constraints constr = {(s->constraints->sporadic.work)};
            constraints->sporadic = constr;
        }
        d->constraints = constraints;
        d->start_time = 0;
        d->run_time = 0;
        d->deadline = constraints->periodic.period;
        d->exit_time = 0;
        enqueue_thread_logic(simulator->runnable, d);
    }

    printk("SCHEDULER APERIODIC SIZE IS %d\n", scheduler->aperiodic->size);
    for (i = 0; i < scheduler->aperiodic->size; i++) {
        rt_thread *s = scheduler->aperiodic->threads[i];
        rt_thread_sim *d = (rt_thread_sim *)MALLOC(sizeof(rt_thread_sim));
        d->type = s->type;
        d->q_type = s->q_type;
        d->status = ADMITTED;

        rt_constraints *constraints = (rt_constraints *)MALLOC(sizeof(rt_constraints));
        struct aperiodic_constraints constr = {(s->constraints->aperiodic.priority)};
        constraints->aperiodic = constr;

        d->constraints = constraints;
        d->start_time = 0;
        d->run_time = 0;
        d->deadline = 0;
        d->exit_time = 0;
        enqueue_thread_logic(simulator->aperiodic, d);
    }


    for (i = 0; i < scheduler->pending->size; i++) {
        rt_thread *s = scheduler->pending->threads[i];
        rt_thread_sim *d = (rt_thread_sim *)MALLOC(sizeof(rt_thread_sim));
        d->type = s->type;
        d->q_type = RUNNABLE_QUEUE;
        d->status = ADMITTED;

        rt_constraints *constraints = (rt_constraints *)MALLOC(sizeof(rt_constraints));
        if (d->type == PERIODIC) {
            struct periodic_constraints constr = {(s->constraints->periodic.period), (s->constraints->periodic.slice)};
            constraints->periodic = constr;
        } else if (d->type == SPORADIC) {
            struct sporadic_constraints constr = {(s->constraints->sporadic.work)};
            constraints->sporadic = constr;
        }

        d->constraints = constraints;
        d->start_time = 0;
        d->run_time = 0;
        if (d->type == PERIODIC) {
           d->deadline = constraints->periodic.period; 
       } else {
            d->deadline = constraints->sporadic.work;
       }
        
        d->exit_time = 0;

        enqueue_thread_logic(simulator->runnable, d);
    }

    rt_thread_sim *new_sim = (rt_thread_sim *)MALLOC(sizeof(rt_thread_sim));
    new_sim->type = new->type;
    new_sim->q_type = RUNNABLE_QUEUE;
    new_sim->status = ADMITTED;

    rt_constraints *constraints = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    if (new->type == PERIODIC) {
        struct periodic_constraints constr = {(new->constraints->periodic.period), (new->constraints->periodic.slice)};
        constraints->periodic = constr;
    } else if (new->type == SPORADIC) {
        struct sporadic_constraints constr = {(new->constraints->sporadic.work)};
        constraints->sporadic = constr;
    }
    new_sim->constraints = constraints;
    new_sim->start_time = 0;
    new_sim->run_time = 0;
    new_sim->exit_time = 0;

    if (new_sim->type == PERIODIC) {
        new_sim->deadline = constraints->periodic.period; 
    } else {
        new_sim->deadline = constraints->sporadic.work;
    }

    enqueue_thread_logic(simulator->runnable, new_sim);

    rt_thread_sim *sched_per = (rt_thread_sim *)MALLOC(sizeof(rt_thread_sim));
    sched_per->type = this->type;
    sched_per->q_type = RUNNABLE_QUEUE;
    sched_per->status = ADMITTED;

    rt_constraints *sched_con = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    if (this->type == PERIODIC) {
        struct periodic_constraints constr = {(this->constraints->periodic.period), (this->constraints->periodic.slice)};
        sched_con->periodic = constr;
    } else if (this->type == SPORADIC) {
        struct sporadic_constraints constr = {(this->constraints->sporadic.work)};
        sched_con->sporadic = constr;
    }
    sched_per->constraints = sched_con;
    sched_per->start_time = 0;
    sched_per->run_time = 0;
    sched_per->exit_time = 0;

    if (sched_per->type == PERIODIC) {
        sched_per->deadline = constraints->periodic.period; 
    } else {
        sched_per->deadline = constraints->sporadic.work;
    }

    enqueue_thread_logic(simulator->runnable, sched_per);
}

static void free_threads_sim(rt_simulator *simulator) {
    int i = 0;
    for (i = 0; i < simulator->runnable->size; i++) {
        FREE(simulator->runnable->threads[i]->constraints);
        FREE(simulator->runnable->threads[i]);
    }
    simulator->runnable->size = 0;

    for (i = 0; i < simulator->aperiodic->size; i++) {
        FREE(simulator->aperiodic->threads[i]->constraints);
        FREE(simulator->aperiodic->threads[i]);
    }
    simulator->aperiodic->size = 0;

    for (i = 0; i < simulator->pending->size; i++) {
        FREE(simulator->pending->threads[i]->constraints);
        FREE(simulator->pending->threads[i]);
    }
    simulator->pending->size = 0;
}

static rt_thread_sim* rt_need_resched_logic(rt_simulator *simulator, rt_thread_sim *thread, uint64_t time, int *failed, int *finished_max, rt_thread_sim *max)
{
    rt_thread_sim *next = NULL;

    while (simulator->pending->size > 0)
    {
        if (simulator->pending->threads[0]->deadline < time)
        {
            rt_thread_sim *arrived_thread = dequeue_thread_logic(simulator->pending);
            update_periodic_logic(arrived_thread, time);
            enqueue_thread_logic(simulator->runnable, arrived_thread);
            continue;
        } else
        {
            break;
        }
    }
    
    switch (thread->type) {
        case APERIODIC:
            thread->constraints->aperiodic.priority = thread->run_time;
            
            if (simulator->runnable->size > 0)
            {
                enqueue_thread_logic(simulator->aperiodic, thread);
                next = dequeue_thread_logic(simulator->runnable);
                return next;
            }

            enqueue_thread_logic(simulator->aperiodic, thread);
            next = dequeue_thread_logic(simulator->aperiodic);
            return next;

            break;
            
        case SPORADIC:
            if (thread->run_time >= thread->constraints->sporadic.work) {
                if (simulator->runnable->size > 0) {
                    next = dequeue_thread_logic(simulator->runnable);
                    return next;
                }
                next = dequeue_thread_logic(simulator->aperiodic);
                return next;
            } else {
                if (simulator->runnable->size > 0)
                {
                    if (thread->deadline > simulator->runnable->threads[0]->deadline) {
                        next = dequeue_thread_logic(simulator->runnable);
                        enqueue_thread_logic(simulator->runnable, thread);
                        return next;
                    }
                }
            }

            return thread;
            break;
            
        case PERIODIC:
            if (thread->run_time >= thread->constraints->periodic.slice) {
                if (thread == max) {
                    printk("INC MAX\n");
                    (*finished_max)++;
                }
                if (check_deadlines_logic(thread, time)) {
                    *failed = 1;
                    update_periodic_logic(thread, time);
                    enqueue_thread_logic(simulator->runnable, thread);
                } else {
                    enqueue_thread_logic(simulator->pending, thread);
                }
                if (simulator->runnable->size > 0) {
                    next = dequeue_thread_logic(simulator->runnable);
                    return next;
                }
                next = dequeue_thread_logic(simulator->aperiodic);
                if (next == NULL) {
                    return thread;
                }
                return next;
            } else {
                if (simulator->runnable->size > 0)
                {
                    if (thread->deadline > simulator->runnable->threads[0]->deadline) {
                        next = dequeue_thread_logic(simulator->runnable);
                        enqueue_thread_logic(simulator->runnable, thread);
                        return next;
                    }
                }
            }
            return thread;
        default:
            return thread;
    }    
}

static inline void update_exit_logic(rt_thread_sim *t, uint64_t time)
{
    t->exit_time = time;
    t->run_time += (t->exit_time - t->start_time);
}

static inline void update_enter_logic(rt_thread_sim *t, uint64_t time)
{
    t->start_time = time;
}

static int check_deadlines_logic(rt_thread_sim *t, uint64_t time)
{
    if (time > t->deadline) {
        printk("INSIDE CHECK DEADLINES LOGIC. TIME IS %llu AND DEADLINE IS %llu\n", time, t->deadline);
        return 1;
    }
    return 0;
}

static inline void update_periodic_logic(rt_thread_sim *t, uint64_t time)
{
    if (t->type == PERIODIC)
    {
        t->deadline  = time + t->constraints->periodic.period;
        t->run_time = 0;
    }
}

static uint64_t set_timer_logic(rt_simulator *simulator, rt_thread_sim *thread, uint64_t time)
{
    if (simulator->pending->size > 0 && thread) {
        rt_thread_sim *next = simulator->pending->threads[0];
        if (thread->type == PERIODIC)
        {
            return umin(next->deadline - time, (thread->constraints->periodic.slice - thread->run_time));
        } else
        {
            return umin(next->deadline - time, QUANTUM);
        }
    } else if (simulator->pending->size == 0 && thread) {
        if (thread->type == PERIODIC)
        {
            return (thread->constraints->periodic.slice - thread->run_time);
        } else {
            return QUANTUM;
        }
    } else {
        return QUANTUM;
    }
}



void nk_rt_test()
{
    nk_thread_id_t r;
    nk_thread_id_t s;
    nk_thread_id_t t;
    nk_thread_id_t u;
    nk_thread_id_t v;
    nk_thread_id_t w;
    nk_thread_id_t x;
    nk_thread_id_t y;
    
    
    
    rt_constraints *constraints_first = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_first = {(10000000000), (10000000)};
    constraints_first->periodic = per_constr_first;
    
    rt_constraints *constraints_second = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_second = {(5000000000), (5000000)};
    constraints_second->periodic = per_constr_second;
    
    rt_constraints *constraints_third = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_third = {(250000000), (250000)};
    constraints_third->periodic = per_constr_third;
    
    rt_constraints *constraints_fifth = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_fifth = {(500000000), (5000000)};
    constraints_fifth->periodic = per_constr_fifth;
    
    rt_constraints *constraints_six = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_six = {(5000000000), (5000000)};
    constraints_six->periodic = per_constr_six;
    
    rt_constraints *constraints_seven = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_seven = {(5000000000), (5000000)};
    constraints_seven->periodic = per_constr_seven;
    
    rt_constraints *constraints_fourth = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct aperiodic_constraints aper_constr = {2};
    constraints_fourth->aperiodic = aper_constr;
    
    rt_constraints *constraints_eighth = (rt_constraints *)MALLOC(sizeof(rt_constraints));
    struct periodic_constraints per_constr_eighth = {(500000000000), (500000000)};
    constraints_eighth->periodic = per_constr_eighth;

    uint64_t first = 1, second = 2, third = 3, fourth = 4, five = 5, six = 6, seven = 7, eight = 8;
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)first, NULL, 0, 0, &r, my_cpu_id(), PERIODIC, constraints_first, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)second, NULL, 0, 0, &s, my_cpu_id(), PERIODIC, constraints_second, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)third, NULL, 0, 0, &t, my_cpu_id(), PERIODIC, constraints_third, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)five, NULL, 0, 0, &v, my_cpu_id(), PERIODIC, constraints_fifth, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)six, NULL, 0, 0, &w, my_cpu_id(), PERIODIC, constraints_six, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)seven, NULL, 0, 0, &x, my_cpu_id(), PERIODIC, constraints_seven, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)fourth, NULL, 0, 0, &u, my_cpu_id(), APERIODIC, constraints_fourth, 0);
    nk_thread_start((nk_thread_fun_t)test_real_time, (void *)eight, NULL, 0, 0, &y, my_cpu_id(), PERIODIC, constraints_eighth, 0);	
}

static inline uint64_t umin(uint64_t x, uint64_t y)
{
    return (x < y) ? x : y;
}

void rt_thread_exit(rt_thread *thread) {

    while (thread->status == ARRIVED);

    if (thread->status == DENIED) {
        return;
    }

    if (thread->status != SLEEPING) {
        thread->status = TOBE_REMOVED;
    }
    struct sys_info *sys = per_cpu_get(system);
    rt_scheduler *sched = sys->cpus[my_cpu_id()]->rt_sched;
    list_enqueue(sched->exited, thread);
}
