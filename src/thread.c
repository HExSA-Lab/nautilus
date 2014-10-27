#include <nautilus.h>
#include <cpu.h>
#include <assert.h>
#include <irq.h>
#include <idle.h>
#include <paging.h>
#include <thread.h>
#include <percpu.h>
#include <atomic.h>
#include <queue.h>
#include <list.h>

#include <lib/liballoc.h>


#ifndef NAUT_CONFIG_DEBUG_THREADS
#undef  DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define SCHED_PRINT(fmt, args...) printk("SCHED: " fmt, ##args)
#define SCHED_DEBUG(fmt, args...) DEBUG_PRINT("SCHED: " fmt, ##args)

static thread_id_t next_tid = 0;

static thread_queue_t * global_thread_list;

extern addr_t boot_stack;

extern void thread_switch(thread_t*);
extern void thread_entry(void *);


static thread_queue_t*
thread_queue_create (void) 
{
    thread_queue_t * q = NULL;

    q = queue_create();

    if (!q) {
        ERROR_PRINT("Could not allocate thread queue\n");
        return NULL;
    }
    return q;
}


/* NOTE: this does not delete the threads in the queue, just
 * their entries in the queue
 */
static void 
thread_queue_destroy (thread_queue_t * q)
{
    // free any remaining entries
    DEBUG_PRINT("Destroying thread queue\n");
    queue_destroy(q, 1);
}


static inline void 
enqueue_thread_on_runq (thread_t * t, int cpu)
{
    thread_queue_t * q = NULL;
    struct sys_info * sys = per_cpu_get(system);

    if (cpu == CPU_ANY || 
        cpu < CPU_ANY  || 
        cpu >= sys->num_cpus || 
        cpu == my_cpu_id()) {

        q = per_cpu_get(run_q);

    } else {
        q = sys->cpus[cpu]->run_q;
    }

    /* bail if the run queue hasn't been created yet */
    if (!q) {
        ERROR_PRINT("Attempt (by cpu %d) to enqueue thread (tid=%d) on non-existant run queue (cpu=%u)\n", my_cpu_id(), t->tid, cpu);
        return;
    }

    t->cur_run_q = q;

    enqueue_entry_atomic(q, &(t->runq_node));
}


static inline void 
enqueue_thread_on_waitq (thread_t * waiter, thread_queue_t * waitq)
{
    enqueue_entry_atomic(waitq, &(waiter->wait_node));
}


static inline thread_t*
dequeue_thread_from_waitq (thread_t * waiter, thread_queue_t * waitq)
{
    thread_t * t = NULL;
    queue_entry_t * elm = dequeue_entry_atomic(waitq, &(waiter->wait_node));
    t = container_of(elm, thread_t, wait_node);
    return t;
}


static inline thread_t*
dequeue_thread_from_runq (thread_t * t)
{
    thread_queue_t * q = t->cur_run_q;
    queue_entry_t * elm = NULL;
    thread_t * ret = NULL;

    /* bail if the run queue doesn't exist */
    if (!q) {
        ERROR_PRINT("Attempt to dequeue thread not on run queue (cpu=%u)\n", my_cpu_id());
        return NULL;
    }

    elm = dequeue_entry_atomic(q, &(t->runq_node));
    ret = container_of(elm, thread_t, runq_node);

    t->cur_run_q = NULL;

    return ret;
}


static inline void
enqueue_thread_on_tlist (thread_t * t)
{
    thread_queue_t * q = global_thread_list;
    enqueue_entry_atomic(q, &(t->thr_list_node));
}


static inline thread_t*
dequeue_thread_from_tlist (thread_t * t)
{
    queue_entry_t * elm = NULL;
    thread_t * ret = NULL;

    thread_queue_t * q = global_thread_list;
    elm = dequeue_entry_atomic(q, &(t->thr_list_node));
    ret = container_of(elm, thread_t, thr_list_node);

    return ret;
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
    thread_t * me = get_cur_thread();

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
    ASSERT(t->owner == get_cur_thread());
    cli();
    if (t->exited == 1) {
        if (t->exit_status) {
            *retval = t->exit_status;
        }
        return 0;
    } else {
        while (!t->exited) {
            wait(t);
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
wait (thread_t * t)
{
    thread_queue_t * wq = t->waitq;
    /* make sure we're not putting ourselves on our 
     * own waitq */
    ASSERT(!irqs_enabled());
    ASSERT(wq != (get_cur_thread()->waitq));

    enqueue_thread_on_waitq(get_cur_thread(), wq);
    schedule();
}


/*
 * get_runnable_thread
 *
 * get the next thread in the specified thread's CPU
 *
 */
static thread_t *
get_runnable_thread (uint32_t cpu)
{
    thread_t * runnable   = NULL;
    thread_queue_t * runq = NULL;
    queue_entry_t * elm   = NULL;
    struct sys_info * sys = per_cpu_get(system);

    if (cpu >= sys->num_cpus || !sys->cpus[cpu]) {
        ERROR_PRINT("Attempt to get thread on invalid CPU (%u)\n", cpu);
        return NULL;
    }

    runq = sys->cpus[cpu]->run_q;

    if (!runq) {
        ERROR_PRINT("Attempt to get thread from invalid run queue on CPU %u\n", cpu);
        return NULL;
    }

    elm = dequeue_first_atomic(runq);
    return container_of(elm, thread_t, runq_node);
}


static thread_t * 
get_runnable_thread_myq (void) 
{
    cpu_id_t id = my_cpu_id();
    return get_runnable_thread(id);
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
    uint8_t flags = irq_disable_save();
    thread_t * t = get_cur_thread();
    enqueue_thread_on_runq(t, t->bound_cpu);
    schedule();
    irq_enable_restore(flags);
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
    thread_t * me  = get_cur_thread();
    queue_entry_t * tmp = NULL;
    queue_entry_t * elm = NULL;

    uint8_t flags = spin_lock_irq_save(&(me->waitq->lock));
    if (list_empty_careful(&(me->waitq->queue))) {
        spin_unlock_irq_restore(&(me->waitq->lock), flags);
        return;
    }

    list_for_each_entry_safe(elm, tmp, &(me->waitq->queue), node) {
        /* KCH TODO: probably shouldn't be holding lock here... */
        thread_t * t = container_of(elm, thread_t, wait_node);
        enqueue_thread_on_runq(t, t->bound_cpu);
        dequeue_entry(&(t->wait_node));
    }

    spin_unlock_irq_restore(&(me->waitq->lock), flags);
}


static int
thread_init (thread_t * t, void * stack, uint8_t is_detached, int cpu)
{
    if (!t) {
        ERROR_PRINT("Given NULL thread pointer...\n");
        return -1;
    }

    t->stack     = stack;
    t->rsp       = (uint64_t)stack + PAGE_SIZE;
    t->tid       = atomic_inc(next_tid) + 1;
    t->refcount  = is_detached ? 1 : 2; // thread references itself as well
    t->owner     = get_cur_thread();
    t->bound_cpu = cpu;

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
 * @fun: the function to run
 * @input: the argument to the thread
 * @output: where the thread should store its output
 * @is_detached: true if this thread won't be attached to a parent (it will
 *               die immediately when it exits)
 * @stack_size: size of the thread's stack. 0 => let us decide
 * @tid: thread id of created thread
 * @cpu: cpu on which to bind the thread. CPU_ANY means any CPU
 *
 */
thread_t* 
thread_create (thread_fun_t fun, 
               void * input,
               void ** output,
               uint8_t is_detached,
               stack_size_t stack_size,
               thread_id_t * tid,
               int cpu)
{
    thread_t * t = NULL;
    void * stack = NULL;

    t = malloc(sizeof(thread_t));
    if (!t) {
        ERROR_PRINT("Could not allocate thread struct\n");
        return NULL;
    }
    memset(t, 0, sizeof(thread_t));

    if (stack_size) {
        stack = (void*)malloc(stack_size);
        t->stack_size = stack_size;
    } else {
        stack = (void*)malloc(PAGE_SIZE);
        t->stack_size =  PAGE_SIZE;
    }

    if (!stack) {
        ERROR_PRINT("Could not allocate thread stack\n");
        goto out_err;
    }

    if (thread_init(t, stack, is_detached, cpu) < 0) {
        ERROR_PRINT("Could not initialize thread\n");
        goto out_err1;
    }
    
    enqueue_thread_on_tlist(t);

    if (tid) {
        *tid = t->tid;
    }

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
    SCHED_DEBUG("Thread (%d) exiting on core %d\n", get_cur_thread()->tid, my_cpu_id());
    exit(0);
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

#define RSP_STACK_OFFSET   8
#define GPR_RDI_OFFSET     48
#define GPR_SAVE_SIZE      120
#define STACK_SAVE_SIZE    64
#define THREAD_SETUP_SIZE  (STACK_SAVE_SIZE + GPR_SAVE_SIZE)

    // this will set the initial GPRs to 0
    memset((void*)t->rsp, 0, THREAD_SETUP_SIZE);

    thread_push(t, (uint64_t)&thread_cleanup);
    thread_push(t, (uint64_t)fun);
    thread_push(t, (uint64_t)KERNEL_DS); //SS
    thread_push(t, (uint64_t)(t->rsp+RSP_STACK_OFFSET)); // rsp
    thread_push(t, (uint64_t)0UL); // rflags
    thread_push(t, (uint64_t)KERNEL_CS);
    thread_push(t, (uint64_t)&thread_entry);
    thread_push(t, 0); // err no
    thread_push(t, 0); // intr no
    *(uint64_t*)(t->rsp-GPR_RDI_OFFSET) = (uint64_t)arg; // we overwrite RDI with the arg
    t->rsp -= GPR_SAVE_SIZE; // account for the GPRS;
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
    dequeue_entry(&(t->wait_node));

    /* remove its own wait queue 
     * (waiters should already have been notified */
    thread_queue_destroy(t->waitq);

    free_page((addr_t)t->stack);
    free(t);
}


/* 
 * thread_start
 *
 * creates a thread and puts it on the specified cpu's run 
 * queue
 *
 * @fun: the function to run
 * @input: the argument to the thread
 * @output: where the thread should store its output
 * @is_detached: true if this thread won't be attached to a parent (it will
 *               die immediately when it exits)
 * @stack_size: size of the thread's stack. 0 => let us decide
 * @tid: thread id of created thread
 * @cpu: cpu on which to bind the thread. CPU_ANY means any CPU 
 *
 */
thread_t* 
thread_start (thread_fun_t fun, 
               void * input,
               void ** output,
               uint8_t is_detached,
               stack_size_t stack_size,
               thread_id_t * tid,
               int cpu)
{
    thread_t * t = NULL;
    t = thread_create(fun, input, output, is_detached, stack_size, tid, cpu);
    if (!t) {
        ERROR_PRINT("Could not create thread\n");
        return NULL;
    }

    thread_setup_init_stack(t, fun, input);

    enqueue_thread_on_runq(t, cpu);
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
    thread_t * runme = NULL;

    ASSERT(!irqs_enabled());

    runme = get_runnable_thread_myq();

    if (!runme) {
        cpu_id_t id = my_cpu_id();
        ERROR_PRINT("Nothing to run (cpu=%d)\n", id);
        return;
    }

    thread_switch(runme);
}


/*
 * sched_init_ap
 *
 * scheduler init routine for APs once they
 * have booted up
 *
 */
int
sched_init_ap (void)
{
    thread_t * me = NULL;
    void * my_stack = NULL;
    cpu_id_t id = my_cpu_id();
    struct cpu * my_cpu = get_cpu();
    uint8_t flags;

    flags = irq_disable_save();

    SCHED_DEBUG("Initializing CPU %u\n", id);

    my_cpu->run_q = thread_queue_create();

    if (!my_cpu->run_q) {
        ERROR_PRINT("Could not create run queue for CPU %u)\n", id);
        goto out_err;
    }

    me = malloc(sizeof(thread_t));
    if (!me) {
        ERROR_PRINT("Could not allocate thread for CPU (%u)\n", id);
        goto out_err1;
    }

    my_stack = malloc(PAGE_SIZE);
    if (!my_stack) {
        ERROR_PRINT("Couldn't allocate new stack for CPU (%u)\n", id);
        goto out_err2;
    }

    if (thread_init(me, my_stack, 1, id) != 0) {
        ERROR_PRINT("Could not init start thread on core %u\n", id);
        goto out_err3;
    }

    // set my current thread
    put_cur_thread(me);

    // start another idle thread
    SCHED_DEBUG("Starting idle thread for cpu %d\n", id);
    thread_start(idle, NULL, NULL, 0, TSTACK_DEFAULT, NULL, id);

    irq_enable_restore(flags);
    return 0;

out_err3:
    free(me->stack);
out_err2:
    free(me);
out_err1:
    thread_queue_destroy(my_cpu->run_q);
out_err:
    sti();
    return -1;
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
    struct cpu * my_cpu = per_cpu_get(system)->cpus[0];
    thread_t * main = NULL;

    cli();

    SCHED_PRINT("Initializing scheduler\n");

    sched = malloc(sizeof(struct sched_state));
    if (!sched) {
        ERROR_PRINT("Could not allocate scheduler state\n");
        goto out_err0;
    }
    memset(sched, 0, sizeof(struct sched_state));

    my_cpu->run_q = thread_queue_create();
    if (!my_cpu->run_q) {
        ERROR_PRINT("Could not create run queue\n");
        goto out_err1;
    }

    sched->thread_list = thread_queue_create();
    if (!sched->thread_list) {
        ERROR_PRINT("Could not create thread list\n");
        goto out_err2;
    }
    global_thread_list = sched->thread_list;


    // first we need to add our current thread as the current thread
    main  = malloc(sizeof(thread_t));
    if (!main) {
        ERROR_PRINT("Could not allocate main thread\n");
        goto out_err3;
    }

    thread_init(main, (void*)&boot_stack, 1, my_cpu_id());
    main->waitq = thread_queue_create();
    if (!main->waitq) {
        ERROR_PRINT("Could not create main thread's wait queue\n");
        goto out_err4;
    }

    put_cur_thread(main);

    enqueue_thread_on_tlist(main);

    sti();

    return 0;

out_err4:
    free(main);
out_err3:
    free(sched->thread_list);
out_err2:
    free(my_cpu->run_q);
out_err1: 
    free(sched);
out_err0:
    sti();
    return -1;
}


thread_id_t
get_tid (void) 
{
    thread_t * t = per_cpu_get(cur_thread);
    return t->tid;
}

thread_id_t
get_parent_tid (void) 
{
    thread_t * t = per_cpu_get(cur_thread);
    if (t && t->owner) {
        return t->owner->tid;
    }
    return -1;
}


