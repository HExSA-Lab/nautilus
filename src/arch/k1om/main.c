#define __NAUTILUS_MAIN__

#include <arch/k1om/init.h>


void 
main (unsigned long mbd, unsigned long magic) 
{
    init(mbd, magic);
}
