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
#include <liballoc.h>


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
#include <scheduler.h>

#include <GUI.h>
#include <GUI_input.h>
#include <fram_loop.h>

#include <ramdisk.h>
#include <fs.h>
#include <hid_i2c.h>



extern "C" void _init(void);

extern "C" void kernel_main(void);

static void gui_render_kernel_task_entry() {
    while (1) {
        frame_loop(render_xp_desktop);
    }
}

static void gui_input_kernel_task_entry() {
    while (1) {
        GUI_input_loop();
    }
}

extern "C" void kernel_main(void) 
{
    _init();
    
    serial_initialize(0x3f8);
    
    initialiseBootloaderParser();

    gdt_initialize();

    paging_initialize();
    
    isr_initialize();
    
    memory_initialize();

    framebuffer_initialize();
        
    pci_initialize();
    
    tasks_initialize();
    printf("tasks initialized.\n"); 

    syscall_inst_initialize();
    
    syscalls_initialize();
    
    initiateSSE();
    
    block_init(); 
    
    acpiInit();
    printf("acpi initialized.\n");
    
    initiateAPIC();
	
    timer_initialize();

    keyboard_initialize();
    
    initiateMouse();
    printf("mouse initialized.\n");

    i2cInitialize();
    
    filesystem_mount();
    
    initialize_xp_desktop();
    
    hid_initialize();

    FRESULT res;
    FIL fp;
    UINT bytes_written_count;
    res = f_open(&fp, "/boot.log", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        printf("Failed to create /boot.log: %d\n", res);
    } else {
        FRESULT write_result = f_write(&fp,
                                    bootloader.Boot_log->log,
                                    bootloader.Boot_log->length,
                                    &bytes_written_count);
        if (write_result != FR_OK) { 
            printf("Failed to write to /boot.log: %d\n", write_result);
        } else if (bytes_written_count != bootloader.Boot_log->length) {
            printf("Warning: not all bytes were written to /boot.log (%u/%d)\n",
                bytes_written_count, bootloader.Boot_log->length);
        }

        f_close(&fp);
    }
    bootloader.Boot_log = NULL;

    gdt_update_tss_rsp0(currentTask->whileTssRsp);

    Task* gui_render_task = task_create_kernel((uint64_t)gui_render_kernel_task_entry, 0);
    task_name_kernel(gui_render_task, "gui_render", 10);

    Task* gui_input_task = task_create_kernel((uint64_t)gui_input_kernel_task_entry, 0);
    task_name_kernel(gui_input_task, "gui_input", 9);

    scheduler_enabled = true;
    printf("scheduler enabled.\n"); 

    while (1) {
        asm volatile("pause");
    }
}