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




struct nk_thread;

#include <nautilus/list.h>
#include <nautilus/thread.h>
#include <gc/bdwgc/bdwgc.h>
#include "bdwgc_internal.h"
#include "gc.h"


typedef unsigned long word;

extern NK_LOCK_T GC_allocate_ml;
#define LOCK() NK_LOCK(&GC_allocate_ml)
#define UNLOCK() NK_UNLOCK(&GC_allocate_ml)
#define DCL_LOCK_STATE


//#define MALLOC(x) ({ void *p  = malloc((x)+2*PAD); if (!p) { panic("Failed to Allocate %d bytes\n",x); } memset(p,0,(x)+2*PAD); p+PAD; })

extern nk_tls_key_t GC_thread_key;
extern int  keys_initialized;


void GC_init_thread_local(GC_tlfs p); // included from thread_local_alloc.h

/* Called from GC_inner_start_routine().  Defined in this file to       */
/* minimize the number of include files in pthread_start.c (because     */
/* sem_t and sem_post() are not used that file directly).               */
nk_thread_t *
GC_start_rtn_prepare_thread(void *(**pstart)(void *),
                            void **pstart_arg,
                            struct GC_stack_base *sb, void *arg)
{
  struct start_info * si = arg;
  nk_thread_t * me = get_cur_thread();
  DCL_LOCK_STATE;

  BDWGC_DEBUG("Starting thread %p, sp = %p\n", me, &arg);

  LOCK();
  //me = GC_register_my_thread_inner(sb, me);
  BDWGC_SPECIFIC_THREAD_STATE(me) -> traced_stack_sect = NULL;
  BDWGC_SPECIFIC_THREAD_STATE(me) -> flags = si -> flags;

  GC_init_thread_local(&(BDWGC_SPECIFIC_THREAD_STATE(me) -> tlfs));

  UNLOCK();
  
  *pstart = si -> start_routine;

  //BDWGC_DEBUG("start_routine = %p\n", (void *)(signed_word)(*pstart));

  *pstart_arg = si -> arg;
  //  sem_post(&(si -> registered));      /* Last action on si.   */
  /* OK to deallocate.    */
  return me;
}


void *nk_gc_bdwgc_thread_state_init(struct nk_thread *thread)
{
  bdwgc_thread_state * s = (bdwgc_thread_state *)malloc(sizeof(bdwgc_thread_state));

  if (!s) { 
      BDWGC_ERROR("Failed to allocate per-thread GC state for thread %ld\n",thread->tid);
      return 0;
  }

  s -> traced_stack_sect = NULL;
  s -> thread_blocked = false;
  /* if (0 != nk_tls_key_create(&GC_thread_key, 0)) { */
  /*   panic("Failed to create key for local allocator"); */
  /* } */

  /* if (0 != nk_tls_set(GC_thread_key, tlfs)) { */
  /*   panic("Failed to set thread specific allocation pointers"); */
  /* } */

  int i;
  for (i = 1; i < GC_TINY_FREELISTS; ++i) {
    s -> tlfs.ptrfree_freelists[i] = (void *)(unsigned long)1;
    s -> tlfs.normal_freelists[i]  = (void *)(unsigned long)1;
  }
  /* Set up the size 0 free lists.    */
  /* We now handle most of them like regular free lists, to ensure    */
  /* That explicit deallocation works.  */
  s -> tlfs.ptrfree_freelists[0] = (void *)(unsigned long)1;
  s -> tlfs.normal_freelists[0]  = (void *)(unsigned long)1;
  
  BDWGC_DEBUG("Initialized per-thread GC state at %p for thread %ld\n", s,thread->tid);

  return s;
}

void nk_gc_bdwgc_thread_state_deinit(struct nk_thread *t)
{
    BDWGC_DEBUG("Freeing per-thread GC state at %p for thread %ld\n", t->gc_state, t->tid);
    free(t->gc_state);
}


int  nk_gc_bdwgc_init()
{
    // We cannot do a GC until threads are up... 
    GC_set_dont_precollect(1);
    // Fail to the kernel allocator if we cannot allocate
    GC_set_oom_fn(kmem_malloc); 
    // Initialize
    GC_INIT();
    // Give us an intial heap so we don't do a collection
    // until we have threads up
    GC_expand_hp(64*1024*1024);
    BDWGC_INFO("inited - initial heap size: %lu (%lu unmapped bytes)\n",GC_get_heap_size(),GC_get_unmapped_bytes());
    return 0;
}
void nk_gc_bdwgc_deinit()
{
    BDWGC_INFO("deinited\n");
}

int nk_gc_bdwgc_collect()
{
    GC_gcollect();
    return 0;
}

#ifdef NAUT_CONFIG_TEST_BDWGC
int  nk_gc_bdwgc_test()
{
    extern int bdwgc_test();

    return bdwgc_test();
}
#endif
