// We need to reference exported rust functions and data here
// so that the linker does not lose them.  Why would it lose them?
// Because Rust insists on making a static library as the closest
// thing to a bloody .o file.

// the rust function we will call from C
// the prototype does not matter here - this is
// for the linker's consumption
extern void nk_rust_example(void);

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
