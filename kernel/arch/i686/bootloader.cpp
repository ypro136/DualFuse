
#include <kernel/bootloader.h>

#include <stdio.h>

extern "C" uint32_t multiboot_magic;
extern "C" struct multiboot_information* multiboot_pointer;

__attribute__((used))
Bootloader bootloader;
  

void initialiseBootloaderParser() 
{
  bootloader.multiboot_magic = multiboot_magic;

	bootloader.mod_1 = *(uint32_t*)(boot_info->mods_address + 4);
	bootloader.physical_allocation_start = (mod_1 & 0xFFF) & ~0xFFF;

  bootloader.memory_high = boot_info->memory_upper * 1024;


}