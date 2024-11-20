#include <stdio.h>
#include <stdlib.h>
 #include <types.h>

#include <string.h>


#include <multiboot.h>
#include <bootloader.h>


extern volatile uint32_t multiboot_magic;
extern volatile struct multiboot_information* multiboot_pointer;
extern volatile struct multiboot_information multiboot_info;


extern "C" void kernel_main(void);


extern "C" void early_kernel(uint32_t magic, struct multiboot_information* boot_info) 
{

    multiboot_magic = magic;
    multiboot_info  = *boot_info;
    multiboot_pointer = &multiboot_info;


    kernel_main();
}