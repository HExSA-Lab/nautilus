#ifndef __CPUID_H__
#define __CPUID_H__

#include <nautilus/intrinsics.h>
#include <nautilus/naut_types.h>

#define CPUID_FEATURE_INFO       0x1

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
    asm volatile ("cpuid" : 
                  "=a"(ret->a), "=b"(ret->b), "=c"(ret->c), "=d"(ret->d) : 
                  "0"(func));

    return 0;
}


static int
cpuid_sub (uint32_t func, uint32_t sub_func, cpuid_ret_t * ret)
{
    asm volatile ("cpuid" : 
                  "=a"(ret->a), "=b"(ret->b), "=c"(ret->c), "=d"(ret->d) : 
                  "0"(func), "2"(sub_func));

    return 0;
}

void detect_cpu(void);

#endif 
