#include <stdio.h>

#include <string.h>


#include <kernel/tty.h>
#include <kernel/serial.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/timer.h>
#include <kernel/keyboard.h>





extern "C" void kernel_main(void) 
{
	terminal_initialize();
	serial_initialize();

	printf("terminal and serial out initialized\n");

	gdt_initialize();
	printf("gdt initialized\n");

	idt_initialize();
	printf("idt initialized\n");

	timer_initialize();
	printf("timer initialized\n");

	keyboard_initialize();
	printf("keyboard initialized\n");


	for(;;);
	
	
}