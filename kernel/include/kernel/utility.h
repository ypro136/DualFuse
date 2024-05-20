#include <sys/cdefs.h>
#include <stddef.h>

#include "stdint.h"

struct interrupt_registers
{
    uint32_t cr2;
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_number, error_code;
    uint32_t eip, csm, eflags, useresp, ss;
};


void outPortByte(uint16_t port, uint8_t data);

char inPortByte(uint16_t port);