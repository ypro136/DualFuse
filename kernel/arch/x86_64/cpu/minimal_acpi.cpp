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
    #if defined(DEBUG_ACPI)
    printf("[acpi] Trying RSDP from bootloader at physical %lx (virtual %lx)\n", bootloader.rsdp, rsdpVirt);
    printf("[acpi] RSDP from bootloader at physical %lx (virtual %lx) is: signature = %c, %c, %c, %c\n",bootloader.rsdp, rsdpVirt, rsdp->signature[0], rsdp->signature[1], rsdp->signature[2], rsdp->signature[3]);
    #endif
    // Verify signature
    if (rsdp->signature[0] == 'R' && rsdp->signature[1] == 'S' &&
        rsdp->signature[2] == 'D') {
      
      // Validate checksum
      if (rsdp->revision >= 2) {
        if (acpiValidateChecksum(rsdp, rsdp->length)) {
          #if defined(DEBUG_ACPI)
          printf("[acpi] Found valid RSDP 2.0+ from bootloader at %lx\n", bootloader.rsdp);
          #endif
          return rsdp;
        }
      } else {
        if (acpiValidateChecksum(rsdp, 20)) {
          #if defined(DEBUG_ACPI)
          printf("[acpi] Found valid RSDP 1.0 from bootloader at %lx\n", bootloader.rsdp);
          #endif
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
    #if defined(DEBUG_ACPI)
    printf("[acpi] Found RSDP signature at physical %lx (attempt %d, revision %d)\n",addr - bootloader.hhdmOffset, sigCount, rsdp->revision);
      #endif
    
    // For ACPI 2.0+, validate extended checksum
    if (rsdp->revision >= 2) {
      if (acpiValidateChecksum(rsdp, rsdp->length)) {
        #if defined(DEBUG_ACPI)
        printf("[acpi] Found valid RSDP 2.0+ at %lx\n", addr - bootloader.hhdmOffset);
          #endif
        return rsdp;
      } else {
        printf("[acpi] RSDP 2.0+ checksum invalid (length: %d)\n", rsdp->length);
      }
    } else {
      // For ACPI 1.0, validate first 20 bytes
      if (acpiValidateChecksum(rsdp, 20)) {
        #if defined(DEBUG_ACPI)
        printf("[acpi] Found valid RSDP 1.0 at %lx\n", addr - bootloader.hhdmOffset);
          #endif
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
          #if defined(DEBUG_ACPI)
          printf("[acpi] Using XSDT at %lx\n", rsdp->xsdt_address);
          #endif
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
          #if defined(DEBUG_ACPI)
          printf("[acpi] Using RSDT at %lx\n", rsdp->rsdt_address);
          #endif
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
        #if defined(DEBUG_ACPI)
        printf("[acpi] Found table %c%c%c%c at %lx\n",signature[0], signature[1], signature[2], signature[3],tableAddr);
          #endif
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
  #if defined(DEBUG_ACPI)
  printf("[acpi] MADT found: lapic_address=%lx\n", cachedMadt->lapic_address);
          #endif
}

AcpiMadt* acpiGetMadt() {
  return cachedMadt;
}

/* =========================================================================
 * acpiGetDsdt - locate DSDT via FADT
 *
 * The FADT (signature "FACP") contains two fields pointing at the DSDT:
 *   dsdt   (offset 40) - 32-bit physical address, all ACPI revisions
 *   x_dsdt (offset 140) - 64-bit physical address, ACPI 2.0+ (revision >= 2)
 *
 * We prefer x_dsdt when available and non-zero, because on 64-bit systems
 * with >4 GB of RAM the DSDT may be placed above the 4 GB boundary.
 * ========================================================================= */

AcpiDsdtInfo acpiGetDsdt() {
  AcpiDsdtInfo result;
  result.ptr    = nullptr;
  result.length = 0;

  /* Step 1 - get FADT */
  AcpiFadt* fadt = (AcpiFadt*)acpiGetTableBySignature("FACP");
  if (!fadt) {
    printf("[acpi] acpiGetDsdt: FADT (FACP) not found\n");
    return result;
  }

  /* Step 2 - pick 32-bit or 64-bit DSDT physical address */
  uint64_t dsdtPhys = 0;

  if (fadt->hdr.revision >= 2 &&
      fadt->hdr.length >= 148 &&   /* x_dsdt is at offset 140; table must be
                                     * at least 148 bytes long to contain it */
      fadt->x_dsdt != 0) {
    dsdtPhys = fadt->x_dsdt;
    #if defined(DEBUG_ACPI)
    printf("[acpi] acpiGetDsdt: using X_DSDT (64-bit) = 0x%lx\n", dsdtPhys);
    #endif
  } else if (fadt->dsdt != 0) {
    dsdtPhys = (uint64_t)fadt->dsdt;
    #if defined(DEBUG_ACPI)
    printf("[acpi] acpiGetDsdt: using DSDT (32-bit) = 0x%lx\n", dsdtPhys);
    #endif
  } else {
    printf("[acpi] acpiGetDsdt: both DSDT and X_DSDT fields are zero\n");
    return result;
  }

  /* Step 3 - apply HHDM offset and read the DSDT table header */
  AcpiTableHeader* dsdtHdr =
      (AcpiTableHeader*)(bootloader.hhdmOffset + dsdtPhys);

  /* Sanity-check the DSDT header signature */
  if (dsdtHdr->signature[0] != 'D' ||
      dsdtHdr->signature[1] != 'S' ||
      dsdtHdr->signature[2] != 'D' ||
      dsdtHdr->signature[3] != 'T') {
    printf("[acpi] acpiGetDsdt: bad DSDT signature at phys 0x%lx "
           "(got '%c%c%c%c')\n",
           dsdtPhys,
           dsdtHdr->signature[0], dsdtHdr->signature[1],
           dsdtHdr->signature[2], dsdtHdr->signature[3]);
    return result;
  }

  /* Sanity-check the length */
  if (dsdtHdr->length < sizeof(AcpiTableHeader) ||
      dsdtHdr->length > 0x400000) {   /* 4 MB hard cap - real DSDTs are <512 KB */
    printf("[acpi] acpiGetDsdt: DSDT length %u looks wrong\n", dsdtHdr->length);
    return result;
  }

  printf("[acpi] acpiGetDsdt: DSDT found at phys=0x%lx virt=0x%lx len=%u\n",
         dsdtPhys, (uint64_t)(uintptr_t)dsdtHdr, dsdtHdr->length);

  result.ptr    = (const uint8_t*)dsdtHdr;
  result.length = dsdtHdr->length;
  return result;
}