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
#include <i2c.h>
#include <hid_i2c.h>

#include <GUI.h>
#include <fram_loop.h>

#include <ramdisk.h>


extern "C" void _init(void);

extern "C" void kernel_main(void);

extern "C" void kernel_main(void) 
{
    _init(); // lib c software dependent fatal to fail
    
    serial_initialize(0x3f8); // serial output(for debuging) non fatal to fail
    
    initialiseBootloaderParser(); // parser for limine bootloder fatal to fail

	gdt_initialize(); // global discriptor table fatal to fail

    paging_initialize(); // paging non fatal to fail
    
    isr_initialize(); // Interrupt Service Routines fatal to fail
    
	memory_initialize(); // memory managment fatal to fail

    framebuffer_initialize(); // framebuffer non fatal to fail
        
    pci_initialize(); // Peripheral Component Interconnect non fatal to fail
    
    // tasks_initialize(); TODO: fix this non fatal to fail
    // printf("tasks initialized.\n");
    
    syscall_inst_initialize(); // syscalls non fatal to fail TODO: thay should be
    
    syscalls_initialize(); // syscalls non fatal to fail TODO: thay should be
    
    initiateSSE(); // Streaming SIMD Extensions non fatal to fail
    
    block_init();
    
    acpiInit();// TODO: this is very minimal
    printf("acpi initialized.\n");
    
    initiateAPIC();
	
    timer_initialize();

    keyboard_initialize();
    
    initiateMouse();
    printf("mouse initialized.\n");

    i2cInitialize();

    // Auto-init HID layer if I2C controller was found
    if (i2cGetBase() != 0) {
        static HidI2cDescriptor bootDesc;
        if (hidI2cInit(i2cGetBase(), I2C_ADDR_ELAN_TOUCHPAD, &bootDesc) == 0)
            printf("[hid] touchpad ready\n");
        else
            printf("[hid] touchpad init failed\n");
    }
    
    initialize_xp_desktop();

    bool should_exit = false;
    int loop_count = 0;
    while (!should_exit) 
    {
        frame_loop(render_xp_desktop);
        should_exit = GUI_input_loop();
    }
    
    // breakpoint; not tested and dose not work
	
    for (;;) {}
}

