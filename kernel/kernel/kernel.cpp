#include <stdio.h>

#include <string.h>


#include <kernel/tty.h>


extern "C" void kernel_main(void) {
	terminal_initialize();

	printf("Hello, I am %s %s !\n", "Dual", "Fuse");

	printf("Hello, kernel World!\n");

	char test = 'y';

	printf("Hello, address of %c is : %h!\n", test, &test);
	printf("{}\n");

	
}