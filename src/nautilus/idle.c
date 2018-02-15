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
#include <nautilus/nautilus.h>
#include <nautilus/idle.h>
#include <nautilus/cpu.h>
#include <nautilus/irq.h>
#include <nautilus/thread.h>
#include <nautilus/task.h>
#include <nautilus/scheduler.h>

#ifndef NAUT_CONFIG_DEBUG_SCHED
#undef DEBUG_PRINT
#define DEBUG_PRINT(...) 
#endif


static inline void 
idle_delay (unsigned long long n)
{
    int i = 0;
    while (--n) {
        i += 1;
        asm volatile ("":::"memory");
    }

    /* force compiler to emit */
    n = n + i;
}


void 
idle (void * in, void ** out)
{
    get_cur_thread()->is_idle = 1;

    struct nk_task *task;

    uint64_t last_steal = nk_sched_get_runtime(get_cur_thread());
    uint64_t runtime;
    uint64_t numstolen;

    while (1) {
	if (!irqs_enabled()) { 
	    panic("Idle running with interrupts off!");
	    return;
	}


#if NAUT_CONFIG_TASK_IN_IDLE
	// consume our own tasks until there are none left
	// we need to assure that if we consume a task, we finish it
	// hence the preemption disable
	do {
#if NAUT_CONFIG_TASK_IN_IDLE_NOPREEMPT
	    preempt_disable();
#endif
	    if ((task = nk_task_try_consume(my_cpu_id(),0,0))) { 
		DEBUG_PRINT("idle consuming task %p\n",task);
		void *output = task->func(task->input);
		nk_task_complete(task, output);
	    }
#if NAUT_CONFIG_TASK_IN_IDLE_NOPREEMPT
	    preempt_enable();
#endif
	} while (task);
#endif
	
#if NAUT_CONFIG_WORK_STEALING
	runtime = nk_sched_get_runtime(get_cur_thread());
	if ((runtime - last_steal) > (NAUT_CONFIG_WORK_STEALING_INTERVAL_MS*1000000ULL)) {
	    preempt_disable();
	    DEBUG_PRINT("CPU %d trying to steal\n",my_cpu_id());
	    nk_sched_cpu_mug(-1,NAUT_CONFIG_WORK_STEALING_AMOUNT,&numstolen);
	    DEBUG_PRINT("CPU %d stole %lu threads\n",my_cpu_id(),numstolen);
	    last_steal = runtime;
	    preempt_enable();
	}
#endif
	    

        nk_yield();

#ifdef NAUT_CONFIG_XEON_PHI
        udelay(1);
#else
        idle_delay(100);
#endif

#ifdef NAUT_CONFIG_HALT_WHILE_IDLE
        sti();
        halt();
#endif
    }
}



