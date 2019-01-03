#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <rt/nesl/nesl.h>


int 
test_nesl (void)
{
    nk_vc_printf("Running the built-in vcode block\n");
    nk_nesl_exec(0);
    nk_vc_printf("Done.\n");
    return 0;
}

static int
handle_nesl (char * buf, void * priv)
{
    test_nesl();
    return 0;
}

static struct shell_cmd_impl nesl_impl = {
    .cmd      = "nesl",
    .help_str = "nesl",
    .handler  = handle_nesl,
};
nk_register_shell_cmd(nesl_impl);
