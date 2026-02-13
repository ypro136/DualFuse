#include <dev.h>
#include <disk.h>
#include <ext2.h>
#include <fat32.h>
#include <linked_list.h>
#include <liballoc.h>
//#include <proc.h>
#include <string.h>
#include <sys.h>
#include <system.h>
#include <task.h>
#include <utility.h>
#include <vfs.h>

// Different mount point separation

// todo: add LL lock to mountPoints

bool fsUnmount(MountPoint *mnt) {
  printf("[vfs] Tried to unmount!\n");
  Halt();
  LinkedListUnregister(&dsMountPoint, sizeof(MountPoint) , mnt);

  // todo!
  // switch (mnt->filesystem) {
  // case FS_FATFS:
  //   f_unmount("/");
  //   break;
  // }

  free(mnt->prefix);
  free(mnt);

  return true;
}

bool isFat(mbr_partition *mbr) {
  uint8_t *rawArr = (uint8_t *)malloc(SECTOR_SIZE);
  get_disk_bytes(rawArr, mbr->lba_first_sector, 1);

  bool ret = (rawArr[66] == 0x28 || rawArr[66] == 0x29);

  free(rawArr);

  return ret;
}

bool isExt2(mbr_partition *mbr) { return mbr->type == 0x83; }

// prefix MUST end with '/': /mnt/handle/
MountPoint *fsMount(char *prefix, CONNECTOR connector, uint32_t disk,
                    uint8_t partition) {
  MountPoint *mount = (MountPoint *)LinkedListAllocate(
      &dsMountPoint, sizeof(MountPoint));

  uint32_t strlength = strlen(prefix);
  mount->prefix = (char *)(malloc(strlength + 1));
  memcpy(mount->prefix, prefix, strlength);
  mount->prefix[strlength] = '\0'; // null terminate

  mount->disk = disk;
  mount->partition = partition;
  mount->connector = connector;

  bool ret = false;
  switch (connector) {
  case CONNECTOR_AHCI:
    if (!open_disk(disk, partition, &mount->mbr)) {
      fsUnmount(mount);
      return 0;
    }

    if (isFat(&mount->mbr)) {
      mount->filesystem = FS_FATFS;
      ret = fat32_mount(mount);
    } else if (isExt2(&mount->mbr)) {
      mount->filesystem = FS_EXT2;
      ret = ext2_mount(mount);
    }
    break;
  case CONNECTOR_DEV:
    mount->filesystem = FS_DEV;
    ret = devMount(mount);
    break;
  case CONNECTOR_SYS:
    mount->filesystem = FS_SYS;
    ret = sys_mount(mount);
    break;
  // case CONNECTOR_PROC:
  //   mount->filesystem = FS_PROC;
  //   ret = procMount(mount);
  //   break;
  default:
    printf("[vfs] Tried to mount with bad connector! id{%d}\n", connector);
    ret = 0;
    break;
  }

  if (!ret) {
    fsUnmount(mount);
    return 0;
  }

  if (!systemDiskInit && strlen(prefix) == 1 && prefix[0] == '/')
    systemDiskInit = true;
  return mount;
}

MountPoint *fsDetermineMountPoint(char *filename) {
  MountPoint *largestAddr = 0;
  uint32_t    largestLen = 0;

  MountPoint *browse = (MountPoint *)dsMountPoint.firstObject;
  while (browse) {
    size_t len = strlen(browse->prefix) - 1; // without trailing /
    if (len >= largestLen && memcmp(filename, browse->prefix, len) == 0 &&
        (filename[len] == '/' || filename[len] == '\0')) {
      largestAddr = browse;
      largestLen = len;
    }
    browse = (MountPoint *)browse->_ll.next;
  }

  return largestAddr;
}

// make SURE to free both! also returns non-safe filename, obviously
char *fsResolveSymlink(MountPoint *mnt, char *symlink) {
  int symlinkLength = strlen(symlink);
  int prefixLength = strlen(mnt->prefix) - 1; // remove slash

  char *ret = 0;
  if (symlink[0] == '/') {
    // needs prefixing
    ret = malloc(prefixLength + symlinkLength + 1);
    memcpy(ret, mnt->prefix, prefixLength);
    memcpy(&ret[prefixLength], symlink, symlinkLength);
    ret[prefixLength + symlinkLength] = '\0';
  } else if (symlink[0] == '!') {
    // doesn't need prefixing
    ret = malloc(symlinkLength + 1);
    memcpy(ret, &symlink[1], symlinkLength);
    ret[symlinkLength] = '\0';
  }

  return ret;
}
