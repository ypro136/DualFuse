#include <timer.h>

#include <utility.h>
#include <string.h>
#include <stdio.h>

#include <idt.h>
#include <isr.h>
#include <rtc.h>
#include <apic.h>

#include <framebufferutil.h>
#include <console.h>
#include <state_monitor.h>
#include <graphic_composer.h>
#include <graphic_composer_examples.h>


__attribute__((used))
uint64_t timerTicks = 0;
uint64_t timerBootUnix;

const uint32_t frequency = 60;

volatile bool frame_ready = false;
uint64_t GUI_frame = 1;

volatile bool copy_happened = false;  // debug only


void timer_irq_0(struct interrupt_registers *registers)
{
    timerTicks += 1;
    if (frame_ready)
    {
        frame_ready = false;
        copy_buffer_to_screan();
        GUI_frame++;
        copy_happened = true;
    }
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
    #if defined(DEBUG_TIMER)
    printf("[timer] timer_initialize....\n");
    #endif
    
    #if defined(DEBUG_TIMER)
    printf("[timer] About to call read_from_CMOS\n");
    #endif
    
    RTC rtc = {0};
    read_from_CMOS(&rtc);

    #if defined(DEBUG_TIMER)
    printf("[timer] rtc:%d\n",rtc.year );
    #endif

    timerTicks = 0;

    // 1.1931816666 Mhz
    uint32_t divisor = 1193180 / frequency;

    #if defined(DEBUG_TIMER)
    printf("[timer] divisor:%d\n",divisor );
    #endif

    if (isr_initialized)
    {
        irq_install_handler(0, &timer_irq_0);
        #if defined(DEBUG_TIMER)
        printf("[timer] handler installed\n");
        #endif
    }
    else 
    {
        printf("[timer] warning: isr not initialized. timer initialization omitted");
        return;
    }

    if (apic_initialized)
    {
        // Use LAPIC timer instead of PIT — PIT not wired on modern UEFI systems
        apicWrite(APIC_REGISTER_TIMER_DIV, 0x3);       // divide by 16
        apicWrite(APIC_REGISTER_LVT_TIMER, 32 | APIC_LVT_TIMER_MODE_PERIODIC); // vector 32, periodic
        apicWrite(APIC_REGISTER_TIMER_INITCNT, 100000); // initial count — calibrate later
    }
    else
    {
        // Legacy PIT path (Toshiba/PIC machines)
        out_port_byte(0x43, 0x36);
        out_port_byte(0x40, (uint8_t)(divisor & 0xFF));
        out_port_byte(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    }

    timerBootUnix = rtc_to_unix(&rtc);

    #if defined(DEBUG_TIMER)
    printf("[timer] timerBootUnix:%d\n", timerBootUnix);
    #endif
    
    #if defined(DEBUG_TIMER)
    printf("[timer] Ready to fire: frequency{%dMHz}\n", divisor);
    #endif
    printf("timer initialized.\n");
    
}