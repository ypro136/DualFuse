#ifndef BITMAP_H
#define BITMAP_H

#include <stddef.h>
#include <stdint.h>

#define IS_LOCKED true 
#define IS_NOT_LOCKED false

typedef struct Bitmap {
  uint8_t *Bitmap;
  size_t   BitmapSizeInBlocks;
  size_t   BitmapSizeInBytes;

  size_t lastDeepFragmented;

  size_t mem_start;
  uint8_t lock_id;
  bool lock = IS_NOT_LOCKED;
  bool ready;
};

#define BLOCKS_PER_BYTE 8 // using uint8_t
#define BLOCK_SIZE 4096
#define INVALID_BLOCK ((size_t)-1)


void spinlock (Bitmap *bitmap);
void lock_bitmap (Bitmap *bitmap);
void unlock_bitmap (Bitmap *bitmap);

void  *block_to_pointer(Bitmap *bitmap, size_t block);
size_t pointer_to_block(Bitmap *bitmap, void *ptr);
size_t pointer_to_block_roundup(Bitmap *bitmap, void *ptr);

size_t bitmap_get_size(size_t totalSize);
int    bitmap_get(Bitmap *bitmap, size_t block);
void   bitmap_set(Bitmap *bitmap, size_t block, bool value);

void bitmap_dump(Bitmap *bitmap);
void bitmap_dump_blocks(Bitmap *bitmap);

void mark_blocks(Bitmap *bitmap, size_t start, size_t size, bool val);
void mark_region(Bitmap *bitmap, void *basePtr, size_t sizeBytes, int isUsed);
size_t find_free_region(Bitmap *bitmap, size_t blocks);
void  *bitmap_allocate(Bitmap *bitmap, size_t blocks);

size_t bitmap_allocate_pageframe(Bitmap *bitmap);
void   bitmap_free_pageframe(Bitmap *bitmap, void *addr);

#endif