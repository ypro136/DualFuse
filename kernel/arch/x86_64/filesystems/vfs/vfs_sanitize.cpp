#include <liballoc.h>
#include <string.h>
#include <task.h>
#include <utility.h>
#include <vfs.h>

// Generic file path/name sanitization
// Copyright (C) 2024 Panagiotis

char  root[] = "/";
char *file_system_strip_mount_point(const char *filename, MountPoint *mnt) {
  char *out = (char *)((size_t)filename + strlen(mnt->prefix) -
                       1); // -1 for putting start slash;

  if (out[0] != '\0')
    return out;
  else
    return root;
}

void file_system_sanitize_copy_safe(char *filename, char *safeFilename) {
  int i, j;
  for (i = 0, j = 0; filename[i] != '\0'; i++) {
    // double slashes
    if (filename[i] == '/' && filename[i + 1] == '/')
      continue;
    // slashes at the end
    if (filename[i] == '/' && filename[i + 1] == '\0')
      continue;
    if (filename[i] == '/' && filename[i + 1] == '.' &&
        (filename[i + 2] == '/' || filename[i + 2] == '\0')) {
      i++; // skip two!
      continue;
    }
    safeFilename[j] = filename[i];
    j++;
  }
  safeFilename[j] = '\0'; // null terminator
  if (j == 0) {
    // whoops
    safeFilename[0] = '/';
    safeFilename[1] = '\0';
  }
}

// prefix can be the current working directory yk
char *file_system_sanitize(char *prefix, char *filename) {
  char  *safeFilename = 0;
  size_t filenameSize = strlen(filename);
  if (filename[0] != '/') {
    // smth/[...]
    size_t cwdLen = strlen(prefix);

    safeFilename = (char *)malloc(cwdLen + 1 + filenameSize + 1);

    // prepend path
    int offset = 0;
    memcpy(safeFilename, prefix, cwdLen);
    if (prefix[0] == '/' && prefix[1] != '\0') {
      safeFilename[cwdLen] = '/';
      offset++;
    }
    // below: memcpy(safeFilename + cwdLen, filename, filenameSize);
    file_system_sanitize_copy_safe(filename, safeFilename + offset + cwdLen);
    file_system_sanitize_copy_safe(safeFilename, safeFilename); // sanitize total
    safeFilename[cwdLen + offset + filenameSize] = '\0';
  } else {
    // /smth/[...]
    safeFilename = (char *)malloc(filenameSize + 1);
    file_system_sanitize_copy_safe(filename, safeFilename);
  }

  // will be re-used throughout :")
  size_t finalSize = strlen(safeFilename);

  // in between [...]/../[...] or [...]/.. fixup
  for (int i = 0; i < finalSize; i++) {
    if (safeFilename[i] == '/' && safeFilename[i + 1] == '.' &&
        safeFilename[i + 2] == '.' &&
        (safeFilename[i + 3] == '/' || safeFilename[i + 3] == '\0')) {
      int indexToCopyInto = 0;
      for (indexToCopyInto = i - 1; indexToCopyInto >= 0; indexToCopyInto--) {
        if (safeFilename[indexToCopyInto] == '/')
          break;
      }
      if (indexToCopyInto < 0)
        indexToCopyInto = 0;
      // +1 for the null terminator
      size_t lentocopy = finalSize - indexToCopyInto - 3 + 1;
      memcpy(safeFilename + indexToCopyInto, safeFilename + i + 3, lentocopy);
      finalSize -= (i + 3) - indexToCopyInto;
      i = indexToCopyInto - 1; // we copied there...
    }
  }

  if (finalSize == 0) {
    // whoops
    safeFilename[0] = '/';
    safeFilename[1] = '\0';
  }

  return safeFilename;
}
