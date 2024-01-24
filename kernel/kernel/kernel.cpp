#include <stdio.h>

#include <string.h>


#include <kernel/tty.h>



extern "C" void kernel_main(void) {
	terminal_initialize();

	printf("Hello, kernel World!\n");
	for (int i = 0; i < 50; i++)
	{
		for (int j = 0; j < i; j++)
		{
			printf("0");
		}
		printf("\n");
	}

	char *string_1 = "first string ";
	char *string_2 = " second string ";

	
	strcat(string_1, string_2);

	

	printf(string_1);
	printf("\n");
	printf("Hello, kernel World!\n");

	printf("=============== \n");
	printf("Hello, I am %s %s !\n", "Dual", "Fuse");

	printf("Hello, I %s %s %s !\n", "should", "be", "printed");


	
}