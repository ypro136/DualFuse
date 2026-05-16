#include <types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdt.h>
#include <apic.h>

#include <liballoc.h>
#include <hcf.hpp>


static GDTEntries gdt;
static GDTPtr     gdtr;
static TSSPtr     tss;

TSSPtr *tssPtr = &tss;

TSSPtr* per_core_tss[256] = {0};

void gdt_update_tss_rsp0(uint64_t kernel_stack_top) {
    tssPtr->rsp0 = kernel_stack_top;
}

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
  encode_gdt_entry(0, 0, 0, 0);
  encode_gdt_entry(1, 0xffff, 0b10011010, 0b00000000);
  encode_gdt_entry(2, 0xffff, 0b10010010, 0b00000000);
  encode_gdt_entry(3, 0xffff, 0b10011010, 0b11001111);
  encode_gdt_entry(4, 0xffff, 0b10010010, 0b11001111);
  encode_gdt_entry(5, 0, 0b10011010, 0b00100000);
  encode_gdt_entry(6, 0, 0b10010010, 0);

  gdt.descriptors[7] = (GDTEntry){0};
  gdt.descriptors[8] = (GDTEntry){0};

  encode_gdt_entry(10, 0, 0b11111010, 0b00100000);
  encode_gdt_entry(9, 0, 0b11110010, 0);

  encode_tss_entry(104, 0b10001001);

  gdtr.limit = sizeof(GDTEntries) - 1;
  gdtr.base = (uint64_t)&gdt;

  gdt_reload();

  memset(&tss, 0, sizeof(TSSPtr));
  gdt_load_tss(&tss);

  per_core_tss[apicGetBspLapicId()] = &tss;

  printf("gdt initialized.\n");

  return 0;
}

void smp_ap_initialize_gdt_and_tss(uint64_t kernel_stack_top) {
    GDTEntries* per_ap_gdt = (GDTEntries*)malloc(sizeof(GDTEntries));
    if (!per_ap_gdt) {
        printf("[smp] ERROR: failed to allocate per-AP GDT\n");
        Halt();
    }
    memcpy(per_ap_gdt, &gdt, sizeof(GDTEntries));   // copy BSP’s descriptors

    TSSPtr* per_ap_tss = (TSSPtr*)calloc(1, sizeof(TSSPtr));
    if (!per_ap_tss) {
        printf("[smp] ERROR: failed to allocate per-AP TSS\n");
        Halt();
    }
    per_ap_tss->rsp0 = kernel_stack_top;            // the only non‑zero field we need

    uint32_t my_lapic_id;
    asm volatile("mov $1, %%eax; cpuid; shrl $24, %%ebx" : "=b"(my_lapic_id));
    per_core_tss[my_lapic_id] = per_ap_tss;

    // Point the TSS descriptor inside the new GDT to the new TSS
    uint64_t tss_addr = (uint64_t)per_ap_tss;
    per_ap_gdt->tss.length        = sizeof(TSSPtr) - 1;
    per_ap_gdt->tss.base_low      = (uint16_t)(tss_addr & 0xFFFF);
    per_ap_gdt->tss.base_mid      = (uint8_t)((tss_addr >> 16) & 0xFF);
    per_ap_gdt->tss.flags1        = 0x89;   // present, 64‑bit TSS available
    per_ap_gdt->tss.flags2        = 0;
    per_ap_gdt->tss.base_high     = (uint8_t)((tss_addr >> 24) & 0xFF);
    per_ap_gdt->tss.base_upper32  = (uint32_t)(tss_addr >> 32);
    per_ap_gdt->tss.reserved      = 0;

    GDTPtr gdt_ptr;
    gdt_ptr.limit = sizeof(GDTEntries) - 1;
    gdt_ptr.base  = (uint64_t)per_ap_gdt;

    // Load the new GDT
    asm volatile("lgdt %0" :: "m"(gdt_ptr) : "memory");

    // Reload segment registers using the new GDT’s 64‑bit code/data selectors
    asm volatile(
        "pushq $0x28\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "movl $0x30, %%eax\n\t"
        "movl %%eax, %%ds\n\t"
        "movl %%eax, %%es\n\t"
        "movl %%eax, %%fs\n\t"
        "movl %%eax, %%gs\n\t"
        "movl %%eax, %%ss\n\t"
        ::: "rax", "memory");

    // Load the task register with the TSS selector (same 0x58, now pointing to our new TSS)
    asm volatile("ltr %%ax" :: "a"(0x58) : "memory");
}