#include <syscalls.h>

#include <fat32.h>
#include <linked_list.h>
#include <linux.h>
#include <liballoc.h>
#include <task.h>

#include <utility.h>
#include <hcf.hpp>

#define SYSCALL_READ 0
static int syscallRead(int fd, char *str, uint32_t count) {
  if (!count)
    return 0;
  OpenFile *browse = file_system_user_get_node(currentTask, fd);
  if (!browse) {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::read] FAIL! Couldn't find file! fd{%d}\n", fd);
#endif
    return -EBADF;
  }
  uint32_t read = file_system_read(browse, (uint8_t *)str, count);
  return read;
}

#define SYSCALL_WRITE 1
static int syscallWrite(int fd, char *str, uint32_t count) {
  if (!count)
    return 0;
  OpenFile *browse = file_system_user_get_node(currentTask, fd);
  if (!browse) {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::write] FAIL! Couldn't find file! fd{%d}\n", fd);
#endif
    return -EBADF;
  }

  uint32_t writtenBytes = file_system_write(browse, (uint8_t *)str, count);
  return writtenBytes;
}

#define SYSCALL_OPEN 2
static int syscallOpen(char *filename, int flags, int mode) {
#if DEBUG_SYSCALLS_EXTRA
  printf("[syscalls::open] filename{%s}\n", filename);
#endif
  if (!filename) {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::open] FAIL! No filename given... yeah.\n");
#endif
    return -1;
  }

  int ret = file_system_user_open(currentTask, filename, flags, mode);
#if DEBUG_SYSCALLS_FAILS
  if (ret < 0)
    printf("[syscalls::open] FAIL! Couldn't open file! filename{%s}\n",
           filename);
#endif

  return ret;
}

#define SYSCALL_CLOSE 3
static int syscallClose(int fd) { return file_system_user_close(currentTask, fd); }

#define SYSCALL_STAT 4
static int syscallStat(char *filename, stat *statbuf) {
#if DEBUG_SYSCALLS_EXTRA
  printf("[syscalls::stat] filename{%s}\n", filename);
#endif
  bool ret = file_system_stat_by_filename(currentTask, filename, statbuf);
  if (!ret) {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::stat] FAIL! Couldn't stat() file! filename{%s}\n",
           filename);
#endif
    return -ENOENT;
  }

  return 0;
}

#define SYSCALL_FSTAT 5
static int syscallFstat(int fd, stat *statbuf) {
  OpenFile *file = file_system_user_get_node(currentTask, fd);
  if (!file)
    return -EBADF;

  bool ret = file_system_stat(file, statbuf);
  if (!ret) {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::fstat] FAIL! Couldn't fstat() file! fd{%d}\n", fd);
#endif
    return -ENOENT;
  }

  return 0;
}

#define SYSCALL_LSTAT 6
static int syscallLstat(char *filename, stat *statbuf) {
  bool ret = file_system_Lstat_by_filename(currentTask, filename, statbuf);
  if (!ret) {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::lstat] FAIL! Couldn't lstat() file! filename{%d}\n",
           filename);
#endif
    return -ENOENT;
  }

  return 0;
}

#define SYSCALL_LSEEK 8
static int syscallLseek(uint32_t file, int offset, int whence) {
  return file_system_user_seek(currentTask, file, offset, whence);
}

#define SYSCALL_IOCTL 16
static int syscallIoctl(int fd, unsigned long request, void *arg) {
  OpenFile *browse = file_system_user_get_node(currentTask, fd);
  if (!browse) {
#if DEBUG_SYSCALLS_FAILS
    printf(
        "[syscalls::ioctl] FAIL! Tried to manipulate non-special file! fd{%d} "
        "req{%lx} arg{%lx}\n",
        fd, request, arg);
#endif
    return -EBADF;
  }

  if (!browse->handlers->ioctl)
    return -ENOTTY;

  int ret = browse->handlers->ioctl(browse, request, arg);

#if DEBUG_SYSCALLS_STUB
  if (ret < 0)
    printf("[syscalls::ioctl] UNIMPLEMENTED! fd{%d} req{%lx} arg{%lx}\n", fd,
           request, arg);
#endif

  return ret;
}

#define SYSCALL_PREAD64 17
static int syscallPread64(uint64_t fd, char *buff, size_t count, size_t pos) {
  int seekOp = syscallLseek(fd, pos, SEEK_SET);
  if (seekOp < 0)
    return seekOp;
  int readOp = syscallRead(fd, buff, count);
  return readOp;
}

#define SYSCALL_WRITEV 20
static int syscallWriteV(uint32_t fd, iovec *iov, uint32_t ioVcnt) {
  int cnt = 0;
  for (int i = 0; i < ioVcnt; i++) {
    iovec *curr = (iovec *)((size_t)iov + i * sizeof(iovec));

#if DEBUG_SYSCALLS_EXTRA
    printf("[syscalls::writev(%d)] fd{%d} iov_base{%x} iov_len{%x}\n", i, fd,
           curr->iov_base, curr->iov_len);
#endif
    int singleCnt = syscallWrite(fd, curr->iov_base, curr->iov_len);
    if (singleCnt < 0)
      return singleCnt;

    cnt += singleCnt;
  }

  return cnt;
}

#define SYSCALL_READV 19
static int syscallReadV(uint32_t fd, iovec *iov, uint32_t ioVcnt) {
  int cnt = 0;
  for (int i = 0; i < ioVcnt; i++) {
    iovec *curr = (iovec *)((size_t)iov + i * sizeof(iovec));

#if DEBUG_SYSCALLS_EXTRA
    printf("[syscalls::readv(%d)] fd{%d} iov_base{%x} iov_len{%x}\n", i, fd,
           curr->iov_base, curr->iov_len);
#endif
    int singleCnt = syscallRead(fd, curr->iov_base, curr->iov_len);
    if (singleCnt < 0)
      return singleCnt;

    cnt += singleCnt;
  }

  return cnt;
}

#define SYSCALL_ACCESS 21
static int syscallAccess(char *filename, int mode) {
  struct stat buf;
  return syscallStat(filename, &buf);
}

#define SYSCALL_DUP 32
static int syscallDup(uint32_t fd) {
  OpenFile *file = file_system_user_get_node(currentTask, fd);
  if (!file)
    return -1;

  OpenFile *new_OpenFile = file_system_user_duplicate_node(currentTask, file);
  new_OpenFile->closeOnExec = 0; // does not persist

  return new_OpenFile ? new_OpenFile->id : -1;
}

#define SYSCALL_DUP2 33
static int syscallDup2(uint32_t oldFd, uint32_t newFd) {
  OpenFile *realFile = file_system_user_get_node(currentTask, oldFd);
  if (!realFile)
    return -EBADF;

  if (oldFd == newFd)
    return newFd;

  if (file_system_user_get_node(currentTask, newFd))
    file_system_user_close(currentTask, newFd);

  // OpenFile    *realFile = currentTask->firstFile;
  OpenFile *targetFile = file_system_user_duplicate_node_unsafe(realFile);
  targetFile->closeOnExec = 0; // does not persist

  linkedlist_push_front_unsafe((void **)(&currentTask->firstFile), targetFile);

  targetFile->id = newFd;

#if DEBUG_SYSCALLS_STUB
  printf("[syscalls::dup2] wonky... old{%d} new{%d}\n", oldFd, newFd);
#endif
  return newFd;
}

#define SYSCALL_FCNTL 72
static int syscallFcntl(int fd, int cmd, uint64_t arg) {
  OpenFile *file = file_system_user_get_node(currentTask, fd);
  if (!file)
    return -EBADF;
  switch (cmd) {
  case F_GETFD:
    return file->closeOnExec;
    break;
  case F_SETFD:
    file->closeOnExec = !file->closeOnExec;
    return 0;
    break;
  case F_DUPFD:
    (void)arg; // todo: not ignore arg
    return syscallDup(fd);
    break;
  case F_GETFL:
    return file->flags;
    break;
  case F_SETFL: {
    int validFlags = O_APPEND | FASYNC | O_DIRECT | O_NOATIME | O_NONBLOCK;
    file->flags &= ~validFlags;
    file->flags |= arg & validFlags;
    return 0;
    break;
  }
  case F_DUPFD_CLOEXEC: {
    int targ = syscallDup(fd);
    if (targ < 0)
      return targ;

    int res = syscallFcntl(targ, F_SETFD, FD_CLOEXEC);
    if (res < 0)
      return res;

    return targ;
  }
  default:
#if DEBUG_SYSCALLS_STUB
    printf("[syscalls::fcntl] cmd{%d} not implemented!\n", cmd);
#endif
    return -1;
    break;
  }
}

#define SYSCALL_MKDIR 83
static int syscallMkdir(char *path, uint32_t mode) {
  mode &= ~(currentTask->umask);
  return file_system_mkdir(currentTask, path, mode);
}

#define SYSCALL_READLINK 89
static int syscallReadlink(char *path, char *buf, int size) {
  return file_system_read_link(currentTask, path, buf, size);
}

#define SYSCALL_UMASK 95
static int syscallUmask(uint32_t mask) {
  int old = currentTask->umask;
  currentTask->umask = mask & 0777;
  return old;
}

#define SYSCALL_GETDENTS64 217
static int syscallGetdents64(unsigned int fd, struct linux_dirent64 *dirp,
                             unsigned int count) {
  OpenFile *browse = file_system_user_get_node(currentTask, fd);
  if (!browse) {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::getdents64] FAIL! Couldn't find file! fd{%d}\n", fd);
#endif
    return -EBADF;
  }
  if (!browse->handlers->getdents64)
    return -ENOTDIR;
  return browse->handlers->getdents64(browse, dirp, count);
}

#define FD_SETSIZE 1024

typedef unsigned long fd_mask;

typedef struct {
  unsigned long fds_bits[FD_SETSIZE / 8 / sizeof(long)];
} fd_set;

#define SYSCALL_PSELECT6 270
static int syscallPselect6(int nfds, fd_set *readfds, fd_set *writefds,
                           fd_set *exceptfds, struct timespec *timeout,
                           void *smthsignalthing) {
  if (timeout && !timeout->tv_nsec && !timeout->tv_sec)
    return 0;

#if DEBUG_SYSCALLS_FAILS
  printf("[syscalls::pselect6::wonky]\n");
#endif

  int amnt = 0;
  int bits_per_long = sizeof(unsigned long) * 8;
  if (readfds)
    for (int fd = 0; fd < nfds; fd++) {
      int index = fd / bits_per_long;
      int bit = fd % bits_per_long;
      if (readfds->fds_bits[index] & (1UL << bit)) {
        // todo: uhm.. poll?
        amnt++;
      }
    }
  if (writefds)
    for (int fd = 0; fd < nfds; fd++) {
      int index = fd / bits_per_long;
      int bit = fd % bits_per_long;
      if (writefds->fds_bits[index] & (1UL << bit)) {
        // todo: uhm.. poll?
        amnt++;
      }
    }
  if (exceptfds)
    for (int fd = 0; fd < nfds; fd++) {
      int index = fd / bits_per_long;
      int bit = fd % bits_per_long;
      if (exceptfds->fds_bits[index] & (1UL << bit)) {
        // todo: uhm.. poll?
        amnt++;
      }
    }

  // todo: timelimit (for when I actually poll)

  return amnt;
}

#define SYSCALL_SELECT 23
static int syscallSelect(int nfds, fd_set *readfds, fd_set *writefds,
                         fd_set *exceptfds, struct timeval *timeout) {
  if (timeout) {
    struct timespec timeoutformat = {.tv_sec = timeout->tv_sec,
                                     .tv_nsec = timeout->tv_usec * 1000};
    return syscallPselect6(nfds, readfds, writefds, exceptfds, &timeoutformat,
                           0);
  }
  return syscallPselect6(nfds, readfds, writefds, exceptfds, 0, 0);
}

#define SYSCALL_OPENAT 257
static int syscallOpenat(int dirfd, char *pathname, int flags, int mode) {
  if (pathname[0] == '\0') { // by fd
    return dirfd;
  } else if (pathname[0] == '/') { // by absolute pathname
    return syscallOpen(pathname, flags, mode);
  } else if (pathname[0] != '/') {
    if (dirfd == AT_FDCWD) { // relative to cwd
      return syscallOpen(pathname, flags, mode);
    } else {
#if DEBUG_SYSCALLS_STUB
      printf("[syscalls::openat] todo: partial sanitization!");
#endif
      return -1;
    }
  } else {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::openat] Unsupported!\n");
#endif
    return -1;
  }
  return -ENOSYS;
}

#define SYSCALL_MKDIRAT 258
static int syscallMkdirAt(int dirfd, char *pathname, int mode) {
  if (pathname[0] == '\0') { // by fd
    return dirfd;
  } else if (pathname[0] == '/') { // by absolute pathname
    return syscallMkdir(pathname, mode);
  } else if (pathname[0] != '/') {
    if (dirfd == AT_FDCWD) { // relative to cwd
      return syscallMkdir(pathname, mode);
    } else {
#if DEBUG_SYSCALLS_STUB
      printf("[syscalls::mkdirat] todo: partial sanitization!");
#endif
      return -1;
    }
  } else {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::mkdirat] Unsupported!\n");
#endif
    return -1;
  }
  return -ENOSYS;
}

#define SYSCALL_FACCESSAT 269
static int syscallFaccessat(int dirfd, char *pathname, int mode) {
  if (pathname[0] == '\0') { // by fd
    return 0;
  } else if (pathname[0] == '/') { // by absolute pathname
    return syscallAccess(pathname, mode);
  } else if (pathname[0] != '/') {
    if (dirfd == AT_FDCWD) { // relative to cwd
      return syscallAccess(pathname, mode);
    } else {
#if DEBUG_SYSCALLS_STUB
      printf("[syscalls::access] todo: partial sanitization!");
#endif
      return -1;
    }
  } else {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::access] Unsupported!\n");
#endif
    return -1;
  }
  return -ENOSYS;
}

#define SYSCALL_STATX 332
static int syscallStatx(int dirfd, char *pathname, int flags, uint32_t mask,
                        struct statx *buff) {
  struct stat simple = {0};
  if (pathname[0] == '\0') { // by fd
    OpenFile *file = file_system_user_get_node(currentTask, dirfd);
    if (!file)
      return -ENOENT;
    if (!file_system_stat(file, &simple))
      return -EBADF;
  } else if (pathname[0] == '/') { // by absolute pathname
    char *safeFilename = file_system_sanitize(currentTask->cwd, pathname);
    bool  ret = false;
    if (flags & AT_SYMLINK_NOFOLLOW)
      ret = file_system_Lstat_by_filename(currentTask, safeFilename, &simple);
    else
      ret = file_system_stat_by_filename(currentTask, safeFilename, &simple);
    free(safeFilename);
    if (!ret)
      return -ENOENT;
  } else if (pathname[0] != '/') {
    if (dirfd == AT_FDCWD) { // relative to cwd
      char *safeFilename = file_system_sanitize(currentTask->cwd, pathname);
      bool  ret = false;
      if (flags & AT_SYMLINK_NOFOLLOW)
        ret = file_system_Lstat_by_filename(currentTask, safeFilename, &simple);
      else
        ret = file_system_stat_by_filename(currentTask, safeFilename, &simple);
      free(safeFilename);
      if (!ret)
        return -ENOENT;
    } else {
#if DEBUG_SYSCALLS_STUB
      printf("[syscalls::statx] todo: partial sanitization!");
#endif
      return -1;
    }
  } else {
#if DEBUG_SYSCALLS_FAILS
    printf("[syscalls::statx] Unsupported!\n");
#endif
    return -1;
  }

  memset(buff, 0, sizeof(struct statx));

  buff->stx_mask = mask;
  buff->stx_blksize = simple.st_blksize;
  buff->stx_attributes = 0; // naw
  buff->stx_nlink = simple.st_nlink;
  buff->stx_uid = simple.st_uid;
  buff->stx_gid = simple.st_gid;
  buff->stx_mode = simple.st_mode;
  buff->stx_ino = simple.st_ino;
  buff->stx_size = simple.st_size;
  buff->stx_blocks = simple.st_blocks;
  buff->stx_attributes_mask = 0; // naw

  buff->stx_atime.tv_sec = simple.st_atime;
  buff->stx_atime.tv_nsec = simple.st_atimensec;

  // well, it's a creation time but whatever
  buff->stx_btime.tv_sec = simple.st_ctime;
  buff->stx_btime.tv_nsec = simple.st_ctimensec;

  buff->stx_ctime.tv_sec = simple.st_ctime;
  buff->stx_ctime.tv_nsec = simple.st_ctimensec;

  buff->stx_mtime.tv_sec = simple.st_mtime;
  buff->stx_mtime.tv_nsec = simple.st_mtimensec;

  // todo: special devices

  return 0;
}

// #define SYSCALL_FACCESSAT2 439
// static int syscallFaccessat2(int dirfd, char *pathname, int mode, int flags)
// {
//   return -1;
// }

void syscallRegFs() {
  register_syscall(SYSCALL_WRITE, syscallWrite);
  register_syscall(SYSCALL_READ, syscallRead);
  register_syscall(SYSCALL_OPEN, syscallOpen);
  register_syscall(SYSCALL_OPENAT, syscallOpenat);
  register_syscall(SYSCALL_MKDIRAT, syscallMkdirAt);
  register_syscall(SYSCALL_CLOSE, syscallClose);
  register_syscall(SYSCALL_LSEEK, syscallLseek);
  register_syscall(SYSCALL_STAT, syscallStat);
  register_syscall(SYSCALL_FSTAT, syscallFstat);
  register_syscall(SYSCALL_LSTAT, syscallLstat);
  register_syscall(SYSCALL_MKDIR, syscallMkdir);
  register_syscall(SYSCALL_UMASK, syscallUmask);
  register_syscall(SYSCALL_PREAD64, syscallPread64);

  register_syscall(SYSCALL_IOCTL, syscallIoctl);
  register_syscall(SYSCALL_READV, syscallReadV);
  register_syscall(SYSCALL_WRITEV, syscallWriteV);
  register_syscall(SYSCALL_DUP2, syscallDup2);
  register_syscall(SYSCALL_DUP, syscallDup);
  register_syscall(SYSCALL_ACCESS, syscallAccess);
  register_syscall(SYSCALL_FACCESSAT, syscallFaccessat);
  register_syscall(SYSCALL_GETDENTS64, syscallGetdents64);
  register_syscall(SYSCALL_PSELECT6, syscallPselect6);
  register_syscall(SYSCALL_SELECT, syscallSelect);
  register_syscall(SYSCALL_FCNTL, syscallFcntl);
  register_syscall(SYSCALL_STATX, syscallStatx);
  register_syscall(SYSCALL_READLINK, syscallReadlink);
  // registerSyscall(SYSCALL_FACCESSAT2, syscallFaccessat2);
}