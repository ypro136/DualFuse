// pmm.cpp — Physical Memory Manager (interrupt‑safe)
#include <paging.h>
#include <pmm.h>
#include <data_structures/bitmap.h>
#include <system.h>
#include <utility.h>
#include <hcf.hpp>
#include <vmm.h>
#include <limine.h>
#include <spinlock.h>      // for SpinlockIrq

#include <stdio.h>

uint64_t physical_used_blocks_count  = 0;
uint64_t physical_total_blocks_count = 0;

Bitmap physical;

// ---------------------------------------------------------------------------
// Lock helpers
// ---------------------------------------------------------------------------
void physical_spinlock_acquire(void) {
    spinlock_irq_acquire(&physical.bitmap_irq_lock);
}

void physical_spinlock_release(void) {
    spinlock_irq_release(&physical.bitmap_irq_lock);
}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------
void physical_memory_manager_initialize(
    uint64_t memory_map_Total,
    uint64_t memory_map_entry_count,
    struct limine_memmap_entry** memory_map_entries,
    uint64_t hhdmOffset)
{
    Bitmap *bitmap = &physical;
    bitmap->ready = false;

    physical.BitmapSizeInBlocks = CEILING_DIVISION(memory_map_Total, BLOCK_SIZE);
    physical.BitmapSizeInBytes  = CEILING_DIVISION(physical.BitmapSizeInBlocks, 8);

    #if defined(DEBUG_MEMORY)
    printf("[pmm] physical.BitmapSizeInBlocks is %d\n", physical.BitmapSizeInBlocks);
    printf("[pmm] physical.BitmapSizeInBytes is %d\n", physical.BitmapSizeInBytes);
    #endif

    // Find a usable memory region large enough for the bitmap
    struct limine_memmap_entry *memory_map = 0;
    for (int i = 0; i < memory_map_entry_count; i++) {
        struct limine_memmap_entry *entry = memory_map_entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE ||
            entry->length < physical.BitmapSizeInBytes)
            continue;
        memory_map = entry;
        break;
    }

    #if defined(DEBUG_MEMORY)
    printf("[pmm] first loop ran\n");
    #endif

    if (!memory_map) {
        printf("[physical_memory_manager] Not enough memory: required{%x}!\n",
               physical.BitmapSizeInBytes);
        Halt();
        return;
    }

    size_t bitmapStartPhys = memory_map->base;
    physical.Bitmap = (uint8_t *)(bitmapStartPhys + hhdmOffset);

    // Initialise the bitmap: all blocks marked as used
    memset(physical.Bitmap, 0xff, physical.BitmapSizeInBytes);

    // Initialise the interrupt‑safe lock for this bitmap
    memset(&physical.bitmap_irq_lock, 0, sizeof(SpinlockIrq));

    // Free blocks that are marked as usable by the memory map
    for (int i = 0; i < memory_map_entry_count; i++) {
        struct limine_memmap_entry *entry = memory_map_entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            #if defined(DEBUG_MEMORY)
            printf("memory_map_entry %d is valid\n", i);
            #endif
            mark_region_unsafe(bitmap, (void *)entry->base, entry->length, 0);
            #if defined(DEBUG_MEMORY)
            printf("memory_map_entry %d got marked\n", i);
            #endif
        } else {
            #if defined(DEBUG_MEMORY)
            printf("memory_map_entry %d is not valid\n", i);
            #endif
        }
    }

    // Reserve non‑usable regions (already marked by the initial 0xff, but be explicit)
    for (int i = 0; i < memory_map_entry_count; i++) {
        struct limine_memmap_entry *entry = memory_map_entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE)
            mark_region_unsafe(bitmap, (void *)entry->base, entry->length, 1);
    }

    // Reserve the space occupied by the bitmap itself
    mark_region_unsafe(bitmap, (void *)bitmapStartPhys, physical.BitmapSizeInBytes, 1);

    #if defined(DEBUG_MEMORY)
    printf("[physical_memory_manager] Bitmap initiated: bitmapStartPhys{%x} size{%x}\n",
           bitmapStartPhys, physical.BitmapSizeInBytes);
    #endif

    bitmap->ready = true;

    // Count used blocks
    physical_total_blocks_count = physical.BitmapSizeInBlocks;
    physical_used_blocks_count  = 0;
    for (uint64_t i = 0; i < physical.BitmapSizeInBytes; i++) {
        uint8_t byte = physical.Bitmap[i];
        while (byte) {
            physical_used_blocks_count += (byte & 1);
            byte >>= 1;
        }
    }
}

// ---------------------------------------------------------------------------
// Allocation / free
// ---------------------------------------------------------------------------
uint64_t physical_allocate(int pages) {
    physical_spinlock_acquire();
    uint64_t phys = (uint64_t)bitmap_allocate_unsafe(&physical, pages);
    if (!phys && pages) {
        printf("[physical_memory_manager::alloc] Physical kernel memory ran out!\n");
        Halt();
    }
    physical_used_blocks_count += (uint64_t)pages;
    physical_spinlock_release();
    return phys;
}

void physical_free(uint64_t ptr, int pages) {
    physical_spinlock_acquire();
    if (physical_used_blocks_count >= (uint64_t)pages)
        physical_used_blocks_count -= (uint64_t)pages;
    mark_region_unsafe(&physical, (void *)ptr, pages * BLOCK_SIZE, 0);
    physical_spinlock_release();
}