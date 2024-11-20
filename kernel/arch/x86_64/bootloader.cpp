#include <hcf.hpp>

#include <bootloader.h>
#include <limine.h>

#include <stdio.h>


// Set the base revision to the recommended latest base revision 2
__attribute__((used, section(".requests")))
static volatile LIMINE_BASE_REVISION(2);

// The Limine requests it is important that
// the compiler does not optimise them away, so, they should
// be made volatile and marked as used with the "used" attribute
__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// define the start and end markers for the Limine requests
__attribute__((used, section(".requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

__attribute__((used, section(".requests")))
static volatile struct limine_paging_mode_request liminePagingreq = {
    .id = LIMINE_PAGING_MODE_REQUEST,
    .revision = 0,
    .mode = LIMINE_PAGING_MODE_X86_64_4LVL};

__attribute__((used, section(".requests")))
static volatile struct limine_kernel_address_request limineKrnreq = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0};

__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request limineHHDMreq = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0};

__attribute__((used, section(".requests")))
static volatile struct limine_memmap_request limineMMreq = {
    .id = LIMINE_MEMMAP_REQUEST, .revision = 0};

__attribute__((used))
Bootloader bootloader;
  

void initialiseBootloaderParser() {

  if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        Halt();
    }

  // Paging mode
  struct limine_paging_mode_response *liminePagingres =liminePagingreq.response;
  if (liminePagingres->mode != LIMINE_PAGING_MODE_X86_64_4LVL) {
    printf("[paging] We explicitly asked for level 4 paging!\n");
    Halt();
  }

  // HHDM (randomized kernel positionings)
  struct limine_hhdm_response *limineHHDMres = limineHHDMreq.response;
  bootloader.hhdmOffset = limineHHDMres->offset;

  // Kernel address
  struct limine_kernel_address_response *limineKrnres = limineKrnreq.response;
  bootloader.kernelVirtBase = limineKrnres->virtual_base;
  bootloader.kernelPhysBase = limineKrnres->physical_base;

  // Memory map
  struct limine_memmap_response *mm_response = limineMMreq.response;
  bootloader.mmEntries = mm_response->entries;
  bootloader.mmEntryCnt = mm_response->entry_count;

  // Total memory
  bootloader.mmTotal = 0;
  for (int i = 0; i < mm_response->entry_count; i++) {
    struct limine_memmap_entry *entry = mm_response->entries[i];
    // entry->type != LIMINE_MEMMAP_FRAMEBUFFER &&
    if (entry->type != LIMINE_MEMMAP_RESERVED)
      bootloader.mmTotal += entry->length;
  }

    // is their a framebuffer
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        Halt();
    }

    // Fetch the first framebuffer
    bootloader.framebuffer = framebuffer_request.response->framebuffers[0];


  bootloader.first_entry_base = bootloader.mmEntries[0]->base;
}