#include <dents.h>
#include <utility.h>


DENTS_RES dents_add(void *buffStart, struct linux_dirent64 **dirp,
                   int *allocatedlimit, unsigned int hardlimit, char *filename,
                   size_t filenameLength, size_t inode, unsigned char type) {
  size_t reclen = 23 + filenameLength + 1;
  if ((*allocatedlimit + reclen + 2) > hardlimit) {
    if (*allocatedlimit)
      return DENTS_RETURN;
    else
      return DENTS_NO_SPACE;
  }

  (*dirp)->d_reclen = reclen;
  (*dirp)->d_ino = inode;
  (*dirp)->d_off = rand(); // xd
  (*dirp)->d_type = type;

  memcpy((*dirp)->d_name, filename, filenameLength);
  (*dirp)->d_name[filenameLength] = '\0';

  *allocatedlimit = *allocatedlimit + reclen;
  *dirp = (struct linux_dirent64 *)((size_t)(*dirp) + reclen);

  return DENTS_SUCCESS;
}
