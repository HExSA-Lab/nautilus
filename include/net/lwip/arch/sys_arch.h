#ifndef __LWIP_SYS_H__
#define __LWIP_SYS_H__

#include <nautilus/nautilus.h>
#include <nautilus/semaphore.h>
#include <nautilus/msg_queue.h>
#include <nautilus/thread.h>
#include "arch/cc.h"


typedef struct nk_semaphore sys_sem;
typedef sys_sem sys_mutex;
typedef nk_thread_id_t sys_thread_t;
typedef struct nk_msg_queue sys_mbox;

typedef struct{
	sys_sem* sem;
} sys_sem_t, sys_mutex_t;


typedef struct {
	sys_mbox* mq;
} sys_mbox_t;

#endif
