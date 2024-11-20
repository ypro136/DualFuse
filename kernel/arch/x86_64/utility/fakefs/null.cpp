#include <fakefs.h>
#include <task.h>
#include <utility.h>
#include<hcf.hpp>

int nullRead(OpenFile *fd, uint8_t *out, size_t limit) { return 0; }
int nullWrite(OpenFile *fd, uint8_t *in, size_t limit) { return limit; }

int nullStat(OpenFile *fd, stat *target) {
  target->st_dev = 70;
  target->st_ino = rand(); // todo!
  target->st_mode = S_IFCHR | S_IRUSR | S_IWUSR;
  target->st_nlink = 1;
  target->st_uid = 0;
  target->st_gid = 0;
  target->st_rdev = 0;
  target->st_blksize = 0x1000;
  target->st_size = 0;
  target->st_blocks = CEILING_DIVISION(target->st_size, 512);
  target->st_atime = 69;
  target->st_mtime = 69;
  target->st_ctime = 69;

  return 0;
}

int  nullIoctl(OpenFile *fd, uint64_t request, void *arg) { return -ENOTTY; }
bool nullDuplicate() { return true; }

size_t nullMmap() {
  printf("[/dev/null] Tried to mmap?\n");
  return (size_t)-1;
}

VfsHandlers handleNull = {
                          .read = nullRead,
                          .write = nullWrite,
                          .ioctl = nullIoctl,
                          .stat = nullStat,
                          .mmap = nullMmap,
                          .getdents64 = 0,
                          .duplicate = nullDuplicate
                          };
