#ifndef PMM_H
#define PMM_H

#include <data_structures/bitmap.h>
#include <limine.h>

#include <stddef.h>
#include <stdint.h>

static volatile Bitmap physical;

extern uint64_t physical_used_blocks_count;
extern uint64_t physical_total_blocks_count;


void physical_memory_manager_initialize(uint64_t memory_map_Total, uint64_t memory_map_entry_count, struct limine_memmap_entry** memory_map_entries, uint64_t hhdmOffset);

uint64_t physical_allocate(int pages);
void   physical_free(uint64_t ptr, int pages);

void physical_spinlock_acquire();
void physical_spinlock_release();

#endif