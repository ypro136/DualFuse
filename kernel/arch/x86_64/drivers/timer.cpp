#include <timer.h>

#include <utility.h>
#include <string.h>
#include <stdio.h>

#include <idt.h>
#include <isr.h>
#include <rtc.h>

#include <framebufferutil.h>
#include <console.h>
#include <state_monitor.h>
#include <graphic_composer.h>
#include <graphic_composer_examples.h>


__attribute__((used))
uint64_t timerTicks = 0;
uint64_t timerBootUnix;

const uint32_t frequency = 60;

bool frame_ready = false;
uint64_t GUI_frame = 1;


void timer_irq_0(struct interrupt_registers *registers)
{
    timerTicks += 1;

    if (console_initialized)
    {
        // update the StateMonitor bars based on timerTicks
        uint8_t a = (uint8_t)(timerTicks % 101);
        uint8_t b = (uint8_t)((timerTicks * 2) % 101);
        uint8_t c = (uint8_t)(100 - a);
        stateMonitor.set_progress(0, a);
        stateMonitor.set_progress(1, b);
        stateMonitor.set_progress(2, c);
        stateMonitor.render();
    }

    // TODO: Uncomment when graphic_composer_initialized is properly defined
    // if (graphic_composer_initialized)
    // {
    //     example_simple_window();
    // }

    // if (console_initialized)
    // {
    //     copy_buffer_to_screan();
    // }
    if (frame_ready)
    {
        copy_buffer_to_screan();
        GUI_frame++;
    }

    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI::timer] GUI_frame : %d\n", GUI_frame);
    #endif

}

uint32_t sleep(uint32_t time) 
{
  // Simple sleep without printf to avoid stack corruption
  uint64_t target = timerTicks + (time);
  while (target > timerTicks) {
    hand_control();
  }
  return 0;
}

uint64_t get_frame_time() {
    return GUI_frame;
}

void timer_initialize()
{
    RTC rtc = {0};
    read_from_CMOS(&rtc);

    timerTicks = 0;


    // 1.1931816666 Mhz
    uint32_t divisor = 1193180 / frequency;

    out_port_byte(0x43, 0x36); // 0x43 mode and command rigister, 0x36Access mode: lobyte/hibyte, 16-bit binary, square wave generator at channel 0

    out_port_byte(0x40, (uint8_t)(divisor & 0xFF)); // 0x40 channel 0 data port
    out_port_byte(0x40, (uint8_t)((divisor >> 8) & 0xFF)); // 0x40 channel 0 data port

    irq_install_handler(0, &timer_irq_0);

    timerBootUnix = rtc_to_unix(&rtc);


    #if defined(DEBUG_TIMER)
    printf("[timer] Ready to fire: frequency{%dMHz}\n", divisor);
    #endif

    
}