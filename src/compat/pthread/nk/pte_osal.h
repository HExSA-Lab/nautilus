#ifndef _PTE_OSAL_H_
#define _PTE_OSAL_H_
#include "pte_types.h"
#include "pte_generic_osal.h"
#include <nautilus/nautilus.h>

#define calloc(a,b) malloc(a*b)

#ifndef EPERM
#define EPERM           1
#endif // EPERM

#ifndef ESRCH
#define ESRCH           3
#endif // ESRCH

#ifndef EINTR
#define EINTR           4
#endif // EINTR

#ifndef EIO
#define EIO             5
#endif // EIO

#ifndef EAGAIN
#define EAGAIN          11
#endif // EAGAIN

#ifndef ENOMEM
#define ENOMEM          12
#endif // ENOMEM

#ifndef EBUSY
#define EBUSY           16
#endif // EBUSY

#ifndef EINVAL
#define EINVAL          22
#endif // EINVAL

#ifndef ENOSPC
#define ENOSPC          28
#endif // ENOSPC

#ifndef EDEADLK
#define EDEADLK         35
#endif /* EDEADLK */

#ifndef ENOSYS
#define ENOSYS          38
#endif /* ENOSYS */

#ifndef ENOTSUP
#define ENOTSUP         95
#endif /* ENOTSUP */

#ifndef ETIMEDOUT
#define ETIMEDOUT       116
#endif // ETIMEDOUT

#ifndef ENOENT
#define ENOENT 7
#endif

#ifndef ERANGE
#define ERANGE 9
#endif 


#endif
