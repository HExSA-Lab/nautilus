#include <paging.h>
#include <spinlock.h>

#include <lib/liballoc_hooks.h>

spinlock_t mmu_lock;


void 
init_liballoc_hooks (void) 
{
    spinlock_init(&mmu_lock);
}


int 
liballoc_lock (void)
{
    spin_lock(&mmu_lock);
    return 0;
}


int 
liballoc_unlock ()
{
    spin_unlock(&mmu_lock);
    return 0;
}


void * 
liballoc_alloc (int num)
{
    return (void*)alloc_pages(num);
}


int
liballoc_free (void * ptr, int num)
{
    return free_pages(ptr, num);
}


