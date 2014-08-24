#include <nautilus.h>
#include <idt.h>
#include <intrinsics.h>
#include <string.h>
#include <paging.h>

extern ulong_t handler_table[NUM_IDT_ENTRIES];

struct gate_desc64 idt64[NUM_IDT_ENTRIES] __page_align;

struct idt_desc idt_descriptor =
{
    .base_addr = (uint64_t)&idt64,
    .size      = (NUM_IDT_ENTRIES<<4)-1,
};
        

int 
null_excp_handler (excp_entry_t * excp,
                   excp_vec_t vector,
                   addr_t fault_addr)
{
    panic("unhandled exception, (v=0x%x), addr=(%p), last_addr=(%p)\n", vector,(void*)excp->rip, (void*)fault_addr);
    return 0;
}


/* TODO: differentiate these!!! */
int
null_irq_handler (excp_entry_t * excp,
                  excp_vec_t vector)
{
    panic("unhandled irq, (v=0x%x), addr=(%p)\n", vector,(void*)excp->rip);
    return 0;

}


static int
df_handler (excp_entry_t * excp,
            excp_vec_t vector,
            addr_t unused)
{
    panic("DOUBLE FAULT. Dying.\n");
    return 0;
}


int
idt_assign_entry (ulong_t entry, ulong_t handler_addr)
{

    if (entry >= NUM_IDT_ENTRIES) {
        ERROR_PRINT("Assigning invalid IDT entry\n");
        return -1;
    }

    if (!handler_addr) {
        ERROR_PRINT("attempt to assign null handler\n");
        return -1;
    }

    handler_table[entry] = handler_addr;

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

    for (i = 32; i < NUM_IDT_ENTRIES; i++) {
        set_intr_gate(idt64, i, &early_irq_handlers[i-32]);
        //set_intr_gate(idt64, i, &early_excp_handlers[i]);
    }

    if (idt_assign_entry(PF_EXCP, (ulong_t)pf_handler) < 0) {
        ERROR_PRINT("Couldn't assign page fault handler\n");
        return -1;
    }

    if (idt_assign_entry(DF_EXCP, (ulong_t)df_handler) < 0) {
        ERROR_PRINT("Couldn't assign double fault handler\n");
        return -1;
    }

    lidt(&idt_descriptor);

    return 0;
}

