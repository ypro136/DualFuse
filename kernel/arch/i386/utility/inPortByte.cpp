#include <kernel/utility.h>



char inPortByte(uint16_t port)
{
    char return_value;
    asm volatile ("inb %1, %0" : "=a" (return_value) : "dN" (port));

    return return_value;
}