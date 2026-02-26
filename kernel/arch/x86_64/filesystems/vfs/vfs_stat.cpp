#include <ext2.h>
#include <fat32.h>
#include <linux.h>
#include <liballoc.h>
#include <task.h>
#include <utility.h>
#include <vfs.h>
// todo: special files & timestamps

bool fsStat(OpenFile *fd, stat *target) {
    if (!fd->handlers->stat)
        return false;
    return fd->handlers->stat(fd, target) == 0;
}

bool fsStatByFilename(void *task, char *filename, stat *target) {
    Task *t = (Task *)task;
    spinlock_acquire(&t->infoFs->LOCK_FS);
    char *safeFilename = fsSanitize(t->infoFs->cwd, filename);
    spinlock_release(&t->infoFs->LOCK_FS);
    
    MountPoint *mnt = fsDetermineMountPoint(safeFilename);
    char *strippedFilename = fsStripMountpoint(safeFilename, mnt);
    
    bool ret = false;
    char *symlink = 0;
    
    if (mnt->stat) {
        ret = mnt->stat(mnt, strippedFilename, target, &symlink);
    }
    
    free(safeFilename);
    
    if (!ret && symlink) {
        char *symlinkResolved = fsResolveSymlink(mnt, symlink);
        free(symlink);
        ret = fsStatByFilename(task, symlinkResolved, target);
        free(symlinkResolved);
    }
    
    return ret;
}

bool fsLstatByFilename(void *task, char *filename, stat *target) {
    Task *t = (Task *)task;
    spinlock_acquire(&t->infoFs->LOCK_FS);
    char *safeFilename = fsSanitize(t->infoFs->cwd, filename);
    spinlock_release(&t->infoFs->LOCK_FS);
    
    MountPoint *mnt = fsDetermineMountPoint(safeFilename);
    char *strippedFilename = fsStripMountpoint(safeFilename, mnt);
    
    bool ret = false;
    char *symlink = 0;
    
    if (mnt->lstat) {
        ret = mnt->lstat(mnt, strippedFilename, target, &symlink);
    }
    
    free(safeFilename);
    
    if (!ret && symlink) {
        char *symlinkResolved = fsResolveSymlink(mnt, symlink);
        free(symlink);
        ret = fsLstatByFilename(task, symlinkResolved, target);
        free(symlinkResolved);
    }
    
    return ret;
}