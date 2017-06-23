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
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
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
#include <nautilus/thread.h>
#include <nautilus/percpu.h>
#include <nautilus/atomic.h>
#include <nautilus/queue.h>
#include <nautilus/list.h>
#include <nautilus/errno.h>
#include <nautilus/mm.h>


extern uint8_t malloc_cpus_ready;



#ifndef NAUT_CONFIG_DEBUG_THREADS
#undef  DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define THREAD_INFO(fmt, args...) INFO_PRINT("Thread: " fmt, ##args)
#define THREAD_ERROR(fmt, args...) ERROR_PRINT("Thread: " fmt, ##args)
#define THREAD_DEBUG(fmt, args...) DEBUG_PRINT("Thread: " fmt, ##args)
#define THREAD_WARN(fmt, args...)  WARN_PRINT("Thread: " fmt, ##args)

static unsigned long next_tid = 0;

extern addr_t boot_stack_start;
extern void nk_thread_switch(nk_thread_t*);
extern void nk_thread_entry(void *);
static struct nk_tls tls_keys[TLS_MAX_KEYS];


/****** SEE BELOW FOR EXTERNAL THREAD INTERFACE ********/


nk_thread_queue_t*
nk_thread_queue_create (void) 
{
    nk_thread_queue_t * q = NULL;

    q = nk_queue_create();

    if (!q) {
        THREAD_ERROR("Could not allocate thread queue\n");
        return NULL;
    }
    return q;
}




/* NOTE: this does not delete the threads in the queue, just
 * their entries in the queue
 */
void 
nk_thread_queue_destroy (nk_thread_queue_t * q)
{
    // free any remaining entries
    THREAD_DEBUG("Destroying thread queue\n");
    nk_queue_destroy(q, 1);
}


/*
 * thread_detach
 *
 * detaches a child from its parent
 *
 * @t: the thread to detach
 *
 */
static int
thread_detach (nk_thread_t * t)
{
    preempt_disable();

    ASSERT(t->refcount > 0);

    /* remove me from my parent's child list */
    list_del(&(t->child_node));

    --t->refcount;

    // conditional reaping is done by the scheduler when threads are created
    // this makes the join+exit path much faster in the common case and 
    // bulks reaping events together
    // the user can also explictly reap when needed
    // plus the autoreaper thread can be enabled 
    // the following code can be enabled if you want to reap immediately once
    // a thread's refcount drops to zero
    // 
    //if (t->refcount==0) { 
    //   nk_thread_destroy(t);
    //}

    preempt_enable();

    return 0;
}




static void 
tls_exit (void) 
{
    nk_thread_t * t = get_cur_thread();
    unsigned i, j;
    uint8_t called = 0;

    for (i = 0; i < MIN_DESTRUCT_ITER; i++) {
        for (j = 0 ; j < TLS_MAX_KEYS; j++) {
            void * val = (void*)t->tls[j]; 
            if (val && tls_keys[j].destructor) {
                called = 1;
                t->tls[j] = NULL;
                tls_keys[j].destructor(val);
            }

            if (!called) {
                break;
            }
        }
    }
}




int
_nk_thread_init (nk_thread_t * t, 
		 void * stack, 
		 uint8_t is_detached, 
		 int bound_cpu, 
		 nk_thread_t * parent)
{
    struct sys_info * sys = per_cpu_get(system);

    if (!t) {
        THREAD_ERROR("Given NULL thread pointer...\n");
        return -EINVAL;
    }

    if (bound_cpu>=0 && bound_cpu>=sys->num_cpus) {
	THREAD_ERROR("Impossible CPU binding %d\n",bound_cpu);
	return -EINVAL;
    }


    t->stack      = stack;
    t->rsp        = (uint64_t)stack + t->stack_size - sizeof(uint64_t);
    t->tid        = atomic_inc(next_tid) + 1;
    t->refcount   = is_detached ? 1 : 2; // thread references itself as well
    t->parent     = parent;
    t->bound_cpu  = bound_cpu;
    t->fpu_state_offset = offsetof(struct nk_thread, fpu_state);

    INIT_LIST_HEAD(&(t->children));

    /* I go on my parent's child list */
    if (parent) {
        list_add_tail(&(t->child_node), &(parent->children));
    }

    if (!(t->sched_state = nk_sched_thread_state_init(t,0))) { 
	THREAD_ERROR("Could not create scheduler state for thread\n");
	return -EINVAL;
    }

    t->waitq = nk_thread_queue_create();

    if (!t->waitq) {
        THREAD_ERROR("Could not create thread's wait queue\n");
        return -EINVAL;
    }

    return 0;
}


static void
thread_cleanup (void)
{
    THREAD_DEBUG("Thread (%d) exiting on core %d\n", get_cur_thread()->tid, my_cpu_id());
    nk_thread_exit(0);
}


/*
 * utility function for setting up
 * a thread's stack 
 */
static inline void
thread_push (nk_thread_t * t, uint64_t x)
{
    t->rsp -= 8;
    *(uint64_t*)(t->rsp) = x;
}


static void
thread_setup_init_stack (nk_thread_t * t, nk_thread_fun_t fun, void * arg)
{

#define RSP_STACK_OFFSET   8
#define GPR_RDI_OFFSET     48
#define GPR_RAX_OFFSET     8
#define GPR_SAVE_SIZE      120
#define STACK_SAVE_SIZE    64
#define THREAD_SETUP_SIZE  (STACK_SAVE_SIZE + GPR_SAVE_SIZE)

    /*
     * if this is a thread fork, this part is taken care of
     * in _thread_fork(). There is no function!
     */
    if (fun) {
        thread_push(t, (uint64_t)&thread_cleanup);
        thread_push(t, (uint64_t)fun);
    }

    thread_push(t, (uint64_t)KERNEL_SS);                 // SS
    thread_push(t, (uint64_t)(t->rsp+RSP_STACK_OFFSET)); // rsp
    thread_push(t, (uint64_t)0UL);                       // rflags
    thread_push(t, (uint64_t)KERNEL_CS);
    thread_push(t, (uint64_t)&nk_thread_entry);
    thread_push(t, 0);                                   // dummy error code
    thread_push(t, 0);                                   // intr no

    /*
     * if we have a function, it needs an input argument 
     * so we overwrite its RDI
     */  
    if (fun) {
        *(uint64_t*)(t->rsp-GPR_RDI_OFFSET) = (uint64_t)arg; 
    }

    /* 
     * if this is a thread fork, we return 0 to the child
     * via RAX - note that _fork_return will not restore RAX
     */
    if (!fun) {
        *(uint64_t*)(t->rsp-GPR_RAX_OFFSET) = 0;
    }

    t->rsp -= GPR_SAVE_SIZE;                             // account for the GPRS;
}




/****** EXTERNAL THREAD INTERFACE ******/



/* 
 * nk_thread_create
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
 * @tid: opaque user object for the thread to be set (this is the output)
 * @bound_cpu: cpu on which to bind the thread. CPU_ANY means any CPU
 *
 * return: on error returns -EINVAL, returns 0 on success
 *
 */
int
nk_thread_create (nk_thread_fun_t fun, 
                  void * input,
                  void ** output,
                  uint8_t is_detached,
                  nk_stack_size_t stack_size,
                  nk_thread_id_t * tid,
                  int bound_cpu)
{
    struct sys_info * sys = per_cpu_get(system);
    nk_thread_t * t = NULL;
    int current_cpu = -1;

    t = malloc(sizeof(nk_thread_t));

    if (!t) {
        THREAD_ERROR("Could not allocate thread struct\n");
        return -EINVAL;
    }

    memset(t, 0, sizeof(nk_thread_t));

    if (stack_size) {
        t->stack      = (void*)malloc(stack_size);
        t->stack_size = stack_size;
    } else {
        t->stack      = (void*)malloc(PAGE_SIZE);
        t->stack_size =  PAGE_SIZE;
    }

    if (!t->stack) { 
	THREAD_ERROR("Failed to allocate a stack\n");
	free(t);
	return -EINVAL;
    }

    if (_nk_thread_init(t, t->stack, is_detached, bound_cpu, get_cur_thread()) < 0) {
        THREAD_ERROR("Could not initialize thread\n");
        goto out_err;
    }

    t->status = NK_THR_INIT;
    
    t->fun = fun;
    t->input = input;
    t->output = output;
    
    if (nk_sched_thread_post_create(t)) {
	THREAD_ERROR("Scheduler does not accept thread creation\n");
	goto out_err;
    }

    if (tid) {
        *tid = (nk_thread_id_t)t;
    }

    THREAD_DEBUG("Thread create creating new thread with t=%p, tid=%lu\n", t, t->tid);

    return 0;

out_err:
    free(t->stack);
    free(t);
    return -EINVAL;
}


/* 
 * nk_thread_start
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
 * @tid: the opaque pointer passed to the user (output variable)
 * @bound_cpu: cpu on which to bind the thread. CPU_ANY means any CPU 
 *
 *
 * on error, returns -EINVAL, otherwise 0
 */
int
nk_thread_start (nk_thread_fun_t fun, 
                 void * input,
                 void ** output,
                 uint8_t is_detached,
                 nk_stack_size_t stack_size,
                 nk_thread_id_t * tid,
                 int bound_cpu)
{
    nk_thread_id_t newtid   = NULL;
    nk_thread_t * newthread = NULL;


    THREAD_DEBUG("Start thread, caller %p\n", __builtin_return_address(0));

    if (nk_thread_create(fun, input, output, is_detached, stack_size, &newtid, bound_cpu) < 0) {
        THREAD_ERROR("Could not create thread\n");
        return -1;
    }

    newthread = (nk_thread_t*)newtid;

    if (tid) {
        *tid = newtid;
    }

    return nk_thread_run(newthread);
}

int nk_thread_run(nk_thread_id_t t)
{
  nk_thread_t * newthread = (nk_thread_t*)t;

  THREAD_DEBUG("Trying to execute thread %p (tid %lu)", newthread,newthread->tid);
  
  THREAD_DEBUG("RUN: Function: %llu\n", newthread->fun);
  THREAD_DEBUG("RUN: Bound_CPU: %llu\n", newthread->bound_cpu);
  THREAD_DEBUG("RUN: Current_CPU: %llu\n", newthread->current_cpu);
  
  thread_setup_init_stack(newthread, newthread->fun, newthread->input);
  

  THREAD_DEBUG("Run thread initialized: %p (tid=%lu) stack=%p size=%lu rsp=%p\n",newthread,newthread->tid,newthread->stack,newthread->stack_size,newthread->rsp);

  if (nk_sched_make_runnable(newthread, newthread->current_cpu,1)) { 
      THREAD_ERROR("Scheduler failed to run thread (%p, tid=%u) on cpu %u\n",
		  newthread, newthread->tid, newthread->current_cpu);
      return -1;
  }

#ifdef NAUT_CONFIG_DEBUG_THREADS
  if (newthread->bound_cpu == CPU_ANY) {
      THREAD_DEBUG("Running thread (%p, tid=%u) on [ANY CPU] current_cpu=%d\n", newthread, newthread->tid,newthread->current_cpu); 
  } else {
      THREAD_DEBUG("Running thread (%p, tid=%u) on bound cpu %u\n", newthread, newthread->tid, newthread->current_cpu); 
  }
#endif

  nk_sched_kick_cpu(newthread->current_cpu);

  return 0;
}

int nk_thread_name(nk_thread_id_t tid, char *name)
{
  nk_thread_t * t = (nk_thread_t*)tid;
  strncpy(t->name,name,MAX_THREAD_NAME);
  t->name[MAX_THREAD_NAME-1] = 0;
  return 0;
}


/*
 * wake_waiters
 *
 * wake all threads that are waiting on me
 *
 */
void 
nk_wake_waiters (void)
{
    nk_thread_t * me  = get_cur_thread();
    nk_thread_queue_wake_all(me->waitq);
}

void nk_yield()
{
    struct nk_thread *me = get_cur_thread();

    spin_lock(&me->lock);

    nk_sched_yield(&me->lock);
}


// defined early to allow inlining - this is used in multiple places
static void _thread_queue_wake_all (nk_thread_queue_t * q, int have_lock)
{
    nk_queue_entry_t * elm = NULL;
    nk_thread_t * t = NULL;
    uint8_t flags=0;

    if (in_interrupt_context()) {
	THREAD_DEBUG("[Interrupt Context] Thread %lu (%s) is waking all waiters on thread queue (q=%p)\n", get_cur_thread()->tid, get_cur_thread()->name, (void*)q);
    } else {
	THREAD_DEBUG("[Thread Context] Thread %lu (%s) is waking all waiters on thread queue (q=%p)\n", get_cur_thread()->tid, get_cur_thread()->name, (void*)q);
    }

    ASSERT(q);

    if (!have_lock) { 
	flags = spin_lock_irq_save(&q->lock);
    }

    THREAD_DEBUG("Wakeup: have lock\n");

    while ((elm = nk_dequeue_first(q))) {
        t = container_of(elm, nk_thread_t, wait_node);

	THREAD_DEBUG("Waking %lu (%s), status %lu\n", t->tid,t->name,t->status);

        ASSERT(t);
	ASSERT(t->status == NK_THR_WAITING);

	if (nk_sched_awaken(t, t->current_cpu)) { 
	    THREAD_ERROR("Failed to awaken thread\n");
	    goto out;
	}

	nk_sched_kick_cpu(t->current_cpu);
	THREAD_DEBUG("Waking all waiters on thread queue (q=%p) woke thread %lu (%s)\n", (void*)q,t->tid,t->name);

    }

 out:
    THREAD_DEBUG("Wakeup complete - releasing lock\n");
    if (!have_lock) {
	spin_unlock_irq_restore(&q->lock, flags);
    }
}


/*
 * nk_thread_exit
 *
 * exit from this thread
 *
 * @retval: the value to return to the parent
 *
 * If there is someone waiting on this thread, this 
 * function will wake them up. This will also call
 * any destructors for thread local storage
 *
 */
void nk_thread_exit (void * retval) 
{
    nk_thread_t * me = get_cur_thread();
    nk_thread_queue_t * wq = me->waitq;
    uint8_t flags;

    THREAD_DEBUG("Thread %p (tid=%u (%s)) exiting, joining with children\n", me, me->tid, me->name);

    /* wait for my children to finish */
    nk_join_all_children(NULL);

    THREAD_DEBUG("Children joined\n");

    /* clear any thread local storage that may have been allocated */
    tls_exit();

    THREAD_DEBUG("TLS exit complete\n");

    // lock out anyone else looking at my wait queue
    // we need to do this before we change our own state
    // so we can avoid racing with someone who is attempting
    // to join with us and is putting themselves on our wait queue
    flags = spin_lock_irq_save(&wq->lock);
    preempt_disable();
    irq_enable_restore(flags);   // we only need interrupts off long enough to lock out the scheduler

    THREAD_DEBUG("Lock acquired\n");

    // at this point, we have the lock on our wait queue and preemption is disabled

    me->output      = retval;
    me->status      = NK_THR_EXITED;

    // force arch and compiler to do above writes now
    __asm__ __volatile__ ("mfence" : : : "memory"); 

    THREAD_DEBUG("State update complete\n");

    /* wake up everyone who is waiting on me */
    _thread_queue_wake_all(wq, 1);

    THREAD_DEBUG("Waiting wakeup complete\n");

    me->refcount--;

    THREAD_DEBUG("Thread %p (tid=%u (%s)) exit complete - invoking scheduler\n", me, me->tid, me->name);

    // the scheduler will reenable preemption and release the lock
    nk_sched_exit(&wq->lock);

    /* we should never get here! */
    panic("Should never get here!\n");
}


/*
 * nk_thread_destroy
 *
 * destroys a thread and reclaims its memory (its stack page mostly)
 *
 * @t: the thread to destroy
 *
 */
void 
nk_thread_destroy (nk_thread_id_t t)
{
    nk_thread_t * thethread = (nk_thread_t*)t;

    THREAD_DEBUG("Destroying thread (%p, tid=%lu)\n", (void*)thethread, thethread->tid);

    preempt_disable();

    nk_sched_thread_pre_destroy(thethread);

    /* remove it from any wait queues */
    nk_dequeue_entry(&(thethread->wait_node));

    /* remove its own wait queue 
     * (waiters should already have been notified */
    nk_thread_queue_destroy(thethread->waitq);

    nk_sched_thread_state_deinit(thethread);
    free(thethread->stack);
    free(thethread);
    
    preempt_enable();
}


static int exit_check(void *state)
{
    volatile nk_thread_t *thethread = (nk_thread_t *)state;
    
    THREAD_DEBUG("exit_check: thread (%lu %s) status is %u\n",thethread->tid,thethread->name,thethread->status);
    return thethread->status==NK_THR_EXITED;
}
    

/*
 * nk_join
 *
 * join (wait) on the given thread
 *
 * t: the thread to wait on
 * retval: where the waited-on thread should 
 *         put its output
 *
 * returns  -EINVAL on error, 0 on success
 *
 */
int
nk_join (nk_thread_id_t t, void ** retval)
{
    nk_thread_t *thethread = (nk_thread_t*)t;

    THREAD_DEBUG("Join initiated for thread %lu \"%s\"\n", thethread->tid, thethread->name);

    ASSERT(thethread->parent == get_cur_thread());

    nk_thread_queue_sleep_extended(thethread->waitq, exit_check, thethread);

    THREAD_DEBUG("Join commenced for thread %lu \"%s\"\n", thethread->tid, thethread->name);

    ASSERT(exit_check(thethread));

    if (retval) {
        *retval = thethread->output;
    }

    thread_detach(thethread);

    THREAD_DEBUG("Join completed for thread %lu \"%s\"\n", thethread->tid, thethread->name);
    
    return 0;
}
    




/* 
 * nk_join_all_children
 *
 * Join all threads that the current thread
 * has either forked or spawned
 *
 * @func: this function will be called with each 
 *        output value generated by this threads
 *        children
 *
 *  returns -EINVAL on error, 0 on success
 *
 */
int 
nk_join_all_children (int (*func)(void * res)) 
{
    nk_thread_t * elm = NULL;
    nk_thread_t * tmp = NULL;
    nk_thread_t * me         = get_cur_thread();
    void * res               = NULL;
    int ret                  = 0;

    list_for_each_entry_safe(elm, tmp, &(me->children), child_node) {

        if (nk_join(elm, &res) < 0) {
            THREAD_ERROR("Could not join child thread (t=%p)\n", elm);
            ret = -1;
            continue;
        }

        if (func) {
            if (func(res) < 0) {
                THREAD_ERROR("Consumer indicated error for child thread (t=%p, output=%p)\n", elm,res);
                ret = -1;
                continue;
            }
        }

    }

    return ret;
}




/* 
 * nk_set_thread_fork_output
 *
 * @result: the output to set
 *
 */
void
nk_set_thread_fork_output (void * result)
{
    nk_thread_t* t = get_cur_thread();
    t->output = result;
}


/*
 * nk_thread_queue_sleep_extended
 *
 * Goes to sleep on the given queue, checking a condition as it does so
 *
 * @q: the thread queue to sleep on
 * @cond_check - condition to check (return nonzero if true) atomically with queuing
 * @state - state for cond_check
 *  
 */
void nk_thread_queue_sleep_extended(nk_thread_queue_t *wq, int (*cond_check)(void *state), void *state)
{
    nk_thread_t * t = get_cur_thread();
    uint8_t flags;

    THREAD_DEBUG("Thread %lu (%s) going to sleep on queue %p\n", t->tid, t->name, (void*)wq);

    // grab control over the the wait queue
    flags = spin_lock_irq_save(&wq->lock);

    // at this point, any waker is either about to start on the
    // queue or has just finished  It's possible that
    // we have raced with with it and it has just finished
    // we therefore need to double check the condition now

    if (cond_check && cond_check(state)) { 
	// The condition we are waiting on has been achieved
	// already.  The waker is either done waking up
	// threads or has not yet started.  In either case
	// we do not want to queue ourselves
	spin_unlock_irq_restore(&wq->lock, flags);
	THREAD_DEBUG("Thread %lu (%s) has fast wakeup on queue %p - condition already met\n", t->tid, t->name, (void*)wq);
	return;
    } else {
	// the condition still is not signalled 
	// or the condition is not important, therefore
	// while still holding the lock, put ourselves on the 
	// wait queue

	THREAD_DEBUG("Thread %lu (%s) is queueing itself on queue %p\n", t->tid, t->name, (void*)wq);
	
	t->status = NK_THR_WAITING;
	nk_enqueue_entry(wq, &(t->wait_node));

	// force arch and compiler to do above writes
	__asm__ __volatile__ ("mfence" : : : "memory"); 

	// disallow the scheduler from context switching this core
	// until we (in particular nk_sched_sleep()) decide otherwise
	preempt_disable(); 

	// reenable local interrupts - the scheduler is still blocked
	// because we have preemption off
	// any waker at this point will still get stuck on the wait queue lock
	// it will be a short spin, hopefully
	irq_enable_restore(flags);
	
	THREAD_DEBUG("Thread %lu (%s) is having the scheduler put itself to sleep on queue %p\n", t->tid, t->name, (void*)wq);

	// We now get the scheduler to do a context switch
	// and just after it completes its scheduling pass, 
	// it will release the wait queue lock for us
	// it will also reenable preemption on its context switch out
	nk_sched_sleep(&wq->lock);
	
	THREAD_DEBUG("Thread %lu (%s) has slow wakeup on queue %p\n", t->tid, t->name, (void*)wq);

	// note no spin_unlock here since nk_sched_sleep will have 
	// done it for us

	return;
    }
}

void nk_thread_queue_sleep(nk_thread_queue_t *wq)
{
    return nk_thread_queue_sleep_extended(wq,0,0);
}


/* 
 * nk_thread_queue_wake_one
 *
 * wake one thread waiting on this queue
 *
 * @q: the queue to use
 *
 */
void nk_thread_queue_wake_one (nk_thread_queue_t * q)
{
    nk_queue_entry_t * elm = NULL;
    nk_thread_t * t = NULL;
    uint8_t flags = irq_disable_save();

    if (in_interrupt_context()) {
	THREAD_DEBUG("[Interrupt Context] Thread %lu (%s) is waking one waiter on thread queue (q=%p)\n", get_cur_thread()->tid, get_cur_thread()->name, (void*)q);
    } else {
	THREAD_DEBUG("Thread %lu (%s) is waking one waiter on thread queue (q=%p)\n", get_cur_thread()->tid, get_cur_thread()->name, (void*)q);
    }

    ASSERT(q);

    elm = nk_dequeue_first_atomic(q);

    /* no one is sleeping on this queue */
    if (!elm) {
        THREAD_DEBUG("No waiters on wait queue\n");
        goto out;
    }

    t = container_of(elm, nk_thread_t, wait_node);

    ASSERT(t);
    ASSERT(t->status == NK_THR_WAITING);

    if (nk_sched_awaken(t, t->current_cpu)) { 
	THREAD_ERROR("Failed to awaken thread\n");
	goto out;
    }

    nk_sched_kick_cpu(t->current_cpu);

    THREAD_DEBUG("Thread queue wake one (q=%p) work up thread %lu (%s)\n", (void*)q, t->tid, t->name);

out:
    irq_enable_restore(flags);

}




/* 
 * nk_thread_queue_wake_all
 *
 * wake all threads waiting on this queue
 *
 * @q: the queue to use
 *
 * returns -EINVAL on error, 0 on success
 *
 */
void nk_thread_queue_wake_all (nk_thread_queue_t * q)
{
    _thread_queue_wake_all(q,0);
}

/* 
 * nk_tls_key_create
 *
 * create thread local storage
 *
 * @key: where to stash the created key
 * @destructor: function pointer to be called when the thread
 *              exits
 *
 * returns -EAGAIN on error, 0 on success
 *
 */
int
nk_tls_key_create (nk_tls_key_t * key, void (*destructor)(void*))
{
    int i;

    for (i = 0; i < TLS_MAX_KEYS; i++) {
        unsigned sn = tls_keys[i].seq_num;

        if (TLS_KEY_AVAIL(sn) && TLS_KEY_USABLE(sn) &&
            atomic_cmpswap(tls_keys[i].seq_num, sn, sn+1) == sn) {

            tls_keys[i].destructor = destructor;
            *key = i;

            /* we're done */
            return 0;
        }
    }

    return -EAGAIN;
}


/*
 * nk_tls_key_delete
 *
 * @key: the key to delete
 *
 * returns -EINVAL on error, 0 on success
 *
 */
int 
nk_tls_key_delete (nk_tls_key_t key)
{
    if (likely(key < TLS_MAX_KEYS)) {
        unsigned sn = tls_keys[key].seq_num;

        if (likely(!TLS_KEY_AVAIL(sn)) &&
            atomic_cmpswap(tls_keys[key].seq_num, sn, sn+1) == sn) {
            return 0;
        }
    }

    return -EINVAL;
}


/* 
 * nk_tls_get
 *
 * get the value stored for this key for this
 * thread
 *
 * @key: the key to act as index for value lookup
 *
 * returns NULL on error, the value otherwise
 *
 */
void*
nk_tls_get (nk_tls_key_t key) 
{
    nk_thread_t * t;

    if (unlikely(key >= TLS_MAX_KEYS)) {
        return NULL;
    }

    t = get_cur_thread();
    return (void*)t->tls[key];
}


/* 
 * nk_tls_set
 *
 * @key: the key to use for index lookup
 * @val: the new value to set at this key
 *
 * returns -EINVAL on error, 0 on success
 *
 */
int 
nk_tls_set (nk_tls_key_t key, const void * val)
{
    nk_thread_t * t;
    unsigned sn;

    if (key >= TLS_MAX_KEYS || 
        TLS_KEY_AVAIL((sn = tls_keys[key].seq_num))) {
        return -EINVAL;
    }

    t = get_cur_thread();
    t->tls[key] = val;
    return 0;
}


/*
 * nk_get_tid
 *
 * get the opaque thread id for the currently
 * running thread
 *
 */
nk_thread_id_t
nk_get_tid (void) 
{
    nk_thread_t * t = (nk_thread_t*)get_cur_thread();
    return (nk_thread_id_t)t;
}


/* 
 * nk_get_parent_tid
 *
 * get this thread's parent tid
 *
 */
nk_thread_id_t
nk_get_parent_tid (void) 
{
    nk_thread_t * t = (nk_thread_t*)get_cur_thread();
    if (t && t->parent) {
        return (nk_thread_id_t)t->parent;
    }
    return NULL;
}





/********** END EXTERNAL INTERFACE **************/





// push the child stack down by this much just in case
// we only have one caller frame to mangle
// the launcher function needs to put a new return address
// prior to the current stack frame, at least.
// should be at least 16
#define LAUNCHPAD 16
// Attempt to clone this many frames when doing a fork
// If these cannot be resolved correctly, then only a single
// frame is cloned
#define STACK_CLONE_DEPTH 2

/* 
 * note that this isn't called directly. It is vectored
 * into from an assembly stub
 *
 * On success, pareant gets child's tid, child gets 0
 */
nk_thread_id_t 
__thread_fork (void)
{
    nk_thread_t *parent = get_cur_thread();
    nk_thread_id_t  tid;
    nk_thread_t * t;
    nk_stack_size_t size, alloc_size;
    uint64_t     rbp1_offset_from_ret0_addr;
    void         *child_stack;
    uint64_t     rsp;

    __asm__ __volatile__ ( "movq %%rsp, %0" : "=r"(rsp) : : "memory");

#ifdef NAUT_CONFIG_ENABLE_STACK_CHECK
    // now check again after update to see if we didn't overrun/underrun the stack in the parent... 
    if ((uint64_t)rsp <= (uint64_t)parent->stack ||
	(uint64_t)rsp >= (uint64_t)(parent->stack + parent->stack_size)) { 
	THREAD_ERROR("Parent's top of stack (%p) exceeds boundaries of stack (%p-%p)\n",
		     rsp, parent->stack, parent->stack+parent->stack_size);
	panic("Detected stack out of bounds in parent during fork\n");
    }
#endif

    THREAD_DEBUG("Forking thread from parent=%p tid=%lu stack=%p-%p rsp=%p\n", parent, parent->tid,parent->stack,parent->stack+parent->stack_size,rsp);

#ifdef NAUT_CONFIG_THREAD_OPTIMIZE 
    THREAD_WARN("Thread fork may function incorrectly with aggressive threading optimizations\n");
#endif

    void *rbp0      = __builtin_frame_address(0);                   // current rbp, *rbp0 = rbp1
    void *rbp1      = __builtin_frame_address(1);                   // caller rbp, *rbp1 = rbp2  (forker's frame)
    void *rbp_tos   = __builtin_frame_address(STACK_CLONE_DEPTH);   // should scan backward to avoid having this be zero or crazy
    void *ret0_addr = rbp0 + 8;

    // we're being called with a stack not as deep as STACK_CLONE_DEPTH...
    // fail back to a single frame...
    if ((uint64_t)rbp_tos <= (uint64_t)parent->stack ||
	(uint64_t)rbp_tos >= (uint64_t)(parent->stack + parent->stack_size)) { 
	THREAD_DEBUG("Cannot resolve %lu stack frames on fork, using just one\n", STACK_CLONE_DEPTH);
        rbp_tos = rbp1;
    }


    // from last byte of tos_rbp to the last byte of the stack on return from this function 
    // (return address of wrapper)
    // the "launch pad" is added so that in the case where there is no stack frame above the caller
    // we still have the space to fake one.
    size = (rbp_tos + 8) - ret0_addr + LAUNCHPAD;   

    //THREAD_DEBUG("rbp0=%p rbp1=%p rbp_tos=%p, ret0_addr=%p\n", rbp0, rbp1, rbp_tos, ret0_addr);

    rbp1_offset_from_ret0_addr = rbp1 - ret0_addr;

    alloc_size = parent->stack_size;

    if (nk_thread_create(NULL,        // no function pointer, we'll set rip explicity in just a sec...
                         NULL,        // no input args, it's not a function
                         NULL,        // no output args
                         0,           // this thread's parent will wait on it
                         alloc_size,  // stack size
                         &tid,        // give me a thread id
                         CPU_ANY)     // not bound to any particular CPU
            < 0) {
        THREAD_ERROR("Could not fork thread\n");
        return 0;
    }

    t = (nk_thread_t*)tid;

    THREAD_DEBUG("Forked thread created: %p (tid=%lu) stack=%p size=%lu rsp=%p\n",t,t->tid,t->stack,t->stack_size,t->rsp);

    child_stack = t->stack;

    // this is at the top of the stack, just in case something goes wrong
    thread_push(t, (uint64_t)&thread_cleanup);

    //THREAD_DEBUG("child_stack=%p, alloc_size=%lu size=%lu\n",child_stack,alloc_size,size);
    //THREAD_DEBUG("copy to %p-%p from %p\n", child_stack + alloc_size - size,
    //             child_stack + alloc_size - size + size - LAUNCHPAD, ret0_addr);

    // Copy stack frames of caller and up to stack max
    // this should copy from 1st byte of my_rbp to last byte of tos_rbp
    // notice that leaves ret
    memcpy(child_stack + alloc_size - size, ret0_addr, size - LAUNCHPAD);
    t->rsp = (uint64_t)(child_stack + alloc_size - size);

    void **rbp2_ptr = (void**)(t->rsp + rbp1_offset_from_ret0_addr);
    void **ret2_ptr = rbp2_ptr+1;

    // rbp2 we don't care about
    *rbp2_ptr = 0x0ULL;

    // fix up the return address to point to our thread cleanup function
    *ret2_ptr = &thread_cleanup;

    // now we need to setup the interrupt stack etc.
    // we provide null for thread func to indicate this is a fork
    thread_setup_init_stack(t, NULL, NULL); 

    THREAD_DEBUG("Forked thread initialized: %p (tid=%lu) stack=%p size=%lu rsp=%p\n",t,t->tid,t->stack,t->stack_size,t->rsp);

#ifdef NAUT_CONFIG_ENABLE_STACK_CHECK
    // now check the child before we attempt to run it
    if ((uint64_t)t->rsp <= (uint64_t)t->stack ||
	(uint64_t)t->rsp >= (uint64_t)(t->stack + t->stack_size)) { 
	THREAD_ERROR("Child's rsp (%p) exceeds boundaries of stack (%p-%p)\n",
		     t->rsp, t->stack, t->stack+t->stack_size);
	panic("Detected stack out of bounds in child during fork\n");
	return 0;
    }
#endif

#ifdef NAUT_CONFIG_FPU_SAVE
    // clone the floating point state
    extern void nk_fp_save(void *dest);
    nk_fp_save(t->fpu_state);
#endif

    if (nk_sched_make_runnable(t,t->current_cpu,1)) { 
	THREAD_ERROR("Scheduler failed to run thread (%p, tid=%u) on cpu %u\n",
		    t, t->tid, t->current_cpu);
	return 0;
    }

    THREAD_DEBUG("Forked thread made runnable: %p (tid=%lu)\n",t,t->tid);

    // return child's tid to parent
    return tid;
}






static void 
tls_dummy (void * in, void ** out)
{
    unsigned i;
    nk_tls_key_t * keys = NULL;

    //THREAD_INFO("Beginning test of thread local storage...\n");
    keys = malloc(sizeof(nk_tls_key_t)*TLS_MAX_KEYS);
    if (!keys) {
        THREAD_ERROR("could not allocate keys\n");
        return;
    }

    for (i = 0; i < TLS_MAX_KEYS; i++) {
        if (nk_tls_key_create(&keys[i], NULL) != 0) {
            THREAD_ERROR("Could not create TLS key (%u)\n", i);
            goto out_err;
        }

        if (nk_tls_set(keys[i], (const void *)(i + 100L)) != 0) {
            THREAD_ERROR("Could not set TLS key (%u)\n", i);
            goto out_err;
        }

    }

    for (i = 0; i < TLS_MAX_KEYS; i++) {
        if (nk_tls_get(keys[i]) != (void*)(i + 100L)) {
            THREAD_ERROR("Mismatched TLS val! Got %p, should be %p\n", nk_tls_get(keys[i]), (void*)(i+100L));
            goto out_err;
        }

        if (nk_tls_key_delete(keys[i]) != 0) {
            THREAD_ERROR("Could not delete TLS key %u\n", i);
            goto out_err;
        }
    }

    if (nk_tls_key_create(&keys[0], NULL) != 0) {
        THREAD_ERROR("2nd key create failed\n");
        goto out_err;
    }
    
    if (nk_tls_key_delete(keys[0]) != 0) {
        THREAD_ERROR("2nd key delete failed\n");
        goto out_err;
    }

    THREAD_INFO("Thread local storage test succeeded\n");

out_err:
    free(keys);
}


void 
nk_tls_test (void)
{
    nk_thread_start(tls_dummy, NULL, NULL, 1, TSTACK_DEFAULT, NULL, 1);
}




