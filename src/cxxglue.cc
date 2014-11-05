/*
  Kernel needs to be compiled with:

  -fno-rtti
  -fno-exceptions
  -fno-omit-frame-pointer

  Do not link glibc/etc, but rather link this


*/

#include <naut_types.h>
//#include <libccompat.h>

typedef enum {
    _URC_NO_REASON = 0,
    _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
    _URC_FATAL_PHASE2_ERROR = 2,
    _URC_FATAL_PHASE1_ERROR = 3,
    _URC_NORMAL_STOP = 4,
    _URC_END_OF_STACK = 5,
    _URC_HANDLER_FOUND = 6,
    _URC_INSTALL_CONTEXT = 7,
    _URC_CONTINUE_UNWIND = 8
} _Unwind_Reason_Code;

struct _Unwind_Exception { 
    void* empty;
};

struct _Unwind_Context {
    void * empty;
};

void * __dso_handle;

extern "C" void printk(const char *fmt, ...);
extern "C" void *malloc(size_t);
extern "C" void *free(void *);
extern "C" void panic(const char * fmt, ...);

#define BAD() panic("Undefined C++ function (%s)\n", __func__)

// Called when a pure virtual function call is attempted
extern "C" void __cxa_pure_virtual(void)
{
  BAD();
}

extern "C" int __cxa_atexit(void (*destructor)(void*), void * arg, void * __dso_handle)
{
    BAD();
    return -1;
}

extern "C" void __cxa_finalize(void *f)
{
    BAD();
}

// Not really needed since we will compile with -fno-rtti and -fno-exceptions
extern "C" void _Unwind_Resume(void)
{
  BAD();
}

extern "C" _Unwind_Reason_Code _Unwind_Resume_or_Rethrow(struct _Unwind_Exception * exception_object)
{
    BAD();
    return _URC_NO_REASON;
}

extern "C" void _Unwind_SetGR(struct _Unwind_Context *context,
       int index,
       uint64_t new_value)
{
    BAD();
}

extern "C" uint64_t _Unwind_GetIP(struct _Unwind_Context *context)
{
    BAD();
    return 0;
}

extern "C" void _Unwind_SetIP(struct _Unwind_Context *context, uint64_t new_value)
{
    BAD();
}

extern "C" uint64_t _Unwind_GetIPInfo (struct _Unwind_Context *context, int *ip_before_insn)
{
    BAD();
    return 0;
}

extern "C" _Unwind_Reason_Code _Unwind_RaiseException(struct _Unwind_Exception *exception_object)
{
    BAD();
    return _URC_NO_REASON;
}

extern "C" void _Unwind_DeleteException(struct _Unwind_Exception *exception_object)
{
    BAD();
}

extern "C" uint64_t _Unwind_GetLanguageSpecificData (struct _Unwind_Context *context){
    BAD();
    return 0;
}

extern "C" uint64_t _Unwind_GetRegionStart(struct _Unwind_Context *context)
{
    BAD();
    return 0;
}


extern "C" uint64_t _Unwind_GetTextRelBase(struct _Unwind_Context *context)
{
    BAD();
    return 0;
}

extern "C" uint64_t _Unwind_GetDataRelBase(struct _Unwind_Context * context)
{
    BAD();
    return 0;
}


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
        BAD();
    }

    void __throw_length_error(char const * c) {
        BAD();
    }
}


