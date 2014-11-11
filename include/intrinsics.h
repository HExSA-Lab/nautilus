#ifndef __INTRINSICS_H__
#define __INTRINSICS_H__


#define __packed __attribute__ ((packed))

#ifndef __always_inline
#define __always_inline __attribute__ ((always_inline))
#endif

#ifndef __noinline
#define __noinline __attribute__ ((noinline))
#endif

#ifndef unlikely
#define unlikely(c) __builtin_expect((c), 0)
#endif

#ifndef likely
#define likely(c)   __builtin_expect((c), 1)
#endif

#endif
