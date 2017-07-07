

#include <nautilus/printk.h>

#include "gc.h"
#include "test.h"

//#include "leak_detector.h"

int leak_test(void) {
    int *p[10];
    int i;

    // Explicitly trigger full collection before mode change to clean
    // up previous tests
    GC_gcollect();
    
    GC_set_find_leak(1); /* for new collect versions not compiled       */
                         /* with -DFIND_LEAK.                           */
    GC_INIT();  /* Needed if thread-local allocation is enabled.        */
                /* FIXME: This is not ideal.                            */

    printk("Check for any pre-test leaks (should be none!)\n");
    GC_gcollect();
    
    for (i = 0; i < 10; ++i) {
        p[i] = GC_MALLOC(sizeof(int)+i);
    }

    for (i = 1; i < 10; ++i) {
        GC_FREE(p[i]);
    }

    // Leak p[0] by overwriting it without freeing 
    for (i = 0; i < 10; ++i) {
        p[i] = GC_MALLOC(sizeof(int)+i);
    }

    printk("Check for any leaks after overwriting pointer (should be 1!)\n");
    GC_gcollect();

    GC_set_find_leak(0); // Not sure if this is allowed 
    return 0;
}
