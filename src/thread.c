#include <nautilus.h>
#include <cpu.h>
#include <assert.h>
#include <irq.h>
#include <idle.h>
#include <paging.h>
#include <thread.h>

#include <lib/liballoc.h>


#ifndef NAUT_CONFIG_DEBUG_THREADS
#undef  DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define SCHED_PRINT(fmt, args...) printk("SCHED: " fmt, ##args)


/* this is the currently running thread */
thread_t * cur_thread;

/* TODO: lock this */
static long next_pid = 0;

static struct thread_queue * global_run_q;
static struct thread_queue * global_thread_list;

extern addr_t boot_stack;

extern void thread_switch(thread_t*);



static struct thread_queue*
thread_queue_create (void) 
{
    struct thread_queue * q = NULL;

    q = malloc(sizeof(struct thread_queue));

    if (!q) {
        ERROR_PRINT("Could not allocate thread queue\n");
        return NULL;
    }

    INIT_Q(q);
    spinlock_init(&(q->lock));
    return q;
}


/* NOTE: this does not delete the threads in the queue, just
 * their entries in the queue
 */
static void 
thread_queue_destroy (struct thread_queue * q)
{
    thread_t * tmp = NULL;
    thread_t * t = NULL;
    spin_lock(&(q->lock));

    list_for_each_entry_safe(t, tmp, &(q->queue), q_node) {
        list_del(&(t->q_node));
    }

    spin_unlock(&(q->lock));

    free(q);
}


static void 
enqueue_entry (struct thread_queue * q, struct list_head * entry) 
{
    spin_lock(&(q->lock));
    list_add_tail(entry, &(q->queue));
    spin_unlock(&(q->lock));
}


static void
dequeue_entry (struct thread_queue * q, struct list_head * entry)
{
    spin_lock(&(q->lock));
    if (!list_empty_careful(&(q->queue))) {
        list_del(entry);
    }
    spin_unlock(&(q->lock));
}


static inline void 
enqueue_thread_on_runq (thread_t * t) 
{
    struct thread_queue * q = global_run_q;
    enqueue_entry(q, &(t->q_node));
}


static inline void 
enqueue_thread_on_waitq (thread_t * waiter, struct thread_queue * waitq)
{
    enqueue_entry(waitq, &(waiter->wait_node));
}


static inline void
dequeue_thread_from_waitq (thread_t * waiter, struct thread_queue * waitq)
{
    dequeue_entry(waitq, &(waiter->wait_node));
}


static inline void
dequeue_thread_from_runq (thread_t * t)
{
    struct thread_queue * q = global_run_q;
    dequeue_entry(q, &(t->q_node));
}


static inline void
enqueue_thread_on_tlist (thread_t * t)
{
    struct thread_queue * q = global_thread_list;
    enqueue_entry(q, &(t->thr_list_node));
}


static inline void
dequeue_thread_from_tlist (thread_t * t)
{
    struct thread_queue * q = global_thread_list;
    dequeue_entry(q, &(t->thr_list_node));
}


/* this function assumes interrupts are off */
static int
thread_detach (thread_t * t)
{
    ASSERT(t->refcount > 0);

    if (--t->refcount == 0) {
        thread_destroy(t);
    }

    return 0;
}


void
exit (void * retval) 
{
    thread_t * me = cur_thread;

    if (irqs_enabled()) {
        cli();
    }

    me->exited = 1;
    me->exit_status = retval;

    /* wake up everyone who is waiting on me */
    wake_waiters();

    schedule();

    /* we should never get here! */
    panic("Should never get here!\n");
}


int
join (thread_t * t, void ** retval)
{

    ASSERT(irqs_enabled());
    ASSERT(t->owner == cur_thread);
    cli();
    if (t->exited == 1) {
        if (t->exit_status) {
            *retval = t->exit_status;
        }
        return 0;
    } else {
        while (!t->exited) {
            wait(t->waitq);
        }
    }

    *retval = t->exit_status;

    thread_detach(t);

    sti();
    return 0;
}


/* 
 * wait
 *
 * wait on a given thread
 * we shouldn't come in here with interrupts on
 *
 */
/* TODO: do we need to worry about interrupts here? */
void
wait (struct thread_queue * wq)
{
    /* make sure we're not putting ourselves on our 
     * own waitq */
    ASSERT(!irqs_enabled());
    ASSERT(wq != cur_thread->waitq);

    enqueue_thread_on_waitq(cur_thread, wq);
    schedule();
}


/*
 * get_runnable_thread
 *
 * get the next thread in the run queue
 *
 */
static thread_t *
get_runnable_thread (void)
{
    thread_t * runnable = NULL;

    spin_lock(&(global_run_q->lock));

    if (!list_empty_careful(&(global_run_q->queue))) {
        struct list_head * first = global_run_q->queue.next;
        runnable = list_entry(first, thread_t, q_node);
        list_del(&(runnable->q_node));
    }

    spin_unlock(&(global_run_q->lock));
    return runnable;
}


/*
 * yield
 *
 * schedule some other thread
 *
 */
void 
yield (void)
{
    cli();
    enqueue_thread_on_runq(cur_thread);
    schedule();
    sti();
}

/*
 * wake_waiters
 *
 * wake all threads that are waiting on me
 *
 */
void 
wake_waiters (void)
{
    thread_t * me  = cur_thread;
    thread_t * tmp = NULL;
    thread_t * t   = NULL;

    spin_lock(&(me->waitq->lock));
    if (list_empty_careful(&(me->waitq->queue))) {
        spin_unlock(&(me->waitq->lock));
        return;
    }

    list_for_each_entry_safe(t, tmp, &(me->waitq->queue), wait_node) {
        /* KCH TODO: probably shouldn't be holding lock here... */
        enqueue_thread_on_runq(t);
        list_del(&(t->wait_node));
    }

    spin_unlock(&(me->waitq->lock));
}


static thread_t* 
get_cur_thread (void)
{
    return cur_thread;
}


static int
thread_init (thread_t * t, void * stack, uint8_t is_detached)
{
    memset(t, 0, sizeof(thread_t));
    t->stack    = stack;
    t->rsp      = (uint64_t)stack + PAGE_SIZE;
    t->pid      = next_pid++;
    t->refcount = is_detached ? 1 : 2; // thread references itself as well
    t->owner    = get_cur_thread();

    t->waitq = thread_queue_create();
    if (!t->waitq) {
        ERROR_PRINT("Could not create thread's wait queue\n");
        return -1;
    }

    return 0;
}


/* 
 * thread_create
 *
 * creates a thread. 
 * its stack wil not be initialized with any intial data, 
 * and it will go on the thread list, but it wont be runnable
 *
 */
thread_t* 
thread_create (thread_fun_t fun, void * arg, uint8_t is_detached)
{
    thread_t * t = NULL;
    void * stack = NULL;

    t = malloc(sizeof(thread_t));
    if (!t) {
        ERROR_PRINT("Could not allocate thread struct\n");
        return NULL;
    }
    memset(t, 0, sizeof(thread_t));

    stack = (void*)alloc_page();
    if (!stack) {
        ERROR_PRINT("Could not allocate thread stack\n");
        goto out_err;
    }

    if (thread_init(t, stack, is_detached) < 0) {
        ERROR_PRINT("Could not initialize thread\n");
        goto out_err1;
    }
    
    enqueue_thread_on_tlist(t);

    return t;

out_err1:
    free_page((addr_t)stack);
out_err:
    free(t);
    return NULL;
}

static void
thread_cleanup (void)
{
    DEBUG_PRINT("thread (%d) exiting\n", cur_thread->pid);
    exit(0);
}


/*
 * the first thing a thread will do
 * when it starts is enable interrupts
 */
static void
thread_entry (void)
{
    sti();
}

/*
 * utility function for setting up
 * a thread's stack 
 */
static inline void
thread_push (thread_t * t, uint64_t x)
{
    t->rsp -= 8;
    *(uint64_t*)(t->rsp) = x;
}


static void
thread_setup_init_stack (thread_t * t, thread_fun_t fun, void * arg)
{

#define THREAD_SETUP_SIZE  (64+120)
    // this will set the initial GPRs to 0
    memset((void*)t->rsp, 0, THREAD_SETUP_SIZE);

    thread_push(t, (uint64_t)&thread_cleanup);
    thread_push(t, (uint64_t)fun);
    thread_push(t, (uint64_t)KERNEL_DS); //SS
    thread_push(t, (uint64_t)(t->rsp+8)); // rsp
    thread_push(t, (uint64_t)0UL); // rflags
    thread_push(t, (uint64_t)KERNEL_CS);
    thread_push(t, (uint64_t)&thread_entry);
    thread_push(t, 0); // err no
    thread_push(t, 0); // intr no
    *(uint64_t*)(t->rsp-64) = (uint64_t)arg;
    t->rsp -= 120; // account for the GPRS;
}


/*
 * thread_destroy
 *
 * destroys a thread and reclaims its memory (its stack page mostly)
 * interrupts should be off
 *
 */
void 
thread_destroy (thread_t * t)
{
    ASSERT(!irqs_enabled());

    dequeue_thread_from_runq(t);
    dequeue_thread_from_tlist(t);

    /* remove it from any wait queues */
    list_del(&(t->wait_node));

    /* remove its own wait queue 
     * (waiters should already have been notified */
    thread_queue_destroy(t->waitq);

    free_page((addr_t)t->stack);
    free(t);
}


/* 
 * thread_start
 *
 * creates a thread and puts it on the run queue 
 */
thread_t*
thread_start (thread_fun_t fun, void * arg, uint8_t is_detached)
{
    thread_t * t = NULL;
    t = thread_create(fun, arg, is_detached);
    if (!t) {
        ERROR_PRINT("Could not create thread\n");
        return NULL;
    }

    thread_setup_init_stack(t, fun, arg);
    enqueue_thread_on_runq(t);
    return t;
}


/* 
 * schedule
 *
 * pick a thread to run
 *
 */
void
schedule (void) 
{
    struct thread * runme = NULL;

    ASSERT(!irqs_enabled());

    runme = get_runnable_thread();

    if (!runme) {
        ERROR_PRINT("Nothing to run\n");
        return;
    }

    thread_switch(runme);
}


/* 
 * sched_init
 *
 * entry point into the scheduler at bootup
 *
 */
int
sched_init (void) 
{
    struct sched_state * sched = NULL;
    thread_t * main = NULL;

    cli();

    SCHED_PRINT("Initializing scheduler\n");

    sched = malloc(sizeof(struct sched_state));
    if (!sched) {
        ERROR_PRINT("Could not allocate scheduler state\n");
        return -1;
    }
    memset(sched, 0, sizeof(struct sched_state));

    sched->run_q = thread_queue_create();
    if (!sched->run_q) {
        ERROR_PRINT("Could not create run queue\n");
        goto out_err;
    }
    global_run_q = sched->run_q;

    sched->thread_list = thread_queue_create();
    if (!sched->thread_list) {
        ERROR_PRINT("Could not create thread list\n");
        goto out_err1;
    }
    global_thread_list = sched->thread_list;


    // first we need to add our current thread as the current thread
    main  = malloc(sizeof(thread_t));
    if (!main) {
        ERROR_PRINT("Could not allocate main thread\n");
        goto out_err2;
    }

    thread_init(main, (void*)&boot_stack, 1);
    main->waitq = thread_queue_create();
    if (!main->waitq) {
        ERROR_PRINT("Could not create main thread's wait queue\n");
        goto out_err3;
    }

    cur_thread = main;

    enqueue_thread_on_tlist(main);

    sti();

    return 0;

out_err3:
    free(main);
out_err2:
    free(sched->thread_list);
out_err1: 
    free(sched->run_q);
out_err:
    free(sched);
    return -1;
}


