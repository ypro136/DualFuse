#ifndef BITMAP_H
#define BITMAP_H

#include <stddef.h>
#include <stdint.h>
#include <spinlock.h>

typedef struct Bitmap {
    uint8_t    *Bitmap;
    size_t      BitmapSizeInBlocks;
    size_t      BitmapSizeInBytes;
    size_t      lastDeepFragmented;
    size_t      mem_start;
    bool        ready;
    SpinlockIrq bitmap_irq_lock;        // interrupt‑safe lock
} Bitmap;

#define BLOCKS_PER_BYTE 8
#define BLOCK_SIZE      4096
#define INVALID_BLOCK   ((size_t)-1)

// Public safe API (locks internally, used during boot)
void   *bitmap_allocate(Bitmap *bitmap, size_t blocks);
void    mark_region(Bitmap *bitmap, void *basePtr, size_t sizeBytes, int isUsed);

// Unsafe API (caller must hold bitmap_irq_lock)
void   *bitmap_allocate_unsafe(Bitmap *bitmap, size_t blocks);
void    mark_region_unsafe(Bitmap *bitmap, void *basePtr, size_t sizeBytes, int isUsed);

// Conversion helpers
void   *block_to_pointer(Bitmap *bitmap, size_t block);
size_t  pointer_to_block(Bitmap *bitmap, void *ptr);
size_t  pointer_to_block_roundup(Bitmap *bitmap, void *ptr);

size_t  bitmap_get_size(size_t totalSize);
int     bitmap_get(Bitmap *bitmap, size_t block);
void    bitmap_set(Bitmap *bitmap, size_t block, bool value);

void    bitmap_dump(Bitmap *bitmap);
void    bitmap_dump_blocks(Bitmap *bitmap);

size_t  bitmap_allocate_pageframe(Bitmap *bitmap);
void    bitmap_free_pageframe(Bitmap *bitmap, void *addr);

#endif