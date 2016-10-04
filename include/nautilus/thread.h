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
#ifndef __THREAD_H__
#define __THREAD_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLER__

#include <nautilus/spinlock.h>
#include <nautilus/queue.h>
#include <nautilus/intrinsics.h>
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
typedef void (*nk_thread_fun_t)(void * input, void ** output);
typedef uint64_t nk_stack_size_t;


int
nk_thread_create (nk_thread_fun_t fun, 
		  void * input,
		  void ** output,
		  uint8_t is_detached,
		  nk_stack_size_t stack_size,
		  nk_thread_id_t * tid,
		  int bound_cpu);  // -1 => not bound

int
nk_thread_run(nk_thread_id_t tid);

int
nk_thread_start (nk_thread_fun_t fun, 
                 void * input,
                 void ** output,
                 uint8_t is_detached,
                 nk_stack_size_t stack_size,
                 nk_thread_id_t * tid,
                 int bound_cpu); // -1 => not bound

int nk_thread_name(nk_thread_id_t tid, char *name);

extern nk_thread_id_t nk_thread_fork(void);

void nk_set_thread_fork_output(void * result);
void nk_thread_exit(void * retval);
void nk_thread_destroy(nk_thread_id_t t); /* like thread_kill */
void nk_wait(nk_thread_id_t t);
void nk_yield();

void nk_wake_waiters(void);
int nk_join(nk_thread_id_t t, void ** retval);
int nk_join_all_children(int (*)(void*));

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

#define FXSAVE_SIZE 512

#define MAX_THREAD_NAME 32

/* FOR TLS */
#define TLS_MAX_KEYS 256
#define MIN_DESTRUCT_ITER 4
#define TLS_KEY_AVAIL(x) (((x) & 1) == 0)
#define TLS_KEY_USABLE(x) ((unsigned long)(x) < (unsigned long)((x)+2))


/* thread status */
typedef enum {
    NK_THR_INIT,
    NK_THR_RUNNING, 
    NK_THR_WAITING,
    NK_THR_SUSPENDED, 
    NK_THR_EXITED
} nk_thread_status_t;

typedef struct nk_queue nk_thread_queue_t;

struct nk_thread {
    uint64_t rsp; /* SHOULD NOT CHANGE POSITION */
    void * stack; /* SHOULD NOT CHANGE POSITION */
    uint16_t fpu_state_offset; /* SHOULD NOT CHANGE POSITION */

    nk_stack_size_t stack_size;
    unsigned long tid;

    int lock;

    nk_queue_entry_t runq_node; // formerly q_node
    nk_queue_entry_t thr_list_node;

    /* parent/child relationship */
    struct nk_thread * parent;
    struct list_head children;
    struct list_head child_node;
    unsigned long refcount;

    nk_thread_queue_t * waitq;
    nk_queue_entry_t wait_node;

    nk_thread_queue_t * cur_run_q;
    
    /* thread state */
    nk_thread_status_t status;

    int bound_cpu;

    int current_cpu;

    uint8_t is_idle;

    void * output;
    void * input;
    nk_thread_fun_t fun;

    struct nk_sched_thread_state *sched_state;

    struct nk_virtual_console *vc;

    char name[MAX_THREAD_NAME];

    const void * tls[TLS_MAX_KEYS];

    uint8_t fpu_state[FXSAVE_SIZE] __align(16);
} __packed;

// internal thread representations
typedef struct nk_thread nk_thread_t;

nk_thread_id_t __thread_fork(void);

int
_nk_thread_init (nk_thread_t * t, 
		 void * stack, 
		 uint8_t is_detached, 
		 int bound_cpu, // -1 => not bound
		 nk_thread_t * parent);


/* thread queues */

nk_thread_queue_t * nk_thread_queue_create (void);
void nk_thread_queue_destroy(nk_thread_queue_t * q);
inline void nk_enqueue_thread_on_runq(nk_thread_t * t, int cpu);
inline nk_thread_t* nk_dequeue_thread_from_runq(nk_thread_t * t);
int nk_thread_queue_sleep(nk_thread_queue_t * q);
int nk_thread_queue_wake_one(nk_thread_queue_t * q);
int nk_thread_queue_wake_all(nk_thread_queue_t * q);

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
