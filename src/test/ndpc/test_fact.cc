#define NDPC_NAUTILUS_KERNEL
#include "ndpc_glue.h"
#include "fact.hh"

//
//using namespace std;



NDPC_TEST(fact)
{
    int input=5;
    int output;

    NDPC_PRINTF("Testing factorial example\n");
    
    ndpc_init_preempt_threads();

    if (fact(output,input)) { 
	NDPC_PRINTF("function call failed\n");
    } else {
	NDPC_PRINTF("fact(%d) = %d\n",input,output);
    }

    ndpc_deinit_preempt_threads();

    NDPC_PRINTF("Done testing factorial example\n");

    return 0;
}

