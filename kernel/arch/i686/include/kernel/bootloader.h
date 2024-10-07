#include <kernel/multiboot.h>

#ifndef BOOTLOADER_H
#define BOOTLOADER_H


typedef struct Bootloader {
uint32_t multiboot_magic;
uint32_t mod_1;
uint32_t memory_high;
uint32_t physical_allocation_start;
};



void initialiseBootloaderParser();

#endif