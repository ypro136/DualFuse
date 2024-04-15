#include <stdio.h>

#include <string.h>


#include <kernel/tty.h>
#include <kernel/serial.h>



extern "C" void kernel_main(void) {
	terminal_initialize();
	serial_initialize();

	printf("Hello, I am %s %s !\n", "Dual", "Fuse");

	write_serial('h');

	printf("Hello, kernel World!\n");

	char test = 'y';

	printf("address of %c is : %h!\n", test, &test);

	printf("{}\n");

	
}