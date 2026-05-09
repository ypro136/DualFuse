#pragma once

#include <isr.h>

extern bool scheduler_enabled;

void schedule(AsmPassedInterrupt* interrupt_frame);
