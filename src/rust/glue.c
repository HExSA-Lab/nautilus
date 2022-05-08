// This variable is used only to rust<->c linkage issues
// it has no actual meaning beyond that
volatile void* nk_rust_link_fakery;

#include "glue/example.c"
#include "glue/parport.c"
