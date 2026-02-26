#include <dents.h>
#include <ext2.h>
#include <liballoc.h>
#include <system.h>
#include <timer.h>
#include <utility.h>

bool ext2DirAllocate(Ext2 *ext2, uint32_t inodeNum, Ext2Inode *parentDirInode, char *filename, uint8_t filenameLen, uint8_t type, uint32_t inode) {
    spinlock_acquire(&ext2->LOCK_DIRALLOC);
    int entryLen = sizeof(Ext2Directory) + filenameLen;

    Ext2Inode *ino = parentDirInode; // <- todo
    uint8_t   *names = (uint8_t *)malloc(ext2->blockSize);

    Ext2LookupControl control = {0};
    size_t            blockNum = 0;

    bool ret = false;
    ext2BlockFetchInit(ext2, &control);

    int blocksContained = CEILING_DIVISION(ino->size, ext2->blockSize);
    for (int i = 0; i < blocksContained; i++) {
        size_t block = ext2BlockFetch(ext2, ino, inodeNum, &control, blockNum);
        if (!block)
            break;
        blockNum++;
        Ext2Directory *dir = (Ext2Directory *)names;

        get_disk_bytes(names, BLOCK_TO_LBA(ext2, 0, block),
                       ext2->blockSize / SECTOR_SIZE);

        while (((size_t)dir - (size_t)names) < ext2->blockSize) {
            if (dir->inode && filenameLen == dir->filenameLength &&
                memcmp(dir->filename, filename, filenameLen) == 0) {
                ret = false;
                ext2BlockFetchCleanup(&control);
                free(names);
                spinlock_release(&ext2->LOCK_DIRALLOC);
                return ret;
            }
            
            int minForOld = (sizeof(Ext2Directory) + dir->filenameLength + 3) & ~3;
            int minForNew = (entryLen + 3) & ~3;
            int remainderForNew = dir->size - minForOld;
            
            if (remainderForNew >= minForNew) {
                // means we now have enough space to put the new directory entry in
                dir->size = minForOld;
                Ext2Directory *new_ext2_directory = (void *)((size_t)dir + dir->size);
                new_ext2_directory->size = remainderForNew;
                new_ext2_directory->type = type;
                memcpy(new_ext2_directory->filename, filename, filenameLen);
                new_ext2_directory->filenameLength = filenameLen;
                new_ext2_directory->inode = inode;

                set_disk_bytes(names, BLOCK_TO_LBA(ext2, 0, block),
                               ext2->blockSize / SECTOR_SIZE);

                ret = true;
                ext2BlockFetchCleanup(&control);
                free(names);
                spinlock_release(&ext2->LOCK_DIRALLOC);
                return ret;
            }
            
            dir = (void *)((size_t)dir + dir->size);
        }
    }

    // means we need to allocate another block for these
    uint32_t group = INODE_TO_BLOCK_GROUP(ext2, inodeNum);
    uint32_t new_ext2_directoryBlock = ext2BlockFind(ext2, group, 1);

    uint8_t *newBlockBuff = names; // reuse names :p
    get_disk_bytes(newBlockBuff, BLOCK_TO_LBA(ext2, 0, new_ext2_directoryBlock),
                   ext2->blockSize / SECTOR_SIZE);

    Ext2Directory *new_ext2_directory = (Ext2Directory *)(newBlockBuff);
    new_ext2_directory->size = ext2->blockSize;
    new_ext2_directory->type = type;
    memcpy(new_ext2_directory->filename, filename, filenameLen);
    new_ext2_directory->filenameLength = filenameLen;
    new_ext2_directory->inode = inode;

    set_disk_bytes(newBlockBuff, BLOCK_TO_LBA(ext2, 0, new_ext2_directoryBlock),
                   ext2->blockSize / SECTOR_SIZE);
    ext2BlockAssign(ext2, ino, inodeNum, &control, blockNum, new_ext2_directoryBlock);

    ino->num_sectors += ext2->blockSize / SECTOR_SIZE;
    ino->size += ext2->blockSize;
    ext2InodeModifyM(ext2, inodeNum, ino);

    ret = true;

    ext2BlockFetchCleanup(&control);
    free(names);
    spinlock_release(&ext2->LOCK_DIRALLOC);

    return ret;
}

bool ext2DirRemove(Ext2 *ext2, Ext2Inode *parentDirInode, uint32_t parentDirInodeNum, char *filename, uint8_t filenameLen) {
    spinlock_acquire(&ext2->LOCK_DIRALLOC);

    Ext2Inode *ino = parentDirInode; // <- todo
    uint8_t   *names = (uint8_t *)malloc(ext2->blockSize);

    Ext2LookupControl control = {0};
    size_t            blockNum = 0;

    bool ret = false;
    ext2BlockFetchInit(ext2, &control);

    int blocksContained = CEILING_DIVISION(ino->size, ext2->blockSize);
    for (int i = 0; i < blocksContained; i++) {
        size_t block =
            ext2BlockFetch(ext2, ino, parentDirInodeNum, &control, blockNum);
        if (!block)
            break;
        blockNum++;
        Ext2Directory *dir = (Ext2Directory *)names;

        get_disk_bytes(names, BLOCK_TO_LBA(ext2, 0, block),
                       ext2->blockSize / SECTOR_SIZE);

        Ext2Directory *before = 0;
        while (((size_t)dir - (size_t)names) < ext2->blockSize) {
            if (dir->inode && filenameLen == dir->filenameLength &&
                memcmp(dir->filename, filename, filenameLen) == 0) {
                // found!
                if ((size_t)dir == (size_t)names) {
                    // it's the first element
                    assert(!before);
                    dir->inode = 0;
                    dir->filenameLength = 0;
                    set_disk_bytes(names, BLOCK_TO_LBA(ext2, 0, block),
                                   ext2->blockSize / SECTOR_SIZE);
                } else {
                    // it's somewhere in between, meaning there's another element behind
                    before->size += dir->size;
                    set_disk_bytes(names, BLOCK_TO_LBA(ext2, 0, block),
                                   ext2->blockSize / SECTOR_SIZE);
                }
                // done successfully!
                ret = true;
            }

            before = dir;
            dir = (void *)((size_t)dir + dir->size);
        }
    }

    ext2BlockFetchCleanup(&control);
    free(names);
    spinlock_release(&ext2->LOCK_DIRALLOC);

    return ret;
}

size_t ext2Getdents64(OpenFile *file,
                      struct linux_dirent64 *start,
                      unsigned int hardlimit)
{
    Ext2       *ext2 = EXT2_PTR(file->mountPoint->fsInfo);
    Ext2OpenFd *edir = EXT2_DIR_PTR(file->dir);

    if ((edir->inode.permission & 0xF000) != EXT2_S_IFDIR)
        return ERR(ENOTDIR);

    size_t allocatedlimit = 0;
    Ext2Inode *ino = &edir->inode;
    uint8_t *names = (uint8_t *)malloc(ext2->blockSize);

    struct linux_dirent64 *dirp = (struct linux_dirent64 *)start;

    int blocksContained = CEILING_DIVISION(ino->size, ext2->blockSize);

    bool stop = false;

    for (int i = 0; i < blocksContained && !stop; i++) {

        size_t block = ext2BlockFetch(ext2, ino, edir->inodeNum,
                                      &edir->lookup,
                                      edir->ptr / ext2->blockSize);

        if (!block)
            break;

        get_disk_bytes(names,
                       BLOCK_TO_LBA(ext2, 0, block),
                       ext2->blockSize / SECTOR_SIZE);

        Ext2Directory *dir =
            (Ext2Directory *)(names + (edir->ptr % ext2->blockSize));

        while (((size_t)dir - (size_t)names) < ext2->blockSize) {

            if (dir->inode) {

                unsigned char type = 0;

                if (dir->type == 2)
                    type = CDT_DIR;
                else if (dir->type == 7)
                    type = CDT_LNK;
                else
                    type = CDT_REG;

                DENTS_RES res =
                    dents_add(start,
                              &dirp,
                              (int*)&allocatedlimit,
                              hardlimit,
                              dir->filename,
                              dir->filenameLength,
                              dir->inode,
                              type);

                if (res == DENTS_NO_SPACE) {
                    allocatedlimit = ERR(EINVAL);
                    stop = true;
                }
                else if (res == DENTS_RETURN) {
                    stop = true;
                }
                else {
                    edir->ptr += dir->size;
                }

                if (stop)
                    break;
            }

            // Equivalent to traverse label
            dir = (Ext2Directory *)((uint8_t *)dir + dir->size);
        }

        if (stop)
            break;

        int rem = edir->ptr % ext2->blockSize;
        if (rem)
            edir->ptr += ext2->blockSize - rem;
    }

    free(names);
    return allocatedlimit;
}