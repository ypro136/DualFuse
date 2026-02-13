#include <minimal_acpi.h>
#include <bootloader.h>
#include <system.h>
#include <liballoc.h>

static AcpiMadt* cachedMadt = nullptr;

/* Helper function: validate ACPI checksum */
static bool acpiValidateChecksum(void* ptr, uint32_t length) {
  uint8_t* data = (uint8_t*)ptr;
  uint8_t checksum = 0;
  
  for (uint32_t i = 0; i < length; i++) {
    checksum += data[i];
  }
  
  return checksum == 0;
}

/* Find RSDP in memory (provided by bootloader via HHDM) */
static AcpiRsdp* acpiFindRsdp() {
  AcpiRsdp* rsdp = nullptr;
  
  // First, try to use RSDP address from Limine bootloader
  if (bootloader.rsdp) {
    uint64_t rsdpVirt = bootloader.hhdmOffset + bootloader.rsdp;
    rsdp = (AcpiRsdp*)rsdpVirt;
    
    printf("[acpi] Trying RSDP from bootloader at physical %lx (virtual %lx)\n", 
           bootloader.rsdp, rsdpVirt);

    printf("[acpi] RSDP from bootloader at physical %lx (virtual %lx) is: signature = %c, %c, %c, %c\n", 
           bootloader.rsdp, rsdpVirt, rsdp->signature[0], rsdp->signature[1], rsdp->signature[2], rsdp->signature[3]);
    
    // Verify signature
    if (rsdp->signature[0] == 'R' && rsdp->signature[1] == 'S' &&
        rsdp->signature[2] == 'D') {
      
      // Validate checksum
      if (rsdp->revision >= 2) {
        if (acpiValidateChecksum(rsdp, rsdp->length)) {
          printf("[acpi] Found valid RSDP 2.0+ from bootloader at %lx\n", bootloader.rsdp);
          return rsdp;
        }
      } else {
        if (acpiValidateChecksum(rsdp, 20)) {
          printf("[acpi] Found valid RSDP 1.0 from bootloader at %lx\n", bootloader.rsdp);
          return rsdp;
        }
      }
      printf("[acpi] Bootloader RSDP signature valid but checksum invalid\n");
    } else {
      printf("[acpi] Bootloader RSDP signature mismatch\n");
    }
  } else {
    printf("[acpi] Bootloader did not provide RSDP address\n");
  }
  
  // Fallback: Search BIOS memory area (0xE0000 - 0xFFFFF)
  printf("[acpi] Falling back to BIOS memory search (0xE0000-0xFFFFF)...\n");
  uint64_t searchStart = bootloader.hhdmOffset + 0xE0000;
  uint64_t searchEnd = bootloader.hhdmOffset + 0x100000;
  
  int sigCount = 0;
  for (uint64_t addr = searchStart; addr < searchEnd; addr += 16) {
    rsdp = (AcpiRsdp*)addr;
    
    // Check signature
    if (rsdp->signature[0] != 'R' || rsdp->signature[1] != 'S' ||
        rsdp->signature[2] != 'D' || rsdp->signature[3] != 'P') {
      continue;
    }
    
    sigCount++;
    printf("[acpi] Found RSDP signature at physical %lx (attempt %d, revision %d)\n", 
           addr - bootloader.hhdmOffset, sigCount, rsdp->revision);
    
    // For ACPI 2.0+, validate extended checksum
    if (rsdp->revision >= 2) {
      if (acpiValidateChecksum(rsdp, rsdp->length)) {
        printf("[acpi] Found valid RSDP 2.0+ at %lx\n", addr - bootloader.hhdmOffset);
        return rsdp;
      } else {
        printf("[acpi] RSDP 2.0+ checksum invalid (length: %d)\n", rsdp->length);
      }
    } else {
      // For ACPI 1.0, validate first 20 bytes
      if (acpiValidateChecksum(rsdp, 20)) {
        printf("[acpi] Found valid RSDP 1.0 at %lx\n", addr - bootloader.hhdmOffset);
        return rsdp;
      } else {
        printf("[acpi] RSDP 1.0 checksum invalid\n");
      }
    }
  }
  
  printf("[acpi] BIOS search complete: found %d RSDP signatures, none valid\n", sigCount);
  printf("[acpi] Warning: Could not find valid RSDP anywhere!\n");
  return nullptr;
}

/* Find RSDT or XSDT */
static void* acpiFindRootTable() {
  AcpiRsdp* rsdp = acpiFindRsdp();
  if (!rsdp) {
    printf("[acpi] Warning: Couldn't find RSDP!\n");
    return nullptr;
  }
  
  // Prefer XSDT (64-bit) if available and revision >= 2
  if (rsdp->revision >= 2 && rsdp->xsdt_address) {
    void* xsdt = (void*)(bootloader.hhdmOffset + rsdp->xsdt_address);
    AcpiTableHeader* xsdtHdr = (AcpiTableHeader*)xsdt;
    
    // Validate length before accessing
    if (xsdtHdr->length >= sizeof(AcpiTableHeader) && 
        xsdtHdr->length < 0x100000 &&  // Sanity check: table shouldn't be > 1MB
        acpiValidateChecksum(xsdt, xsdtHdr->length)) {
      printf("[acpi] Using XSDT at %lx\n", rsdp->xsdt_address);
      return xsdt;
    }
  }
  
  // Fall back to RSDT (32-bit)
  if (rsdp->rsdt_address) {
    void* rsdt = (void*)(bootloader.hhdmOffset + rsdp->rsdt_address);
    AcpiTableHeader* rsdtHdr = (AcpiTableHeader*)rsdt;
    
    // Validate length before accessing
    if (rsdtHdr->length >= sizeof(AcpiTableHeader) && 
        rsdtHdr->length < 0x100000 &&  // Sanity check
        acpiValidateChecksum(rsdt, rsdtHdr->length)) {
      printf("[acpi] Using RSDT at %lx\n", rsdp->rsdt_address);
      return rsdt;
    }
  }
  
  printf("[acpi] Warning: Couldn't find valid RSDT or XSDT!\n");
  return nullptr;
}

/* Find a table by signature in RSDT/XSDT */
void* acpiGetTableBySignature(const char* signature) {
  void* root = acpiFindRootTable();
  if (!root) return nullptr;
  
  AcpiTableHeader* rootHdr = (AcpiTableHeader*)root;
  
  // Sanity check root table header
  if (rootHdr->length < sizeof(AcpiTableHeader) || rootHdr->length > 0x100000) {
    printf("[acpi] Invalid root table length: %u\n", rootHdr->length);
    return nullptr;
  }
  
  // Determine if RSDT or XSDT based on signature
  bool isXsdt = (rootHdr->signature[0] == 'X');
  
  uint32_t entrySize = isXsdt ? 8 : 4;
  uint32_t numEntries = (rootHdr->length - sizeof(AcpiTableHeader)) / entrySize;
  
  if (numEntries > 4096) {  // Sanity check: shouldn't have more than 4K entries
    printf("[acpi] Suspiciously large number of RSDT/XSDT entries: %u\n", numEntries);
    return nullptr;
  }
  
  uint8_t* entries = (uint8_t*)(rootHdr + 1);
  
  for (uint32_t i = 0; i < numEntries; i++) {
    uint64_t tableAddr;
    
    if (isXsdt) {
      tableAddr = *(uint64_t*)(entries + i * 8);
    } else {
      tableAddr = *(uint32_t*)(entries + i * 4);
    }
    
    if (tableAddr == 0) continue;
    
    void* tablePtr = (void*)(bootloader.hhdmOffset + tableAddr);
    AcpiTableHeader* table = (AcpiTableHeader*)tablePtr;
    
    // Sanity check table header before accessing
    if (table->length < sizeof(AcpiTableHeader) || table->length > 0x100000) {
      continue;
    }
    
    // Check signature match
    if (table->signature[0] == signature[0] &&
        table->signature[1] == signature[1] &&
        table->signature[2] == signature[2] &&
        table->signature[3] == signature[3]) {
      
      // Validate checksum
      if (acpiValidateChecksum(table, table->length)) {
        printf("[acpi] Found table %c%c%c%c at %lx\n",
               signature[0], signature[1], signature[2], signature[3],
               tableAddr);
        return tablePtr;
      }
    }
  }
  
  return nullptr;
}

void acpiInit() {
  // Find and cache MADT
  cachedMadt = (AcpiMadt*)acpiGetTableBySignature("APIC");
  
  if (!cachedMadt) {
    printf("[acpi] Warning: MADT table not found\n");
    return;
  }
  
  printf("[acpi] MADT found: lapic_address=%lx\n", cachedMadt->lapic_address);
}

AcpiMadt* acpiGetMadt() {
  return cachedMadt;
}
