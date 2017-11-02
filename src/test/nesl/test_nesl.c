#include <nautilus/nautilus.h>
#include <rt/nesl/nesl.h>



 int test_nesl()
 {
     nk_vc_printf("Running the built-in vcode block\n");
     nk_nesl_exec(0);
     nk_vc_printf("Done.\n");
     return 0;
 }
