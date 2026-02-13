#ifndef MINIMAL_ACPI_H
#define MINIMAL_ACPI_H

#include <types.h>

/* ACPI Table Header */
typedef struct AcpiTableHeader {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oem_id[6];
  char oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
}  __attribute__ ((packed)) AcpiTableHeader;

/* RSDP (Root System Description Pointer) */
typedef struct {
  char signature[8];
  uint8_t checksum;
  char oem_id[6];
  uint8_t revision;
  uint32_t rsdt_address;
  uint32_t length;
  uint64_t xsdt_address;
  uint8_t extended_checksum;
  uint8_t reserved[3];
}  __attribute__ ((packed)) AcpiRsdp;

/* MADT (Multiple APIC Description Table) */
typedef struct {
  AcpiTableHeader hdr;
  uint32_t lapic_address;
  uint32_t flags;
}  __attribute__ ((packed)) AcpiMadt;

/* ACPI Entry Header */
typedef struct {
  uint8_t type;
  uint8_t length;
}  __attribute__ ((packed)) AcpiEntryHdr;

/* MADT Entry Types */
#define ACPI_MADT_ENTRY_TYPE_LAPIC 0
#define ACPI_MADT_ENTRY_TYPE_IOAPIC 1
#define ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE 2
#define ACPI_MADT_ENTRY_TYPE_NMI_SOURCE 3
#define ACPI_MADT_ENTRY_TYPE_LAPIC_NMI 4
#define ACPI_MADT_ENTRY_TYPE_LAPIC_ADDRESS_OVERRIDE 5

/* MADT Entry: Local APIC */
typedef struct {
  AcpiEntryHdr hdr;
  uint8_t processor_uid;
  uint8_t lapic_id;
  uint32_t flags;
}  __attribute__ ((packed)) AcpiMadtLapic;

/*  __attribute__ ((packed)) MADT Entry: I/O APIC */
typedef struct {
  AcpiEntryHdr hdr;
  uint8_t id;
  uint8_t reserved;
  uint32_t address;
  uint32_t gsi_base;
} AcpiMadtIoapic;

/* MADT Entry: Interrupt Source Override */
typedef struct {
  AcpiEntryHdr hdr;
  uint8_t bus;
  uint8_t source;
  uint32_t gsi;
  uint16_t flags;
}  __attribute__ ((packed)) AcpiMadtInterruptSourceOverride;

/* MADT Entry: Local APIC Address Override */
typedef struct {
  AcpiEntryHdr hdr;
  uint8_t reserved[2];
  uint64_t lapic_address;
}  __attribute__ ((packed)) AcpiMadtLapicAddressOverride;

/* Functions */
void acpiInit();
AcpiMadt* acpiGetMadt();
void* acpiGetTableBySignature(const char* signature);

#endif
