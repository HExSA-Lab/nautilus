#ifndef __LIMITS_H__
#define __LIMITS_H__

#ifndef USHRT_MAX
#define USHRT_MAX   ((u16)(~0U))
#endif
#ifndef SHRT_MAX
#define SHRT_MAX    ((s16)(USHRT_MAX>>1))
#endif
#ifndef SHRT_MIN
#define SHRT_MIN    ((s16)(-SHRT_MAX - 1))
#endif
#ifndef INT_MAX
#define INT_MAX     ((int)(~0U>>1))
#endif
#ifndef INT_MIN
#define INT_MIN     (-INT_MAX - 1)
#endif
#ifndef UINT_MAX
#define UINT_MAX    (~0U)
#endif
#ifndef LONG_MAX
#define LONG_MAX    ((long)(~0UL>>1))
#endif
#ifndef LONG_MIN
#define LONG_MIN    (-LONG_MAX - 1)
#endif
#ifndef ULONG_MAX
#define ULONG_MAX   (~0UL)
#endif
#ifndef LLONG_MAX
#define LLONG_MAX   ((long long)(~0ULL>>1))
#endif
#ifndef LLONG_MIN
#define LLONG_MIN   (-LLONG_MAX - 1)
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX  (~0ULL)
#endif
#ifndef SIZE_MAX
#define SIZE_MAX    (~(size_t)0)
#endif

#endif
