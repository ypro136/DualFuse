 #include <types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bootloader.h>

#include <serial.h>
#include <gdt.h>
#include <isr.h>
#include <timer.h>
#include <keyboard.h>
#include <memory.h>
#include <pci.h>

#include <hcf.hpp>

#if defined(__is_x86_64)
#include <framebufferutil.h>
#include <console.h>
#include <paging.h>
#include <task.h>
#include <syscalls.h>
#include <fastSyscall.h>
#include <system.h>

bool systemDiskInit;
#endif




extern "C" void _init(void);


extern "C" void kernel_main(void) 
{

     _init();

    serial_initialize(0x3f8);
    printf("serial initialized.\n");

	gdt_initialize();
	printf("gdt initialized\n");

    isr_initialize();
    printf("idt and isr initialized.\n");

	timer_initialize();
	printf("timer initialized\n");

    initialiseBootloaderParser();
    printf("Bootloader Parser initialized\n");

    #if defined(__is_x86_64)

    systemDiskInit = false;

    paging_initialize();
    printf("paging initialized.\n");

    framebuffer_initialize();
    printf("limine framebuffer initialized.\n");

    console_initialize();
    clear_screen();

    #endif

	memory_initialize();
	printf("memory initialized\n");

    #if defined(__is_x86_64)
    
    pci_initialize();

    #endif

	keyboard_initialize();
    printf("keyboard initialized\n");

    #if defined(__is_x86_64)
    
    tasks_initialize();

    syscall_inst_initialize();

    syscalls_initialize();

    //syscall test
    printf("task id is :%d \n",syscallGetPid());

    #endif

	
    for (;;) {}
}