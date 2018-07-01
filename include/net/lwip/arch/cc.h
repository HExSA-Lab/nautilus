#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#include "nautilus/libccompat.h"
#include "nautilus/printk.h"
#include "nautilus/limits.h"
#include "nautilus/naut_types.h"


typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;
typedef sint8_t  s8_t;
typedef sint16_t s16_t;
typedef sint32_t s32_t;
typedef sint64_t s64_t;
typedef addr_t mem_ptr_t;
#define LWIP_ERR_T int

typedef void* sys_prot_t;

#define U16_F "hu"
#define S16_F "d"
#define X16_F "hx"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "uz"

#define BYTE_ORDER LITTLE_ENDIAN

#define LWIP_PLATFORM_DIAG(x) do {printk x;} while(0)
#define LWIP_PLATFORM_ASSERT(x) do {printk("Assertion \"%s\" failed at line %d in %s\n", \
					x, __LINE__, __FILE__); fflush(NULL); abort();} while(0) 

#define LWIP_PROVIDE_ERRNO

#define LWIP_NO_STDDEF_H 1
#define LWIP_NO_STDINT_H 1
#define LWIP_NO_INTTYPES_H 1
#define LWIP_NO_LIMITS_H 1

#define PACK_STRUCT_FIELD(x) x __attribute__((packed))
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END


#endif /* __ARCH_CC_H__ */
