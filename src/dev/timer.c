#include <nautilus.h>
#include <irq.h>
#include <dev/timer.h>


#define HZ 100

static int
timer_handler (excp_entry_t * excp, excp_vec_t vec)
{
    printk("Timer Interrupt!\n");
    IRQ_HANDLER_END();
    return 0;
}


int 
timer_init (struct naut_info * naut)
{
    uint16_t foo = 1193182L / HZ;
    printk("Initializing 8254 PIT\n");

    outb(0x36, 0x43);         // channel 0, LSB/MSB, mode 3, binary
    outb(foo & 0xff, 0x40);   // LSB
    outb(foo >> 8, 0x40);     // MSB

    if (register_irq_handler(TIMER_IRQ, timer_handler) < 0) {
        ERROR_PRINT("Could not register timer interrupt handler\n");
        return -1;
    }

    return 0;
}

