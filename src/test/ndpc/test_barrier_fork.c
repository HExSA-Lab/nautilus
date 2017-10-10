#include <nautilus/nautilus.h>
#include <rt/ndpc/ndpc_preempt_threads.h>

#define N 8

static int test() {
    int x;
    for (x = 0; x < N; x++) {
        if (ndpc_fork_preempt_thread() == ndpc_my_preempt_thread()) {
            ndpc_barrier();
            ndpc_barrier();
            ndpc_barrier();
            ndpc_barrier();
            return 0;
        }
    }
    ndpc_join_child_preempt_threads();
    return 0;
}

int ndpc_test_barrier_fork() {
    nk_vc_printf("Testing fork+barrier (N=%d)\n",N);
    ndpc_init_preempt_threads();
    test();
    ndpc_deinit_preempt_threads();

    return 0;
}

