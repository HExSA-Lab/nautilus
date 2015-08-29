#include <nautilus/nautilus.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/percpu.h>
#include <nautilus/mm.h>

#include <stddef.h>

#include <dev/timer.h>

#ifndef NAUT_CONFIG_DEBUG_TIMERS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif


static inline void 
timer_init_event (struct nk_timer_event * t, uint_t delay)
{
    t->ticks  = delay;
    t->active = 1;
}


static inline void
timer_clear_event (struct nk_timer_event * t)
{
    t->ticks  = 0;
    t->active = 0;
}


static void 
timer_wait_event (struct nk_timer_event * te)
{
    te->event_flag = TEVENT_WAITING;

    /* TODO: use a wait queue here */
    while (1) {
        cli();
        if (te->event_flag == TEVENT_READY) {
            sti();
            break;
        }
        sti();
        asm volatile ("pause");
    }
}


static void
timer_notify_event (struct nk_timer_event * te) 
{
    DEBUG_PRINT("Notifying timer event (%p)\n", (void*)te);
    while (!__sync_bool_compare_and_swap(&te->event_flag, 
                                     TEVENT_WAITING,
                                     TEVENT_READY));
}


int
nk_timer_handler (excp_entry_t * excp, excp_vec_t vec)
{

    struct sys_info  * sys = per_cpu_get(system);
    int i;
    struct nk_timer_event * te = sys->time_events;

    for (i = 0; i < sys->num_tevents; i++) {
        if (te[i].active && te[i].ticks > 0) {
            te[i].ticks--;
            if (te[i].ticks == 0) {
                timer_notify_event(&te[i]);
            } 
        }
    }

    IRQ_HANDLER_END();
    return 0;
}


static struct nk_timer_event * 
find_avail_timer_event (void)
{
    uint_t num_tevents = per_cpu_get(system)->num_tevents;
    struct nk_timer_event * time_events = per_cpu_get(system)->time_events;
    int i;

    for (i = 0; i < num_tevents; i++) {
        if (!time_events[i].active) {
            return &(time_events[i]);
        }
    }

    return NULL;
}


void 
nk_sleep (uint_t msec) 
{
    struct nk_timer_event * t;
    if ((t = find_avail_timer_event()) == NULL) {
        return;
    }

    timer_init_event(t, msec);
    timer_wait_event(t);
    timer_clear_event(t);
}


int 
nk_timer_init (struct naut_info * naut)
{
    uint16_t hz;

    naut->sys.num_tevents = NUM_TIMERS;
    naut->sys.time_events = malloc(NUM_TIMERS*sizeof(struct nk_timer_event));
    if (!naut->sys.time_events) {
        ERROR_PRINT("Could not allocate timers\n");
        return -1;
    }

    memset(naut->sys.time_events, 0, NUM_TIMERS*sizeof(struct nk_timer_event));

    return 0;
}

