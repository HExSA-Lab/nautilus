#ifndef __THREAD_H__
#define __THREAD_H__

#ifndef __ASSEMBLER__

#include <list.h>
#include <spinlock.h>

#define QNODE(x) struct list_head x
#define INIT_Q(x) INIT_LIST_HEAD(&((x)->queue));


struct thread {
    uint64_t rsp; /* KCH: this cannot change */
    void * stack;
    QNODE(q_node);
    QNODE(thr_list_node);
    long pid;
    struct thread * owner;

    struct thread_queue * waitq;
    QNODE(wait_node);
    void * exit_status;

    /* thread has finished? */
    uint8_t exited;

    unsigned long refcount;
};


struct thread_queue {
    spinlock_t lock;
    QNODE(queue);
    uint32_t num_entries;
};


typedef struct thread thread_t;

struct sched_state {
    struct thread_queue * run_q;
    struct thread_queue * thread_list;
    uint_t num_threads;
};

typedef void (*thread_fun_t)(void * arg);


/* the thread interface */
void yield(void);
void schedule(void);
int sched_init(void);
void exit(void * retval);
void wait(struct thread_queue * wq);
void wake_waiters(void);
int join(thread_t * t, void ** retval);
thread_t* thread_create(thread_fun_t fun, void * arg, uint8_t is_detached);
thread_t* thread_start(thread_fun_t fun, void * arg, uint8_t is_detached);
void thread_destroy(thread_t * t);




#endif /* !__ASSEMBLER */

#endif  /* !__THREAD_H__ */
