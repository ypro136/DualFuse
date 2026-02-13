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
#include <dbg.h>


#include <framebufferutil.h>
#include <graphic_composer.h>
#include <console.h>
#include <psf.h>
#include <state_monitor.h>
#include <paging.h>
#include <task.h>
#include <syscalls.h>
#include <fastSyscall.h>
#include <system.h>
#include <minimal_acpi.h>
#include <mouse.h>
#include <apic.h>



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

    // Initialize the global console as a window at position (100,50)
    console = Console(800, 600, 10, 10);

    // Use the legacy initializer to set the global `console_initialized` flag
    console_initialize();
    console.clear_screen();
    // Set a visible title for the console window
    console.set_title("Kernel Console");

    // Initialize the global StateMonitor window (updates driven by timer IRQ)
    stateMonitor = *(new StateMonitor(800, 170, 10, 620));
    stateMonitor.initialize();
    stateMonitor.clear_screen();


    pci_initialize();

	keyboard_initialize();
    printf("keyboard initialized.\n");

    tasks_initialize();
    
    syscall_inst_initialize();
    
    syscalls_initialize();
    
    initiateSSE();

    //acpiInit();

    initiateAPIC();
    printf("[kernel] APIC initialization complete\n");

    printf("[kernel] About to call initiateMouse...\n");
    initiateMouse();
    printf("[kernel] Mouse initialization complete\n");

    //syscall test
    printf("[kernel] Getting PID...\n");
    printf("task id is :%d \n",syscallGetPid());
    
    // breakpoint; tested and works
	
    for (;;) {}
}

