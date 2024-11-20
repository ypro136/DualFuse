#include <types.h>
#include <vfs.h>

#ifndef NULL_H
#define NULL_H

extern VfsHandlers handleNull;

typedef struct FakefsFile {
  struct FakefsFile *next;
  struct FakefsFile *inner;

  char *filename;
  int   filenameLength;

  uint16_t filetype;
  uint64_t inode;

  char *symlink;
  int   symlinkLength;

  int   size;
  void *extra;

  VfsHandlers *handlers;
} FakefsFile;

typedef struct Fakefs {
  FakefsFile *rootFile;
  uint64_t    lastInode;
} Fakefs;

typedef struct FakefsOverlay {
  Fakefs *fake_file_system;
} FakefsOverlay;

void        fake_file_system_setup_root(FakefsFile **ptr);
FakefsFile *fake_file_system_add_file(Fakefs *fake_file_system, FakefsFile *under, char *filename,
                          char *symlink, uint16_t filetype,
                          VfsHandlers *handlers);
void        fake_file_system_attach_file(FakefsFile *file, void *ptr, int size);
bool        fake_file_system_stat(MountPoint *mnt, char *filename, struct stat *target,
                       char **symlinkResolve);
bool        fake_file_system_lstat(MountPoint *mnt, char *filename, struct stat *target,
                        char **symlinkResolve);
int         fake_file_systemFstat(OpenFile *fd, stat *target);
int         fake_file_systemSimpleRead(OpenFile *fd, uint8_t *out, size_t limit);

extern VfsHandlers fake_file_systemHandlers;
extern VfsHandlers fake_file_systemRootHandlers;
extern VfsHandlers fake_file_systemSimpleReadHandlers;

#endif
