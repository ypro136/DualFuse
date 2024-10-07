#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <string.h>

#include <kernel/bootloader.h>

#include <kernel/serial.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/timer.h>
#include <kernel/keyboard.h>
#include <kernel/memory.h>

#if defined(__is_i686)
// 32 bit
#include <kernel/tty.h>
#else
// 64 bit
#include <kernel/framebufferutil.h>
#include <kernel/isr.h>

// Halt and catch fire function
#include <utility/hcf.hpp>
#endif



extern "C" void _init(void);


extern "C" void kernel_main(void) 
{

     _init();

    #if defined(__is_i686)
    // 32 bit
	terminal_initialize();
	printf("terminal and serial out initialized\n");
    #else
    serial_initialize(0x3f8);
    printf("serial initialized.\n");
    #endif

    initialiseBootloaderParser();
    printf("Bootloader Parser initialized\n");

    #if defined(__is_i686)
    #else
    framebuffer_initialize();
    printf("limine framebuffer initialized.\n");
    #endif

	memory_initialize();
	printf("memory initialized\n");

	gdt_initialize();
	printf("gdt initialized\n");

    #if defined(__is_i686)
	idt_initialize();
	printf("idt initialized\n");
    #else
    isr_initialize(); // timer gets initialized by the first timer tick.
    printf("idt and isr initialized.\n");
    #endif

	timer_initialize();
	printf("timer initialized\n");

	keyboard_initialize();
	printf("keyboard initialized\n");


    #if defined(__is_i686)
    #else
    bool test_state = !false;
    uint32_t test_color;

    


    if (test_state)
    {
        test_color = 0x40ff40;
    }
    else
    {
        test_color = 0xff4040;
    }


    test_framebuffer(test_color);
    #endif
	
    Halt();

}

