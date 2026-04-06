/*------------------------------------------------------------------------*/
/* diskio.c — FatFs Media Access Interface                                */
/* DualFuse port: reads/writes against a flat memory buffer               */
/* (the disk.img loaded by Limine as a module)                            */
/*------------------------------------------------------------------------*/

#include <ff.h>
#include <diskio.h>

/* -----------------------------------------------------------------------
 * Drive numbers.
 * We only have one drive: the in-memory FAT32 image.
 * ----------------------------------------------------------------------- */
#define DEV_RAM     0   /* disk.img loaded from Limine module */

/* -----------------------------------------------------------------------
 * State: pointer + size set once at boot via fatfs_set_drive()
 * ----------------------------------------------------------------------- */
static unsigned char *g_img      = 0;
static unsigned long  g_img_size = 0;  /* bytes */

#define SECTOR_SIZE 512UL

/* Call this once in kernel_main after finding the Limine module:
 *
 *   fatfs_set_drive(module->address, module->size);
 *   FATFS fs;
 *   f_mount(&fs, "", 1);
 */
void fatfs_set_drive(void *base, unsigned long size)
{
    g_img      = (unsigned char *)base;
    g_img_size = size;
}

/* -----------------------------------------------------------------------
 * disk_status
 * ----------------------------------------------------------------------- */
DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != DEV_RAM) return STA_NOINIT;
    if (!g_img)          return STA_NOINIT;
    return 0; /* drive ready */
}

/* -----------------------------------------------------------------------
 * disk_initialize
 * Nothing to initialize for a RAM image — just confirm we're set up.
 * ----------------------------------------------------------------------- */
DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != DEV_RAM) return STA_NOINIT;
    if (!g_img)          return STA_NOINIT;
    return 0;
}

/* -----------------------------------------------------------------------
 * disk_read
 * ----------------------------------------------------------------------- */
DRESULT disk_read(BYTE pdrv, BYTE *buf, LBA_t sector, UINT count)
{
    unsigned long offset;
    unsigned long length;

    if (pdrv != DEV_RAM)   return RES_PARERR;
    if (!g_img)            return RES_NOTRDY;
    if (count == 0)        return RES_PARERR;

    offset = (unsigned long)sector * SECTOR_SIZE;
    length = (unsigned long)count  * SECTOR_SIZE;

    if (offset + length > g_img_size) return RES_PARERR;

    /* Plain byte copy — no DMA, no interrupts needed */
    {
        unsigned long i;
        for (i = 0; i < length; i++)
            buf[i] = g_img[offset + i];
    }

    return RES_OK;
}

/* -----------------------------------------------------------------------
 * disk_write
 * Writes go back into the RAM image (changes lost on reboot — fine for now).
 * ----------------------------------------------------------------------- */
DRESULT disk_write(BYTE pdrv, const BYTE *buf, LBA_t sector, UINT count)
{
    unsigned long offset;
    unsigned long length;

    if (pdrv != DEV_RAM)   return RES_PARERR;
    if (!g_img)            return RES_NOTRDY;
    if (count == 0)        return RES_PARERR;

    offset = (unsigned long)sector * SECTOR_SIZE;
    length = (unsigned long)count  * SECTOR_SIZE;

    if (offset + length > g_img_size) return RES_PARERR;

    {
        unsigned long i;
        for (i = 0; i < length; i++)
            g_img[offset + i] = buf[i];
    }

    return RES_OK;
}

/* -----------------------------------------------------------------------
 * disk_ioctl
 * ----------------------------------------------------------------------- */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buf)
{
    if (pdrv != DEV_RAM) return RES_PARERR;
    if (!g_img)          return RES_NOTRDY;

    switch (cmd) {

        case CTRL_SYNC:
            /* RAM — nothing to flush */
            return RES_OK;

        case GET_SECTOR_COUNT:
            *(LBA_t *)buf = (LBA_t)(g_img_size / SECTOR_SIZE);
            return RES_OK;

        case GET_SECTOR_SIZE:
            *(WORD *)buf = (WORD)SECTOR_SIZE;
            return RES_OK;

        case GET_BLOCK_SIZE:
            /* No erase block concept on RAM — report 1 sector */
            *(DWORD *)buf = 1;
            return RES_OK;

        default:
            return RES_PARERR;
    }
}