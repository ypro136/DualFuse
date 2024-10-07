#include <limine.h>

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

// From the linker
extern uint64_t kernel_text_start, kernel_text_end;
extern uint64_t kernel_rodata_start, kernel_rodata_end;
extern uint64_t kernel_data_start, kernel_data_end;
extern uint64_t kernel_start, kernel_end;

typedef struct Bootloader {
  uint64_t hhdmOffset;
  uint64_t kernelVirtBase;
  uint64_t kernelPhysBase;

  struct limine_framebuffer *framebuffer;

  uint64_t first_entry_base;

  uint64_t   mmTotal;
  uint64_t mmEntryCnt;
  LIMINE_PTR(struct limine_memmap_entry **) mmEntries;
};



void initialiseBootloaderParser();

#endif