#include <i2c.h>
#include <minimal_acpi.h>
#include <bootloader.h>
#include <pci.h>
#include <stdio.h>
#include <string.h>
#include <timer.h>
#include <paging.h>

static uint64_t i2cInitializedBase = 0;


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

/* FIX: removed the 0xFE000000-0xFEFFFFFF range check.
 * On MSI Modern 14 Alder Lake, confirmed via /proc/iomem:
 *   i2c_designware.0 lives at 0x4017000000 - above 4 GB entirely.
 * The old check rejected every valid address on this board.
 * Real verification is IC_COMP_TYPE == 0x44570140 after mapping.       */
static bool i2cPlausibleMmio32(uint32_t addr) {
    return (addr >= 0x1000u && addr != 0xFFFFFFFFu && !(addr & 0x3u));
}

static bool i2cPlausibleMmio64(uint64_t addr) {
    if (addr == 0 || addr == 0xFFFFFFFFFFFFFFFFull) return false;
    if (addr & 0x3ull) return false;
    if (addr < 0x1000ull) return false;
    return true;
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
            config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, func,
                               (uint8_t)(capPtr + 4), 0x00000000);
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

void i2cDeassertReset(uint64_t virtBase) {
    volatile uint32_t* clkReg   = (volatile uint32_t*)(virtBase + 0x200);
    volatile uint32_t* resetReg = (volatile uint32_t*)(virtBase + 0x204);
    *clkReg   = 0x1;
    *resetReg = 0x0;
    *resetReg = 0x7;
    printf("[i2c] LPSS clock enabled, resets deasserted\n");
}

static void i2cMapMmio(uint64_t physBase) {
    uint64_t virtBase = bootloader.hhdmOffset + physBase;
    /* Only map if not already present — virtual_map on an already-mapped
     * page allocates a new PT entry and leaks the old one, or crashes.  */
    if (paging_virtual_to_physical(virtBase) != 0) {
        printf("[i2c] MMIO phys=0x%lx already mapped\n", physBase);
        return;
    }
    virtual_map(virtBase, physBase, PF_PRESENT | PF_RW | PF_PCD);
    printf("[i2c] mapped MMIO phys=0x%lx virt=0x%lx\n", physBase, virtBase);
}

/* FIX: adlFallback is now uint64_t with the addresses confirmed from
 * /proc/iomem on the MSI Modern 14:
 *   4017000000-40170001ff : i2c_designware.0 lpss_dev
 *   4017001000-40170011ff : i2c_designware.1 lpss_dev
 * Both BAR0 (offset 0x10) and BAR1 (offset 0x14) must be written for a
 * 64-bit BAR - writing only BAR0 leaves the high 32 bits stale.         */
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

        printf("[i2c] PCI 00:15.%d: vendor=0x%04x devId=0x%04x\n",
           func, vendor, devId);

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
            printf("[i2c] PCI 00:15.%d has I/O BAR0 - skipping\n", func);
            continue;
        }

        uint64_t physBase = ((uint64_t)bar0_hi << 32) | (bar0_lo & ~0xFull);

        printf("[i2c] PCI 00:15.%d: raw BAR lo=0x%08x hi=0x%08x => phys=0x%lx\n",
               func, bar0_lo, bar0_hi, physBase);

        if (physBase == 0) {
            /* FIX: 64-bit fallbacks - confirmed from /proc/iomem on MSI Modern 14.
             * Previous fallback used 0xFE04xxxx which is wrong for Alder Lake.  */
            static const uint64_t adlFallback[] = {
                0x4017000000ULL,   /* I2C0 - confirmed */
                0x4017001000ULL,   /* I2C1 - confirmed */
                0x4017002000ULL,   /* I2C2 */
                0x4017003000ULL,   /* I2C3 */
            };
            physBase = adlFallback[func < 4 ? func : 0];
            printf("[i2c] PCI 00:15.%d BAR0 zero, using ADL64 default 0x%lx\n",
                   func, physBase);
            /* Write both halves of the 64-bit BAR */
            config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, func,
                               0x10, (uint32_t)(physBase & 0xFFFFFFFFull));
            config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, func,
                               0x14, (uint32_t)(physBase >> 32));
        }

        i2cMapMmio(physBase);
        uint64_t virtBase = bootloader.hhdmOffset + physBase;

        printf("[i2c] PCI path: vendor=0x%04x devId=0x%04x at 00:15.%d\n",
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

    /* FIX: "PNP0C50" added first - this is the confirmed HID on the MSI
     * Modern 14 C12MO (Alder Lake). Linux reports:
     *   /sys/bus/acpi/devices/PNP0C50:00 → \_SB_.PC00.I2C0.TPD0
     * The previous list only had INTC10x / INT33Cx strings, none of which
     * appear in the MSI DSDT for the touchpad controller.                */
    static const char* const hidStrings[] = {
        "PNP0C50",   /* MSI Modern 14 Alder Lake - confirmed             */
        "INTC10B",   /* Alder Lake I2C0 (some OEM firmware)              */
        "INTC10C",   /* Alder Lake I2C1                                  */
        "INTC10D",   /* Alder Lake I2C2                                  */
        "INTC10E",   /* Alder Lake I2C3                                  */
        "INT33C2",   /* Haswell/Broadwell                                */
        "INT33C3",   /* Haswell/Broadwell                                */
        "INT3432",   /* Skylake/Kaby Lake                                */
        "INT3433",   /* Skylake/Kaby Lake                                */
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

        const uint8_t* dsdt_end = base_ptr + base_len;

        for (const uint8_t* p = match; p + 8 <= dsdt_end; p++) {

            /* Memory32Fixed: 0x86 0x09 0x00 | rw | base32 | len32 */
            if (p[0] == 0x86 && p[1] == 0x09 && p[2] == 0x00) {
                if (p + 12 > dsdt_end) break;
                uint32_t phys =
                    (uint32_t)p[4] | ((uint32_t)p[5]<<8) |
                    ((uint32_t)p[6]<<16) | ((uint32_t)p[7]<<24);
                if (!i2cPlausibleMmio32(phys)) {
                    printf("[i2c] DSDT: Memory32Fixed phys=0x%08x rejected\n", phys);
                    continue;
                }
                uint64_t virtBase = bootloader.hhdmOffset + (uint64_t)phys;
                printf("[i2c] DSDT: Memory32Fixed phys=0x%08x virt=0x%lx - VALID\n",
                       phys, virtBase);
                config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, 0,
                                   0x10, phys);
                config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, 0,
                                   0x14, 0);
                                i2cMapMmio((uint64_t)phys);
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
                if (!i2cPlausibleMmio64(phys)) {
                    printf("[i2c] DSDT: QWordMemory phys=0x%lx rejected\n", phys);
                    continue;
                }
                uint64_t virtBase = bootloader.hhdmOffset + phys;
                printf("[i2c] DSDT: QWordMemory phys=0x%lx virt=0x%lx - VALID\n",
                       phys, virtBase);
                /* Write both BAR halves */
                config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, 0,
                                   0x10, (uint32_t)(phys & 0xFFFFFFFFull));
                config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, 0,
                                   0x14, (uint32_t)(phys >> 32));
                                i2cMapMmio((uint64_t)phys);
                i2cMapMmio(phys);
                I2cBaseResult result;
                result.virtBase = virtBase; result.pciFunction = 0xFF;
                result.foundViaPci = false;
                return result;
            }
        }

        printf("[i2c] DSDT: HID \"%s\" found but no plausible I2C address in DSDT\n", hid);
    }

    printf("[i2c] DSDT fallback: no I2C controller resource found\n");
    return none;
}


void i2cDumpDsdtContext() {
    AcpiDsdtInfo dsdt = acpiGetDsdt();
    if (!dsdt.ptr) { printf("[i2c] DSDT not found\n"); return; }

    static const char* const hids[] = {
        "PNP0C50","INTC10B","INTC10C","INTC10D","INT33C2","INT33C3", nullptr
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

        const uint8_t* end = base + len;
        int found = 0;
        for (const uint8_t* p = match; p + 12 <= end && found < 8; p++) {
            if (p[0] != 0x86 || p[1] != 0x09 || p[2] != 0x00) continue;
            uint32_t phys = (uint32_t)p[4] | ((uint32_t)p[5]<<8) |
                            ((uint32_t)p[6]<<16) | ((uint32_t)p[7]<<24);
            uint32_t offset = (uint32_t)(p - base);
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

/* FIX: 64-bit candidate addresses - confirmed from /proc/iomem.
 * Both BAR halves written so PCI config space matches the mapping.      */
I2cBaseResult i2cScanKnownBases() {
    static const uint64_t candidates[] = {
        0x4017000000ULL,   /* ADL-P I2C0 - confirmed on MSI Modern 14   */
        0x4017001000ULL,   /* ADL-P I2C1 - confirmed on MSI Modern 14   */
        0x4017002000ULL,
        0x4017003000ULL,
        0
    };

    I2cBaseResult none;
    none.virtBase = 0; none.pciFunction = 0xFF; none.foundViaPci = false;

    for (int i = 0; candidates[i]; i++) {
        i2cMapMmio(candidates[i]);
        uint64_t virtBase = bootloader.hhdmOffset + candidates[i];

        config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, 0,
                           0x10, (uint32_t)(candidates[i] & 0xFFFFFFFFull));
        config_write_dword(I2C_PCH_PCI_BUS, I2C_PCH_PCI_DEVICE, 0,
                           0x14, (uint32_t)(candidates[i] >> 32));

        volatile uint32_t* clkReg   = (volatile uint32_t*)(virtBase + 0x200);
        volatile uint32_t* resetReg = (volatile uint32_t*)(virtBase + 0x204);
        *clkReg   = 0x1;
        *resetReg = 0x0;
        *resetReg = 0x7;

        uint32_t comp = *((volatile uint32_t*)(virtBase + DW_IC_COMP_TYPE));
        printf("[i2c] scan 0x%lx: IC_COMP_TYPE=0x%08x%s\n",
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
    /* Guard: only attempt on machines with an Intel device at 00:15.0.
     * On Toshiba (Haswell, no LPSS I2C) this slot returns 0xFFFF.
     * Without this check the DSDT scanner finds PNP0C50 (a generic HID
     * class string present on many platforms), maps a garbage address,
     * and crashes on the first register access.                        */
    uint16_t vendorCheck = config_read_word(0, 0x15, 0, 0x00);
    if (vendorCheck != I2C_INTEL_VENDOR_ID) {
        printf("[i2c] i2cFindBase: no Intel device at 00:15.0 (vendor=0x%04x) - skip\n",
               vendorCheck);
        I2cBaseResult none;
        none.virtBase = 0; none.pciFunction = 0xFF; none.foundViaPci = false;
        return none;
    }
    I2cBaseResult res = i2cFindViaPci();
    if (res.virtBase == 0) {
        res = i2cFindViaDsdt();
    }
    // if (res.virtBase == 0) {
    //     printf("[i2c] all paths failed - trying known-base scan\n");
    //     res = i2cScanKnownBases();
    // }
    if (res.virtBase == 0) {
        printf("[i2c] i2cFindBase: FAILED\n");
    } else {
        printf("[i2c] i2cFindBase: virtBase=0x%lx\n", res.virtBase);
    }
    return res;
}

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

    sleep(6);

    printf("[i2c] init done: target=0x%02x CON=0x%x STATUS=0x%x\n",
           targetAddr,
           i2cRead(virtBase, DW_IC_CON),
           i2cRead(virtBase, DW_IC_STATUS));
}

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

    sleep(1);

    uint32_t abrt = i2cRead(virtBase, DW_IC_TX_ABRT_SOURCE);
    i2cRead(virtBase, DW_IC_CLR_TX_ABRT);

    printf("[i2c] probe: TX_ABRT_SOURCE=0x%x\n", abrt);

    if (abrt & DW_IC_TX_ABRT_ADDR_NOACK) return 0;
    return 1;
}

uint64_t i2cGetBase() { return i2cInitializedBase; }

void i2cInitialize() 
{
    uint16_t v = config_read_word(0, 0x15, 0, 0x00);
    uint16_t d = config_read_word(0, 0x15, 0, 0x02);
    printf("[i2c] direct check 00:15.0: vendor=0x%04x devId=0x%04x\n", v, d);
    i2cPowerOnAll();
    I2cBaseResult res = i2cFindBase(); 

    if (res.virtBase == 0) {
        printf("[i2c] initialize: controller not found\n");
        return;
    }
    i2cDeassertReset(res.virtBase);

    uint32_t comp = i2cRead(res.virtBase, DW_IC_COMP_TYPE);
    printf("[i2c] IC_COMP_TYPE=0x%08x %s\n", comp,
           comp == DW_IC_COMP_TYPE_VALUE ? "(DesignWare OK)" : "(unexpected!)");
    if (comp != DW_IC_COMP_TYPE_VALUE) {
        printf("[i2c] initialize: bad IC_COMP_TYPE - aborting\n");
        return;
    }

    i2cInit(res.virtBase, I2C_ADDR_ELAN_TOUCHPAD);
    i2cInitializedBase = res.virtBase;
    int ack = i2cProbeAck(res.virtBase);
    if (ack == 1) {
        printf("[i2c] initialize: ELAN touchpad ACK at 0x%02x - controller ready\n",
               I2C_ADDR_ELAN_TOUCHPAD);
        /* Flush any byte left in RX FIFO by the probe READ command */
        while (i2cRead(res.virtBase, DW_IC_RXFLR) & 0xFF)
            i2cRead(res.virtBase, DW_IC_DATA_CMD);
    } else {
        printf("[i2c] initialize: no touchpad ACK (r=%d)\n", ack);
        i2cWrite(res.virtBase, DW_IC_ENABLE, 0);
    }
}

int i2cSend(uint64_t base, const uint8_t* data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t timeout = 100000;
        while (!(i2cRead(base, DW_IC_STATUS) & DW_IC_STATUS_TFNF) && timeout--) {}
        if (!timeout) { printf("[i2c] write: TX FIFO full timeout\n"); return -1; }
        i2cWrite(base, DW_IC_DATA_CMD, (uint32_t)data[i]);
    }
    uint32_t timeout = 200000;
    while (!(i2cRead(base, DW_IC_STATUS) & DW_IC_STATUS_TFE) && timeout--) {}
    uint32_t abrt = i2cRead(base, DW_IC_TX_ABRT_SOURCE);
    i2cRead(base, DW_IC_CLR_TX_ABRT);
    if (abrt & 0x1FFFF) { printf("[i2c] write: TX_ABRT=0x%x\n", abrt); return -1; }
    return 0;
}

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

int i2cWriteRead(uint64_t base,
                 const uint8_t* wbuf, uint32_t wlen,
                 uint8_t* rbuf,       uint32_t rlen) {
    for (uint32_t i = 0; i < wlen; i++) {
        uint32_t timeout = 100000;
        while (!(i2cRead(base, DW_IC_STATUS) & DW_IC_STATUS_TFNF) && timeout--) {}
        if (!timeout) return -1;
        i2cWrite(base, DW_IC_DATA_CMD, (uint32_t)wbuf[i]);
    }
    for (uint32_t i = 0; i < rlen; i++) {
        uint32_t timeout = 100000;
        while (!(i2cRead(base, DW_IC_STATUS) & DW_IC_STATUS_TFNF) && timeout--) {}
        if (!timeout) return -1;
        uint32_t cmd = (1u << 8);
        if (i == rlen - 1) cmd |= (1u << 9); /* STOP */
        i2cWrite(base, DW_IC_DATA_CMD, cmd);
    }
    for (uint32_t i = 0; i < rlen; i++) {
        uint32_t timeout = 200000;
        while (!(i2cRead(base, DW_IC_RXFLR) & 0xFF) && timeout--) {}
        if (!timeout) { printf("[i2c] writeread: RX timeout at %u\n", i); return (int)i; }
        rbuf[i] = (uint8_t)(i2cRead(base, DW_IC_DATA_CMD) & 0xFF);
    }
    uint32_t abrt = i2cRead(base, DW_IC_TX_ABRT_SOURCE);
    i2cRead(base, DW_IC_CLR_TX_ABRT);
    if (abrt & 0x1FFFF) { printf("[i2c] writeread: TX_ABRT=0x%x\n", abrt); return -1; }
    return (int)rlen;
}