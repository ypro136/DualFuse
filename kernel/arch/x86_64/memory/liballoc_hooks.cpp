#include <liballoc_hooks.h>

#include <vmm.h>

#ifdef __cplusplus
extern "C" {
#endif


int   liballoc_lock()
{
    virtual_spinlock_acquire();
    return 0;

}

int   liballoc_unlock()
{

    virtual_spinlock_release();

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