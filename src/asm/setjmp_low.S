
#include <asm/lowlevel.h>



// rdi holds the jmp buf environment
ENTRY(setjmp)
	movq %rbx, (%rdi)     // save rbx
	movq %rbp, 0x8(%rdi)  // save frame
	movq %r12, 0x10(%rdi) // save r12
	movq %r13, 0x18(%rdi) // save r13
	movq %r14, 0x20(%rdi) // save r14
	movq %r15, 0x28(%rdi) // save r15
	leaq 8(%rsp), %rdx
	movq %rdx, 0x30(%rdi) // save rsp above ret addr
	movq (%rsp), %rdx     // ret addr -> rdx
	movq %rdx, 0x38(%rdi) // save it
	xorq %rax, %rax       // return 0
	retq


ENTRY(longjmp)
	// should we return 1 or val?
	xorq %rax, %rax
	inc %rax
	testq %rsi, %rsi      // if rsi is nonzero, we return val
	cmovnzq %rsi, %rax

	movq (%rdi), %rbx     // retsore rbx
	movq 0x8(%rdi), %rbp  // restore frame ptr
	movq 0x10(%rdi), %r12 // restore r12
	movq 0x18(%rdi), %r13 // restore r13
	movq 0x20(%rdi), %r14 // restore r14
	movq 0x28(%rdi), %r15 // restore r15
	movq 0x30(%rdi), %rdx // soon to be stack ptr
	movq %rdx, %rsp
	movq 0x38(%rdi), %rdx // soon to be rip
	jmp  *%rdx

