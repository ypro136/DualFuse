
#ifdef __cplusplus
extern "C" {
#endif

int   liballoc_lock();
int   liballoc_unlock();
void* liballoc_alloc(int pages);
int   liballoc_free(void *ptr,int pages);


#ifdef __cplusplus
}
#endif