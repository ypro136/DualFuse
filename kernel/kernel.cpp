#include "../drivers/screen.hpp"
//#include "../drivers/keyboard.hpp"
#include "utility.hpp"
#include "../cpu/isr.hpp"
#include "../cpu/idt.hpp"
#include "../cpu/timer.hpp"



void main() {
    isr_install();

    asm volatile("sti");
    init_timer(50);
    /* Comment out the timer IRQ handler to read
     * the keyboard IRQs easier */
    //init_keyboard();
}