#include <data_structures/bitmap.h>

#include <stdio.h>
#include <utility.h>
#include <spinlock.h>      // for spinlock_irq_acquire / release

/* ===================================================================
 * Internal (unsafe) helpers – caller must hold bitmap_irq_lock
 * =================================================================== */

static int bitmap_get_unsafe(Bitmap *bitmap, size_t block) {
    size_t addr = block / BLOCKS_PER_BYTE;
    size_t off  = block % BLOCKS_PER_BYTE;
    return (bitmap->Bitmap[addr] & (1 << off)) != 0;
}

static void bitmap_set_unsafe(Bitmap *bitmap, size_t block, bool value) {
    size_t addr = block / BLOCKS_PER_BYTE;
    size_t off  = block % BLOCKS_PER_BYTE;
    if (value)
        bitmap->Bitmap[addr] |= (1 << off);
    else
        bitmap->Bitmap[addr] &= ~(1 << off);
}

static void mark_blocks_unsafe(Bitmap *bitmap, size_t start, size_t size,
                               bool val) {
    if (!val && start < bitmap->lastDeepFragmented)
        bitmap->lastDeepFragmented = start;

    for (size_t i = start; i < start + size; i++)
        bitmap_set_unsafe(bitmap, i, val);
}

static size_t find_free_region_unsafe(Bitmap *bitmap, size_t blocks) {
    size_t currentRegionStart = bitmap->lastDeepFragmented;
    size_t currentRegionSize  = 0;

    for (size_t i = currentRegionStart; i < bitmap->BitmapSizeInBlocks; i++) {
        if (bitmap_get_unsafe(bitmap, i)) {
            currentRegionSize  = 0;
            currentRegionStart = i + 1;
        } else {
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

/* ===================================================================
 * Public safe API – these acquire the interrupt‑safe lock internally
 * =================================================================== */

int bitmap_get(Bitmap *bitmap, size_t block) {
    spinlock_irq_acquire(&bitmap->bitmap_irq_lock);
    int val = bitmap_get_unsafe(bitmap, block);
    spinlock_irq_release(&bitmap->bitmap_irq_lock);
    return val;
}

void bitmap_set(Bitmap *bitmap, size_t block, bool value) {
    spinlock_irq_acquire(&bitmap->bitmap_irq_lock);
    bitmap_set_unsafe(bitmap, block, value);
    spinlock_irq_release(&bitmap->bitmap_irq_lock);
}

void mark_region(Bitmap *bitmap, void *basePtr, size_t sizeBytes, int isUsed) {
    size_t base;
    size_t size;

    if (isUsed) {
        base = pointer_to_block(bitmap, basePtr);
        size = CEILING_DIVISION(sizeBytes, BLOCK_SIZE);
    } else {
        base = pointer_to_block_roundup(bitmap, basePtr);
        size = sizeBytes / BLOCK_SIZE;
    }

    spinlock_irq_acquire(&bitmap->bitmap_irq_lock);
    mark_blocks_unsafe(bitmap, base, size, isUsed);
    spinlock_irq_release(&bitmap->bitmap_irq_lock);
}

void *bitmap_allocate(Bitmap *bitmap, size_t blocks) {
    spinlock_irq_acquire(&bitmap->bitmap_irq_lock);
    void *ptr = bitmap_allocate_unsafe(bitmap, blocks);
    spinlock_irq_release(&bitmap->bitmap_irq_lock);
    return ptr;
}

void *bitmap_allocate_unsafe(Bitmap *bitmap, size_t blocks) {
    if (blocks == 0)
        return 0;

    size_t pickedRegion = find_free_region_unsafe(bitmap, blocks);
    if (pickedRegion == INVALID_BLOCK)
        return 0;

    mark_blocks_unsafe(bitmap, pickedRegion, blocks, 1);
    return block_to_pointer(bitmap, pickedRegion);
}

void mark_region_unsafe(Bitmap *bitmap, void *basePtr, size_t sizeBytes,
                        int isUsed) {
    size_t base;
    size_t size;

    if (isUsed) {
        base = pointer_to_block(bitmap, basePtr);
        size = CEILING_DIVISION(sizeBytes, BLOCK_SIZE);
    } else {
        base = pointer_to_block_roundup(bitmap, basePtr);
        size = sizeBytes / BLOCK_SIZE;
    }

    mark_blocks_unsafe(bitmap, base, size, isUsed);
}

void bitmap_free(Bitmap *bitmap, void *base, size_t blocks) {
    spinlock_irq_acquire(&bitmap->bitmap_irq_lock);
    mark_region_unsafe(bitmap, base, BLOCK_SIZE * blocks, 0);
    spinlock_irq_release(&bitmap->bitmap_irq_lock);
}

/* ===================================================================
 * Pageframe convenience functions (safe)
 * =================================================================== */

size_t bitmap_allocate_pageframe(Bitmap *bitmap) {
    spinlock_irq_acquire(&bitmap->bitmap_irq_lock);
    size_t pickedRegion = find_free_region_unsafe(bitmap, 1);
    mark_blocks_unsafe(bitmap, pickedRegion, 1, 1);
    size_t phys = bitmap->mem_start + (pickedRegion * BLOCK_SIZE);
    spinlock_irq_release(&bitmap->bitmap_irq_lock);
    return phys;
}

void bitmap_free_pageframe(Bitmap *bitmap, void *addr) {
    spinlock_irq_acquire(&bitmap->bitmap_irq_lock);
    mark_region_unsafe(bitmap, addr, BLOCK_SIZE, 0);
    spinlock_irq_release(&bitmap->bitmap_irq_lock);
}

/* ===================================================================
 * Conversion utilities (no locking required)
 * =================================================================== */

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
    return (size_t)CEILING_DIVISION((size_t)(u8Ptr - bitmap->mem_start),
                                     BLOCK_SIZE);
}

size_t bitmap_get_size(size_t totalSize) {
    size_t blocks = CEILING_DIVISION(totalSize, BLOCK_SIZE);
    return CEILING_DIVISION(blocks, 8);
}

/* ===================================================================
 * Debugging (safe, acquire lock when needed)
 * =================================================================== */

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
        // bitmap_get() locks for us
        BITMAP_DEBUG_F("%d ", bitmap_get(bitmap, i));
    }
    BITMAP_DEBUG_F("\n");
}