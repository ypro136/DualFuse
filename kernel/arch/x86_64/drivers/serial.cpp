#ifndef _SERIAL_H
#define _SERIAL_H 1

#include <serial.h>
#include <serialio.h>
#include <isr.h>

// ── State ────────────────────────────────────────────────────────────────────

uint16_t port = 0x3f8;
bool serial_initialized = false;

// ── Ring buffer ──────────────────────────────────────────────────────────────

#define SERIAL_BUFFER_SIZE 256

static volatile char rx_buf[SERIAL_BUFFER_SIZE];
static volatile int  rx_head = 0;
static volatile int  rx_tail = 0;

// ── Message assembly (newline-delimited, for Arduino lines) ──────────────────

#define SERIAL_MSG_MAX 128
static char     msg_buf[SERIAL_MSG_MAX];
static int      msg_idx = 0;

// Optional: define SERIAL_ON_MESSAGE(str) somewhere to handle complete lines.
// Default behaviour here just discards them; hook in your console as needed.
__attribute__((weak)) void serial_on_message(const char* /*msg*/) {}

// ── IRQ4 handler (COM1) ──────────────────────────────────────────────────────

static void serial_irq_handler(registers_t* /*r*/) {
    while (inb(port + 5) & 0x01) {
        char c = (char)inb(port);

        // Put in ring buffer (drop if full)
        int next = (rx_head + 1) % SERIAL_BUFFER_SIZE;
        if (next != rx_tail) {
            rx_buf[rx_head] = c;
            rx_head = next;
        }

        // Assemble newline-terminated messages
        if (c == '\r') continue;
        if (c == '\n') {
            msg_buf[msg_idx] = '\0';
            serial_on_message(msg_buf);
            msg_idx = 0;
        } else if (msg_idx < SERIAL_MSG_MAX - 1) {
            msg_buf[msg_idx++] = c;
        }
    }
}

// ── Initialisation ───────────────────────────────────────────────────────────

bool serial_initialize(uint16_t _port) {
    port = _port;

    outb(port + 1, 0x00);    // Disable all interrupts
    outb(port + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(port + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(port + 1, 0x00);    //                  (hi byte)
    outb(port + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(port + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(port + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    outb(port + 4, 0x1E);    // Set in loopback mode, test the serial chip
    outb(port + 0, 0xAE);    // Test serial chip

    if (inb(port + 0) != 0xAE) {
        serial_initialized = false;
        return false;
    }

    outb(port + 4, 0x0F);    // Normal operation mode

    // Enable receive-data-available interrupt on the UART
    outb(port + 1, 0x01);

    // IRQ4 = COM1.  With PIC remapped to base 0x20: vector = 0x20 + 4 = 0x24
    register_interrupt_handler(0x24, serial_irq_handler);

    serial_initialized = true;
    printf("serial initialization :%d.\n", serial_initialized);
    return true;
}

// ── Low-level transmit ───────────────────────────────────────────────────────

static bool transmit_empty() {
    return serial_initialized && (inb(port + 5) & 0x20);
}

void serial_write(char a) {
    if (!serial_initialized) return;
    while (!transmit_empty());
    outb(port, a);
}

// ── String helpers ───────────────────────────────────────────────────────────

void serial_print(const char* str) {
    while (*str) serial_write(*str++);
}

// Sends str followed by \r\n — matches Arduino Serial.println() framing
void serial_send_line(const char* str) {
    serial_print(str);
    serial_write('\r');
    serial_write('\n');
}

// ── Ring buffer consumer API ─────────────────────────────────────────────────

int serial_available() {
    return (rx_head - rx_tail + SERIAL_BUFFER_SIZE) % SERIAL_BUFFER_SIZE;
}

char serial_read_byte() {
    if (rx_head == rx_tail) return 0;
    char c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % SERIAL_BUFFER_SIZE;
    return c;
}

// ── Polled fallback (use in frame_loop if IRQ4 not wired on MSI) ─────────────
// Call this from your frame_loop on MSI if serial_on_message() never fires.
void serial_poll() {
    if (!serial_initialized) return;
    while (inb(port + 5) & 0x01) {
        char c = (char)inb(port);
        int next = (rx_head + 1) % SERIAL_BUFFER_SIZE;
        if (next != rx_tail) {
            rx_buf[rx_head] = c;
            rx_head = next;
        }
        if (c == '\r') continue;
        if (c == '\n') {
            msg_buf[msg_idx] = '\0';
            serial_on_message(msg_buf);
            msg_idx = 0;
        } else if (msg_idx < SERIAL_MSG_MAX - 1) {
            msg_buf[msg_idx++] = c;
        }
    }
}

#endif