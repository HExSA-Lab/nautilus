#include <nautilus.h>
#include <irq.h>
#include <cpu.h>

#include <dev/timer.h>

#include <lib/liballoc.h>

/* TODO: FIX THIS, (priv data for irq handlers) */
static struct naut_info * timer_naut = NULL;


static inline void 
timer_init_event (struct timer_event * t, uint_t delay)
{
    t->ticks  = delay;
    t->active = 1;
}


static inline void
timer_clear_event (struct timer_event * t)
{
    t->ticks  = 0;
    t->active = 0;
}


static void 
timer_wait_event (struct timer_event * te)
{
    te->event_flag = TEVENT_WAITING;

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
timer_notify_event (struct timer_event * te) 
{
    while (!__sync_bool_compare_and_swap(&te->event_flag, 
                                     TEVENT_WAITING,
                                     TEVENT_READY));
}

static int
timer_handler (excp_entry_t * excp, excp_vec_t vec)
{
    int i;
    struct naut_info * naut = timer_naut;
    struct timer_event * te = naut->sys.time_events;

    for (i = 0; i < naut->sys.num_tevents; i++) {
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


static struct timer_event * 
find_avail_timer_event (struct naut_info * naut)
{
    int i;
    for (i = 0; i < naut->sys.num_tevents; i++) {
        if (!naut->sys.time_events[i].active) {
            return &(naut->sys.time_events[i]);
        }
    }

    return NULL;
}


void 
sleep (uint_t msec) 
{
    struct timer_event * t;
    if ((t = find_avail_timer_event(timer_naut)) == NULL) {
        return;
    }

    timer_init_event(t, msec);
    timer_wait_event(t);
    timer_clear_event(t);
}


int 
timer_init (struct naut_info * naut)
{
    uint16_t hz;

    naut->sys.num_tevents = NUM_TIMERS;
    naut->sys.time_events = malloc(NUM_TIMERS*sizeof(struct timer_event));
    if (!naut->sys.time_events) {
        ERROR_PRINT("Could not allocate timers\n");
        return -1;
    }
    memset(naut->sys.time_events, 0, NUM_TIMERS*sizeof(struct timer_event));

    hz = 1193182UL / NAUT_CONFIG_HZ;

    printk("Initializing 8254 PIT\n");

    outb(0x36, 0x43);         // channel 0, LSB/MSB, mode 3, binary
    outb(hz & 0xff, 0x40);   // LSB
    outb(hz >> 8, 0x40);     // MSB

    if (register_irq_handler(TIMER_IRQ, timer_handler, NULL) < 0) {
        ERROR_PRINT("Could not register timer interrupt handler\n");
        return -1;
    }

    timer_naut = naut;

    return 0;
}

