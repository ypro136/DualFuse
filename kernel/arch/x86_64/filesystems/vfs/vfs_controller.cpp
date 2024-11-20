#include <vfs.h>

#include <disk.h>
#include <ext2.h>
#include <fat32.h>
#include <data_structures/linked_list.h>
#include <liballoc.h>
#include <string.h>
#include <task.h>

#include <utility.h>
#include <hcf.hpp>



OpenFile *file_system_register_node(Task *task) {
  spinlock_cnt_write_acquire(&task->WLOCK_FILES);
  OpenFile *ret =
      linked_list_allocate((void **)&task->firstFile, sizeof(OpenFile));
  spinlock_cnt_write_release(&task->WLOCK_FILES);
  return ret;
}

bool file_system_unregister_node(Task *task, OpenFile *file) {
  spinlock_cnt_write_acquire(&task->WLOCK_FILES);
  bool ret = linkedlist_unregister((void **)&task->firstFile, file);
  spinlock_cnt_write_release(&task->WLOCK_FILES);
  return ret;
}

// TODO! flags! modes!
// todo: openId in a bitmap or smth, per task/kernel

char     *prefix = "/";
int       openId = 3;
OpenFile *file_system_open_generic(char *filename, Task *task, int flags, int mode) {
  char *safeFilename = file_system_sanitize(task ? task->cwd : prefix, filename);

  OpenFile *target = file_system_register_node(task);
  target->id = openId++;
  target->mode = mode;
  target->flags = flags;

  target->pointer = 0;
  target->tmp1 = 0;

  MountPoint *mnt = file_system_determine_mount_point(safeFilename);
  if (!mnt) {
    // no mountpoint for this
    file_system_unregister_node(task, target);
    free(target);
    free(safeFilename);
    return 0;
  }
  target->mountPoint = mnt;
  target->handlers = mnt->handlers;

  if (flags & O_CLOEXEC)
    target->closeOnExec = true;

  char *strippedFilename = file_system_strip_mount_point(safeFilename, mnt);
  // check for open handler
  if (target->handlers->open) {
    char *symlink = 0;
    int   ret =
        target->handlers->open(strippedFilename, flags, mode, target, &symlink);
    if (ret < 0) {
      // failed to open
      file_system_unregister_node(task, target);
      free(target);
      free(safeFilename);

      if (symlink && ret != -ELOOP) {
        // we came across a symbolic link
        char *symlinkResolved = file_system_resolve_symlink(mnt, symlink);
        free(symlink);
        OpenFile *res = file_system_open_generic(symlinkResolved, task, flags, mode);
        free(symlinkResolved);
        return res;
      }
      return (OpenFile *)((size_t)(-ret));
    }
    free(safeFilename);
  }
  return target;
}

// returns an ORPHAN!
OpenFile *file_system_user_duplicate_node_unsafe(OpenFile *original) {
  OpenFile *orphan = (OpenFile *)malloc(sizeof(OpenFile));
  orphan->next = 0; // duh

  memcpy((void *)((size_t)orphan + sizeof(orphan->next)),
         (void *)((size_t)original + sizeof(original->next)),
         sizeof(OpenFile) - sizeof(orphan->next));

  if (original->handlers->duplicate &&
      !original->handlers->duplicate(original, orphan)) {
    free(orphan);
    return 0;
  }

  return orphan;
}

OpenFile *file_system_user_duplicate_node(void *taskPtr, OpenFile *original) {
  Task *task = (Task *)taskPtr;

  OpenFile *target = file_system_user_duplicate_node_unsafe(original);
  target->id = openId++;

  spinlock_cnt_write_acquire(&task->WLOCK_FILES);
  linkedlist_push_front_unsafe((void **)(&task->firstFile), target);
  spinlock_cnt_write_release(&task->WLOCK_FILES);

  return target;
}

OpenFile *file_system_user_get_node(void *task, int fd) {
  Task *target = (Task *)task;
  spinlock_cnt_read_acquire(&target->WLOCK_FILES);
  OpenFile *browse = target->firstFile;
  while (browse) {
    if (browse->id == fd)
      break;

    browse = browse->next;
  }
  spinlock_cnt_read_release(&target->WLOCK_FILES);

  return browse;
}

OpenFile *file_system_kernel_open(char *filename, int flags, uint32_t mode) {
  Task     *target = task_get(KERNEL_TASK_ID);
  OpenFile *ret = file_system_open_generic(filename, target, flags, mode);
  if ((size_t)(ret) < 1024)
    return 0;
  return ret;
}

int file_system_user_open(void *task, char *filename, int flags, int mode) {
  if (flags & FASYNC) {
    printf("[syscalls::fs] FATAL! Tried to open %s with O_ASYNC!\n", filename);
    return -ENOSYS;
  }
  OpenFile *file = file_system_open_generic(filename, (Task *)task, flags, mode);
  if ((size_t)(file) < 1024)
    return -((size_t)file);

  return file->id;
}

bool file_system_close_generic(OpenFile *file, Task *task) {
  file_system_unregister_node(task, file);

  bool res = file->handlers->close ? file->handlers->close(file) : true;
  free(file);
  return res;
}

bool file_system_kernel_close(OpenFile *file) {
  Task *target = task_get(KERNEL_TASK_ID);
  return file_system_close_generic(file, target);
}

int file_system_user_close(void *task, int fd) {
  OpenFile *file = file_system_user_get_node(task, fd);
  if (!file)
    return -EBADF;
  bool res = file_system_close_generic(file, (Task *)task);
  if (res)
    return 0;
  else
    return -1;
}

size_t file_system_get_filesize(OpenFile *file) {
  return file->handlers->getFilesize(file);
}

uint32_t file_system_read(OpenFile *file, uint8_t *out, uint32_t limit) {
  if (!file->handlers->read)
    return -EBADF;
  return file->handlers->read(file, out, limit);
}

uint32_t file_system_write(OpenFile *file, uint8_t *in, uint32_t limit) {
  if (!(file->flags & O_RDWR) && !(file->flags & O_WRONLY))
    return -EBADF;
  if (!file->handlers->write)
    return -EBADF;
  return file->handlers->write(file, in, limit);
}

void file_system_read_full_file(OpenFile *file, uint8_t *out) {
  file_system_read(file, out, file_system_get_filesize(file));
}

int file_system_user_seek(void *task, uint32_t fd, int offset, int whence) {
  OpenFile *file = file_system_user_get_node(task, fd);
  if (!file) // todo "special"
    return -1;
  int target = offset;
  if (whence == SEEK_SET)
    target += 0;
  else if (whence == SEEK_CURR)
    target += file->pointer;
  else if (whence == SEEK_END)
    target += file_system_get_filesize(file);

  if (!file->handlers->seek)
    return -ESPIPE;

  return file->handlers->seek(file, target, offset, whence);
}

int file_system_read_link(void *task, char *path, char *buf, int size) {
  Task       *target = task;
  char       *safeFilename = file_system_sanitize(target->cwd, path);
  MountPoint *mnt = file_system_determine_mount_point(safeFilename);
  int         ret = -1;

  char *symlink = 0;
  switch (mnt->filesystem) {
  case FS_FATFS:
    ret = -EINVAL;
    break;
  case FS_EXT2:
    ret =
        ext2_read_link((Ext2 *)(mnt->fsInfo), safeFilename, buf, size, &symlink);
    break;
  default:
    printf("[vfs] Tried to readLink() with bad filesystem! id{%d}\n",
           mnt->filesystem);
    break;
  }

  free(safeFilename);

  if (symlink) {
    char *symlinkResolved = file_system_resolve_symlink(mnt, symlink);
    free(symlink);
    ret = file_system_read_link(task, symlinkResolved, buf, size);
    free(symlinkResolved);
  }
  return ret;
}

int file_system_mkdir(void *task, char *path, uint32_t mode) {
  Task       *target = (Task *)task;
  char       *safeFilename = file_system_sanitize(target->cwd, path);
  MountPoint *mnt = file_system_determine_mount_point(safeFilename);

  int ret = 0;

  char *symlink = 0;
  if (mnt->mkdir) {
    ret = mnt->mkdir(mnt, safeFilename, mode, &symlink);
  } else {
    ret = -EROFS;
  }

  free(safeFilename);

  if (symlink) {
    char *symlinkResolved = file_system_resolve_symlink(mnt, symlink);
    free(symlink);
    ret = file_system_mkdir(task, symlinkResolved, mode);
    free(symlinkResolved);
  }

  return ret;
}
