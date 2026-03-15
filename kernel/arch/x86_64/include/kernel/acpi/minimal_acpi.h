#ifndef MINIMAL_ACPI_H
#define MINIMAL_ACPI_H

#include <types.h>

/* ACPI Table Header */
typedef struct AcpiTableHeader {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) AcpiTableHeader;

/* RSDP (Root System Description Pointer) */
typedef struct {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed)) AcpiRsdp;

/* MADT (Multiple APIC Description Table) */
typedef struct {
    AcpiTableHeader hdr;
    uint32_t        lapic_address;
    uint32_t        flags;
} __attribute__((packed)) AcpiMadt;

/* ACPI Entry Header */
typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) AcpiEntryHdr;

/* MADT Entry Types */
#define ACPI_MADT_ENTRY_TYPE_LAPIC                      0
#define ACPI_MADT_ENTRY_TYPE_IOAPIC                     1
#define ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE  2
#define ACPI_MADT_ENTRY_TYPE_NMI_SOURCE                 3
#define ACPI_MADT_ENTRY_TYPE_LAPIC_NMI                  4
#define ACPI_MADT_ENTRY_TYPE_LAPIC_ADDRESS_OVERRIDE     5

/* MADT Entry: Local APIC */
typedef struct {
    AcpiEntryHdr hdr;
    uint8_t      processor_uid;
    uint8_t      lapic_id;
    uint32_t     flags;
} __attribute__((packed)) AcpiMadtLapic;

/* MADT Entry: I/O APIC */
typedef struct {
    AcpiEntryHdr hdr;
    uint8_t      id;
    uint8_t      reserved;
    uint32_t     address;
    uint32_t     gsi_base;
} AcpiMadtIoapic;

/* MADT Entry: Interrupt Source Override */
typedef struct {
    AcpiEntryHdr hdr;
    uint8_t      bus;
    uint8_t      source;
    uint32_t     gsi;
    uint16_t     flags;
} __attribute__((packed)) AcpiMadtInterruptSourceOverride;

/* MADT Entry: Local APIC Address Override */
typedef struct {
    AcpiEntryHdr hdr;
    uint8_t      reserved[2];
    uint64_t     lapic_address;
} __attribute__((packed)) AcpiMadtLapicAddressOverride;

/* -------------------------------------------------------------------------
 * FADT (Fixed ACPI Description Table, signature "FACP")
 *
 * Key offsets:
 *   36  DSDT           (32-bit DSDT physical address)   <- fallback
 *  140  X_DSDT         (64-bit DSDT physical address)   <- preferred
 * ------------------------------------------------------------------------- */
typedef struct {
    AcpiTableHeader hdr;            /* offset   0 - 36 bytes                  */

    uint32_t firmware_ctrl;         /* offset  36                             */
    uint32_t dsdt;                  /* offset  40 - 32-bit DSDT phys addr     */

    uint8_t  reserved0;
    uint8_t  preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_cnt;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t  pm1_evt_len;
    uint8_t  pm1_cnt_len;
    uint8_t  pm2_cnt_len;
    uint8_t  pm_tmr_len;
    uint8_t  gpe0_blk_len;
    uint8_t  gpe1_blk_len;
    uint8_t  gpe1_base;
    uint8_t  cst_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alrm;
    uint8_t  mon_alrm;
    uint8_t  century;
    uint16_t iapc_boot_arch;
    uint8_t  reserved1;
    uint32_t flags;

    uint8_t  reset_reg[12];         /* offset 116 - Generic Address Structure */
    uint8_t  reset_value;           /* offset 128                             */
    uint16_t arm_boot_arch;         /* offset 129                             */
    uint8_t  fadt_minor_version;    /* offset 131                             */

    uint64_t x_firmware_ctrl;       /* offset 132                             */
    uint64_t x_dsdt;                /* offset 140 - 64-bit DSDT phys addr     */
} __attribute__((packed)) AcpiFadt;

/* -------------------------------------------------------------------------
 * DSDT discovery result - returned by acpiGetDsdt()
 * ------------------------------------------------------------------------- */
typedef struct {
    const uint8_t* ptr;     /* Virtual pointer to DSDT (hhdmOffset applied).
                             * nullptr on failure.                             */
    uint32_t       length;  /* Total byte length from DSDT header. 0 on fail. */
} AcpiDsdtInfo;

/* -------------------------------------------------------------------------
 * Functions
 * ------------------------------------------------------------------------- */

void         acpiInit();
AcpiMadt*    acpiGetMadt();
void*        acpiGetTableBySignature(const char* signature);
AcpiDsdtInfo acpiGetDsdt();

#endif