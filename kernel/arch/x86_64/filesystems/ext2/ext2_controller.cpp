#include <ext2.h>

#include <bootloader.h>
#include <liballoc.h>
#include <paging.h>
#include <string.h>
#include <system.h>
#include <task.h>
#include <timer.h>
#include <spinlock.h>


#include <utility.h>
#include <hcf.hpp>


bool ext2_mount(MountPoint *mount) {
  // assign handlers
  mount->handlers = &ext2Handlers;
  mount->stat = ext2_stat;
  mount->lstat = ext2_lstat;

  mount->mkdir = ext2_mkdir;

  // assign fsInfo
  mount->fsInfo = malloc(sizeof(Ext2));
  memset(mount->fsInfo, 0, sizeof(Ext2));
  Ext2 *ext2 = EXT2_PTR(mount->fsInfo);

  // base offset
  ext2->offsetBase = mount->mbr.lba_first_sector;
  ext2->offsetSuperblock = mount->mbr.lba_first_sector + 2;

  // get superblock
  uint8_t tmp[sizeof(Ext2Superblock)] = {0};
  get_disk_bytes(tmp, ext2->offsetSuperblock, 2);

  // store it
  memcpy(&ext2->superblock, tmp, sizeof(Ext2Superblock));

  // checks
  if (ext2->superblock.ext2_magic != 0xEF53) {
    printf("[ext2] Invalid magic number!\n");
    free(ext2);
    return false;
  }

  if (ext2->superblock.major < 1) {
    printf(
        "[ext2] FATAL! Ancient, pre-historic ext2 partition discovered! Please "
        "contact your local museum for further info...\n");
    free(ext2);
    return false;
  }

  if (ext2->superblock.extended.required_feature != EXT2_R_F_TYPE_FIELD) {
    printf("[ext2] FATAL! Unsupported flags detected: compression{%d} type{%d} "
           "replay{%d} device{%d}\n",
           ext2->superblock.extended.required_feature & EXT2_R_F_COMPRESSION,
           ext2->superblock.extended.required_feature & EXT2_R_F_TYPE_FIELD,
           ext2->superblock.extended.required_feature & EXT2_R_F_JOURNAL_REPLAY,
           ext2->superblock.extended.required_feature &
               EXT2_R_F_JOURNAL_DEVICE);
    free(ext2);
    return false;
  }

  if (ext2->superblock.fs_state != EXT2_FS_S_CLEAN) {
    if (ext2->superblock.err == EXT2_FS_E_REMOUNT_RO) {
      printf("[ext2] FATAL! Read-only partition!\n");
      free(ext2);
      return false;
    } else if (ext2->superblock.err == EXT2_FS_E_KHalt) {
      printf("[ext2] FATAL! Superblock error caused Halt!\n");
      Halt();
    }
  }

  // log2.. why???!
  ext2->blockSize = 1024 << ext2->superblock.log2block_size;

  if ((ext2->blockSize % SECTOR_SIZE) != 0) {
    printf("[ext2] FATAL! Block size is not sector-aligned! blockSize{%d}\n",
           ext2->blockSize);
    free(ext2);
    return false;
  }

  // calculate block groups
  uint64_t blockGroups1 = CEILING_DIVISION(ext2->superblock.total_blocks,
                                     ext2->superblock.blocks_per_group);
  uint64_t blockGroups2 = CEILING_DIVISION(ext2->superblock.total_inodes,
                                     ext2->superblock.inodes_per_group);
  if (blockGroups1 != blockGroups2) {
    printf("[ext2] Total block group calculation doesn't match up! 1{%ld} "
           "2{%ld}\n",
           blockGroups1, blockGroups2);
    free(ext2);
    return false;
  }
  ext2->blockGroups = blockGroups1;

  // find the Block Group Descriptor Table
  // remember, very max is block size
  ext2->offsetBGDT = BLOCK_TO_LBA(ext2, 0, ext2->superblock.superblock_idx + 1);
  ext2->bgdts = (Ext2BlockGroup *)malloc(ext2->blockSize);
  get_disk_bytes((void *)ext2->bgdts, ext2->offsetBGDT,
               CEILING_DIVISION(ext2->blockSize, SECTOR_SIZE));

  // set up counting spinlocks for the BGDTs
  // remember to zero them, just in case
  int bgdtLockSize = sizeof(Spinlock) * ext2->blockGroups;
  ext2->LOCKS_BLOCK_BITMAP = (Spinlock *)malloc(bgdtLockSize);
  memset(ext2->LOCKS_BLOCK_BITMAP, 0, bgdtLockSize);

  ext2->LOCKS_INODE_BITMAP = (Spinlock *)malloc(bgdtLockSize);
  memset(ext2->LOCKS_INODE_BITMAP, 0, bgdtLockSize);

  ext2->inodeSize = ext2->superblock.extended.inode_size;
  ext2->inodeSizeRounded =
      CEILING_DIVISION(ext2->inodeSize, SECTOR_SIZE) * SECTOR_SIZE;

  return true;

}

int ext2_open(char *filename, int flags, int mode, OpenFile *fd,
             char **symlinkResolve) {
  Ext2 *ext2 = EXT2_PTR(fd->mountPoint->fsInfo);

  uint32_t inode =
      ext2_traverse_path(ext2, filename, EXT2_ROOT_INODE, true, symlinkResolve);

  if (!inode && *symlinkResolve) {
    // last entry is a soft symlink that'll have to be resolved back on the
    // open() phase..
    if (flags & O_NOFOLLOW)
      return -ELOOP;
    else
      return -ENOENT;
  }

  if (inode && flags & O_EXCL && flags & O_CREAT)
    return -EEXIST;

  if (!inode) {
    if (flags & O_CREAT) {
      // create this thing
      int ret = ext2_touch(fd->mountPoint, filename, mode, symlinkResolve);
      // printf("creation: %d\n", ret);
      if (ret < 0)
        return ret;

      // created successfully
      inode = ext2_traverse_path(ext2, filename, EXT2_ROOT_INODE, true,
                               symlinkResolve);
    } else
      return -ENOENT;
  }

  Ext2Inode *inodeFetched = ext2_inode_fetch(ext2, inode);
  if (flags & O_DIRECTORY && !(inodeFetched->permission & S_IFDIR)) {
    free(inodeFetched);
    return -ENOTDIR;
  }

  if (flags & O_TRUNC) {
    inodeFetched->size = 0;
    inodeFetched->size_high = 0;
    inodeFetched->num_sectors = 0;

    ext2_inode_modifyM(ext2, inode, inodeFetched);
  }

  Ext2OpenFd *dir = (Ext2OpenFd *)malloc(sizeof(Ext2OpenFd));
  memset(dir, 0, sizeof(Ext2OpenFd));
  fd->dir = dir;

  dir->inodeNum = inode;
  memcpy(&dir->inode, inodeFetched, sizeof(Ext2Inode));

  if ((dir->inode.permission & 0xF000) == EXT2_S_IFDIR) {
    size_t len = strlen(filename) + 1;
    fd->dirname = malloc(len);
    memcpy(fd->dirname, filename, len);
  }

  ext2_block_fetch_init(ext2, &dir->lookup);

  // pointers & stuff
  dir->ptr = 0;

  free(inodeFetched);
  return 0;
}

int ext2_read(OpenFile *fd, uint8_t *buff, size_t naiveLimit) {
  Ext2       *ext2 = EXT2_PTR(fd->mountPoint->fsInfo);
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);

  size_t filesize = ext2_get_filesize(fd);
  if (dir->ptr >= filesize)
    return 0;

  size_t limit = naiveLimit;
  if (limit > (filesize - dir->ptr))
    limit = filesize - dir->ptr;

  size_t    blocksRequired = CEILING_DIVISION(limit, ext2->blockSize);
  uint32_t *blocks =
      ext2_block_chain(ext2, dir, dir->ptr / ext2->blockSize, blocksRequired);

  uint8_t *tmp = (uint8_t *)calloc((blocksRequired + 1) * ext2->blockSize, 1);
  int      currBlock = 0;

  // optimization: we can use consecutive sectors to make our life easier
  int consecStart = -1;
  int consecEnd = 0;

  // +1 for starting
  for (int i = 0; i < (blocksRequired + 1); i++) {
    if (!blocks[i])
      break;
    bool last = i == (blocksRequired - 1);
    if (consecStart < 0) {
      // nothing consecutive yet
      if (!last && blocks[i + 1] == (blocks[i] + 1)) {
        // consec starts here
        consecStart = i;
        continue;
      }
    } else {
      // we are in a consecutive that started since consecStart
      if (last || blocks[i + 1] != (blocks[i] + 1))
        consecEnd = i; // either last or the end
      else             // otherwise, we good
        continue;
    }

    if (consecEnd) {
      // optimized consecutive cluster reading
      int needed = consecEnd - consecStart + 1;
      get_disk_bytes(&tmp[currBlock * ext2->blockSize],
                   BLOCK_TO_LBA(ext2, 0, blocks[consecStart]),
                   (needed * ext2->blockSize) / SECTOR_SIZE);
      currBlock += needed;
    } else {
      get_disk_bytes(&tmp[currBlock * ext2->blockSize],
                   BLOCK_TO_LBA(ext2, 0, blocks[i]),
                   ext2->blockSize / SECTOR_SIZE);
      currBlock++;
    }

    // traverse
    consecStart = -1;
    consecEnd = 0;
  }

  // actually fill the buffer
  uint32_t offsetStarting = dir->ptr % ext2->blockSize; // remainder
  size_t   headtoCopy = MIN(ext2->blockSize - offsetStarting, limit);
  memcpy(buff, &tmp[offsetStarting], headtoCopy);
  if (limit > headtoCopy) {
    memcpy(&buff[headtoCopy], &tmp[ext2->blockSize], limit - headtoCopy);
  }

  dir->ptr += limit; // set pointer

  // cleanup
  free(blocks);
  free(tmp);

  // printf("[fd:%d id:%d] read %d bytes\n", fd->id, currentTask->id, curr);
  // printf("%d / %d\n", dir->ptr, dir->inode.size);
  return limit;
}

int ext2_write(OpenFile *fd, uint8_t *buff, size_t limit) {
  Ext2       *ext2 = EXT2_PTR(fd->mountPoint->fsInfo);
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);

  spinlock_acquire(&ext2->LOCK_WRITE);

  size_t appendCursor = (size_t)(-1);
  if (fd->flags & O_APPEND) {
    appendCursor = dir->ptr;
    dir->ptr = COMBINE_64(dir->inode.size_high, dir->inode.size);
  }

  int ptrIgnoredBlocks = dir->ptr / ext2->blockSize;
  int ptrIgnoredBytes = dir->ptr % ext2->blockSize;

  size_t remainder = limit;
  size_t left = 0;

  if (ptrIgnoredBytes > 0) {
    left = MIN(ext2->blockSize - ptrIgnoredBytes, remainder);
    uint32_t block =
        ext2_block_fetch(ext2, &dir->inode, &dir->lookup, ptrIgnoredBlocks);
    uint8_t *tmp = (uint8_t *)malloc(ext2->blockSize);
    get_disk_bytes(tmp, BLOCK_TO_LBA(ext2, 0, block),
                 ext2->blockSize / SECTOR_SIZE);
    memcpy(&tmp[ptrIgnoredBytes], buff, left);
    set_disk_bytes(tmp, BLOCK_TO_LBA(ext2, 0, block),
                 ext2->blockSize / SECTOR_SIZE);

    free(tmp);
    remainder -= left;
    dir->ptr += left;

    // ignore the first block in the functions that follow
    ptrIgnoredBlocks++;
  }

  if (remainder > 0) {
    // we are aligned on block boundaries so we can use this
    int       blocksRequired = CEILING_DIVISION(remainder, ext2->blockSize);
    uint32_t *blocks =
        ext2_block_chain(ext2, dir, ptrIgnoredBlocks, blocksRequired - 1);

    uint32_t group = INODE_TO_BLOCK_GROUP(ext2, dir->inodeNum);

    int startsAt = -1;
    for (int i = 0; i < blocksRequired; i++) {
      if (!blocks[i]) {
        startsAt = i;
        break;
      }
    }

    if (startsAt != -1) {
      int needed = blocksRequired - startsAt;

      uint32_t freeBlocks = ext2_block_find(ext2, group, needed);
      for (int i = 0; i < needed; i++) {
        // todo: standardize weird lookup stuff
        ext2_block_assign(ext2, &dir->inode, dir->inodeNum, &dir->lookup,
                        ptrIgnoredBlocks + startsAt + i, freeBlocks + i);
        blocks[startsAt + i] = freeBlocks + i;
      }
    }

    uint8_t *tmp = (uint8_t *)calloc((blocksRequired + 1) * ext2->blockSize, 1);

    // our first block will have junk data in the start!
    get_disk_bytes(tmp, BLOCK_TO_LBA(ext2, 0, blocks[0]),
                 ext2->blockSize / SECTOR_SIZE);

    // the last block might have junk data at the end!
    int target = blocksRequired - 1;
    if (target > 0)
      get_disk_bytes(&tmp[target * ext2->blockSize],
                   BLOCK_TO_LBA(ext2, 0, blocks[target]),
                   ext2->blockSize / SECTOR_SIZE);
    memcpy(tmp, &buff[left], remainder);

    int currBlock = 0;

    // optimization: we can use consecutive sectors to make our life easier
    int consecStart = -1;
    int consecEnd = 0;

    // +1 for starting
    for (int i = 0; i < blocksRequired; i++) {
      if (!blocks[i]) {
        printf("[ext2::write] FATAL! Out of sync!\n");
        Halt();
      }

      bool last = i == (blocksRequired - 1);
      if (consecStart < 0) {
        // nothing consecutive yet
        if (!last && blocks[i + 1] == (blocks[i] + 1)) {
          // consec starts here
          consecStart = i;
          continue;
        }
      } else {
        // we are in a consecutive that started since consecStart
        if (last || blocks[i + 1] != (blocks[i] + 1))
          consecEnd = i; // either last or the end
        else             // otherwise, we good
          continue;
      }

      if (consecEnd) {
        // optimized consecutive cluster reading
        int needed = consecEnd - consecStart + 1;
        set_disk_bytes(&tmp[currBlock * ext2->blockSize],
                     BLOCK_TO_LBA(ext2, 0, blocks[consecStart]),
                     (needed * ext2->blockSize) / SECTOR_SIZE);
        currBlock += needed;
      } else {
        set_disk_bytes(&tmp[currBlock * ext2->blockSize],
                     BLOCK_TO_LBA(ext2, 0, blocks[i]),
                     ext2->blockSize / SECTOR_SIZE);
        currBlock++;
      }

      // traverse
      consecStart = -1;
      consecEnd = 0;
    }

    dir->ptr += remainder; // set pointer

    // cleanup
    free(blocks);
    free(tmp);
  }

  if (dir->ptr > dir->inode.size) {
    // update size
    dir->inode.size = dir->ptr;
    dir->inode.num_sectors = CEILING_DIVISION(dir->ptr, SECTOR_SIZE) * SECTOR_SIZE;
    ext2_inode_modifyM(ext2, dir->inodeNum, &dir->inode);
  }

  if (fd->flags & O_APPEND)
    dir->ptr = appendCursor;

  spinlock_release(&ext2->LOCK_WRITE);

  // printf("[fd:%d id:%d] read %d bytes\n", fd->id, currentTask->id, curr);
  // printf("%d / %d\n", dir->ptr, dir->inode.size);
  return limit;
}

size_t ext2_seek(OpenFile *fd, size_t target, long int offset, int whence) {
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);

  // "hack" because openfile ptr is not used
  if (whence == SEEK_CURR)
    target += dir->ptr;

  size_t filesize = ext2_get_filesize(fd);
  if (target > filesize) {
    if (!(fd->flags & O_RDWR) && !(fd->flags & O_WRONLY))
      return -EINVAL;

    size_t   remainder = target - filesize;
    uint8_t *bytePlacement = calloc(remainder, 1);

    // todo: optimize direct resolution of HHDM memory instead of allocating
    // todo: space for it again in ext2_write()
    /*printf("filesize{%d} remainder{%d} ptr{%d} target{%d}\n", filesize,
           remainder, dir->ptr, target);*/
    dir->ptr = filesize;
    int written = ext2_write(fd, bytePlacement, remainder);
    if (written != remainder) {
      printf("[ext2::seek] FAILED! Write not in sync!!\n");
      Halt();
    }

    if (dir->ptr != target) {
      printf("[ext2::seek] What?\n");
      Halt();
    }

    free(bytePlacement);
  }
  dir->ptr = target;

  return dir->ptr;
}

size_t ext2_get_filesize(OpenFile *fd) {
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);
  return COMBINE_64(dir->inode.size_high, dir->inode.size);
}

void ext2_stat_internal(Ext2 *ext2, Ext2Inode *inode, uint32_t inodeNum,
                      struct stat *target) {
  target->st_dev = 69; // todo
  target->st_ino = inodeNum;
  // target->st_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IXUSR; // lie
  target->st_mode = inode->permission;
  target->st_nlink = inode->hard_links;
  target->st_uid = 0;
  target->st_gid = 0;
  target->st_rdev = 0;
  target->st_blksize = ext2->blockSize;

  target->st_size = COMBINE_64(inode->size_high, inode->size);
  /*if ((inode->permission & 0xF000) == 0x4000) { // lies
    target->st_mode &= ~S_IFREG;                // mark as dir
    target->st_mode |= S_IFDIR;
  } else if ((inode->permission & 0xF000) == 0xA000) {
    target->st_mode &= ~S_IFREG; // mark as symlink
    target->st_mode |= S_IFLNK;
  }*/

  target->st_blocks =
      (CEILING_DIVISION(target->st_size, target->st_blksize) * target->st_blksize) /
      512;

  target->st_atime = inode->atime;
  target->st_mtime = inode->mtime;
  target->st_ctime = inode->ctime;
}

bool ext2_stat(MountPoint *mnt, char *filename, struct stat *target,
              char **symlinkResolve) {
  Ext2    *ext2 = EXT2_PTR(mnt->fsInfo);
  uint32_t inodeNum =
      ext2_traverse_path(ext2, filename, EXT2_ROOT_INODE, true, symlinkResolve);
  if (!inodeNum)
    return false;
  Ext2Inode *inode = ext2_inode_fetch(ext2, inodeNum);

  ext2_stat_internal(ext2, inode, inodeNum, target);

  free(inode);
  return true;
}

bool ext2_lstat(MountPoint *mnt, char *filename, struct stat *target,
               char **symlinkResolve) {
  Ext2    *ext2 = EXT2_PTR(mnt->fsInfo);
  uint32_t inodeNum =
      ext2_traverse_path(ext2, filename, EXT2_ROOT_INODE, false, symlinkResolve);
  if (!inodeNum)
    return false;
  Ext2Inode *inode = ext2_inode_fetch(ext2, inodeNum);

  ext2_stat_internal(ext2, inode, inodeNum, target);

  free(inode);
  return true;
}

int ext2_statFd(OpenFile *fd, struct stat *target) {
  Ext2       *ext2 = EXT2_PTR(fd->mountPoint->fsInfo);
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);
  ext2_stat_internal(ext2, &dir->inode, dir->inodeNum, target);
  return 0;
}

int ext2_read_link(Ext2 *ext2, char *path, char *buf, int size,
                 char **symlinkResolve) {
  if (size < 0)
    return -EINVAL;
  else if (!size)
    return 0;

  uint32_t inodeNum =
      ext2_traverse_path(ext2, path, EXT2_ROOT_INODE, false, symlinkResolve);
  if (!inodeNum)
    return -ENOENT;

  int ret = -1;

  Ext2Inode *inode = ext2_inode_fetch(ext2, inodeNum);
  if ((inode->permission & 0xF000) != 0xA000) {
    ret = -EINVAL;
    free(inode);
    return ret;
  }

  if (inode->size > 60) {
    printf("[ext2::symlink] Todo! size{%d}\n", inode->size);
    ret = -1;
    free(inode);
    return ret;
  }

  char *start = (char *)inode->blocks;
  int   toCopy = inode->size;
  if (toCopy > size)
    toCopy = size;

  memcpy(buf, start, toCopy);
  ret = toCopy;
  free(inode);
  return ret;
}

bool ext2_close(OpenFile *fd) {
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);

  ext2_block_fetch_cleanup(&dir->lookup);

  free(fd->dir);
  return true;
}

bool ext2_duplicate_node_unsafe(OpenFile *original, OpenFile *orphan) 
{
  orphan->dir = malloc(sizeof(Ext2OpenFd));
  memcpy(orphan->dir, original->dir, sizeof(Ext2OpenFd));

  Ext2       *ext2 = EXT2_PTR(orphan->mountPoint->fsInfo);
  Ext2OpenFd *dir = EXT2_DIR_PTR(orphan->dir);
  Ext2OpenFd *dirOriginal = EXT2_DIR_PTR(original->dir);

  if (dir->lookup.tmp1) {
    dir->lookup.tmp1 = malloc(ext2->blockSize);
    memcpy(dir->lookup.tmp1, dirOriginal->lookup.tmp1, ext2->blockSize);
  }

  if (dir->lookup.tmp2) {
    dir->lookup.tmp2 = malloc(ext2->blockSize);
    memcpy(dir->lookup.tmp2, dirOriginal->lookup.tmp2, ext2->blockSize);
  }

  if (original->dirname) {
    size_t len = strlen(original->dirname) + 1;
    orphan->dirname = (char *)malloc(len);
    memcpy(orphan->dirname, original->dirname, len);
  }

  return true;
}

// task is taken into account
size_t ext2_mmap(size_t addr, size_t length, int prot, int flags, OpenFile *fd, size_t pgoffset) 
{
  if (!(flags & MAP_PRIVATE)) {
    printf("[ext2::mmap] Unsupported flags! flags{%x}\n", flags);
    return -ENOSYS;
  }

  // if (prot & PROT_WRITE && !(fd->flags & O_WRONLY || fd->flags & O_RDWR))
  //   return -EACCES;

  uint64_t mappingFlags = PF_USER;
  if (prot & PROT_WRITE)
    mappingFlags |= PF_RW;
  // read & execute don't have to be specified..

  int pages = CEILING_DIVISION(length, PAGE_SIZE);

  size_t virt = 0;
  if (!(flags & MAP_FIXED)) {
    virt = currentTask->mmap_end;
    currentTask->mmap_end += pages * PAGE_SIZE;
  } else {
    virt = addr;
    if (virt > bootloader.hhdmOffset &&
        virt < (bootloader.hhdmOffset + bootloader.mmTotal))
      return -EACCES;
    else if (virt > bootloader.kernelVirtBase &&
             virt < bootloader.kernelVirtBase + 268435456) {
      return -EACCES;
    }
  }

  // allocate physical space required
  size_t phys = physical_allocate(pages);
  size_t hhdmAddition = bootloader.hhdmOffset + phys;

  // now access it properly (via the HHDM, obviously)
  for (int i = 0; i < pages; i++)
    virtual_map(virt + i * PAGE_SIZE, phys + i * PAGE_SIZE, mappingFlags);
  memset((void *)(hhdmAddition), 0, pages * PAGE_SIZE);

  // do the read
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);
  size_t      oldPtr = dir->ptr;
  dir->ptr = pgoffset;
  ext2_read(fd, (void *)hhdmAddition, length);
  dir->ptr = oldPtr;

  return virt;
}

VfsHandlers ext2Handlers = {
                            .read = ext2_read,
                            .write = ext2_write,
                            .seek = ext2_seek,
                            .stat = ext2_statFd,
                            .mmap = ext2_mmap,
                            .getdents64 = ext2_get_dents64,
                            .getFilesize = ext2_get_filesize,
                            .duplicate = ext2_duplicate_node_unsafe,
                            .open = ext2_open,
                            .close = ext2_close,
                            };

