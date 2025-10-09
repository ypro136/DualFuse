
#include <serial.h>
#include <idt.h>
#include <isr.h>
#include <gdt.h>
#include <utility.h>


#define IDT_ENTRIES 256
static idt_entry idt[IDT_ENTRIES] = {0}; // Create an array of IDT entries; aligned for performance
static _idt_pointer idt_pointer;



void set_idt_gate(uint32_t index, uint64_t handler, uint8_t flags) {
  idt[index].isr_low = (uint16_t)handler;
  idt[index].kernel_cs = GDT_KERNEL_CODE;
  idt[index].ist = 0;
  idt[index].reserved = 0;
  idt[index].attributes = flags;
  idt[index].isr_mid = (uint16_t)(handler >> 16);
  idt[index].isr_high = (uint32_t)(handler >> 32);
}

void set_idt() 
{
  idt_pointer.base = (size_t)&idt;
  idt_pointer.limit = IDT_ENTRIES * sizeof(idt_entry) - 1;
  asm volatile("lidt %0" ::"m"(idt_pointer) : "memory");
}