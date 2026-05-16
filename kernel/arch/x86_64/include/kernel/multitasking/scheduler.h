#pragma once

#include <isr.h>

extern bool scheduler_enabled;
extern volatile uint8_t per_core_in_interrupt[256];

void schedule(AsmPassedInterrupt* interrupt_frame);
void scheduler_lapic_timer_start_on_current_ap();
