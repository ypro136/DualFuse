#include <sys/cdefs.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>

#define CEILING_DIVISION(a,b) (((a + b) - 1) / b)


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
