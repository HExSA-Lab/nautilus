#ifndef __NDPC_GLUE__
#define __NDPC_GLUE__

#ifdef NDPC_LINUX_USER
#define NDPC_TEST() int main()
extern "C" int printf(...);
#define NDPC_PRINTF(...) printf(__VA_ARGS__)
#endif

#ifdef NDPC_NAUTILUS_KERNEL
#define NDPC_TEST(X) extern "C" int test_ndpc_##X()
extern "C" int printk(...);
#define NDPC_PRINTF(...) nk_vc_printf(__VA_ARGS__)
extern "C" {
int nk_vc_printf(...); 
#include <rt/ndpc/ndpc_preempt_threads.h>
}
#endif

#endif
