#define __NAUTILUS_MAIN__

#include <arch/x64/init.h>


/*
 * 2nd two parameters are 
 * only used if this is an HRT 
 */
void 
main (unsigned long mbd, 
      unsigned long magic)

{
    init(mbd, magic);
}
