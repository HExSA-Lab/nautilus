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
 * Copyright (c) 2018, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
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
#include <nautilus/smp.h>

#include <nautilus/backtrace.h>
#ifndef NAUT_CONFIG_DEBUG_FPU
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

//
// NOTE: Turning on debugging output for fpu initialization can
// result in failure if the compiler has optimized print-formatting
// using SSE or other similar instructions.   
//

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

#define _INTEL_FPU_EXT_FEAT_QUERY(r, feat)  \
    ({ \
     cpuid_ret_t ret; \
     struct cpuid_ext_feat_flags_e ## r ## x f; \
     cpuid_sub(CPUID_EXT_FEATURE_INFO, CPUID_EXT_FEATURE_SUB_INFO, &ret); \
     f.val = ret.r; \
     f.feat; \
     })
     
#define FPU_ECX_FEAT_QUERY(feat) _INTEL_FPU_FEAT_QUERY(c, feat)
#define FPU_EDX_FEAT_QUERY(feat) _INTEL_FPU_FEAT_QUERY(d, feat)
#define FPU_EBX_EXT_FEAT_QUERY(feat) _INTEL_FPU_EXT_FEAT_QUERY(b, feat)

#define AMD_FPU_ECX_FEAT_QUERY(feat) _AMD_FPU_FEAT_QUERY(c, feat)
#define AMD_FPU_EDX_FEAT_QUERY(feat) _AMD_FPU_FEAT_QUERY(d, feat)

#define DEFAULT_FUN_CHECK(fun, str) \
    if (fun()) { \
        FPU_DEBUG("\t[" #str "]\n"); \
    }


extern uint8_t cpu_info_ready;

static inline uint16_t
get_x87_status (void)
{
    uint16_t status;
    asm volatile("fnstsw %[_a]"  : [_a] "=a" (status) : :);

    return status;
}

static inline void
set_x87_ctrl (uint16_t val)
{
    asm volatile ("fldcw %[_m]" :: [_m] "m" (val) : "memory");
}

static inline uint16_t 
get_x86_ctrl (void)
{
    uint16_t ctl;

    asm volatile("fnstcw %[_m]" : [_m] "=m" (ctl) :: "memory");

    return ctl;
}

static inline void
clear_x87_excp (void)
{
    asm volatile ("fnclex" :::);
}

int mf_handler (excp_entry_t * excp, excp_vec_t vec, void *state)
{
    cpu_id_t cpu_id = cpu_info_ready ? my_cpu_id() : 0xffffffff;
    unsigned tid = cpu_info_ready ? get_cur_thread()->tid : 0xffffffff;
    FPU_WARN("x87 Floating Point Exception (RIP=%p (core=%u, thread=%u)\n",
            (void*)excp->rip, cpu_id, tid);

    clear_x87_excp();

    return 0;
}


int xm_handler (excp_entry_t * excp, excp_vec_t vec, void *state)
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

    /*
     * - mask all x87 exceptions
     * - double extended precision
     * - round to nearest (even)
     * - infinity ctrl N/A
     *
     * (fninit should set to this value,
     * but being paranoid)
     */ 
    set_x87_ctrl(0x037f);
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
has_avx2 (void)
{
    return FPU_EBX_EXT_FEAT_QUERY(avx2);
}

static uint8_t
has_avx512f (void)
{
    return FPU_EBX_EXT_FEAT_QUERY(avx512f);
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
    cpuid_sub(0x0d, 0, &r);
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
enable_xsave (void)
{
    /* Enables XSAVE features by reading CR4 and writing OSXSAVE bit */
    set_osxsave();

    /* XSAVE feature setup done later after we figure out support of AVX, AVX512f and SSE */
     
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
    uint8_t xsave_ready = 0;
    uint8_t avx_ready = 0;
    uint8_t avx2_ready = 0;
    uint8_t avx512f_ready = 0;
    uint32_t xsave_support = 0;

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

    if (has_xsave()) {
        ++xsave_ready;
        FPU_DEBUG("\t[XSAVE/RESTORE]\n");
    }

    if (has_avx()) {
        ++avx_ready;
        FPU_DEBUG("\t[AVX]\n");
    }

    if (has_avx2()) {
        ++avx2_ready;
        FPU_DEBUG("\t[AVX2]\n");
    }

    if (has_avx512f()) {
        ++avx512f_ready;
        FPU_DEBUG("\t[AVX512]\n");
    }

    DEFAULT_FUN_CHECK(has_sse4d1, SSE4.1)
    DEFAULT_FUN_CHECK(has_sse4d2, SSE4.2)
    DEFAULT_FUN_CHECK(has_mmx, MMX)

    /* should we turn on x87? */
    if (x87_ready) {
        FPU_DEBUG("\tInitializing legacy x87 FPU\n");
        enable_x87();
    }

    /* does processor have xsave instructions? */
    #ifdef NAUT_CONFIG_XSAVE_SUPPORT
    if (xsave_ready) {
        FPU_DEBUG("\tInitializing XSAVE instructions\n");
        enable_xsave();
        /* x87 support is non-optional if xsave enabled */
        xsave_support |= 0x1;
    }
    #endif

    /* did we meet SSE requirements? */
    if (sse_ready >= 3) {
        FPU_DEBUG("\tInitializing SSE extensions\n");
        enable_sse();
        /* If we want XSAVE to save SSE registers, add it to bit mask */
        #ifdef NAUT_CONFIG_XSAVE_SSE_SUPPORT
        if (xsave_support >= 1) {
            xsave_support |= 0x2;
        }
        #endif
    }

    #ifdef NAUT_CONFIG_XSAVE_AVX_SUPPORT
    /* Does processor have AVX registers? */
    if (avx_ready) {
        /* Can only enable XSAVE AVX support if processor has SSE support */
        if (xsave_support >= 3) {
            FPU_DEBUG("\tInitializing XSAVE AVX support\n");
            xsave_support |= 0x4;
        }
    }
    #endif

    /* Does processor have AVX2 registers? */
    if (avx2_ready) {
        FPU_DEBUG("\tInitializing AVX2 support\n");
    }

    #ifdef NAUT_CONFIG_XSAVE_AVX512F_SUPPORT
    // Does processor have AVX512f registers?
    if (avx512f_ready) {
        /* Can only enable AVX512f if processor has SSE and AVX support */
        if (xsave_support >= 7 && avx_ready) {
            FPU_DEBUG("\tInitializing AVX512f support\n");
            /* Bits correspond to AVX512 opmasks, top half of lower ZMM regs, and upper ZMM regs */
            xsave_support |= 0xe0;
        }
    }
    #endif
    
    #ifdef NAUT_CONFIG_XSAVE_SUPPORT
    /* Configure XSAVE Support */
    if (xsave_ready) {
        xsave_support &= get_xsave_features();
        asm volatile ("xor %%rcx, %%rcx ;"
                      "xsetbv ;"
                      : : "a"(xsave_support) : "rcx", "memory");
    }
    #endif
}

/* 
 * this just ensures that we have
 * SSE and SSE2. Pretty sure that long mode
 * requires them so chances are if we make it
 * to this point then we do...
 *
 */
void
fpu_init (struct naut_info * naut, int is_ap)
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

    if (is_ap == 0) {

        if (register_int_handler(XM_EXCP, xm_handler, NULL) != 0) {
            ERROR_PRINT("Could not register excp handler for XM\n");
            return;
        }

        if (register_int_handler(MF_EXCP, mf_handler, NULL) != 0) {
            ERROR_PRINT("Could not register excp handler for MF\n");
            return;
        }

    }
}
