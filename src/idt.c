#include <printk.h>
#include <idt.h>
#include <intrinsics.h>
#include <string.h>

struct gate_desc64 idt64[NUM_IDT_ENTRIES] __page_align;

static struct idt_desc idt_descriptor =
{
    .base_addr = (uint64_t)&idt64,
    .size      = (NUM_IDT_ENTRIES<<4)-1,
};
        

static int
test_pf_handler (void) {
    panic("PAGE FAULT!!!!\n");
    return 0;
}


static int 
null_int_handler (void) {
    /* do nothing */
    panic("null handler\n");
    return 0;
}


int 
setup_idt (void)
{
    uint_t i;

    // clear the IDT out
    memset(&idt64, 0, sizeof(struct gate_desc64) * NUM_IDT_ENTRIES);

    for (i = 0; i < NUM_EXCEPTIONS; i++) {
        if (i == 14) {
            set_intr_gate(idt64, i, test_pf_handler);
        } else {
            set_intr_gate(idt64, i, null_int_handler);
        }
    }

    lidt(&idt_descriptor);

    return 0;
}

