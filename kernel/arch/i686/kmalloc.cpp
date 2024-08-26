#include <kernel/kmalloc.h>

#include <stdint.h>

#include <kernel/memory.h>
#include <kernel/utility.h>


static uint32_t heap_start;
static uint32_t heap_size;
static uint32_t threshold;
static bool kmalloc_is_initialized = false;


void kmalloc_initialize(uint32_t initial_heap_size)
{
    heap_start = KERNEL_MALLOC;
    heap_size = 0;
    kmalloc_is_initialized = true;

    change_heap_size(heap_size);
}


void change_heap_size(int new_size)
{
    int old_page_top = CEILING_DIVISION(heap_size, 0x1000);
    int new_page_top = CEILING_DIVISION(new_size, 0x1000);

    int difference = new_page_top - old_page_top;

    for (int i = 0; i < difference; i++)
    {
        uint32_t physical = physical_memory_manager_alloc_page_frame();

        memory_map_page(KERNEL_MALLOC + old_page_top * 0x1000 + i * 0x1000, physical, PAGE_FLAG_WRITE); 
    }
}