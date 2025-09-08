#include <paging.h>
#include <pmm.h>
#include <data_structures/bitmap.h>
#include <system.h>
#include <utility.h>
#include <hcf.hpp>
#include <vmm.h>
#include <limine.h>

#include <stdio.h>



void physical_memory_manager_initialize(uint64_t memory_map_Total, uint64_t memory_map_entry_count, struct limine_memmap_entry** memory_map_entries, uint64_t hhdmOffset) 
{
  Bitmap *bitmap = &physical; // pointer to physical memory manager bitmap
  bitmap->ready = false;         // for bitmap dependency of virtual memory manager

  physical.BitmapSizeInBlocks = CEILING_DIVISION(memory_map_Total, BLOCK_SIZE);
  physical.BitmapSizeInBytes = CEILING_DIVISION(physical.BitmapSizeInBlocks, 8);

  #if defined(DEBUG_MEMORY)
  printf("[pmm] physical.BitmapSizeInBlocks is %d\n", physical.BitmapSizeInBlocks);
  printf("[pmm] physical.BitmapSizeInBytes is %d\n", physical.BitmapSizeInBytes);
  #endif
  
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

  memset(physical.Bitmap, 0xff, physical.BitmapSizeInBytes);
  for (int i = 0; i < memory_map_entry_count; i++) 
  {
    struct limine_memmap_entry *entry = memory_map_entries[i];
    if (entry->type == LIMINE_MEMMAP_USABLE)
    {
      #if defined(DEBUG_MEMORY)
      printf("memory_map_entry %d is valid\n", i);
      #endif
      mark_region(bitmap, (void *)entry->base, entry->length, 0);
      #if defined(DEBUG_MEMORY)
      printf("memory_map_entry %d got marked\n", i);
      #endif
    }
    else
    {
      #if defined(DEBUG_MEMORY)
      printf("memory_map_entry %d is not valid\n", i);
      #endif
    }
  }


  for (int i = 0; i < memory_map_entry_count; i++) {
    struct limine_memmap_entry *entry = memory_map_entries[i];
    if (entry->type != LIMINE_MEMMAP_USABLE)
      mark_region(bitmap, (void *)entry->base, entry->length, 1);
  }

  mark_region(bitmap, (void *)bitmapStartPhys, physical.BitmapSizeInBytes, 1);

  #if defined(DEBUG_MEMORY)
  printf("[physical_memory_manager] Bitmap initiated: bitmapStartPhys{%x} size{%x}\n", bitmapStartPhys, physical.BitmapSizeInBytes);
  #endif

  // bitmap_dump_blocks(bitmap);
  bitmap->ready = true;
}

uint64_t physical_allocate(int pages) {
  uint64_t phys = (uint64_t)bitmap_allocate(&physical, pages);

  if (!phys && pages) {
    printf("[physical_memory_manager::alloc] Physical kernel memory ran out!\n");
    Halt();
  }

  return phys;
}

void physical_free(uint64_t ptr, int pages) 
{

  mark_region(&physical, (void *)ptr, pages * BLOCK_SIZE, 0);
}

void physical_spinlock_acquire() 
{
  physical.lock_id = 1;
  lock_bitmap(&physical);
}

void physical_spinlock_release() 
{
  physical.lock_id = 0;
  unlock_bitmap (&physical);
}