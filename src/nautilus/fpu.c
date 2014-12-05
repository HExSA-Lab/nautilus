#include <nautilus/nautilus.h>
#include <nautilus/printk.h>
#include <nautilus/fpu.h>
#include <nautilus/cpu.h>
#include <nautilus/cpuid.h>
#include <nautilus/msr.h>
#include <nautilus/cga.h>

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
        printk("\t[" #str "]\n"); \
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
    uint32_t m = MXCSR_ZM;

    /* NOTE: assuming EM and MP bit in CR0 are already set */

    r |= CR4_OSFXSR |    // OS supports save/restore of FPU regs
         CR4_OSXMMEXCPT; // OS can handle SIMD floating point exceptions

    write_cr4(r);

    /* bang on the EFER so we dont' get fast FXSAVE */
    r = msr_read(IA32_MSR_EFER);
    r &= ~(EFER_FFXSR);
    msr_write(IA32_MSR_EFER, r);

    // Enable div by zero exceptions
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
    printk("Probing for AMD-specific FPU/SIMD extensions\n");
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
    printk("Probing for Intel specific FPU/SIMD extensions\n");
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
        printk("\t[x87]\n");
        x87_ready = 1;
    }

    if (has_sse()) {
        ++sse_ready;
        printk("\t[SSE]\n");
    }

    if (has_clflush()) {
        ++sse_ready;
        printk("\t[CLFLUSH]\n");
    }

    DEFAULT_FUN_CHECK(has_sse2, SSE2)
    DEFAULT_FUN_CHECK(has_sse2, SSE2)

    if (has_fxsr()) {
        ++sse_ready;
        printk("\t[FXSAVE/RESTORE]\n");
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
        printk("\tInitializing legacy x87 FPU\n");
        enable_x87();
    }

    /* did we meet SSE requirements? */
    if (sse_ready >= 3) {
        printk("\tInitializing SSE extensions\n");
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
    printk("Probing for Floating Point/SIMD extensions...\n");

    fpu_init_common(naut);

    if (nk_is_amd()) {
        amd_fpu_init(naut);
    } else if (nk_is_intel()) {
        intel_fpu_init(naut);
    } else {
        ERROR_PRINT("Unsupported processor type!\n");
        return;
    }
}
