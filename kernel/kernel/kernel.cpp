#include <stdio.h>

#include <string.h>


#include <kernel/tty.h>
#include <kernel/serial.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/timer.h>
#include <kernel/keyboard.h>
#include <kernel/multiboot.h>
#include <kernel/memory.h>

// TODO: fix serial working weard.


extern "C" void kernel_main(uint32_t magic, struct multiboot_information* boot_info) 
{
	terminal_initialize();
    
	printf("terminal and serial out initialized\n");

	gdt_initialize();
	printf("gdt initialized\n");

	idt_initialize();
	printf("idt initialized\n");

	timer_initialize();
	printf("timer initialized\n");

	uint32_t mod_1 = *(uint32_t*)(boot_info->mods_address + 4);
	uint32_t physical_allocation_start = (mod_1 & 0xFFF) & ~0xFFF;


	memory_initialize(boot_info->memory_upper * 1024, physical_allocation_start);
	printf("memory initialized\n");



	keyboard_initialize();
	printf("keyboard initialized\n");


	for(;;);
	
	
}