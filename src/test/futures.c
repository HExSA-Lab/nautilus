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
 * Copyright (c) 2019, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
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
#include <nautilus/future.h>
#include <nautilus/scheduler.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>

#define DO_PRINT       1

#if DO_PRINT
#define PRINT(...) nk_vc_printf(__VA_ARGS__)
#else
#define PRINT(...) 
#endif

#define NUM_PASSES 10
#define NUM_FUTURES 512

static void test_basic_producer(void *in, void **out)
{
    nk_future_t *f = (nk_future_t *) in;

    nk_future_finish(f,(void*)42);
}

static int test_basic()
{
    int i,j;
    int rc = 0;
    nk_future_t *futures[NUM_FUTURES];
    
    for (i=0;i<NUM_PASSES;i++) {
	//PRINT("pass %d\n",i);
	for (j=0;j<NUM_FUTURES;j++) {
	    futures[j] = nk_future_alloc();
	    if (!futures[j]) {
 		PRINT("Cannot allocate future\n");
		rc = -1;
	    }
	    if (nk_thread_start(test_basic_producer,
				futures[j],
				0,
				1, // detach
				PAGE_SIZE_4KB,
				NULL,
				-1)) { 
		PRINT("Failed to launch thread %d on pass %d\n", j,i);
		rc = -1;
		goto out_clean;
	    }
	}
	// now get the futures
	for (j=0;j<NUM_FUTURES;j++) {
	    void *ret;
	    // randomly flip between spin wait and block wait
	    if (nk_future_wait(futures[j], rdtsc() % 2,&ret)) {
		PRINT("Failed to wait on future %d\n", j);
		rc = -1;
		goto out_clean;
	    }
	    if (ret!=(void*)42) {
		PRINT("future %d has return value %p\n",ret);
		rc = -1;
		goto out_clean;
	    }
	    nk_future_free(futures[j]);
	}
	nk_sched_reap(1); // clean up unconditionally
    }


 out_clean:
    for (i=0;i<NUM_FUTURES;i++) {
	if (futures[i]) { nk_future_free(futures[i]); }
    }
    return rc;
}




static int test_futures()
{
    int basic = test_basic();
    
    nk_vc_printf("Basic future test: %s\n", basic ? "FAIL" : "PASS");
    return basic;
}


static int handle_futures(char *buf, void *priv)
{
    test_futures();
    return 0;
}

static struct shell_cmd_impl futures_impl = {
    .cmd      = "futuretest",
    .help_str = "futuretest",
    .handler  = handle_futures,
};
nk_register_shell_cmd(futures_impl);
