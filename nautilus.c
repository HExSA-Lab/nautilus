#include <cga.h>
#include <printk.h>

/**
 * This is the first C function executed
 */

void nautilus_main(void);
void nautilus_main (void) {
    int i;
    term_init();
    for (i = 0; i < 50; i++) {
        printk("Hello World %d\n", i);
    }
}
