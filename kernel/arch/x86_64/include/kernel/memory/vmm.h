#include <data_structures/bitmap.h>
#include <limine.h>

#include <stddef.h>
#include <stdint.h>


#ifndef VMM_H
#define VMM_H


static volatile Bitmap _virtual;

void virtual_memory_manager_initialize(uint64_t memory_map_Total, uint64_t memory_map_entry_count, struct limine_memmap_entry** memory_map_entries, uint64_t hhdmOffset);

void *virtual_allocate(int pages);
bool  virtual_free(void *ptr, int pages);
void *virtual_to_physical(uint64_t pointer);

void virtual_spinlock_acquire();
void virtual_spinlock_release();


#endif