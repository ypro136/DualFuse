#include <kernel/memory.h>


#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#include <kernel/utility.h>
#include <utility/data_structures/bitmap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>

#include <kernel/bootloader.h>
extern Bootloader bootloader;




static uint64_t heap_size;
static uint64_t threshold;
static bool kmalloc_is_initialized = false;


void memory_initialize()
{

    physical_memory_manager_initialize(bootloader.mmTotal, bootloader.mmEntryCnt, bootloader.mmEntries , bootloader.hhdmOffset);

    virtual_memory_manager_initialize(bootloader.mmTotal, bootloader.mmEntryCnt, bootloader.mmEntries , bootloader.hhdmOffset);

    kernel_malloc_initialize(0);
}


void kernel_malloc_initialize(uint64_t initial_memory_request)
{
    heap_size = 0;
    kmalloc_is_initialized = true;

    kernel_malloc(initial_memory_request);
}


void *kernel_malloc(uint64_t size)
{
    int number_of_page_to_allocate = CEILING_DIVISION(size, BLOCK_SIZE);
    void *address = 0;

    address = VirtualAllocate(number_of_page_to_allocate);

    // printf("[kernel_malloc] allocated %d pages or %d bytes of memory at %x\n", number_of_page_to_allocate, size, address);

    heap_size += number_of_page_to_allocate;

    return address;
}

void *kernel_free(uint64_t size)
{
    int number_of_page_to_allocate = CEILING_DIVISION(size, BLOCK_SIZE);
    void *address = 0;

    address = VirtualAllocate(number_of_page_to_allocate);

    // printf("[kernel_malloc] allocated %d pages or %d bytes of memory at %x\n", number_of_page_to_allocate, size, address);

    heap_size += number_of_page_to_allocate;

    return address;
}
