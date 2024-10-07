#include <utility/data_structures/bitmap.h>

#include <stdio.h>
#include <kernel/utility.h>

// Bitmap-based memory region manager
// 10000000 -> first block is allocated, other are free :")

/* Conversion utilities */

void *block_to_pointer(Bitmap *bitmap, size_t block) {
  uint8_t *u8Ptr = (uint8_t *)(bitmap->mem_start + (block * BLOCK_SIZE));
  return (void *)(u8Ptr);
}

size_t pointer_to_block(Bitmap *bitmap, void *ptr) {
  uint8_t *u8Ptr = (uint8_t *)ptr;
  return (size_t)(u8Ptr - bitmap->mem_start) / BLOCK_SIZE;
}

size_t pointer_to_block_roundup(Bitmap *bitmap, void *ptr) {
  uint8_t *u8Ptr = (uint8_t *)ptr;
  return (size_t)CEILING_DIVISION((size_t)(u8Ptr - bitmap->mem_start), BLOCK_SIZE);
}

/* Bitmap data structure essentials */

size_t bitmap_get_size(size_t totalSize) {
  size_t BitmapSizeInBlocks = CEILING_DIVISION(totalSize, BLOCK_SIZE);
  size_t BitmapSizeInBytes = CEILING_DIVISION(BitmapSizeInBlocks, 8);
  return BitmapSizeInBytes;
}

int BitmapGet(Bitmap *bitmap, size_t block) {
  size_t addr = block / BLOCKS_PER_BYTE;
  size_t offset = block % BLOCKS_PER_BYTE;
  return (bitmap->Bitmap[addr] & (1 << offset)) != 0;
}

void BitmapSet(Bitmap *bitmap, size_t block, bool value) {
  size_t addr = block / BLOCKS_PER_BYTE;
  size_t offset = block % BLOCKS_PER_BYTE;
  if (value)
    bitmap->Bitmap[addr] |= (1 << offset);
  else
    bitmap->Bitmap[addr] &= ~(1 << offset);
}

/* Debugging functions */

#define BITMAP_DEBUG_F printf
void bitmap_dump(Bitmap *bitmap) {
  BITMAP_DEBUG_F("=== BYTE DUMPING %d -> %d BYTES ===\n",
                 bitmap->BitmapSizeInBlocks, bitmap->BitmapSizeInBytes);
  for (int i = 0; i < bitmap->BitmapSizeInBytes; i++) {
    BITMAP_DEBUG_F("%x ", bitmap->Bitmap[i]);
  }
  BITMAP_DEBUG_F("\n");
}

void bitmap_dump_blocks(Bitmap *bitmap) {
  BITMAP_DEBUG_F("=== BLOCK DUMPING %d (512-limited) ===\n",
                 bitmap->BitmapSizeInBlocks);
  for (int i = 0; i < 512; i++) {
    BITMAP_DEBUG_F("%d ", BitmapGet(bitmap, i));
  }
  BITMAP_DEBUG_F("\n");
}

/* Marking large chunks of memory */
void mark_blocks(Bitmap *bitmap, size_t start, size_t size, bool val) {
  // optimization(1): bitmap.h
  if (!val && start < bitmap->lastDeepFragmented)
    bitmap->lastDeepFragmented = start;

  for (size_t i = start; i < start + size; i++) {
    BitmapSet(bitmap, i, val);
  }
}

void mark_region(Bitmap *bitmap, void *basePtr, size_t sizeBytes,int isUsed) 
{
  size_t base;
  size_t size;

  if (isUsed) {
    base = pointer_to_block(bitmap, basePtr);
    size = CEILING_DIVISION(sizeBytes, BLOCK_SIZE);
  } else {
    base = pointer_to_block_roundup(bitmap, basePtr);
    size = sizeBytes / BLOCK_SIZE;
  }

  mark_blocks(bitmap, base, size, isUsed);
}

size_t find_free_region(Bitmap *bitmap, size_t blocks) {
  size_t currentRegionStart = bitmap->lastDeepFragmented;
  size_t currentRegionSize = 0;

  for (size_t i = currentRegionStart; i < bitmap->BitmapSizeInBlocks; i++) {
    if (BitmapGet(bitmap, i)) {
      currentRegionSize = 0;
      currentRegionStart = i + 1;
    } else {
      // optimization(1): bitmap.h
      if (blocks == 1)
        bitmap->lastDeepFragmented = currentRegionStart + 1;

      currentRegionSize++;
      if (currentRegionSize >= blocks)
        return currentRegionStart;
    }
  }

  printf("[bitmap] Didn't find jack shit!\n");
  return INVALID_BLOCK;
}

void *bitmap_allocate(Bitmap *bitmap, size_t blocks) {
  if (blocks == 0)
    return 0;

  size_t pickedRegion = find_free_region(bitmap, blocks);
  if (pickedRegion == INVALID_BLOCK)
    return 0;

  mark_blocks(bitmap, pickedRegion, blocks, 1);
  return block_to_pointer(bitmap, pickedRegion);
}

void bitmap_free(Bitmap *bitmap, void *base, size_t blocks) {
  mark_region(bitmap, base, BLOCK_SIZE * blocks, 0);
}

/* Pageframes (1 block) */

size_t bitmap_allocate_pageframe(Bitmap *bitmap) {
  size_t pickedRegion = find_free_region(bitmap, 1);
  // if (pickedRegion == INVALID_BLOCK) {
  //   printf("no!");
  //   panic();
  // }
  mark_blocks(bitmap, pickedRegion, 1, 1);

  return (bitmap->mem_start + (pickedRegion * BLOCK_SIZE));
}

void bitmap_free_pageframe(Bitmap *bitmap, void *addr) {
  mark_region(bitmap, addr, BLOCK_SIZE * 1, 0);
}