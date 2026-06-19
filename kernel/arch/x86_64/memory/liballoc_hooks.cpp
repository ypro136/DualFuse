#include <liballoc_hooks.h>
#include <spinlock.h>

#include <vmm.h>

#ifdef __cplusplus
extern "C" {
#endif


static SpinlockIrq liballoc_spinlock = {0, 0};

int   liballoc_lock()
{
    spinlock_irq_acquire(&liballoc_spinlock);
    return 0;

} 

int   liballoc_unlock()
{

    spinlock_irq_release(&liballoc_spinlock);

    return 0;
}

void* liballoc_alloc(int pages)
{
    return virtual_allocate(pages);

}

int   liballoc_free(void *ptr,int pages)
{
    virtual_free(ptr, pages);
    return 0;

}

#ifdef __cplusplus
}
#endif