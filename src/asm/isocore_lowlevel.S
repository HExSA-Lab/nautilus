/*
	On entry, we have:

	rdi - capsule starting address
	rsi - capsule size in bytes
	rdx - entry point / stack start
	rcx - argument to drop into rdi at entry

	This should really be a stub that we prepend to
	the user's code... 
*/

.global _nk_isolate_entry
_nk_isolate_entry:
	// Interrupts off
	cli
	// at this point we cannot be interrupted
	// by an interrupt or IPI, except (perhaps) INIT/SIPI
	// We can be interrupted by an NMI or SMI
	// If we are top of a VMM, the VMM still has full control

	// MTRR manipulation would happen here
	// I'm assuming that the capsule is already cacheable
	// according to MTRR / PAT so skipping this

	// flush cache
	wbinvd
	invd

	// ensure cache is enabled
	movq %cr0, %rax
	andq $~(0x3<<29), %rax  // set CD and NWT to zero
	movq %rax, %cr0

	// stash our args as we will use their regs now
	pushq %rdi
	pushq %rsi
	pushq %rdx
	pushq %rcx

	// now fault the capsule into cache
	// we assume it fits...
	// also capsule must be a nonzero-multiple of 8 bytes long
	// note this read loop is also burning through icache...
	// which looks mighty suspicious
read_loop:	
	movq (%rdi), %rax // read and throw away
	addq $8, %rdi
	subq $8, %rsi
	jne read_loop

	// Now the capsule should be in the cache

	// turn off cache (put into no-fill mode)
	movq %cr0, %rax
	orq $(0x3<<29), %rax  // set CD and NWT to one
	movq %rax, %cr0
	
	// restore to state at call
	popq %rcx
	popq %rdx
	popq %rsi
	popq %rdi
	
	// set up our argument
	movq %rcx, %rdi
	
	// switch to our isolated stack
	movq %rdx, %rsp

	// jump into our isolated code
	jmp *%rdx

	// we should never return from that.... 

	movq $-1, %rax
	
	retq

	

