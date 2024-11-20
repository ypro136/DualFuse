#include <vfs.h>

#include <ext2.h>
#include <fat32.h>
#include <linux.h>
#include <liballoc.h>
#include <task.h>

#include <utility.h>
#include <hcf.hpp>

// todo: special files & timestamps
bool file_system_stat(OpenFile *fd, stat *target) {
  if (!fd->handlers->stat) {
    printf("[vfs] Lacks stat handler fd{%d}!\n", fd->id);
    Halt();
  }
  return fd->handlers->stat(fd, target) == 0;
}

bool file_system_stat_by_filename(void *task, char *filename, stat *target) {
  char *safeFilename = file_system_sanitize(((Task *)task)->cwd, filename);

  MountPoint *mnt = file_system_determine_mount_point(safeFilename);
  bool        ret = false;
  char       *strippedFilename = file_system_strip_mount_point(safeFilename, mnt);

  if (!mnt->stat)
  {
    free(safeFilename);
  }

  char *symlink = 0;
  ret = mnt->stat(mnt, strippedFilename, target, &symlink);


  if (!ret && symlink) {
    char *symlinkResolved = file_system_resolve_symlink(mnt, symlink);
    free(symlink);
    bool ret = file_system_stat_by_filename(task, symlinkResolved, target);
    free(symlinkResolved);
    return ret;
  }
  return ret;
}

bool file_system_Lstat_by_filename(void *task, char *filename, stat *target) {
  char *safeFilename = file_system_sanitize(((Task *)task)->cwd, filename);

  MountPoint *mnt = file_system_determine_mount_point(safeFilename);
  bool        ret = false;
  char       *strippedFilename = file_system_strip_mount_point(safeFilename, mnt);

  if (!mnt->lstat)
  {
    free(safeFilename);
  }

  char *symlink = 0;
  ret = mnt->lstat(mnt, strippedFilename, target, &symlink);


  if (!ret && symlink) {
    char *symlinkResolved = file_system_resolve_symlink(mnt, symlink);
    free(symlink);
    bool ret = file_system_Lstat_by_filename(task, symlinkResolved, target);
    free(symlinkResolved);
    return ret;
  }
  return ret;
}
