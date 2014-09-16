#ifndef __LIMITS_H__
#define __LIMITS_H__

#define USHRT_MAX   ((u16)(~0U))
#define SHRT_MAX    ((s16)(USHRT_MAX>>1))
#define SHRT_MIN    ((s16)(-SHRT_MAX - 1))
#define INT_MAX     ((int)(~0U>>1))
#define INT_MIN     (-INT_MAX - 1)
#define UINT_MAX    (~0U)
#define LONG_MAX    ((long)(~0UL>>1))
#define LONG_MIN    (-LONG_MAX - 1)
#define ULONG_MAX   (~0UL)
#define LLONG_MAX   ((long long)(~0ULL>>1))
#define LLONG_MIN   (-LLONG_MAX - 1)
#define ULLONG_MAX  (~0ULL)
#define SIZE_MAX    (~(size_t)0)

#endif
