#include "stdint.h"
#include "tty.h"
#include "serial.h"
#include "isr.h"
#include "utility.h"

#include <string.h>
#include <stdio.h>


struct idt_entry idt_entries[256];
struct idt_pointer idt_pointer;



extern "C" void setIDT(uint32_t);


void isr_initialize()
{
    idt_pointer.limit = sizeof(struct idt_entry) * 256 - 1;
    idt_pointer.base = (uint32_t) &idt_entries;

    memset(&idt_entries, 0, sizeof(struct idt_entry) * 256);

    out_port_byte(0x20, 0x11);
    out_port_byte(0xA0, 0x11);

    out_port_byte(0x21, 0x20);
    out_port_byte(0xA1, 0x28);

    out_port_byte(0x21, 0x04);
    out_port_byte(0xA1, 0x02);

    out_port_byte(0x21, 0x01);
    out_port_byte(0xA1, 0x01);

    out_port_byte(0x21, 0x0);
    out_port_byte(0xA1, 0x0);

    encode_idt(0, (uint32_t)isr0, 0x08, 0x8E);
    encode_idt(1, (uint32_t)isr1, 0x08, 0x8E);
    encode_idt(2, (uint32_t)isr2, 0x08, 0x8E);
    encode_idt(3, (uint32_t)isr3, 0x08, 0x8E);
    encode_idt(4, (uint32_t)isr4, 0x08, 0x8E);
    encode_idt(5, (uint32_t)isr5, 0x08, 0x8E);
    encode_idt(6, (uint32_t)isr6, 0x08, 0x8E);
    encode_idt(7, (uint32_t)isr7, 0x08, 0x8E);
    encode_idt(8, (uint32_t)isr8, 0x08, 0x8E);
    encode_idt(9, (uint32_t)isr9, 0x08, 0x8E);
    encode_idt(10, (uint32_t)isr10, 0x08, 0x8E);
    encode_idt(11, (uint32_t)isr11, 0x08, 0x8E);
    encode_idt(12, (uint32_t)isr12, 0x08, 0x8E);
    encode_idt(13, (uint32_t)isr13, 0x08, 0x8E);
    encode_idt(14, (uint32_t)isr14, 0x08, 0x8E);
    encode_idt(15, (uint32_t)isr15, 0x08, 0x8E);
    encode_idt(16, (uint32_t)isr16, 0x08, 0x8E);
    encode_idt(17, (uint32_t)isr17, 0x08, 0x8E);
    encode_idt(18, (uint32_t)isr18, 0x08, 0x8E);
    encode_idt(19, (uint32_t)isr19, 0x08, 0x8E);
    encode_idt(20, (uint32_t)isr20, 0x08, 0x8E);
    encode_idt(21, (uint32_t)isr21, 0x08, 0x8E);
    encode_idt(22, (uint32_t)isr22, 0x08, 0x8E);
    encode_idt(23, (uint32_t)isr23, 0x08, 0x8E);
    encode_idt(24, (uint32_t)isr24, 0x08, 0x8E);
    encode_idt(25, (uint32_t)isr25, 0x08, 0x8E);
    encode_idt(26, (uint32_t)isr26, 0x08, 0x8E);
    encode_idt(27, (uint32_t)isr27, 0x08, 0x8E);
    encode_idt(28, (uint32_t)isr28, 0x08, 0x8E);
    encode_idt(29, (uint32_t)isr29, 0x08, 0x8E);
    encode_idt(30, (uint32_t)isr30, 0x08, 0x8E);
    encode_idt(31, (uint32_t)isr31, 0x08, 0x8E);

    encode_idt(32, (uint32_t)irq0, 0x08, 0x8E);
    encode_idt(33, (uint32_t)irq1, 0x08, 0x8E);
    encode_idt(34, (uint32_t)irq2, 0x08, 0x8E);
    encode_idt(35, (uint32_t)irq3, 0x08, 0x8E);
    encode_idt(36, (uint32_t)irq4, 0x08, 0x8E);
    encode_idt(37, (uint32_t)irq5, 0x08, 0x8E);
    encode_idt(38, (uint32_t)irq6, 0x08, 0x8E);
    encode_idt(39, (uint32_t)irq7, 0x08, 0x8E);
    encode_idt(40, (uint32_t)irq8, 0x08, 0x8E);
    encode_idt(41, (uint32_t)irq9, 0x08, 0x8E);
    encode_idt(42, (uint32_t)irq10, 0x08, 0x8E);
    encode_idt(43, (uint32_t)irq11, 0x08, 0x8E);
    encode_idt(44, (uint32_t)irq12, 0x08, 0x8E);
    encode_idt(45, (uint32_t)irq13, 0x08, 0x8E);
    encode_idt(46, (uint32_t)irq14, 0x08, 0x8E);
    encode_idt(47, (uint32_t)irq15, 0x08, 0x8E);

    




    encode_idt(128, (uint32_t)isr128, 0x08, 0x8E); // system call
    encode_idt(177, (uint32_t)isr177, 0x08, 0x8E); // system call

    setIDT((uint32_t)&idt_pointer);

}


void encode_idt(uint8_t number, uint32_t base, uint16_t selector, uint8_t flags)
{
    idt_entries[number].base_low = base & 0xFFFF;
    idt_entries[number].base_high = (base >> 16) & 0xFFFF;
    idt_entries[number].selector = selector;
    idt_entries[number].always0 = 0;
    idt_entries[number].flags = flags | 0x60;
}

char* exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",  
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Fault",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

extern "C" void isr_handler(struct interrupt_registers* registers)
{
    if (registers->int_number < 32)
    {
        printf("Exception %d: %s\n", registers->int_number, exception_messages[registers->int_number]);
        printf("\n");
        printf("Exception! system halted\n");
        for (;;);
    }
}

void *irq_routines[16] = {
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

void irq_install_handler(int irq, void (*handler)(struct interrupt_registers *registers))
{
    irq_routines[irq] = handler;
}

void irq_uninstall_handler(int irq)
{
    irq_routines[irq] = 0;
}

extern "C" void irq_handler(struct interrupt_registers* registers)
{
    void (*handler)(struct interrupt_registers *registers);

    handler = irq_routines[registers->int_number - 32];

    if(handler)
    {
        handler(registers);
    }

    if(registers->int_number >= 40)
    {
        out_port_byte(0xA0, 0x20);
    }

    out_port_byte(0x20, 0x20);
}


