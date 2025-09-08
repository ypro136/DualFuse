#include <timer.h>

#include <utility.h>
#include <string.h>
#include <stdio.h>

#include <idt.h>
#include <isr.h>
#include <rtc.h>

#include <framebufferutil.h>
#include <console.h>



__attribute__((used))
uint64_t timerTicks = 0;
uint64_t timerBootUnix;

const uint32_t frequency = 60;


void timer_irq_0(struct interrupt_registers *registers)
{
    timerTicks += 1;
    if (console_initialized)
    {
        copy_buffer_to_screan();
    }
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