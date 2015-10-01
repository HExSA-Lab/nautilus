/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/idt.h>
#include <nautilus/intrinsics.h>
#include <nautilus/naut_string.h>
#include <nautilus/paging.h>
#include <nautilus/percpu.h>
#include <nautilus/cpu.h>
#include <nautilus/thread.h>
#include <nautilus/irq.h>
#include <nautilus/backtrace.h>

extern ulong_t handler_table[NUM_IDT_ENTRIES];

struct gate_desc64 idt64[NUM_IDT_ENTRIES] __align(8);

extern uint8_t cpu_info_ready;

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
    .size      = (NUM_IDT_ENTRIES*16)-1,
};
        

int 
null_excp_handler (excp_entry_t * excp,
                   excp_vec_t vector,
                   addr_t fault_addr)
{
    cpu_id_t cpu_id = cpu_info_ready ? my_cpu_id() : 0xffffffff;
    /* TODO: this should be based on scheduler initialization, not CPU */
    unsigned tid = cpu_info_ready ? get_cur_thread()->tid : 0xffffffff;

    printk("\n+++ UNHANDLED EXCEPTION +++\n");

    if (vector < 32) {
        printk("[%s] (0x%x) error=0x%x <%s>\n    RIP=%p      (core=%u, thread=%u)\n", 
                excp_codes[vector][EXCP_NAME],
                vector,
                excp->error_code,
                excp_codes[vector][EXCP_MNEMONIC],
                (void*)excp->rip, 
                cpu_id, tid);
    } else {
        printk("[Unknown Exception] (vector=0x%x)\n    RIP=(%p)     (core=%u)\n", 
                vector,
                (void*)excp->rip,
                cpu_id);
    }

    struct nk_regs * r = (struct nk_regs*)((char*)excp - 128);
    nk_print_regs(r);
    backtrace(r->rbp);

    panic("+++ HALTING +++\n");

    return 0;
}


int
null_irq_handler (excp_entry_t * excp,
                  excp_vec_t vector)
{
    printk("[Unhandled IRQ] (vector=0x%x)\n    RIP=(%p)     (core=%u)\n", 
            vector,
            (void*)excp->rip,
            my_cpu_id());

    struct nk_regs * r = (struct nk_regs*)((char*)excp - 128);
    nk_print_regs(r);
    backtrace(r->rbp);

    panic("+++ HALTING +++\n");
    
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


static int
pic_spur_int_handler (excp_entry_t * excp,
                      excp_vec_t vector,
                      addr_t unused)
{
    WARN_PRINT("Received Spurious interrupt from PIC\n");
    IRQ_HANDLER_END();
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


extern void early_irq_handlers(void);
extern void early_excp_handlers(void);

int
setup_idt (void)
{
    uint_t i;

    ulong_t irq_start = (ulong_t)&early_irq_handlers;
    ulong_t excp_start = (ulong_t)&early_excp_handlers;

    // clear the IDT out
    memset(&idt64, 0, sizeof(struct gate_desc64) * NUM_IDT_ENTRIES);

    for (i = 0; i < NUM_EXCEPTIONS; i++) {
        set_intr_gate(idt64, i, (void*)(excp_start + i*16));
        idt_assign_entry(i, (ulong_t)null_excp_handler);
    }

    for (i = 32; i < NUM_IDT_ENTRIES; i++) {
        set_intr_gate(idt64, i, (void*)(irq_start + (i-32)*16));
        idt_assign_entry(i, (ulong_t)null_irq_handler);
    }

    if (idt_assign_entry(PF_EXCP, (ulong_t)nk_pf_handler) < 0) {
        ERROR_PRINT("Couldn't assign page fault handler\n");
        return -1;
    }

    if (idt_assign_entry(DF_EXCP, (ulong_t)df_handler) < 0) {
        ERROR_PRINT("Couldn't assign double fault handler\n");
        return -1;
    }

    if (idt_assign_entry(0xf, (ulong_t)pic_spur_int_handler) < 0) {
        ERROR_PRINT("Couldn't assign PIC spur int handler\n");
        return -1;
    }

    lidt(&idt_descriptor);

    return 0;
}

