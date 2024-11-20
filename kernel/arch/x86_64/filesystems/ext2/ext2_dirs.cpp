#include <dents.h>
#include <ext2.h>
#include <liballoc.h>
#include <system.h>
#include <timer.h>
#include <utility.h>

bool ext2_air_allocate(Ext2 *ext2, uint32_t inodeNum, Ext2Inode *parentDirInode,
                     char *filename, uint8_t filenameLen, uint8_t type,
                     uint32_t inode) {
  spinlock_acquire(&ext2->LOCK_DIRALLOC);
  int entryLen = sizeof(Ext2Directory) + filenameLen;

  Ext2Inode *ino = parentDirInode; // <- todo
  uint8_t   *names = (uint8_t *)malloc(ext2->blockSize);

  Ext2LookupControl control = {0};
  size_t            blockNum = 0;

  bool ret = false;
  ext2_block_fetch_init(ext2, &control);

  int blocksContained = CEILING_DIVISION(ino->size, ext2->blockSize);
  for (int i = 0; i < blocksContained; i++) {
    size_t block = ext2_block_fetch(ext2, ino, &control, blockNum);
    if (!block)
      break;
    blockNum++;
    Ext2Directory *dir = (Ext2Directory *)names;

    get_disk_bytes(names, BLOCK_TO_LBA(ext2, 0, block),
                 ext2->blockSize / SECTOR_SIZE);

    while (((size_t)dir - (size_t)names) < ext2->blockSize) {
      if (dir->inode && filenameLen == dir->filenameLength &&
          memcmp(dir->filename, filename, filenameLen) == 0) {
        ret = false;
        ext2_block_fetch_cleanup(&control);
        free(names);
        spinlock_release(&ext2->LOCK_DIRALLOC);

        return ret;
      }
      int minForOld = (sizeof(Ext2Directory) + dir->filenameLength + 3) & ~3;
      int minForNew = (entryLen + 3) & ~3;

      int remainderForNew = dir->size - minForOld;
      if (remainderForNew < minForNew)
        dir = (void *)((size_t)dir + dir->size);


      // means we now have enough space to put the new one in

      dir->size = minForOld;
      Ext2Directory *new_ext2_directory = (void *)((size_t)dir + dir->size);
      new_ext2_directory->size = remainderForNew;

      new_ext2_directory->type = type;
      memcpy(new_ext2_directory->filename, filename, filenameLen);
      new_ext2_directory->filenameLength = filenameLen;
      new_ext2_directory->inode = inode;

      set_disk_bytes(names, BLOCK_TO_LBA(ext2, 0, block),
                   ext2->blockSize / SECTOR_SIZE);

      ret = true;
      ext2_block_fetch_cleanup(&control);
      free(names);
      spinlock_release(&ext2->LOCK_DIRALLOC);

      return ret;

      dir = (void *)((size_t)dir + dir->size);
    }
  }

  // means we need to allocate another block for these
  uint32_t group = INODE_TO_BLOCK_GROUP(ext2, inodeNum);
  uint32_t newBlock = ext2_block_find(ext2, group, 1);

  uint8_t *newBlockBuff = names; // reuse names :p
  get_disk_bytes(newBlockBuff, BLOCK_TO_LBA(ext2, 0, newBlock),
               ext2->blockSize / SECTOR_SIZE);

  Ext2Directory *new_ext2_directory = (Ext2Directory *)(newBlockBuff);
  new_ext2_directory->size = ext2->blockSize;
  new_ext2_directory->type = type;
  memcpy(new_ext2_directory->filename, filename, filenameLen);
  new_ext2_directory->filenameLength = filenameLen;
  new_ext2_directory->inode = inode;

  set_disk_bytes(newBlockBuff, BLOCK_TO_LBA(ext2, 0, newBlock),
               ext2->blockSize / SECTOR_SIZE);
  ext2_block_assign(ext2, ino, inodeNum, &control, blockNum, newBlock);

  ino->num_sectors += ext2->blockSize / SECTOR_SIZE;
  ino->size += ext2->blockSize;
  ext2_inode_modifyM(ext2, inodeNum, ino);

  ret = true;

  ext2_block_fetch_cleanup(&control);
  free(names);
  spinlock_release(&ext2->LOCK_DIRALLOC);

  return ret;
}
int ext2_get_dents64(OpenFile *file, struct linux_dirent64 *start, unsigned int hardlimit) {
  Ext2       *ext2 = EXT2_PTR(file->mountPoint->fsInfo);
  Ext2OpenFd *edir = EXT2_DIR_PTR(file->dir);

  if ((edir->inode.permission & 0xF000) != EXT2_S_IFDIR)
    return -ENOTDIR;

  int        allocatedlimit = 0;
  Ext2Inode *ino = &edir->inode;
  uint8_t   *names = (uint8_t *)malloc(ext2->blockSize);

  struct linux_dirent64 *dirp = (struct linux_dirent64 *)start;

  int blocksContained = CEILING_DIVISION(ino->size, ext2->blockSize);
  for (int i = 0; i < blocksContained; i++) {
    size_t block =
        ext2_block_fetch(ext2, ino, &edir->lookup, edir->ptr / ext2->blockSize);
    if (!block)
      break;
    Ext2Directory *dir =
        (Ext2Directory *)((size_t)names + (edir->ptr % ext2->blockSize));

    get_disk_bytes(names, BLOCK_TO_LBA(ext2, 0, block),
                 ext2->blockSize / SECTOR_SIZE);

    while (((size_t)dir - (size_t)names) < ext2->blockSize) {
      if (!dir->inode)
        continue;

      unsigned char type = 0;
      if (dir->type == 2)
        type = CDT_DIR;
      else if (dir->type == 7)
        type = CDT_LNK;
      else
        type = CDT_REG;

      DENTS_RES res =
          dents_add(start, &dirp, &allocatedlimit, hardlimit, dir->filename,
                   dir->filenameLength, dir->inode, type);

      if (res == DENTS_NO_SPACE) {
        allocatedlimit = -EINVAL;
        goto cleanup;
      } else if (res == DENTS_RETURN)
        goto cleanup;

      edir->ptr += dir->size;
      dir = (void *)((size_t)dir + dir->size);
    }

    int rem = edir->ptr % ext2->blockSize;
    if (rem)
      edir->ptr += ext2->blockSize - rem;
  }

cleanup:
  free(names);
  return allocatedlimit;
}