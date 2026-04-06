#include <hid_i2c.h>
#include <framebufferutil.h>
#include <timer.h>
#include <i2c.h>
#include <mouse.h>
#include <bootloader.h>
#include <stdio.h>
#include <string.h>
#include <GUI_input.h>



static bool             hidI2cActive = false;
static HidI2cDescriptor hidGlobalDesc;
static uint64_t         hidGlobalBase = 0;
static uint8_t          hidGlobalAddr = 0;

static uint8_t hidLastReport[64] = {0};
static uint16_t hidLastReportLen = 0;

bool hidI2cIsActive() { return hidI2cActive; }

/* HID-over-I2C register address for HID descriptor (spec §5.1) */
#define HID_DESC_REG     0x0001

/* Command opcodes - placed in byte[2] of the 4-byte command frame:
 *   [cmdReg_lo][cmdReg_hi][opcode][param]
 * Source: HID over I2C spec §7.2 and Linux i2c-hid-core.c              */
#define HID_OP_SET_POWER 0x08   /* was incorrectly 0x0800 in prior version */
#define HID_OP_RESET     0x01   /* was incorrectly 0x0100 in prior version */
#define HID_POWER_ON     0x00

/* Touchpad resolution - from actual HID report descriptor on MSI Modern 14.
 * Extracted from /sys/kernel/debug/hid/0018:04F3:3282.0001/rdesc:
 *   X: 26 5b 0e  → logical max = 0x0E5B = 3675
 *   Y: 26 d5 08  → logical max = 0x08D5 = 2261
 * Report ID for multitouch: 0x04  (from rdesc: 85 04 = Report ID 4)
 * Driver used by Linux: hid-multitouch (not elan_i2c - standard HID mode) */
#define ELAN_MAX_X  3675
#define ELAN_MAX_Y  2261
#define HID_REPORT_ID_TOUCH  0x04

/* Counter for raw report dumps - print first N reports unconditionally
 * so we can verify byte layout against the rdesc on real hardware.      */
static int hidRawDumpCount = 0;
#define HID_RAW_DUMP_MAX 0


void hid_initialize()
{
    uint64_t i2c_virtual_base_address = i2cGetBase();
    if (i2c_virtual_base_address == 0) { printf("[HID] warning: no I2C controller at boot\n"); return; }
    printf("  base=%x", i2c_virtual_base_address);
    if (hidI2cIsActive()) {
        printf("  HID already active from boot — touchpad running.\n");
        printf("  vendor=%x\n", hidI2cGetDesc()->wVendorID);
        printf("  product=%x\n", hidI2cGetDesc()->wProductID);
        printf("  inputReg=%x\n", hidI2cGetDesc()->wInputRegister);
        return;
    }
    HidI2cDescriptor hid_descriptor;
    if (hidI2cInit(i2c_virtual_base_address, I2C_ADDR_ELAN_TOUCHPAD, &hid_descriptor) < 0) {
        printf("hidI2cInit failed\n"); return;
    }
    printf("HID ready. Raw report dump (5 reads)...\n");
    uint8_t  input_register_address_bytes[2] = {
        (uint8_t)(hid_descriptor.wInputRegister & 0xFF),
        (uint8_t)(hid_descriptor.wInputRegister >> 8)
    };
    uint16_t maximum_input_report_length = hid_descriptor.wMaxInputLength;
    if (maximum_input_report_length < 4 || maximum_input_report_length > 64)
        maximum_input_report_length = 32;
    for (int report_read_index = 0; report_read_index < 5; report_read_index++) {
        sleep(120);
        uint8_t report_receive_buffer[64] = {0};
        int bytes_received = i2cWriteRead(i2c_virtual_base_address,
                                           input_register_address_bytes, 2,
                                           report_receive_buffer, maximum_input_report_length);
        if (bytes_received < 0) { printf("  read error\n"); continue; }
        uint16_t reported_packet_length = (uint16_t)report_receive_buffer[0]
                                        | ((uint16_t)report_receive_buffer[1] << 8);
        printf("  [%d", report_read_index);
        printf("] id=%x", report_receive_buffer[2]);
        printf(" len=%d", reported_packet_length);
        printf(" : ");
        for (int byte_index = 0; byte_index < bytes_received && byte_index < 12; byte_index++) {
            if (report_receive_buffer[byte_index] < 0x10) printf("0");
            printf("%d ", report_receive_buffer[byte_index]);
        }
    }
    printf("hid done.");
}

static int hidReadReg(uint64_t base, uint16_t reg, uint8_t* buf, uint16_t len) {
    uint8_t addr[2] = { (uint8_t)(reg & 0xFF), (uint8_t)(reg >> 8) };
    return i2cWriteRead(base, addr, 2, buf, len);
}

/* Send the standard 4-byte HID-over-I2C command frame (spec §7.2):
 *   byte 0: wCommandRegister low byte
 *   byte 1: wCommandRegister high byte
 *   byte 2: opcode  (HID_OP_*)
 *   byte 3: param   (e.g. HID_POWER_ON or 0x00)                        */
static int hidWriteCmd(uint64_t base, uint16_t cmdReg,
                       uint8_t opcode, uint8_t param) {
    uint8_t buf[4];
    buf[0] = (uint8_t)(cmdReg & 0xFF);
    buf[1] = (uint8_t)(cmdReg >> 8);
    buf[2] = opcode;
    buf[3] = param;
    return i2cSend(base, buf, 4);
}

const HidI2cDescriptor* hidI2cGetDesc() { return &hidGlobalDesc; }

int hidI2cGetDescriptor(uint64_t base, uint8_t addr,
                        HidI2cDescriptor* out) {

    (void)addr;
    
    /* Flush any stale RX FIFO bytes before reading descriptor */
    while (i2cRead(base, DW_IC_RXFLR) & 0xFF)
        i2cRead(base, DW_IC_DATA_CMD);
        
    uint8_t buf[30];

    int ok = -1;
    for (int attempt = 0; attempt < 5 && ok < 0; attempt++) {
        memset(buf, 0, sizeof(buf));
        if (attempt > 0) {
            printf("[hid] descriptor retry %d\n", attempt);
            sleep(20);
        }
        ok = hidReadReg(base, HID_DESC_REG, buf, 30);
    }
    if (ok < 0) {
        printf("[hid] descriptor read failed\n");
        return -1;
    }

    memcpy(out, buf, sizeof(HidI2cDescriptor));

    printf("[hid] descriptor: len=%u bcd=0x%04x\n",
           out->wHIDDescLength, out->bcdVersion);
    printf("[hid]   wInputRegister  =0x%04x  wMaxInputLength=%u\n",
           out->wInputRegister, out->wMaxInputLength);
    printf("[hid]   wCommandRegister=0x%04x  wDataRegister  =0x%04x\n",
           out->wCommandRegister, out->wDataRegister);
    printf("[hid]   wVendorID=0x%04x  wProductID=0x%04x  wVersionID=0x%04x\n",
           out->wVendorID, out->wProductID, out->wVersionID);

    if (out->wHIDDescLength < 18 || out->wHIDDescLength > 30) {
        printf("[hid] warning: descriptor length suspicious: %u\n", out->wHIDDescLength);
        //return -1;
    }
    return 0;
}

int hidI2cReset(uint64_t base, uint8_t addr,
                const HidI2cDescriptor* desc) {
    (void)addr;

    // Helper for retrying I2C commands with delay
    #define RETRY_MAX 5
    #define RETRY_DELAY_MS 20

    /* Flush any stale TX abort state before issuing commands */
    i2cWrite(base, DW_IC_ENABLE, 0);
    sleep(10);
    i2cRead(base, DW_IC_CLR_TX_ABRT);
    i2cRead(base, DW_IC_CLR_INTR);
    i2cWrite(base, DW_IC_ENABLE, 1);
    sleep(10);

    /* SET_POWER ON - retry up to RETRY_MAX times */
    int power_ok = 0;
    for (int attempt = 0; attempt < RETRY_MAX; attempt++) {
        if (hidWriteCmd(base, desc->wCommandRegister,
                        HID_OP_SET_POWER, HID_POWER_ON) >= 0) {
            power_ok = 1;
            break;
        }
        printf("[hid] SET_POWER ON attempt %d failed, retrying...\n", attempt+1);
        sleep(RETRY_DELAY_MS);
    }
    if (!power_ok) {
        printf("[hid] SET_POWER ON failed after %d attempts\n", RETRY_MAX);
        return -1;
    }
    sleep(20);  // device needs settling time after power-on

    /* RESET - retry up to RETRY_MAX times */
    int reset_ok = 0;
    for (int attempt = 0; attempt < RETRY_MAX; attempt++) {
        if (hidWriteCmd(base, desc->wCommandRegister,
                        HID_OP_RESET, 0x00) >= 0) {
            reset_ok = 1;
            break;
        }
        printf("[hid] RESET attempt %d failed, retrying...\n", attempt+1);
        sleep(RETRY_DELAY_MS);
    }
    if (!reset_ok) {
        printf("[hid] RESET failed after %d attempts\n", RETRY_MAX);
        return -1;
    }

    /* Spec §7.2.1.2: after RESET the device asserts its interrupt line
     * and sends exactly two zero bytes {0x00, 0x00} as a sentinel.
     * This MUST be drained before any real report read. */
    sleep(50);  // give device time to produce sentinel

    uint8_t sentinel[2];
    uint8_t inputReg[2] = { (uint8_t)(desc->wInputRegister & 0xFF),
                             (uint8_t)(desc->wInputRegister >> 8) };

    int sentinel_ok = 0;
    for (int attempt = 0; attempt < RETRY_MAX; attempt++) {
        sentinel[0] = 0xFF;
        sentinel[1] = 0xFF;
        i2cWriteRead(base, inputReg, 2, sentinel, 2);
        printf("[hid] sentinel attempt %d: %02x %02x\n", attempt+1, sentinel[0], sentinel[1]);

        if (sentinel[0] == 0x00 && sentinel[1] == 0x00) {
            printf("[hid] sentinel OK after %d attempts\n", attempt+1);
            sentinel_ok = 1;
            break;
        }
        if (attempt < RETRY_MAX - 1) {
            sleep(RETRY_DELAY_MS);
        }
    }
    if (!sentinel_ok) {
        printf("[hid] sentinel failed after %d attempts (last read: %02x %02x)\n",
               RETRY_MAX, sentinel[0], sentinel[1]);
        // non-fatal? We'll continue but warn.
    }

    /* SET_REPORT mode – this command is usually reliable, but retry if needed */
    uint8_t setMode[6];
    setMode[0] = (uint8_t)(desc->wCommandRegister & 0xFF);
    setMode[1] = (uint8_t)(desc->wCommandRegister >> 8);
    setMode[2] = 0x03;   // SET_REPORT opcode
    setMode[3] = 0x03;   // report type: Feature (0x03) | report ID: 0x00 → = 0x03
    setMode[4] = (uint8_t)(desc->wDataRegister & 0xFF);
    setMode[5] = (uint8_t)(desc->wDataRegister >> 8);

    int setmode_ok = 0;
    for (int attempt = 0; attempt < RETRY_MAX; attempt++) {
        if (i2cSend(base, setMode, 6) == 0) {
            setmode_ok = 1;
            break;
        }
        printf("[hid] SET_REPORT mode attempt %d failed, retrying...\n", attempt+1);
        sleep(RETRY_DELAY_MS);
    }
    if (!setmode_ok) {
        printf("[hid] SET_REPORT mode failed after %d attempts\n", RETRY_MAX);
        // non-fatal, device may still work
    } else {
        printf("[hid] SET_REPORT mode cmd sent\n");
    }
    sleep(20);

    memset(hidLastReport, 0, sizeof(hidLastReport));
    hidLastReportLen = 0;
    hidRawDumpCount = 0;

    printf("[hid] reset complete\n");
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

    /* HID-over-I2C framing (spec §5.2):
     *   buf[0..1] = total packet length LE (includes these 2 bytes)
     *   buf[2]    = HID report ID
     *   buf[3..N] = report payload                                      */
    uint16_t reportLen = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);

    /* Empty / idle packet */
    if (reportLen < 3 || reportLen > maxLen) return 0;

    if (reportLen == hidLastReportLen &&
        memcmp(buf, hidLastReport, reportLen) == 0) return 0;
    memcpy(hidLastReport, buf, reportLen);
    hidLastReportLen = reportLen;

    uint8_t reportId = buf[2];

    /* Unconditionally dump first HID_RAW_DUMP_MAX reports so we can
     * verify byte layout against the rdesc on real hardware.
     * Remove or guard with a flag once layout is confirmed.             */
    if (hidRawDumpCount < HID_RAW_DUMP_MAX) {
        printf("[hid] raw[%d]: len=%u id=0x%02x  ", hidRawDumpCount, reportLen, reportId);
        int dumpN = (int)reportLen < 16 ? (int)reportLen : 16;
        for (int i = 3; i < dumpN + 2 && i < (int)maxLen; i++) {
            printf("%02x ", buf[i]);
        }
        printf("\n");
        hidRawDumpCount++;
    }

    /* ------------------------------------------------------------------ *
     * Report ID 0x04 - standard HID multitouch (primary touch report)    *
     *                                                                     *
     * Confirmed from rdesc on MSI Modern 14 (04F3:3282):                 *
     *   Report ID 4, Usage Page: Digitizer, Usage: Touch Pad             *
     *   Contact collection:                                               *
     *     Confidence (1 bit) + Tip Switch (1 bit) → buf[3] bits [0,1]    *
     *     Padding (2 bits)                         → buf[3] bits [2,3]    *
     *     Contact ID (4 bits)                      → buf[3] bits [4..7]   *
     *     X: 16-bit LE, max 3675                   → buf[4..5]            *
     *     Y: 16-bit LE, max 2261                   → buf[6..7]            *
     *   Scan Time (16-bit)                         → follows contacts     *
     *   Contact Count (8-bit)                                             *
     *   Button 1 (1 bit)                                                  *
     *                                                                     *
     * NOTE: byte offsets beyond [7] depend on how many contacts precede  *
     * the button field. Verify with the raw dump on first boot.          *
     * ------------------------------------------------------------------ */
    if (reportId == HID_REPORT_ID_TOUCH) {
        /* Tip Switch is bit 1 of buf[3] (Confidence is bit 0 per rdesc) */
        out->fingerDown = (buf[3] & 0x02) != 0;
        /* Button field position depends on contact count field value.
         * For single-contact packets it's typically at buf[10] bit 0.
         * Set leftClick conservatively from buf[3] bit 0 (Confidence)
         * until the raw dump confirms the real button byte position.    */
        out->leftClick  = false;  /* will be fixed once raw dump confirms offset */

        if (out->fingerDown && reportLen >= 8) {
            out->x = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
            out->y = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8);
            /* Clamp to declared max */
            if (out->x > ELAN_MAX_X) out->x = ELAN_MAX_X;
            if (out->y > ELAN_MAX_Y) out->y = ELAN_MAX_Y;
        }
        return 1;
    }

    /* ------------------------------------------------------------------ *
     * Report ID 0x01 - relative mouse fallback                           *
     * Present in the rdesc as a separate top-level collection.           *
     * Layout: flags byte, rel-X byte, rel-Y byte (standard HID mouse).  *
     * ------------------------------------------------------------------ */
    if (reportId == 0x01) {
        /* Relative mouse report — rdesc: flags, rel-X (signed), rel-Y (signed).
         * Device is in relative mode. Use deltas to move cursor.
         * Return 2 so caller applies as relative, not absolute.         */
        out->leftClick  = (buf[3] & 0x01) != 0;
        out->rightClick = (buf[3] & 0x02) != 0;
        out->fingerDown = true;
        out->x = (uint16_t)(int8_t)buf[4];   /* signed delta, cast via int8 */
        out->y = (uint16_t)(int8_t)buf[5];
        return 2;
    }

    /* Unknown report ID - already dumped above if within dump window */
    return 0;
}

int hidI2cInit(uint64_t base, uint8_t addr, HidI2cDescriptor* out) {
    if (hidI2cGetDescriptor(base, addr, out) < 0) return -1;
    if (hidI2cReset(base, addr, out) < 0) return -1;
    printf("[hid] ready: vendor=0x%04x product=0x%04x\n",
           out->wVendorID, out->wProductID);
    hidGlobalBase = base;
    hidGlobalAddr = addr;
    hidGlobalDesc = *out;
    hidI2cActive  = true;
    return 0;
}

void hidI2cPoll(uint64_t base, uint8_t addr,
                const HidI2cDescriptor* desc) {
    TouchReport rep;
    int r = hidI2cReadReport(base, addr, desc, &rep);
    if (r == 0 || r < 0) return;

    if (r == 1) {
        /* Absolute multitouch (report 0x04) */
        mouse_position_x = (int)((uint32_t)rep.x * SCREEN_WIDTH  / ELAN_MAX_X);
        mouse_position_y = (int)((uint32_t)rep.y * SCREEN_HEIGHT / ELAN_MAX_Y);
    } else if (r == 2) {
        int dx = (int)(int16_t)rep.x;
        int dy = (int)(int16_t)rep.y;
        if (dx != 0) {
            int sign = dx > 0 ? 1 : -1;
            int abs_d = dx < 0 ? -dx : dx;
            mouse_position_x += sign * (abs_d > 3 ? abs_d + (abs_d - 3) : abs_d);
        }
        if (dy != 0) {
            int sign = dy > 0 ? 1 : -1;
            int abs_d = dy < 0 ? -dy : dy;
            mouse_position_y += sign * (abs_d > 3 ? abs_d + (abs_d - 3) : abs_d);
        }
    }

    if (mouse_position_x < 0) mouse_position_x = 0;
    if (mouse_position_y < 0) mouse_position_y = 0;
    if (mouse_position_x > (int)SCREEN_WIDTH  - 1)
        mouse_position_x = (int)SCREEN_WIDTH  - 1;
    if (mouse_position_y > (int)SCREEN_HEIGHT - 1)
        mouse_position_y = (int)SCREEN_HEIGHT - 1;

    clickedLeft = rep.leftClick;
    clickedRight = rep.rightClick;
}

/* Called from timer_irq_0 at 60 Hz. No printf, no blocking. */
void hidI2cTickPoll() {
    if (!hidI2cActive || !hidGlobalBase) return;

    hidI2cPoll(hidGlobalBase, hidGlobalAddr, &hidGlobalDesc);
    GUI_input_loop();
}