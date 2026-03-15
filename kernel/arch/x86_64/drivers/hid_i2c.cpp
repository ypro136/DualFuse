#include <hid_i2c.h>
#include <framebufferutil.h>
#include <timer.h>
#include <i2c.h>
#include <mouse.h>
#include <bootloader.h>
#include <stdio.h>
#include <string.h>

static bool             hidI2cActive = false;
static HidI2cDescriptor hidGlobalDesc;
static uint64_t         hidGlobalBase = 0;
static uint8_t          hidGlobalAddr = 0;

bool hidI2cIsActive() { return hidI2cActive; }

/* HID-over-I2C register for ELAN descriptor */
#define HID_DESC_REG        0x0001

/* HID output commands (written to wCommandRegister) */
#define HID_CMD_RESET       0x0100
#define HID_CMD_SET_POWER   0x0800

/* SET_POWER values */
#define HID_POWER_ON        0x00
#define HID_POWER_SLEEP     0x01

/* Write a 2-byte little-endian register address then read `len` bytes. */
static int hidReadReg(uint64_t base, uint16_t reg,
                      uint8_t* buf, uint16_t len) {
    uint8_t addr[2] = { (uint8_t)(reg & 0xFF), (uint8_t)(reg >> 8) };
    return i2cWriteRead(base, addr, 2, buf, len);
}

/* Write `len` bytes to `reg` (register address first, then data). */
static int hidWriteReg(uint64_t base, uint16_t reg,
                       const uint8_t* data, uint16_t len) {
    uint8_t buf[64];
    if (len + 2 > 64) return -1;
    buf[0] = (uint8_t)(reg & 0xFF);
    buf[1] = (uint8_t)(reg >> 8);
    memcpy(buf + 2, data, len);
    return i2cSend(base, buf, (uint32_t)(len + 2));
}

int hidI2cGetDescriptor(uint64_t base, uint8_t addr,
                        HidI2cDescriptor* out) {
    (void)addr;
    uint8_t buf[30];

    /* Retry up to 3 times - ELAN may need an extra wake cycle */
    int ok = -1;
    for (int attempt = 0; attempt < 5 && ok < 0; attempt++) {
        memset(buf, 0, sizeof(buf));
        if (attempt > 0) {
            printf("[hid] descriptor retry %d\n", attempt);
            sleep(2);
        }
        ok = hidReadReg(base, HID_DESC_REG, buf, 30);
    }
    if (ok < 0) {
        printf("[hid] descriptor read failed\n");
        return -1;
    }

    memcpy(out, buf, sizeof(HidI2cDescriptor));

    printf("[hid] descriptor: len=%u inputReg=0x%04x maxIn=%u cmdReg=0x%04x\n",
           out->wHIDDescLength,
           out->wInputRegister,
           out->wMaxInputLength,
           out->wCommandRegister);

    if (out->wHIDDescLength < 18 || out->wHIDDescLength > 30) {
        printf("[hid] descriptor length suspicious: %u\n", out->wHIDDescLength);
        return -1;
    }
    return 0;
}

int hidI2cReset(uint64_t base, uint8_t addr,
                const HidI2cDescriptor* desc) {
    (void)addr;

    /* Flush stale TX abort state before issuing commands.
     * On the second call the controller may have leftover abort state
     * from the previous run which corrupts transactions.              */
    i2cWrite(base, DW_IC_ENABLE, 0);
    sleep(1);
    i2cRead(base, DW_IC_CLR_TX_ABRT);
    i2cRead(base, DW_IC_CLR_INTR);
    i2cWrite(base, DW_IC_ENABLE, 1);
    sleep(1);

    /* SET_POWER ON */
    uint8_t setPower[4];
    setPower[0] = (uint8_t)(desc->wCommandRegister & 0xFF);
    setPower[1] = (uint8_t)(desc->wCommandRegister >> 8);
    setPower[2] = (uint8_t)((HID_CMD_SET_POWER | HID_POWER_ON) & 0xFF);
    setPower[3] = (uint8_t)((HID_CMD_SET_POWER | HID_POWER_ON) >> 8);
    if (i2cSend(base, setPower, 4) < 0) {
        printf("[hid] SET_POWER ON failed\n");
        return -1;
    }

    /* small delay */
    sleep(1);

    /* RESET */
    uint8_t reset[4];
    reset[0] = (uint8_t)(desc->wCommandRegister & 0xFF);
    reset[1] = (uint8_t)(desc->wCommandRegister >> 8);
    reset[2] = (uint8_t)(HID_CMD_RESET & 0xFF);
    reset[3] = (uint8_t)(HID_CMD_RESET >> 8);
    if (i2cSend(base, reset, 4) < 0) {
        printf("[hid] RESET failed\n");
        return -1;
    }

    sleep(2);
    printf("[hid] reset complete\n");

    /* Switch ELAN to absolute multitouch mode.
     * Write value 0x0003 to ELAN vendor register 0x0300 (TP Driver Control).
     * This is a raw I2C write: [reg_lo, reg_hi, value_lo, value_hi].
     * After this command reports change from relative (ID=0x01, 3 bytes)
     * to absolute multitouch (ID=0x5D, variable length).              */
    uint8_t setAbs[4] = { 0x00, 0x03, 0x03, 0x00 };
    i2cWrite(base, DW_IC_ENABLE, 0);
    sleep(1);
    i2cWrite(base, DW_IC_TAR, 0x15);
    i2cWrite(base, DW_IC_ENABLE, 1);
    sleep(1);
    if (i2cSend(base, setAbs, 4) < 0)
        printf("[hid] abs mode cmd failed (non-fatal)\n");
    else
        printf("[hid] absolute mode cmd sent\n");
    sleep(1);
    return 0;
}

int hidI2cReadReport(uint64_t base, uint8_t addr,
                     const HidI2cDescriptor* desc,
                     TouchReport* out) {
    (void)addr;
    memset(out, 0, sizeof(TouchReport));

    uint16_t maxLen = desc->wMaxInputLength;
    if (maxLen < 4 || maxLen > 256) maxLen = 64;

    uint8_t buf[256];
    if (hidReadReg(base, desc->wInputRegister, buf, maxLen) < 0)
        return -1;
    /* HID-over-I2C framing: buf[0..1]=length LE, buf[2]=reportID, buf[3..N]=payload
     * ELAN 14-byte confirmed: reportID 0x01=touch, 0x03=idle
     *   buf[3] flags: bit0=finger, bit1=btn_left, bit2=btn_right
     *   buf[4..5] X (12-bit LE: lo byte, hi nibble)
     *   buf[6..7] Y (12-bit LE: lo byte, hi nibble)
     *   buf[8..13] pressure/reserved                                   */
    uint16_t reportLen = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    if (reportLen < 3 || reportLen > maxLen) return 0;

    uint8_t reportId = buf[2];
    if (reportId != 0x01) return 0;

    uint8_t flags   = buf[3];
    out->fingerDown = (flags & 0x01) != 0;
    out->leftClick  = (flags & 0x02) != 0;

    if (out->fingerDown && reportLen >= 8) {
        out->x = (uint16_t)buf[4] | ((uint16_t)(buf[5] & 0x0F) << 8);
        out->y = (uint16_t)buf[6] | ((uint16_t)(buf[7] & 0x0F) << 8);
    }

    return 1;
}

int hidI2cInit(uint64_t base, uint8_t addr, HidI2cDescriptor* out) {
    if (hidI2cGetDescriptor(base, addr, out) < 0) return -1;
    if (hidI2cReset(base, addr, out) < 0) return -1;
    printf("[hid] HID-over-I2C ready: vendor=0x%04x product=0x%04x\n",
           out->wVendorID, out->wProductID);
    hidGlobalBase = base;
    hidGlobalAddr = addr;
    hidGlobalDesc = *out;
    hidI2cActive  = true;
    return 0;
}

/* Touchpad resolution - ELAN typically reports 0..4096 on both axes.
 * Scale to screen coords.                                              */
#define ELAN_MAX_X  4096
#define ELAN_MAX_Y  4096

void hidI2cPoll(uint64_t base, uint8_t addr,
                const HidI2cDescriptor* desc) {
    TouchReport rep;
    if (hidI2cReadReport(base, addr, desc, &rep) != 1) return;
    if (!rep.fingerDown) return;

    mouse_position_x = (int)((uint32_t)rep.x * SCREEN_WIDTH  / ELAN_MAX_X);
    mouse_position_y = (int)((uint32_t)rep.y * SCREEN_HEIGHT / ELAN_MAX_Y);

    if (mouse_position_x < 0) mouse_position_x = 0;
    if (mouse_position_y < 0) mouse_position_y = 0;
    if (mouse_position_x > (int)SCREEN_WIDTH  - 1)
        mouse_position_x = (int)SCREEN_WIDTH  - 1;
    if (mouse_position_y > (int)SCREEN_HEIGHT - 1)
        mouse_position_y = (int)SCREEN_HEIGHT - 1;

    clickedLeft = rep.leftClick;
}

/* Called from timer_irq_0 at 60Hz. No printf, no blocking.
 * Reads one report and updates mouse_position_x/y + clickedLeft.       */
void hidI2cTickPoll() {
    if (!hidI2cActive || !hidGlobalBase) return;
    hidI2cPoll(hidGlobalBase, hidGlobalAddr, &hidGlobalDesc);
}