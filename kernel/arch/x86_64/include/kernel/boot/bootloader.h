#include <limine.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

// From the linker
extern uint64_t kernel_text_start, kernel_text_end;
extern uint64_t kernel_rodata_start, kernel_rodata_end;
extern uint64_t kernel_data_start, kernel_data_end;
extern uint64_t kernel_start, kernel_end;


typedef struct log_buffer {
  char log[320000];
  int length;
} log_buffer;

extern log_buffer boot_log;

typedef struct Bootloader {
  uint64_t hhdmOffset;
  uint64_t kernelVirtBase;
  uint64_t kernelPhysBase;

  log_buffer* Boot_log;

  struct limine_framebuffer *framebuffer;

  struct limine_module_response* modules;

  size_t rsdp;
  
  uint64_t first_entry_base;

  uint64_t   mmTotal;
  uint64_t mmExecTotal;
  uint64_t mmPhysExtent;
  uint64_t mmEntryCnt;
  LIMINE_PTR(struct limine_memmap_entry **) mmEntries;
};

#define IS_INSIDE_HHDM(a)   ((size_t)a >= bootloader.hhdmOffset && (size_t)a <= (bootloader.hhdmOffset + bootloader.mmTotal))

extern Bootloader bootloader;


void initialiseBootloaderParser();

#endif