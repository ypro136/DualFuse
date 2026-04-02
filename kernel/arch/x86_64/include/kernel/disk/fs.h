/*------------------------------------------------------------------------*/
/* fs.h — DualFuse filesystem API                                         */
/* Thin wrapper over FatFs. Use this in the rest of the kernel,           */
/* not ff.h directly, so swapping the backend later is painless.          */
/*------------------------------------------------------------------------*/

#ifndef DUALFUSE_FS_H
#define DUALFUSE_FS_H

#include "ff.h"   /* FatFs types: FIL, DIR, FILINFO, FRESULT … */

/* -----------------------------------------------------------------------
 * Init — call once in kernel_main after Limine module is found
 *
 *   void *base  = limine_module->address
 *   size_t size = limine_module->size
 *
 * Returns 0 on success, non-zero on failure.
 * ----------------------------------------------------------------------- */
int  fs_init(void *base, unsigned long size);
void filesystem_mount();
void load_background();

/* -----------------------------------------------------------------------
 * File I/O — thin aliases so callers use fs_ prefix consistently
 * (all return FRESULT; FR_OK == 0)
 * ----------------------------------------------------------------------- */
#define fs_open(fp, path, mode)         f_open(fp, path, mode)
#define fs_close(fp)                    f_close(fp)
#define fs_read(fp, buf, btr, br)       f_read(fp, buf, btr, br)
#define fs_write(fp, buf, btw, bw)      f_write(fp, buf, btw, bw)
#define fs_seek(fp, ofs)                f_lseek(fp, ofs)
#define fs_tell(fp)                     f_tell(fp)
#define fs_size(fp)                     f_size(fp)
#define fs_eof(fp)                      f_eof(fp)

/* -----------------------------------------------------------------------
 * Directory I/O
 * ----------------------------------------------------------------------- */
#define fs_opendir(dp, path)            f_opendir(dp, path)
#define fs_closedir(dp)                 f_closedir(dp)
#define fs_readdir(dp, fno)             f_readdir(dp, fno)

/* -----------------------------------------------------------------------
 * Convenience: read an entire file into a caller-supplied buffer.
 * Returns bytes read, or -1 on error.
 * ----------------------------------------------------------------------- */
long fs_read_file(const char *path, void *buf, unsigned long buf_size);

/* -----------------------------------------------------------------------
 * Convenience: check if a path exists
 * Returns 1 if exists, 0 if not.
 * ----------------------------------------------------------------------- */
int  fs_exists(const char *path);

/* -----------------------------------------------------------------------
 * Error string helper (for debug serial output)
 * ----------------------------------------------------------------------- */
const char *fs_strerr(FRESULT res);

/* -----------------------------------------------------------------------
 * Open flags — re-export so callers don't need to know FatFs names
 * ----------------------------------------------------------------------- */
#define FS_READ         FA_READ
#define FS_WRITE        FA_WRITE
#define FS_CREATE_NEW   FA_CREATE_NEW
#define FS_CREATE_ALWAYS FA_CREATE_ALWAYS
#define FS_OPEN_ALWAYS  FA_OPEN_ALWAYS
#define FS_OPEN_APPEND  FA_OPEN_APPEND

#endif /* DUALFUSE_FS_H */