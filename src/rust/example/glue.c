#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
// We need to reference exported rust functions and data here
// so that the linker does not lose them.  Why would it lose them?
// Because Rust insists on making a static library as the closest
// thing to a bloody .o file.

// the rust function we will call from C
// the prototype does not matter here - this is
// for the linker's consumption
extern int nk_rust_example(int,int);

// Reference to the top-level rust<->C glue.  This is
// here just to make sure that the linker does not strip
// out what we care about from the 
extern volatile void *nk_rust_link_fakery;

// never called, we don't care as long as there is no name collision
void _please_mr_linker_do_not_lose_my_module_named_example()
{
    // we do not care about this assignment
    nk_rust_link_fakery = (void*) nk_rust_example;
}


static int
handle_rust (char * buf, void * priv)
{
    nk_vc_printf("Testing rust\n");
    int sum = nk_rust_example(1,1);
    nk_vc_printf("Rust indicates that 1+1=%d\n", sum);
    return 0;
}


static struct shell_cmd_impl rust_impl = {
    .cmd      = "rust",
    .help_str = "rust",
    .handler  = handle_rust,
};
nk_register_shell_cmd(rust_impl);
