#include <utility.h>



char in_port_byte(uint16_t port)
{
    char return_value;
    asm volatile ("inb %1, %0" : "=a" (return_value) : "dN" (port));

    return return_value;
}