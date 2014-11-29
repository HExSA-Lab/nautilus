#ifndef __CXXGLUE_H__
#define __CXXGLUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define ATEXIT_MAX_FUNCS 128

struct atexit_func_entry_t
{
    void (*destructor_func)(void*);
    void *obj_ptr;
    void *dso_handle;
};

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

void __cxa_pure_virtual(void);
int __cxa_atexit(void (*destructor)(void*), void * arg, void * __dso_handle);
void __cxa_finalize(void *f);
void _Unwind_Resume(void);

#ifdef __cplusplus
}
#endif

#endif
