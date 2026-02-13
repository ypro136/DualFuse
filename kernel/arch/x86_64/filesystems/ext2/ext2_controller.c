#include <bootloader.h>
#include <ext2.h>
#include <liballoc.h>
#include <paging.h>
#include <string.h>
#include <syscalls.h>
#include <system.h>
#include <task.h>
#include <timer.h>
#include <utility.h>
#include <vmm.h>
#include <hcf.hpp>



bool ext2_mount(MountPoint *mount) {
  // assign handlers
  mount->handlers = &ext2Handlers;
  mount->stat = ext2Stat;
  mount->lstat = ext2Lstat;

  mount->mkdir = ext2Mkdir;
  mount->remove = ext2Delete;
  mount->readlink = ext2Readlink;
  mount->link = ext2Link;

  // assign fsInfo
  mount->fsInfo = malloc(sizeof(Ext2));
  memset(mount->fsInfo, 0, sizeof(Ext2));
  Ext2 *ext2 = EXT2_PTR(mount->fsInfo);

  // base offset
  ext2->offsetBase = mount->mbr.lba_first_sector;
  ext2->offsetSuperblock = mount->mbr.lba_first_sector + 2;

  // get superblock
  uint8_t tmp[sizeof(Ext2Superblock)] __attribute__((aligned(2))) = {0};
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
  int bgdtLockSize = sizeof(SpinlockCnt) * ext2->blockGroups;
  ext2->WLOCKS_BLOCK_BITMAP = (SpinlockCnt *)malloc(bgdtLockSize);
  memset(ext2->WLOCKS_BLOCK_BITMAP, 0, bgdtLockSize);

  ext2->WLOCKS_INODE = (SpinlockCnt *)malloc(bgdtLockSize);
  memset(ext2->WLOCKS_INODE, 0, bgdtLockSize);

  ext2->inodeSize = ext2->superblock.extended.inode_size;
  ext2->inodeSizeRounded =
      CEILING_DIVISION(ext2->inodeSize, SECTOR_SIZE) * SECTOR_SIZE;

  // done :")
  return true;
}

size_t ext2Open(char *filename, int flags, int mode, OpenFile *fd,char **symlinkResolve) {
  Ext2 *ext2 = EXT2_PTR(fd->mountPoint->fsInfo);

  size_t ret = 0;
  spinlock_cnt_read_acquire(&ext2->WLOCK_GLOBAL_NOFD);

  uint32_t inode =
      ext2TraversePath(ext2, filename, EXT2_ROOT_INODE, true, symlinkResolve);

  if (!inode && *symlinkResolve) {
    // last entry is a soft symlink that'll have to be resolved back on the
    // open() phase..
    if (flags & O_NOFOLLOW) {
      ret = ERR(ELOOP);
      spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
    } else {
      ret = ERR(ENOENT);
      spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
    }
  }

  if (inode && flags & O_EXCL && flags & O_CREAT) {
    ret = ERR(EEXIST);
    spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
  }

  if (!inode) {
    if (flags & O_CREAT) {
      // escalate permissions & ensure nobody created this before us
      spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
      spinlock_cnt_write_acquire(&ext2->WLOCK_GLOBAL_NOFD);
      if (!(inode = ext2TraversePath(ext2, filename, EXT2_ROOT_INODE, true,
                                     symlinkResolve))) {
        // create this thing
        size_t touch =
            ext2Touch(fd->mountPoint, filename, mode, symlinkResolve);
        if (RET_IS_ERR(touch)) {
          ret = touch;
          spinlock_cnt_write_release(&ext2->WLOCK_GLOBAL_NOFD);
          spinlock_cnt_read_acquire(&ext2->WLOCK_GLOBAL_NOFD);
          spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
        }

        // created successfully
        inode = ext2TraversePath(ext2, filename, EXT2_ROOT_INODE, true,
                                 symlinkResolve);
      }
      spinlock_cnt_write_release(&ext2->WLOCK_GLOBAL_NOFD);
      spinlock_cnt_read_acquire(&ext2->WLOCK_GLOBAL_NOFD);
    } else {
      ret = ERR(ENOENT);
      spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
    }
  }

  Ext2Inode *inodeFetched = ext2InodeFetch(ext2, inode);
  if (flags & O_DIRECTORY && !(inodeFetched->permission & S_IFDIR)) {
    free(inodeFetched);
    ret = ERR(ENOTDIR);
    spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
  }

  if (flags & O_TRUNC) {
    inodeFetched->size = 0;
    inodeFetched->size_high = 0;
    inodeFetched->num_sectors = 0;

    ext2InodeModifyM(ext2, inode, inodeFetched);
  }

  spinlock_acquire(&ext2->LOCK_OBJECT);
  Ext2FoundObject *targetObject = ext2->firstObject;
  while (targetObject) {
    if (targetObject->inode == inode)
      break;
    targetObject = targetObject->next;
  }
  spinlock_release(&ext2->LOCK_OBJECT);
  if (!targetObject) {
    targetObject =
        calloc(sizeof(Ext2FoundObject), 1); // out of the locks in case
    spinlock_acquire(&ext2->LOCK_OBJECT);
    targetObject->inode = inode;

    targetObject->next = ext2->firstObject; // dll stuff
    ext2->firstObject = targetObject;
    if (targetObject->next)
      targetObject->next->prev = targetObject;
    spinlock_release(&ext2->LOCK_OBJECT);
  }

  // we opened a file!
  spinlock_acquire(&targetObject->LOCK_PROP);
  targetObject->openFds++;
  spinlock_release(&targetObject->LOCK_PROP);

  Ext2OpenFd *dir = (Ext2OpenFd *)malloc(sizeof(Ext2OpenFd));
  memset(dir, 0, sizeof(Ext2OpenFd));
  fd->dir = dir;

  dir->inodeNum = inode;
  memcpy(&dir->inode, inodeFetched, sizeof(Ext2Inode));

  dir->globalObject = targetObject;

  if ((dir->inode.permission & 0xF000) == EXT2_S_IFDIR) {
    size_t len = strlen(filename) + 1;
    fd->dirname = malloc(len);
    memcpy(fd->dirname, filename, len);
  }

  ext2BlockFetchInit(ext2, &dir->lookup);

  // pointers & stuff
  dir->ptr = 0;

  free(inodeFetched);

  spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
}

size_t ext2Read(OpenFile *fd, uint8_t *buff, size_t naiveLimit) {
  Ext2       *ext2 = EXT2_PTR(fd->mountPoint->fsInfo);
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);

  if (dir->inode.permission & S_IFDIR)
    return ERR(EISDIR);

  ext2CachePush(ext2, dir);

  size_t filesize = ext2GetFilesize(fd);
  if (dir->ptr >= filesize)
    return 0;

  size_t limit = naiveLimit;
  if (limit > (filesize - dir->ptr))
    limit = filesize - dir->ptr;

  size_t blocksRequired = CEILING_DIVISION(limit, ext2->blockSize);

  spinlock_cnt_read_acquire(&dir->globalObject->WLOCK_FILE);

  // find anything as a start that is close
  spinlock_cnt_read_acquire(&dir->globalObject->WLOCK_CACHE);
  size_t           blockIndexStart = dir->ptr / ext2->blockSize;
  Ext2CacheObject *cacheObj = dir->globalObject->firstCacheObj;
  while (cacheObj) {
    if (cacheObj->blockIndex >= blockIndexStart &&
        cacheObj->blockIndex < (blockIndexStart + blocksRequired))
      break;
    cacheObj = cacheObj->next;
  }
  spinlock_cnt_read_release(&dir->globalObject->WLOCK_CACHE);

  size_t left = limit;
  for (size_t i = 0; i < (blocksRequired + 1); i++) {
    if (cacheObj && cacheObj->blockIndex == (blockIndexStart + i)) {
      // we are in a valid cache region
      spinlock_cnt_read_acquire(&dir->globalObject->WLOCK_CACHE);
      uint32_t rem = dir->ptr % ext2->blockSize;
      size_t   toCopy = MIN(left, cacheObj->blocks * ext2->blockSize - rem);
      memcpy(&buff[limit - left], &cacheObj->buff[rem], toCopy);
      left -= toCopy;
      i += cacheObj->blocks - 1; // -1 cause it's added automatically
      dir->ptr += toCopy;
      cacheObj = cacheObj->next;
      spinlock_cnt_read_release(&dir->globalObject->WLOCK_CACHE);
    } else {
      // we are not inside caching, let's see if we're close to it
      size_t blocksToScan = blocksRequired - i;
      if (!blocksToScan) // the extra block caused by the rem thing
        blocksToScan = 1;
      if (cacheObj)
        // to_be_scanned = target_location - current_location;
        blocksToScan = cacheObj->blockIndex - (dir->ptr / ext2->blockSize);
      uint32_t rem = dir->ptr % ext2->blockSize;
      size_t   toCopy = MIN(left, blocksToScan * ext2->blockSize - rem);
      assert(ext2ReadInner(fd, &buff[limit - left], toCopy) == toCopy);
      left -= toCopy;
      i += blocksToScan - 1; // -1 cause it's added automatically
    }
    if (left == 0)
      break;
  }

  assert(left == 0);
  spinlock_cnt_read_release(&dir->globalObject->WLOCK_FILE);
  return limit;
}

size_t ext2ReadInner(OpenFile *fd, uint8_t *buff, size_t limit) {
  Ext2       *ext2 = EXT2_PTR(fd->mountPoint->fsInfo);
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);

  size_t    blocksRequired = CEILING_DIVISION(limit, ext2->blockSize);
  uint32_t *blocks =
      ext2BlockChain(ext2, dir, dir->ptr / ext2->blockSize, blocksRequired);
  size_t tmpSize =
      CEILING_DIVISION((blocksRequired + 1) * ext2->blockSize, BLOCK_SIZE);
  uint8_t *tmp = (uint8_t *)virtual_allocate(tmpSize);
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

  // cache it for later
  ext2CacheAddSecurely(fd->mountPoint, dir->globalObject, tmp,
                       dir->ptr / ext2->blockSize, blocksRequired);

  dir->ptr += limit; // set pointer

  // cleanup
  free(blocks);
  // virtual_free(tmp, tmpSize);

  // printf("[fd:%d id:%d] read %d bytes\n", fd->id, currentTask->id, curr);
  // printf("%d / %d\n", dir->ptr, dir->inode.size);
  return limit;
}

size_t ext2Write(OpenFile *fd, uint8_t *buff, size_t limit) {
  Ext2       *ext2 = EXT2_PTR(fd->mountPoint->fsInfo);
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);

  ext2CachePush(ext2, dir);

  // todo! memory leak!
  spinlock_cnt_write_acquire(&dir->globalObject->WLOCK_CACHE);
  dir->globalObject->firstCacheObj = 0;
  spinlock_cnt_write_release(&dir->globalObject->WLOCK_CACHE);

  if (dir->inode.permission & S_IFDIR)
    return ERR(EISDIR);

  spinlock_cnt_write_acquire(&dir->globalObject->WLOCK_FILE);

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
    uint32_t block = ext2BlockFetch(ext2, &dir->inode, dir->inodeNum,
                                    &dir->lookup, ptrIgnoredBlocks);
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
        ext2BlockChain(ext2, dir, ptrIgnoredBlocks, blocksRequired - 1);

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

      uint32_t freeBlocks = ext2BlockFind(ext2, group, needed);
      for (int i = 0; i < needed; i++) {
        // todo: standardize weird lookup stuff
        ext2BlockAssign(ext2, &dir->inode, dir->inodeNum, &dir->lookup,
                        ptrIgnoredBlocks + startsAt + i, freeBlocks + i);
        blocks[startsAt + i] = freeBlocks + i;
      }
    }

    size_t tmpSize =
        CEILING_DIVISION((blocksRequired + 1) * ext2->blockSize, BLOCK_SIZE);
    uint8_t *tmp = (uint8_t *)virtual_allocate(tmpSize);

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
    virtual_free(tmp, tmpSize);
  }

  if (dir->ptr > dir->inode.size) {
    // update size
    dir->inode.size = dir->ptr;
    dir->inode.num_sectors =
        ext2BlockSizeCalculate(ext2, dir->inode.size) / SECTOR_SIZE;
    // todo: use this field properly considering it has indirect blocks too
    ext2InodeModifyM(ext2, dir->inodeNum, &dir->inode);
  }

  if (fd->flags & O_APPEND)
    dir->ptr = appendCursor;

  spinlock_cnt_write_release(&dir->globalObject->WLOCK_FILE);

  // printf("[fd:%d id:%d] read %d bytes\n", fd->id, currentTask->id, curr);
  // printf("%d / %d\n", dir->ptr, dir->inode.size);
  return limit;
}

size_t ext2Seek(OpenFile *fd, size_t target, long int offset, int whence) {
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);

  // "hack" because openfile ptr is not used
  if (whence == SEEK_CURR)
    target += dir->ptr;

  size_t filesize = ext2GetFilesize(fd);
  if (target > filesize) {
    if (!(fd->flags & O_RDWR) && !(fd->flags & O_WRONLY))
      return ERR(EINVAL);

    size_t   remainder = target - filesize;
    uint8_t *bytePlacement = calloc(remainder, 1);

    // todo: optimize direct resolution of HHDM memory instead of allocating
    // todo: space for it again in ext2Write()
    /*printf("filesize{%d} remainder{%d} ptr{%d} target{%d}\n", filesize,
           remainder, dir->ptr, target);*/
    dir->ptr = filesize;
    int written = ext2Write(fd, bytePlacement, remainder);
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

size_t ext2GetFilesize(OpenFile *fd) {
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);
  return COMBINE_64(dir->inode.size_high, dir->inode.size);
}

void ext2StatInternal(Ext2 *ext2, Ext2Inode *inode, uint32_t inodeNum,
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

bool ext2Stat(MountPoint *mnt, char *filename, struct stat *target,
              char **symlinkResolve) {
  Ext2 *ext2 = EXT2_PTR(mnt->fsInfo);

  spinlock_cnt_read_acquire(&ext2->WLOCK_GLOBAL_NOFD);
  uint32_t inodeNum =
      ext2TraversePath(ext2, filename, EXT2_ROOT_INODE, true, symlinkResolve);
  if (!inodeNum) {
    spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
    return false;
  }
  Ext2Inode *inode = ext2InodeFetch(ext2, inodeNum);

  ext2StatInternal(ext2, inode, inodeNum, target);

  free(inode);
  spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
  return true;
}

bool ext2Lstat(MountPoint *mnt, char *filename, struct stat *target,
               char **symlinkResolve) {
  Ext2 *ext2 = EXT2_PTR(mnt->fsInfo);

  spinlock_cnt_read_acquire(&ext2->WLOCK_GLOBAL_NOFD);
  uint32_t inodeNum =
      ext2TraversePath(ext2, filename, EXT2_ROOT_INODE, false, symlinkResolve);
  if (!inodeNum) {
    spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
    return false;
  }
  Ext2Inode *inode = ext2InodeFetch(ext2, inodeNum);

  ext2StatInternal(ext2, inode, inodeNum, target);

  free(inode);
  spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
  return true;
}

size_t ext2StatFd(OpenFile *fd, struct stat *target) {
  Ext2       *ext2 = EXT2_PTR(fd->mountPoint->fsInfo);
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);
  ext2StatInternal(ext2, &dir->inode, dir->inodeNum, target);
  return 0;
}

size_t ext2Readlink(MountPoint *mnt, char *path, char *buf, int size,char **symlinkResolve) {
  Ext2 *ext2 = EXT2_PTR(mnt->fsInfo);
  if (size < 0)
    return ERR(EINVAL);
  else if (!size)
    return 0;

  size_t     ret = -1;
  Ext2Inode *inode = 0;
  spinlock_cnt_read_acquire(&ext2->WLOCK_GLOBAL_NOFD);

  uint32_t inodeNum =
      ext2TraversePath(ext2, path, EXT2_ROOT_INODE, false, symlinkResolve);
  if (!inodeNum) {
    ret = ERR(ENOENT);
  if (inode)
    free(inode);
  spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
  }

  inode = ext2InodeFetch(ext2, inodeNum);
  if ((inode->permission & 0xF000) != 0xA000) {
    ret = ERR(EINVAL);
  if (inode)
    free(inode);
  spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
  }

  char *start = (char *)inode->blocks;
  if (inode->size > 60) {
    assert(inode->size < ext2->blockSize);
    start = calloc(ext2->blockSize + 1, 1);
    get_disk_bytes((uint8_t *)start, BLOCK_TO_LBA(ext2, 0, inode->blocks[0]),
                 ext2->blockSize / SECTOR_SIZE);
  }

  int toCopy = inode->size;
  if (toCopy > size)
    toCopy = size;

  memcpy(buf, start, toCopy);
  ret = toCopy;

  if (inode->size > 60)
    free(start);

  if (inode)
    free(inode);
  spinlock_cnt_read_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
}

bool ext2Close(OpenFile *fd) {
  Ext2OpenFd *dir = EXT2_DIR_PTR(fd->dir);

  ext2BlockFetchCleanup(&dir->lookup);

  spinlock_acquire(&dir->globalObject->LOCK_PROP);
  dir->globalObject->openFds--;
  spinlock_release(&dir->globalObject->LOCK_PROP);

  free(fd->dir);
  return true;
}

bool ext2DuplicateNodeUnsafe(OpenFile *original, OpenFile *orphan) {
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

  spinlock_acquire(&dirOriginal->globalObject->LOCK_PROP);
  dirOriginal->globalObject->openFds++;
  spinlock_release(&dirOriginal->globalObject->LOCK_PROP);

  return true;
}

// task is taken into account
size_t ext2Mmap(size_t addr, size_t length, int prot, int flags, OpenFile *fd,
                size_t pgoffset) {
  if (!(flags & MAP_PRIVATE)) {
    printf("[ext2::mmap] Unsupported flags! flags{%x}\n", flags);
    // return ERR(ENOSYS);
  }

  // if (prot & PROT_WRITE && !(fd->flags & O_WRONLY || fd->flags & O_RDWR))
  //   return ERR(EACCES);

  uint64_t mappingFlags = PF_USER;
  if (prot & PROT_WRITE)
    mappingFlags |= PF_RW;
  // read & execute don't have to be specified..

  int pages = CEILING_DIVISION(length, PAGE_SIZE);

  size_t virt = 0;
  if (!(flags & MAP_FIXED)) {
    spinlock_acquire(&currentTask->infoPd->LOCK_PD);
    virt = currentTask->infoPd->mmap_end;
    currentTask->infoPd->mmap_end += pages * PAGE_SIZE;
    spinlock_release(&currentTask->infoPd->LOCK_PD);
  } else {
    virt = addr;
    if (virt > bootloader.hhdmOffset &&
        virt < (bootloader.hhdmOffset + bootloader.mmTotal))
      return ERR(EACCES);
    else if (virt > bootloader.kernelVirtBase &&
             virt < bootloader.kernelVirtBase + 268435456) {
      return ERR(EACCES);
    }
  }

  spinlock_acquire(&currentTask->infoPd->LOCK_PD);
  size_t end = virt + pages * PAGE_SIZE;
  if (end > currentTask->infoPd->mmap_end)
    currentTask->infoPd->mmap_end = end;
  spinlock_release(&currentTask->infoPd->LOCK_PD);

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
  ext2Read(fd, (void *)hhdmAddition, length);
  dir->ptr = oldPtr;

  return virt;
}

Ext2FoundObject *ext2GlobalFetch(Ext2 *ext2, uint32_t inode) {
  spinlock_acquire(&ext2->LOCK_OBJECT);
  Ext2FoundObject *browse = ext2->firstObject;
  while (browse) {
    if (browse->inode == inode)
      break;
    browse = browse->next;
  }
  spinlock_release(&ext2->LOCK_OBJECT);
  return browse;
}

size_t ext2Delete(MountPoint *mnt, char *filename, bool directory,char **symlinkResolve) {
  size_t   ret = 0;
  Ext2    *ext2 = EXT2_PTR(mnt->fsInfo);
  uint32_t inodeNum =
      ext2TraversePath(ext2, filename, 2, false, symlinkResolve);
  Ext2Inode *inode = 0;

  spinlock_cnt_write_acquire(&ext2->WLOCK_GLOBAL_NOFD);
  if (!inodeNum) {
    ret = ERR(ENOENT);
  if (inode)
    free(inode);
  spinlock_cnt_write_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
  }

  // todo: bool deleted, remove direntry, wipe on last close
  Ext2FoundObject *global = ext2GlobalFetch(ext2, inodeNum);
  assert(!global || !global->openFds);

  inode = ext2InodeFetch(ext2, inodeNum);
  if (!inode) {
    ret = ERR(ENOENT);
  if (inode)
    free(inode);
  spinlock_cnt_write_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
  }

  if (directory) {
    // we're in directory mode, check if it's a directory
    if (!(inode->permission & S_IFDIR)) {
      ret = ERR(ENOTDIR);
  if (inode)
    free(inode);
  spinlock_cnt_write_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
    }
  } else {
    // we're in file mode, check if it's a file
    if (inode->permission & S_IFDIR) {
      ret = ERR(EISDIR);
  if (inode)
    free(inode);
  spinlock_cnt_write_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
    }
  }

  // start the fun
  int filenameLen = strlen(filename);
  if (filenameLen == 1) {
    // talking about /
    if (directory)
      ret = ERR(ENOTEMPTY);
    else
      ret = ERR(EISDIR);
  if (inode)
    free(inode);
  spinlock_cnt_write_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
  }
  int parentLen = 0;

  if (directory) {
    // directory special: check if the directory is empty first
    uint8_t       *names = (uint8_t *)malloc(ext2->blockSize);
    Ext2Directory *dir = (Ext2Directory *)names;
    get_disk_bytes((uint8_t *)dir, BLOCK_TO_LBA(ext2, 0, inode->blocks[0]),
                 ext2->blockSize / SECTOR_SIZE);
    int i = 0;
    while (((size_t)dir - (size_t)names) < ext2->blockSize) {
      if (dir->filenameLength > 2 || i > 1) {
        free(names);
        ret = ERR(ENOTEMPTY);
          if (inode)
            free(inode);
  spinlock_cnt_write_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
      }
      if (dir->inode)
        i++;
      dir = (void *)((size_t)dir + dir->size);
    }
    free(names);
  }

  // find the parent
  char *parent = (char *)malloc(filenameLen + 1);
  memcpy(parent, filename, filenameLen + 1);
  for (int i = filenameLen; i >= 0; i--) {
    if (parent[i] == '/') {
      if (i != 0)
        parent[i] = '\0';
      parentLen = i;
      break;
    }
    parent[i] = '\0';
  }

  // shouldn't need symlinkResolve now since the upper one resolved
  uint32_t parentInodeNum = ext2TraversePath(ext2, parent, 2, false, 0);
  assert(parentInodeNum);

  Ext2Inode *parentInode = ext2InodeFetch(ext2, parentInodeNum);
  assert(parentInode && parentInode->permission & S_IFDIR);

  inode->hard_links--;
  if (inode->permission & S_IFDIR &&
      inode->hard_links == 1) { // only the "." thing
    // if we reached this point of the code, it shall be empty
    inode->hard_links = 0;
  }
  if (!inode->hard_links) {
    if (inode->permission & S_IFREG || inode->permission & S_IFDIR) {
      // regular file, delete the contents (really just mark them as free)
      // same applies with empty directories that host the "." & ".." stuff
      Ext2LookupControl control = {0};
      ext2BlockFetchInit(ext2, &control);
      size_t i = 0;
      while (true) {
        uint32_t block = ext2BlockFetch(ext2, inode, inodeNum, &control, i++);
        if (!block || (i * ext2->blockSize) >= (inode->num_sectors * 512))
          break;

        uint32_t group = block / ext2->superblock.blocks_per_group;
        uint32_t index = block % ext2->superblock.blocks_per_group;
        ext2BlockDelete(ext2, group, index);
        // todo: free indirect blocks
      }
      ext2BlockFetchCleanup(&control);
    }

    // before deleting, do some sanity stuff
    inode->dtime = timerBootUnix + timerTicks / 1000; // needed
    memset(&inode->blocks, 0, sizeof(inode->blocks));
    inode->num_sectors = 0;
    inode->size = 0;
    inode->size_high = 0;
    ext2InodeModifyM(ext2, inodeNum, inode);

    // get rid of this inode
    ext2InodeDelete(ext2, inodeNum);

    // if it's a directory inform the parent
    if (inode->permission & S_IFDIR) {
      parentInode->hard_links--;
      ext2InodeModifyM(ext2, parentInodeNum, parentInode);
    }
  } else {
    ext2InodeModifyM(ext2, inodeNum, inode);
  }

  // keep only the last part of filename, let's recycle parent
  memmove(parent, &filename[parentLen + 1], filenameLen - parentLen - 1);
  ret = ext2DirRemove(ext2, parentInode, parentInodeNum, parent,
                      filenameLen - parentLen - 1)
            ? 0
            : -1;
  free(parent);

  if (inode)
    free(inode);
  spinlock_cnt_write_release(&ext2->WLOCK_GLOBAL_NOFD);
  return ret;
}

// todo: add nofd write locks here as well & do the mountPoint1 != mountPoint2
// for global move operations/renames!
size_t ext2Link(MountPoint *mnt, char *filename, char *target,
                char **symlinkResolve, char **symlinkResolveTarget) {
  Ext2    *ext2 = EXT2_PTR(mnt->fsInfo);
  uint32_t inodeNum =
      ext2TraversePath(ext2, filename, 2, false, symlinkResolve);
  if (!inodeNum)
    return ERR(ENOENT);

  Ext2Inode *inode = ext2InodeFetch(ext2, inodeNum);
  if (!inode)
    return ERR(ENOENT);
  if (!(inode->permission & S_IFREG || inode->permission & S_IFDIR)) {
    free(inode);
    return ERR(EPERM);
  }

  char *targetDir = strdup(target);
  char *targetFilename = strrchr(targetDir, '/');
  if (!targetFilename) {
    free(inode);
    free(targetDir);
    return ERR(ENOENT);
  }
  *targetFilename = '\0'; // zero so targetDir works
  targetFilename++;       // targetFilename starts afterwards

  uint32_t targetDirInodeNum =
      ext2TraversePath(ext2, targetDir, 2, false, symlinkResolveTarget);
  if (!targetDirInodeNum) {
    free(inode);
    free(targetDir);
    return ERR(ENOENT);
  }

  Ext2Inode *targetDirInode = ext2InodeFetch(ext2, targetDirInodeNum);
  assert(targetDirInode->permission & S_IFDIR); // not checking again

  // make it hard
  inode->hard_links++;
  ext2InodeModifyM(ext2, inodeNum, inode);

  // create a directory entry
  uint8_t dirType = inode->permission & S_IFREG ? 1 : 2;
  ext2DirAllocate(ext2, targetDirInodeNum, targetDirInode, targetFilename,
                  strlen(targetFilename), dirType, inodeNum);

  // cleanup
  free(inode);
  free(targetDirInode);
  free(targetDir);
  return 0;
}

VfsHandlers ext2Handlers = {
    .read = ext2Read,
    .write = ext2Write,
    .seek = ext2Seek,
    .ioctl = NULL,
    .stat = ext2StatFd,
    .mmap = ext2Mmap,
    .getdents64 = ext2Getdents64,
    .getFilesize = ext2GetFilesize,
    .poll = NULL,
    .fcntl = NULL,
    .bind = NULL,
    .listen = NULL,
    .accept = NULL,
    .connect = NULL,
    .recvfrom = NULL,
    .sendto = NULL,
    .recvmsg = NULL,
    .sendmsg = NULL,
    .getsockname = NULL,
    .getsockopts = NULL,
    .getpeername = NULL,
    .internalPoll = NULL,
    .reportKey = NULL,
    .addWatchlist = NULL,
    .duplicate = ext2DuplicateNodeUnsafe,
    .open = ext2Open,
    .close = ext2Close
};
