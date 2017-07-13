/*
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

#include "private/gc_priv.h"

#if defined(THREAD_LOCAL_ALLOC)

#ifndef THREADS
# error "invalid config - THREAD_LOCAL_ALLOC requires GC_THREADS"
#endif

#include "private/thread_local_alloc.h"

#ifndef NAUT
#include <stdlib.h>
#endif

#if defined(USE_COMPILER_TLS)
  __thread
#elif defined(USE_WIN32_COMPILER_TLS)
  __declspec(thread)
#endif

/* #ifdef NAUT */
/*   //extern GC_key_t GC_thread_key; */
/* #else  */
/* GC_key_t GC_thread_key; */
/* #endif */

static GC_bool keys_initialized;

/* Return a single nonempty freelist fl to the global one pointed to    */
/* by gfl.      */

static void return_single_freelist(void *fl, void **gfl)
{
    void *q, **qptr;

    if (*gfl == 0) {
      *gfl = fl;
    } else {
      GC_ASSERT(GC_size(fl) == GC_size(*gfl));
      /* Concatenate: */
        qptr = &(obj_link(fl));
        while ((word)(q = *qptr) >= HBLKSIZE)
          qptr = &(obj_link(q));
        GC_ASSERT(0 == q);
        *qptr = *gfl;
        *gfl = fl;
    }
}

/* Recover the contents of the freelist array fl into the global one gfl.*/
/* We hold the allocator lock.                                          */
static void return_freelists(void **fl, void **gfl)
{
    int i;

    for (i = 1; i < TINY_FREELISTS; ++i) {
        if ((word)(fl[i]) >= HBLKSIZE) {
          return_single_freelist(fl[i], gfl+i);
        }
        /* Clear fl[i], since the thread structure may hang around.     */
        /* Do it in a way that is likely to trap if we access it.       */
        fl[i] = (ptr_t)HBLKSIZE;
    }
    /* The 0 granule freelist really contains 1 granule objects.        */
#   ifdef GC_GCJ_SUPPORT
      if (fl[0] == ERROR_FL) return;
#   endif
    if ((word)(fl[0]) >= HBLKSIZE) {
        return_single_freelist(fl[0], gfl+1);
    }
}

/* Each thread structure must be initialized.   */
/* This call must be made from the new thread.  */
GC_INNER void GC_init_thread_local(GC_tlfs p)
{
  #ifndef NAUT
    int i;

    printk("BDWGC: GC_init_thread_local\n");
    
    GC_ASSERT(I_HOLD_LOCK());
    if (!keys_initialized) {
        if (0 != GC_key_create(&GC_thread_key, 0)) {
            ABORT("Failed to create key for local allocator");
        }
        keys_initialized = TRUE;
    }
    if (0 != GC_setspecific(GC_thread_key, p)) {
        ABORT("Failed to set thread specific allocation pointers");
    }
    for (i = 1; i < TINY_FREELISTS; ++i) {
        p -> ptrfree_freelists[i] = (void *)(word)1;
        p -> normal_freelists[i] = (void *)(word)1;
#       ifdef GC_GCJ_SUPPORT
          p -> gcj_freelists[i] = (void *)(word)1;
#       endif
    }
    /* Set up the size 0 free lists.    */
    /* We now handle most of them like regular free lists, to ensure    */
    /* That explicit deallocation works.  However, allocation of a      */
    /* size 0 "gcj" object is always an error.                          */
    p -> ptrfree_freelists[0] = (void *)(word)1;
    p -> normal_freelists[0] = (void *)(word)1;
#   ifdef GC_GCJ_SUPPORT
        p -> gcj_freelists[0] = ERROR_FL;
#   endif

#endif
}

/* We hold the allocator lock.  */
GC_INNER void GC_destroy_thread_local(GC_tlfs p)
{
    /* We currently only do this from the thread itself or from */
    /* the fork handler for a child process.                    */
    return_freelists(p -> ptrfree_freelists, GC_aobjfreelist);
    return_freelists(p -> normal_freelists, GC_objfreelist);
}


GC_bool GC_is_thread_tsd_valid(void *tsd);

GC_API void * GC_CALL GC_malloc(size_t bytes)
{
    BDWGC_DEBUG("Allocating %d bytes for thread %p\n", bytes, get_cur_thread());
    size_t granules = ROUNDED_UP_GRANULES(bytes);
    void *result;

    if (!BDWGC_THREAD_TLFS())
      {
        BDWGC_DEBUG("ENTER CORE MALLOC %p\n", get_cur_thread());
        result =  GC_core_malloc(bytes);
        BDWGC_DEBUG("core malloc is returning %p\n", result);
        return result; 
      }
    
    GC_ASSERT(GC_is_initialized);

    BDWGC_DEBUG("FAST_MALLOC_GRANS\n");

    GC_FAST_MALLOC_GRANS(result,
                         granules,
                         BDWGC_THREAD_TLFS()->normal_freelists,
                         DIRECT_GRANULES,
                         NORMAL,
                         GC_core_malloc(bytes),
                         obj_link(result)=0);

#   ifdef LOG_ALLOCS
      GC_err_printf("GC_malloc(%u) = %p : %u\n",
                        (unsigned)bytes, result, (unsigned)GC_gc_no);
#   endif

    BDWGC_DEBUG("Malloc is returning %p\n",result);
    return result;
}


GC_API void * GC_CALL GC_malloc_atomic(size_t bytes)
{
    size_t granules = ROUNDED_UP_GRANULES(bytes);
    void *tsd = BDWGC_THREAD_TLFS();
    void *result;
    void **tiny_fl;

    GC_ASSERT(GC_is_initialized);
    tiny_fl = ((GC_tlfs)tsd) -> ptrfree_freelists;
    GC_FAST_MALLOC_GRANS(result, granules, tiny_fl, DIRECT_GRANULES, PTRFREE,
                         GC_core_malloc_atomic(bytes), (void)0 /* no init */);
    return result;
}


/* The thread support layer must arrange to mark thread-local   */
/* free lists explicitly, since the link field is often         */
/* invisible to the marker.  It knows how to find all threads;  */
/* we take care of an individual thread freelist structure.     */
GC_INNER void GC_mark_thread_local_fls_for(GC_tlfs p)
{
    ptr_t q;
    int j;

    for (j = 0; j < TINY_FREELISTS; ++j) {
      q = p->ptrfree_freelists[j];
      if ((word)q > HBLKSIZE) GC_set_fl_marks(q);
      q = p->normal_freelists[j];
      if ((word)q > HBLKSIZE) GC_set_fl_marks(q);
    }
}

#if defined(GC_ASSERTIONS)
    /* Check that all thread-local free-lists in p are completely marked. */
    void GC_check_tls_for(GC_tlfs p)
    {
        BDWGC_DEBUG("Checking %p is marked\n", p);

        int j;

        for (j = 1; j < TINY_FREELISTS; ++j) {
          GC_check_fl_marks(&p->ptrfree_freelists[j]);
          GC_check_fl_marks(&p->normal_freelists[j]);
        }
    }
#endif /* GC_ASSERTIONS */

#endif /* THREAD_LOCAL_ALLOC */
