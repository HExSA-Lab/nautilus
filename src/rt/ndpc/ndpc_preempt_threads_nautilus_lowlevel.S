//
// Copyright (c) 2017 Peter Dinda  All Rights Reserved
//


// note semantic change -
// nautilus: parent gets child, child gets zero, failure gets -1
// ndpc: parent gets child, child gets self, failure gets 0
	
// Note that nk_thread_fork will see us as part of the stack frame
// of our caller provided we do not manipulate %rbp
.global ndpc_fork_preempt_thread
ndpc_fork_preempt_thread:
	call nk_thread_fork     // do actual fork here
	// the fork will modify the child's return address (the
	// second copy, from our prespective)
	pushfq                  // we are about to nuke the flags
	cmpq $-1, %rax          // did we fail?
	je failout
	testq %rax, %rax        // are we the child?
	jz childout             // child needs special handling

parentout:
	// rax already has what we need to return
	popfq                   // flag restore
	retq                    // parent return

childout:
	// fork must happen only after per-cpu is available...
	movq %gs:0, %rax        // put our own tid into rax
	popfq                   // flag restore
	retq                    // child return


failout:
	xorq %rax, %rax         // return zero
	popfq                   // restore flags
	retq                    // normal return
	


	
