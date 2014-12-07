#include <nautilus/nautilus.h>
#include <nautilus/idt.h>
#include <nautilus/intrinsics.h>
#include <nautilus/naut_string.h>
#include <nautilus/paging.h>
#include <nautilus/percpu.h>
#include <nautilus/cpu.h>
#include <nautilus/thread.h>

extern ulong_t handler_table[NUM_IDT_ENTRIES];

struct gate_desc64 idt64[NUM_IDT_ENTRIES] __page_align;

#define EXCP_NAME 0
#define EXCP_MNEMONIC 1
const char * excp_codes[NUM_EXCEPTIONS][2] = {
    {"Divide By Zero",           "#DE"},
    {"Debug",                    "#DB"},
    {"Non-maskable Interrupt",   "N/A"},
    {"Breakpoint Exception",     "#BP"},
    {"Overflow Exception",       "#OF"},
    {"Bound Range Exceeded",     "#BR"},
    {"Invalid Opcode",           "#UD"},
    {"Device Not Available",     "#NM"},
    {"Double Fault",             "#DF"},
    {"Coproc Segment Overrun",   "N/A"},
    {"Invalid TSS",              "#TS"},
    {"Segment Not Present",      "#NP"},
    {"Stack Segment Fault",      "#SS"},
    {"General Protection Fault", "#GP"},
    {"Page Fault",               "#PF"},
    {"Reserved",                 "N/A"},
    {"x86 FP Exception",         "#MF"},
    {"Alignment Check",          "#AC"},
    {"Machine Check",            "#MC"},
    {"SIMD FP Exception",        "#XM"},
    {"Virtualization Exception", "#VE"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Security Exception",       "#SX"},
    {"Reserved",                 "N/A"},
};


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
    printk("\n+++ UNHANDLED EXCEPTION +++\n");
    if (vector >= 0 && vector < 32) {
        printk("[%s] <%s>\n    RIP=%p    fault addr=%p    (core=%u, thread=%u)\n", 
                excp_codes[vector][EXCP_NAME],
                excp_codes[vector][EXCP_MNEMONIC],
                (void*)excp->rip, 
                (void*)fault_addr, 
                my_cpu_id(), get_cur_thread()->tid);
    } else {
        printk("[Unknown Exception] (vector=0x%x)\n    RIP=(%p)    fault addr=%p    (core=%u)\n", 
                vector,
                (void*)excp->rip,
                (void*)fault_addr,
                my_cpu_id());
    }

    panic("+++ HALTING +++\n");
    return 0;
}


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

    if (idt_assign_entry(PF_EXCP, (ulong_t)nk_pf_handler) < 0) {
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

