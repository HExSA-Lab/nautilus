#include <cga.h>



void nautilus_main () {
    /*__asm__ __volatile__("vmmcall");*/
    term_init();
    term_print("Hello World\n");
}
