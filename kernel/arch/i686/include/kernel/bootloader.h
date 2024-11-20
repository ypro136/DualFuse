#include <multiboot.h>

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

__attribute__((used))
static volatile uint32_t multiboot_magic;
__attribute__((used))
static volatile struct multiboot_information* multiboot_pointer;
__attribute__((used))
static volatile struct multiboot_information multiboot_info;

typedef struct Bootloader {
uint32_t multiboot_magic;
uint32_t mod_1;
uint32_t memory_high;
uint32_t physical_allocation_start;
};



void initialiseBootloaderParser();

#endif