#include <nautilus.h>
#include <irq.h>
#include <cpu.h>
#include <percpu.h>
#include <math.h>

#include <stddef.h>

#include <dev/timer.h>
#include <lib/liballoc.h>

#define PIT_RATE 1193182ul
#define MS      10
#define LATCH   (PIT_RATE / (1000 / MS))
#define LOOPS   1000


/* TODO: get rid of this */
static struct naut_info * timer_naut;

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
    struct sys_info  * sys = per_cpu_get(system);
    int i;
    struct timer_event * te = sys->time_events;

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

/* KCH: below taken from linux */
static unsigned long 
calibrate_tsc (void)
{
    uint32_t latch = LATCH;
    ulong_t ms = MS;
    int loopmin = LOOPS;
    uint64_t tsc, t1, t2, delta;
	unsigned long tscmin, tscmax;
	int pitcnt;

	/* Set the Gate high, disable speaker */
	outb((inb(0x61) & ~0x02) | 0x01, 0x61);

	/*
	 * Setup CTC channel 2* for mode 0, (interrupt on terminal
	 * count mode), binary count. Set the latch register to 50ms
	 * (LSB then MSB) to begin countdown.
	 */
	outb(0xb0, 0x43);
	outb(latch & 0xff, 0x42);
	outb(latch >> 8, 0x42);

	tsc = t1 = t2 = rdtsc();

	pitcnt = 0;
	tscmax = 0;
	tscmin = ULONG_MAX;
	while ((inb(0x61) & 0x20) == 0) {
		t2 = rdtsc();
		delta = t2 - tsc;
		tsc = t2;
		if ((unsigned long) delta < tscmin)
			tscmin = (unsigned int) delta;
		if ((unsigned long) delta > tscmax)
			tscmax = (unsigned int) delta;
		pitcnt++;
	}

	/*
	 * Sanity checks:
	 *
	 * If we were not able to read the PIT more than loopmin
	 * times, then we have been hit by a massive SMI
	 *
	 * If the maximum is 10 times larger than the minimum,
	 * then we got hit by an SMI as well.
	 */
	if (pitcnt < loopmin) {
        return ULONG_MAX;
    }
        
    /*
    if (tscmax > 10 * tscmin) {
        return ULONG_MAX;
    }
    */

	/* Calculate the PIT value */
	delta = t2 - t1;
	return delta / ms;
}


static void
detect_cpu_freq (void) 
{
    uint8_t flags = irq_disable_save();
    ulong_t khz = calibrate_tsc();
    if (khz == ULONG_MAX) {
        printk("Unable to detect CPU frequency\n");
        goto out;
    }
    printk("CPU frequency detected as %lu.%03lu MHz\n", (ulong_t)khz / 1000, (ulong_t)khz % 1000);

out:
    irq_enable_restore(flags);
}


int 
timer_init (struct naut_info * naut)
{
    uint16_t hz;

    detect_cpu_freq();

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


