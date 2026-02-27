#include <types.h>

bool keyboard_task_read(uint32_t taskId, char *buff, uint32_t limit, bool changeTaskState);
void keyboard_initialize();
void keyboard_handler(struct interrupt_registers *registers);
void user_input(char *input);
int tokenize(char *input, char *argv[], int max_args);
void hex_to_ascii(int n, char str[]);
void append(char s[], char n, int max_len);
void keyboard_Write(uint16_t port, uint8_t value);
void backspace(char s[]);