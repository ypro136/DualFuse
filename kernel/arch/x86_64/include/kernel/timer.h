#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>

void timer_initialize();
void on_irq_0(struct interrupt_registers *registers);
