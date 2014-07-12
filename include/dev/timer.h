#ifndef __TIMER_H__
#define __TIMER_H__

#define TIMER_IRQ 0
#define NUM_TIMERS 1

#define TEVENT_WAITING 0
#define TEVENT_READY   1

struct naut_info;

struct timer_event {
    uint8_t active;
    uint32_t ticks;
    volatile uint8_t event_flag;
};


int timer_init (struct naut_info * naut);
void sleep (uint_t msec);

#endif
