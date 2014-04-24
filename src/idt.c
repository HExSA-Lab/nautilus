#include <printk.h>
#include <idt.h>
#include <intrinsics.h>
#include <string.h>
#include <paging.h>

extern ulong_t excp_handler_table[NUM_EXCEPTIONS];

struct gate_desc64 idt64[NUM_IDT_ENTRIES] __page_align;

static struct idt_desc idt_descriptor =
{
    .base_addr = (uint64_t)&idt64,
    .size      = (NUM_IDT_ENTRIES<<4)-1,
};
        

int 
null_excp_handler ()
{
    panic("unhandled exception!\n");
    return 0;
}


int 
setup_idt (void)
{
    uint_t i;

    // clear the IDT out
    memset(&idt64, 0, sizeof(struct gate_desc64) * NUM_IDT_ENTRIES);

    for (i = 0; i < NUM_EXCEPTIONS; i++) {
            set_intr_gate(idt64, i, &early_excp_handlers[i]);
    }

    excp_handler_table[PF_EXCP] = (ulong_t)pf_handler;

    lidt(&idt_descriptor);

    return 0;
}

