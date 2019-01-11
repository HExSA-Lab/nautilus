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
// the Nautilus kernel, based on code copyrighted as follows

/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
 * Copyright (c) 2000-2009 by Hewlett-Packard Development Company.
 * All rights reserved.
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


#include <nautilus/list.h>
#include <nautilus/scheduler.h>
#include "private/pthread_support.h"


#ifdef NAUT_THREADS


GC_INNER void GC_stop_world(void)
{
    BDWGC_DEBUG("Stopping the world from thread %p tid %lu\n", get_cur_thread(),get_cur_thread()->tid);
    nk_sched_stop_world();
}


/* Caller holds allocation lock, and has held it continuously since     */
/* the world stopped.                                                   */
GC_INNER void GC_start_world(void)
{
    BDWGC_DEBUG("Starting the world from %p (tid %lu)\n", get_cur_thread(),get_cur_thread()->tid);
    nk_sched_start_world();
}


static void push_thread_stack(nk_thread_t *t, void *state)
{
  ptr_t lo, hi;
  struct GC_traced_stack_sect_s *traced_stack_sect;
  word total_size = 0;

  //if (!GC_thr_initialized) GC_thr_init();
  
  traced_stack_sect = BDWGC_SPECIFIC_THREAD_STATE(t) -> traced_stack_sect;

  lo = BDWGC_SPECIFIC_STACK_TOP(t);
  hi = BDWGC_SPECIFIC_STACK_BOTTOM(t);
  
  if (traced_stack_sect != NULL
      && traced_stack_sect->saved_stack_ptr == lo) {
    /* If the thread has never been stopped since the recent  */
    /* GC_call_with_gc_active invocation then skip the top    */
    /* "stack section" as stack_ptr already points to.        */
    //    traced_stack_sect = traced_stack_sect->prev;
  }
  
  if (lo==hi) { 
      BDWGC_DEBUG("Skipping stack as it is empty\n");
  } else {
      BDWGC_DEBUG("Pushing stack for thread (%p, tid=%u), range = [%p,%p)\n", t, t->tid, lo, hi);
      //GC_push_all_stack_sections(lo, hi, traced_stack_sect);
      GC_push_all_stack_sections(lo, hi, NULL);
      total_size += hi - lo; /* lo <= hi */
  }
}

/* We hold allocation lock.  Should do exactly the right thing if the   */
/* world is stopped.  Should not fail if it isn't.                      */
GC_INNER void GC_push_all_stacks(void)
{									
    if (!get_cur_thread()) { 
	BDWGC_ERROR("We are doing a garbage collection before threads are active..\n");
	panic("Garbage collection before threads active!\n");
	return;
    } else {
	BDWGC_DEBUG("Pushing stacks from thread %p\n", get_cur_thread());
	nk_sched_map_threads(-1,push_thread_stack,0);
    }
}


GC_INNER void GC_stop_init(void)
{
  BDWGC_DEBUG("GC_stop_init ... does nothing..\n");
}

#endif // NAUT_THREADS



