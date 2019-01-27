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
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>
#include <nautilus/scheduler.h>
#include <nautilus/smp.h>
#include <rt/openmp/gomp/gomp.h>


#ifndef NAUT_CONFIG_OPENMP_RT_DEBUG
#define DEBUG(fmt, args...)
#else
#define DEBUG(fmt, args...) DEBUG_PRINT("gomp: " fmt, ##args)
#endif

#define ERROR(fmt, args...) ERROR_PRINT("gomp: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("gomp: " fmt, ##args)


// this is hanging off the "input" argument
// for a nautilus thread and it supplies the metadata
// that would be usually kept in TLS in a pthread implementation
struct omp_thread
{
#define OMP_COOKIE 0xf0d0f0d01234abcdULL
    uint64_t cookie;   // this is disgusting...
    int      team;
    int      max_threads_in_team; // 0 => numprocs
    int      num_threads_in_team;
    int      thread_num_in_team;
    int      level;
    int      num_threads_in_level;
    void     (*f)(void *); // func;
    void     *in;
    int      thread_num;
    void     *cur_single;
    struct omp_thread *team_leader;
    nk_counting_barrier_t team_barrier;
    struct nk_thread  *thread;
};


// nesting level for the active parallel blocks, 
// which enclose the calling call
int omp_get_active_level()
{
    struct omp_thread *o = (struct omp_thread *) (get_cur_thread()->input);

    DEBUG("omp_get_active_level()=%d\n", o->level);
    return o->level;
}

// This function returns the thread identification number for the
// given nesting level of the current thread. For values of level
// outside zero to omp_get_level -1 is returned; if level is
// omp_get_level the result is identical to omp_get_thread_num.
int omp_get_ancestor_thread_num(int level)
{
    struct omp_thread *o = (struct omp_thread *) (get_cur_thread()->parent->input);
    
    DEBUG("omp_get_ancestor_thread_num()=%d\n", o->thread_num);
    return o->thread_num;
}

// This function returns true if cancellation is activated, false
// otherwise. Here, true and false represent their language-specific
// counterparts. Unless OMP_CANCELLATION is set true, cancellations
// are deactivated.
int omp_get_cancellation()
{
    DEBUG("omp_get_cancellation()=0 UNIMPLEMENTED\n");
    return 0;
}

// Get the default device for target regions without device clause.
int omp_get_default_device()
{
    DEBUG("omp_get_default_device()=0 UNIMPLEMENTED\n");
    return 0;
}

//This function returns true if enabled, false otherwise. Here, true
//and false represent their language-specific counterparts.
//
//The dynamic team setting may be initialized at startup by the
//OMP_DYNAMIC environment variable or at runtime using
//omp_set_dynamic. If undefined, dynamic adjustment is disabled by
//default.
int omp_get_dynamic()
{
    DEBUG("omp_get_dynamic()=0 UNIMPLEMENTED\n");
    return 0;
}




//This function returns the nesting level for the parallel blocks,
//which enclose the calling call.
int omp_get_level(void)
{
    struct omp_thread *o = (struct omp_thread *) (get_cur_thread()->input);
    DEBUG("omp_get_level()=%d\n",o->level);
    return o->level;
}

//This function obtains the maximum allowed number of nested, active
//parallel regions.
int omp_get_max_active_levels(void)
{
    DEBUG("omp_get_max_active_levels()=%d\n", nk_get_num_cpus());
    return nk_get_num_cpus();
}

//This function obtains the maximum allowed priority number for tasks.
int omp_get_max_task_priority(void)
{
    DEBUG("omp_get_max_task_priority()=0 UNIMPLEMENTED\n");
    return 0;
}

// Return the maximum number of threads used for the current parallel
// region that does not use the clause num_threads.
int omp_get_max_threads(void)
{
    DEBUG("omp_get_max_threads()=%d UNIMPLEMENTED\n", NAUT_CONFIG_MAX_THREADS);
    return NAUT_CONFIG_MAX_THREADS;
}

// This function returns true if nested parallel regions are enabled,
// false otherwise. Here, true and false represent their
// language-specific counterparts.
//
//    Nested parallel regions may be initialized at startup by the
//    OMP_NESTED environment variable or at runtime using
//    omp_set_nested. If undefined, nested parallel regions are
//    disabled by default.
int omp_get_nested(void)
{
    DEBUG("omp_get_nested()=1\n");
    return 1;
}

//    Returns the number of target devices.
//
// ?? is the processor complex a device?
int omp_get_num_devices(void)
{
    DEBUG("omp_get_num_devices()=1 => assuming processor complex is device\n");
    return 1;
}

// Returns the number of processors online on that device.
//
// WHAT DEVICE - it does not take an argument...
// 
// we will give them all of the processors, but we should
// probably not give them zero, and probably not give them any hw
// thread that interacts with it...
int omp_get_num_procs(void)
{
    DEBUG("omp_get_num_procs()=%d GUESS\n", nk_get_num_cpus());
    return nk_get_num_cpus();
}


//   Returns the number of teams in the current team region.
//
// WTF is a team and how shall we map it?
//
// Looks like this just means the number of leafs in the current
// parallel recursion... ? 
//
int omp_get_num_teams(void)
{
    DEBUG("omp_get_num_teams()=1 GUESS\n");
    return 1;
}

//    Returns the number of threads in the current team. In a
//    sequential section of the program omp_get_num_threads returns 1.
//
//    The default team size may be initialized at startup by the
//    OMP_NUM_THREADS environment variable. At runtime, the size of
//    the current team may be set either by the NUM_THREADS clause or
//    by omp_set_num_threads. If none of the above were used to define
//    a specific value and OMP_DYNAMIC is disabled, one thread per CPU
//    online is used.
//
// the idea here is presumably that a team is a thread pool and this
// is asking for the size of the thread pool.   That's unfortunate
// since we can easily do cheap thread launches - this is quite different
// from what we could do in the ndpc model
int omp_get_num_threads(void)
{
    struct omp_thread *o = (struct omp_thread *) (get_cur_thread()->input);

    DEBUG("omp_get_num_threads()=%d\n",!o ? 1 : o->num_threads_in_team); 
    return !o ? 1 : o->num_threads_in_team;
}

// This functions returns the currently active thread affinity policy,
// which is set via OMP_PROC_BIND. Possible values are
// omp_proc_bind_false, omp_proc_bind_true, omp_proc_bind_master,
// omp_proc_bind_close and omp_proc_bind_spread.
omp_proc_bind_t omp_get_proc_bind(void)
{
    DEBUG("omp_get_proc_bind()=false - do not allow thread migration yet\n");
    return 0;
}


//  Obtain the runtime scheduling method. The kind argument will be
//  set to the value omp_sched_static, omp_sched_dynamic,
//  omp_sched_guided or omp_sched_auto. The second argument,
//  chunk_size, is set to the chunk size.
void omp_get_schedule(omp_sched_t *kind, int *chunk_size)
{
    DEBUG("comp_get_schedule()=sched_static?, chunk_size=1?\n");
    *kind = 0;
    *chunk_size = 1 ; //?
}

// Returns the team number of the calling thread.
int omp_get_team_num(void)
{
    struct omp_thread *o = (struct omp_thread *) (get_cur_thread()->input);

    DEBUG("omp_get_team_num()=%d\n", 
	  !o ? 0 : o->team);
    return !o ? 0 : o->team;
}

// This function returns the number of threads in a thread team to
// which either the current thread or its ancestor belongs. For values
// of level outside zero to omp_get_level, -1 is returned; if level is
// zero, 1 is returned, and for omp_get_level, the result is identical
// to omp_get_num_threads.
int omp_get_team_size(int level)
{
    struct omp_thread *o = (struct omp_thread *) (get_cur_thread()->input);
    DEBUG("omp_get_team_size(%d) = %d GUESS\n", !o ? 1 : o->num_threads_in_team);
    return !o ? 1 : o->num_threads_in_team;
}

// Return the maximum number of threads of the program.
int omp_get_thread_limit(void)
{
    DEBUG("omp_get_thread_limit() = %d max threads in kernel GUESS\n", NAUT_CONFIG_MAX_THREADS);
    return NAUT_CONFIG_MAX_THREADS;
}

// Returns a unique thread identification number within the current
// team. In a sequential parts of the program, omp_get_thread_num
// always returns 0. In parallel regions the return value varies from
// 0 to omp_get_num_threads-1 inclusive. The return value of the
// master thread of a team is always 0.
int omp_get_thread_num(void)
{
    struct omp_thread *o = (struct omp_thread *) (get_cur_thread()->input);
    DEBUG("omp_get_threadnum()=%d (within team)\n",!o ? 0 : o->thread_num);
    return !o ? 0 : o->thread_num;
}

//  This function returns true if currently running in parallel, false
//  otherwise. Here, true and false represent their language-specific
//  counterparts.
int omp_in_parallel(void)
{
    DEBUG("omp_in_parallel()=0 UNIMPLEMENTED\n");
    return 0;
}


// This function returns true if currently running in a final or
// included task region, false otherwise. Here, true and false
// represent their language-specific counterparts.
//
// what is the "final" abstraction here?   
int omp_in_final(void)
{
    DEBUG("omp_in_final()=0 UNIMPLMENTED\n");
    return 0;
}

// This function returns true if currently running on the host device,
// false otherwise. Here, true and false represent their
// language-specific counterparts.
//
// OK, so the cpu complex is a device, device zero, presumably
int omp_is_initial_device(void)
{
    DEBUG("omp_is_initial_device()=1 GUESS - only have host device\n");
    return 1;
}

// Get the default device for target regions without device
// clause. The argument shall be a nonnegative device number.
// 
// Seriously?  This is their interface?  A global context?
// Per team?  huh?
//
void omp_set_default_device(int device_num)
{
    DEBUG("omp_set_default_device(%d) => GUESS\n", device_num);
}

// Enable or disable the dynamic adjustment of the number of threads
// within a team. The function takes the language-specific equivalent
// of true and false, where true enables dynamic adjustment of team
// sizes and false disables it.
void omp_set_dynamic(int dynamic_threads)
{
    DEBUG("omp_set_dynamic(%d) => IGNORED FOR NOW\n",dynamic_threads);
}


// This function limits the maximum allowed number of nested, active
// parallel regions.
void omp_set_max_active_levels(int max_levels)
{
    DEBUG("omp_set_max_active_levels(%d) => IGNORED FOR NOW\n", max_levels);
}

// Enable or disable nested parallel regions, i.e., whether team
// members are allowed to create new teams. The function takes the
// language-specific equivalent of true and false, where true enables
// dynamic adjustment of team sizes and false disables it.
//
// Let this be nested parallelism
void omp_set_nested(int nested)
{
    DEBUG("omp_set_nested(%d) => IGNORED FOR NOW\n", nested);
}

// Specifies the number of threads used by default in subsequent
// parallel sections, if those do not specify a num_threads
// clause. The argument of omp_set_num_threads shall be a positive
// integer.
//
// this again implies a program-level context...
void omp_set_num_threads(int num_threads)
{
    struct nk_thread *t = get_cur_thread();

    struct omp_thread *o = (struct omp_thread *)t->input;

    o->max_threads_in_team = num_threads;
    
    DEBUG("omp_set_num_threads(%d)\n", num_threads);
}

// Sets the runtime scheduling method. The kind argument can have the
// value omp_sched_static, omp_sched_dynamic, omp_sched_guided or
// omp_sched_auto. Except for omp_sched_auto, the chunk size is set to
// the value of chunk_size if positive, or to the default value if
// zero or negative. For omp_sched_auto the chunk_size argument is
// ignored.
void omp_set_schedule(omp_sched_t kind, int chunk_size)
{
    DEBUG("omp_set_schedule(kind=%d, chunk_size=%d) => IGNORED FOR NOW\n", kind, chunk_size);
}


// Initialize a simple lock. After initialization, the lock is in an
// unlocked state.
void omp_init_lock(omp_lock_t *lock)
{
    spinlock_t *s = malloc(sizeof(spinlock_t));
    if (!s) { 
	ERROR("Failed to allocate OMP lock\n");
	*lock = 0;
    } else {
	spinlock_init(s);
	DEBUG("omp_init_lock()-> %p\n",s);
	*lock = s;
    }
}


//   Before setting a simple lock, the lock variable must be
//   initialized by omp_init_lock. The calling thread is blocked until
//   the lock is available. If the lock is already held by the current
//   thread, a deadlock occurs.
void omp_set_lock(omp_lock_t *lock)
{
    DEBUG("omp_set_lock(%p)\n");
    spin_lock(*lock);
    DEBUG("omp_set_lock(%p) - lock acquired\n");
}

// Before setting a simple lock, the lock variable must be initialized
// by omp_init_lock. Contrary to omp_set_lock, omp_test_lock does not
// block if the lock is not available. This function returns true upon
// success, false otherwise. Here, true and false represent their
// language-specific counterparts.
//
int omp_test_lock(omp_lock_t *lock)
{
    int rc = spin_try_lock(*lock) == 0;
    
    DEBUG("omp_test_lock(%p) => %d\n", lock, rc);
    return rc;
}

// A simple lock about to be unset must have been locked by
// omp_set_lock or omp_test_lock before. In addition, the lock must be
// held by the thread calling omp_unset_lock. Then, the lock becomes
// unlocked. If one or more threads attempted to set the lock before,
// one of them is chosen to, again, set the lock to itself.
void omp_unset_lock(omp_lock_t *lock)
{
    DEBUG("omp_unset_lock(%p)\n", lock);
    spin_unlock(*lock);
}

// Destroy a simple lock. In order to be destroyed, a simple lock must
// be in the unlocked state.
void omp_destroy_lock(omp_lock_t *lock)
{
    DEBUG("omp_destroy_lock(%p)\n");
    spinlock_deinit(*lock);
    free(*lock);
}

//    Initialize a nested lock. After initialization, the lock is in
//    an unlocked state and the nesting count is set to zero.
void omp_init_nest_lock(omp_nest_lock_t *lock)
{
    DEBUG("omp_init_nest_lock(%p) => UNIMPLEMENTED - doing regular init\n", lock);
    omp_init_lock(lock);
}

// Before setting a nested lock, the lock variable must be initialized
// by omp_init_nest_lock. The calling thread is blocked until the lock
// is available. If the lock is already held by the current thread,
// the nesting count for the lock is incremented.
void omp_set_nest_lock(omp_nest_lock_t *lock)
{
    DEBUG("omp_set_nest_lock(%p) => UNIMPLEMENTED - doing regular set\n", lock);
    omp_set_lock(lock);
}

// Before setting a nested lock, the lock variable must be initialized
// by omp_init_nest_lock. Contrary to omp_set_nest_lock,
// omp_test_nest_lock does not block if the lock is not available. If
// the lock is already held by the current thread, the new nesting
// count is returned. Otherwise, the return value equals zero.
int omp_test_nest_lock(omp_nest_lock_t *lock)
{
    DEBUG("omp_test_nest_lock(%p) => UNIMPLEMENTED - doing regular test\n", lock);
    return omp_test_lock(lock);
}

// A nested lock about to be unset must have been locked by
// omp_set_nested_lock or omp_test_nested_lock before. In addition,
// the lock must be held by the thread calling
// omp_unset_nested_lock. If the nesting count drops to zero, the lock
// becomes unlocked. If one ore more threads attempted to set the lock
// before, one of them is chosen to, again, set the lock to itself.
void omp_unset_nest_lock(omp_nest_lock_t *lock)
{
    DEBUG("omp_unset_nest_lock(%p) => UNIMPLEMENTED - doing regular unset\n", lock);
    omp_unset_lock(lock);
}

// Destroy a nested lock. In order to be destroyed, a nested lock must
// be in the unlocked state and its nesting count must equal zero.
void omp_destroy_nest_lock(omp_nest_lock_t *lock)
{
    DEBUG("omp_destroy_nest_lock(%p) => UNIMPLEMENTED - doing regular destroy\n", lock);
    omp_destroy_lock(lock);
}

// Gets the timer precision, i.e., the number of seconds between two
// successive clock ticks.
// nanosecond resolution 
double omp_get_wtick(void)
{
    DEBUG("omp_get_wtick()=%lf\n",1e-9);
    return 1e-9;
}

// Elapsed wall clock time in seconds. The time is measured per
// thread, no guarantee can be made that two distinct threads measure
// the same time. Time is measured from some "time in the past", which
// is an arbitrary time guaranteed not to change during the execution
// of the program.
double omp_get_wtime(void)
{
    uint64_t ns = nk_sched_get_realtime();
    DEBUG("omp_get_wtime() = scheduler time %lu ns (%lf s)\n", ns, (double)ns/1e9);
    return (double)ns/1e9;
}
   
// The mess here is in the GOMP_ support routines that are called by
// the emitted code.   These are GOMP-specific, and version-specific.  
// the gcc omp transform seems to basically turn every omp construct
// into code that uses these GOMP routines as helpers, and most of the 
// action seems to be in the helpers.   Perhaps the Intel and LLVM
// openmp stuff operates differently.
//
// The actual GOMP interface seems pretty unclear - 
// the GOMP manual is not too clear about internals, but does cover
// the notion of roles, which is perhaps the same thing.  
// 
// Interesting thing for a different compiler
//
// https://github.com/rose-compiler/rose-develop/tree/master/src/midend/programTransformation/ompLowering
//

static void parallel_start_wrapper(void *in, void **out)
{
    struct omp_thread *o = (struct omp_thread *)in;
    char buf[32];

    o->thread = get_cur_thread();
    
    o->thread->vc = o->thread->parent->vc;

    snprintf(buf,32,"omp-%d-%d-%d",o->team,o->level,o->thread_num_in_team);

    nk_thread_name(o->thread,buf);
    

    DEBUG("Launch - thread cookie=%lx, team=%d, num_threads_in_team=%d, thread_num_in_team=%d, level=%d, num_threads_in_level=%d,f=%p, in=%p, thread_num=%d, thread=%p\n", o->cookie, o->team, o->num_threads_in_team, o->thread_num_in_team, o->level, o->num_threads_in_level, o->f, o->in, o->thread_num, o->thread);

    o->f(o->in);

    DEBUG("Finish - thread cookie=%lx, team=%d, num_threads_in_team=%d, thread_num_in_team=%d, level=%d, num_threads_in_level=%d,f=%p, in=%p, thread_num=%d, thread=%p\n", o->cookie, o->team, o->num_threads_in_team, o->thread_num_in_team, o->level, o->num_threads_in_level, o->f, o->in, o->thread_num, o->thread);

    free(o);
}

void GOMP_parallel_start(void (*f)(void*), void *d, unsigned numthreads)
{
    DEBUG("GOMP_parallel_start(f=%p,d=%p,numthreads=%u)\n", f, d, numthreads);

    struct omp_thread *p = (struct omp_thread*)(get_cur_thread()->input);

    
    if (!p || (p->cookie != OMP_COOKIE)) {
	ERROR("GOMP_parallel_start() from thread that is not an OMP thread\n");
	return;
    }

    unsigned i;

    if (!numthreads) { 
	if (p->max_threads_in_team) {
	    numthreads = p->max_threads_in_team;
	} else {
	    numthreads = nk_get_num_cpus();
	}
    }

    // configure myself

    p->num_threads_in_team = numthreads;
    p->num_threads_in_level = numthreads; // wrong
    p->thread_num_in_team = 0; // wrong
    p->thread_num = 0; //wrong?
    p->team_leader = p;

    nk_counting_barrier_init(&p->team_barrier,p->num_threads_in_team);


    for (i=1;i<numthreads;i++) { 
	struct omp_thread *c = (struct omp_thread *) malloc(sizeof(*c));
	if (!c) { 
	    ERROR("Failed to allocate block - running as function\n");
	    f(d);
	} else {
	    memset(c,0,sizeof(*c));
	    c->cookie=OMP_COOKIE;
	    c->team = p->team;
	    c->level = p->level+1;
	    c->num_threads_in_team = numthreads; // wrong
	    c->num_threads_in_level = numthreads; // wrong
	    c->thread_num_in_team = i; // wrong
	    c->thread_num = i; // wrong
	    c->f=f;
	    c->in=d;
	    c->team_leader = p;
	    DEBUG("thread %d: cookie=%lx, team=%d, num_threads_in_team=%d, thread_num_in_team=%d, level=%d, num_threads_in_level=%d,f=%p, in=%p, thread_num=%d, thread=%p\n", i, c->cookie, c->team, c->num_threads_in_team, c->thread_num_in_team, c->level, c->num_threads_in_level, c->f, c->in, c->thread_num, c->thread);
	    if (nk_thread_start(parallel_start_wrapper,
				c,0,0,TSTACK_DEFAULT,0,-1)) {
		DEBUG("failed to launch as thread, running as function\n");
		free(c);
		f(d);
	    }
	}
    }
}

void GOMP_parallel_end()
{
    DEBUG("GOMP_parallel_end()\n");
    nk_join_all_children(0);
    DEBUG("GOMP_parallel_end() complete\n");
}


// this is "any" - we will make it simply be the first thread in the team
int GOMP_single_start()
{
    struct omp_thread *o = (struct omp_thread*)(get_cur_thread()->input);

    if (o->thread_num_in_team==0) { 
	DEBUG("GOMP_single_start() => 1 (first thread in team)\n");
	return 1;
    } else {
	DEBUG("GOMP_single_start() => 0 (not first thread in team)\n");
	return 0;
    }
}

void GOMP_parallel(void (*f)(void*), void *d, unsigned numthreads, unsigned flags)
{
    DEBUG("GOMP_parallel(f=%p, d=%p, numthreads=%u, flags=%x)\n",
	  f,d,numthreads,flags);
    GOMP_parallel_start(f,d,numthreads);
    f(d);
    GOMP_parallel_end();
}


// this is "any" - we will make it simply be the first thread in the team
// other threads wait
void *GOMP_single_copy_start()
{
    struct omp_thread *o = (struct omp_thread*)(get_cur_thread()->input);

    if (o->thread_num_in_team==0) { 
	o->cur_single=0;
	DEBUG("GOMP_single_copy_start() => 0 (first thread in team)\n");
	return 0;
    } else {
	DEBUG("GOMP_single_copy_start() [Waiting]\n");
        nk_counting_barrier(&o->team_leader->team_barrier);
	DEBUG("GOMP_single_copy_start() [Done]\n");
        return o->team_leader->cur_single;
    }
}

void GOMP_single_copy_end(void *data)
{
    struct omp_thread *o = (struct omp_thread*)(get_cur_thread()->input);
    DEBUG("GOMP_single_copy_end(%p) (start)\n",data);
    o->cur_single = data;
    nk_counting_barrier(&o->team_barrier);
    DEBUG("GOMP_single_copy_end(%p) (end)\n",data);
}
    
void GOMP_barrier()
{ 
    struct omp_thread *o = (struct omp_thread*)(get_cur_thread()->input);
    DEBUG("GOMP_barrier (start)\n");
    nk_counting_barrier(&o->team_leader->team_barrier);
    DEBUG("GOMP_barrier (end)\n");
}


static spinlock_t gomp_global_lock=0;

void GOMP_critical_start(void)
{
    DEBUG("GOMP_critical_start (start)\n");
    spin_lock(&gomp_global_lock);
    DEBUG("GOMP_critical_start (end)\n");
}

void GOMP_critical_end(void)
{
    DEBUG("GOMP_critical_end\n");
    spin_unlock(&gomp_global_lock);
}


/*
SYNCBENCH

/home/pdinda/test/nautilus/src/test/ompbench/syncbench.c:212: undefined reference to `GOMP_loop_ordered_static_start'
/home/pdinda/test/nautilus/src/test/ompbench/syncbench.c:212: undefined reference to `GOMP_ordered_start'
/home/pdinda/test/nautilus/src/test/ompbench/syncbench.c:212: undefined reference to `GOMP_ordered_end'
/home/pdinda/test/nautilus/src/test/ompbench/syncbench.c:214: undefined reference to `GOMP_loop_ordered_static_next'
/home/pdinda/test/nautilus/src/test/ompbench/syncbench.c:214: undefined reference to `GOMP_loop_end_nowait'
make: *** [nautilus.bin] Error 1
*/

/*
   TASKBENCH
src/built-in.o: In function `testParallelTaskGeneration._omp_fn.0':
/home/pdinda/test/nautilus/src/test/ompbench/taskbench.c:116: undefined reference to `GOMP_task'
ence to `GOMP_taskwait'
*/


void GOMP_task (void (*fn) (void *), 
		void *data, 
		void (*cpyfn) (void *, void *),
		long arg_size, 
		long arg_align, 
		int if_clause,
		unsigned flags,
		void **depend, 
		int priority)
{
    struct omp_thread *p = (struct omp_thread*)(get_cur_thread()->input);
    DEBUG("GOMP_task(fn=%p, data=%p, cpy=%p, arg_size=%ld arg_align=%ld if=%d flags=0x%x depend=%p priority=%d\n",
	  fn, data, cpyfn, arg_size, arg_align, if_clause, flags, depend, priority);

    struct omp_thread *c = (struct omp_thread *) malloc(sizeof(*c));
    if (!c) { 
	ERROR("Failed to allocate block\n");
    } else {
	memset(c,0,sizeof(*c));
	
	c->cookie=OMP_COOKIE;
	c->team = p->team;
	c->level = p->level+1;
	c->num_threads_in_team = 1; // wrong
	c->num_threads_in_level = 1; // wrong
	c->thread_num_in_team = 0; // wrong
	c->thread_num = 0; // wrong
	c->f=fn;
	c->in=data;
	c->team_leader = p;
	
	DEBUG("thread: cookie=%lx, team=%d, num_threads_in_team=%d, thread_num_in_team=%d, level=%d, num_threads_in_level=%d,f=%p, in=%p, thread_num=%d, thread=%p\n", c->cookie, c->team, c->num_threads_in_team, c->thread_num_in_team, c->level, c->num_threads_in_level, c->f, c->in, c->thread_num, c->thread);
	if (nk_thread_start(parallel_start_wrapper,
			    c,0,0,TSTACK_DEFAULT,0,-1)) {
	    ERROR("Failed to start thread for task, running as function\n");
	    fn(data);
	}
    }
}

void GOMP_taskwait()
{
    DEBUG("GOMP_taskwait() [begin]\n");
    nk_join_all_children(0);
    DEBUG("GOMP_taskwait() [end]\n");
}

/*
  SCHEDBENCH
src/built-in.o: In function `testdynamicn._omp_fn.2':
/home/pdinda/test/nautilus/src/test/ompbench/schedbench.c:118: undefined reference to `GOMP_loop_dynamic_next'
/home/pdinda/test/nautilus/src/test/ompbench/schedbench.c:118: undefined reference to `GOMP_loop_end'
/home/pdinda/test/nautilus/src/test/ompbench/schedbench.c:122: undefined reference to `GOMP_loop_dynamic_start'
src/built-in.o: In function `testguidedn._omp_fn.3':
/home/pdinda/test/nautilus/src/test/ompbench/schedbench.c:131: undefined reference to `GOMP_loop_guided_next'
/home/pdinda/test/nautilus/src/test/ompbench/schedbench.c:131: undefined reference to `GOMP_loop_end'
/home/pdinda/test/nautilus/src/test/ompbench/schedbench.c:135: undefined reference to `GOMP_loop_guided_start'
make: *** [nautilus.bin] Error 1
*/


int nk_openmp_thread_init()
{
    struct nk_thread *t = get_cur_thread();
    
    DEBUG("nk_openmp_thread_init() of thread %p\n", t);

    if (t->input) { 
	DEBUG("Overriding current thread input...\n");
    }

    struct omp_thread *o = (struct omp_thread *)malloc(sizeof(*o));

    if (!o) { 
	ERROR("Cannot allocate space\n");
	return -1;
    }

    memset(o,0,sizeof(*o));

    o->cookie=OMP_COOKIE;

    o->team = 0;
    o->level = 0;
    o->num_threads_in_team = 1;
    o->num_threads_in_level = 1;
    o->thread_num_in_team = 0; 
    o->thread_num = 0;
    o->f=0;
    o->in=t->input; // stash

    t->input = o;

    o->team_leader = o;

    nk_counting_barrier_init(&o->team_barrier,1);

    DEBUG("nk_openmp_init(): cookie=%lx, team=%d, num_threads_in_team=%d, thread_num_in_team=%d, level=%d, num_threads_in_level=%d,f=%p, in=%p, thread_num=%d, thread=%p\n", o->cookie, o->team, o->num_threads_in_team, o->thread_num_in_team, o->level, o->num_threads_in_level, o->f, o->in, o->thread_num, o->thread);


    return 0;

}


int nk_openmp_thread_deinit()
{
    struct nk_thread *t = get_cur_thread();
    
    DEBUG("nk_openmp_thread_deinit() of thread %p\n", t);

    struct omp_thread *o = (struct omp_thread *)t->input;

    t->input = o->in; // restore

    free(o);

    return 0;
}


int nk_openmp_init()
{
    INFO("init\n");
    return 0;
}

void nk_openmp_deinit()
{
    INFO("deinit\n");
}
