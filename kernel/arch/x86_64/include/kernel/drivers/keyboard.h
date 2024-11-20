#include <types.h>

bool keyboard_task_read(uint32_t taskId, char *buff, uint32_t limit, bool changeTaskState);
void keyboard_initialize();
void keyboard_handler(struct interrupt_registers *registers);