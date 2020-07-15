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
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2017, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __THREAD_H__
#define __THREAD_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLER__

#include <nautilus/spinlock.h>
#include <nautilus/intrinsics.h>

// Always included so we get the necessary type
#include <nautilus/cachepart.h>
#include <nautilus/aspace.h>

typedef uint64_t nk_stack_size_t;
    
#include <nautilus/scheduler.h>

#define CPU_ANY       -1

/* common thread stack sizes */
#define TSTACK_DEFAULT 0  // will be 4K
#define TSTACK_4KB     0x001000
#define TSTACK_1MB     0x100000
#define TSTACK_2MB     0x200000

/******** EXTERNAL INTERFACE **********/

// opaque pointer given to users
typedef void* nk_thread_id_t;
// this bad id value is intended for use with fork
// which cannot do error reporting the usual way
#define NK_BAD_THREAD_ID ((void*)(-1ULL))
typedef void (*nk_thread_fun_t)(void * input, void ** output);


// Create thread but do not launch it 
int
nk_thread_create (nk_thread_fun_t fun, 
		  void * input,
		  void ** output,
		  uint8_t is_detached,
		  nk_stack_size_t stack_size,
		  nk_thread_id_t * tid,
		  int bound_cpu);  // -1 => not bound

// Launch a previously created thread
int
nk_thread_run(nk_thread_id_t tid);

// Create and launch a thread
int
nk_thread_start (nk_thread_fun_t fun, 
                 void * input,
                 void ** output,
                 uint8_t is_detached,
                 nk_stack_size_t stack_size,
                 nk_thread_id_t * tid,
                 int bound_cpu); // -1 => not bound

// fork the current thread 
//   - parent is returned the tid of child
//   - child is returned zero
//   - child runs until it returns from the 
//     current function, which returns into
//     the thread cleanup logic instead of to
//     the caller
// on error, parents gets NK_BAD_THREAD_ID
extern nk_thread_id_t nk_thread_fork(void);

// Allow a child thread to set output explicitly
// This is not overwritten by an nk_thread_exit()
// or by subsequnt calls to nk_set_thread_output().
// thread function itself may write the output
// with its own semantics if needed
void nk_set_thread_output(void * result);

// Give a thread a name
int nk_thread_name(nk_thread_id_t tid, char *name);

// Give a thread a HW TLS space
int nk_thread_change_hw_tls(nk_thread_id_t tid, void *hwtls);

// explicit yield (hides details of the scheduler)
void nk_yield();

// Wait for a child or all children to exit
// This includes forked threads
// When joining all children, the optional
// function consumes their output values
int nk_join(nk_thread_id_t t, void ** retval);
int nk_join_all_children(int (*output_consumer)(void *output));

// called explictly or implicitly when a thread exits
void nk_thread_exit(void * retval) __attribute__((noreturn));

// garbage collect an exited thread - typically called
// by the reaper logic in the scheduler
void nk_thread_destroy(nk_thread_id_t t);


#ifndef __LEGION__
nk_thread_id_t nk_get_tid(void);
#endif
nk_thread_id_t nk_get_parent_tid(void);



/* thread local storage */
typedef unsigned int nk_tls_key_t; 
int nk_tls_key_create(nk_tls_key_t * key, void (*destructor)(void*));
int nk_tls_key_delete(nk_tls_key_t key);
void* nk_tls_get(nk_tls_key_t key);
int nk_tls_set(nk_tls_key_t key, const void * val);


/********* INTERNALS ***********/

// Support both fxsave and xsave for FP state
// The specific instruction used is determined in thread_lowlevel.S
#define XSAVE_SIZE 4096           // guess - actually is extensible format
                                  // current reasoning: 512 bytes for legacy
                                  // 1KB + for AVX2 (32*512 bit)
                                  // extra space
#define XSAVE_ALIGN 64            // per Intel docs
#define FXSAVE_SIZE 512           // per Intel docs
#define FXSAVE_ALIGN 16           // per Intel docs

#define FPSTATE_SIZE (XSAVE_SIZE>FXSAVE_SIZE ? XSAVE_SIZE : FXSAVE_SIZE)
#define FPSTATE_ALIGN (XSAVE_ALIGN>FXSAVE_ALIGN ? XSAVE_ALIGN : FXSAVE_ALIGN)

#define MAX_THREAD_NAME 32

/* FOR TLS */
#define TLS_MAX_KEYS 256
#define MIN_DESTRUCT_ITER 4
#define TLS_KEY_AVAIL(x) (((x) & 1) == 0)
#define TLS_KEY_USABLE(x) ((unsigned long)(x) < (unsigned long)((x)+2))


/* thread status */
typedef enum {
    NK_THR_INIT=0,
    NK_THR_RUNNING, 
    NK_THR_WAITING,
    NK_THR_SUSPENDED, 
    NK_THR_EXITED,
} nk_thread_status_t;


typedef struct nk_wait_queue nk_wait_queue_t;

struct nk_thread {
    uint64_t rsp;                /* +0  SHOULD NOT CHANGE POSITION */
    void * stack;                /* +8  SHOULD NOT CHANGE POSITION */
    uint16_t fpu_state_offset;   /* +16 SHOULD NOT CHANGE POSITION */
    nk_cache_part_thread_state_t /* +18 SHOULD NOT CHANGE POSITION */
             cache_part_state;   /* Always included to reserve this "slot" for asm code */
    nk_aspace_t      *aspace;    /* +24 SHOULD NOT CHANGE POSITION */
                                 /* Always included to reserve this "slot" for asm code */
    void             *hwtls;     /* +32 SHOULD NOT CHANGE POSITION */
                                 /* Always included to reserve this "slot" for asm code */
                                 /* even if TLS is off; this is the FSBASE for the thread */

    nk_stack_size_t stack_size;
    unsigned long tid;

    int lock;

    /* parent/child relationship */
    struct nk_thread * parent;
    struct list_head children;
    struct list_head child_node;
    unsigned long refcount;
    
    nk_wait_queue_t * waitq;             // wait queue for threads waiting on this thread
    
    int               num_wait;          // how many wait queues this thread is currently on

    // the per-thread default timer is allocated on first use
    struct nk_timer  *timer;

    /* thread state */
    nk_thread_status_t status;

    int bound_cpu;
    int placement_cpu;
    int current_cpu;

    uint8_t is_idle;

    void **output_loc;  // where the thread should write output
    void * output;      // our capture of the thread output (from exit)
    void * input;
    nk_thread_fun_t fun;
   
    struct nk_sched_thread_state *sched_state;

    struct nk_virtual_console *vc;

#ifdef NAUT_CONFIG_GARBAGE_COLLECTION
    void  *gc_state;
#endif

    char name[MAX_THREAD_NAME];

    const void * tls[TLS_MAX_KEYS];

    uint8_t fpu_state[FPSTATE_SIZE] __align(FPSTATE_ALIGN);
} ;

// internal thread representations
typedef struct nk_thread nk_thread_t;

nk_thread_id_t __thread_fork(void);

int
_nk_thread_init (nk_thread_t * t, 
		 void * stack, 
		 uint8_t is_detached, 
		 int bound_cpu, // -1 => not bound
		 int placement_cpu, // must be >=0 - where thread will go initially
		 nk_thread_t * parent);


struct nk_tls {
    unsigned seq_num;
    void (*destructor)(void*);
};

void nk_tls_test(void);

#include <nautilus/percpu.h>

static inline nk_thread_t*
get_cur_thread (void)
{
    return (nk_thread_t*)per_cpu_get(cur_thread);
}

static inline void
put_cur_thread (nk_thread_t * t) 
{
    per_cpu_put(cur_thread, t);
}


#endif /* !__ASSEMBLER */

#define SAVE_GPRS() \
    movq %rax, -8(%rsp); \
    movq %rbx, -16(%rsp); \
    movq %rcx, -24(%rsp); \
    movq %rdx, -32(%rsp); \
    movq %rsi, -40(%rsp); \
    movq %rdi, -48(%rsp); \
    movq %rbp, -56(%rsp); \
    movq %r8,  -64(%rsp); \
    movq %r9,  -72(%rsp); \
    movq %r10, -80(%rsp); \
    movq %r11, -88(%rsp); \
    movq %r12, -96(%rsp); \
    movq %r13, -104(%rsp); \
    movq %r14, -112(%rsp); \
    movq %r15, -120(%rsp); \
    subq $120, %rsp; 

#define RESTORE_GPRS() \
    movq (%rsp), %r15; \
    movq 8(%rsp), %r14; \
    movq 16(%rsp), %r13; \
    movq 24(%rsp), %r12; \
    movq 32(%rsp), %r11; \
    movq 40(%rsp), %r10; \
    movq 48(%rsp), %r9; \
    movq 56(%rsp), %r8; \
    movq 64(%rsp), %rbp; \
    movq 72(%rsp), %rdi; \
    movq 80(%rsp), %rsi; \
    movq 88(%rsp), %rdx; \
    movq 96(%rsp), %rcx; \
    movq 104(%rsp), %rbx; \
    movq 112(%rsp), %rax; \
    addq $120, %rsp; 

#ifdef __cplusplus
}
#endif

#endif  /* !__THREAD_H__ */
