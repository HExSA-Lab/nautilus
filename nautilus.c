#include <cga.h>


/**
 * This is the first C function executed
 */

void nautilus_main(void);
void nautilus_main (void) {
    int i;
    term_init();
    for (i = 0; i < 50; i++) {
        term_print("Hello World...\n");
    }
}
