#include <apic.h>
#include <bootloader.h>
#include <framebufferutil.h>
#include <linked_list.h>
#include <liballoc.h>
#include <system.h>
#include <timer.h>
#include <hcf.hpp>
#include <keyboard.h>
#include <minimal_acpi.h>
#include <timer.h>
#include <utility.h>

// Advanced Programmable Interrupt Controller driver
// (SMP-friendly 8259 PIC)

/* Global variable definitions */
uint64_t apicPhys = 0;
uint64_t apicVirt = 0;
LLcontrol dsIoapic = {0};
uint8_t *irqPerCpu = nullptr;
bool x2apic_mode = false;
uint8_t  irqGenericArray[MAX_IRQ] = {0};
uint32_t lapicGenericArray[MAX_IRQ] = {0};

uint8_t smp_ap_lapic_id_list[256] = {0};
uint8_t smp_ap_count = 0;
uint8_t per_lapic_core_type[256] = {0};

static uint8_t bsp_lapic_id_cached = 0xFF;

bool apic_initialized = false;


 bool apicIsX2Apic() 
 {
    uint64_t msr = rdmsr(IA32_APIC_BASE_MSR);
    return (msr & IA32_APIC_BASE_MSR_X2APIC) != 0;
}

// apicGetBspLapicId() to mean "me".
uint8_t apicCurrentLapicId(void) {
    if (!apicVirt && !(rdmsr(IA32_APIC_BASE_MSR) & IA32_APIC_BASE_MSR_X2APIC)) {
        // Not yet initialized in MMIO mode; best we can do is 0.
        return 0;
    }
    if (x2apic_mode) {
        // x2APIC: APIC ID is in MSR 0x802.
        return (uint8_t)(rdmsr(0x802) & 0xFF);
    }
    return (apicRead(APIC_REGISTER_ID) >> 24) & 0xFF;
}

// BSP's LAPIC ID — constant after initiateAPIC() has run on the BSP.
uint8_t apicGetBspLapicId(void) {
    return bsp_lapic_id_cached;
}

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
    printf("[apic] Warning: apicWrite called before APIC initialized by %s\n", boot_step_name);
    return;
  }
  uint32_t volatile *ptr = (uint32_t volatile *)(apicVirt + offset);
  *ptr = value;
}

uint32_t apicRead(uint32_t offset) {
  if (!apicVirt) {
    printf("[apic] Warning: apicRead called before APIC initialized by %s\n", boot_step_name);
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

size_t apicGetBase(void) {
    return (size_t)(rdmsr(IA32_APIC_BASE_MSR) & 0x0000000FFFFFF000ULL);
}

void apicSetBase(size_t apic) {
    uint64_t cur = rdmsr(IA32_APIC_BASE_MSR);

    // Preserve BSP and x2APIC — do not invent either.
    uint64_t preserved = cur & (IA32_APIC_BASE_MSR_BSP |
                                IA32_APIC_BASE_MSR_X2APIC);

    // When x2APIC is active, bits 12-35 of the MSR are architecturally
    // ignored. Leave the base field alone; writing it has no effect and
    // some CPUs log it as a reserved write.
    uint64_t base;
    if (cur & IA32_APIC_BASE_MSR_X2APIC) {
        base = cur & 0x0000000FFFFFF000ULL;
    } else {
        base = (uint64_t)apic & 0x0000000FFFFFF000ULL;
    }

    wrmsr(IA32_APIC_BASE_MSR,
          base | IA32_APIC_BASE_MSR_ENABLE | preserved);
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
  uint8_t bspId = apicGetBspLapicId();
  lapicGenericArray[irqGenericIndex] = bspId;
  irqPerCpu[minIndex]++;
  irqLast++;
  *lapicId = bspId;

  return (gsi < 16) ? (32 + gsi) : (irqGenericIndex + 32);
}

void apicPrintCb(void *data, void *ctx) {
  IOAPIC *apic = data;
}

void initiateAPIC(void) {
    if (!apicCheck()) {
        printf("[apic] Warning: APIC is not supported!\n");
        Halt();
    }

    // If you have not implemented x2APIC MSR paths yet, force xAPIC mode
    // and be done with it. Delete this block once you implement the MSR
    // versions of apicRead/apicWrite.
    {
        uint64_t cur = rdmsr(IA32_APIC_BASE_MSR);
        if (cur & IA32_APIC_BASE_MSR_X2APIC) {
            cur &= ~IA32_APIC_BASE_MSR_X2APIC;
            wrmsr(IA32_APIC_BASE_MSR, cur);
        }
    }

    x2apic_mode = apicIsX2Apic();   // now reliably false

    initiateIrqPerCore();

    apicPhys = apicGetBase();
    if (!apicPhys || apicPhys > 0xFFFFFFFF) {
        printf("[apic] Invalid APIC address: %lx, using fallback 0xFEE00000\n", apicPhys);
        apicPhys = 0xFEE00000;
    }
    apicVirt = bootloader.hhdmOffset + apicPhys;

    AcpiMadt* madt = acpiGetMadt();
    if (!madt) {
        printf("[apic] MADT not available - initializing basic LAPIC only\n");
        apicSetBase(apicPhys);
        apicWrite(0xF0, apicRead(0xF0) | 0x1FF);
        bsp_lapic_id_cached = apicCurrentLapicId();
        apic_initialized = true;
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

    // ---- Pass 1: IOAPICs & LAPIC address overrides ----
    size_t curr = (size_t)madt + sizeof(AcpiMadt);
    size_t end  = curr + madt->hdr.length;
    int    ioapics = 0;

    while (curr < end) {
        AcpiEntryHdr *browse = (AcpiEntryHdr *)curr;
        if (browse->type == ACPI_MADT_ENTRY_TYPE_IOAPIC) {
            AcpiMadtIoapic *specialized = (AcpiMadtIoapic *)browse;
            IOAPIC *ioapic = (IOAPIC *)LinkedListAllocate(&dsIoapic, sizeof(IOAPIC));
            ioapic->id             = specialized->id;
            ioapic->ioapicPhys     = specialized->address;
            ioapic->ioapicVirt     = bootloader.hhdmOffset + ioapic->ioapicPhys;
            ioapic->ioapicRedStart = specialized->gsi_base;
            int capacity = (ioApicRead(ioapic->ioapicVirt, 1) >> 16) & 0xFF;
            ioapic->ioapicRedEnd   = ioapic->ioapicRedStart + capacity;
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

    if (ioapics == 0)
        printf("[apic] No I/O APICs found - skipping I/O APIC setup\n");

    // Enable the BSP's LAPIC.
    apicSetBase(apicPhys);
    apicWrite(0xF0, apicRead(0xF0) | 0x1FF);

    // Cache the BSP's LAPIC ID now that the LAPIC is enabled.
    bsp_lapic_id_cached = apicCurrentLapicId();

    // ---- Pass 2: collect enabled AP LAPIC IDs ----
    uint8_t bsp_lapic_id = bsp_lapic_id_cached;
    curr = (size_t)madt + sizeof(AcpiMadt);   // rewind
    end  = curr + madt->hdr.length;
    while (curr < end) {
        AcpiEntryHdr *browse = (AcpiEntryHdr *)curr;
        if (browse->type == ACPI_MADT_ENTRY_TYPE_LAPIC) {
            AcpiMadtLapic *lapic = (AcpiMadtLapic *)browse;
            if ((lapic->flags & 1) && lapic->lapic_id != bsp_lapic_id) {
                smp_ap_lapic_id_list[smp_ap_count++] = lapic->lapic_id;
            }
        }
        curr += browse->length;
    }

    // BSP core type via CPUID 0x1A
    {
        uint32_t eax = 0x1A, ebx = 0, ecx = 0, edx = 0;
        cpuid(&eax, &ebx, &ecx, &edx);
        uint8_t bsp_core_type = (eax >> 24) & 0xFF;
        if (bsp_core_type == CORE_TYPE_PCORE || bsp_core_type == CORE_TYPE_ECORE)
            per_lapic_core_type[bsp_lapic_id] = bsp_core_type;
    }

    printf("[smp] found %u APs: lapic_ids=[", smp_ap_count);
    for (int i = 0; i < smp_ap_count; i++) {
        printf("%u", smp_ap_lapic_id_list[i]);
        if (i < smp_ap_count - 1) printf(", ");
    }
    printf("]\n");

    // Re-read x2APIC state one final time before any IPIs are sent.
    x2apic_mode = apicIsX2Apic();

    apic_initialized = true;

    // Disable legacy 8259 PIC.
    out_port_byte(0x21, 0xff);
    out_port_byte(0xa1, 0xff);

    printf("APIC initialized.\n");
}

void smpInitiateAPIC(void) {
    if (x2apic_mode) {
        // In x2APIC mode the SVR is still at MMIO offset 0xF0 logically,
        // but must be accessed via MSR 0x80F (SVR). If you haven't
        // implemented the MSR helpers yet, force xAPIC mode instead.
        uint64_t svr = rdmsr(0x80F);
        wrmsr(0x80F, svr | 0x1FF);
    } else {
        apicWrite(0xF0, apicRead(0xF0) | 0x1FF);
    }
}

void apicSendIpiToAll(uint8_t vector) {
    if (x2apic_mode) {
        // broadcast to all excluding self
        wrmsr(0x830, (uint64_t)0xC0000 | vector); // shorthand: all-exc-self
    } else {
        apicWrite(0x310, 0);
        apicWrite(0x300, (3 << 18) | (1 << 14) | vector); // all-exc-self, fixed
        while (apicRead(0x300) & (1 << 12)) asm volatile("pause");
    }
}

void tlb_shootdown_all() {
    apicSendIpiToAll(IPI_VECTOR_TLB_SHOOTDOWN);
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