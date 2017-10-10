#include <nautilus/nautilus.h>
#include <rt/ndpc/ndpc_preempt_threads.h>



#define NUM_THREADS 16
#define NUM_LOOP    100000

static int test(void *input, void **output)
{
    int me = (int)(long)(input);
    int i=0;
    volatile int j;

    nk_vc_printf("I am the child thread. Spinnng\n");
    while(i<10) { 
	nk_vc_printf("Core %u Child %d, loop %d\n",my_cpu_id(),me,i);
	i++;
	for (j=0;j<NUM_LOOP;j++) {}
    }

  
    return 0;
	
}


int ndpc_test_integral()
{
    thread_id_t tid;
    thread_id_t me;
    int i;

    nk_vc_printf("Integral test\n");

    ndpc_init_preempt_threads();

    nk_vc_printf("Create\n");

    for (i=0;i<NUM_THREADS;i++) { 
	ndpc_create_preempt_thread(test,(void*)(long)(i),0,-1,16*4096,&tid);
	nk_vc_printf("Created thread %d (%p)\n",i,tid);
    }


    nk_vc_printf("Join\n");
    
    ndpc_join_child_preempt_threads();

    nk_vc_printf("Done\n");

    ndpc_deinit_preempt_threads();

    return 0;


}
