.global nk_simple_timing_loop
nk_simple_timing_loop:
	addq $1, %rax
	decq %rdi
	jnz nk_simple_timing_loop
	rep ; retq
	

