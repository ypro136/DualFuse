#include <kernel/utility.h>

void outPortByte(uint16_t port, uint8_t data)
{
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (data));
}