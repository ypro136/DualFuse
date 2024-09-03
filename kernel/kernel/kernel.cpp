#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <string.h>

#if defined(__is_i686)
// 32 bit

#include <kernel/serial.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/timer.h>
#include <kernel/keyboard.h>
#include <kernel/multiboot.h>
#include <kernel/memory.h>
#include <kernel/tty.h>

// 32 bit
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

#else
// 64 bit



#include <limine.h>

#include <kernel/serial.h>


// Halt and catch fire function
#include <utility/hcf.hpp>

// Set the base revision to the recommended latest base revision 2
__attribute__((used, section(".requests")))
static volatile LIMINE_BASE_REVISION(2);

// The Limine requests it is important that
// the compiler does not optimise them away, so, they should
// be made volatile and marked as used with the "used" attribute
__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// define the start and end markers for the Limine requests
__attribute__((used, section(".requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

extern "C" void _init(void);

// 64 bit
extern "C" void kernel_main(void)
{

    _init();
    
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        Halt();
    }

    // is their a framebuffer
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        Halt();
    }

    // Fetch the first framebuffer
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    bool serial_state = serial_initialize(0x3f8);
    uint32_t test_state;


    if (serial_state)
    {
        test_state = 0x40ff40;
        serial_write('T');
    }
    else
    {
        test_state = 0xff4040;
    }


    // framebuffer model is assumed to be RGB with 32-bit pixels
    for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4) + i] = test_state;
    }

    for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4)] = 0xffffff;
    }

    for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[(framebuffer->pitch / 4) + i] = 0xffffff;
    }


	


    Halt();

}

#endif