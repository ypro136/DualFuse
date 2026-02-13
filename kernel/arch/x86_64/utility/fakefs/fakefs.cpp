#include <dents.h>
#include <fakefs.h>
#include <linked_list.h>
#include <liballoc.h>
#include <string.h>
#include <task.h>

FakefsFile *fakefsAddFile(Fakefs *fakefs, FakefsFile *under, char *filename,
                          char *symlink, uint16_t filetype,
                          VfsHandlers *handlers) {
  FakefsFile *file =
      (FakefsFile *)LinkedListAllocate(&under->inner, sizeof(FakefsFile));
  LinkedListInit(&file->inner, sizeof(FakefsFile));

  file->filename = filename;
  file->filenameLength = strlen(filename);
  file->filetype = filetype;
  file->inode = ++fakefs->lastInode;
  file->handlers = handlers;

  if (symlink) {
    file->symlink = symlink;
    file->symlinkLength = strlen(symlink);
  }

  return file;
}

void fakefsAttachFile(FakefsFile *file, void *ptr, int size) {
  file->extra = ptr;
  file->size = size;
}

FakefsFile *fakefsTraverse(FakefsFile *start, char *search,
                           size_t searchLength) {
  FakefsFile *browse = start;
  while (browse) {
    if (browse->filename[0] == '*')
      break;
    if (memcmp(search, browse->filename,
               MAX(browse->filenameLength, searchLength)) == 0)
      break;
    browse = (FakefsFile *)browse->_ll.next;
  }

  return browse;
}

void fakefsSetupRoot(LLcontrol *root) {
  FakefsFile fakefsRoot = {
      ._ll = {0},
      .inner = {0},
      .filename = "/",
      .filenameLength = 1,
      .filetype = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR,
      .inode = 0,
      .symlink = 0,
      .symlinkLength = 0,
      .size = 3620,
      .extra = NULL,
      .handlers = &fakefsRootHandlers
  };

  LinkedListInit(root, sizeof(FakefsFile));
  FakefsFile *target = LinkedListAllocate(root, sizeof(FakefsFile));
  memcpy(target, &fakefsRoot, sizeof(FakefsFile));
  LinkedListInit(&target->inner, sizeof(FakefsFile));
}

FakefsFile *fakefsTraversePath(FakefsFile *start, char *path) {
  FakefsFile *fakefs = (FakefsFile *)start->inner.firstObject;
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

      FakefsFile *res = fakefsTraverse(fakefs, path + lastslash + 1, length);

      // return fail or last's success
      if (!res || i == (len - 1))
        return res;

      fakefs = (FakefsFile *)res->inner.firstObject;
      lastslash = i;
    }
  }

  // will never be reached but whatever
  return 0;
}

size_t fakefsOpen(char *filename, int flags, int mode, OpenFile *target,
                  char **symlinkResolve) {
  MountPoint    *mnt = target->mountPoint;
  FakefsOverlay *fakefs = (FakefsOverlay *)mnt->fsInfo;

  FakefsFile *file = fakefsTraversePath(
      (FakefsFile *)fakefs->fakefs->rootFile.firstObject, filename);
  if (!file) {
    // printf("! %s\n", filename);
    return ERR(ENOENT);
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

void fakefsStatGeneric(FakefsFile *file, struct stat *target) {
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

bool fakefsStat(MountPoint *mnt, char *filename, struct stat *target,
                char **symlinkResolve) {
  FakefsOverlay *fakefs = (FakefsOverlay *)mnt->fsInfo;
  FakefsFile    *file = fakefsTraversePath(
      (FakefsFile *)fakefs->fakefs->rootFile.firstObject, filename);
  if (!file)
    return false;

  fakefsStatGeneric(file, target);

  return true;
}

size_t fakefsFstat(OpenFile *fd, stat *target) {
  FakefsFile *file = fd->fake_file_system;
  fakefsStatGeneric(file, target);
  return 0;
}

bool fakefsLstat(MountPoint *mnt, char *filename, struct stat *target,
                 char **symlinkResolve) {
  // todo (when we got actual symlinks lol)
  return fakefsStat(mnt, filename, target, symlinkResolve);
}

VfsHandlers fakefsHandlers = {
    .read = 0,
    .write = 0,
    .seek = NULL,
    .ioctl = 0,
    .stat = 0,
    .mmap = 0,
    .getdents64 = 0,
    .getFilesize = NULL,
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
    .duplicate = 0,
    .open = fakefsOpen,
    .close = 0
};

size_t fakefsReadlink(MountPoint *mnt, char *path, char *buf, int size,
                      char **symlinkResolve) {
  if (size < 1)
    return ERR(EINVAL);

  FakefsOverlay *fakefs = (FakefsOverlay *)mnt->fsInfo;
  FakefsFile    *entry = fakefsTraversePath(
      (FakefsFile *)fakefs->fakefs->rootFile.firstObject, path);
  if (!entry)
    return ERR(ENOENT);

  if (!(entry->filetype & S_IFLNK))
    return ERR(EINVAL);
  assert(entry->symlink && entry->symlinkLength > 0);

  size_t out = MIN(entry->symlinkLength, size);
  memcpy(buf, entry->symlink, out);
  return out;
}

size_t fakefsGetDents64(OpenFile *fd, struct linux_dirent64 *start,
                        unsigned int hardlimit) {
  FakefsOverlay *fakefs = (FakefsOverlay *)fd->mountPoint->fsInfo;

  FakefsFile *weAt = fakefsTraversePath(
      (FakefsFile *)fakefs->fakefs->rootFile.firstObject, fd->dirname);

  if (!fd->tmp1)
    fd->tmp1 = (size_t)weAt->inner.firstObject;

  if (!fd->tmp1)
    fd->tmp1 = (size_t)(-1); // in case it's empty

  struct linux_dirent64 *dirp = (struct linux_dirent64 *)start;
  size_t                 allocatedlimit = 0;
  bool done = false;

  while (fd->tmp1 != (size_t)(-1) && !done) {
    FakefsFile *current = (FakefsFile *)fd->tmp1;
    
    if (current->filename[0] != '*') {
      DENTS_RES res =
          dents_add(start, &dirp, (int *)&allocatedlimit, hardlimit, current->filename,
                   current->filenameLength, current->inode, 0); // todo: type

      if (res == DENTS_NO_SPACE) {
        allocatedlimit = ERR(EINVAL);
        done = true;
      } else if (res == DENTS_RETURN) {
        done = true;
      }
    }

    if (!done) {
      fd->tmp1 = (size_t)current->_ll.next;
      if (!fd->tmp1)
        fd->tmp1 = (size_t)(-1);
    }
  }

  return allocatedlimit;
}

// taking in mind that void *extra points to a null terminated string
size_t fakefsSimpleRead(OpenFile *fd, uint8_t *out, size_t limit) {
  FakefsFile *file = (FakefsFile *)fd->fake_file_system;
  if (!file->extra) {
    printf("[vfs::fakefs] simple read failed! no extra! FATAL!\n");
    Halt();
  }

  char *in = (char *)((size_t)file->extra + fd->pointer);
  int   cnt = 0;
  for (int i = 0; i < limit; i++) {
    if (!in[i])
      break;

    out[i] = in[i];
    fd->pointer++;
    cnt++;
  }

  return cnt;
}

size_t fakefsSimpleSeek(OpenFile *file, size_t target, long int offset,
                        int whence) {
  // we're using the official ->pointer so no need to worry about much
  file->pointer = target;
  return 0;
}

VfsHandlers fakefsNoHandlers = {0};

VfsHandlers fakefsRootHandlers = {
    .read = 0,
    .write = 0,
    .seek = NULL,
    .ioctl = 0,
    .stat = fakefsFstat,
    .mmap = 0,
    .getdents64 = fakefsGetDents64,
    .getFilesize = NULL,
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
    .duplicate = 0,
    .open = 0,
    .close = 0
};

VfsHandlers fakefsSimpleReadHandlers = {
    .read = fakefsSimpleRead,
    .write = 0,
    .seek = fakefsSimpleSeek,
    .ioctl = 0,
    .stat = fakefsFstat,
    .mmap = 0,
    .getdents64 = 0,
    .getFilesize = NULL,
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
    .duplicate = 0,
    .open = 0,
    .close = 0
};