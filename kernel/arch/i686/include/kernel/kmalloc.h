#include <stddef.h>
#include <stdint.h>


void kmalloc_initialize(uint32_t initial_heap_size);
void change_heap_size(int new_size);