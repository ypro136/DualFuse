#include <kernel/memory.h>


#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#include <kernel/multiboot.h>
#include <kernel/utility.h>


static uint32_t page_frame_min;
static uint32_t page_frame_max;
static uint32_t total_memory_allocated;

#define NUM_PAGE_DIRECTORIES 256
#define NUM_PAGE_FRAMES (0x100000000 / 0x1000 / 8)

uint8_t physical_memory_bitmap[NUM_PAGE_FRAMES / 8];  // TODO: Make this a bit arry and dinamically allocate it.

static uint32_t page_directories[NUM_PAGE_DIRECTORIES][1024] __attribute__((aligned(4096)));
static uint8_t page_directories_used[NUM_PAGE_DIRECTORIES];

void physical_memory_manager_initialize(uint32_t memory_low, uint32_t memory_high)
{
    page_frame_min = CEILING_DIVISION(memory_low, 0x1000);
    page_frame_max = memory_high / 0x1000;
    total_memory_allocated = 0;

    memset(physical_memory_bitmap, 0, sizeof(physical_memory_bitmap));
}


void memory_initialize(uint32_t memory_high, uint32_t physical_allocation_start)
{
    
    initial_page_directory[0] = 0;
    invalidate(0);
    initial_page_directory[1023] = ((uint32_t) initial_page_directory - KERNEL_START) | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE;
    invalidate(0xFFFFF000);

    physical_memory_manager_initialize(physical_allocation_start, memory_high);

    memset(page_directories, 0, 0x1000 * NUM_PAGE_DIRECTORIES);
    memset(page_directories_used, 0, NUM_PAGE_DIRECTORIES);

}

void invalidate(uint32_t virtual_address)
{
    asm volatile ("invlpg %0" : : "m" (virtual_address));
}