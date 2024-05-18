#ifndef _KERNEL_GDT_H
#define _KERNEL_GDT_H


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Represents a single entry in the Global Descriptor Table (GDT).
 * The GDT is a data structure used by the CPU to define memory segments
 * and their access privileges.
 *
 * Each GDTEntry defines a single memory segment, including its base address,
 * limit, and access permissions.
 */
typedef struct GDT_entry
{
  uint16_t limit;      /**< The size limit of the segment in bytes (0 to 65535). */
  uint16_t base_low;   /**< The lower 16 bits of the segment base address. */
  uint8_t base_middle; /**< The middle 8 bits of the segment base address. */
  uint8_t access_byte; /**< The access byte, defining the segment's permissions. */
  uint8_t flags_byte;  /**< The flags byte, defining additional segment properties. */
  uint8_t base_high;   /**< The upper 8 bits of the segment base address. */
} __attribute__((packed)) GDTEntry;


typedef struct GDT_Pointer {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed)) GDTPointer;


void setGdt(GDT_Pointer gdt_Pointer);
void encode_Gdt(uint32_t number, uint32_t base, uint32_t limit, uint8_t access_byte, uint8_t granularity); 
int gdt_initialize();


#endif

