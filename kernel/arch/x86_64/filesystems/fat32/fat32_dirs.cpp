#include <fat32.h>

#include <dents.h>
#include <liballoc.h>
#include <string.h>
#include <system.h>

#include <utility.h>
#include <hcf.hpp>


int fat32_get_dents64(OpenFile *file, struct linux_dirent64 *start,
                    unsigned int hardlimit) {
  FAT32       *fat = FAT_PTR(file->mountPoint->fsInfo);
  FAT32OpenFd *fatDir = FAT_DIR_PTR(file->dir);

  if (!(fatDir->dirEnt.attrib & FAT_ATTRIB_DIRECTORY))
    return -ENOTDIR;

  if (!fatDir->directoryCurr)
    return 0;

  int allocatedlimit = 0;

  int bytesPerCluster = LBA_TO_OFFSET(fat->bootsec.sectors_per_cluster);

  uint8_t lfnName[LFN_MAX_TOTAL_CHARS] = {0};
  int     lfnLast = -1;

  uint8_t               *bytes = (uint8_t *)malloc(bytesPerCluster);
  struct linux_dirent64 *dirp = (struct linux_dirent64 *)start;

  while (true) {
    if (!fatDir->directoryCurr)
      goto cleanup;

    uint32_t offsetStarting = fatDir->ptr % bytesPerCluster;
    get_disk_bytes(bytes, fat32_cluster_to_LBA(fat, fatDir->directoryCurr),
                 fat->bootsec.sectors_per_cluster);

    for (uint32_t i = offsetStarting; i < bytesPerCluster;
         i += sizeof(FAT32DirectoryEntry)) {
      FAT32DirectoryEntry *dir = (FAT32DirectoryEntry *)(&bytes[i]);
      FAT32LFN            *lfn = (FAT32LFN *)dir;

      if (dir->attrib == FAT_ATTRIB_LFN && !lfn->type) {
        int index = (lfn->order & ~LFN_ORDER_FINAL) - 1;

        if (index >= LFN_MAX) {
          printf("[fat32] Invalid LFN index{%d} size! (>= 20)\n", index);
          Halt();
        }

        if (lfn->order & LFN_ORDER_FINAL)
          lfnLast = index;

        fat32_LFNmem_cpy(lfnName, lfn, index);
      }

      if (dir->attrib == FAT_ATTRIB_DIRECTORY ||
          dir->attrib == FAT_ATTRIB_ARCHIVE) {
        // printf("(%.8s)\n", dir->name);
        int lfnLen = 0;
        if (lfnLast >= 0) {
          while (lfnName[lfnLen++])
            ;
          lfnLen--; // without null terminator
          if (lfnLen < 0) {
            printf("[fat32] Something is horribly wrong... lfnLen{%d}\n",
                   lfnLen);
            Halt();
          }

          // we good!

          lfnLast = -1;
        } else {
          lfnLen = fat32_SFN_to_normal(lfnName, dir);
        }

        unsigned char type = 0;
        if (dir->attrib & FAT_ATTRIB_DIRECTORY)
          type = CDT_DIR;
        else
          type = CDT_REG;

        DENTS_RES res = dents_add(
            start, &dirp, &allocatedlimit, hardlimit, (void *)lfnName, lfnLen,
            FAT_INODE_GEN(fatDir->directoryCurr, i / 32), type);

        if (res == DENTS_NO_SPACE) {
          allocatedlimit = -EINVAL;
          goto cleanup;
        } else if (res == DENTS_RETURN)
          goto cleanup;

        memset(lfnName, 0, LFN_MAX_TOTAL_CHARS); // for good measure

        // traverse...
        // printf("%ld\n", dirp->d_reclen);
      }
      fatDir->ptr += sizeof(FAT32DirectoryEntry);
    }

    fatDir->directoryCurr = fat32_FAT_traverse(fat, fatDir->directoryCurr);
    // printf("moving on to %d\n", fatDir->directoryCurr);
    if (!fatDir->directoryCurr)
      break;
  }

cleanup:
  free(bytes);
  return allocatedlimit;
}
