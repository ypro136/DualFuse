#ifndef HID_I2C_H
#define HID_I2C_H

#include <types.h>

/* HID over I2C descriptor (30 bytes, little-endian).
 * Returned by reading the HID descriptor register (0x0001 for ELAN).    */
struct __attribute__((packed)) HidI2cDescriptor {
    uint16_t wHIDDescLength;
    uint16_t bcdVersion;
    uint16_t wReportDescLength;
    uint16_t wReportDescRegister;
    uint16_t wInputRegister;
    uint16_t wMaxInputLength;
    uint16_t wOutputRegister;
    uint16_t wMaxOutputLength;
    uint16_t wCommandRegister;
    uint16_t wDataRegister;
    uint16_t wVendorID;
    uint16_t wProductID;
    uint16_t wVersionID;
    uint32_t reserved;
};

/* Parsed touch report from ELAN touchpad.
 * Filled by hidI2cReadReport() when a finger is detected.               */
struct TouchReport {
    bool     fingerDown;
    uint16_t x;
    uint16_t y;
    bool     leftClick;
};

/* Read 30-byte HID descriptor from device at `addr` over `base` I2C.
 * Returns 0 on success.                                                 */
int hidI2cGetDescriptor(uint64_t base, uint8_t addr,
                        HidI2cDescriptor* out);

/* Send SET_POWER ON + RESET to put device in operational mode.          */
int hidI2cReset(uint64_t base, uint8_t addr,
                const HidI2cDescriptor* desc);

/* Read one input report. Returns 0 on success, fills `out`.
 * Call in a poll loop or from an IRQ handler.                           */
int hidI2cReadReport(uint64_t base, uint8_t addr,
                     const HidI2cDescriptor* desc,
                     TouchReport* out);

/* Top-level init: get descriptor, reset device, return descriptor.
 * Called from hid_i2c layer init in kernel_main.                        */
int hidI2cInit(uint64_t base, uint8_t addr, HidI2cDescriptor* out);

/* Poll loop tick - call from timer ISR or a polling shell command.
 * Reads one report and updates mouse_position_x/y + clickedLeft.       */
void hidI2cPoll(uint64_t base, uint8_t addr,
                const HidI2cDescriptor* desc);

#endif