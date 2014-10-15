#ifndef __INTRINSICS_H__
#define __INTRINSICS_H__


#define __packed __attribute__ ((packed))
#define __always_inline __attribute__ ((always_inline))

#define unlikely(c) __builtin_expect((c), 0)
#define likely(c)   __builtin_expect((c), 1)

#endif
