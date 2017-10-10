#define NDPC_NAUTILUS_KERNEL
#include "ndpc_glue.h"
#include "prod2.hh"
#include "vector.hh"

using namespace std;

#define TYPE int
#define N 10

NDPC_TEST(prod2)
{
    int i;
    Vector<TYPE> input1(N);
    Vector<TYPE> input2(N);
    Vector<TYPE> output(N);

    NDPC_PRINTF("Testing prod2\n");
    
    ndpc_init_preempt_threads();

    for (i=0;i<N;i++) { 
	input1[i] = i;
	input2[i] = i;
    }

    if (prod2(output,input1,input2)) { 
	NDPC_PRINTF("function call failed\n");
    } else {
	for (i=0;i<output.numitems();i++) { 
	    NDPC_PRINTF("prod2(%d,%d)=%d\n",input1[i],input2[i],output[i]);
	}
    }

    ndpc_deinit_preempt_threads();

    NDPC_PRINTF("Done testing prod2\n");

    return 0;
}

