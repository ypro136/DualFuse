#include <i2c.h>
#include <minimal_acpi.h>
#include <bootloader.h>
#include <pci.h>
#include <stdio.h>
#include <string.h>
#include <timer.h>


static uint32_t pciReadDword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint16_t lo = config_read_word(bus, slot, func, offset);
    uint16_t hi = config_read_word(bus, slot, func, (uint8_t)(offset + 2));
    return ((uint32_t)hi << 16) | lo;
}

static const uint8_t* i2cMemMem(const uint8_t* hay,  uint32_t hlen,
                                  const uint8_t* needle, uint32_t nlen) {
    if (nlen == 0 || hlen < nlen) return nullptr;
    const uint8_t* end = hay + hlen - nlen;
    for (const uint8_t* p = hay; p <= end; p++) {
        if (p[0] == needle[0] && memcmp(p, needle, nlen) == 0)
            return p;
    }
    return nullptr;
}

/* Full Intel PCH reserved MMIO range: 0xFE000000-0xFEFFFFFF.
 * Excludes HPET (0xFED00000-0xFEDFFFFF), APIC (0xFEC00000-0xFECFFFFF).
 * No alignment requirement - PCH LPSS bases can be sub-page-aligned.   */
static bool i2cPlausibleMmio32(uint32_t addr) {
    if (addr < 0xFE000000u || addr > 0xFEFFFFFFu) return false;
    if (addr >= 0xFEC00000u && addr <= 0xFECFFFFFu) return false; /* IOAPIC */
    if (addr >= 0xFED00000u && addr <= 0xFEDFFFFFu) return false; /* HPET   */
    if (addr >= 0xFEE00000u && addr <= 0xFEEFFFFFu) return false; /* LAPIC  */
    return true;
}

static bool i2cPlausibleMmio64(uint64_t addr) {
    return i2cPlausibleMmio32((uint32_t)(addr & 0xFFFFFFFFull));
}


/* Walk PCI capability list on 00:15.func, find Power Management cap (ID=0x01),
 * write 0 to PM_CSR to move device from D3 -> D0.
 * LPSS I2C is left in D3 by firmware; MMIO reads all-zero until D0.      */
static void i2cSetD0(uint8_t func) {
    uint8_t capPtr = (uint8_t)(config_read_word(I2C_PCH_PCI_BUS,
                                I2C_PCH_PCI_DEVICE, func, 0x34) & 0xFF);
    int safety = 48;
    while (capPtr >= 0x40 && safety-- > 0) {
        uint16_t capWord = config_read_word(I2C_PCH_PCI_BUS,
                                            I2C_PCH_PCI_DEVICE, func, capPtr);
        uint8_t capId   = (uint8_t)(capWord & 0xFF);
        uint8_t capNext = (uint8_t)(capWord >> 8);
        if (capId == 0x01) {
            /* PM_CSR is at cap_offset + 4; write 0x0000 = D0, no PME */
            config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, func,
                               (uint8_t)(capPtr + 4), 0x00000000);
            /* Enable Memory Space (bit1) + Bus Master (bit2) in command reg */
            uint16_t cmd = config_read_word(I2C_PCH_PCI_BUS,
                                            I2C_PCH_PCI_DEVICE, func, 0x04);
            cmd |= (1u << 1) | (1u << 2);
            config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, func,
                               0x04, cmd);
            printf("[i2c] PCI 00:15.%d: D0 done, mem space enabled (cmd=0x%04x)\n",
                   func, cmd);
            return;
        }
        capPtr = capNext;
    }
    printf("[i2c] PCI 00:15.%d: no PM capability found, proceeding anyway\n", func);
}

/* Call i2cSetD0 for all matching I2C functions found on 00:15.x.
 * Safe to call before or after i2cFindBase().                            */
void i2cPowerOnAll() {
    static const uint16_t knownDevIds[] = {
        I2C_ADL_PCH_DEV_ID_I2C0, I2C_ADL_PCH_DEV_ID_I2C1,
        I2C_ADL_PCH_DEV_ID_I2C2, I2C_ADL_PCH_DEV_ID_I2C3,
    };
    for (uint8_t func = 0; func < 4; func++) {
        uint16_t vendor = config_read_word(I2C_PCH_PCI_BUS,
                                           I2C_PCH_PCI_DEVICE, func, 0x00);
        if (vendor != I2C_INTEL_VENDOR_ID) continue;
        uint16_t devId = config_read_word(I2C_PCH_PCI_BUS,
                                          I2C_PCH_PCI_DEVICE, func, 0x02);
        bool match = false;
        for (int i = 0; i < 4; i++)
            if (devId == knownDevIds[i]) { match = true; break; }
        if (match) i2cSetD0(func);
    }
}

/* Deassert LPSS functional resets at BAR0+0x204.
 * Must be called after virtBase is known and before any DW register access. */
void i2cDeassertReset(uint64_t virtBase) {
    volatile uint32_t* clkReg   = (volatile uint32_t*)(virtBase + 0x200);
    volatile uint32_t* resetReg = (volatile uint32_t*)(virtBase + 0x204);
    *clkReg   = 0x1;          /* LPSS_PRIV_CLK: enable clock gate */
    *resetReg = 0x0;          /* assert all resets */
    *resetReg = 0x7;          /* deassert: func + iDMA (bits 2:0) */
    printf("[i2c] LPSS clock enabled, resets deasserted\n");
}

static I2cBaseResult i2cFindViaPci() {
    static const uint16_t knownDevIds[] = {
        I2C_ADL_PCH_DEV_ID_I2C0,
        I2C_ADL_PCH_DEV_ID_I2C1,
        I2C_ADL_PCH_DEV_ID_I2C2,
        I2C_ADL_PCH_DEV_ID_I2C3,
    };

    for (uint8_t func = 0; func < 4; func++) {
        uint16_t vendor = config_read_word(I2C_PCH_PCI_BUS,
                                           I2C_PCH_PCI_DEVICE, func, 0x00);
        if (vendor != I2C_INTEL_VENDOR_ID) continue;

        uint16_t devId = config_read_word(I2C_PCH_PCI_BUS,
                                           I2C_PCH_PCI_DEVICE, func, 0x02);

        bool match = false;
        for (uint32_t i = 0; i < 4; i++) {
            if (devId == knownDevIds[i]) { match = true; break; }
        }
        if (!match) continue;

        uint32_t bar0_lo = pciReadDword(I2C_PCH_PCI_BUS,
                                        I2C_PCH_PCI_DEVICE, func, 0x10);
        uint32_t bar0_hi = pciReadDword(I2C_PCH_PCI_BUS,
                                        I2C_PCH_PCI_DEVICE, func, 0x14);

        if (bar0_lo & 0x1) {
            printf("[i2c] PCI 00:15.%d has I/O BAR0  -  skipping\n", func);
            continue;
        }

        uint64_t physBase = ((uint64_t)bar0_hi << 32) | (bar0_lo & ~0xFull);

        if (physBase == 0) {
            /* ADL-P firmware leaves BAR0 unprogrammed - use known base */
            static const uint32_t adlFallback[] = {
                0xFE040000, 0xFE041000, 0xFE042000, 0xFE043000
            };
            physBase = adlFallback[func < 4 ? func : 0];
            printf("[i2c] PCI 00:15.%d BAR0 zero, using ADL default 0x%lx\n",
                   func, physBase);
            config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, func,
                               0x10, (uint32_t)physBase);
        }

        uint64_t virtBase = bootloader.hhdmOffset + physBase;

        printf("[i2c] PCI path: found vendor=0x%04x devId=0x%04x at 00:15.%d\n",
               vendor, devId, func);
        printf("[i2c] PCI path: physBase=0x%lx virtBase=0x%lx\n",
               physBase, virtBase);

        I2cBaseResult result;
        result.virtBase    = virtBase;
        result.pciFunction = func;
        result.foundViaPci = true;
        return result;
    }

    I2cBaseResult none;
    none.virtBase    = 0;
    none.pciFunction = 0xFF;
    none.foundViaPci = true;
    return none;
}


static I2cBaseResult i2cFindViaDsdt() {
    I2cBaseResult none;
    none.virtBase    = 0;
    none.pciFunction = 0xFF;
    none.foundViaPci = false;

    AcpiDsdtInfo dsdt = acpiGetDsdt();
    if (!dsdt.ptr || dsdt.length < 36) {
        printf("[i2c] DSDT fallback: DSDT not found or too small\n");
        return none;
    }

    printf("[i2c] DSDT fallback: scanning %u bytes at virt 0x%lx\n",
           dsdt.length, (uint64_t)(uintptr_t)dsdt.ptr);

    static const char* const hidStrings[] = {
        "INTC10B",   /* Alder Lake I2C0 */
        "INTC10C",   /* Alder Lake I2C1 */
        "INTC10D",   /* Alder Lake I2C2 */
        "INTC10E",   /* Alder Lake I2C3 (some firmware variants) */
        "INT33C2",   /* Haswell/Broadwell */
        "INT33C3",   /* Haswell/Broadwell */
        "INT3432",   /* Skylake/Kaby Lake */
        "INT3433",   /* Skylake/Kaby Lake */
        nullptr
    };

    const uint8_t* base_ptr = dsdt.ptr;
    uint32_t       base_len = dsdt.length;

    for (int hidIdx = 0; hidStrings[hidIdx] != nullptr; hidIdx++) {
        const char*    hid    = hidStrings[hidIdx];
        const uint8_t* needle = (const uint8_t*)hid;
        uint32_t       nlen   = (uint32_t)strlen(hid);

        const uint8_t* match = i2cMemMem(base_ptr, base_len, needle, nlen);
        if (!match) continue;

        printf("[i2c] DSDT: found HID string \"%s\" at DSDT offset 0x%lx\n",
               hid, (uint64_t)(match - base_ptr));

        /* Scan from HID hit to end of DSDT for Memory32Fixed/QWordMemory.
         * Address filter is tight (ADL PCH I2C range only) so no window needed. */
        const uint8_t* dsdt_end = base_ptr + base_len;

        for (const uint8_t* p = match; p + 8 <= dsdt_end; p++) {

            /* Memory32Fixed: 0x86 0x09 0x00 | rw | base32 | len32 */
            if (p[0] == 0x86 && p[1] == 0x09 && p[2] == 0x00) {
                if (p + 12 > dsdt_end) break;
                uint32_t phys =
                    (uint32_t)p[4] | ((uint32_t)p[5]<<8) |
                    ((uint32_t)p[6]<<16) | ((uint32_t)p[7]<<24);
                if (!i2cPlausibleMmio32(phys)) continue;
                uint64_t virtBase = bootloader.hhdmOffset + (uint64_t)phys;
                printf("[i2c] DSDT: Memory32Fixed phys=0x%08x virt=0x%lx - VALID\n",
                       phys, virtBase);
                /* Program phys into BAR0 of 00:15.0 so the PCH decodes it */
                config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, 0,
                                   0x10, phys);
                printf("[i2c] BAR0 programmed with phys=0x%08x\n", phys);
                I2cBaseResult result;
                result.virtBase = virtBase; result.pciFunction = 0xFF;
                result.foundViaPci = false;
                return result;
            }

            /* QWordMemory: 0x8A | len16 | ... | base64 at +10 */
            if (p[0] == 0x8A) {
                uint16_t rlen = (uint16_t)p[1] | ((uint16_t)p[2]<<8);
                if (rlen < 42 || p + 18 > dsdt_end) continue;
                uint64_t phys =
                    (uint64_t)p[10] | ((uint64_t)p[11]<<8) |
                    ((uint64_t)p[12]<<16) | ((uint64_t)p[13]<<24) |
                    ((uint64_t)p[14]<<32) | ((uint64_t)p[15]<<40) |
                    ((uint64_t)p[16]<<48) | ((uint64_t)p[17]<<56);
                if (!i2cPlausibleMmio64(phys)) continue;
                uint64_t virtBase = bootloader.hhdmOffset + phys;
                printf("[i2c] DSDT: QWordMemory phys=0x%lx virt=0x%lx - VALID\n",
                       phys, virtBase);
                I2cBaseResult result;
                result.virtBase = virtBase; result.pciFunction = 0xFF;
                result.foundViaPci = false;
                return result;
            }
        }

        printf("[i2c] DSDT: HID \"%s\" found but no ADL PCH I2C address"
               " in DSDT\n", hid);
    }

    printf("[i2c] DSDT fallback: no I2C controller resource found\n");
    return none;
}



/* Print hex bytes around a DSDT offset - for diagnosing false positives.
 * Call from shell I2CDSDT command.                                       */
void i2cDumpDsdtContext() {
    AcpiDsdtInfo dsdt = acpiGetDsdt();
    if (!dsdt.ptr) { printf("[i2c] DSDT not found\n"); return; }

    static const char* const hids[] = {
        "INTC10B","INTC10C","INTC10D","INT33C2","INT33C3", nullptr
    };
    const uint8_t* base = dsdt.ptr;
    uint32_t       len  = dsdt.length;

    for (int h = 0; hids[h]; h++) {
        const uint8_t* match = i2cMemMem(base, len,
                                          (const uint8_t*)hids[h],
                                          (uint32_t)strlen(hids[h]));
        if (!match) continue;
        printf("[i2c] HID %s at DSDT+0x%lx\n",
               hids[h], (uint64_t)(match - base));

        /* Scan forward for any Memory32Fixed in full DSDT range */
        const uint8_t* end = base + len;
        int found = 0;
        for (const uint8_t* p = match; p + 12 <= end && found < 8; p++) {
            if (p[0] != 0x86 || p[1] != 0x09 || p[2] != 0x00) continue;
            uint32_t phys = (uint32_t)p[4] | ((uint32_t)p[5]<<8) |
                            ((uint32_t)p[6]<<16) | ((uint32_t)p[7]<<24);
            uint32_t offset = (uint32_t)(p - base);
            /* print context: 4 bytes before descriptor, then full descriptor */
            printf("  +0x%05x: ", offset);
            const uint8_t* ctx = (p >= base + 4) ? p - 4 : base;
            for (const uint8_t* b = ctx; b < p + 12 && b < end; b++)
                printf("%02x ", *b);
            printf("  phys=0x%08x\n", phys);
            found++;
        }
        if (!found) printf("  no Memory32Fixed found after HID\n");
    }
}

/* Try known ADL-P PCH LPSS I2C MMIO base addresses and return the first
 * one that reads IC_COMP_TYPE == 0x44570140 after reset deassert.
 * Use when DSDT scan fails to find a valid dynamic _CRS address.        */
I2cBaseResult i2cScanKnownBases() {
    static const uint32_t candidates[] = {
        0xFE040000,   /* ADL-P I2C0 (00:15.0) - Intel reference default */
        0xFE041000,   /* ADL-P I2C1 */
        0xFE042000,   /* ADL-P I2C2 */
        0xFE043000,   /* ADL-P I2C3 */
        0xFE044000,   /* ADL-P I2C4 */
        0xFE045000,   /* ADL-P I2C5 */
        0xFE0B0000,   /* rounded from DSDT false-positive 0xfe0b0400 */
        0xFE070000,   /* other DSDT candidate seen on MSI */
        0
    };

    I2cBaseResult none; none.virtBase = 0; none.pciFunction = 0xFF; none.foundViaPci = false;

    for (int i = 0; candidates[i]; i++) {
        uint64_t virtBase = bootloader.hhdmOffset + (uint64_t)candidates[i];

        config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, 0,
                           0x10, candidates[i]);

        volatile uint32_t* clkReg   = (volatile uint32_t*)(virtBase + 0x200);
        volatile uint32_t* resetReg = (volatile uint32_t*)(virtBase + 0x204);
        *clkReg   = 0x1;
        *resetReg = 0x0;
        *resetReg = 0x7;

        uint32_t comp = *((volatile uint32_t*)(virtBase + DW_IC_COMP_TYPE));
        printf("[i2c] scan 0x%08x: IC_COMP_TYPE=0x%08x%s\n",
               candidates[i], comp,
               comp == DW_IC_COMP_TYPE_VALUE ? " <-- VALID" : "");

        if (comp == DW_IC_COMP_TYPE_VALUE) {
            I2cBaseResult result;
            result.virtBase    = virtBase;
            result.pciFunction = 0;
            result.foundViaPci = false;
            return result;
        }
    }

    printf("[i2c] scan: no valid base found\n");
    return none;
}

I2cBaseResult i2cFindBase() {
    I2cBaseResult res = i2cFindViaPci();
    if (res.virtBase == 0) {
        res = i2cFindViaDsdt();
    }
    if (res.virtBase == 0) {
        printf("[i2c] i2cFindBase: FAILED\n");
    } else {
        printf("[i2c] i2cFindBase: virtBase=0x%lx\n", res.virtBase);
    }
    return res;
}

/* Disable controller, set clock divisors for 400 kHz at 100 MHz input,
 * configure as master fast-mode, set target address, re-enable.
 * Must call i2cDeassertReset() before this.                             */
void i2cInit(uint64_t virtBase, uint8_t targetAddr) {
    i2cWrite(virtBase, DW_IC_ENABLE, 0);

    i2cWrite(virtBase, DW_IC_SS_SCL_HCNT, DW_I2C_SS_SCL_HCNT_100MHZ);
    i2cWrite(virtBase, DW_IC_SS_SCL_LCNT, DW_I2C_SS_SCL_LCNT_100MHZ);
    i2cWrite(virtBase, DW_IC_FS_SCL_HCNT, DW_I2C_FS_SCL_HCNT_100MHZ);
    i2cWrite(virtBase, DW_IC_FS_SCL_LCNT, DW_I2C_FS_SCL_LCNT_100MHZ);

    i2cWrite(virtBase, DW_IC_CON, DW_IC_CON_MASTER_FAST_MODE);
    i2cWrite(virtBase, DW_IC_TAR, (uint32_t)targetAddr);

    i2cWrite(virtBase, DW_IC_INTR_MASK, 0);
    i2cRead (virtBase, DW_IC_CLR_INTR);

    i2cWrite(virtBase, DW_IC_ENABLE, 1);

    sleep(6); /* ~100ms - ELAN cold-start wake time */

    printf("[i2c] init done: target=0x%02x CON=0x%x\n",
           targetAddr, i2cRead(virtBase, DW_IC_CON));
}

/* Send a single read command to the current IC_TAR address and check
 * whether the device ACKs.
 * Returns: 1 = ACK (device present), 0 = NACK (no device), -1 = timeout.
 *
 * Sequence:
 *   1. Clear any previous TX abort
 *   2. Write IC_DATA_CMD with READ bit set - triggers address phase
 *   3. Poll until TX FIFO empty (address + command sent)
 *   4. Read IC_TX_ABRT_SOURCE:
 *        bit 0 set = 7-bit addr NACK
 *        0         = ACK received                                        */
int i2cProbeAck(uint64_t virtBase) {
    i2cRead(virtBase, DW_IC_CLR_TX_ABRT);
    i2cRead(virtBase, DW_IC_CLR_INTR);

    i2cWrite(virtBase, DW_IC_DATA_CMD, (1u << 8));

    uint32_t timeout = 100000;
    while (timeout--) {
        uint32_t status = i2cRead(virtBase, DW_IC_STATUS);
        if (status & DW_IC_STATUS_TFE) break;
    }
    if (timeout == 0) {
        printf("[i2c] probe: TX FIFO timeout\n");
        return -1;
    }

    sleep(1); /* NACK propagation */

    uint32_t abrt = i2cRead(virtBase, DW_IC_TX_ABRT_SOURCE);
    i2cRead(virtBase, DW_IC_CLR_TX_ABRT);

    printf("[i2c] probe: TX_ABRT_SOURCE=0x%x\n", abrt);

    if (abrt & DW_IC_TX_ABRT_ADDR_NOACK) return 0;
    return 1;
}

/* Called from kernel_main after initiateAPIC() and initiateMouse().
 * Brings up I2C0, confirms DesignWare, probes for ELAN touchpad at 0x15. */
void i2cInitialize() {
    i2cPowerOnAll();
    I2cBaseResult res = i2cFindBase();
    if (res.virtBase == 0) {
        printf("[i2c] initialize: controller not found\n");
        return;
    }
    i2cDeassertReset(res.virtBase);

    uint32_t comp = i2cRead(res.virtBase, DW_IC_COMP_TYPE);
    if (comp != DW_IC_COMP_TYPE_VALUE) {
        printf("[i2c] initialize: bad IC_COMP_TYPE 0x%x\n", comp);
        return;
    }

    i2cInit(res.virtBase, I2C_ADDR_ELAN_TOUCHPAD);
    int ack = i2cProbeAck(res.virtBase);
    if (ack == 1) {
        printf("[i2c] initialize: ELAN touchpad ACK at 0x%x\n",
               I2C_ADDR_ELAN_TOUCHPAD);
        /* leave controller enabled for HID-over-I2C in Session 3 */
    } else {
        printf("[i2c] initialize: no touchpad ACK (r=%d)\n", ack);
        i2cWrite(res.virtBase, DW_IC_ENABLE, 0);
    }
}

/* Write `len` bytes from `data` to the current IC_TAR target.
 * Controller must be enabled. Returns 0 on success, -1 on abort.        */
int i2cSend(uint64_t base, const uint8_t* data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t timeout = 100000;
        while (!(i2cRead(base, DW_IC_STATUS) & DW_IC_STATUS_TFNF) && timeout--) {}
        if (!timeout) { printf("[i2c] write: TX FIFO full timeout\n"); return -1; }
        i2cWrite(base, DW_IC_DATA_CMD, (uint32_t)data[i]);
    }
    /* wait for TX empty */
    uint32_t timeout = 200000;
    while (!(i2cRead(base, DW_IC_STATUS) & DW_IC_STATUS_TFE) && timeout--) {}
    uint32_t abrt = i2cRead(base, DW_IC_TX_ABRT_SOURCE);
    i2cRead(base, DW_IC_CLR_TX_ABRT);
    if (abrt) { printf("[i2c] write: TX_ABRT=0x%x\n", abrt); return -1; }
    return 0;
}

/* Issue `len` read commands then collect bytes into `buf`.
 * Controller must be enabled with IC_TAR already set.
 * Returns bytes read, or -1 on error.                                   */
int i2cRecv(uint64_t base, uint8_t* buf, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t timeout = 100000;
        while (!(i2cRead(base, DW_IC_STATUS) & DW_IC_STATUS_TFNF) && timeout--) {}
        if (!timeout) return -1;
        i2cWrite(base, DW_IC_DATA_CMD, (1u << 8));
    }
    for (uint32_t i = 0; i < len; i++) {
        uint32_t timeout = 200000;
        while (!(i2cRead(base, DW_IC_RXFLR) & 0xFF) && timeout--) {}
        if (!timeout) { printf("[i2c] read: RX timeout at byte %u\n", i); return (int)i; }
        buf[i] = (uint8_t)(i2cRead(base, DW_IC_DATA_CMD) & 0xFF);
    }
    return (int)len;
}

/* Write `wlen` bytes then read `rlen` bytes in a single I2C transaction
 * (repeated start). Required for HID-over-I2C register reads.
 * All bytes are pushed into IC_DATA_CMD before waiting for completion,
 * so DesignWare issues Sr (repeated start) instead of P+S.             */
int i2cWriteRead(uint64_t base,
                 const uint8_t* wbuf, uint32_t wlen,
                 uint8_t* rbuf,       uint32_t rlen) {
    /* queue write bytes */
    for (uint32_t i = 0; i < wlen; i++) {
        uint32_t timeout = 100000;
        while (!(i2cRead(base, DW_IC_STATUS) & DW_IC_STATUS_TFNF) && timeout--) {}
        if (!timeout) return -1;
        i2cWrite(base, DW_IC_DATA_CMD, (uint32_t)wbuf[i]);
    }
    /* queue read commands - last one gets STOP bit (bit 9) */
    for (uint32_t i = 0; i < rlen; i++) {
        uint32_t timeout = 100000;
        while (!(i2cRead(base, DW_IC_STATUS) & DW_IC_STATUS_TFNF) && timeout--) {}
        if (!timeout) return -1;
        uint32_t cmd = (1u << 8);
        if (i == rlen - 1) cmd |= (1u << 9); /* STOP */
        i2cWrite(base, DW_IC_DATA_CMD, cmd);
    }
    /* drain RX FIFO */
    for (uint32_t i = 0; i < rlen; i++) {
        uint32_t timeout = 200000;
        while (!(i2cRead(base, DW_IC_RXFLR) & 0xFF) && timeout--) {}
        if (!timeout) { printf("[i2c] writeread: RX timeout at %u\n", i); return (int)i; }
        rbuf[i] = (uint8_t)(i2cRead(base, DW_IC_DATA_CMD) & 0xFF);
    }
    uint32_t abrt = i2cRead(base, DW_IC_TX_ABRT_SOURCE);
    i2cRead(base, DW_IC_CLR_TX_ABRT);
    if (abrt) { printf("[i2c] writeread: TX_ABRT=0x%x\n", abrt); return -1; }
    return (int)rlen;
}