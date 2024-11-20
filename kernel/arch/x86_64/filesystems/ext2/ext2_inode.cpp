#include <ext2.h>
#include <liballoc.h>
#include <system.h>
#include <utility.h>

Ext2Inode *ext2_inode_fetch(Ext2 *ext2, size_t inode) {
  spinlock_cnt_read_acquire(&ext2->WLOCK_INODE);
  uint32_t group = INODE_TO_BLOCK_GROUP(ext2, inode);
  uint32_t index = INODE_TO_INDEX(ext2, inode);

  size_t leftovers = index * ext2->inodeSize;
  size_t leftoversLba = leftovers / SECTOR_SIZE;
  size_t leftoversRem = leftovers % SECTOR_SIZE;

  // large enough just in case
  size_t len = CEILING_DIVISION(ext2->inodeSize * 4, SECTOR_SIZE) * SECTOR_SIZE;
  size_t lba =
      BLOCK_TO_LBA(ext2, 0, ext2->bgdts[group].inode_table) + leftoversLba;

  uint8_t *buf = (uint8_t *)malloc(len);
  get_disk_bytes(buf, lba, len / SECTOR_SIZE);
  Ext2Inode *tmp = (Ext2Inode *)(buf + leftoversRem);

  Ext2Inode *ret = (Ext2Inode *)malloc(ext2->inodeSize);
  memcpy(ret, tmp, ext2->inodeSize);

  free(buf);
  spinlock_cnt_read_release(&ext2->WLOCK_INODE);
  return ret;
}

// IMPORTANT! Remember to manually set the lock **before** calling
void ext2_inode_modifyM(Ext2 *ext2, size_t inode, Ext2Inode *target) {
  spinlock_cnt_write_acquire(&ext2->WLOCK_INODE);
  uint32_t group = INODE_TO_BLOCK_GROUP(ext2, inode);
  uint32_t index = INODE_TO_INDEX(ext2, inode);

  size_t leftovers = index * ext2->inodeSize;
  size_t leftoversLba = leftovers / SECTOR_SIZE;
  size_t leftoversRem = leftovers % SECTOR_SIZE;

  // large enough just in case
  size_t len = CEILING_DIVISION(ext2->inodeSize * 4, SECTOR_SIZE) * SECTOR_SIZE;
  size_t lba =
      BLOCK_TO_LBA(ext2, 0, ext2->bgdts[group].inode_table) + leftoversLba;

  uint8_t *buf = (uint8_t *)malloc(len);
  get_disk_bytes(buf, lba, len / SECTOR_SIZE);
  Ext2Inode *tmp = (Ext2Inode *)(buf + leftoversRem);
  memcpy(tmp, target, sizeof(Ext2Inode));
  set_disk_bytes(buf, lba, len / SECTOR_SIZE);

  free(buf);
  spinlock_cnt_write_release(&ext2->WLOCK_INODE);
}

uint32_t ext2_inode_find(Ext2 *ext2, int groupSuggestion) {
  if (ext2->superblock.free_inodes < 1){
    printf("[ext2] FATAL! Couldn't find a single inode! Drive is full!\n");
    Halt();
    return 0;
  }

  uint32_t suggested = ext2_inode_findL(ext2, groupSuggestion);
  if (suggested)
    return suggested;

  for (int i = 0; i < ext2->blockGroups; i++) {
    if (i == groupSuggestion)
      continue;

    uint32_t ret = ext2_inode_findL(ext2, i);
    if (ret)
      return ret;
  }

  printf("[ext2] FATAL! Couldn't find a single inode! Drive is full!\n");
  Halt();
  return 0;
}

uint32_t ext2_inode_findL(Ext2 *ext2, int group) {
  if (ext2->bgdts[group].free_inodes < 1)
    return 0;

  spinlock_acquire(&ext2->LOCKS_INODE_BITMAP[group]);

  uint32_t ret = 0;
  uint8_t *buff = malloc(ext2->blockSize);

  get_disk_bytes(buff, BLOCK_TO_LBA(ext2, 0, ext2->bgdts[group].inode_bitmap),
               ext2->blockSize / SECTOR_SIZE);

  int firstInodeDiv = 0;
  int firstInodeRem = 0;

  if (group == 0) {
    firstInodeDiv = ext2->superblock.extended.first_inode / 8;
    firstInodeRem = ext2->superblock.extended.first_inode % 8;
  }

  for (int i = firstInodeDiv; i < ext2->blockSize; i++) {
    if (buff[i] == 0xff)
      continue;
    for (int j = (i == firstInodeDiv ? firstInodeRem : 0); j < 8; j++) {
      if (!(buff[i] & (1 << j))) {
        ret = i * 8 + j;
        goto cleanup;
      }
    }
  }

cleanup:
  if (ret) {
    // we found blocks successfully, mark them as allocated
    uint32_t where = ret / 8;
    uint32_t remainder = ret % 8;
    buff[where] |= (1 << remainder);
    set_disk_bytes(buff, BLOCK_TO_LBA(ext2, 0, ext2->bgdts[group].inode_bitmap),
                 ext2->blockSize / SECTOR_SIZE);

    // set the bgdt accordingly
    spinlock_acquire(&ext2->LOCK_BGDT_WRITE);
    ext2->bgdts[group].free_inodes--;
    ext2_bgdt_pushM(ext2);
    spinlock_release(&ext2->LOCK_BGDT_WRITE);

    // and the superblock
    spinlock_acquire(&ext2->LOCK_SUPERBLOCK_WRITE);
    ext2->superblock.free_inodes--;
    ext2_superblock_pushM(ext2);
    spinlock_release(&ext2->LOCK_SUPERBLOCK_WRITE);
  }

  spinlock_release(&ext2->LOCKS_INODE_BITMAP[group]);
  // +1 necessary because inodes start at inode number 1
  return ret ? (group * ext2->superblock.inodes_per_group + ret + 1) : 0;
}
