#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


 
#include <gdt.h>

static const uint16_t number_of_gdt_entries = 6;

static struct GDT_entry gdt_entries[number_of_gdt_entries];
static struct GDT_Pointer gdt_Pointer;
static struct tss_entry tss_entry;




// 32 bit 
// Offset 	                   Use 	                             Content
// 0x0000 	                   Null Descriptor 	                 Base = 0
//                                                               Limit = 0x00000000
//                                                               Access Byte = 0x00
//                                                               Flags = 0x0

// 0x0008 	                   Kernel Mode Code Segment 	       Base = 0
//                                                               Limit = 0xFFFFF
//                                                               Access Byte = 0x9A
//                                                               Flags = 0xC

// 0x0010 	                   Kernel Mode Data Segment	         Base = 0
//                                                               Limit = 0xFFFFF 
//                                                               Access Byte = 0x92
//                                                               Flags = 0xC

// 0x0018 	                   User Mode Code Segment 	         Base = 0
//                                                               Limit = 0xFFFFF
//                                                               Access Byte = 0xFA
//                                                               Flags = 0xC

// 0x0020 	                   User Mode Data Segment 	         Base = 0
//                                                               Limit = 0xFFFFF
//                                                               Access Byte = 0xF2
//                                                               Flags = 0xC


/**
 * Encodes a global descriptor table (GDT) entry with the provided parameters.
 *
 * This function is responsible for setting up the individual fields of a GDT entry
 * based on the provided base address, limit, access byte, and granularity. The
 * encoded GDT entry is stored in the `gdt_entries` array at the specified index.
 *
 * @param number The index of the GDT entry to encode.
 * @param base The base address of the memory segment.
 * @param limit The limit (size) of the memory segment.
 * @param access_byte The access byte that defines the segment's privileges and type.
 * @param granularity The granularity of the segment limit (byte or page).
 */
void encode_Gdt(uint32_t number, uint32_t base, uint32_t limit, uint8_t access_byte, uint8_t granularity)
{
  // Encode the base
  gdt_entries[number].base_low = (base & 0xFFFF);
  gdt_entries[number].base_middle = (base >> 16) & 0xFF;
  gdt_entries[number].base_high = (base >> 24) & 0xFF;

  // Encode the limit
  gdt_entries[number].limit = (limit & 0xFFFF);

  // Encode the flags
  gdt_entries[number].flags_byte = (limit >> 16) & 0x0F;
  gdt_entries[number].flags_byte |= (granularity & 0xF0);

  // Encode the access byte
  gdt_entries[number].access_byte = access_byte;
}

void encode_TSS(uint32_t number, uint16_t ss0, uint32_t esp0)
{
  uint32_t base = (uint32_t) &tss_entry;
  uint32_t limit = base + sizeof(tss_entry);

  encode_Gdt(number, base, limit, 0xE9, 0x00);

  memset(&tss_entry, 0, sizeof(tss_entry));

  tss_entry.ss0 = ss0;
  tss_entry.esp0 = esp0;

  tss_entry.cs = 0x08 | 0x3;

  tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x10 | 0x3;
}


/**
 * Initializes the global descriptor table (GDT) and updates the segment registers.
 * This function is responsible for setting up the memory management system during
 * system initialization.
 *
 * The GDT is a low-level data structure that defines the memory segments available
 * to the system. This function encodes the GDT entries and then loads the GDT
 * using the `setGdt()` function.
 *
 * @return 0 on success, non-zero on failure
 */
int gdt_initialize()
{

  gdt_Pointer.limit = (sizeof(struct GDT_entry) * number_of_gdt_entries) - 1;

  gdt_Pointer.base = (uint32_t)&gdt_entries;

  encode_Gdt(0, 0, 0, 0, 0);                // null descriptor segmant
  encode_Gdt(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // kernel code segment
  encode_Gdt(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // kernel data segment
  encode_Gdt(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // user code segment
  encode_Gdt(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // user data segment

  encode_TSS(5, 0x10, 0x0);


  setGdt(gdt_Pointer);
  setTSS();

  return 0;
}

/**
 * Loads the global descriptor table (GDT) and updates the segment registers.
 * This is a low-level operation that is typically performed during system
 * initialization to set up the memory management system.
 */
void setGdt(GDT_Pointer gdt_Pointer)
{

  /**
   * Loads the global descriptor table (GDT) and updates the segment registers.
   * This is a low-level operation that is typically performed during system
   * initialization to set up the memory management system.
   */
  // Inline assembly
  asm volatile(
      "lgdt %0\n"
      "mov $0x10, %%eax\n"
      "mov %%eax, %%ds\n"
      "mov %%eax, %%es\n"
      "mov %%eax, %%fs\n"
      "mov %%eax, %%gs\n"
      "mov %%eax, %%ss\n"
      "ljmp $0x08, $reload_cs_%=\n"
      "reload_cs_%=:\n"
      : /* no outputs */
      : "m"(gdt_Pointer)
      : "eax");
}

void setTSS(void)
{
  // Inline assembly
  asm volatile(
      "mov $0x2B, %%ax\n"
      "ltr %%ax\n"
      : /* no outputs */
      : /* no inputs */
      : "eax");
}




