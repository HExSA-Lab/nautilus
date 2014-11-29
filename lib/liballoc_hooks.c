#include <nautilus/paging.h>
#include <nautilus/spinlock.h>

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
    return spin_lock_irq_save(&mmu_lock);
}


int 
liballoc_unlock (int flags)
{
    spin_unlock_irq_restore(&mmu_lock, flags);
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


