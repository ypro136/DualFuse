#include <hcf.hpp>

#include <bootloader.h>
#include <limine.h>

#include <stdio.h>


// Set the base revision to the recommended latest base revision 2
__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(2);

// The Limine requests it is important that
// the compiler does not optimise them away, so, they should
// be made volatile and marked as used with the "used" attribute
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// define the start and end markers for the Limine requests
__attribute__((used, section(".requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile struct limine_paging_mode_request liminePagingreq = {
    .id = LIMINE_PAGING_MODE_REQUEST,
    .revision = 0,
    .mode = LIMINE_PAGING_MODE_X86_64_4LVL};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_address_request limineKrnreq = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request limineHHDMreq = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request limineMMreq = {
    .id = LIMINE_MEMMAP_REQUEST, .revision = 0};

__attribute__((used, section(".limine_requests"))) 
static volatile struct limine_rsdp_request limineRsdpReq = {
    .id = LIMINE_RSDP_REQUEST, .revision = 0};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

__attribute__((used))
Bootloader bootloader;

extern "C" void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

extern "C" void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

extern "C" void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

extern "C" int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}
  

void initialiseBootloaderParser() {
    
  if (LIMINE_BASE_REVISION_SUPPORTED && false) // WARNING: this is very wrong
    {
        printf("LIMINE_BASE_REVISION_SUPPORTED is false\n");
        Halt();
    }

  // Paging mode
  struct limine_paging_mode_response *liminePagingres = liminePagingreq.response;
  if (liminePagingres->mode != LIMINE_PAGING_MODE_X86_64_4LVL) {
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

  // Total memory — categorize carefully
    bootloader.mmTotal    = 0;
    bootloader.mmExecTotal = 0;

    for (int i = 0; i < mm_response->entry_count; i++) {
        struct limine_memmap_entry *entry = mm_response->entries[i];

        switch (entry->type) {
            case LIMINE_MEMMAP_USABLE:
                bootloader.mmTotal += entry->length;
                break;

            case LIMINE_MEMMAP_KERNEL_AND_MODULES:
                // Kernel binary and Limine module buffers live here.
                // Track separately — your memory manager must never
                // hand these pages to malloc.
                bootloader.mmExecTotal += entry->length;
                break;

            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
                // Limine's own data structures (including the module response
                // and limine_file descriptors themselves) sit here. Safe to
                // reclaim after you've copied out anything you need, but leave
                // them alone until after kernel init is complete.
                bootloader.mmTotal += entry->length;
                break;

            case LIMINE_MEMMAP_RESERVED:
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            case LIMINE_MEMMAP_ACPI_NVS:
            case LIMINE_MEMMAP_BAD_MEMORY:
            case LIMINE_MEMMAP_FRAMEBUFFER:
            default:
                // Do not count or allocate into any of these.
                break;
        }
    }

    printf("[bootloader] usable: %d MB  exec+modules: %d KB\n",
        bootloader.mmTotal    / (1024 * 1024),
        bootloader.mmExecTotal / 1024);

    bootloader.mmPhysExtent = 0;
    for (uint64_t i = 0; i < bootloader.mmEntryCnt; i++) {
        struct limine_memmap_entry* e = bootloader.mmEntries[i];
        uint64_t top = e->base + e->length;
        if (top > bootloader.mmPhysExtent)
            bootloader.mmPhysExtent = top;
    }
printf("[bootloader] physical extent: %d MB\n",
    bootloader.mmPhysExtent / (1024 * 1024));

    // is their a framebuffer
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        Halt();
    }

    // Fetch the first framebuffer
    bootloader.framebuffer = framebuffer_request.response->framebuffers[0];


  bootloader.first_entry_base = bootloader.mmEntries[0]->base;

    // RSDP
  // todo: revision >= 3 and it's not virtual!

  struct limine_rsdp_response *rsdp_response = limineRsdpReq.response;

  if (rsdp_response == NULL) 
  {
    printf("[bootloderparser] FATAL: No RSDP halt!\n");
    Halt();
    }else 
    {
        bootloader.rsdp = (size_t)rsdp_response->address - bootloader.hhdmOffset;
    }

    struct limine_module_response* mod_response = module_request.response;
    if (mod_response != nullptr) {
        bootloader.modules = mod_response;
        printf("[bootloader] %d module(s) loaded\n", mod_response->module_count);
    } else {
        bootloader.modules = nullptr;
        printf("[bootloader] no modules loaded\n");
    }

  printf("Bootloader Parser initialized.\n");
}