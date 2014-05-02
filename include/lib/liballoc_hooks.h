#ifndef __LIBALLOC_HOOKS_H__
#define __LIBALLOC_HOOKS_H__

int liballoc_lock(void);
int liballoc_unlock(void);
void * liballoc_alloc(int num);
int liballoc_free(void * ptr, int num);

void init_liballoc_hooks(void);


#endif
