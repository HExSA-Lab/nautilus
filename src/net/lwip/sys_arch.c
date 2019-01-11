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
 * Copyright (c) 2018, Hangjie Mo
 * Copyright (c) 2018, Weizheng Xu
 * Copyright (c) 2018, Peter Dinda
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Hangjie Mo   <HangjieMo2019@u.northwestern.edu>
 *           Weizheng Xu  <WeizhengXu2019@u.northwestern.edu> 
 *           Peter Dinda  <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/*
  This is a wrapper for a port of LWIP to the Nautilus kernel framework.  
  The copyright claims in the above statement apply to this wrapper and
  the changes to the LWIP code made during the port.  

  No claims are made to the LWIP codebase, which is BSD licensed.  
  The license statement is repeated below, and is in the source files.

  Copyright (c) 2001-2004 Swedish Institute of Computer Science. 
  All rights reserved. 

  Redistribution and use in source and binary forms, with or without modification, 
  are permitted provided that the following conditions are met: 

  1. Redistributions of source code must retain the above copyright notice, 
  this list of conditions and the following disclaimer. 
  2. Redistributions in binary form must reproduce the above copyright notice, 
  this list of conditions and the following disclaimer in the documentation 
  and/or other materials provided with the distribution. 
  3. The name of the author may not be used to endorse or promote products 
  derived from this software without specific prior written permission. 

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS AND ANY EXPRESS OR IMPLIED 
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
  SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
  OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
  OF SUCH DAMAGE.
  
*/

#include <nautilus/nautilus.h>
#include <nautilus/semaphore.h>
#include <nautilus/msg_queue.h>
#include <nautilus/timer.h>
#include <nautilus/errno.h>
#include <nautilus/spinlock.h>
#include <nautilus/thread.h>
#include <arch/sys_arch.h>
#include <lwip/err.h>
#include <lwip/debug.h>
#include <lwip/sys.h>
#include <lwip/arch.h>

static u64_t start_time; // in ms

#define NS_PER_MS 1000000ULL
#define FLOOR_DIV(x,y) (((x)/(y)))
#define CEIL_DIV(x,y) (((x)/(y)) + (!!((x)%(y))))

void sys_init(void)
{
    start_time = FLOOR_DIV(nk_sched_get_realtime(),NS_PER_MS);
}

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    DEBUG("Sem new count=%u\n",count);

    if(sem==NULL) {
	return ERR_MEM;
    }
    
    sem->sem = nk_semaphore_create(0,count,NK_SEMAPHORE_DEFAULT,0);

    if(sem->sem == NULL) {
	return ERR_MEM;
    }
    
    DEBUG("Sem new success %p\n",sem);

    return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem)
{
    if (sem==NULL || sem->sem==NULL) { return; }

    DEBUG("Sem free %p\n",sem);
	
    LWIP_ASSERT("invalid sem", (sem->sem != NULL));
    
    nk_semaphore_release(sem->sem);
    
    sem->sem=NULL;
}

void sys_sem_signal(sys_sem_t *sem)
{
    LWIP_ASSERT("invalid sem", (sem->sem != NULL));
    DEBUG("Sem signal %p\n",sem);
    nk_semaphore_up(sem->sem);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    u64_t timeout_ns = timeout * NS_PER_MS;
    
    LWIP_ASSERT("invalid sem", (sem->sem != NULL));

    DEBUG("Sem wait %p (timeout=%u)\n",sem,timeout);
    
    if(timeout == 0){
	nk_semaphore_down(sem->sem);
    } else {

	u64_t start = nk_sched_get_realtime();
	u32_t flag = nk_semaphore_down_timeout(sem->sem, timeout_ns);
	
	if(flag > 0) {
	    return SYS_ARCH_TIMEOUT;
	} else {
	    u64_t end = nk_sched_get_realtime();
	    return CEIL_DIV(end-start,NS_PER_MS);
	}
    }
    return 0;
}

int sys_sem_valid(sys_sem_t *sem)
{
    DEBUG("Is sem %p valid ? sem->sem=%p\n", sem, sem? sem->sem : 0);
    int rc = (sem && sem->sem);
    DEBUG("sem returning %d\n",rc);
    return rc;
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
    DEBUG("Sem set invalid %p\n",sem);
    if (sem->sem) {
	nk_semaphore_release(sem->sem);
	sem->sem = 0;
    }
}

u32_t sys_now()
{
    u64_t now_time = CEIL_DIV(nk_sched_get_realtime(),NS_PER_MS);
    DEBUG("sys now %u ms\n",now_time-start_time);
    return now_time-start_time;
}

err_t sys_mutex_new(sys_mutex_t *mutex)
{
    DEBUG("Mutex new %p\n",mutex);
    return sys_sem_new(mutex, 1);
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
    DEBUG("Mutex lock %p\n",mutex);
    sys_arch_sem_wait(mutex, 0);
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
    DEBUG("Mutex signal %p\n",mutex);
    sys_sem_signal(mutex);
}

void sys_mutex_free(sys_mutex_t *mutex)
{
    DEBUG("Mutex free %p\n",mutex);
    sys_sem_free(mutex);
}

int sys_mutex_valid(sys_mutex_t *mutex)
{
    DEBUG("Is mu %p valid\n",mutex);
    int rc=sys_sem_valid(mutex);
    DEBUG("mu returning %d\n",rc);
    return rc;
}

void sys_mutex_set_invalid(sys_mutex_t *mutex)
{
    DEBUG("Mutex set invalid %p\n",mutex);
    sys_sem_set_invalid(mutex);
}


int sys_mbox_valid(sys_mbox_t *mbox)
{
    DEBUG("Is mb %p valid ? mb->mq=%p\n", mbox, mbox ? mbox->mq : 0);

    int rc = (mbox && mbox->mq);
    DEBUG("mb returning %d\n",rc);
    return rc;
}

err_t sys_mbox_new(sys_mbox_t *mb, int size)
{
    DEBUG("Mbox new %p size=%d\n",mb,size);

    if (size<=0) {
	size = 256;
	DEBUG("Corrected to %d\n",size);
    }
    
    mb->mq = nk_msg_queue_create(0,size,NK_MSG_QUEUE_DEFAULT,0);

    if(!mb->mq) {
	return ERR_MEM;
    }

    DEBUG("Mbox new %p size=%d succeeded\n",mb,size);

    return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mb)
{
    if(mb==NULL || mb->mq==NULL ) { return; }

    DEBUG("Mbox free %p\n",mb);

    nk_msg_queue_release(mb->mq);

    mb->mq=0;

    return;
}

void sys_mbox_post(sys_mbox_t *mb, void *msg)
{
    DEBUG("Mbox post %p %p\n",mb,msg);
    
    LWIP_ASSERT("invalid mbox", sys_mbox_valid(mb) );

    nk_msg_queue_push(mb->mq,msg);
}


err_t sys_mbox_trypost(sys_mbox_t *mb, void *msg)
{
    DEBUG("Mbox try post %p %p\n",mb,msg);

    LWIP_ASSERT("invalid mbox", sys_mbox_valid(mb) );
    
    if (nk_msg_queue_try_push(mb->mq,msg)) {
	DEBUG("Mbox try post %p %p Failed\n",mb,msg);
	return ERR_MEM;
    } else {
	DEBUG("Mbox try post %p %p Succeeded\n",mb,msg);
	return ERR_OK;
    }
}


u32_t sys_arch_mbox_fetch(sys_mbox_t *mb, void **msg, u32_t timeout)
{
    
    LWIP_ASSERT("invalid mbox", sys_mbox_valid(mb));
    uint64_t start = nk_sched_get_realtime();
    uint64_t end;

    DEBUG("Mbox fetch %p timeout=%u\n",mb,timeout);
    
    if (timeout==0) {
	nk_msg_queue_pull(mb->mq,msg);
	end = nk_sched_get_realtime();
	DEBUG("Mbox fetch %p %p succeeded time=%u\n",mb,*msg, CEIL_DIV(end-start,NS_PER_MS));
	return CEIL_DIV(end-start,NS_PER_MS);
    } else {
	int flag = nk_msg_queue_pull_timeout(mb->mq,msg,timeout*NS_PER_MS);
	end = nk_sched_get_realtime();
	if (flag>0) {
	    DEBUG("Mbox fetch %p timed out\n",mb);
	    return SYS_ARCH_TIMEOUT;
	} else if (flag==0) {
	    DEBUG("Mbox fetch %p %p succeeded time=%u\n",mb,*msg, CEIL_DIV(end-start,NS_PER_MS));
	    return CEIL_DIV(end-start,NS_PER_MS);
	} else {
	    ERROR("Unexpected mbox failure\n");
	    return SYS_ARCH_TIMEOUT;
	}
    }
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mb, void **msg)
{
    uint64_t start = nk_sched_get_realtime();
    uint64_t end;

    DEBUG("Mbox try fetch %p\n",mb);
    
    LWIP_ASSERT("invalid mbox", sys_mbox_valid(mb));
    
    if (nk_msg_queue_try_pull(mb->mq,msg)) {
	DEBUG("Mbox try fetch %p failed\n",mb);
	return SYS_MBOX_EMPTY;
    } else {
	end = nk_sched_get_realtime();
	DEBUG("Mbox try fetch %p %p succeeded\n",mb,*msg);
	return CEIL_DIV(end-start,NS_PER_MS);;
    }
}

void sys_mbox_set_invalid(sys_mbox_t *mb)
{
    DEBUG("Mbox set invalid %p\n",mb);
    if (mb->mq) { 
	nk_msg_queue_release(mb->mq);
	mb->mq=0;
    }
}

sys_thread_t sys_thread_new(char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
    DEBUG("Thread new\n");	
    nk_thread_id_t tid=0;
    int code = nk_thread_start((nk_thread_fun_t)thread, arg, NULL, 1, stacksize, &tid, CPU_ANY);
    if (code!=0) {
	ERROR("Failed to start thread\n");
	return 0;
    }
    code = nk_thread_name(tid, name);
    if (code!=0) {
	ERROR("Failed to name thread\n");
	return 0;
    }
    DEBUG("Thread new success - TID = %x\n", tid);
    return tid;
}


static spinlock_t big_giant_lock=0;

sys_prot_t sys_arch_protect(void)
{
    //DEBUG("Protect\n");
    return (sys_prot_t) (uint64_t) spin_lock_irq_save(&big_giant_lock);
    //return 0;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    spin_unlock_irq_restore(&big_giant_lock, (uint8_t) (uint64_t) pval);
    //DEBUG("Unprotect\n");
}

//void sys_msleep(u32_t ms)
//{
//    nk_sleep(ms*NS_PER_MS);
//}
