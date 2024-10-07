#ifndef IDT_H
#define IDT_H


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>

typedef struct {
  uint16_t isr_low;
  uint16_t kernel_cs;
  uint8_t  ist;
  uint8_t  attributes;
  uint16_t isr_mid;
  uint32_t isr_high;
  uint32_t reserved;
} __attribute__((packed)) idt_entry;

typedef struct {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed)) _idt_pointer;

void set_idt_gate(uint32_t index, uint64_t handler, uint8_t flags);
void set_idt();

#endif