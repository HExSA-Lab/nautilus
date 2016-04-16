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
#include <nautilus/nautilus.h>
#include <nautilus/printk.h>
#include <nautilus/fpu.h>
#include <nautilus/cpu.h>
#include <nautilus/cpuid.h>
#include <nautilus/idt.h>
#include <nautilus/irq.h>
#include <nautilus/msr.h>

#ifndef NAUT_CONFIG_DEBUG_FPU
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define FPU_PRINT(fmt, args...) printk("FPU: " fmt, ##args)
#define FPU_DEBUG(fmt, args...) DEBUG_PRINT("FPU: " fmt, ##args)
#define FPU_WARN(fmt, args...)  WARN_PRINT("FPU: " fmt, ##args)

#define _INTEL_FPU_FEAT_QUERY(r, feat)  \
    ({ \
     cpuid_ret_t ret; \
     struct cpuid_e ## r ## x_flags f; \
     cpuid(CPUID_FEATURE_INFO, &ret); \
     f.val = ret.r; \
     f.feat; \
     })

#define _AMD_FPU_FEAT_QUERY(r, feat)  \
    ({ \
     cpuid_ret_t ret; \
     struct cpuid_amd_e ## r ## x_flags f; \
     cpuid(CPUID_AMD_FEATURE_INFO, &ret); \
     f.val = ret.r; \
     f.feat; \
     })
     
#define FPU_ECX_FEAT_QUERY(feat) _INTEL_FPU_FEAT_QUERY(c, feat)
#define FPU_EDX_FEAT_QUERY(feat) _INTEL_FPU_FEAT_QUERY(d, feat)

#define AMD_FPU_ECX_FEAT_QUERY(feat) _AMD_FPU_FEAT_QUERY(c, feat)
#define AMD_FPU_EDX_FEAT_QUERY(feat) _AMD_FPU_FEAT_QUERY(d, feat)

#define DEFAULT_FUN_CHECK(fun, str) \
    if (fun()) { \
        FPU_DEBUG("\t[" #str "]\n"); \
    }



int mf_handler (excp_entry_t * excp, excp_vec_t vec);
int 
mf_handler (excp_entry_t * excp, excp_vec_t vec)
{
    panic("x86 Floating Point Exception\n");
    return 0;
}


int xm_handler (excp_entry_t * excp, excp_vec_t vec);
int
xm_handler (excp_entry_t * excp, excp_vec_t vec)
{
    uint32_t m;
    asm volatile ("stmxcsr %[_m]" : [_m] "=m" (m) : : "memory");
    FPU_WARN("SIMD Floating point exception (MXCSR=0x%x)\n", m);
    return 0;
}


static uint8_t 
has_x87 (void)
{
    return FPU_EDX_FEAT_QUERY(fpu);
}

static void
enable_x87 (void)
{
    ulong_t r;

    r = read_cr0();
    r |= CR0_MP | // If TS flag is set, WAIT/FWAIT generate #NM exception (for lazy switching)
         CR0_NE;  // Generate FP exceptions internally, not on a PIC

    write_cr0(r);

    asm volatile ("fninit" ::: "memory");
}

static uint8_t 
has_clflush (void)
{
    return FPU_EDX_FEAT_QUERY(clfsh);
}

static uint8_t 
has_mmx (void)
{
    return FPU_EDX_FEAT_QUERY(mmx);
}

static uint8_t
amd_has_mmx_ext (void)
{
    return AMD_FPU_EDX_FEAT_QUERY(mmx_ext);
}

static uint8_t
amd_has_3dnow (void)
{
    return AMD_FPU_EDX_FEAT_QUERY(amd3dnow);
}

static uint8_t 
amd_has_3dnow_ext (void)
{
    return AMD_FPU_EDX_FEAT_QUERY(amd3dnowext);
}

static uint8_t
has_sse (void) 
{
    return FPU_EDX_FEAT_QUERY(sse);
}

static uint8_t
has_sse2 (void)
{
    return FPU_EDX_FEAT_QUERY(sse2);
}

static uint8_t
has_sse3 (void)
{
    return FPU_ECX_FEAT_QUERY(sse3);
}

static uint8_t
has_ssse3 (void)
{
    return FPU_ECX_FEAT_QUERY(ssse3);
}

static uint8_t
has_sse4d1 (void)
{
    return FPU_ECX_FEAT_QUERY(sse4dot1);
}

static uint8_t
has_sse4d2 (void)
{
    return FPU_ECX_FEAT_QUERY(sse4dot2);
}

static uint8_t
amd_has_sse4a (void)
{
    return AMD_FPU_ECX_FEAT_QUERY(sse4a);
}

static uint8_t
amd_has_prefetch (void)
{
    return AMD_FPU_ECX_FEAT_QUERY(prefetch3d);
}

static uint8_t
amd_has_misal_sse (void)
{
    return AMD_FPU_ECX_FEAT_QUERY(misalignsse);
}

static void
enable_sse (void)
{
    ulong_t r = read_cr4();
    uint32_t m;

    /* NOTE: assuming EM and MP bit in CR0 are already set */

    r |= CR4_OSFXSR |    // OS supports save/restore of FPU regs
         CR4_OSXMMEXCPT; // OS can handle SIMD floating point exceptions

    write_cr4(r);

    /* bang on the EFER so we dont' get fast FXSAVE */
    r = msr_read(IA32_MSR_EFER);
    r &= ~(EFER_FFXSR);
    msr_write(IA32_MSR_EFER, r);

    m = 0x00001f80; // mask all FPU exceptions, no denormals are zero
    asm volatile ("ldmxcsr %[_m]" :: [_m] "m" (m) : "memory");
}


static uint8_t
has_fma4 (void)
{
    return FPU_ECX_FEAT_QUERY(fma);
}

static uint8_t
amd_has_fma4 (void)
{
    return AMD_FPU_ECX_FEAT_QUERY(fma4);
}

static uint8_t
has_cvt16 (void)
{
    return FPU_ECX_FEAT_QUERY(f16c);
}


static uint8_t
has_cx16 (void)
{
    return FPU_ECX_FEAT_QUERY(cx16);
}

static uint8_t
has_avx (void)
{
    return FPU_ECX_FEAT_QUERY(avx);
}


static uint8_t
has_fxsr (void)
{
    return FPU_EDX_FEAT_QUERY(fxsr);
}


static uint8_t
has_xsave (void)
{
    return FPU_ECX_FEAT_QUERY(xsave);
}

static uint32_t 
get_xsave_features (void)
{
    cpuid_ret_t r;
    cpuid_sub(0, 1, &r);
    return r.a;
}

static void
set_osxsave (void)
{
    ulong_t r = read_cr4();
    r |= CR4_OSXSAVE;
    write_cr4(r);
}

static void 
amd_fpu_init (struct naut_info * naut)
{
    FPU_DEBUG("Probing for AMD-specific FPU/SIMD extensions\n");
    DEFAULT_FUN_CHECK(amd_has_fma4, FMA4)
    DEFAULT_FUN_CHECK(amd_has_mmx_ext, AMDMMXEXT)
    DEFAULT_FUN_CHECK(amd_has_sse4a, SSE4A)
    DEFAULT_FUN_CHECK(amd_has_3dnow, 3DNOW)
    DEFAULT_FUN_CHECK(amd_has_3dnow_ext, 3DNOWEXT)
    DEFAULT_FUN_CHECK(amd_has_prefetch, PREFETCHW)
    DEFAULT_FUN_CHECK(amd_has_misal_sse, MISALSSE)
}

static void 
intel_fpu_init (struct naut_info * naut)
{
    FPU_DEBUG("Probing for Intel specific FPU/SIMD extensions\n");
    DEFAULT_FUN_CHECK(has_cx16, CX16)
    DEFAULT_FUN_CHECK(has_cvt16, CVT16)
    DEFAULT_FUN_CHECK(has_fma4, FMA4)
    DEFAULT_FUN_CHECK(has_ssse3, SSSE3)
}

static void
fpu_init_common (struct naut_info * naut)
{
    uint8_t x87_ready = 0;
    uint8_t sse_ready = 0;

    if (has_x87()) {
        FPU_DEBUG("\t[x87]\n");
        x87_ready = 1;
    }

    if (has_sse()) {
        ++sse_ready;
        FPU_DEBUG("\t[SSE]\n");
    }

    if (has_clflush()) {
        ++sse_ready;
        FPU_DEBUG("\t[CLFLUSH]\n");
    }

    DEFAULT_FUN_CHECK(has_sse2, SSE2)
    DEFAULT_FUN_CHECK(has_sse2, SSE2)

    if (has_fxsr()) {
        ++sse_ready;
        FPU_DEBUG("\t[FXSAVE/RESTORE]\n");
    } else {
        panic("No FXSAVE/RESTORE support. Thread switching will be broken\n");
    }

    DEFAULT_FUN_CHECK(has_xsave, XSAVE/RESTORE)
    DEFAULT_FUN_CHECK(has_sse4d1, SSE4.1)
    DEFAULT_FUN_CHECK(has_sse4d2, SSE4.2)
    DEFAULT_FUN_CHECK(has_mmx, MMX)
    DEFAULT_FUN_CHECK(has_avx, AVX)

    /* should we turn on x87? */
    if (x87_ready) {
        FPU_DEBUG("\tInitializing legacy x87 FPU\n");
        enable_x87();
    }

    /* did we meet SSE requirements? */
    if (sse_ready >= 3) {
        FPU_DEBUG("\tInitializing SSE extensions\n");
        enable_sse();
    }
}

/* 
 * this just ensures that we have
 * SSE and SSE2. Pretty sure that long mode
 * requires them so chances are if we make it
 * to this point then we do...
 *
 */
void
fpu_init (struct naut_info * naut)
{
    FPU_DEBUG("Probing for Floating Point/SIMD extensions...\n");

    fpu_init_common(naut);

    if (nk_is_amd()) {
        amd_fpu_init(naut);
    } else if (nk_is_intel()) {
        intel_fpu_init(naut);
    } else {
        ERROR_PRINT("Unsupported processor type!\n");
        return;
    }

    if (register_int_handler(XM_EXCP, xm_handler, NULL) != 0) {
        ERROR_PRINT("Could not register excp handler for XM\n");
        return;
    }

    if (register_int_handler(MF_EXCP, mf_handler, NULL) != 0) {
        ERROR_PRINT("Could not register excp handler for MF\n");
        return;
    }

}
