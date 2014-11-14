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
liballoc_alloc (unsigned num)
{
    return (void*)nk_alloc_pages(num);
}


int
liballoc_free (void * ptr, unsigned num)
{
    return nk_free_pages(ptr, num);
}


