#include "timer.h"

#include "utility.h"
#include "tty.h"
#include <string.h>
#include <stdio.h>

#include "isr.h"

uint64_t ticks;

const uint32_t frequency = 100;

void timer_irq_0(struct interrupt_registers *registers)
{
    ticks += 1;
}

void timer_initialize()
{
    ticks = 0;
    irq_install_handler(0, &timer_irq_0);

    // 1.1931816666 Mhz
    uint32_t divisor = 1193180 / frequency;

    out_port_byte(0x43, 0x36); // 0x43 mode and command rigister, 0x36Access mode: lobyte/hibyte, 16-bit binary, square wave generator at channel 0

    out_port_byte(0x40, (uint8_t)(divisor & 0xFF)); // 0x40 channel 0 data port
    out_port_byte(0x40, (uint8_t)((divisor >> 8) & 0xFF)); // 0x40 channel 0 data port


    
}