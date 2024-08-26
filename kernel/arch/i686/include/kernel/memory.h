#include <stddef.h>
#include <stdint.h>

#define KERNEL_START 0xC0000000
#define KERNEL_MALLOC 0xD0000000
#define REC_PAGE_DIRECTORY ((uint32_t*)0xFFFFF000)
#define REC_PAGE_TABLE(i) ((uint32_t*) (0xFFC00000) + ((i) << 12))
#define PAGE_FLAG_PRESENT (1 << 0)
#define PAGE_FLAG_WRITE (1 << 1)
#define PAGE_FLAG_OWNER (1 << 9)



extern uint32_t initial_page_directory[1024];

void physical_memory_manager_initialize(uint32_t memory_low, uint32_t memory_high);

uint32_t *memory_get_current_page_directory();

void memory_change_page_directory(uint32_t* page_directory);

void sync_page_directories();

void memory_map_page(uint32_t virtual_address, uint32_t physical_address, uint32_t flags);

uint32_t physical_memory_manager_alloc_page_frame();

void memory_initialize(uint32_t memory_high, uint32_t physical_allocation_start);

void invalidate(uint32_t virtual_address);
