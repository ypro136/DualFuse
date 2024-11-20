#include <ext2.h>

#include <liballoc.h>
#include <string.h>
#include <system.h>
#include <timer.h>
#include <pci.h>

#include <utility.h>

int ext2_mkdir(MountPoint *mnt, char *dirname, uint32_t mode,
              char **symlinkResolve) {
  Ext2 *ext2 = EXT2_PTR(mnt->fsInfo);

  // dirname will be sanitized anyways
  int len = strlen(dirname);
  int lastSlash = 0;
  for (int i = 0; i < len; i++) {
    if (dirname[i] == '/')
      lastSlash = i;
  }

  uint32_t inode = 0;
  if (lastSlash > 0) {
    char *parent = malloc(lastSlash);
    memcpy(parent, dirname, lastSlash);
    parent[lastSlash] = '\0';
    inode =
        ext2_traverse_path(ext2, parent, EXT2_ROOT_INODE, true, symlinkResolve);
    free(parent);
  } else // if it's trying to open / just set the inode directly
    inode = 2;

  if (!inode)
    return -ENOENT;

  // main scope
  int ret = 0;

  // various checks
  char *name = &dirname[lastSlash + 1];
  int   nameLen = strlen(name);

  if (!nameLen) // going for /
    return -EEXIST;

  Ext2Inode *inodeContents = ext2_inode_fetch(ext2, inode);
  if (!(inodeContents->permission & S_IFDIR)) {
    ret = -ENOTDIR;
    free(inodeContents);
    return ret;  
  
  }

  if (ext2_traverse(ext2, inode, name, nameLen)) {
    ret = -EEXIST;
  free(inodeContents);
  return ret;  
  }

  size_t time = timerBootUnix + timerTicks / 1000;

  // prepare what we want to write to that inode
  Ext2Inode newInode = {0};
  newInode.permission = S_IFDIR | mode;
  newInode.userid = 0;
  newInode.size = 0; // will get increased by . & .. entries
  newInode.atime = time;
  newInode.ctime = time;
  newInode.mtime = time;
  newInode.dtime = 0; // not yet :p
  newInode.gid = 0;
  newInode.hard_links = 1;
  // newInode.blocks = ; // should be zero'd
  newInode.num_sectors = newInode.size / SECTOR_SIZE;
  newInode.generation = 0;
  newInode.file_acl = 0;
  newInode.dir_acl = 0;
  newInode.f_block_addr = 0;
  newInode.size_high = 0;

  // assign an inode
  uint32_t group = INODE_TO_BLOCK_GROUP(ext2, inode);
  uint32_t newInodeNum = ext2_inode_find(ext2, group);

  // write to it
  ext2_inode_modifyM(ext2, newInodeNum, &newInode);

  // create the . and .. entries as children
  ext2_air_allocate(ext2, newInodeNum, &newInode, ".", 1, 2, newInodeNum);
  ext2_air_allocate(ext2, newInodeNum, &newInode, "..", 2, 2, inode);

  // finally, assign it to the parent
  ext2_air_allocate(ext2, inode, inodeContents, name, nameLen, 2, newInodeNum);

  free(inodeContents);
  return ret;
}

int ext2_touch(MountPoint *mnt, char *filename, uint32_t mode,
              char **symlinkResolve) {
  Ext2 *ext2 = EXT2_PTR(mnt->fsInfo);

  // dirname will be sanitized anyways
  int len = strlen(filename);
  int lastSlash = 0;
  for (int i = 0; i < len; i++) {
    if (filename[i] == '/')
      lastSlash = i;
  }

  uint32_t inode = 0;
  if (lastSlash > 0) {
    char *parent = malloc(lastSlash + 1);
    memcpy(parent, filename, lastSlash);
    parent[lastSlash] = '\0';
    inode =
        ext2_traverse_path(ext2, parent, EXT2_ROOT_INODE, true, symlinkResolve);
    free(parent);
  } else // if it's trying to open / just set the inode directly
    inode = 2;

  if (!inode)
    return -ENOENT;

  // main scope
  int ret = 0;

  // various checks
  char *name = &filename[lastSlash + 1];
  int   nameLen = strlen(name);

  if (!nameLen) // going for /
    return -EISDIR;

  Ext2Inode *inodeContents = ext2_inode_fetch(ext2, inode);
  if (!(inodeContents->permission & S_IFDIR)) {
    ret = -ENOTDIR;
    free(inodeContents);
    return ret;

  }

  if (ext2_traverse(ext2, inode, name, nameLen)) {
    ret = -EEXIST;
    free(inodeContents);
    return ret;
  }

  size_t time = timerBootUnix + timerTicks / 1000;

  // prepare what we want to write to that inode
  Ext2Inode newInode = {0};
  newInode.permission = S_IFREG | mode;
  newInode.userid = 0;
  newInode.size = 0;
  newInode.atime = time;
  newInode.ctime = time;
  newInode.mtime = time;
  newInode.dtime = 0; // not yet :p
  newInode.gid = 0;
  newInode.hard_links = 1;
  // newInode.blocks = ; // should be zero'd
  newInode.num_sectors = newInode.size / SECTOR_SIZE;
  newInode.generation = 0;
  newInode.file_acl = 0;
  newInode.dir_acl = 0;
  newInode.f_block_addr = 0;
  newInode.size_high = 0;

  // assign an inode
  uint32_t group = INODE_TO_BLOCK_GROUP(ext2, inode);
  uint32_t newInodeNum = ext2_inode_find(ext2, group);

  // write to it
  ext2_inode_modifyM(ext2, newInodeNum, &newInode);

  // finally, assign it to the parent
  ext2_air_allocate(ext2, inode, inodeContents, name, nameLen, 1, newInodeNum);

  free(inodeContents);
  return ret;
}
