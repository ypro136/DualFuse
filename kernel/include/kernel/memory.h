#include <stddef.h>
#include <stdint.h>

#define KERNEL_START 0xC0000000
#define PAGE_FLAG_PRESENT (1 << 0)
#define PAGE_FLAG_WRITE (1 << 1)



extern uint32_t initial_page_directory[1024];

void physical_memory_manager_initialize(uint32_t memory_low, uint32_t memory_high);

void memory_initialize(uint32_t memory_high, uint32_t physical_allocation_start);

void invalidate(uint32_t virtual_address);
