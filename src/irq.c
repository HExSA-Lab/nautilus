#include <nautilus.h>
#include <idt.h>
#include <irq.h>
#include <cpu.h>


static uint8_t irq_to_vec_map[NUM_EXT_IRQS] = {
    0xec,  // IRQ 0 (timer)
    0xe4,  // IRQ 1
    0xec,  // disabled
    0x94,  // IRQ 3
    0x8c,  // IRQ 4 (COM1)
    0x84,  // IRQ 5
    0x7c,  // IRQ 6
    0x74,  // IRQ 7
    0xd4,  // IRQ 8
    0xcc,  // IRQ 9
    0xc4,  // IRQ 10
    0xbc,  // IRQ 11
    0xb4,  // IRQ 12
    0xac,  // IRQ 13
    0xa4,  // IRQ 14
    0x9c,  // IRQ 15
};

uint8_t 
irqs_enabled (void)
{
    uint64_t rflags = read_rflags();
    return (rflags & RFLAGS_IF) != 0;
}


inline uint8_t
irq_to_vec (uint8_t irq)
{
    return irq_to_vec_map[irq];
}


/* 
 * this should only be used when the OS interrupt vector
 * is known ahead of time, that is, *not* in the case
 * of external interrupts. It's much more likely you
 * should be using register_irq_handler
 */
int 
register_int_handler (uint16_t int_vec, 
                      int (*handler)(excp_entry_t *, excp_vec_t),
                      void * priv_data)
{

    if (!handler) {
        ERROR_PRINT("Attempt to register interrupt %d with invalid handler\n");
        return -1;
    }

    if (int_vec > 0xff) {
        ERROR_PRINT("Attempt to register invalid interrupt(0x%x)\n", int_vec);
        return -1;
    }

    idt_assign_entry(int_vec, (ulong_t)handler);

    return 0;
}


int 
register_irq_handler (uint16_t irq, 
                      int (*handler)(excp_entry_t *, excp_vec_t),
                      void * priv_data)
{
    uint8_t int_vector;

    if (!handler) {
        ERROR_PRINT("Attempt to register IRQ %d with invalid handler\n");
        return -1;
    }

    if (irq > MAX_IRQ_NUM) {
        ERROR_PRINT("Attempt to register invalid IRQ (0x%x)\n", irq);
        return -1;
    }

    int_vector = irq_to_vec(irq);

    idt_assign_entry(int_vector, (ulong_t)handler);

    return 0;
}


void 
disable_8259pic (void)
{
    printk("Disabling legacy 8259 PIC\n");
    outb(0xff, PIC_MASTER_DATA_PORT);
    outb(0xff, PIC_SLAVE_DATA_PORT);
}

void 
imcr_begin_sym_io (void)
{
    /* ICMR */
    outb(0x70, 0x22);
    outb(0x01, 0x23);
}


