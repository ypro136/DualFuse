#ifndef HID_I2C_H
#define HID_I2C_H

#include <types.h>

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

struct TouchReport {
    bool     fingerDown;
    uint16_t x;
    uint16_t y;
    bool     leftClick;
    bool     rightClick;
};

const HidI2cDescriptor* hidI2cGetDesc();
void hid_initialize();

bool hidI2cIsActive();

int  hidI2cGetDescriptor(uint64_t base, uint8_t addr, HidI2cDescriptor* out);
int  hidI2cReset(uint64_t base, uint8_t addr, const HidI2cDescriptor* desc);
int  hidI2cReadReport(uint64_t base, uint8_t addr,
                      const HidI2cDescriptor* desc, TouchReport* out);
int  hidI2cInit(uint64_t base, uint8_t addr, HidI2cDescriptor* out);
void hidI2cPoll(uint64_t base, uint8_t addr, const HidI2cDescriptor* desc);
void hidI2cTickPoll();

#endif