//#include <sys/cdefs.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>

#ifndef UTILITY_H
#define UTILITY_H

#define CEILING_DIVISION(a,b) (((a + b) - 1) / b)

#define COMBINE_64(higher, lower) (((uint64_t)(higher) << 32) | (uint64_t)(lower))
#define SPLIT_64_HIGHER(value) ((value) >> 32)
#define SPLIT_64_LOWER(value) ((value) & 0xFFFFFFFF)

#define SPLIT_32_HIGHER(value) ((value) >> 16)
#define SPLIT_32_LOWER(value) ((value) & 0xFFFF)


extern bool tasksInitiated;

extern bool systemDiskInit;




struct interrupt_registers
{
    uint32_t cr2;
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_number, error_code;
    uint32_t eip, csm, eflags, useresp, ss;
};



void out_port_byte(uint16_t port, uint8_t data);

char in_port_byte(uint16_t port);

uint32_t inportlong(uint16_t portid);
void     out_port_long(uint16_t portid, uint32_t value);

void hand_control();

#endif