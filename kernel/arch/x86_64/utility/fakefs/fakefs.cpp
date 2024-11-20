#include <dents.h>
#include <fakefs.h>
#include <linked_list.h>
#include <liballoc.h>

#include <string.h>
#include <task.h>

FakefsFile *fake_file_system_add_file(Fakefs *fake_file_system, FakefsFile *under, char *filename,
                          char *symlink, uint16_t filetype,
                          VfsHandlers *handlers) {
  FakefsFile *file = (FakefsFile *)linked_list_allocate((void **)(&under->inner),
                                                      sizeof(FakefsFile));

  file->filename = filename;
  file->filenameLength = strlen(filename);
  file->filetype = filetype;
  file->inode = ++fake_file_system->lastInode;
  file->handlers = handlers;

  if (symlink) {
    file->symlink = symlink;
    file->symlinkLength = strlen(symlink);
  }

  return file;
}

void fake_file_system_attach_file(FakefsFile *file, void *ptr, int size) {
  file->extra = ptr;
  file->size = size;
}

FakefsFile *fake_file_systemTraverse(FakefsFile *start, char *search,
                           size_t searchLength) {
  FakefsFile *browse = start;
  while (browse) {
    if (browse->filename[0] == '*')
      break;
    if (memcmp(search, browse->filename,
               MAX(browse->filenameLength, searchLength)) == 0)
      break;
    browse = browse->next;
  }

  return browse;
}

void fake_file_system_setup_root(FakefsFile **ptr) {
  FakefsFile fake_file_systemRoot = {
                           .next = 0,
                           .inner = 0,
                           .filename = "/",
                           .filenameLength = 1,
                           .filetype = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR,
                           .symlink = 0,
                           .symlinkLength = 0,
                           .size = 3620,
                           .handlers = &fake_file_systemRootHandlers
                           };



  *ptr = (FakefsFile *)malloc(sizeof(FakefsFile));
  memcpy(*ptr, &fake_file_systemRoot, sizeof(FakefsFile));
}

FakefsFile *fake_file_systemTraversePath(FakefsFile *start, char *path) {
  FakefsFile *fake_file_system = start->inner;
  size_t      len = strlen(path);

  if (len == 1) // meaning it's trying to open /
    return start;

  int lastslash = 0;
  for (int i = 1; i < len; i++) { // 1 to skip /[...]
    bool last = i == (len - 1);

    if (path[i] == '/' || last) {
      size_t length = i - lastslash - 1;
      if (last) // no need to remove trailing /
        length += 1;

      FakefsFile *res = fake_file_systemTraverse(fake_file_system, path + lastslash + 1, length);

      // return fail or last's success
      if (!res || i == (len - 1))
        return res;

      fake_file_system = res->inner;
      lastslash = i;
    }
  }

  // will never be reached but whatever
  return 0;
}

int fake_file_systemOpen(char *filename, int flags, int mode, OpenFile *target,
               char **symlinkResolve) {
  MountPoint    *mnt = target->mountPoint;
  FakefsOverlay *fake_file_system = (FakefsOverlay *)mnt->fsInfo;

  FakefsFile *file = fake_file_systemTraversePath(fake_file_system->fake_file_system->rootFile, filename);
  if (!file) {
    // printf("! %s\n", filename);
    return -ENOENT;
  }
  target->handlers = file->handlers;

  if (file->filetype | S_IFDIR) {
    // if it's a directory yk
    int len = strlen(filename) + 1;
    target->dirname = (char *)malloc(len);
    memcpy(target->dirname, filename, len);
  }

  if (file->handlers->open) {
    // if a specific open handler is in place
    int ret =
        file->handlers->open(filename, flags, mode, target, symlinkResolve);
    if (ret < 0)
      return ret;
  }

  target->fake_file_system = file;

  return 0;
}

void fake_file_system_statGeneric(FakefsFile *file, struct stat *target) {
  target->st_dev = 69;          // haha
  target->st_ino = file->inode; // could work
  target->st_mode = file->filetype;
  target->st_nlink = 1;
  target->st_uid = 0;
  target->st_gid = 0;
  target->st_rdev = 0;
  target->st_blksize = 0x1000;

  target->st_size = file->size;
  target->st_blocks =
      (CEILING_DIVISION(target->st_size, target->st_blksize) * target->st_blksize) /
      512;

  target->st_atime = 0;
  target->st_mtime = 0;
  target->st_ctime = 0;
}

bool fake_file_system_stat(MountPoint *mnt, char *filename, struct stat *target,
                char **symlinkResolve) {
  FakefsOverlay *fake_file_system = (FakefsOverlay *)mnt->fsInfo;
  FakefsFile    *file = fake_file_systemTraversePath(fake_file_system->fake_file_system->rootFile, filename);
  if (!file)
    return false;

  fake_file_system_statGeneric(file, target);

  return true;
}

int fake_file_systemFstat(OpenFile *fd, stat *target) {
  FakefsFile *file = fd->fake_file_system;
  fake_file_system_statGeneric(file, target);
  return 0;
}

bool fake_file_system_lstat(MountPoint *mnt, char *filename, struct stat *target,
                 char **symlinkResolve) {
  // todo (when we got actual symlinks lol)
  return fake_file_system_stat(mnt, filename, target, symlinkResolve);
}

VfsHandlers fake_file_systemHandlers = {
                              .read = 0,
                              .write = 0,
                              .ioctl = 0,
                              .stat = 0,
                              .mmap = 0,
                              .getdents64 = 0,
                              .duplicate = 0,
                              .open = fake_file_systemOpen,
                              .close = 0
                              };


int fake_file_systemGetDents64(OpenFile *fd, struct linux_dirent64 *start,
                     unsigned int hardlimit) {
  FakefsOverlay *fake_file_system = (FakefsOverlay *)fd->mountPoint->fsInfo;

  FakefsFile *weAt = fake_file_systemTraversePath(fake_file_system->fake_file_system->rootFile, fd->dirname);

  if (!fd->tmp1)
    fd->tmp1 = (size_t)weAt->inner;

  if (!fd->tmp1)
    fd->tmp1 = (size_t)(-1); // in case it's empty

  struct linux_dirent64 *dirp = (struct linux_dirent64 *)start;
  int                    allocatedlimit = 0;

  while (fd->tmp1 != (size_t)(-1)) {
    FakefsFile *current = (FakefsFile *)fd->tmp1;
    DENTS_RES   res =
        dents_add(start, &dirp, &allocatedlimit, hardlimit, current->filename,
                 current->filenameLength, current->inode, 0); // todo: type

    if (res == DENTS_NO_SPACE) {
      allocatedlimit = -EINVAL;
      goto cleanup;
    } else if (res == DENTS_RETURN)
      goto cleanup;

    fd->tmp1 = (size_t)current->next;
    if (!fd->tmp1)
      fd->tmp1 = (size_t)(-1);
  }

cleanup:
  return allocatedlimit;
}

// taking in mind that void *extra points to a null terminated string
int fake_file_systemSimpleRead(OpenFile *fd, uint8_t *out, size_t limit) {
  FakefsFile *file = (FakefsFile *)fd->fake_file_system;
  if (!file->extra) {
    printf("[vfs::fake_file_system] simple read failed! no extra! FATAL!\n");
    Halt();
  }

  char *in = (char *)((size_t)file->extra + fd->pointer);
  int   cnt = 0;
  for (int i = 0; i < limit; i++) {
    if (!in[i])
      goto cleanup;

    out[i] = in[i];
    fd->pointer++;
    cnt++;
  }

cleanup:
  return cnt;
}

size_t fake_file_systemSimpleSeek(OpenFile *file, size_t target, long int offset,
                        int whence) {
  // we're using the official ->pointer so no need to worry about much
  file->pointer = target;
  return 0;
}

VfsHandlers fake_file_systemRootHandlers = {
                                  .read = 0,
                                  .write = 0,
                                  .ioctl = 0,
                                  .stat = 0,
                                  .mmap = 0,
                                  .getdents64 = fake_file_systemGetDents64,
                                  .duplicate = 0,
                                  .open = 0,
                                  .close = 0
                                  };

VfsHandlers fake_file_systemSimpleReadHandlers = {
                                        .read = fake_file_systemSimpleRead,
                                        .write = 0,
                                        .seek = fake_file_systemSimpleSeek,
                                        .ioctl = 0,
                                        .stat = fake_file_systemFstat,
                                        .mmap = 0,
                                        .getdents64 = 0,
                                        .duplicate = 0
                                        };


