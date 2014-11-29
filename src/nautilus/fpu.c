#include <nautilus/nautilus.h>
#include <nautilus/printk.h>
#include <nautilus/fpu.h>
#include <nautilus/cpu.h>
#include <nautilus/cpuid.h>
#include <nautilus/msr.h>
#include <nautilus/cga.h>

#define _FPU_FEAT_QUERY(r, feat)  \
    ({ \
     cpuid_ret_t ret; \
     struct cpuid_e ## r ## x_flags f; \
     cpuid(CPUID_FEATURE_INFO, &ret); \
     f.val = ret.r; \
     f.feat; \
     })
     
#define FPU_ECX_FEAT_QUERY(feat) _FPU_FEAT_QUERY(c, feat)
#define FPU_EDX_FEAT_QUERY(feat) _FPU_FEAT_QUERY(d, feat)

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
    uint8_t x87_ready = 0;
    uint8_t sse_ready = 0;

    printk("Probing for Floating Point/SIMD extensions...\n");
    
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
    }

    if (has_sse2()) {
        printk("\t[SSE2]\n");
    }

    if (has_sse3()) {
        printk("\t[SSE3]\n");
    }

    if (has_fxsr()) {
        ++sse_ready;
        printk("\t[FXSAVE/RESTORE]\n");
    } else {
        panic("No FXSAVE/RESTORE support. Thread switching will be broken\n");
    }

    if (has_xsave()) {
        printk("\t[XSAVE/RESTORE]\n");
    }

    if (has_ssse3()) {
        printk("\t[SSSE3]\n");
    }

    if (has_sse4d1()) {
        printk("\t[SSE4.1]\n");
    }

    if (has_sse4d2()) {
        printk("\t[SSE4.2]\n");
    }

    if (has_mmx()) {
        printk("\t[MMX]\n");
    }

    if (has_avx()) {
        printk("\t[AVX]\n");
    }

    if (has_cx16()) {
        printk("\t[CX16]\n");
    }

    if (has_cvt16()) {
        printk("\t[CVT16]\n");
    }

    if (has_fma4()) {
        printk("\t[FMA4]\n");
    }

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
