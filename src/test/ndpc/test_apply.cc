#define NDPC_NAUTILUS_KERNEL
#include "ndpc_glue.h"
#include "apply.hh"
#include "vector.hh" 

using namespace std;

#define TYPE int
#define N 10

NDPC_TEST(apply)
{
    int i;
    Vector<TYPE> input(N);
    Vector<TYPE> output(N);

    NDPC_PRINTF("Testing apply\n");
    
    ndpc_init_preempt_threads();

    for (i=0;i<N;i++) { 
	input[i] = i;
    }

    if (timestwo(output,input)) { 
	NDPC_PRINTF("function call failed\n");
    } else {
	for (i=0;i<N;i++) { 
	    NDPC_PRINTF("timestwo(%d) = %d\n", input[i], output[i]);
	}
    }

    ndpc_deinit_preempt_threads();

    NDPC_PRINTF("Done testing  apply\n"); 

    return 0;
}

