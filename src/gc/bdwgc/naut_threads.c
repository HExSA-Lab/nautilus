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
 * Copyright (c) 2017, Matt George <11georgem@gmail.com>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Matt George <11georgem@gmail.com>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

// This is a port of the Boehm garbage collector to
// the Nautilus kernel base on code copyrighted as follows

/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
 * Copyright (c) 2000-2005 by Hewlett-Packard Company.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

struct nk_thread;
struct nk_thread_constraints;
#include <nautilus/list.h>
#include <nautilus/thread.h>
#include <nautilus/scheduler.h>
#include <gc/bdwgc/bdwgc.h>
#include "bdwgc_internal.h"
#include "private/pthread_support.h"


# include "gc_inline.h"

STATIC long GC_nprocs = 1;
GC_INNER GC_bool GC_need_to_lock = FALSE;
GC_INNER GC_bool GC_in_thread_creation = FALSE;
GC_INNER unsigned long GC_lock_holder = NO_THREAD;
GC_INNER GC_bool GC_thr_initialized = FALSE;
GC_INNER NK_LOCK_T GC_allocate_ml; //PTHREAD_MUTEX_INITIALIZER;


/* Return the number of processors. */
STATIC int GC_get_nprocs(void)
{
  BDWGC_DEBUG("(WARNING: UNIMPLEMENTED!) GC_get_npos: Returning number of proccesors as %d\n", 2);
  return 2;
}



/* Called by GC_finalize() (in case of an allocation failure observed). */
GC_INNER void GC_reset_finalizer_nested(void)
{
  bdwgc_thread_state* me = BDWGC_THREAD_STATE();
  me->finalizer_nested = 0;
}


/* Checks and updates the thread-local level of finalizers recursion.   */
/* Returns NULL if GC_invoke_finalizers() should not be called by the   */
/* collector (to minimize the risk of a deep finalizers recursion),     */
/* otherwise returns a pointer to the thread-local finalizer_nested.    */
/* Called by GC_notify_or_invoke_finalizers() only (the lock is held).  */
GC_INNER unsigned char *GC_check_finalizer_nested(void)
{
  bdwgc_thread_state* me = BDWGC_THREAD_STATE(); 
  unsigned nesting_level = me->finalizer_nested;
  if (nesting_level) {
    /* We are inside another GC_invoke_finalizers().            */
    /* Skip some implicitly-called GC_invoke_finalizers()       */
    /* depending on the nesting (recursion) level.              */
    if (++me->finalizer_skipped < (1U << nesting_level)) return NULL;
    me->finalizer_skipped = 0;
  }
  me->finalizer_nested = (unsigned char)(nesting_level + 1);
  return &me->finalizer_nested;
}


void GC_push_thread_structures(void)
{
  //  GC_push_all((ptr_t)(get_cur_thread()),
  //(ptr_t)(get_cur_thread()) + sizeof(nk_thread_t));

  //BDWGC_DEBUG("Pushing local thread structures between %p and %p\n",
  //get_cur_thread(), get_cur_thread() + sizeof(nk_thread_t));
}



static void mark_thread_local_fls(struct nk_thread *t, void *state)
{
  if (t->gc_state == NULL) {
    BDWGC_DEBUG("Initializing gc_state for %p (tid %d)\n", t, t->tid);
    t->gc_state = nk_gc_bdwgc_thread_state_init(t);
  }
  
  BDWGC_DEBUG("Marking tlfs for  %p (tid %d) with gc_state %p \n", t, t->tid, t->gc_state);
  GC_mark_thread_local_fls_for(&(BDWGC_SPECIFIC_THREAD_STATE(t) -> tlfs));
}



/* We must explicitly mark ptrfree and gcj free lists, since the free */
/* list links wouldn't otherwise be found.  We also set them in the   */
/* normal free lists, since that involves touching less memory than   */
/* if we scanned them normally.                                       */
GC_INNER void GC_mark_thread_local_free_lists(void)
{
    BDWGC_DEBUG("Marking all thread local free lists from %p (tid %lu)\n", get_cur_thread(),get_cur_thread()->tid);
    nk_sched_map_threads(-1, mark_thread_local_fls,0);
}


/* Perform all initializations, including those that    */
/* may require allocation.                              */
/* Called without allocation lock.                      */
/* Must be called before a second thread is created.    */
/* Did we say it's called without the allocation lock?  */
static GC_bool parallel_initialized = FALSE;

GC_INNER void GC_init_parallel(void)
{
  BDWGC_DEBUG("GC_init_parallel\n");
  
  DCL_LOCK_STATE;
  
  if (parallel_initialized) return;
  parallel_initialized = TRUE;

  /* GC_init() calls us back, so set flag first.      */
  if (!GC_is_initialized) GC_init();
  
  /* Initialize thread local free lists if used.      */
  LOCK();
  GC_init_thread_local(BDWGC_THREAD_TLFS());
  UNLOCK();
}


/* Wrapper for functions that are likely to block for an appreciable    */
/* length of time.                                                      */

/*ARGSUSED*/
GC_INNER void GC_do_blocking_inner(ptr_t data, void * context)
{
    struct blocking_data * d = (struct blocking_data *) data;
    bdwgc_thread_state* me = BDWGC_THREAD_STATE();

    DCL_LOCK_STATE;
    LOCK();
    GC_ASSERT(!(me -> thread_blocked));
    //        me -> stop_info.stack_ptr = GC_approx_sp();
    me -> thread_blocked = (unsigned char)TRUE;
    /* Save context here if we want to support precise stack marking */
    UNLOCK();
    d -> client_data = (d -> fn)(d -> client_data);
    LOCK();   /* This will block if the world is stopped.       */
    me -> thread_blocked = FALSE;

    UNLOCK();
}


/* GC_call_with_gc_active() has the opposite to GC_do_blocking()        */
/* functionality.  It might be called from a user function invoked by   */
/* GC_do_blocking() to temporarily back allow calling any GC function   */
/* and/or manipulating pointers to the garbage collected heap.          */
GC_API void * GC_CALL GC_call_with_gc_active(GC_fn_type fn,
                                             void * client_data)
{
    struct GC_traced_stack_sect_s stacksect;
    nk_thread_t* me = get_cur_thread();
    DCL_LOCK_STATE;

    LOCK();   /* This will block if the world is stopped.       */

    /* Adjust our stack base value (this could happen unless    */
    /* GC_get_stack_base() was used which returned GC_SUCCESS). */
    //    if ((me -> flags & MAIN_THREAD) == 0) {
    //GC_ASSERT(me -> stack_end != NULL);

    //    if (me -> stack HOTTER_THAN (ptr_t)(&stacksect))  // SHOULD ENABLE
            //me -> stack_end = (ptr_t)(&stacksect);  // SHOULD ENABLE
    
    //else {
      /* The original stack. */
      if (GC_stackbottom HOTTER_THAN (ptr_t)(&stacksect))
        GC_stackbottom = (ptr_t)(&stacksect);
      //}

    /* Setup new "stack section".       */
    //stacksect.saved_stack_ptr = me -> gc_state -> stop_info.stack_ptr;    // SHOULD ENABLE
    stacksect.prev = BDWGC_SPECIFIC_THREAD_STATE(me) -> traced_stack_sect;
    BDWGC_SPECIFIC_THREAD_STATE(me) -> thread_blocked = FALSE;
    BDWGC_SPECIFIC_THREAD_STATE(me) -> traced_stack_sect = &stacksect;

    UNLOCK();
    client_data = fn(client_data);
    //    GC_ASSERT(me -> gc_state -> thread_blocked == FALSE);
    GC_ASSERT(BDWGC_SPECIFIC_THREAD_STATE(me) -> traced_stack_sect == &stacksect);

    /* Restore original "stack section".        */
    LOCK();
    BDWGC_SPECIFIC_THREAD_STATE(me) -> traced_stack_sect = stacksect.prev;
    BDWGC_SPECIFIC_THREAD_STATE(me) -> thread_blocked = (unsigned char)TRUE;
    //BDWGC_SPECIFIC_THREAD_STATE(me) -> stop_info.stack_ptr = stacksect.saved_stack_ptr; // SHOULD ENABLE
    UNLOCK();

    return client_data; /* result */
}

/* It may not be safe to allocate when we register the first thread.    */
//static struct GC_Thread_Rep first_thread;

/* Delete a thread from GC_threads.  We assume it is there.     */
/* (The code intentionally traps if it wasn't.)                 */
/* It is safe to delete the main thread.                        */
STATIC void GC_delete_thread(nk_thread_t* t) { }


/* We hold the GC lock.  Wait until an in-progress GC has finished.     */
/* Repeatedly RELEASES GC LOCK in order to wait.                        */
/* If wait_for_all is true, then we exit with the GC lock held and no   */
/* collection in progress; otherwise we just wait for the current GC    */
/* to finish.                                                           */
STATIC void GC_wait_for_gc_completion(GC_bool wait_for_all)
{
    BDWGC_DEBUG("Waiting for GC Completion");
    
    DCL_LOCK_STATE;
    GC_ASSERT(I_HOLD_LOCK());
    ASSERT_CANCEL_DISABLED();
    if (GC_incremental && GC_collection_in_progress()) {
        word old_gc_no = GC_gc_no;

        /* Make sure that no part of our stack is still on the mark stack, */
        /* since it's about to be unmapped.                                */
        while (GC_incremental && GC_collection_in_progress()
               && (wait_for_all || old_gc_no == GC_gc_no)) {
            ENTER_GC();
            GC_in_thread_creation = TRUE;
            GC_collect_a_little_inner(1);
            GC_in_thread_creation = FALSE;
            EXIT_GC();
            UNLOCK();

            //sched_yield();
            nk_yield();
            
            LOCK();
        }
    }
}


STATIC void GC_unregister_my_thread_inner(nk_thread_t* me)
{
    BDWGC_DEBUG("Unregistering thread %p\n", get_cur_thread());
    GC_destroy_thread_local(&(BDWGC_SPECIFIC_THREAD_STATE(me) -> tlfs));
}


GC_API int GC_CALL GC_unregister_my_thread(void)
{
    IF_CANCEL(int cancel_state;)
    DCL_LOCK_STATE;

    LOCK();
    DISABLE_CANCEL(cancel_state);
    /* Wait for any GC that may be marking from our stack to    */
    /* complete before we remove this thread.                   */
    GC_wait_for_gc_completion(FALSE);
    GC_unregister_my_thread_inner(get_cur_thread());

    RESTORE_CANCEL(cancel_state);
    UNLOCK();
    return GC_SUCCESS;
}


/* It may not be safe to allocate when we register the first thread.    */
//static struct GC_Thread_Rep first_thread;

/* Add a thread to GC_threads.  We assume it wasn't already there.      */
/* Caller holds allocation lock.                                        */
STATIC nk_thread_t* GC_new_thread(nk_thread_t* t)
{
    BDWGC_DEBUG("Adding a new thread");
    return t;
}


/* We hold the allocation lock. */
GC_INNER void GC_thr_init(void)
{
  if (GC_thr_initialized) return;
  GC_thr_initialized = TRUE;

  /* Add the initial thread, so we can stop it. */
  nk_thread_t* t = get_cur_thread();

  GC_stop_init();

  /* Set GC_nprocs.     */
  GC_nprocs = GC_get_nprocs();
  
  if (GC_nprocs <= 0) {
    WARN("GC_get_nprocs() returned %" GC_PRIdPTR "\n", GC_nprocs);
    GC_nprocs = 2; /* assume dual-core */
  }
}


# if defined(GC_ASSERTIONS)
    void GC_check_tls_for(GC_tlfs p);

    static void check_thread_tls (struct nk_thread * t, void * state)
    {
       GC_check_tls_for(&(BDWGC_SPECIFIC_THREAD_STATE(t) -> tlfs));
    }

    /* Check that all thread-local free-lists are completely marked.    */
    /* Also check that thread-specific-data structures are marked.      */
    void GC_check_tls(void)
    {
      BDWGC_DEBUG("Checking thread-local free lists are marked\n");
      nk_sched_map_threads(-1, check_thread_tls,0);
    }

  /* This is called from thread-local GC_malloc(). */
  GC_bool GC_is_thread_tsd_valid(void *tsd)
  {
    nk_thread_t* me = get_cur_thread();

    //BDWGC_DEBUG("tsd = %d , me->tlfs =  %d\n", (char *)tsd, (char*)&me->tlfs);
    
    return (char *)tsd >= (char *)&(BDWGC_SPECIFIC_THREAD_STATE(me) -> tlfs)
      && (char *)tsd < (char *)&(BDWGC_SPECIFIC_THREAD_STATE(me) -> tlfs) + sizeof(BDWGC_SPECIFIC_THREAD_STATE(me) -> tlfs);
  }
# endif // GC_ASSERTIONS
