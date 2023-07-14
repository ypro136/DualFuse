#include "../drivers/screen.hpp"
#include "utility.hpp"
#include "../cpu/isr.hpp"
#include "../cpu/idt.hpp"


void main() {
    isr_install();
    /* Test the interrupts */
    kernel_print("\n");
    kernel_print("Test the interrupts:");
    kernel_print("\n");
    __asm__ __volatile__("int $2");
    __asm__ __volatile__("int $3");
}