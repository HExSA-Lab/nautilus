#ifndef __TIMER_H__
#define __TIMER_H__

#define NUM_TIMERS 1

#define TEVENT_WAITING 0
#define TEVENT_READY   1

struct naut_info;

struct nk_timer_event {
    uint8_t active;
    uint32_t ticks;
    volatile uint8_t event_flag;
};

#include <nautilus/idt.h>
int nk_timer_handler(excp_entry_t *, excp_vec_t);
int nk_timer_init (struct naut_info * naut);
void nk_sleep (uint_t msec);

#endif
