
int ndpc_test_integral();
int ndpc_test_fork();
int ndpc_test_barrier_fork();

int test_ndpc_fact();
int test_ndpc_apply();
int test_ndpc_manyfact();
int test_ndpc_prod2();


int test_ndpc()
{
   
    ndpc_test_integral();
    ndpc_test_fork();
    ndpc_test_barrier_fork();

    test_ndpc_fact();
    test_ndpc_manyfact();
    test_ndpc_apply();
    test_ndpc_prod2();
    
    return 0;
}
