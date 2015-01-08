#include <nautilus/nautilus.h>

#ifndef NAUT_CONFIG_DEBUG_CXX
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define CXX_DEBUG(fmt, args...) DEBUG_PRINT("C++ INIT: " fmt, ##args)
#define CXX_PRINT(fmt, args...) printk("C++ INIT:" fmt, ##args)

/* for global constructors */
extern void (*_init_array_start []) (void) __attribute__((weak));
extern void (*_init_array_end []) (void) __attribute__((weak));

static void 
__do_ctors_init (void) 
{
    void (**p)(void) = NULL;

    for (p = _init_array_start; p < _init_array_end; p++) {
        if (*p) {
            CXX_DEBUG("Calling static constructor (%p)\n", (void*)(*p));
            (*p)();
        }
    }
}


void 
nk_cxx_init (void)
{
    __do_ctors_init();
}
