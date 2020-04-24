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
#include <nautilus/idt.h>
#include <nautilus/thread.h>

#define GPIO_OUTPUT 1

#ifdef NAUT_CONFIG_CACHEPART_INTERRUPT
#define INTERRUPT_PARTITION 0x1
#else
#define INTERRUPT_PARTITION 0x0
#endif
	
.code64
.macro make_excp_handler v
    .if (ERR_CODE_EXCP_MASK >> \v) & 1
        pushq $0
    .else
        GEN_NOP(NOP_2BYTE)
    .endif
    pushq $\v
    jmp early_excp_common
    .align 16
.endm

.align 16
GLOBAL(early_excp_handlers)
    vector = 0
    .rept 32
    make_excp_handler vector
    vector = vector + 1
    .endr
END(early_excp_handlers)


.macro make_irq_handler v
    pushq $0 // KCH DANGER
    pushq $\v
    jmp early_irq_common
    .align 16
.endm
    
.align 16
GLOBAL(early_irq_handlers)
    vector = 32
    .rept NUM_IDT_ENTRIES - 32
    make_irq_handler vector
    vector = vector + 1
    .endr
END(early_irq_handlers)


.align 16
GLOBAL(early_irq_common)

    SAVE_GPRS()

#ifdef NAUT_CONFIG_WATCHDOG
    // this call can nuke registers, but we don't
    // care because it is first, and we don't depend on
    // the register contents yet	 
    call nk_watchdog_pet
#endif
	
#ifdef NAUT_CONFIG_CACHEPART
    // note that we have already saved all GPRs
    // we will also use callee-save registers to maintain state
    // %rbx => have_cat, %r12 =>  previous partition %r13 => extra msr 
	 
    movq _nk_cache_part_has_cat, %rbx // check if we have cat
    test %rbx, %rbx                   
    jz skip_cat_entry                 // skip if we do not

    // save the current partition
    movq $0xc8f, %rcx    // IA32_PQR_ASSOC
    rdmsr
    // these zero extend r12 and r13	
    movl %edx, %r12d     // this is the partition we are switching away from
    movl %eax, %r13d     // this is the extra msr stuff we will also restore
	

    // and switch to the interrupt partition 
    movq $INTERRUPT_PARTITION, %rdx
    // eax will be the same as before
    wrmsr

    // we are now running with the new partition
    // and have previous state stashed away	 

skip_cat_entry:

#endif

#if GPIO_OUTPUT && defined(NAUT_CONFIG_GPIO)
    movq $0x10, %rdi   // bit 4
    movq $0x0, %rsi    // or it high
    callq nk_gpio_output_mask
#endif
    // Interrupts are enabled only after per-cpu functonality
    // is enabled (and gsbase is set), so these are safe to execute
    // without a guard
    
    incw %gs:8   // increment interrupt nesting level -
	         // cpu is now in interrupt context
    incq %gs:16  // increment interrupt count

    // we must save the stack pointer early to support
    // both remote debugging and garbage collection, both
    // of which can either occur during an exception or
    // can depend on having every thread's rsp snapshotted
    // correctly during a world stop
    movq %gs:0x0, %rax
    movq %rsp, (%rax)
    

#ifdef NAUT_CONFIG_PROFILE
    callq nk_irq_prof_enter
#endif

    leaq 128(%rsp), %rdi # pointer to exception struct
    movq 120(%rsp), %rsi # irq num
    movabs $idt_handler_table, %rdx
    movq (%rdx, %rsi, 8), %r8   # pointer to handler
    movabs $idt_state_table, %rdx
    movq (%rdx, %rsi, 8), %rdx   # pointer to state
	
    
#ifdef NAUT_CONFIG_ENABLE_ASSERTS
    cmpq $256, %rsi
    jge irq_err

    cmpq $32, %rsi
    jl irq_err
#endif

    callq *%r8    # rdi = exp struct, rsi = irq num, rdx = state

    testq %rax, %rax
    jnz irq_err

#ifdef NAUT_CONFIG_PROFILE
    callq nk_irq_prof_exit
#endif

    // we're back from the irq handler
    // do we need to switch to someone else?
    callq nk_sched_need_resched
    testq %rax, %rax
    jz thr_return
    movq %rax, %rdi
     // on a context switch, this will handle cache partition changes for the new thread	
    jmp nk_thread_switch_intr_entry    

thr_return:
#if GPIO_OUTPUT && defined(NAUT_CONFIG_GPIO)
    movq $0x10, %rdi   // bit 4
    notq %rdi          // clear
    movq $0x1, %rsi    // and it low
    callq nk_gpio_output_mask
#endif

#ifdef NAUT_CONFIG_CACHEPART
    // we still expect to have the following in the callee-save registers
    // %rbx => have_cat, %r12 =>  previous partition %r13 => extra msr 
	 
    test %rbx, %rbx                   
    jz skip_cat_exit                 // skip if we do not have cat

    // restore the old partition
    movq $0xc8f, %rcx    // IA32_PQR_ASSOC
    movq %r12, %rdx
    movq %r13, %rax

    wrmsr

    // we are now running with the old partition

skip_cat_exit:

#endif

    RESTORE_GPRS()

    
    addq $16, %rsp  // pop the vector and the error code

    // This update is safe to do since if interrupts are on
    // gsbase has already been set
    decw %gs:8      // decrement interrupt nesting level
	            // we may now be outside of interrupt context

    iretq

irq_err:
    movabs $irq_err_str, %rdi
    movabs $panic, %rax
    callq *%rax
errloop:
    hlt
    jmp errloop

.align 8
GLOBAL(early_excp_common)

    SAVE_GPRS()

    // Exceptions may occur before gsbase is set, for example breakpoints
    // or bugs in early code - so we need to guard
    // execution of the following code

    movl $0xc0000101, %ecx    // gsbase msr
    rdmsr
    orl %edx, %eax            
    je skip_per_cpu_update1    // skip if gsbase==0
    
    incw %gs:8   // increment interrupt nesting level -
	         // cpu is now in interrupt context 
    incq %gs:24  // increment exception count

    // we must save the stack pointer early to support
    // both remote debugging and garbage collection, both
    // of which can either occur during an exception or
    // can depend on having every thread's rsp snapshotted
    // correctly during a world stop
    movq %gs:0x0, %rax
    movq %rsp, (%rax)

skip_per_cpu_update1:   
    
    movq 120(%rsp), %rsi
    cmpq $32, %rsi
    jge excp_err

    leaq 128(%rsp), %rdi # pointer to exception struct
    movq 120(%rsp), %rsi # excp vector
    movabs $idt_handler_table, %rdx
    movq (%rdx, %rsi, 8), %r8   # pointer to handler
    movabs $idt_state_table, %rdx
    movq (%rdx, %rsi, 8), %rdx   # pointer to state

    callq *%r8   # rdi = exp struct, rsi = irq num, rdx = state
    testq %rax, %rax
    jnz excp_err

    // we need to guard the per-cpu update here
    movl $0xc0000101, %ecx    // gsbase msr
    rdmsr
    orl %edx, %eax            
    je skip_per_cpu_update2    // skip if gsbase==0
    decw %gs:8      // decrement interrupt nesting level
	            // we may now be outside of interrupt context

skip_per_cpu_update2:   
    
    RESTORE_GPRS()
    /* pop error code and excp number */
    addq $16, %rsp
    iretq

excp_err:
    movabs $excp_err_str, %rdi
    movabs $panic, %rax
    callq *%rax
loop1:
    hlt
    jmp loop1


.align 8
GLOBAL(idt_handler_table)
.rept NUM_EXCEPTIONS
    .quad null_excp_handler
.endr

.rept NUM_IDT_ENTRIES - NUM_EXCEPTIONS
    .quad null_irq_handler
.endr
END(idt_handler_table)

.align 8
GLOBAL(idt_state_table)
.rept NUM_IDT_ENTRIES
    .quad 0
.endr
END(idt_state_table)


irq_err_str:
.asciz "Received illegal interrupt %u\12"

excp_err_str:
.asciz "Received illegal exception %u\12"
