#include <stddef.h>
#include <stdint.h>


void memory_initialize();

void kernel_malloc_initialize(uint64_t initial_heap_size);
void *kernel_malloc(uint64_t size);
void *kernel_free(uint64_t size);


