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
// the Nautilus kernel


/** Includes support code for the Boehm Garbage collector. */

#include <nautilus/vc.h>
#include <nautilus/backtrace.h>


#ifndef __BDWGC_INTERNAL__
#define __BDWGC_INTERNAL__


#ifndef NAUT_CONFIG_DEBUG_BDWGC
#define BDWGC_DEBUG(fmt, args...)
#else
#define BDWGC_DEBUG(fmt, args...) DEBUG_PRINT("bdwgc: gc_state=%p: " fmt, !get_cur_thread() ? 0 : get_cur_thread()->gc_state, ##args)
#endif

#define BDWGC_INFO(fmt, args...) INFO_PRINT("bdwgc: gc_state=%p: " fmt, !get_cur_thread() ? 0 : get_cur_thread()->gc_state, ##args)
#define BDWGC_WARN(fmt, args...) WARN_PRINT("bdwgc: gc_state=%p: " fmt, !get_cur_thread() ? 0 : get_cur_thread()->gc_state, ##args)
#define BDWGC_ERROR(fmt, args...) ERROR_PRINT("bdwgc: gc_state=%p: " fmt, !get_cur_thread() ? 0 : get_cur_thread()->gc_state, ##args)


#define BDWGC_SPECIFIC_THREAD_TLFS(t) ((t) ? (t)->gc_state ? &(((bdwgc_thread_state*)((t)->gc_state))->tlfs) : 0 : 0)
#define BDWGC_THREAD_TLFS() (BDWGC_SPECIFIC_THREAD_TLFS(get_cur_thread()))

#define BDWGC_SPECIFIC_THREAD_STATE(t) ((t) ? (bdwgc_thread_state*)((t)->gc_state) : 0)
#define BDWGC_THREAD_STATE() (BDWGC_SPECIFIC_THREAD_STATE(get_cur_thread()))

#define BDWGC_SPECIFIC_STACK_TOP(t) ((t) ? ((void*)((uint64_t)(t)->rsp)) : 0 )
#define BDWGC_STACK_TOP() (BDWGC_SPECIFIC_STACK_TOP(get_cur_thread()))

#define BDWGC_SPECIFIC_STACK_BOTTOM(t) ((t) ? ((void*)((uint64_t)(t)->stack + (t)->stack_size - 0)) : 0)
#define BDWGC_STACK_BOTTOM() (BDWGC_SPECIFIC_STACK_BOTTOM(get_cur_thread()))

/* A generic pointer to which we can add        */
/* byte displacements and which can be used     */
/* for address comparisons.                     */
typedef char * ptr_t;
  
/* This is used by GC_call_with_gc_active(), GC_push_all_stack_sections(). */
struct GC_traced_stack_sect_s {
  ptr_t saved_stack_ptr;
  struct GC_traced_stack_sect_s *prev;
};  

/* Originally from gc_tiny_fl.h
 * 
 * Constants and data structures for "tiny" free lists.
 * These are used for thread-local allocation or in-lined allocators.
 * Each global free list also essentially starts with one of these.
 * However, global free lists are known to the GC.  "Tiny" free lists
 * are basically private to the client.  Their contents are viewed as
 * "in use" and marked accordingly by the core of the GC.
 *
 * Note that inlined code might know about the layout of these and the constants
 * involved.  Thus any change here may invalidate clients, and such changes should
 * be avoided.  Hence we keep this as simple as possible.
 */

/*
 * We always set GC_GRANULE_BYTES to twice the length of a pointer.
 * This means that all allocation requests are rounded up to the next
 * multiple of 16 on 64-bit architectures or 8 on 32-bit architectures.
 * This appears to be a reasonable compromise between fragmentation overhead
 * and space usage for mark bits (usually mark bytes).
 * On many 64-bit architectures some memory references require 16-byte
 * alignment, making this necessary anyway.
 * For a few 32-bit architecture (e.g. x86), we may also need 16-byte alignment
 * for certain memory references.  But currently that does not seem to be the
 * default for all conventional malloc implementations, so we ignore that
 * problem.
 * It would always be safe, and often useful, to be able to allocate very
 * small objects with smaller alignment.  But that would cost us mark bit
 * space, so we no longer do so.
 */
#ifndef GC_GRANULE_BYTES
/* GC_GRANULE_BYTES should not be overridden in any instances of the GC */
/* library that may be shared between applications, since it affects	  */
/* the binary interface to the library.				  */
# if defined(__LP64__) || defined (_LP64) || defined(_WIN64)    \
  || defined(__s390x__)                                         \
  || (defined(__x86_64__) && !defined(__ILP32__))               \
  || defined(__alpha__) || defined(__powerpc64__)               \
  || defined(__arch64__)
#  define GC_GRANULE_BYTES 16
#  define GC_GRANULE_WORDS 2
# else
#  define GC_GRANULE_BYTES 8
#  define GC_GRANULE_WORDS 2
# endif
#endif /* !GC_GRANULE_BYTES */

#if GC_GRANULE_WORDS == 2
#  define GC_WORDS_TO_GRANULES(n) ((n)>>1)
#else
#  define GC_WORDS_TO_GRANULES(n) ((n)*sizeof(void *)/GC_GRANULE_BYTES)
#endif

/* A "tiny" free list header contains TINY_FREELISTS pointers to 	*/
/* singly linked lists of objects of different sizes, the ith one	*/
/* containing objects i granules in size.  Note that there is a list	*/
/* of size zero objects.						*/
#ifndef GC_TINY_FREELISTS
# if GC_GRANULE_BYTES == 16
#   define GC_TINY_FREELISTS 25
# else
#   define GC_TINY_FREELISTS 33	/* Up to and including 256 bytes */
# endif
#endif /* !GC_TINY_FREELISTS */

/* The ith free list corresponds to size i*GC_GRANULE_BYTES	*/
/* Internally to the collector, the index can be computed with	*/
/* ROUNDED_UP_GRANULES.  Externally, we don't know whether	*/
/* DONT_ADD_BYTE_AT_END is set, but the client should know.	*/

/* Convert a free list index to the actual size of objects	*/
/* on that list, including extra space we added.  Not an	*/
/* inverse of the above.					*/
#define GC_RAW_BYTES_FROM_INDEX(i) ((i) * GC_GRANULE_BYTES)

  
/* Originally from thread_local_alloc.h
 * 
 * Free lists contain either a pointer or a small count       */
/* reflecting the number of granules allocated at that        */
/* size.                                                      */
/* 0 ==> thread-local allocation in use, free list            */
/*       empty.                                               */
/* > 0, <= DIRECT_GRANULES ==> Using global allocation,       */
/*       too few objects of this size have been               */
/*       allocated by this thread.                            */
/* >= HBLKSIZE  => pointer to nonempty free list.             */
/* > DIRECT_GRANULES, < HBLKSIZE ==> transition to            */
/*    local alloc, equivalent to 0.                           */
typedef struct thread_local_freelists {
  void * ptrfree_freelists[GC_TINY_FREELISTS];
  void * normal_freelists[GC_TINY_FREELISTS];

  /* Don't use local free lists for up to this much       */
  /* allocation.                                          */
  # define DIRECT_GRANULES (HBLKSIZE/GRANULE_BYTES)

  //  # define DIRECT_GRANULES 99999999999
  
} *GC_tlfs;



typedef unsigned int sem_t;
struct start_info {
    void *(*start_routine)(void *);
    void *arg;
    unsigned long flags;
    sem_t registered;           /* 1 ==> in our thread table, but       */
                                /* parent hasn't yet noticed.           */
};


typedef struct {
    //Extra bookkeeping information the stopping code uses */
    //struct thread_stop_info stop_info;
    unsigned char flags;

    /* Protected by GC lock.                */
    /* Treated as a boolean value.  If set, */
    /* thread will acquire GC lock before   */
    /* doing any pointer manipulations, and */
    /* has set its SP value.  Thus it does  */
    /* not need to be sent a signal to stop */
    /* it.                                  */
    unsigned char thread_blocked;

  /* Used by GC_check_finalizer_nested()  */
  /* to minimize the level of recursion   */
  /* when a client finalizer allocates    */
  /* memory (initially both are 0).       */
    unsigned short finalizer_skipped;
    unsigned char finalizer_nested;

  /* Points to the "frame" data held in stack by  */
  /* the innermost GC_call_with_gc_active() of    */
  /* this thread.  May be NULL.                   */
  struct GC_traced_stack_sect_s *traced_stack_sect;

  /* The value returned from the thread.  */
  /* Used only to avoid premature         */
  /* reclamation of any data it might     */
  /* reference.                           */
  /* This is unfortunately also the       */
  /* reason we need to intercept join     */
  /* and detach.                          */
    void * gc_status;
    struct thread_local_freelists tlfs;

  //  struct thread_stop_info stop_info;
  
} bdwgc_thread_state ;

bdwgc_thread_state * bdwgc_thread_state_init();


#endif
