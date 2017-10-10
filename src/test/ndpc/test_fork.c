#include <nautilus/nautilus.h>
#include <rt/ndpc/ndpc_preempt_threads.h>

#define NUM_FORK 16

static int bar(int x ) 
{

    int sum=0;


    nk_vc_printf("bar (tid=%p x=%d) launch\n",(void*) ndpc_my_preempt_thread(),x);

    while (x>0) { 
	sum+=x;
	x--;
	//sleep(rand()%5);
    }

    nk_vc_printf("bar (tid=%p) = %d\n",(void*)ndpc_my_preempt_thread(),sum);

    ndpc_set_result_of_forked_preempt_thread((void*)(long)sum);

    return 0;
}


int foo(int piecewise)
{
    int i;

    thread_id_t tid[NUM_FORK];

    nk_vc_printf("Hi before foo before doing anything piecewise=%d\n", piecewise);

    for (i=0;i<NUM_FORK;i++) { 
	nk_vc_printf( "Thread %p iteration %d\n", (void*)ndpc_my_preempt_thread(),i);
	
	tid[i] = ndpc_fork_preempt_thread();
	
	if (!tid[i]) {
	    nk_vc_printf("Error!\n");
	    return -1;
	}
	
	if (tid[i]==ndpc_my_preempt_thread()) { 
	    //nk_bind_vc(get_cur_thread(),get_cur_thread()->parent->vc);
	    nk_vc_printf( "I am the child thread and will now do some work\n");
	    bar(i);
	    return 0; // this will kill the thread
	    
	} else {
	    nk_vc_printf( "I am the parent, printing threads and going to sleep for a while\n");
	    //dump_threads();
	    //sleep(rand()%5);
	}
	
		    
    }

    nk_vc_printf("I am the parent and am joining\n");

    if (piecewise) { 
	nk_vc_printf("Now joining thread by thread\n");
	
	for (i=0;i<NUM_FORK;i++) { 
	    int s;
	    
	    s = (int) (long)ndpc_join_preempt_thread(tid[i]);
	    
	    nk_vc_printf("Result of thread %d = %d\n", i, s);
	}
    } else {
	nk_vc_printf("Now joining with all children\n");
	ndpc_join_child_preempt_threads();
	nk_vc_printf("Done with join\n");
    }

    return 0;

}
	

int ndpc_test_fork()
{
    thread_id_t tid;


    nk_vc_printf("Fork test\n");
    
    ndpc_init_preempt_threads();

    foo(0);
    foo(1);

    ndpc_deinit_preempt_threads();

    
    return 0;
}


