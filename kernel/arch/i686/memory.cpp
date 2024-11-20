#include <memory.h>


#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#include <bootloader.h>
#include <utility.h>

extern Bootloader bootloader;

static uint32_t page_frame_min;
static uint32_t page_frame_max;
static uint32_t total_memory_allocated;

#define NUM_PAGE_DIRECTORIES 256
#define NUM_PAGE_FRAMES (0x100000000 / 0x1000 / 8)

uint8_t physical_memory_bitmap[NUM_PAGE_FRAMES / 8];  // TODO: Make this a bit arry and dinamically allocate it.

static uint32_t page_directories[NUM_PAGE_DIRECTORIES][1024] __attribute__((aligned(4096)));
static uint8_t page_directories_used[NUM_PAGE_DIRECTORIES];
int memory_number_of_virtual_pages;

void physical_memory_manager_initialize(uint32_t memory_low, uint32_t memory_high)
{
    page_frame_min = CEILING_DIVISION(memory_low, 0x1000);
    page_frame_max = memory_high / 0x1000;
    total_memory_allocated = 0;

    memset(physical_memory_bitmap, 0, sizeof(physical_memory_bitmap));
}

uint32_t *memory_get_current_page_directory()
{
    uint32_t page_directory;

    asm volatile("MOV %%cr3, %0" : "=r"(page_directory));

    page_directory += KERNEL_START;

    return (uint32_t*)page_directory;
}

void memory_change_page_directory(uint32_t *page_directory)
{
    page_directory = (uint32_t*)(((uint32_t)page_directory) - KERNEL_START);

    asm volatile("MOV %0, %%eax \n MOV %%eax, %%cr3 \n" :: "m"(page_directory));
}

void sync_page_directories()
{
    for (int i = 0; i < NUM_PAGE_DIRECTORIES; i++)
    {
        if(page_directories_used[i])
        {
            uint32_t* page_directory = page_directories[i];
            
            for (int i = 786; i < 1023; i++)
            {
                page_directory[i] = initial_page_directory[i] & ~PAGE_FLAG_OWNER;
            }

        }
    }
}

void memory_map_page(uint32_t virtual_address, uint32_t physical_address, uint32_t flags)
{
    uint32_t *previous_page_directory = 0;

    if (virtual_address >= KERNEL_START)
    {
        previous_page_directory = memory_get_current_page_directory();
        if (previous_page_directory != initial_page_directory)
        {
            memory_change_page_directory(initial_page_directory);
        }
    }

    uint32_t page_directory_index = virtual_address >> 22;
    uint32_t page_table_index = virtual_address >> 12 & 0x3FF;

    uint32_t* page_directory = REC_PAGE_DIRECTORY;
    uint32_t* page_table = REC_PAGE_TABLE(page_directory_index);

    if (!(page_directory[page_directory_index] & PAGE_FLAG_PRESENT))
    {
        uint32_t page_table_address = physical_memory_manager_alloc_page_frame();
        page_directory[page_directory_index] = page_table_address | PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE | PAGE_FLAG_OWNER | flags;
        invalidate(virtual_address);

        for (uint32_t i = 0; i < 1024; i++)
        {
            page_table[i] = 0;
        }
    }

    page_table[page_table_index] = physical_address | PAGE_FLAG_PRESENT | flags;
    memory_number_of_virtual_pages++;
    invalidate(virtual_address);

    if (previous_page_directory != 0)
    {
        sync_page_directories();
        if (previous_page_directory != initial_page_directory)
        {
            memory_change_page_directory(previous_page_directory);
        }
    }
}

uint32_t physical_memory_manager_alloc_page_frame()
{
    uint32_t start = page_frame_min / 8 + ((page_frame_min & 7) != 0 ? 1 : 0);
    uint32_t end = page_frame_max / 8 - ((page_frame_max & 7) != 0 ? 1 : 0);

    for(uint32_t b = start; b < end; b++)
    {
        uint8_t byte = physical_memory_bitmap[b];

        if (byte == 0xFF)
        {
            continue;
        }

        for (uint32_t i = 0; i < 8; i++)
        {
            bool used = byte >> i & 1;

            if (!used)
            {
                byte ^= (-1 ^byte) & (1 << i);
                physical_memory_bitmap[b] = byte;

                total_memory_allocated++;

                uint32_t address = (b*8*i) * 0x1000;
                return address;
            }
        }
    }
    return 0;           
}

void memory_initialize()
{
    
    uint32_t memory_high = bootloader.memory_high;
    uint32_t physical_allocation_start = bootloader.physical_allocation_start;


    memory_number_of_virtual_pages = 0;
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