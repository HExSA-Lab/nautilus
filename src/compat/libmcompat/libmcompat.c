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
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter Dinda <pdinda@nodthwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/* 
 *
 * This file is intended to support libm functions
 * 
 *
 */


#define LIBMCOMPAT 1

#include <nautilus/libccompat.h>
#include <nautilus/nautilus.h>


#ifdef NAUT_CONFIG_DEBUG_BASE_LIBM_COMPAT
#define DEBUG(fmt, args...) DEBUG_PRINT("libm: " fmt, ##args)
#else
#define DEBUG(fmt, args...)
#endif
#define INFO(fmt, args...) INFO_PRINT("libm: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("libm: " fmt, ##args)
#define WARN(fmt, args...)  WARN_PRINT("libm: " fmt, ##args)


#define GEN_UNDEF(t,f,v)			\
    t f () {   				        \
        UNDEF_FUN_ERR();			\
        return v;				\
    } 


#define BOGUS()    BOGUS_FUN_ERR()



union dbl {
    double d;
    uint64_t ui;
    struct {
        uint64_t frac:52;
        uint32_t exp:11;
        uint32_t sign:1;
    } __packed;
} __packed;

union flt {
    float f;
    uint32_t ui;
    struct {
        uint32_t frac:23;
        uint32_t exp:8;
        uint32_t sign:1;
    } __packed;
} __packed;


double fabs(double x)
{
    union dbl f = {.d = x};
    f.sign = 0;
    return f.d;
}

double fmod(double y, double x)
{
    BOGUS();
    return (int)y % (int)x;
}
 
double fmodnew(int y, int x)
{
    BOGUS();
    return y -x*(y/x);
}

double modf(double y, double *x)
{
    BOGUS();
    *x = 0;
    return 0.000;
}


double frexp(double x, int *e)
{
    BOGUS();
    *e = 0;
    return 0.5;
}

double ldexp(double x, int exp)
{
    BOGUS();
    union dbl f = {.d = x};
    f.exp+=exp;
    return f.d;
}

GEN_UNDEF(double,sin,0.0)
GEN_UNDEF(double,sinh,0.0)
GEN_UNDEF(double,cos,0.0)
GEN_UNDEF(double,cosh,0.0)
GEN_UNDEF(double,tan,0.0)
GEN_UNDEF(double,tanh,0.0)
GEN_UNDEF(double,asin,0.0)
GEN_UNDEF(double,acos,0.0)
GEN_UNDEF(double,atan,0.0)
GEN_UNDEF(double,atan2,0.0)

double ceil(double x)
{
    BOGUS();
    return x;
}

double floor(double x)
{
    BOGUS();
    return x;
}

double sqrt(double x)
{
    BOGUS();
    return x;
}

double pow(double x, double y)
{
    BOGUS();
    union dbl f = {.d = x};
    f.exp*=y;
    return f.d;
}

double log(double x)
{
    BOGUS();
    return x;
}

double log10(double x)
{
    BOGUS();
    return x;
}

double exp(double x)
{
    BOGUS();
    return x;
}

#define CLD double // complex long double
#define CD  double // complex double
#define CF  float  // complex float

GEN_UNDEF(CLD,__mulxc3,0.0)
GEN_UNDEF(CD,__muldc3,0.0)
GEN_UNDEF(CF,__mulsc3,0.0)
GEN_UNDEF(CLD,__divxc3,0.0)
GEN_UNDEF(CD,__divdc3,0.0)
GEN_UNDEF(CF,__divsc3,0.0)


