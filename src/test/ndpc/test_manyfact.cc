#define NDPC_NAUTILUS_KERNEL
#include "ndpc_glue.h"
#include "manyfact.hh"
#include "vector.hh"

using namespace std;

#define TYPE int
#define N 10

NDPC_TEST(manyfact)
{
    int i;
    Vector<TYPE> input(N);
    Vector<TYPE> output(N);

    NDPC_PRINTF("Testing many factorial example\n");
    
    ndpc_init_preempt_threads();


    for (i=0;i<N;i++) { 
	input[i] = i;
    }

    if (manyfact(output,input)) { 
	NDPC_PRINTF("function call failed\n");
    } else {
	for (i=0;i<N;i++) { 
	    NDPC_PRINTF("fact(%d) = %d\n", input[i], output[i]);
	}
    }

    ndpc_deinit_preempt_threads();

    NDPC_PRINTF("Done testing many factorial example\n");
    
    return 0;
}

