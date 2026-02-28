#pragma once

#include <types.h>

void keyboard_initialize();
void keyboard_Write(uint16_t port, uint8_t value);
void keyboard_handler(struct interrupt_registers* registers);
bool keyboard_is_occupied();
bool keyboard_task_read(uint32_t taskId, char* buff, uint32_t limit, bool changeTaskState);