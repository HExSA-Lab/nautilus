#include <nautilus/nautilus.h>
#include <nautilus/shell.h>

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

static int
handle_ndpc (char * buf, void * priv)
{
    test_ndpc();
    return 0;
}

static struct shell_cmd_impl ndpc_impl = {
    .cmd      = "ndpc",
    .help_str = "ndpc",
    .handler  = handle_ndpc,
};
nk_register_shell_cmd(ndpc_impl);
