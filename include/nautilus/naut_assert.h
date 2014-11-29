#ifndef __NAUT_ASSERT_H__
#define __NAUT_ASSERT_H__

#ifdef __cplusplus
extern "C" {
#endif

extern void serial_print_redirect(const char * format, ...);
extern int printk(const char* fmt, ...);


#ifdef NAUT_CONFIG_ENABLE_ASSERTS
#define ASSERT(cond)                   \
do {                            \
    if (!(cond)) {                  \
    printk("Failed assertion in %s: %s at %s, line %d, RA=%lx\n",\
        __func__, #cond, __FILE__, __LINE__,    \
        (ulong_t) __builtin_return_address(0));  \
    while (1)                   \
       ;                        \
    }                           \
} while (0)
#else 
#define ASSERT(cond)
#endif

#ifdef __cplusplus
}
#endif

#endif
