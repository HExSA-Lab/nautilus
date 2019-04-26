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
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/scheduler.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>

#define DO_PRINT       0

#if DO_PRINT
#define PRINT(...) nk_vc_printf(__VA_ARGS__)
#else
#define PRINT(...) 
#endif

// these are volatiles to stop
// compiler from optimizing the fork tests to the point of failure
#define NUM_PASSES 10
#define NUM_THREADS 512
#define DEPTH 8

struct test_arg {
    int pass;
    int thread;
};

static void thread_func(void *in, void **out)
{
    char buf[32];
    struct nk_thread *t = get_cur_thread();
    struct test_arg *arg = (struct test_arg *)in;
    
    snprintf(buf,32, "test_cj_%d_%d",arg->pass, arg->thread);
    nk_thread_name(get_cur_thread(),buf);

    get_cur_thread()->vc = get_cur_thread()->parent->vc;

    PRINT("Hello from thread tid %lu - %d pass %d\n", t->tid, arg->thread, arg->pass);

    free(arg);
}

static int test_create_join(int nump, int numt)
{
    int i,j;

    PRINT("Starting on threads create/join stress test (%d passes, %d threads)\n",nump,numt);
    
    for (i=0;i<nump;i++) {
	PRINT("Starting to launch %d threads on pass %d\n",numt,i);
	for (j=0;j<numt;j++) { 
	    struct test_arg *arg = (struct test_arg *) malloc(sizeof(struct test_arg));
	    arg->pass=i;
	    arg->thread=j;
	    if (nk_thread_start(thread_func,
				arg,
				0,
				0,
				PAGE_SIZE_4KB,
				NULL,
				-1)) { 
		PRINT("Failed to launch thread %d on pass %d\n", j,i);
		nk_join_all_children(0);
		return -1;
	    }
	}
	PRINT("Launched %d threads in pass %d\n", j, i);
	if (nk_join_all_children(0)) {
	    PRINT("Failed to join threads on pass %d\n",i);
	    return -1;
	}
	PRINT("Joined %d threads in pass %d\n", j, i);
	nk_sched_reap(1); // clean up unconditionally
    }
    PRINT("Done with thread create/join stress test (SUCCESS)\n");
    return 0;
}


static int __noinline
#ifndef __clang__
__attribute__((noclone))
#endif
test_fork_join(int nump, int numt)
{
    int i,j;

    PRINT("Starting on threads fork/join stress test (%d passes, %d threads)\n",nump,numt);

    nk_thread_id_t t;
    
    for (i=0;i<nump;i++) {
	PRINT("Starting to fork %d threads on pass %d\n",numt,i);
	for (j=0;j<numt;j++) {
	    t = nk_thread_fork();
	    if (t==NK_BAD_THREAD_ID) {
		PRINT("Failed to fork thread\n");
		return 0;
	    }
	    if (t==0) { 
		// child thread
		char buf[32];
		struct nk_thread *t = get_cur_thread();
		snprintf(buf,32, "test_fj_%d_%d",i,j);
		nk_thread_name(get_cur_thread(),buf);
		get_cur_thread()->vc = get_cur_thread()->parent->vc;
		PRINT("Hello from forked thread tid %lu - %d pass %d\n", t->tid, j, i);
		// if the function being forked is inlined
		// we must explicitly invoke
		// nk_thread_exit(0);
		// here
		return 0;
	    } else {
		// parent thread just forks again
	    }
	}
	PRINT("Forked %d threads in pass %d\n", j, i);
	if (nk_join_all_children(0)) {
	    PRINT("Failed to join forked threads on pass %d\n",i);
	    return -1;
	}
	PRINT("Joined %d forked threads in pass %d\n", j, i);
	nk_sched_reap(1); // clean up unconditionally
    }
    PRINT("Done with thread fork/join stress test (SUCCESS)\n");
    return 0;
}

static void _test_recursive_create_join(void *in, void **out)
{
    uint64_t depth = (uint64_t) in;

    if (depth > 0) { 
	get_cur_thread()->vc = get_cur_thread()->parent->vc;
    }

    PRINT("Hello from tid %lu at depth %lu\n", get_cur_thread()->tid, depth);

    if (depth==DEPTH) { 
	return;
    } else {
	// left
	if (nk_thread_start(_test_recursive_create_join,
			    (void*)(depth+1),
			    0,
			    0,
			    PAGE_SIZE_4KB,
			    NULL,
			    -1)) { 
	    PRINT("Failed to launch left thread at depth %lu\n", depth);
	    nk_join_all_children(0);
	    return ;
	}
	// right
	if (nk_thread_start(_test_recursive_create_join,
			    (void*)(depth+1),
			    0,
			    0,
			    PAGE_SIZE_4KB,
			    NULL,
			    -1)) { 
	    PRINT("Failed to launch right thread at depth %lu\n", depth);
	    nk_join_all_children(0);
	    return ;
	}
	PRINT("Thread %lu launched left and right threads at depth %lu\n",get_cur_thread()->tid, depth);
	nk_join_all_children(0);
	PRINT("Thread %lu joined with left and right threads at depth %lu\n",get_cur_thread()->tid,depth);
	return ;
    }
}

static int test_recursive_create_join()
{
    int i;
    for (i=0;i<NUM_PASSES;i++) {
	_test_recursive_create_join(0,0);
	nk_sched_reap(1);
    }
    return 0;
}


volatile static uint64_t hack;

static void __noinline
#ifndef __clang__
__attribute__((noclone))
#endif
_test_recursive_fork_join(uint64_t depth, uint64_t pass)
{
    if (depth > 0) { 
	get_cur_thread()->vc = get_cur_thread()->parent->vc;
    }

    PRINT("Hello from forked tid %lu at pass %lu depth %lu\n", get_cur_thread()->tid, pass, depth);

    nk_thread_id_t l,r;
    
    if (depth==DEPTH) { 
	return;
    } else {
	l = nk_thread_fork();
	if (l==NK_BAD_THREAD_ID) {
	    PRINT("Failed to fork left thread\n");
	    return;
	}
	if (l == 0) {
	    // left child
	    _test_recursive_fork_join(depth+1,pass);
	    nk_thread_exit(0);
	    return;
	}
	r = nk_thread_fork();
	if (r==NK_BAD_THREAD_ID) {
	    PRINT("Failed to fork right thread\n");
	    return;
	}
	if (r == 0) {
	    // right child
	    _test_recursive_fork_join(depth+1,pass);
	    nk_thread_exit(0);
	    return;
	}
	// parent
	PRINT("Thread %lu forked left and right threads at pass %lu depth %lu\n",get_cur_thread()->tid, pass, depth);
	nk_join_all_children(0);
	PRINT("Thread %lu joined with left and right threads at pass %lu depth %lu\n",get_cur_thread()->tid,pass,depth);
	return ;
    }
}

static int test_recursive_fork_join()
{
    int i;
    for (i=0;i<NUM_PASSES;i++) {
	_test_recursive_fork_join(0,i);
	nk_sched_reap(1);
    }
    return 0;
}




int test_threads()
{
    int create_join;
    int fork_join;
    int recursive_create_join;
    int recursive_fork_join;

    create_join = test_create_join(NUM_PASSES,NUM_THREADS);

    nk_vc_printf("Create-join test of %lu passes with %lu threads each: %s\n", 
		 NUM_PASSES,NUM_THREADS, create_join ? "FAIL" : "PASS");
    
    fork_join = test_fork_join(NUM_PASSES,NUM_THREADS);
    
    nk_vc_printf("Fork-join test of %lu passes with %lu threads each: %s\n", 
		 NUM_PASSES,NUM_THREADS, fork_join ? "FAIL" : "PASS");
    
    recursive_create_join = test_recursive_create_join();

    nk_vc_printf("Recursive create-join test of %lu passes with depth %lu (%lu threads): %s\n", 
		 NUM_PASSES,DEPTH, 1ULL<<(DEPTH+1),recursive_create_join ? "FAIL" : "PASS");

    recursive_fork_join = test_recursive_fork_join();

    nk_vc_printf("Recursive fork-join test of %lu passes with depth %lu (%lu threads): %s\n", 
		 NUM_PASSES,DEPTH, 1ULL<<(DEPTH+1),recursive_fork_join ? "FAIL" : "PASS");


    return create_join | fork_join | recursive_create_join | recursive_fork_join;

return fork_join;
}


static int
handle_threads (char * buf, void * priv)
{
    test_threads();
    return 0;
}

static struct shell_cmd_impl threads_impl = {
    .cmd      = "threadtest",
    .help_str = "threadtest",
    .handler  = handle_threads,
};
nk_register_shell_cmd(threads_impl);
