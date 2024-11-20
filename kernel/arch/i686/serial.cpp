
#include <serial.h>
#include <serialio.h>

#if defined(__is_i686)
// 32 bit
#include <tty.h>
#endif


uint16_t port = 0x3f8;


bool serial_initialize(uint16_t _port) 
{
    #if defined(__is_i686)
    // 32 bit
	terminal_initialize();
    #endif
    port = _port;

    outb(port + 1, 0x00);    // Disable all interrupts
    outb(port + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(port + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(port + 1, 0x00);    //                  (hi byte)
    outb(port + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(port + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(port + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    outb(port + 4, 0x1E);    // Set in loopback mode, test the serial chip
    outb(port + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)

    // Check if serial is faulty (i.e: not same byte as sent)
    if (inb(port + 0) != 0xAE) {
        return false;
    }

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(port + 4, 0x0F);
    return true;
}


bool received() {
    return inb(port + 5) & 1;
}

char read() {
    while (!received());
    return inb(port);
}

bool transmit_empty() {
    return inb(port + 5) & 0x20;
}

void serial_write(char a) {
    while (!transmit_empty());
    outb(port, a);
}