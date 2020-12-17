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
#ifndef __CPUID_H__
#define __CPUID_H__

#include <nautilus/intrinsics.h>
#include <nautilus/naut_types.h>

uint32_t cpuid_leaf_max(void);
uint32_t cpuid_ext_leaf_max(void);
uint8_t cpuid_get_family(void);
uint8_t cpuid_get_model(void);
uint8_t cpuid_get_step(void);
void detect_cpu(void);

#define CPUID_BASIC_INFO            0x0
#define CPUID_FEATURE_INFO          0x1
#define CPUID_EXT_FEATURE_INFO      0x7
#define CPUID_EXT_FEATURE_SUB_INFO  0x7
#define CPUID_AMD_BASIC_INFO        0x80000000
#define CPUID_AMD_FEATURE_INFO      0x80000001
#define CPUID_AMD_EXT_INFO          0x80000008

#define CPUID_GET_BASE_FAM(x)   (((x) >> 8) & 0xfu)
#define CPUID_GET_BASE_MOD(x)   (((x) >> 4) & 0xfu)
#define CPUID_GET_PROC_STEP(x)  ((x) & 0xfu)
#define CPUID_GET_EXT_FAM(x)    (((x) >> 20) & 0xffu)
#define CPUID_GET_EXT_MOD(x)    (((x) >> 16) & 0xfu)

#define CPUID_HAS_FEATURE_ECX(flags, feat) ((flags).ecx.##feat)
#define CPUID_HAS_FEATURE_EDX(flags, feat) ((flags).edx.##feat)

#define CPUID_LEAF_BASIC_INFO0   0x0
#define CPUID_LEAF_BASIC_INFO1   0x1
#define CPUID_LEAF_BASIC_INFO2   0x2
#define CPUID_LEAF_BASIC_INFO3   0x3
#define CPUID_LEAF_CACHE_PARM    0x4
#define CPUID_LEAF_MWAIT         0x5
#define CPUID_LEAF_THERMAL       0x6
#define CPUID_LEAF_EXT_FEATS     0x7
#define CPUID_LEAF_CACHE_ACCESS  0x9
#define CPUID_LEAF_PERF_MON      0xa
#define CPUID_LEAF_TOP_ENUM      0xb
#define CPUID_LEAF_EXT_STATE     0xd
#define CPUID_LEAF_QOS_MON       0xf
#define CPUID_LEAF_QOS_ENF       0x10

#define CPUID_EXT_FUNC_MAXVAL    0x80000000
#define CPUID_EXT_FUNC_SIG_FEAT  0x80000001
#define CPUID_EXT_FUNC_BRND_STR0 0x80000002
#define CPUID_EXT_FUNC_BRND_STR1 0x80000003
#define CPUID_EXT_FUNC_BRND_STR2 0x80000004
#define CPUID_EXT_FUNC_RSVD      0x80000005
#define CPUID_EXT_FUNC_CACHE1    0x80000006
#define CPUID_EXT_FUNC_INV_TSC   0x80000007
#define CPUID_EXT_FUNC_ADDR_SZ   0x80000008


/* the following were taken from 
 * Intel Instruction Set Reference Vol 2A 3-171 (rev. Feb 2014)
 */
struct cpuid_ecx_flags {
    union {
        uint32_t val;
        struct {
            uint8_t sse3     : 1; // Streaming SIMD Extensions 3 (SSE3)
            uint8_t pclmuldq : 1; // PCLMULQDQ instruction
            uint8_t dtest64  : 1; // 64-bit DS Area
            uint8_t monitor  : 1; // MONITOR/MWAIT
            uint8_t ds_cpl   : 1; // CPL Qualified Debug Store
            uint8_t vmx      : 1; // Virtual machine extensions
            uint8_t smx      : 1; // Safer mode extensions
            uint8_t eist     : 1; // Enchanced Intel SpeedStep technology
            uint8_t tm2      : 1; // Thermal Monitor 2
            uint8_t ssse3    : 1; // Supplemental Streaming SIMD Extensions 3 (SSSE3)
            uint8_t cnxt_id  : 1; // L1 Context ID
            uint8_t sdbg     : 1; // supports IA32_DEBUG_INTERFACE MSR
            uint8_t fma      : 1; // FMA extensions using YMM state
            uint8_t cx16     : 1; // CMPXCHG16B instruction available
            uint8_t xtpr     : 1; // xTPR update control
            uint8_t pdcm     : 1; // perfmon and debug capability
            uint8_t rsvd0    : 1; 
            uint8_t pcid     : 1; // process-context identifiers
            uint8_t dca      : 1; // can prefetch data from memory mapped device
            uint8_t sse4dot1 : 1; // SSE 4.1
            uint8_t sse4dot2 : 1; // SSE 4.2
            uint8_t x2apic   : 1; // x2APIC
            uint8_t movbe    : 1; // MOVBE instruction
            uint8_t popcnt   : 1; // POPCNT instruction
            uint8_t tsc_dline: 1; // local APIC timer supports one-shot operation using TSC deadline value
            uint8_t aesni    : 1; // AESNI instruction extensions
            uint8_t xsave    : 1; // XSAVE/XRSTOR
            uint8_t osxsave  : 1; // OS has set CR4.OSXSAVE (bit 18) to enable the XSAVE feature set
            uint8_t avx      : 1; // AVX instruction extensions
            uint8_t f16c     : 1; // 16-bit floating-point conversion instructions
            uint8_t rdrand   : 1; // RDRAND instruction
            uint8_t notused  : 1; // always returns 0
        } __packed;
    } __packed;
} __packed;

struct cpuid_edx_flags {
    union {
        uint32_t val;
        struct {
            uint8_t fpu      : 1; // floating point unit on-chip
            uint8_t vme      : 1; // virtual 8086 mode enhancements
            uint8_t de       : 1; // debugging extensions
            uint8_t pse      : 1; // page size extension
            uint8_t tsc      : 1; // time stamp counter
            uint8_t msr      : 1; // model specific registers RDMSR and WRMSR instructions
            uint8_t pae      : 1; // physical address extension
            uint8_t mce      : 1; // machine check exception
            uint8_t cx8      : 1; // cmpxchg8b instruction
            uint8_t apic     : 1; // apic on-chip
            uint8_t rsvd0    : 1;
            uint8_t sep      : 1; // SYSENTER and SYSEXIT instructions
            uint8_t mtrr     : 1; // memory type range registers
            uint8_t pge      : 1; // page global bit
            uint8_t mca      : 1; // machine check architecture
            uint8_t cmov     : 1; // conditional move instructions
            uint8_t pat      : 1; // page attribute table
            uint8_t pse_36   : 1; // 36-bit page size extension
            uint8_t psn      : 1; // processor serial number
            uint8_t clfsh    : 1; // CLFLUSH instruction
            uint8_t rsvd1    : 1; 
            uint8_t ds       : 1; // debug store
            uint8_t acpi     : 1; // thermal monitor and software controlled clock facilities
            uint8_t mmx      : 1; // intel mmx technology
            uint8_t fxsr     : 1; // FXSAVE and FXRSTOR instructions
            uint8_t sse      : 1; // SSE
            uint8_t sse2     : 1; // SSE2
            uint8_t ss       : 1; // self snoop
            uint8_t htt      : 1; // max APIC IDs reserved field is valid
            uint8_t tm       : 1; // thermal monitor
            uint8_t rsvd2    : 1; 
            uint8_t pbe      : 1; //pending break enable
        } __packed;
    } __packed;
} __packed;

/* AMD CPUID Specification Rev 2.34 Sept 2010, pp. 20--22 */
struct cpuid_amd_ecx_flags {
    union {
        uint32_t val;
        struct { 
            uint8_t lahfsahf    : 1; // LAHF & SAHF instr support in long mode
            uint8_t cmplegacy   : 1; // core multi-processing legacy mode
            uint8_t svm         : 1; // secure virtual machine
            uint8_t extapicspace: 1; // extended APIC space
            uint8_t altmovcr8   : 1; // LOCK MOV CR0 means MOV CR8
            uint8_t abm         : 1; // advanced bit manipulation (LZCNT instr)
            uint8_t sse4a       : 1; // EXTRQ, INSERTQ, MOVNTSS, & MOVNTSD
            uint8_t misalignsse : 1; // misaligned SSE mode
            uint8_t prefetch3d  : 1; // PREFETCH and PREFETCHW instrs
            uint8_t osvw        : 1; // OS visible workaround
            uint8_t ibs         : 1; // instruction based sampling
            uint8_t xop         : 1; // extended operation support
            uint8_t skinit      : 1; // SKINIT and STGI are supported
            uint8_t wdt         : 1; // watch dog timer support
            uint8_t rsvd0       : 1; 
            uint8_t lwp         : 1; // lighweight profiling support
            uint8_t fma4        : 1; // 4-operand FMA instruction support
            uint8_t rsvd1       : 2; 
            uint8_t nodeid      : 1; // Indicates support for MSRC001_100C [NodeID, NodesPerProcessor]
            uint8_t rsvd2       : 1; 
            uint8_t tbm         : 1; // trailing bit manipulation instr support
            uint8_t topext      : 1; // Topology extensions support
            uint8_t rsvd3          ; 
        } __packed;
    } __packed;
} __packed;
            
struct cpuid_amd_edx_flags {
    union {
        uint32_t val;
        struct {
            uint8_t fpu         : 1; // x87 floating-point unit on-chip
            uint8_t vme         : 1; // virtual-mode enhancements
            uint8_t de          : 1; // debugging extensions
            uint8_t pse         : 1; // page-size extensions
            uint8_t tsc         : 1; // time stamp counter
            uint8_t msr         : 1; // AMD model-specific registers
            uint8_t pae         : 1; // physical-address extensions
            uint8_t mce         : 1; // machine check exception
            uint8_t cmpxchg8b   : 1; // CMPXCHG8B instruction
            uint8_t apic        : 1; // APIC support
            uint8_t rsvd0       : 1; 
            uint8_t syscall     : 1; // SYSCALL & SYSRET instructions
            uint8_t mtrr        : 1; // memory-type range registers
            uint8_t pge         : 1; // page global extensions
            uint8_t mca         : 1; // machine check architecture
            uint8_t cmov        : 1; // conditional move instruction
            uint8_t pat         : 1; // page attribute table
            uint8_t pse36       : 1; // page-size extensions
            uint8_t rsvd1       : 2; 
            uint8_t nx          : 1; // no-execute page protection
            uint8_t rsvd2       : 1; 
            uint8_t mmx_ext     : 1; // AMD extensions to MMX instructions
            uint8_t mmx         : 1; // MMX instructions
            uint8_t fxsr        : 1; // FXSAVE & FXSTOR instructions
            uint8_t ffxsr       : 1; // FXSAVE & FXSTOR instr optimizations
            uint8_t pg1gb       : 1; // 1GB page support
            uint8_t rdtscp      : 1; // RDTSCP instruction
            uint8_t rsvd3       : 1;
            uint8_t lm          : 1; // Long Mode
            uint8_t amd3dnowext : 1; // 3DNow AMD extensions 
            uint8_t amd3dnow    : 1; // 3DNow instructions 
        } __packed;
    } __packed;
} __packed;


/* Structured Extended Feature Flags Main Enumeration Leaf */
/* Last updated: 2019-08-28 according to Intel Arch. Instruction Set Extensions Programming Reference pg. 2-16*/
struct cpuid_ext_feat_flags_ebx {
    union {
        uint32_t val;
        struct {
            uint8_t fsgsbase            : 1; /* Access to base of %fs and %gs */
            uint8_t ia32_tsc_adjust     : 1; 
            uint8_t sgx                 : 1; /* software guard extensions */
            uint8_t bmi1                : 1; /* bit manipulation instruction set 1 */
            uint8_t hle                 : 1; /* transactional synchronization extensions */
            uint8_t avx2                : 1; /* advanced vector extensions 2 */
            uint8_t rsvd1               : 1; /* reserved */
            uint8_t smep                : 1; /* supervisor mode execution protection */
            uint8_t bmi2                : 1; /* bit manipulation instruction set 2 */
            uint8_t erms                : 1; /* enhanced REP MOVSB/STOSB */
            uint8_t invpcid             : 1; /* INVPCID instruction */
            uint8_t rtm                 : 1; /* transactional synchronization extensions */
            uint8_t pqm                 : 1; /* Platform quality of service monitoring */
            uint8_t deprecate_fpu       : 1; /*deprecates FPU CS and FPU DS values */
            uint8_t intel_mem_prot_ext  : 1; /* memory protection extensions */
            uint8_t pqe                 : 1; /* platform quality of service enforcement */
            uint8_t avx512f             : 1; /* AVX512 Foundation (core extension) */
            uint8_t avx512dq            : 1; /* AVX512 Doubleword and Quadword Extensions */
            uint8_t rdseed              : 1; /* RDSEED instruction */
            uint8_t adx                 : 1; /* ADX (multi-precision add-carry instruction extensions */
            uint8_t smap                : 1; /* Supervisor mode access prevention */
            uint8_t avx512ifma          : 1; /* AVX512 Integer fused multiply-add instructions */
            uint8_t pcommit             : 1; /* PCOMMIT instruction */
            uint8_t clflushopt          : 1; /* CLFLUSHOPT instruction */
            uint8_t clwb                : 1; /* CLWB Instruction */
            uint8_t ipt                 : 1; /* intel processor trace */
            uint8_t avx512pf            : 1; /* AVX512 Prefetch Instructions */
            uint8_t avx512er            : 1; /* AVX512 Exponential and Reciprocal Instructions */
            uint8_t avx512cd            : 1; /* AVX512 Conflict Detection Instructions */
            uint8_t sha                 : 1; /* Intel SHA extensions */
            uint8_t avx512bw            : 1; /* AVX512 Byte and Word Instructions */
            uint8_t avx512vl            : 1; /* AVX512 Vector Length Extensions */
        } __packed;
    } __packed;
} __packed;

struct cpuid_feature_flags {
    struct cpuid_ecx_flags ecx;
    struct cpuid_edx_flags edx;
} __packed;

typedef struct cpuid_ret {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
} cpuid_ret_t; 


static int
cpuid (uint32_t func, cpuid_ret_t * ret)
{
    asm volatile ("pushq %%rbx;"
            "movq %%rdi, %%rbx;"
            "cpuid ;"
            "movq %%rbx, %%rdi; "
            "popq %%rbx ;"
            : "=a"(ret->a), "=D"(ret->b), "=c"(ret->c), "=d"(ret->d)
            : "0"(func));

    return 0;
}


static int
cpuid_sub (uint32_t func, uint32_t sub_func, cpuid_ret_t * ret)
{
    asm volatile ("pushq %%rbx;"
            "movq %%rdi, %%rbx;"
            "cpuid ;"
            "movq %%rbx, %%rdi; "
            "popq %%rbx ;"
            : "=a"(ret->a), "=D"(ret->b), "=c"(ret->c), "=d"(ret->d)
            : "0"(func), "2"(sub_func));

    return 0;
}


#endif 
