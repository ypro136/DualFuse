#include <ahci.h>

 #include <types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <isr.h>
#include <utility.h>
#include <pmm.h>
#include <vmm.h>
#include <liballoc.h>

#include <timer.h>
#include <bootloader.h>
#include <pci.h>


#include <data_structures/linked_list.h>
#include <hcf.hpp>



// Generic AHCI driver (tested on real hardware in cavOS)


const AHCI_DEVICE *is_AHCI_controller(pci_device *device) {
  for (int i = 0; i < (sizeof(ahci_ids) / sizeof(ahci_ids[0])); i++) {
    if (COMBINE_WORD(device->device_id, device->vendor_id) == ahci_ids[i].id)
      return &ahci_ids[i];
  }

  return 0;
}

/* Port command engine: */

void ahci_cmd_start(HBA_PORT *port) {
  // Wait until CR (bit15) is cleared
  while (port->cmd & HBA_PxCMD_CR)
    ;

  // Set FRE (bit4) and ST (bit0)
  port->cmd |= HBA_PxCMD_FRE;
  port->cmd |= HBA_PxCMD_ST;
}

void ahci_cmd_stop(HBA_PORT *port) {
  // Clear ST (bit0)
  port->cmd &= ~HBA_PxCMD_ST;

  // Clear FRE (bit4)
  port->cmd &= ~HBA_PxCMD_FRE;

  // Wait until FR (bit14), CR (bit15) are cleared
  while (1) {
    if (port->cmd & HBA_PxCMD_FR)
      continue;
    if (port->cmd & HBA_PxCMD_CR)
      continue;
    break;
  }
}

/* Command port operations: */

//Spinlock LOCK_AHCI_CMD_FIND = ATOMIC_FLAG_INIT;
int      ahci_cmd_find(ahci *ahciPtr, HBA_PORT *port) {
  //spinlock_acquire(&LOCK_AHCI_CMD_FIND);

  int ret = -1;

  // Don't let the waiting get out of hand
  uint64_t start = timerTicks;
  while (ret == -1 && timerTicks <= (start + 500)) {
    // If not set in SACT and CI, the slot is free
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < 32; i++) {
      if ((slots & 1) == 0 && !(ahciPtr->cmdSlotsPreping & (1 << i))) {
        ret = i;
        break;
      }
      slots >>= 1;
    }
  }

  if (ret == -1)
    printf("Cannot find free command list entry\n");
  else
    ahciPtr->cmdSlotsPreping |= (1 << ret);
  //spinlock_release(&LOCK_AHCI_CMD_FIND);
  return ret;
}

inline bool ahci_cmd_issue(ahci *ahciPtr, HBA_PORT *port, int slot) {
  port->ci = 1 << slot; // Issue command

  // Done "preparing"
  ahciPtr->cmdSlotsPreping &= ~(1 << slot);

  // Wait for completion
  while (1) {
    // In some longer duration reads, it may be helpful to spin on the DPS bit
    // in the PxIS port field as well (1 << 5)
    if ((port->ci & (1 << slot)) == 0)
      break;
  }

  return true;
}

/* Set up AHCI parts for reading/writing: */

inline HBA_CMD_HEADER *ahci_setup_cmd_header(ahci *ahciPtr, uint32_t portId,
                                                uint32_t cmdslot, uint32_t prdt,
                                                bool write) {
  HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER *)ahciPtr->clbVirt[portId];
  cmdheader =
      (HBA_CMD_HEADER *)((size_t)cmdheader + cmdslot * sizeof(HBA_CMD_HEADER));
  cmdheader->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t); // Command FIS size
  cmdheader->w = (uint8_t)write; // 0 = read, 1 = write
  cmdheader->prdtl = prdt;       // PRDT entries count

  return cmdheader;
}

inline HBA_CMD_TBL *ahci_setup_cmd_table(ahci           *ahciPtr,
                                            HBA_CMD_HEADER *cmdheader,
                                            uint32_t        portId,
                                            uint32_t        cmdTableId) {
  HBA_CMD_TBL *cmdtbl =
      (HBA_CMD_TBL *)((size_t)ahciPtr->ctbaVirt[portId] + cmdTableId * 256);
  memset(cmdtbl, 0,
         sizeof(HBA_CMD_TBL) + (cmdheader->prdtl - 1) * sizeof(HBA_PRDT_ENTRY));
  return cmdtbl;
}

inline void ahci_setup_PRDT(HBA_CMD_HEADER *cmdheader, HBA_CMD_TBL *cmdtbl,
                                uint16_t *buff, uint32_t *count) {
  // 4 MiB (16 sectors) per PRDT
  int i = 0;
  for (i = 0; i < cmdheader->prdtl - 1; i++) {
    size_t targPhys = virtual_to_physical((size_t)buff);
    cmdtbl->prdt_entry[i].dba = SPLIT_64_LOWER(targPhys);
    cmdtbl->prdt_entry[i].dbau = SPLIT_64_HIGHER(targPhys);
    cmdtbl->prdt_entry[i].dbc =
        4194304 - 1; // 4 MiB (this value should always be set to 1 less
                     // than the actual value)
    cmdtbl->prdt_entry[i].i = 1;
    buff += 4194304 / 2;     // appropriate words
    *count -= 4194304 / 512; // appropriate sectors
  }
  size_t targPhys = virtual_to_physical((size_t)buff);
  // Last entry
  cmdtbl->prdt_entry[i].dba = SPLIT_64_LOWER(targPhys);
  cmdtbl->prdt_entry[i].dbau = SPLIT_64_HIGHER(targPhys);
  cmdtbl->prdt_entry[i].dbc = (*count << 9) - 1; // 512 bytes per sector
  cmdtbl->prdt_entry[i].i = 1;
}

/* Port initialization (used only on startup): */

int ahci_port_type(HBA_PORT *port) {
  uint32_t ssts = port->ssts;

  uint8_t ipm = (ssts >> 8) & 0x0F;
  uint8_t det = ssts & 0x0F;

  if (det != HBA_PORT_DET_PRESENT) // Check drive status
    return AHCI_DEV_NULL;
  if (ipm != HBA_PORT_IPM_ACTIVE)
    return AHCI_DEV_NULL;

  switch (port->sig) {
  case SATA_SIG_ATAPI:
    return AHCI_DEV_SATAPI;
  case SATA_SIG_SEMB:
    return AHCI_DEV_SEMB;
  case SATA_SIG_PM:
    return AHCI_DEV_PM;
  default:
    return AHCI_DEV_SATA;
  }
}

void ahci_port_rebase(ahci *ahciPtr, HBA_PORT *port, int portno) 
{

  ahci_cmd_stop(port); // Stop command engine



  // enable (all) interrupts
  port->ie = 1;

  // Command list offset: 1K*portno
  // Command list entry size = 32
  // Command list entry maxim count = 32
  // Command list maxim size = 32*32 = 1K per port
  uint32_t clbPages = CEILING_DIVISION(sizeof(HBA_CMD_HEADER) * 32, BLOCK_SIZE);
  void    *clbVirt = virtual_allocate(clbPages); //!
  ahciPtr->clbVirt[portno] = clbVirt;
  size_t clbPhys = virtual_to_physical((size_t)clbVirt);
  port->clb = SPLIT_64_LOWER(clbPhys);
  port->clbu = SPLIT_64_HIGHER(clbPhys);
  memset(clbVirt, 0, clbPages * BLOCK_SIZE); // could've just done 1024

  // FIS offset: 32K+256*portno
  // FIS entry size = 256 bytes per port
  // use a bit of the wasted (256-aligned) space for this
  size_t fbPhys = clbPhys + 2048;
  port->fb = SPLIT_64_LOWER(fbPhys);
  port->fbu = SPLIT_64_HIGHER(fbPhys);
  // memset((void *)(port->fb), 0, 256); already 0'd

  // Command table offset: 40K + 8K*portno
  // Command table size = 256*32 = 8K per port
  HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER *)clbVirt;
  void           *ctbaVirt = virtual_allocate(2); // 2 pages = 8192 bytes //!
  ahciPtr->ctbaVirt[portno] = ctbaVirt;
  size_t ctbaPhys = (size_t)virtual_to_physical((size_t)ctbaVirt);
  memset(ctbaVirt, 0, 8192);
  for (int i = 0; i < 32; i++) {
    cmdheader[i].prdtl = 8; // 8 prdt entries per command table
                            // 256 bytes per command table, 64+16+48+16*8
    // Command table offset: 40K + 8K*portno + cmdheader_index*256
    size_t ctbaPhysCurr = ctbaPhys + i * 256;
    cmdheader[i].ctba = SPLIT_64_LOWER(ctbaPhysCurr);
    cmdheader[i].ctbau = SPLIT_64_HIGHER(ctbaPhysCurr);
    // memset((void *)cmdheader[i].ctba, 0, 256); already 0'd
  }

  if (port->serr & (1 << 10))
    port->serr |= (1 << 10);

  // COMRESET / CLO*
  port->cmd |= (1 << 3);
  uint64_t targ = timerTicks + 150;
  bool  target = false;
  while (port->cmd & (1 << 3) && target)
  {
    target = timerTicks < targ;
  }

  // OpenBSD hack (made pavilion work)
  port->serr = port->serr;
  ahci_cmd_start(port); // Start command engine

  ahciPtr->sata |= (1 << portno);
}

void ahci_port_probe(ahci *ahciPtr, HBA_MEM *abar) {
  uint32_t pi = abar->pi;
  for (int i = 0; i < 32; i++) {
    if (pi & 1) {
      int dt = ahci_port_type(&abar->ports[i]);
      if (dt == AHCI_DEV_SATA) {
        printf("[pci::ahci] SATA drive found at port %d\n", i);
        ahci_port_rebase(ahciPtr, &abar->ports[i], i);
      } else if (dt == AHCI_DEV_SATAPI) {
        printf("[pci::ahci] (unsupported) SATAPI drive found at port %d\n", i);
      } else if (dt == AHCI_DEV_SEMB) {
        printf("[pci::ahci] (unsupported) SEMB drive found at port %d\n", i);
      } else if (dt == AHCI_DEV_PM) {
        printf("[pci::ahci] (unsupported) PM drive found at port %d\n", i);
      }
      // otherwise, no drive is in this port
    }

    pi >>= 1;
  }
}

// Await for port to stop being "busy" and send results
inline bool ahci_port_ready(HBA_PORT *port) {
  uint64_t start = timerTicks;
  while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ))) {
    if (timerTicks >= (start + 1000)) {
      printf("[pci::ahci] Port is hung ATA_DEV_BUSY{%d} ATA_DEV_DRQ{%d}\n",
             port->tfd & ATA_DEV_BUSY, port->tfd & ATA_DEV_DRQ);
      return false;
    }
  }

  return true;
}

bool ahci_read(ahci *ahciPtr, uint32_t portId, HBA_PORT *port, uint32_t startl,
              uint32_t starth, uint32_t count, uint8_t *buff) {
  port->is = (uint32_t)-1; // Clear pending interrupt bits
  int slot = ahci_cmd_find(ahciPtr, port);
  // printf("%d ", slot);
  if (slot == -1)
    return false;

  HBA_CMD_HEADER *cmdheader =
      ahci_setup_cmd_header(ahciPtr, portId, slot, AHCI_CALC_PRDT(count), false);
  HBA_CMD_TBL *cmdtbl = ahci_setup_cmd_table(ahciPtr, cmdheader, portId, slot);

  // todo: look this up
  uint32_t countSec = count;
  ahci_setup_PRDT(cmdheader, cmdtbl, (uint16_t *)buff, &countSec);

  FIS_REG_H2D *cmdfis = (FIS_REG_H2D *)(&cmdtbl->cfis);

  cmdfis->fis_type = FIS_TYPE_REG_H2D;
  cmdfis->c = 1; // Command
  cmdfis->command = ATA_CMD_READ_DMA_EX;

  cmdfis->lba0 = (uint8_t)startl;
  cmdfis->lba1 = (uint8_t)(startl >> 8);
  cmdfis->lba2 = (uint8_t)(startl >> 16);
  cmdfis->device = 1 << 6; // LBA mode

  cmdfis->lba3 = (uint8_t)(startl >> 24);
  cmdfis->lba4 = (uint8_t)starth;
  cmdfis->lba5 = (uint8_t)(starth >> 8);

  cmdfis->countl = count & 0xFF;
  cmdfis->counth = (count >> 8) & 0xFF;

  if (!ahci_port_ready(port))
    return false;

  return ahci_cmd_issue(ahciPtr, port, slot);
}

bool ahci_write(ahci *ahciPtr, uint32_t portId, HBA_PORT *port, uint32_t startl,
               uint32_t starth, uint32_t count, uint8_t *buff) {
  port->is = (uint32_t)-1; // Clear pending interrupt bits
  int slot = ahci_cmd_find(ahciPtr, port);
  if (slot == -1)
    return false;

  HBA_CMD_HEADER *cmdheader =
      ahci_setup_cmd_header(ahciPtr, portId, slot, AHCI_CALC_PRDT(count), true);
  HBA_CMD_TBL *cmdtbl = ahci_setup_cmd_table(ahciPtr, cmdheader, portId, slot);

  ahci_setup_PRDT(cmdheader, cmdtbl, (uint16_t *)buff, &count);

  FIS_REG_H2D *cmdfis = (FIS_REG_H2D *)(&cmdtbl->cfis);

  cmdfis->fis_type = FIS_TYPE_REG_H2D;
  cmdfis->c = 1; // Command
  cmdfis->command = ATA_CMD_WRITE_DMA_EX;

  cmdfis->lba0 = (uint8_t)startl;
  cmdfis->lba1 = (uint8_t)(startl >> 8);
  cmdfis->lba2 = (uint8_t)(startl >> 16);
  cmdfis->device = 1 << 6; // LBA mode

  cmdfis->lba3 = (uint8_t)(startl >> 24);
  cmdfis->lba4 = (uint8_t)starth;
  cmdfis->lba5 = (uint8_t)(starth >> 8);

  cmdfis->countl = count & 0xFF;
  cmdfis->counth = (count >> 8) & 0xFF;

  if (!ahci_port_ready(port))
    return false;

  return ahci_cmd_issue(ahciPtr, port, slot);
}

void ahci_interrupt_handler(AsmPassedInterrupt *regs) {
  PCI *browse = firstPCI;
  while (browse) {
    if (browse->driver == PCI_DRIVER_AHCI) {
      ahci *ahciPtr = browse->extra;
      // printf("[pci::ahci] Interrupt hit!\n");
      for (int portNum = 0; portNum < 32; portNum++) {
        if (!(ahciPtr->sata & (1 << portNum)))
          continue;

        HBA_PORT *port = &ahciPtr->mem->ports[portNum];
        if (port->is & HBA_PxIS_TFES) {
          // Task file error
          printf("[pci::ahci] FATAL! Task file error!\n");
          Halt();
        }
        ahciPtr->mem->is = ahciPtr->mem->is;
        ahciPtr->mem->ports[portNum].is = ahciPtr->mem->ports[portNum].is;
      }
    }

    browse = browse->next;
  }
}

bool AHCI_initialize(pci_device *device) 
{

  const AHCI_DEVICE *ahciDevice = is_AHCI_controller(device);
  if (!ahciDevice)
    return false;

  printf("[pci::ahci] Detected controller! name{%s} quirks{%x}\n",
         ahciDevice->name, ahciDevice->quirks);

  pci_general_device *details =
      (pci_general_device *)malloc(sizeof(pci_general_device));
  get_general_device(device, details);
  uint32_t base = details->bar[5] & 0xFFFFFFF0;

  // Enable PCI Bus Mastering, memory access and interrupts (if not already)
  uint32_t command_status = COMBINE_WORD(device->status, device->command);
  if (!(command_status & (1 << 2)))
    command_status |= (1 << 2);
  if (!(command_status & (1 << 1)))
    command_status |= (1 << 1);
  if (command_status & (1 << 10))
    command_status |= (1 << 10);
  config_write_dword(device->bus, device->slot, device->function, PCI_COMMAND,
                   command_status);

  PCI *pci = lookup_pci_device(device);
  setup_pci_device_driver(pci, PCI_DRIVER_AHCI, PCI_DRIVER_CATEGORY_STORAGE);

  ahci *ahciPtr = (ahci *)malloc(sizeof(ahci));
  memset(ahciPtr, 0, sizeof(ahci));
  pci->extra = ahciPtr;

  HBA_MEM *mem = (HBA_MEM *)(bootloader.hhdmOffset + base); //!

  ahciPtr->bsdInfo = ahciDevice;
  ahciPtr->mem = mem;
  uint32_t size = strlen(ahciDevice->name) + 1; // null terminated
  pci->name = (char *)malloc(size);
  memcpy(pci->name, ahciDevice->name, size);

  // do a full HBA reset (as per 10.4.3)
  mem->ghc |= (1 << 0);
  while (mem->ghc & (1 << 0))
    ;
  // printf("[pci::ahci] Reset successfully!\n");

  if (!(mem->bohc & 2) || mem->cap2 & AHCI_BIOS_OWNED) 
  {
    printf("[pci::ahci] Performing BIOS->OS handoff required!\n");
    // mem->bohc |= AHCI_OS_OWNED;
    // while (mem->bohc & AHCI_OS_OWNED)
    //   ;
    // sleep(50);
    // if (mem->bohc & AHCI_BIOS_BUSY)
    //   sleep(2000);
    mem->bohc = (mem->bohc & ~8) | 2;
    while ((mem->bohc & 1))
      ;

    if ((mem->bohc & 1)) {
      // forcibly take it
      mem->bohc = 2;
      mem->bohc |= 8;
    }
  }

  if (!(mem->ghc & (1 << 31)))
    mem->ghc |= (1 << 31);

  ahci_port_probe(ahciPtr, mem);

  // enable interrupts
  pci->irqHandler = irq_install_handler(details->interruptLine, &ahci_interrupt_handler);
  if (!(mem->ghc & (1 << 1)))
    mem->ghc |= 1 << 1;

  return true;
}
