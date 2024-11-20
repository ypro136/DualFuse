
#include <bootloader.h>
#include <multiboot.h>

#include <stdio.h>




__attribute__((used))
Bootloader bootloader;
  

void initialiseBootloaderParser() 
{
  bootloader.multiboot_magic = multiboot_magic;

  // printf("multiboot pointer = %x", multiboot_pointer);

	bootloader.mod_1 = (uint32_t*)(multiboot_pointer->mods_address + 4);
	bootloader.physical_allocation_start = (bootloader.mod_1 & 0xFFF) & ~0xFFF;

  bootloader.memory_high = multiboot_pointer->memory_upper * 1024;


}