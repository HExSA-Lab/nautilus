#ifndef __LIBALLOC_HOOKS_H__
#define __LIBALLOC_HOOKS_H__

int liballoc_lock(void);
int liballoc_unlock(int flags);
void * liballoc_alloc(unsigned num);
int liballoc_free(void * ptr, unsigned num);

void init_liballoc_hooks(void);


#endif
