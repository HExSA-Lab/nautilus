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
#include <nautilus/printk.h>
#include <nautilus/hashtable.h>
#include <nautilus/naut_string.h>
#include <nautilus/percpu.h>
#include <nautilus/atomic.h>
#include <nautilus/mm.h>
#include <nautilus/libccompat.h>
#include <nautilus/shell.h>
#include <nautilus/irq.h>

#include <nautilus/instrument.h>


#define INFO(fmt, args...) INFO_PRINT("instrument: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("instrument: " fmt, ##args)

#ifdef NAUT_CONFIG_DEBUG_PROFILE
#define DEBUG(fmt, args...) DEBUG_PRINT("instrument: " fmt, ##args)
#else
#define DEBUG(fmt, args...) 
#endif


static uint8_t instr_active = 0;
static uint64_t instr_start_count = 0;
static uint64_t instr_end_count = 0;

struct func_data {
    uint64_t call_count;
    uint64_t avg_nsec;
    uint64_t max_nsec;
    uint64_t min_nsec;
    uint64_t start_count;
    uint64_t total_count;
};


static unsigned 
instr_hash_fn(addr_t key) {
    char * name = (char *)key;
    return nk_hash_buffer((uint8_t *)name, strlen(name));
}


static int 
instr_eq_fn (addr_t key1, addr_t key2) {
    char * name1 = (char *)key1;
    char * name2 = (char *)key2;

    return (strcmp(name1, name2) == 0);
}


static void 
instr_calibrate (void)
{
    NK_PROFILE_ENTRY();

    NK_PROFILE_EXIT();
}


void 
nk_profile_func_enter (const char  *func)
{
    struct func_data * new = NULL;
    struct timespec ts;

    if (!instr_active) {
        return;
    }

    if (!(per_cpu_get(instr_data)->func_htable)) {
        return;
    }

    new = (struct func_data*)nk_htable_search(per_cpu_get(instr_data)->func_htable, (addr_t)(func));

    if (!new) {

        new = malloc(sizeof(struct func_data));
        memset(new, 0, sizeof(struct func_data));

        new->call_count  = 1;
        new->min_nsec = ULONG_MAX;

        nk_htable_insert(per_cpu_get(instr_data)->func_htable, (addr_t)func, (addr_t)new);
    } else {
        new->call_count++;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);
    new->start_count = ts.tv_sec * 1000000000 + ts.tv_nsec;
}


void 
nk_profile_func_exit (const char *func)
{
    struct timespec te;
    struct func_data * data = NULL;
    uint64_t end;
    uint64_t time;

    if (!instr_active) {
        return;
    }

    if (!(per_cpu_get(instr_data)->func_htable)) {
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &te);

    data = (struct func_data*)nk_htable_search(per_cpu_get(instr_data)->func_htable, (addr_t)func);

    if (!data) {
        return;
    } else {
        end = 1000000000 * te.tv_sec + te.tv_nsec;
        if (end < data->start_count) {
            return;
        }
        time = end - data->start_count;
        data->total_count += time;
        if (time < data->min_nsec) {
            data->min_nsec = time;
        }
        if (time > data->max_nsec) {
            data->max_nsec = time;
        }
        data->avg_nsec = (((data->avg_nsec * (data->call_count - 1)) + time )/ data->call_count);
    }
}


void 
nk_instrument_clear (void) 
{
    int i;

    DEBUG("Clearing Instrumentation\n");

    uint8_t flags = irq_disable_save();
    
    for (i = 0; i < nk_get_nautilus_info()->sys.num_cpus; i++) {
        struct cpu * this_cpu = nk_get_nautilus_info()->sys.cpus[i];
        int flags2 = spin_lock_irq_save(&this_cpu->lock);
        if (!this_cpu) {
            ERROR("Could not get CPU\n");
            spin_unlock_irq_restore(&this_cpu->lock, flags2);
            return;
        }

	if (!this_cpu->instr_data) { 
	    // first time

	    this_cpu->instr_data = malloc(sizeof(struct nk_instr_data));
	    memset(this_cpu->instr_data, 0, sizeof(struct nk_instr_data));
	    this_cpu->instr_data->mallocstat.min_latency = ULONG_MAX;
	    this_cpu->instr_data->freestat.min_latency = ULONG_MAX;
	    this_cpu->instr_data->irqstat.min_latency    = ULONG_MAX;
	    this_cpu->instr_data->thr_switch.min_latency = ULONG_MAX;

	    this_cpu->instr_data->func_htable = nk_create_htable(0, instr_hash_fn, instr_eq_fn);
	    if (!this_cpu->instr_data->func_htable) {
		ERROR("Could not create instrumentation hash table for core %u\n",
		      i);
		spin_unlock_irq_restore(&this_cpu->lock, flags2);
		return;
	    }
	    
	} else {
	    // reinit
	    memset(&this_cpu->instr_data->mallocstat, 0, sizeof(struct malloc_data));
	    memset(&this_cpu->instr_data->freestat, 0, sizeof(struct free_data));
	    memset(&this_cpu->instr_data->irqstat,0,sizeof(struct irq_data));
	    memset(&this_cpu->instr_data->thr_switch,0,sizeof(struct thread_switch_data));
	    this_cpu->instr_data->mallocstat.min_latency = ULONG_MAX;
	    this_cpu->instr_data->freestat.min_latency = ULONG_MAX;
	    this_cpu->instr_data->irqstat.min_latency    = ULONG_MAX;
	    this_cpu->instr_data->thr_switch.min_latency = ULONG_MAX;
	    
	    // blow away htable and delete values (keys are just func addresses, so not freed)
	    nk_free_htable(this_cpu->instr_data->func_htable, 1, 0);

	    this_cpu->instr_data->func_htable = nk_create_htable(0, instr_hash_fn, instr_eq_fn);

	    if (!this_cpu->instr_data->func_htable) {
		ERROR("Could not create instrumentation hash table for core %u\n",
		      i);
		spin_unlock_irq_restore(&this_cpu->lock, flags2);
		return;
	    }
	}
	spin_unlock_irq_restore(&this_cpu->lock, flags2);
    }

    irq_enable_restore(flags);
}

/* TODO: calibrate and subtract the overhead of instrumentation */
void 
nk_instrument_init (void) 
{
    nk_instrument_clear();
    INFO("inited\n");
}

void
nk_instrument_start (void)
{
    struct timespec ts;
    DEBUG("Beginning Instrumentation\n");
    clock_gettime(CLOCK_MONOTONIC, &ts);
    instr_start_count = 1000000000 * ts.tv_sec + ts.tv_nsec;
    atomic_cmpswap(instr_active, 0, 1);
}

void 
nk_instrument_end (void) 
{
    struct timespec te;
    clock_gettime(CLOCK_MONOTONIC, &te);
    instr_end_count = 1000000000 * te.tv_sec + te.tv_nsec;
    DEBUG("Deactivating instrumentation\n");
    atomic_cmpswap(instr_active, 1, 0);
}

void
nk_malloc_enter (void)
{
    struct malloc_data * md = NULL;
    struct timespec ts;

    if (!instr_active) {
        return;
    }

    md = &(per_cpu_get(instr_data)->mallocstat);
    md->count++;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    md->start_count = 1000000000 * ts.tv_sec + ts.tv_nsec;
}

void
nk_malloc_exit (void)
{
    struct malloc_data * md = NULL;
    struct timespec te;
    uint64_t end;
    uint64_t time;

    if (!instr_active) {
        return;
    }

    md = &(per_cpu_get(instr_data)->mallocstat);
    
    clock_gettime(CLOCK_MONOTONIC, &te);

    end = 1000000000 * te.tv_sec + te.tv_nsec;
    if (end < md->start_count) {
        return;
    }
    time = end - md->start_count;
    if (time < md->min_latency) {
        md->min_latency = time;
    }
    if (time > md->max_latency) {
        md->max_latency = time;
    }
    md->avg_latency = (((md->avg_latency * (md->count - 1)) + time)/ md->count);
}

void
nk_free_enter (void)
{
    struct free_data * md = NULL;
    struct timespec ts;

    if (!instr_active) {
        return;
    }

    md = &(per_cpu_get(instr_data)->freestat);
    md->count++;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    md->start_count = 1000000000 * ts.tv_sec + ts.tv_nsec;
}

void
nk_free_exit (void)
{
    struct free_data * md = NULL;
    struct timespec te;
    uint64_t end;
    uint64_t time;

    if (!instr_active) {
        return;
    }

    md = &(per_cpu_get(instr_data)->freestat);
    
    clock_gettime(CLOCK_MONOTONIC, &te);

    end = 1000000000 * te.tv_sec + te.tv_nsec;
    if (end < md->start_count) {
        return;
    }
    time = end - md->start_count;
    if (time < md->min_latency) {
        md->min_latency = time;
    }
    if (time > md->max_latency) {
        md->max_latency = time;
    }
    md->avg_latency = (((md->avg_latency * (md->count - 1)) + time)/ md->count);
}


void
nk_irq_prof_enter (void)
{
    struct irq_data * irq = NULL;
    struct timespec ts;

    if (!instr_active) {
        return;
    }

    irq = &(per_cpu_get(instr_data)->irqstat);
    irq->count++;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    irq->start_count = 1000000000 * ts.tv_sec + ts.tv_nsec;
}


void
nk_irq_prof_exit (void)
{
    struct irq_data * irq = NULL;
    struct timespec te;
    uint64_t end;
    uint64_t time;

    if (!instr_active) {
        return;
    }

    irq = &(per_cpu_get(instr_data)->irqstat);
    
    clock_gettime(CLOCK_MONOTONIC, &te);

    end = 1000000000 * te.tv_sec + te.tv_nsec;
    if (end < irq->start_count) {
        return;
    }
    time = end - irq->start_count;
    if (time < irq->min_latency) {
        irq->min_latency = time;
    }
    if (time > irq->max_latency) {
        irq->max_latency = time;
    }
    irq->avg_latency = (((irq->avg_latency * (irq->count - 1)) + time)/ irq->count);
}


void
nk_thr_switch_prof_enter (void)
{
    struct thread_switch_data * thr = NULL;
    struct timespec ts;

    if (!instr_active) {
        return;
    }

    thr = &(per_cpu_get(instr_data)->thr_switch);
    thr->count++;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    thr->start_count = 1000000000 * ts.tv_sec + ts.tv_nsec;
}


void
nk_thr_switch_prof_exit (void)
{
    struct thread_switch_data * thr = NULL;
    struct timespec te;
    uint64_t end;
    uint64_t time;

    if (!instr_active) {
        return;
    }

    thr = &(per_cpu_get(instr_data)->thr_switch);

    if (thr->count == 0) {
        return;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &te);

    end = 1000000000 * te.tv_sec + te.tv_nsec;
    if (end < thr->start_count) {
        return;
    }
    time = end - thr->start_count;
    if (time < thr->min_latency) {
        thr->min_latency = time;
    }
    if (time > thr->max_latency) {
        thr->max_latency = time;
    }
    thr->avg_latency = (((thr->avg_latency * (thr->count - 1)) + time)/ thr->count);
}


void 
nk_instrument_query (void)
{
    int i;

    printk("Dumping instrumentation data...\n");
    for (i = 0; i < nk_get_nautilus_info()->sys.num_cpus; i++) {
        struct cpu * this_cpu = nk_get_nautilus_info()->sys.cpus[i];


        printk("Function Table Stats for Core %u:\n", i);
        struct nk_hashtable_iter * iter = nk_create_htable_iter(this_cpu->instr_data->func_htable);
        if (!iter) {
            ERROR("Could not iterate over function htable on core %u\n", i);
            continue;
        }

        do {
            char * func = (void*)nk_htable_get_iter_key(iter);
            struct func_data * data = (struct func_data*)nk_htable_get_iter_value(iter);

            uint64_t total_nsec = (instr_end_count - instr_start_count);

            if (data->call_count > 0) {
                printk("\t%lu.%lu\% Func: %s\n\tCount: %16u Lat - Avg: %16llunsec Max: %16llunsec Min: %16llunsec\n", 
                        data->total_count / total_nsec,
                        data->total_count % total_nsec,
                        func,
                        data->call_count,
                        data->avg_nsec,
                        data->max_nsec,
                        data->min_nsec);
            }


        } while (nk_htable_iter_advance(iter) != 0);

        printk("Malloc Stats for Core %u:\n", i);

        /* print malloc data */
        printk("\tCount: %16u Lat - Avg: %16llunsec, Max: %16llunsec Min: %16llunsec\n", 
                this_cpu->instr_data->mallocstat.count,
                this_cpu->instr_data->mallocstat.avg_latency,
                this_cpu->instr_data->mallocstat.max_latency,
                this_cpu->instr_data->mallocstat.min_latency);

        printk("Free Stats for Core %u:\n", i);

        /* print free data */
        printk("\tCount: %16u Lat - Avg: %16llunsec, Max: %16llunsec Min: %16llunsec\n", 
                this_cpu->instr_data->freestat.count,
                this_cpu->instr_data->freestat.avg_latency,
                this_cpu->instr_data->freestat.max_latency,
                this_cpu->instr_data->freestat.min_latency);

        printk("IRQ Stats for Core %u:\n", i);
        printk("\tCount: %16u Lat - Avg: %16llunsec Max: %16llunsec Min: %16llunsec\n", 
                this_cpu->instr_data->irqstat.count,
                this_cpu->instr_data->irqstat.avg_latency,
                this_cpu->instr_data->irqstat.max_latency,
                this_cpu->instr_data->irqstat.min_latency);
        printk("Thread Switch Stats for Core %u:\n", i);
        printk("\tCount: %16u Lat - Avg: %16llunsec Max: %16llunsec Min: %16llunsec\n", 
                
                this_cpu->instr_data->thr_switch.count,
                this_cpu->instr_data->thr_switch.avg_latency,
                this_cpu->instr_data->thr_switch.max_latency,
                this_cpu->instr_data->thr_switch.min_latency);

    }
}


void
nk_instrument_calibrate (unsigned loops)
{
    int i; 
    for (i = 0; i < loops; i++) {
        instr_calibrate();
    }
}


static int
handle_shell_instr (char * buf, void * priv)
{
    char what[80];

    if (sscanf(buf,"inst%s* %s", what)==1) { 
        if (!strncasecmp(what,"sta",3)) {
            nk_vc_printf("starting instrumentation\n");
            nk_instrument_start();
            return 0;
        } else if (!strncasecmp(what,"e",1) || !strncasecmp(what,"sto",3)) {
            nk_vc_printf("ending instrumentation\n");
            nk_instrument_end();
            return 0;
        } else if (!strncasecmp(what,"c",1)) {
            nk_vc_printf("clearing instrumentation\n");
            nk_instrument_clear();
            return 0;
        } else if (!strncasecmp(what,"q",1)) {
            nk_vc_printf("querying instrumentation\n");
            nk_instrument_query();
            return 0;
        } 
    }

    nk_vc_printf("unknown instrumentation request\n");

    return 0;
}


static struct shell_cmd_impl instr_impl = {
    .cmd      = "instr",
    .help_str = "instrument start|end/stop|clear|query",
    .handler  = handle_shell_instr,
};
nk_register_shell_cmd(instr_impl);
