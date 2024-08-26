//#include <stdio.h>

//#include <string.h>


#include <kernel/tty.h>
#include <kernel/serial.h>
#include <kernel/gdt.h>
#include <kernel/timer.h>
#include <kernel/keyboard.h>
#include <kernel/multiboot.h>




extern "C" void kernel_main(uint32_t magic, struct multiboot_information* boot_info) 
{
	terminal_initialize();

	//serial_initialize(0x3f8);

	printf("terminal and serial out initialized\n");


	uint32_t mod_1 = *(uint32_t*)(boot_info->mods_address + 4);
	uint32_t physical_allocation_start = (mod_1 & 0xFFF) & ~0xFFF;




	for(;;);
	
	
}