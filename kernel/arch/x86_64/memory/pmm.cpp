//#include <paging.h>
#include <kernel/memory/pmm.h>
#include <utility/data_structures/bitmap.h>
//#include <system.h>
#include <kernel/utility.h>
#include <utility/hcf.hpp>
#include <kernel/memory/vmm.h>
#include <limine.h>

#include <stdio.h>



void physical_memory_manager_initialize(uint64_t memory_map_Total, uint64_t memory_map_entry_count, struct limine_memmap_entry** memory_map_entries, uint64_t hhdmOffset) 
{
  Bitmap *bitmap = &physical; // pointer to physical memory manager bitmap
  bitmap->ready = false;         // for bitmap dependency of virtual memory manager

  physical.BitmapSizeInBlocks = CEILING_DIVISION(memory_map_Total, BLOCK_SIZE);
  physical.BitmapSizeInBytes = CEILING_DIVISION(physical.BitmapSizeInBlocks, 8);

  struct limine_memmap_entry *memory_map = 0;


  for (int i = 0; i < memory_map_entry_count; i++) {
    struct limine_memmap_entry *entry = memory_map_entries[i];
    if (entry->type != LIMINE_MEMMAP_USABLE ||
        entry->length < physical.BitmapSizeInBytes)
      continue;
    memory_map = entry;
    break;
  }

  if (!memory_map) {
    printf("[physical_memory_manager] Not enough memory: required{%x}!\n",
           physical.BitmapSizeInBytes);
    Halt();
    return;
  }

  size_t bitmapStartPhys = memory_map->base;
  physical.Bitmap = (uint8_t *)(bitmapStartPhys + hhdmOffset);

  memset(physical.Bitmap, 0xff, physical.BitmapSizeInBytes);
  for (int i = 0; i < memory_map_entry_count; i++) {
    struct limine_memmap_entry *entry = memory_map_entries[i];
    if (entry->type == LIMINE_MEMMAP_USABLE)
      mark_region(bitmap, (void *)entry->base, entry->length, 0);
  }
  for (int i = 0; i < memory_map_entry_count; i++) {
    struct limine_memmap_entry *entry = memory_map_entries[i];
    if (entry->type != LIMINE_MEMMAP_USABLE)
      mark_region(bitmap, (void *)entry->base, entry->length, 1);
  }

  mark_region(bitmap, (void *)bitmapStartPhys, physical.BitmapSizeInBytes, 1);

  // printf("[physical_memory_manager] Bitmap initiated: bitmapStartPhys{%x} size{%x}\n", bitmapStartPhys, physical.BitmapSizeInBytes);

  // bitmap_dump_blocks(bitmap);
  bitmap->ready = true;
}

uint64_t physical_allocate(int pages) {
  // spinlockAcquire(&LOCK_PMM);
  uint64_t phys = (uint64_t)bitmap_allocate(&physical, pages);
  // spinlockRelease(&LOCK_PMM);

  if (!phys && pages) {
    printf("[physical_memory_manager::alloc] Physical kernel memory ran out!\n");
    Halt();
  }

  return phys;
}

void physical_free(uint64_t ptr, int pages) {
  // maybe verify no double-frees are occuring..

  // spinlockAcquire(&LOCK_PMM);
  mark_region(&physical, (void *)ptr, pages * BLOCK_SIZE, 0);
  // spinlockRelease(&LOCK_PMM);
}