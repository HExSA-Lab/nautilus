

#ifndef __BDWGC_TEST__
#define __BDWGC_TEST__

// Internal tests
int huge_test();
int realloc_test();
int leak_test();

int bdwgc_test_gc(); // Run all internal tets
int bdwgc_test_leak_detector(); // Run all internal tets


#endif
