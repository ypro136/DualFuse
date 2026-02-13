#include <ext2.h>
#include <liballoc.h>
#include <system.h>
#include <utility.h>
#include <hcf.hpp>

Ext2Inode *ext2InodeFetch(Ext2 *ext2, size_t inode) {
  uint32_t group = INODE_TO_BLOCK_GROUP(ext2, inode);
  uint32_t index = INODE_TO_INDEX(ext2, inode);

  spinlock_cnt_read_acquire(&ext2->WLOCKS_INODE[group]);

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
  spinlock_cnt_read_release(&ext2->WLOCKS_INODE[group]);
  return ret;
}

// IMPORTANT! Remember to manually set the lock **before** calling
void ext2InodeModifyM(Ext2 *ext2, size_t inode, Ext2Inode *target) {
  uint32_t group = INODE_TO_BLOCK_GROUP(ext2, inode);
  uint32_t index = INODE_TO_INDEX(ext2, inode);

  spinlock_cnt_write_acquire(&ext2->WLOCKS_INODE[group]);

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
  spinlock_cnt_write_release(&ext2->WLOCKS_INODE[group]);
}

void ext2InodeDelete(Ext2 *ext2, size_t inode) {
  uint32_t group = INODE_TO_BLOCK_GROUP(ext2, inode);
  uint32_t index = INODE_TO_INDEX(ext2, inode);

  spinlock_cnt_write_acquire(&ext2->WLOCKS_INODE[group]);

  uint32_t where = index / 8;
  uint32_t remainder = index % 8;

  size_t lba = BLOCK_TO_LBA(ext2, 0, ext2->bgdts[group].inode_bitmap);

  uint8_t *buf = (uint8_t *)malloc(ext2->blockSize);
  get_disk_bytes(buf, lba, ext2->blockSize / SECTOR_SIZE);
  assert(buf[where] & (1 << remainder));
  buf[where] &= ~(1 << remainder);
  set_disk_bytes(buf, lba, ext2->blockSize / SECTOR_SIZE);

  free(buf);

  // set the bgdt accordingly
  spinlock_acquire(&ext2->LOCK_BGDT_WRITE);
  ext2->bgdts[group].free_inodes++;
  ext2BgdtPushM(ext2);
  spinlock_release(&ext2->LOCK_BGDT_WRITE);

  // and the superblock
  spinlock_acquire(&ext2->LOCK_SUPERBLOCK_WRITE);
  ext2->superblock.free_inodes++;
  ext2SuperblockPushM(ext2);
  spinlock_release(&ext2->LOCK_SUPERBLOCK_WRITE);

  spinlock_cnt_write_release(&ext2->WLOCKS_INODE[group]);
}

uint32_t ext2InodeFind(Ext2 *ext2, int groupSuggestion) {
  if (ext2->superblock.free_inodes >= 1) {

    uint32_t suggested = ext2InodeFindL(ext2, groupSuggestion);
    if (suggested)
      return suggested;

    for (int i = 0; i < ext2->blockGroups; i++) {
      if (i == groupSuggestion)
        continue;

      uint32_t ret = ext2InodeFindL(ext2, i);
      if (ret)
        return ret;
    }
  }

  // Failure path
  printf("[ext2] FATAL! Couldn't find a single inode! Drive is full!\n");
  Halt();
  return 0;
}


uint32_t ext2InodeFindL(Ext2 *ext2, int group) {
  if (ext2->bgdts[group].free_inodes < 1)
    return 0;

  spinlock_cnt_write_acquire(&ext2->WLOCKS_INODE[group]);

  uint32_t ret = 0;
  uint8_t *buff = (uint8_t *)malloc(ext2->blockSize);

  get_disk_bytes(
      buff,
      BLOCK_TO_LBA(ext2, 0, ext2->bgdts[group].inode_bitmap),
      ext2->blockSize / SECTOR_SIZE
  );

  int firstInodeDiv = 0;
  int firstInodeRem = 0;

  if (group == 0) {
    firstInodeDiv = ext2->superblock.extended.first_inode / 8;
    firstInodeRem = ext2->superblock.extended.first_inode % 8;
  }

  bool found = false;

  for (int i = firstInodeDiv; i < ext2->blockSize && !found; i++) {
    if (buff[i] == 0xff)
      continue;

    for (int j = (i == firstInodeDiv ? firstInodeRem : 0); j < 8; j++) {
      if (!(buff[i] & (1 << j))) {
        ret = i * 8 + j;
        found = true;
        break;
      }
    }
  }

  if (ret) {
    // mark inode as allocated
    uint32_t where = ret / 8;
    uint32_t remainder = ret % 8;
    buff[where] |= (1 << remainder);

    set_disk_bytes(
        buff,
        BLOCK_TO_LBA(ext2, 0, ext2->bgdts[group].inode_bitmap),
        ext2->blockSize / SECTOR_SIZE
    );

    // update BGDT
    spinlock_acquire(&ext2->LOCK_BGDT_WRITE);
    ext2->bgdts[group].free_inodes--;
    ext2BgdtPushM(ext2);
    spinlock_release(&ext2->LOCK_BGDT_WRITE);

    // update superblock
    spinlock_acquire(&ext2->LOCK_SUPERBLOCK_WRITE);
    ext2->superblock.free_inodes--;
    ext2SuperblockPushM(ext2);
    spinlock_release(&ext2->LOCK_SUPERBLOCK_WRITE);
  }

  spinlock_cnt_write_release(&ext2->WLOCKS_INODE[group]);

  // +1 because inode numbering starts at 1
  return ret ? (group * ext2->superblock.inodes_per_group + ret + 1) : 0;
}

