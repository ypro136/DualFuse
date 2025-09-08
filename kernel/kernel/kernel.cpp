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
#include <fakefs.h>


#include <framebufferutil.h>
#include <console.h>
#include <paging.h>
#include <task.h>
#include <syscalls.h>
#include <fastSyscall.h>
#include <system.h>

bool systemDiskInit;



extern "C" void _init(void);

extern "C" void kernel_main(void);

extern "C" void kernel_main(void) 
{

    _init();

    systemDiskInit = false;

    serial_initialize(0x3f8);
    printf("serial initialized.\n");

    initialiseBootloaderParser();
    printf("Bootloader Parser initialized.\n");

	gdt_initialize();
	printf("gdt initialized.\n");

    paging_initialize();
    printf("paging initialized.\n");

    isr_initialize();
    printf("idt and isr initialized.\n");
    
    framebuffer_initialize();
    printf("limine framebuffer initialized.\n");
    
	timer_initialize();
	printf("timer initialized.\n");

	memory_initialize();
	printf("memory initialized.\n");

    console_initialize();
    printf("console initialized.\n");
    clear_screen();
    printf("Welcome to DualFuse kernel!\n");

    pci_initialize();

	keyboard_initialize();
    printf("keyboard initialized.\n");

    tasks_initialize();
    
    syscall_inst_initialize();
    
    syscalls_initialize();
    
    initiateSSE();

    // fsMount("/", CONNECTOR_AHCI, 0, 1);
    // fsMount("/boot/", CONNECTOR_AHCI, 0, 0);
    // fsMount("/sys/", CONNECTOR_SYS, 0, 0);
    // fsMount("/proc/", CONNECTOR_PROC, 0, 0);

    //syscall test
    printf("task id is :%d \n",syscallGetPid());
    
    // test_framebuffer(0xffffff);
	
    for (;;) {}
}