#include <apic.h>
#include <bootloader.h>
#include <linked_list.h>
#include <liballoc.h>
#include <system.h>
#include <timer.h>
#include <hcf.hpp>
#include <keyboard.h>
#include <minimal_acpi.h>

// Advanced Programmable Interrupt Controller driver
// (SMP-friendly 8259 PIC)

/* Global variable definitions */
uint64_t apicPhys = 0;
uint64_t apicVirt = 0;
LLcontrol dsIoapic = {0};
uint8_t *irqPerCpu = nullptr;
uint8_t  irqGenericArray[MAX_IRQ] = {0};
uint32_t lapicGenericArray[MAX_IRQ] = {0};

bool apic_initialized = false;

/*
 * Trigger mode: 0 is edge-triggered, 1 is level-triggered.
 * Pin polarity: 0 is active-high, 1 is active-low.
 */

bool apicCheck() {
  uint32_t eax = 1; // info
  uint32_t ebx, ecx, edx;

  cpuid(&eax, &ebx, &ecx, &edx);

  // bit 9 in edx is APIC support
  return (edx & (1 << 9)) != 0;
}

/* APIC */

void apicWrite(uint32_t offset, uint32_t value) {
  if (!apicVirt) {
    printf("[apic] Warning: apicWrite called before APIC initialized\n");
    return;
  }
  uint32_t volatile *ptr = (uint32_t volatile *)(apicVirt + offset);
  *ptr = value;
}

uint32_t apicRead(uint32_t offset) {
  if (!apicVirt) {
    printf("[apic] Warning: apicRead called before APIC initialized\n");
    return 0;
  }
  uint32_t volatile *ptr = (uint32_t volatile *)(apicVirt + offset);
  return *ptr;
}

/* I/O APIC */

uint32_t ioApicRead(uint64_t ioapicVirt, uint32_t reg) {
  uint32_t volatile *ioapic = (uint32_t volatile *)ioapicVirt;
  ioapic[0] = (reg & 0xff);
  return ioapic[4];
}

void ioApicWrite(uint64_t ioapicVirt, uint32_t reg, uint32_t value) {
  uint32_t volatile *ioapic = (uint32_t volatile *)ioapicVirt;
  ioapic[0] = (reg & 0xff);
  ioapic[4] = value;
}

void ioApicWriteRedEntry(uint64_t ioApicVirt, uint8_t entry, uint8_t vector,
                         uint8_t delivery, uint8_t destmode, uint8_t polarity,
                         uint8_t mode, uint8_t mask, uint8_t dest) {
  uint32_t val = vector;
  val |= (delivery & 0b111) << 8;
  val |= (destmode & 1) << 11;
  val |= (polarity & 1) << 13;
  val |= (mode & 1) << 15;
  val |= (mask & 1) << 16;

  ioApicWrite(ioApicVirt, 0x10 + entry * 2, val);
  ioApicWrite(ioApicVirt, 0x11 + entry * 2, (uint32_t)dest << 24);
}

bool ioApicFetchCb(void *data, void *ctx) {
  IOAPIC *browse = data;
  uint8_t irq = *(uint8_t *)ctx;
  // = cause of +1
  return irq >= browse->ioapicRedStart && irq <= browse->ioapicRedEnd;
}

IOAPIC *ioApicFetch(uint8_t irq) {
  return LinkedListSearch(&dsIoapic, ioApicFetchCb, &irq);
}

/* Base stuff */

size_t apicGetBase() { return rdmsr(IA32_APIC_BASE_MSR) & 0xFFFFF000; }
void   apicSetBase(size_t apic) {
  wrmsr(IA32_APIC_BASE_MSR,
          apic | IA32_APIC_BASE_MSR_ENABLE | IA32_APIC_BASE_MSR_BSP);
}

uint32_t apicCurrentCore() {
  if (!apicVirt) {
    printf("[lapic::currCore] APIC hasn't been initialized yet!\n");
    return 0;
  }

  return (apicRead(APIC_REGISTER_ID) >> 24) & 0xFF;
}

/* PCI routing - TODO: Implement when needed without UACPI */

uint8_t ioApicPciRegister(pci_device *device, pci_general_device *details) {
  printf("[ioapic::pci] PCI routing not yet implemented (would need ACPI AML parsing)\n");
  return 0;
}

/* Legacy ISA redirection */

uint8_t ioApicRedirect(uint8_t irq, bool ignored) {
  uint8_t polarity = 0; // active high
  uint8_t trigger = 0;  // edge triggered

  AcpiMadt* madt = acpiGetMadt();
  if (!madt) {
    printf("[apic] Warning: Cannot configure ISA IRQ - no MADT available\n");
    return 0;
  }

  // Browse the override table
  size_t curr = (size_t)madt + sizeof(AcpiMadt);
  size_t end = curr + madt->hdr.length;
  
  while (curr < end) {
    AcpiEntryHdr *browse = (AcpiEntryHdr *)curr;
    
    if (browse->type == ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
      AcpiMadtInterruptSourceOverride *specialized =
          (AcpiMadtInterruptSourceOverride *)browse;
      
      if (specialized->source == irq) {
        // Found override!
        irq = specialized->gsi;
        polarity = (specialized->flags & 2) ? 1 : 0;
        trigger = (specialized->flags & 8) ? 1 : 0;
        break;
      }
    }
    curr += browse->length;
  }

  IOAPIC *ioapic = ioApicFetch(irq);
  if (!ioapic) {
    printf("[apic] Warning: I/O APIC not found for irq{%d} - cannot configure\n", irq);
    return 0;
  }

  // Allocate
  uint32_t lapic = 0;
  uint8_t  targIrq = irqPerCoreAllocate(irq, &lapic);

  uint8_t entry = irq - ioapic->ioapicRedStart;
  ioApicWriteRedEntry(ioapic->ioapicVirt, entry, targIrq, 0, 0, polarity,
                      trigger, ignored, lapic);

  return targIrq;
}

/* IRQ/Vector allocation utilities */
int  irqLast = 0; // starting point

// Static IRQ buffer for single-core (pre-allocated, no calloc needed)
static uint8_t staticIrqBuffer[1] = {0};

void initiateIrqPerCore() {
  #if defined(DEBUG_APIC)
  printf("[apic] Setting up IRQ per-core buffer\n");
  #endif
  // For single-core, just point to our static buffer
  irqPerCpu = staticIrqBuffer;
  #if defined(DEBUG_APIC)
  printf("[apic] IRQ per-core buffer ready at %lx\n", (uint64_t)irqPerCpu);
  #endif
}

uint8_t irqPerCoreAllocate(uint8_t gsi, uint32_t *lapicId) {
  // check for existing items
  for (int i = 0; i < irqLast; i++) {
    if (irqGenericArray[i] == gsi) {
      // found smth for this gsi
      *lapicId = lapicGenericArray[i];
      return 32 + i;
    }
  }

  // spurious vector checks (extendable, hence the while)
  while (irqLast == 0xff)
    irqLast++;

  // check boundaries
  int irqGenericIndex = irqLast;
  if (irqGenericIndex > (MAX_IRQ - 1)) {
    printf("[irq] Irq overflow! index{%d}\n", irqGenericArray);
    Halt();
  }

  // note to the todo: for when the time comes, keep in mind that a lot of
  // handlers assume only they are ran and nothing else at the same time sooo

  // find the core with the least irqs allocated
  int    min = MAX_IRQ;
  size_t minIndex = 0;
  for (size_t i = 0; i < 1; i++) { // todo: bootloader.smp->cpu_count
    if (irqPerCpu[i] < min) {
      irqPerCpu[i] = min;
      minIndex = i;
    }
  }

  irqGenericArray[irqGenericIndex] = gsi; // keep track
  // lapicGenericArray[irqGenericIndex] = bootloader.smp->cpus[minIndex]->lapic_id; // keep track TODO: fix after smp
  lapicGenericArray[irqGenericIndex] = 0;
  irqPerCpu[minIndex]++;                        // count per CPU

  irqLast++; // for the next one
//   *lapicId = bootloader.smp->cpus[minIndex]->lapic_id; TODO: fix after smp
  *lapicId = 1;
  return irqGenericIndex + 32;
}

void apicPrintCb(void *data, void *ctx) {
  IOAPIC *apic = data;
}

void initiateAPIC() {
  if (!apicCheck()) {
    printf("[apic] Warning: APIC is not supported!\n");
    Halt();
  }
  #if defined(DEBUG_APIC)
  printf("[apic] APIC is supported, beginning initialization...\n");

  printf("[apic] Initializing IRQ per core...\n");
  #endif
  initiateIrqPerCore();
  #if defined(DEBUG_APIC)
  printf("[apic] IRQ per core initialized\n");
  
  printf("[apic] Reading APIC base from MSR...\n");
  #endif
  apicPhys = apicGetBase();
  #if defined(DEBUG_APIC)
  printf("[apic] Successfully read MSR\n");
  
  printf("[apic] APIC physical address from MSR: %lx\n", apicPhys);
  #endif
  
  // Validate APIC physical address
  if (!apicPhys || apicPhys > 0xFFFFFFFF) {
    printf("[apic] Invalid APIC address: %lx, using fallback 0xFEE00000\n", apicPhys);
    apicPhys = 0xFEE00000;
  }
  
  // Set virtual address
  apicVirt = bootloader.hhdmOffset + apicPhys;
  #if defined(DEBUG_APIC)
  printf("[apic] APIC virtual address: %lx (phys: %lx, hhdm: %lx)\n", 
         apicVirt, apicPhys, bootloader.hhdmOffset);
  #endif

  #if defined(DEBUG_APIC)
  // Initialize minimal ACPI parser and get MADT
  printf("[apic] Initializing ACPI...\n");
  #endif
  //acpiInit();
  #if defined(DEBUG_APIC)
  printf("[apic] geting madt from acpiGetMadt\n");
  #endif
  AcpiMadt* madt = acpiGetMadt();
  #if defined(DEBUG_APIC)
  printf("[apic] madt at %lx\n", madt);
  #endif
  
  if (!madt) {
    printf("[apic] MADT not available - initializing basic LAPIC only\n");
    #if defined(DEBUG_APIC)
    printf("[apic] Setting APIC base at %lx...\n", apicPhys);
  #endif
    
    // Very basic LAPIC initialization
    // Set the APIC enable bit in MSR
    #if defined(DEBUG_APIC)
    printf("[apic] Enabling APIC via MSR...\n");
  #endif
    apicSetBase(apicPhys);
    
    #if defined(DEBUG_APIC)
    printf("[apic] Reading SVR register at offset 0xF0...\n");
    #endif
    // Try to enable the LAPIC (software enable bit in SVR register at offset 0xF0)
    uint32_t svr = apicRead(0xF0);
    #if defined(DEBUG_APIC)
    printf("[apic] SVR value: %lx\n", svr);
  #endif
    
  #if defined(DEBUG_APIC)
  printf("[apic] Setting software enable bit...\n");
  #endif
    apicWrite(0xF0, svr | 0x1FF);
    
    #if defined(DEBUG_APIC)
    printf("[apic] Basic LAPIC initialization complete\n");
  #endif
    return;
  }

  if (madt->lapic_address != apicPhys) {
    printf("[apic] Warning! MADT physical address doesn't match fetched one! "
           "madt_phys{%lx} rdmsr_phys{%lx}\n",
           madt->lapic_address, apicPhys);
    apicPhys = madt->lapic_address;
    apicVirt = bootloader.hhdmOffset + apicPhys;
  }

  LinkedListInit(&dsIoapic, sizeof(IOAPIC));
  
  // Parse MADT entries
  size_t curr = (size_t)madt + sizeof(AcpiMadt);
  size_t end = curr + madt->hdr.length;
  int    ioapics = 0;
  
  while (curr < end) {
    AcpiEntryHdr *browse = (AcpiEntryHdr *)curr;
    
    #if defined(DEBUG_APIC)
    printf("[apic] MADT browse type is: %u\n",browse->type);
  #endif

    if (browse->type == ACPI_MADT_ENTRY_TYPE_IOAPIC) {
      AcpiMadtIoapic *specialized = (AcpiMadtIoapic *)browse;
      #if defined(DEBUG_APIC)
      printf("[apic] MADT &dsIoapic is: %lx , sizeof(IOAPIC) is: %u\n",&dsIoapic, sizeof(IOAPIC));
  #endif
      IOAPIC *ioapic = (IOAPIC *)LinkedListAllocate(&dsIoapic, sizeof(IOAPIC));
      ioapic->id = specialized->id;
    
      ioapic->ioapicPhys = specialized->address;
      ioapic->ioapicVirt = bootloader.hhdmOffset + ioapic->ioapicPhys;
    
      ioapic->ioapicRedStart = specialized->gsi_base;
      int capacity = (ioApicRead(ioapic->ioapicVirt, 1) >> 16) & 0xFF;
      ioapic->ioapicRedEnd = ioapic->ioapicRedStart + capacity;
      ioapics++;
    } else if (browse->type == ACPI_MADT_ENTRY_TYPE_LAPIC_ADDRESS_OVERRIDE) {
      AcpiMadtLapicAddressOverride *specialized =
          (AcpiMadtLapicAddressOverride *)browse;
      printf("[apic] Warning! Local APIC override! old{%lx} new{%lx}\n",
             apicPhys, specialized->lapic_address);
      apicPhys = specialized->lapic_address;
      apicVirt = bootloader.hhdmOffset + apicPhys;
    }
    curr += browse->length;
  }

  #if defined(DEBUG_APIC)
  printf("[apic] ioapics is :%d\n", ioapics);
  #endif
  if (ioapics == 0) 
  {
    printf("[apic] No I/O APICs found - skipping I/O APIC setup\n");
  }

  #if defined(DEBUG_APIC)
  printf("[apic] Detection completed: lapic{%lx} ", apicPhys);
  #endif
  if (ioapics) LinkedListTraverse(&dsIoapic, apicPrintCb, 0);
  printf("\n");

  // enable lapic (for the bootstrap core)
  apicSetBase(apicPhys);
  apicWrite(0xF0, apicRead(0xF0) | 0x1FF);

  apic_initialized = true;
  keyboard_initialize();
}

void smpInitiateAPIC() {
  // enable lapic
  apicSetBase(apicPhys);
  apicWrite(0xF0, apicRead(0xF0) | 0x1FF);
}


/* TODO: uncomment this and make it work
size_t acpiPoweroff() {
  uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
  if (uacpi_unlikely_error(ret)) {
    debugf("[acpi] Couldn't prepare for poweroff: %s\n",
           uacpi_status_to_string(ret));
    return ERR(EIO);
  }

  asm volatile("cli");
  uacpi_status retPoweroff = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
  if (uacpi_unlikely_error(retPoweroff)) {
    asm volatile("sti");
    debugf("[acpi] Couldn't power off the system: %s\n",
           uacpi_status_to_string(retPoweroff));
    return ERR(EIO);
  }

  debugf("[acpi] Shouldn't be reached after power off!\n");
  panic();
  return 0;
}

size_t acpiReboot() {
  uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);

  uacpi_status ret = uacpi_reboot();
  if (uacpi_unlikely_error(ret)) {
    debugf("[acpi] Couldn't restart the system: %s\n",
           uacpi_status_to_string(ret));
    return ERR(EIO);
  }

  debugf("[acpi] Shouldn't be reached after reboot!\n");
  panic();
  return 0;
}

*/