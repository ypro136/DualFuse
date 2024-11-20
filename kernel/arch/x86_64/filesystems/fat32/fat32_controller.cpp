#include <fat32.h>

#include <disk.h>
#include <liballoc.h>
#include <string.h>
#include <system.h>
#include <timer.h>

#include <utility.h>
#include <hcf.hpp>

// todo: directories are still rough around the edges...
// also, make the vfs remove mountpoint prefixes before handing them off to here
// also, optimization is REALLY needed!

bool fat32_mount(MountPoint *mount) {
  // assign handlers
  mount->handlers = &fat32Handlers;
  mount->stat = fat32_stat;
  mount->lstat = fat32_stat; // fat doesn't support symlinks

  // assign fsInfo
  mount->fsInfo = malloc(sizeof(FAT32));
  memset(mount->fsInfo, 0, sizeof(FAT32));
  FAT32 *fat = FAT_PTR(mount->fsInfo);

  // base offset
  fat->offsetBase = mount->mbr.lba_first_sector; // 2048 (in LBA)

  // get first sector
  uint8_t firstSec[SECTOR_SIZE] = {0};
  get_disk_bytes(firstSec, fat->offsetBase, 1);

  // store it
  memcpy(&fat->bootsec, firstSec, sizeof(FAT32BootSector));

  if (fat->bootsec.bytes_per_sector != SECTOR_SIZE) {
    printf("[fat32] What kind of devil-made FAT partition doesn't have 512 "
           "bytes / sector?\n");
    Halt();
  }

  // setup some other offsets so it's easier later
  fat->offsetFats = fat->offsetBase + fat->bootsec.reserved_sector_count;
  fat->offsetClusters =
      fat->offsetFats +
      fat->bootsec.table_count * fat->bootsec.extended_section.table_size_32;

  for (int i = 0; i < FAT32_CACHE_MAX; i++) {
    fat->cacheBase[i] = FAT32_CACHE_BAD;
    fat->cache[i] = malloc(LBA_TO_OFFSET(fat->bootsec.sectors_per_cluster));
    memset(fat->cache[i], 0, LBA_TO_OFFSET(fat->bootsec.sectors_per_cluster));
  }

  // done :")
  return true;
}

int fat32_open(char *filename, int flags, int mode, OpenFile *fd,
              char **symlinkResolve) {
  if (flags & O_CREAT || flags & O_WRONLY || flags & O_RDWR)
    return -EROFS;
  FAT32 *fat = FAT_PTR(fd->mountPoint->fsInfo);

  // first make sure it.. exists!
  FAT32TraverseResult res = fat32_traverse_path(
      fat, filename, fat->bootsec.extended_section.root_cluster);
  if (!res.directory)
    return -ENOENT;

  // allocate some space
  fd->dir = malloc(sizeof(FAT32OpenFd));
  FAT32OpenFd *dir = FAT_DIR_PTR(fd->dir);
  memset(dir, 0, sizeof(FAT32OpenFd));

  // fill in some initial info
  dir->ptr = 0;
  dir->index = res.index;
  dir->directoryStarting = res.directory;
  dir->directoryCurr =
      FAT_COMB_HIGH_LOW(res.dirEntry.clusterhigh, res.dirEntry.clusterlow);
  memcpy(&dir->dirEnt, &res.dirEntry, sizeof(FAT32DirectoryEntry));

  if (res.dirEntry.attrib & FAT_ATTRIB_DIRECTORY) {
    size_t len = strlen(filename) + 1;
    fd->dirname = (char *)malloc(len);
    memcpy(fd->dirname, filename, len);
  }

  return 0;
}

int fat32_read(OpenFile *fd, uint8_t *buff, size_t limit) {
  FAT32       *fat = FAT_PTR(fd->mountPoint->fsInfo);
  FAT32OpenFd *dir = FAT_DIR_PTR(fd->dir);

  if (dir->dirEnt.attrib & FAT_ATTRIB_DIRECTORY)
    return 0;

  int curr = 0; // will be used to return
  int bytesPerCluster = LBA_TO_OFFSET(fat->bootsec.sectors_per_cluster);
  // ^ is used everywhere!

  uint8_t  *bytes = (uint8_t *)malloc(bytesPerCluster);
  int       fatLookupsNeeded = CEILING_DIVISION(limit, bytesPerCluster);
  uint32_t *fatLookup =
      fat32_FAT_chain(fat, dir->directoryCurr, fatLookupsNeeded);

  // optimization: we can use consecutive sectors to make our life easier
  int consecStart = -1;
  int consecEnd = 0;

  // +1 for starting
  for (int i = 0; i < (fatLookupsNeeded + 1); i++) {
    if (!fatLookup[i])
      break;
    dir->directoryCurr = fatLookup[i];
    bool last = i == (fatLookupsNeeded - 1);
    if (consecStart < 0) {
      // nothing consecutive yet
      if (!last && fatLookup[i + 1] == (fatLookup[i] + 1)) {
        // consec starts here
        consecStart = i;
        continue;
      }
    } else {
      // we are in a consecutive that started since consecStart
      if (last || fatLookup[i + 1] != (fatLookup[i] + 1))
        consecEnd = i; // either last or the end
      else             // otherwise, we good
        continue;
    }

    uint32_t offsetStarting = dir->ptr % bytesPerCluster; // remainder
    if (consecEnd) {
      // optimized consecutive cluster reading
      int      needed = consecEnd - consecStart + 1;
      uint8_t *optimizedBytes = malloc(needed * bytesPerCluster);
      get_disk_bytes(optimizedBytes,
                   fat32_cluster_to_LBA(fat, fatLookup[consecStart]),
                   needed * fat->bootsec.sectors_per_cluster);

      for (uint32_t i = offsetStarting; i < (needed * bytesPerCluster); i++) {
        if (curr >= limit || dir->ptr >= dir->dirEnt.filesize) {
          free(optimizedBytes);
          goto cleanup;
        }

        if (buff)
          buff[curr] = optimizedBytes[i];

        dir->ptr++;
        curr++;
      }

      free(optimizedBytes);
    } else {
      get_disk_bytes(bytes, fat32_cluster_to_LBA(fat, fatLookup[i]),
                   fat->bootsec.sectors_per_cluster);

      for (uint32_t i = offsetStarting; i < bytesPerCluster; i++) {
        if (curr >= limit || dir->ptr >= dir->dirEnt.filesize)
          goto cleanup;

        if (buff)
          buff[curr] = bytes[i];

        dir->ptr++;
        curr++;
      }
    }

    // traverse
    consecStart = -1;
    consecEnd = 0;
  }

cleanup:
  free(bytes);
  free(fatLookup);
  return curr;
}

size_t fat32_seek(OpenFile *fd, size_t target, long int offset, int whence) {
  FAT32       *fat = FAT_PTR(fd->mountPoint->fsInfo);
  FAT32OpenFd *dir = FAT_DIR_PTR(fd->dir);

  // "hack" because openfile ptr is not used
  if (whence == SEEK_CURR)
    target += dir->ptr;

  if (dir->dirEnt.attrib & FAT_ATTRIB_DIRECTORY)
    return -EINVAL;

  if (target > dir->dirEnt.filesize)
    return -EINVAL;

  int old = dir->ptr;
  if (old > target) {
    dir->directoryCurr =
        FAT_COMB_HIGH_LOW(dir->dirEnt.clusterhigh, dir->dirEnt.clusterlow);
    old = 0;
  }
  dir->ptr = target;

  // this REALLY needs optimization!
  int toBeSkipped = target / LBA_TO_OFFSET(fat->bootsec.sectors_per_cluster);
  int alreadySkipped = old / LBA_TO_OFFSET(fat->bootsec.sectors_per_cluster);
  for (int i = 0; i < (toBeSkipped - alreadySkipped); i++) {
    dir->directoryCurr = fat32_FAT_traverse(fat, dir->directoryCurr);
    if (!dir->directoryCurr)
      goto cleanup;
  }

cleanup:
  return dir->ptr;
}

size_t fat32_get_filesize(OpenFile *fd) {
  FAT32OpenFd *dir = FAT_DIR_PTR(fd->dir);

  if (dir->dirEnt.attrib & FAT_ATTRIB_DIRECTORY)
    return 0;

  return dir->dirEnt.filesize;
}

bool fat32_close(OpenFile *fd) {
  FAT32OpenFd *dir = FAT_DIR_PTR(fd->dir);
  if (dir->dirEnt.attrib & FAT_ATTRIB_DIRECTORY && fd->dirname)
    free(fd->dirname);

  // :p
  free(fd->dir);
  return true;
}

bool fat32_duplicate_node_unsafe(OpenFile *original, OpenFile *orphan) {
  orphan->dir = malloc(sizeof(FAT32OpenFd));
  memcpy(orphan->dir, original->dir, sizeof(FAT32OpenFd));

  if (original->dirname) {
    size_t len = strlen(original->dirname) + 1;
    orphan->dirname = (char *)malloc(len);
    memcpy(orphan->dirname, original->dirname, len);
  }

  return true;
}

VfsHandlers fat32Handlers = {
                             .read = fat32_read,
                             .seek = fat32_seek,
                             .stat = fat32_stat_fd,
                             .getdents64 = fat32_get_dents64,
                             .getFilesize = fat32_get_filesize,
                             .duplicate = fat32_duplicate_node_unsafe,
                             .open = fat32_open,
                             .close = fat32_close
                             };
