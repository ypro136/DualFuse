#include <fat32.h>
#include <liballoc.h>
#include <system.h>

#include <utility.h>
#include <hcf.hpp>


uint32_t fat32_FAT_cache_lookup(FAT32 *fat, uint32_t offset) {
  for (int i = 0; i < FAT32_CACHE_MAX; i++) {
    if (fat->cacheBase[i] == offset)
      return i;
  }

  return FAT32_CACHE_BAD;
}

void fat32_FAT_cache_add(FAT32 *fat, uint32_t offset, uint8_t *bytes) {
  if (fat->cacheCurr >= FAT32_CACHE_MAX)
    fat->cacheCurr = 0;
  fat->cacheBase[fat->cacheCurr] = offset;
  memcpy(fat->cache[fat->cacheCurr], bytes, SECTOR_SIZE);
  fat->cacheCurr++;
}

void fat32_FAT_fetch(FAT32 *fat, uint32_t offsetSector, uint8_t *bytes) {
  uint32_t cacheRes = fat32_FAT_cache_lookup(fat, offsetSector);
  if (cacheRes != FAT32_CACHE_BAD) {
    memcpy(bytes, fat->cache[cacheRes], SECTOR_SIZE);
    return;
  }
  get_disk_bytes(bytes, offsetSector, 1);
  fat32_FAT_cache_add(fat, offsetSector, bytes);
}

uint32_t fat32_FAT_traverse(FAT32 *fat, uint32_t offset) {
  int bytesPerCluster = SECTOR_SIZE;

  uint32_t offsetFAT = offset * 4; // every entry is sizeof(uint32_t) = 4
  uint32_t offsetSector = fat->offsetFats + (offsetFAT / bytesPerCluster);
  uint32_t offsetEntry = offsetFAT % bytesPerCluster;

  uint8_t *bytes = (uint8_t *)malloc(bytesPerCluster);
  fat32_FAT_fetch(fat, offsetSector, bytes);

  uint32_t *retLocation = (uint32_t *)(&bytes[offsetEntry]);
  uint32_t  ret = (*retLocation) & 0x0FFFFFFF; // remember; we're on FAT32
  free(bytes);

  if (ret >= 0x0FFFFFF8) // end of cluster chain
    return 0;

  if (ret == 0x0FFFFFF7) // invalid/bad cluster
    return 0;

  return ret;
}

// +1 for starting
uint32_t *fat32_FAT_chain(FAT32 *fat, uint32_t offsetStart, uint32_t amount) {
  uint32_t *ret = malloc((amount + 1) * sizeof(uint32_t));
  memset(ret, 0, (amount + 1) * sizeof(uint32_t));

  ret[0] = offsetStart;
  for (uint32_t i = 1; i < (amount + 1); i++)
    ret[i] = fat32_FAT_traverse(fat, ret[i - 1]);

  return ret;
}
