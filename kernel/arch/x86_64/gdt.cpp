#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


 
#include <kernel/gdt.h>


static GDTEntries gdt;
static GDTPtr     gdtr;
static TSSPtr     tss;

TSSPtr *tssPtr = &tss;

void gdt_load_tss(TSSPtr *tss) {
  size_t addr = (size_t)tss;

  gdt.tss.base_low = (uint16_t)addr;
  gdt.tss.base_mid = (uint8_t)(addr >> 16);
  gdt.tss.flags1 = 0b10001001;
  gdt.tss.flags2 = 0;
  gdt.tss.base_high = (uint8_t)(addr >> 24);
  gdt.tss.base_upper32 = (uint32_t)(addr >> 32);
  gdt.tss.reserved = 0;

  asm volatile("ltr %0" : : "rm"((uint16_t)0x58) : "memory");
}

void gdt_reload() {
  asm volatile("lgdt %0\n\t"
               "push $0x28\n\t"
               "lea 1f(%%rip), %%rax\n\t"
               "push %%rax\n\t"
               "lretq\n\t"
               "1:\n\t"
               "mov $0x30, %%eax\n\t"
               "mov %%eax, %%ds\n\t"
               "mov %%eax, %%es\n\t"
               "mov %%eax, %%fs\n\t"
               "mov %%eax, %%gs\n\t"
               "mov %%eax, %%ss\n\t"
               :
               : "m"(gdtr)
               : "rax", "memory");
}

void encode_gdt_entry(uint32_t index, uint16_t limit, uint8_t access, uint8_t granularity)
{
  gdt.descriptors[index].limit = limit;
  gdt.descriptors[index].base_low = 0;
  gdt.descriptors[index].base_mid = 0;
  gdt.descriptors[index].access = access;
  gdt.descriptors[index].granularity = granularity;
  gdt.descriptors[index].base_high = 0;
}

void encode_tss_entry(uint16_t length, uint8_t flags)
{
    gdt.tss.length = length;
    gdt.tss.base_low = 0;
    gdt.tss.base_mid = 0;
    gdt.tss.flags1 = flags;
    gdt.tss.flags2 = 0;
    gdt.tss.base_high = 0;
    gdt.tss.base_upper32 = 0;
    gdt.tss.reserved = 0;
}

int gdt_initialize() {
  // Null descriptor. (0)
  encode_gdt_entry(0, 0, 0, 0);

  // Kernel code 16. (8)
  encode_gdt_entry(1, 0xffff, 0b10011010, 0b00000000);

  // Kernel data 16. (16)
  encode_gdt_entry(2, 0xffff, 0b10010010, 0b00000000);

  // Kernel code 32. (24)
  encode_gdt_entry(3, 0xffff, 0b10011010, 0b11001111);

  // Kernel data 32. (32)
  encode_gdt_entry(4, 0xffff, 0b10010010, 0b11001111);

  // Kernel code 64. (40)
  encode_gdt_entry(5, 0, 0b10011010, 0b00100000);

  // Kernel data 64. (48)
  encode_gdt_entry(6, 0, 0b10010010, 0);

  // SYSENTER
  gdt.descriptors[7] = (GDTEntry){0}; // (56)
  gdt.descriptors[8] = (GDTEntry){0}; // (64)

  // User code 64. (72)
  encode_gdt_entry(10, 0, 0b11111010, 0b00100000);

  // User data 64. (80)
  encode_gdt_entry(9, 0, 0b11110010, 0);

  // TSS. (88)
  encode_tss_entry(104, 0b10001001);

  gdtr.limit = sizeof(GDTEntries) - 1;
  gdtr.base = (uint64_t)&gdt;

  gdt_reload();

  memset(&tss, 0, sizeof(TSSPtr));
  gdt_load_tss(&tss);

  return 0;
}