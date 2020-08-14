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
 * http://xstack.sandia.gov/hobbes
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
#include <nautilus/intrinsics.h>
#include <nautilus/nautilus.h>
#include <nautilus/cpu.h>
#include <nautilus/thread.h>
#ifdef NAUT_CONFIG_PROVENANCE
#include <nautilus/provenance.h>
#endif

extern int printk (const char * fmt, ...);

#ifdef NAUT_CONFIG_PROVENANCE
static void print_prov_info(uint64_t addr) {
	provenance_info* prov_info = nk_prov_get_info(addr);
	if (prov_info != NULL) {
		printk("Symbol: %s   ", (prov_info->symbol != NULL) ? (char*) prov_info->symbol : "???");
		printk("Section: %s\n", (prov_info->section != NULL) ? (char*) prov_info->section : "???");
		if (prov_info->line_info != NULL) {
			// TODO: print line info
		}
		if (prov_info->file_info != NULL) {
			// TODO: print file info
		}
		free(prov_info);
	}
}
#endif

void __attribute__((noinline))
__do_backtrace (void ** fp, unsigned depth)
{
    if (!fp || fp >= (void**)nk_get_nautilus_info()->sys.mem.phys_mem_avail) {
        return;
    }
    
    printk("[%2u] RIP: %p RBP: %p\n", depth, *(fp+1), *fp);
#ifdef NAUT_CONFIG_PROVENANCE
	print_prov_info((uint64_t) *(fp+1));
#endif
    __do_backtrace(*fp, depth+1);
}


/*
 * dump memory in 16 byte chunks
 */
void 
nk_dump_mem (const void * addr, ulong_t n)
{
    int i, j;
    ulong_t new = (n % 16 == 0) ? n : ((n+16) & ~0xf);

    for (i = 0; i < new/(sizeof(void*)); i+=2) {
        printk("%p: %08p  %08p  ", ((void**)addr + i), *((void**)addr + i), *((void**)addr + i + 1));
        for (j = 0; j < 16; j++) {
            char tmp = *((char*)addr + j);
            printk("%c", (tmp < 0x7f && tmp > 0x1f) ? tmp : '.');
        }

        printk("\n");
    }
}


void 
nk_stack_dump (ulong_t n)
{
    void * rsp = NULL;

    asm volatile ("movq %%rsp, %[_r]" : [_r] "=r" (rsp));

    if (!rsp) {
        return;
    }

    printk("Stack Dump:\n");

    nk_dump_mem(rsp, n);
}


void 
nk_print_regs (struct nk_regs * r)
{
    int i = 0;
    ulong_t cr0 = 0ul;
    ulong_t cr2 = 0ul;
    ulong_t cr3 = 0ul;
    ulong_t cr4 = 0ul;
    ulong_t cr8 = 0ul;
    ulong_t fs  = 0ul;
    ulong_t gs  = 0ul;
    ulong_t sgs = 0ul;
    uint_t  fsi;
    uint_t  gsi;
    uint_t  cs;
    uint_t  ds;
    uint_t  es;
    ulong_t efer;

    printk("Current Thread=0x%x (%p) \"%s\"\n", 
            get_cur_thread() ? get_cur_thread()->tid : -1,
            get_cur_thread() ? (void*)get_cur_thread() :  NULL,
            !get_cur_thread() ? "NONE" : get_cur_thread()->is_idle ? "*idle*" : get_cur_thread()->name);

    
    printk("[-------------- Register Contents --------------]\n");
    printk("RIP: %04lx:%016lx\n", r->cs, r->rip);
    printk("RSP: %04lx:%016lx RFLAGS: %08lx Vector: %08lx Error: %08lx\n", 
            r->ss, r->rsp, r->rflags, r->vector, r->err_code);
    printk("RAX: %016lx RBX: %016lx RCX: %016lx\n", r->rax, r->rbx, r->rcx);
    printk("RDX: %016lx RDI: %016lx RSI: %016lx\n", r->rdx, r->rdi, r->rsi);
    printk("RBP: %016lx R08: %016lx R09: %016lx\n", r->rbp, r->r8, r->r9);
    printk("R10: %016lx R11: %016lx R12: %016lx\n", r->r10, r->r11, r->r12);
    printk("R13: %016lx R14: %016lx R15: %016lx\n", r->r13, r->r14, r->r15);

    asm volatile("movl %%cs, %0": "=r" (cs));
    asm volatile("movl %%ds, %0": "=r" (ds));
    asm volatile("movl %%es, %0": "=r" (es));
    asm volatile("movl %%fs, %0": "=r" (fsi));
    asm volatile("movl %%gs, %0": "=r" (gsi));

    gs  = msr_read(MSR_GS_BASE);
    fs  = msr_read(MSR_FS_BASE);
    gsi = msr_read(MSR_KERNEL_GS_BASE);
    efer = msr_read(IA32_MSR_EFER);

    asm volatile("movq %%cr0, %0": "=r" (cr0));
    asm volatile("movq %%cr2, %0": "=r" (cr2));
    asm volatile("movq %%cr3, %0": "=r" (cr3));
    asm volatile("movq %%cr4, %0": "=r" (cr4));
    asm volatile("movq %%cr8, %0": "=r" (cr8));

    printk("FS: %016lx(%04x) GS: %016lx(%04x) knlGS: %016lx\n", 
            fs, fsi, gs, gsi, sgs);
    printk("CS: %04x DS: %04x ES: %04x CR0: %016lx\n", 
            cs, ds, es, cr0);
    printk("CR2: %016lx CR3: %016lx CR4: %016lx\n", 
            cr2, cr3, cr4);
    printk("CR8: %016lx EFER: %016lx\n", cr8, efer);

    printk("[-----------------------------------------------]\n");
}
