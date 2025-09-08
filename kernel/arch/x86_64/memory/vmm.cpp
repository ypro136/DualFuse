#include <vmm.h>

#include <pmm.h>
#include <utility.h>
#include <paging.h>
#include <hcf.hpp>


#define VMM_DEBUG 0

// HHDM might be mapped as a full-GB entry, so we have to be careful!
#define VMM_POS_ENSURE 0x40000000

uint64_t hhdmOffset;

void virtual_memory_manager_initialize(uint64_t memory_map_Total, uint64_t memory_map_entry_count, struct limine_memmap_entry** memory_map_entries, uint64_t _hhdmOffset)
{
  hhdmOffset = _hhdmOffset;
  size_t targetPosition = CEILING_DIVISION(hhdmOffset - memory_map_Total - VMM_POS_ENSURE, PAGE_SIZE) * PAGE_SIZE;


  _virtual.ready = false;
  _virtual.mem_start = targetPosition;
  _virtual.BitmapSizeInBlocks = CEILING_DIVISION(memory_map_Total, BLOCK_SIZE);
  _virtual.BitmapSizeInBytes = CEILING_DIVISION(_virtual.BitmapSizeInBlocks, 8);

  uint64_t pagesRequired = CEILING_DIVISION(_virtual.BitmapSizeInBytes, BLOCK_SIZE);
  _virtual.Bitmap = (uint8_t *)virtual_allocate(pagesRequired);
  memset(_virtual.Bitmap, 0, _virtual.BitmapSizeInBytes);

  // should NEVER get put inside (since it's on the HHDM region)
  // MarkRegion(&virtual, _virtual.Bitmap, _virtual.BitmapSizeInBytes, 1);

  _virtual.ready = true;
}

void *virtual_allocate(int pages) {
  size_t phys = physical_allocate(pages);

  uint64_t output = phys + hhdmOffset;
#if VMM_DEBUG
  printf("[vmm::alloc] Found region: out{%x} phys{%x}\n", output, phys);
#endif
  return (void *)(output);
}

void *virtual_to_physical(uint64_t pointer)
{
  uint64_t output = pointer - hhdmOffset;

  #if VMM_DEBUG
  printf("[vmm::virtual_to_physical] Found region: virtual{%x} physical{%x}\n", output, phys);
  #endif
  return (void *)(output);
}


bool virtual_free(void *ptr, int pages) {
  size_t phys = virtual_to_physical((uint64_t)ptr);
  if (!phys) {
    printf("[vmm::free] Could not find physical address! virt{%x}\n", ptr);
    Halt();
  }

  physical_free(phys, pages);
  return true;
}


void virtual_spinlock_acquire() 
{
  _virtual.lock_id = 2;
  lock_bitmap(&_virtual);
  physical_spinlock_acquire();
}

void virtual_spinlock_release() 
{
  _virtual.lock_id = 0;
  unlock_bitmap(&_virtual);
  physical_spinlock_release();
}