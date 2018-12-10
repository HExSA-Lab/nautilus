/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */


/*
  Kernel needs to be compiled with:

  -fno-rtti
  -fno-exceptions
  -fno-omit-frame-pointer

  Do not link glibc/etc, but rather link this


*/

#include <nautilus/naut_types.h>
#include <nautilus/cxxglue.h>
#include <nautilus/mm.h>

void * __dso_handle;
unsigned __atexit_func_count = 0;
atexit_func_entry_t __atexit_funcs[ATEXIT_MAX_FUNCS];

extern "C" void printk(const char *fmt, ...);
extern "C" void panic(const char * fmt, ...);

#define BAD() panic("Undefined C++ function (%s)\n", __func__)

// Called when a pure virtual function call is attempted
void 
__cxa_pure_virtual (void)
{
  BAD();
}


int 
__cxa_atexit (void (*destructor)(void*), void * arg, void * dso_handle)
{

    if (__atexit_func_count >= ATEXIT_MAX_FUNCS) {
        return -1;
    }

    __atexit_funcs[__atexit_func_count].destructor_func = destructor;
    __atexit_funcs[__atexit_func_count].obj_ptr         = arg;
    __atexit_funcs[__atexit_func_count].dso_handle      = dso_handle;
    __atexit_func_count++;
    return 0;
}


void 
__cxa_finalize (void *f)
{
    unsigned i = __atexit_func_count;
    /* "If f is NULL, it shall call all the termination funtions." */
    if (!f) {
        while (i--) {
            if (__atexit_funcs[i].destructor_func) {
                (*__atexit_funcs[i].destructor_func)(__atexit_funcs[i].obj_ptr);
            }
        }
    } else {

        for (; i != 0 ; --i) {
            if (__atexit_funcs[i].destructor_func == f) {
                (*__atexit_funcs[i].destructor_func)(__atexit_funcs[i].obj_ptr);
                __atexit_funcs[i].destructor_func = NULL;
            }
        }
    }
}

// Not really needed since we will compile with -fno-rtti and -fno-exceptions
void _Unwind_Resume(void)
{
  BAD();
}


extern "C" _Unwind_Reason_Code 
_Unwind_Resume_or_Rethrow (struct _Unwind_Exception * exception_object)
{
    BAD();
    return _URC_NO_REASON;
}


extern "C" void 
_Unwind_SetGR (struct _Unwind_Context *context,
       int index,
       uint64_t new_value)
{
    BAD();
}


extern "C" uint64_t 
_Unwind_GetIP (struct _Unwind_Context *context)
{
    BAD();
    return 0;
}


extern "C" void 
_Unwind_SetIP (struct _Unwind_Context *context, uint64_t new_value)
{
    BAD();
}


extern "C" uint64_t 
_Unwind_GetIPInfo (struct _Unwind_Context *context, int *ip_before_insn)
{
    BAD();
    return 0;
}


extern "C" _Unwind_Reason_Code 
_Unwind_RaiseException (struct _Unwind_Exception *exception_object)
{
    BAD();
    return _URC_NO_REASON;
}


extern "C" void 
_Unwind_DeleteException (struct _Unwind_Exception *exception_object)
{
    BAD();
}


extern "C" uint64_t 
_Unwind_GetLanguageSpecificData (struct _Unwind_Context *context){
    BAD();
    return 0;
}


extern "C" uint64_t 
_Unwind_GetRegionStart(struct _Unwind_Context *context)
{
    BAD();
    return 0;
}


extern "C" uint64_t 
_Unwind_GetTextRelBase(struct _Unwind_Context *context)
{
    BAD();
    return 0;
}


extern "C" uint64_t 
_Unwind_GetDataRelBase(struct _Unwind_Context * context)
{
    BAD();
    return 0;
}


void *operator 
new (size_t size)
{
  return malloc(size);
}


void *operator 
new[] (size_t size)
{
  return malloc(size);
}


void operator 
delete (void *p)
{
  free(p);
}


void operator 
delete[] (void *p)
{
  free(p);
}


/*
namespace std {

    void __throw_bad_alloc(void) {
        BAD();
    }

    void __throw_length_error(char const * c) {
        BAD();
    }
}
*/


