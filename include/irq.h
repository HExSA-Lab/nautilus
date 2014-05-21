#ifndef __IRQ_H__
#define __IRQ_H__

#include <nautilus.h>
#include <idt.h>
#include <cpu.h>

#include <dev/apic.h>

#define PIC_MASTER_CMD_PORT  0x20
#define PIC_MASTER_DATA_PORT 0x21
#define PIC_SLAVE_CMD_PORT   0xa0
#define PIC_SLAVE_DATA_PORT  0xa1

#define PIC_MODE_ON (1 << 7)

#define MAX_IRQ_NUM 15
#define NUM_EXT_IRQS   16
#define FIRST_EXT_IRQ  32


#define IRQ_HANDLER_END() apic_do_eoi()


#define enable_irqs()                     \
do  {                                     \
    if (irqs_enabled()) {                 \
         panic("irqs already enabled!\n");\
    }                                     \
    sti();                                \
} while (0)


#define disable_irqs()                     \
do  {                                      \
    if (!irqs_enabled()) {                 \
         panic("irqs already disabled!\n");\
    }                                      \
    cli();                                 \
} while (0)

uint8_t irqs_enabled(void);

static inline uint8_t
atomic_begin_irq (void)
{
    uint8_t enabled = irqs_enabled();

    if (enabled) {
        disable_irqs();
    }

    return enabled;
}
        

static inline void 
atomic_end_irq (uint8_t iflag)
{
    if (irqs_enabled()) {
        panic("interrupts enabled when they shouldn't be!\n");
    }

    if (iflag) {
        /* Interrupts were originally enabled, so turn them back on */
        enable_irqs();
    }
}


uint8_t irq_to_vec (uint8_t irq);
void disable_8259pic(void);
void imcr_begin_sym_io(void);
int register_irq_handler (uint16_t irq, 
                          int (*handler)(excp_entry_t *, excp_vec_t));

#endif
