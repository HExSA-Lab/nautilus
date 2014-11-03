/*
  Kernel needs to be compiled with:

  -fno-rtti
  -fno-exceptions
  -fno-omit-frame-pointer

  Do not link glibc/etc, but rather link this


*/

#include <types.h>
//#include <new>

extern "C" void printk(const char *fmt, ...);
extern "C" void *malloc(size_t);
extern "C" void *free(void *);
extern "C" void panic(const char * fmt, ...);

// Fail out
static void bad()
{
  panic("bad (C++)!\n");
}


// Called when a pure virtual function call is attempted
extern "C" void __cxa_pure_virtual(void)
{
  bad();
}

/* Not really needed since we will compile with -fno-rtti and -fno-exceptions
extern "C" void _Unwind_Resume(void)
{
  bad();
}
*/

void *operator new(size_t size)
{
  return malloc(size);
}

void *operator new[](size_t size)
{
  return malloc(size);
}

void operator delete(void *p)
{
  free(p);
}

void operator delete[](void *p)
{
  free(p);
}


namespace std {

    void __throw_bad_alloc(void) {
        panic("NO %s\n", __func__);
    }

    void __throw_length_error(char const * c) {
        panic("NO %s\n", __func__);
    }
}


