#ifndef _KERNEL_TELE_TYPE_H
#define _KERNEL_TELE_TYPE_H

#include <stddef.h>
#include <stdint.h>


void terminal_initialize(void);

void terminal_setcolor(uint8_t color);
void terminal_putchar(char c);
void terminal_write(char data);
void terminal_writestring(const char* data, size_t size);
 
#endif