/*
	rax = nk_lowlevel_memset(rdi=dest, rsi=src, rdx=count)
*/
.global nk_low_level_memset
nk_low_level_memset:
	push %rdi  // keep for later return
	testq %rdx, %rdx
	jz memset_out			
memset_loop:	
	movb %sil, (%rdi)
	incq %rdi
	decq %rdx
	jnz memset_loop
memset_out:
	popq %rax
	rep ; retq

.global nk_low_level_memset_word
nk_low_level_memset_word:
	push %rdi  // keep for later return
	testq %rdx, %rdx
	jz memset_word_out			
memset_word_loop:	
	movw %si, (%rdi)
	addq $2, %rdi
	decq %rdx
	jnz memset_word_loop
memset_word_out:
	popq %rax
	rep ; retq
	

