#include <disk.h>

#include <ahci.h>
#include <liballoc.h>
#include <system.h>
#include <pci.h>
#include <types.h>


#include <utility.h>
#include <hcf.hpp>

// Multiple disk handler

uint16_t mbr_partition_indexes[] = {MBR_PARTITION_1, MBR_PARTITION_2,
                                    MBR_PARTITION_3, MBR_PARTITION_4};

bool open_disk(uint32_t disk, uint8_t partition, mbr_partition *out) 
{
  if (partition >= 4) return false; // Prevent out-of-bounds
  uint8_t *rawArr = (uint8_t *)malloc(SECTOR_SIZE);
  if (!rawArr) return false;
  get_disk_bytes(rawArr, 0x0, 1);
  bool ret = validate_mbr(rawArr);
  if (!ret) {
    free(rawArr);
    return false;
  }
  memcpy(out, (void *)((size_t)rawArr + mbr_partition_indexes[partition]),
         sizeof(mbr_partition));
  free(rawArr);
  return true;
}

bool validate_mbr(uint8_t *mbrSector) {
  return mbrSector[510] == 0x55 && mbrSector[511] == 0xaa;
}

void disk_bytes(uint8_t *target_address, uint32_t LBA, uint32_t sector_count,
               bool write) {
  PCI *browse = firstPCI;
  while (browse) {
    if (browse->driver == PCI_DRIVER_AHCI && browse->extra && ((ahci *)browse->extra)->sata)
      break;
    browse = browse->next;
  }

  if (!browse || !browse->extra) {
    memset(target_address, 0, sector_count * SECTOR_SIZE);
    return;
  }

  ahci *target = (ahci *)browse->extra;
  if (!target->mem) {
    memset(target_address, 0, sector_count * SECTOR_SIZE);
    return;
  }
  int   pos = 0;
  while (!(target->sata & (1 << pos)))
    pos++;

  if (write)
    ahci_write(target, pos, &target->mem->ports[pos], LBA, 0, sector_count,
              target_address);
  else
    ahci_read(target, pos, &target->mem->ports[pos], LBA, 0, sector_count,
             target_address);
}

// todo: allow concurrent stuff
force_inline void disk_bytes_max(uint8_t *target_address, uint32_t LBA,
                               size_t sector_count, bool write) {
  // calculated by: (bytesPerPRDT * PRDTamnt) / SECTOR_SIZE
  //                (    4MiB     *     8   ) /     512
  int max = 65536;
  size_t total_bytes = sector_count * SECTOR_SIZE;
  // Add a debug print here to check the buffer
  // printf("disk_bytes_max: target_address=%p, total_bytes=%zu\n", target_address, total_bytes);

  size_t chunks = sector_count / max;
  size_t remainder = sector_count % max;
  if (chunks)
    for (size_t i = 0; i < chunks; i++)
      disk_bytes(target_address + i * max * SECTOR_SIZE, LBA + i * max, max, write);

  if (remainder)
    disk_bytes(target_address + chunks * max * SECTOR_SIZE, LBA + chunks * max,
              remainder, write);

  // for (int i = 0; i < sector_count; i++)
  //   disk_bytes(target_address + i * 512, LBA + i, 1, false);

  // disk_bytes(target_address, LBA, sector_count, false);
}

void get_disk_bytes(uint8_t *target_address, uint32_t LBA, size_t sector_count) 
{
  return disk_bytes_max(target_address, LBA, sector_count, false);
}

void set_disk_bytes(const uint8_t *target_address, uint32_t LBA,
                  size_t sector_count) {
  // bad solution but idc, my code is safe
  uint8_t *rw_target_address = (uint8_t *)((size_t)target_address);
  return disk_bytes_max(rw_target_address, LBA, sector_count, true);
}
