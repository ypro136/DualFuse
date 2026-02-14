#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>


extern uint64_t timerTicks;
extern uint64_t timerBootUnix;


void timer_initialize();
uint32_t sleep(uint32_t time);
uint64_t get_frame_time();
void timer_irq_0(struct interrupt_registers *registers);
