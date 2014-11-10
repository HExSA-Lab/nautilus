#ifndef __THREAD_H__
#define __THREAD_H__

#ifdef __cplusplus
extern "C" {
#endif

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

#ifndef __ASSEMBLER__

#include <spinlock.h>
#include <queue.h>

#define CPU_ANY -1
#define TSTACK_DEFAULT 0



typedef void (*thread_fun_t)(void * input, void ** output);
typedef uint64_t stack_size_t;
typedef long thread_id_t;
typedef struct thread thread_t;
typedef struct queue thread_queue_t;


#define TLS_MAX_KEYS 256
#define MIN_DESTRUCT_ITER 4
#define TLS_KEY_AVAIL(x) (((x) & 1) == 0)
#define TLS_KEY_USABLE(x) ((unsigned long)(x) < (unsigned long)((x)+2))

typedef unsigned int tls_key_t; 

struct tls {
    unsigned seq_num;
    void (*destructor)(void*);
};

struct thread {
    uint64_t rsp; /* KCH: this cannot change */
    void * stack;
    stack_size_t stack_size;

    queue_entry_t runq_node; // formerly q_node
    queue_entry_t thr_list_node;
    thread_id_t tid;
    struct thread * owner;

    thread_queue_t * waitq;
    queue_entry_t wait_node;
    uint8_t waiting;

    thread_queue_t * cur_run_q;
    void * exit_status;

    /* thread has finished? */
    uint8_t exited;

    unsigned long refcount;
    int bound_cpu;

    const void * tls[TLS_MAX_KEYS];
};


struct sched_state {
    thread_queue_t * thread_list;
    uint_t num_threads;
};


/* the thread interface */
void yield(void);
void schedule(void);
thread_t* need_resched(void);
int sched_init(void);
int sched_init_ap(void);
void thread_exit(void * retval);
void wait(thread_t * t);
void wake_waiters(void);
int join(thread_t * t, void ** retval);

thread_t* 
thread_create (thread_fun_t fun, 
               void * input,
               void ** output,
               uint8_t is_detached,
               stack_size_t stack_size,
               thread_id_t * tid,
               int cpu);
thread_t* 
thread_start (thread_fun_t fun, 
               void * input,
               void ** output,
               uint8_t is_detached,
               stack_size_t stack_size,
               thread_id_t * tid,
               int cpu);

extern thread_id_t thread_fork(void);
thread_id_t _thread_fork(void);

void thread_destroy(thread_t * t);
thread_id_t get_tid(void);
thread_id_t get_parent_tid(void);

thread_queue_t * thread_queue_create (void);
void thread_queue_destroy(thread_queue_t * q);
inline void enqueue_thread_on_runq(thread_t * t, int cpu);
int thread_queue_sleep(thread_queue_t * q);
int thread_queue_wake_one(thread_queue_t * q);
int thread_queue_wake_all(thread_queue_t * q);


int tls_key_create(tls_key_t * key, void (*destructor)(void*));
int tls_key_delete(tls_key_t key);
void* tls_get(tls_key_t key);
int tls_set(tls_key_t key, const void * val);
void tls_test(void);

#include <percpu.h>

static inline thread_t*
get_cur_thread (void) 
{
    return (thread_t*)per_cpu_get(cur_thread);
}

static inline void
put_cur_thread (thread_t * t)
{
    per_cpu_put(cur_thread, t);
}



#endif /* !__ASSEMBLER */

#ifdef __cplusplus
}
#endif

#endif  /* !__THREAD_H__ */
