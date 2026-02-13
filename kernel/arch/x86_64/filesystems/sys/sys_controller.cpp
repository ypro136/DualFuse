#include <sys.h>

#include <liballoc.h>
#include <pci.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utility.h>
#include <hcf.hpp>


#include <framebufferutil.h>
#include <syscalls.h>

Fakefs rootSys = {0};

typedef struct PciConf {
  uint16_t bus;
  uint8_t  slot;
  uint8_t  function;
} PciConf;

int pci_config_read(OpenFile *fd, uint8_t *out, size_t limit) {
  FakefsFile *file = (FakefsFile *)fd->fake_file_system;
  PciConf    *conf = (PciConf *)file->extra;

  if (fd->pointer >= 4096)
    return 0;
  int toCopy = 4096 - fd->pointer;
  if (toCopy > limit)
    toCopy = limit;

  for (int i = 0; i < toCopy; i++) {
    uint16_t word =
        config_read_word(conf->bus, conf->slot, conf->function, fd->pointer++);
    out[i] = EXPORT_BYTE(word, true);
  }

  return toCopy;
}

VfsHandlers handlePciConfig = {
                               .read = pci_config_read,
                               .write = 0,
                               .ioctl = 0,
                               .stat = fakefsFstat,
                               .mmap = 0,
                               .getdents64 = 0,
                               .duplicate = 0
                               };



void sys_setup_pci(FakefsFile *devices) {
  pci_device        *device = (pci_device *)malloc(sizeof(pci_device));
  pci_general_device *out = (pci_general_device *)malloc(sizeof(pci_general_device));

  for (uint16_t bus = 0; bus < PCI_MAX_BUSES; bus++) {
    for (uint8_t slot = 0; slot < PCI_MAX_DEVICES; slot++) {
      for (uint8_t function = 0; function < PCI_MAX_FUNCTIONS; function++) {
        if (!Filter_device(bus, slot, function))
          continue;

        get_device(device, bus, slot, function);
        get_general_device(device, out);

        char *dirname = (char *)malloc(128);
        sprintf(dirname, "0000:%02d:%02d.%d", bus, slot, function);

        PciConf *pciconf = (PciConf *)malloc(sizeof(PciConf));
        pciconf->bus = bus;
        pciconf->slot = slot;
        pciconf->function = function;

        FakefsFile *dir =
            fakefsAddFile(&rootSys, devices, dirname, 0,
                          S_IFDIR | S_IRUSR | S_IWUSR, &fakefsRootHandlers);

        // [..]/config
        FakefsFile *confFile =
            fakefsAddFile(&rootSys, dir, "config", 0,
                          S_IFREG | S_IRUSR | S_IWUSR, &handlePciConfig);
        fakefsAttachFile(confFile, pciconf, 4096);

        // [..]/vendor
        char *vendorStr = (char *)malloc(8);
        sprintf(vendorStr, "0x%04x\n", device->vendor_id);
        FakefsFile *vendorFile = fakefsAddFile(&rootSys, dir, "vendor", 0,
                                               S_IFREG | S_IRUSR | S_IWUSR,
                                               &fakefsSimpleReadHandlers);
        fakefsAttachFile(vendorFile, vendorStr, 4096);

        // [..]/irq
        char *irqStr = (char *)malloc(8);
        sprintf(irqStr, "%d\n", out->interruptLine);
        FakefsFile *irqFile =
            fakefsAddFile(&rootSys, dir, "irq", 0, S_IFREG | S_IRUSR | S_IWUSR,
                          &fakefsSimpleReadHandlers);
        fakefsAttachFile(irqFile, irqStr, 4096);

        // [..]/device
        char *deviceStr = (char *)malloc(8);
        sprintf(deviceStr, "0x%04x\n", device->device_id);
        FakefsFile *deviceFile = fakefsAddFile(&rootSys, dir, "device", 0,
                                               S_IFREG | S_IRUSR | S_IWUSR,
                                               &fakefsSimpleReadHandlers);
        fakefsAttachFile(deviceFile, deviceStr, 4096);

        // [..]/class
        uint32_t class_code = config_read_word(device->bus, device->slot,
                                             device->function, PCI_SUBCLASS)
                              << 8;
        class_code |= ((uint32_t)device->progIF) << 8;
        char *classStr = (char *)malloc(16);
        sprintf(classStr, "0x%x\n", class_code);
        FakefsFile *device_class = fakefsAddFile(&rootSys, dir, "class", 0,
                                          S_IFREG | S_IRUSR | S_IWUSR,
                                          &fakefsSimpleReadHandlers);
        fakefsAttachFile(device_class, classStr, 4096);
      }
    }
  }

  free(device);
  free(out);
}

void sys_setup() {
  FakefsFile *bus =
      fakefsAddFile(&rootSys, (FakefsFile *)&rootSys.rootFile, "bus", 0,
                    S_IFDIR | S_IRUSR | S_IWUSR, &fakefsRootHandlers);
  FakefsFile *pci =
      fakefsAddFile(&rootSys, bus, "pci", 0, S_IFDIR | S_IRUSR | S_IWUSR,
                    &fakefsRootHandlers);
  FakefsFile *devices =
      fakefsAddFile(&rootSys, pci, "devices", 0, S_IFDIR | S_IRUSR | S_IWUSR,
                    &fakefsRootHandlers);

  sys_setup_pci(devices);
}

bool sys_mount(MountPoint *mount) {
  // install handlers
  mount->handlers = &fakefsHandlers;
  mount->stat = fakefsStat;
  mount->lstat = fakefsLstat;

  mount->fsInfo = malloc(sizeof(FakefsOverlay));
  memset(mount->fsInfo, 0, sizeof(FakefsOverlay));
  FakefsOverlay *sys = (FakefsOverlay *)mount->fsInfo;

  sys->fakefs = &rootSys;
  if (!rootSys.rootFile) {
    fakefsSetupRoot(&rootSys.rootFile);
    sys_setup();
  }

  return true;
}
