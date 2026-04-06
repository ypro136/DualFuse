/*------------------------------------------------------------------------*/
/* fs.c — DualFuse filesystem implementation                              */
/*------------------------------------------------------------------------*/
#include <stdio.h>
#include <bootloader.h>
#include <liballoc.h>
#include <png_loader.h>

#include "fs.h"
#include "diskio.h"  /* fatfs_set_drive() */

/* Single mounted volume — one disk.img, one filesystem object */
static FATFS g_fs;

/* -----------------------------------------------------------------------
 * fs_init
 * ----------------------------------------------------------------------- */
int fs_init(void *base, unsigned long size)
{
    FRESULT res;

    fatfs_set_drive(base, size);

    /* Mount immediately (1 = mount now, not deferred) */
    res = f_mount(&g_fs, "", 1);
    return (res == FR_OK) ? 0 : (int)res;
}

void filesystem_mount()
{
    char address_string_buffer[32];
    char size_string_buffer[32];

    if (!bootloader.modules || bootloader.modules->module_count == 0) {
        printf("no modules loaded");
        return;
    }

    void*         module_base_address = (void*)bootloader.modules->modules[0]->address;
    unsigned long module_size_bytes   =        bootloader.modules->modules[0]->size;

    int mount_error_code = fs_init(module_base_address, module_size_bytes);
    if (mount_error_code == 0) 
    {
        printf("mounted fat32");
    }
    else { printf("mount failed: "); printf(fs_strerr((FRESULT)mount_error_code)); }
}
void load_background()
{
    FIL     wallpaper_file_handle;
    FRESULT open_result = f_open(&wallpaper_file_handle, "/assets/wallpaper.png", FA_READ);
    if (open_result != FR_OK) {
        printf("no wallpaper found at /assets/wallpaper.png, using gradient fallback");
        return;
    }

    uint32_t wallpaper_file_size_bytes = (uint32_t)f_size(&wallpaper_file_handle);
    uint8_t* wallpaper_pixel_data_buffer = (uint8_t*)malloc(wallpaper_file_size_bytes);
    if (!wallpaper_pixel_data_buffer) {
        printf("out of memory for wallpaper, using gradient fallback");
        f_close(&wallpaper_file_handle);
        return;
    }

    UINT    bytes_read_count;
    FRESULT read_result = f_read(&wallpaper_file_handle,
                                  wallpaper_pixel_data_buffer,
                                  wallpaper_file_size_bytes,
                                  &bytes_read_count);
    f_close(&wallpaper_file_handle);

    if (read_result != FR_OK) {
        printf("wallpaper read failed: ");
        printf(fs_strerr(read_result));
        free(wallpaper_pixel_data_buffer);
        return;
    }

    if (png_load_wallpaper(wallpaper_pixel_data_buffer, bytes_read_count)) {
        printf("wallpaper loaded from /assets/wallpaper.png");
    } else {
        printf("wallpaper decode failed, using gradient fallback");
    }
 
    free(wallpaper_pixel_data_buffer);
}

/* -----------------------------------------------------------------------
 * load_ssfn_font
 * Reads an entire .sfn font file into a malloc'd buffer.
 * Returns pointer to buffer (must be freed later) or NULL on error.
 * ----------------------------------------------------------------------- */
uint8_t* load_ssfn_font(const char *path, uint32_t *out_size)
{
#if defined(DEBUG_SSFN)
    printf("[DEBUG_SSFN] load_ssfn_font: opening %s\n", path);
#endif
    FIL fp;
    FRESULT res = f_open(&fp, path, FA_READ);
    if (res != FR_OK) {
        printf("[fs] Failed to open font file %s\n", path);
        return NULL;
    }

    uint32_t size = f_size(&fp);
    uint8_t *buf = (uint8_t*)malloc(size);
    if (!buf) {
        printf("[fs] Out of memory for font (%u bytes)\n", size);
        f_close(&fp);
        return NULL;
    }

    UINT br;
    res = f_read(&fp, buf, size, &br);
    f_close(&fp);

    if (res != FR_OK || br != size) {
        printf("[fs] Failed to read font file %s\n", path);
        free(buf);
        return NULL;
    }

    if (out_size) *out_size = size;
    printf("[fs] Loaded font %s (%u bytes)\n", path, size);
#if defined(DEBUG_SSFN)
    printf("[DEBUG_SSFN] load_ssfn_font: success, buffer=%p, size=%u\n", (void*)buf, size);
#endif
    return buf;
}

/* -----------------------------------------------------------------------
 * fs_read_file
 * Read an entire file into buf. Returns bytes read or -1 on error.
 * ----------------------------------------------------------------------- */
long fs_read_file(const char *path, void *buf, unsigned long buf_size)
{
    FIL    file;
    FRESULT res;
    UINT   bytes_read;

    res = f_open(&file, path, FA_READ);
    if (res != FR_OK) return -1;

    res = f_read(&file, buf, (UINT)buf_size, &bytes_read);
    f_close(&file);

    return (res == FR_OK) ? (long)bytes_read : -1;
}

/* -----------------------------------------------------------------------
 * fs_exists
 * ----------------------------------------------------------------------- */
int fs_exists(const char *path)
{
    FILINFO fno;
    return (f_stat(path, &fno) == FR_OK) ? 1 : 0;
}

/* -----------------------------------------------------------------------
 * fs_strerr
 * ----------------------------------------------------------------------- */
const char *fs_strerr(FRESULT res)
{
    switch (res) {
        case FR_OK:                  return "OK";
        case FR_DISK_ERR:            return "DISK_ERR";
        case FR_INT_ERR:             return "INT_ERR";
        case FR_NOT_READY:           return "NOT_READY";
        case FR_NO_FILE:             return "NO_FILE";
        case FR_NO_PATH:             return "NO_PATH";
        case FR_INVALID_NAME:        return "INVALID_NAME";
        case FR_DENIED:              return "DENIED";
        case FR_EXIST:               return "EXIST";
        case FR_INVALID_OBJECT:      return "INVALID_OBJECT";
        case FR_WRITE_PROTECTED:     return "WRITE_PROTECTED";
        case FR_INVALID_DRIVE:       return "INVALID_DRIVE";
        case FR_NOT_ENABLED:         return "NOT_ENABLED";
        case FR_NO_FILESYSTEM:       return "NO_FILESYSTEM";
        case FR_MKFS_ABORTED:        return "MKFS_ABORTED";
        case FR_TIMEOUT:             return "TIMEOUT";
        case FR_LOCKED:              return "LOCKED";
        case FR_NOT_ENOUGH_CORE:     return "NOT_ENOUGH_CORE";
        case FR_TOO_MANY_OPEN_FILES: return "TOO_MANY_OPEN_FILES";
        case FR_INVALID_PARAMETER:   return "INVALID_PARAMETER";
        default:                     return "UNKNOWN";
    }
}