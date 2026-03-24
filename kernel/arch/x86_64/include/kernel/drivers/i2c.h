#pragma once

#include <types.h>

/*
 * i2c.h - Intel DesignWare I2C host controller driver
 *
 * Targets the Intel Alder Lake PCH I2C controller (PCI 00:15.x) which hosts
 * the ELAN/Synaptics HID-over-I2C touchpad on the MSI Modern 14.
 *
 * Reference: Linux drivers/i2c/busses/i2c-designware-*.c, U-Boot dw_i2c.c,
 *            coreboot soc/intel/common/block/i2c/, QEMU hw/i2c/i2c_dw.c
 *
 * All register offsets are from the MMIO BAR0 base.
 * Access pattern: 32-bit MMIO reads/writes via volatile uint32_t*.
 */

/* -------------------------------------------------------------------------
 * DesignWare I2C register offsets
 * ------------------------------------------------------------------------- */

#define DW_IC_CON               0x00    /* Control register                   */
#define DW_IC_TAR               0x04    /* Target (slave) address             */
#define DW_IC_SAR               0x08    /* Own slave address (unused in master)*/
#define DW_IC_DATA_CMD          0x10    /* TX/RX FIFO - write=send, read=recv
                                         * bit[8]=1: issue a READ command     */
#define DW_IC_SS_SCL_HCNT       0x14    /* Standard-speed clock HIGH count    */
#define DW_IC_SS_SCL_LCNT       0x18    /* Standard-speed clock LOW count     */
#define DW_IC_FS_SCL_HCNT       0x1C    /* Fast-speed clock HIGH count        */
#define DW_IC_FS_SCL_LCNT       0x20    /* Fast-speed clock LOW count         */
#define DW_IC_INTR_STAT         0x2C    /* Interrupt status (read-only)       */
#define DW_IC_INTR_MASK         0x30    /* Interrupt mask                     */
#define DW_IC_CLR_INTR          0x40    /* Clear all interrupts (read-to-clr) */
#define DW_IC_CLR_TX_ABRT       0x54    /* Clear TX_ABRT (must read after NACK)*/
#define DW_IC_ENABLE            0x6C    /* Enable/disable - 1=enable, 0=disable*/
#define DW_IC_STATUS            0x70    /* Bus activity + FIFO status         */
#define DW_IC_TXFLR             0x74    /* TX FIFO fill level                 */
#define DW_IC_RXFLR             0x78    /* RX FIFO fill level                 */
#define DW_IC_TX_ABRT_SOURCE    0x80    /* TX abort reason flags              */
#define DW_IC_COMP_PARAM_1      0xF4    /* FIFO depth encoded here            */
#define DW_IC_COMP_TYPE         0xFC    /* Should read 0x44570140 for genuine
                                         * Synopsys DesignWare IP              */

/* -------------------------------------------------------------------------
 * DW_IC_CON bit flags
 * ------------------------------------------------------------------------- */

#define DW_IC_CON_MASTER                0x01    /* Enable master mode         */
#define DW_IC_CON_SPEED_STD             0x02    /* 100 kHz standard speed     */
#define DW_IC_CON_SPEED_FAST            0x04    /* 400 kHz fast mode          */
#define DW_IC_CON_SPEED_MASK            0x06    /* Speed field mask           */
#define DW_IC_CON_10BITADDR_MASTER      0x10    /* Use 10-bit addressing      */
#define DW_IC_CON_RESTART_EN            0x20    /* Allow RESTART condition    */
#define DW_IC_CON_SLAVE_DISABLE         0x40    /* Disable slave mode         */

/* Canonical master-only fast-mode init value:
 *   MASTER | SPEED_FAST | RESTART_EN | SLAVE_DISABLE = 0x65             */
#define DW_IC_CON_MASTER_FAST_MODE  \
    (DW_IC_CON_MASTER | DW_IC_CON_SPEED_FAST | \
     DW_IC_CON_RESTART_EN | DW_IC_CON_SLAVE_DISABLE)

/* -------------------------------------------------------------------------
 * DW_IC_STATUS bit flags
 * ------------------------------------------------------------------------- */

#define DW_IC_STATUS_ACTIVITY           0x01    /* Bus activity               */
#define DW_IC_STATUS_TFNF               0x02    /* TX FIFO not full           */
#define DW_IC_STATUS_TFE                0x04    /* TX FIFO empty (idle)       */
#define DW_IC_STATUS_RFNE               0x08    /* RX FIFO not empty (data!)  */

/* -------------------------------------------------------------------------
 * DW_IC_TX_ABRT_SOURCE significant bits
 * ------------------------------------------------------------------------- */

#define DW_IC_TX_ABRT_7B_ADDR_NOACK    (1 << 0)  /* No ACK on 7-bit addr     */
#define DW_IC_TX_ABRT_10ADDR1_NOACK    (1 << 1)  /* No ACK on 10-bit addr[0] */
#define DW_IC_TX_ABRT_10ADDR2_NOACK    (1 << 2)  /* No ACK on 10-bit addr[1] */
#define DW_IC_TX_ABRT_TXDATA_NOACK     (1 << 3)  /* No ACK on data byte      */

/* Any NACK on the address phase - device not present */
#define DW_IC_TX_ABRT_ADDR_NOACK  \
    (DW_IC_TX_ABRT_7B_ADDR_NOACK | DW_IC_TX_ABRT_10ADDR1_NOACK | \
     DW_IC_TX_ABRT_10ADDR2_NOACK)

/* -------------------------------------------------------------------------
 * Genuine DesignWare component type identifier
 * ------------------------------------------------------------------------- */

#define DW_IC_COMP_TYPE_VALUE           0x44570140u

/* -------------------------------------------------------------------------
 * Intel PCH I2C PCI identifiers - Alder Lake (MSI Modern 14)
 *
 * Bus 0, Device 0x15 (21), Functions 0–3.
 * All four controllers share the same vendor ID; device IDs are sequential.
 *
 *   0x51E8  I2C0  PCI 00:15.0  ← most likely touchpad host
 *   0x51E9  I2C1  PCI 00:15.1
 *   0x51EA  I2C2  PCI 00:15.2
 *   0x51EB  I2C3  PCI 00:15.3
 * ------------------------------------------------------------------------- */

#define I2C_INTEL_VENDOR_ID         0x8086u

#define I2C_ADL_PCH_DEV_ID_I2C0    0x51E8u
#define I2C_ADL_PCH_DEV_ID_I2C1    0x51E9u
#define I2C_ADL_PCH_DEV_ID_I2C2    0x51EAu
#define I2C_ADL_PCH_DEV_ID_I2C3    0x51EBu

/* PCI slot for all Intel PCH I2C controllers on Alder Lake */
#define I2C_PCH_PCI_BUS             0
#define I2C_PCH_PCI_DEVICE          0x15    /* slots 0x15.0 – 0x15.3 */

/* -------------------------------------------------------------------------
 * Known touchpad I2C addresses (7-bit)
 * ------------------------------------------------------------------------- */

#define I2C_ADDR_ELAN_TOUCHPAD      0x15
#define I2C_ADDR_SYNAPTICS_TOUCHPAD 0x2C

/* -------------------------------------------------------------------------
 * Clock timing - Alder Lake PCH I2C reference clock is 100 MHz
 *
 * Formula (from DesignWare databook):
 *   hcnt = ceil(clk_mhz * t_high_ns / 1000) + 1
 *   lcnt = ceil(clk_mhz * t_low_ns  / 1000) + 1
 *
 * Fast mode (400 kHz): t_high = 600 ns, t_low = 1300 ns
 * Standard mode (100 kHz): t_high = 4000 ns, t_low = 4700 ns
 *
 * At 100 MHz input clock (= 10 ns / tick):
 *   FS_HIGH = ceil(100 * 600  / 1000) + 1 = 61
 *   FS_LOW  = ceil(100 * 1300 / 1000) + 1 = 131
 *   SS_HIGH = ceil(100 * 4000 / 1000) + 1 = 401
 *   SS_LOW  = ceil(100 * 4700 / 1000) + 1 = 471
 * ------------------------------------------------------------------------- */

#define DW_I2C_CLK_MHZ              100

#define DW_I2C_FS_SCL_HCNT_100MHZ  61
#define DW_I2C_FS_SCL_LCNT_100MHZ  131
#define DW_I2C_SS_SCL_HCNT_100MHZ  401
#define DW_I2C_SS_SCL_LCNT_100MHZ  471

/* -------------------------------------------------------------------------
 * Result info struct returned by i2cFindBase()
 * ------------------------------------------------------------------------- */

struct I2cBaseResult {
    uint64_t virtBase;      /* MMIO virtual address (hhdmOffset applied), 0 = not found */
    uint8_t  pciFunction;   /* PCI function number found (0–3), or 0xFF if via DSDT     */
    bool     foundViaPci;   /* true = PCI path, false = DSDT fallback                   */
};

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/* Discover the DesignWare I2C MMIO base for the first Alder Lake PCH I2C
 * controller that matches a known device ID.
 *
 * Strategy:
 *   1. PCI scan - bus 0, device 0x15, functions 0–3.  If vendor==0x8086 and
 *      device ID is in the Alder Lake set, read BAR0 as the 64-bit physical
 *      base and apply hhdmOffset.
 *   2. DSDT byte scan fallback - scan raw DSDT bytes for known _HID strings
 *      (INTC10B / INTC10C / INTC10D / INT33C2 / INT33C3), then locate the
 *      nearest Memory32Fixed resource descriptor to extract the MMIO base.
 *
 * Returns an I2cBaseResult; check virtBase != 0 for success.
 */
/* Transition all ADL PCH I2C PCI functions on 00:15.x from D3 to D0.
 * Must be called before any MMIO access - firmware leaves LPSS in D3.  */
void i2cPowerOnAll();
void i2cDeassertReset(uint64_t virtBase);
void i2cDumpDsdtContext();
I2cBaseResult i2cScanKnownBases();

/* Session 2 - init and ACK probe */
void i2cInit(uint64_t virtBase, uint8_t targetAddr);
int  i2cProbeAck(uint64_t virtBase); /* 1=ACK, 0=NACK, -1=timeout */
int  i2cSend(uint64_t base, const uint8_t* data, uint32_t len);
int  i2cWriteRead(uint64_t base, const uint8_t* wbuf, uint32_t wlen, uint8_t* rbuf, uint32_t rlen);
int  i2cRecv(uint64_t base, uint8_t* buf,        uint32_t len);
uint64_t i2cGetBase();
void i2cInitialize();

I2cBaseResult i2cFindBase();

/* MMIO helpers - thin wrappers around volatile 32-bit reads/writes.
 * base must already have hhdmOffset applied.                              */
static inline uint32_t i2cRead(uint64_t base, uint32_t reg) {
    return *((volatile uint32_t *)(base + reg));
}

static inline void i2cWrite(uint64_t base, uint32_t reg, uint32_t val) {
    *((volatile uint32_t *)(base + reg)) = val;
}