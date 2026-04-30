/*------------------------------------------------------------------------*/
/* FatFs Configuration File — DualFuse                                    */
/*------------------------------------------------------------------------*/

#ifndef FFCONF_DEF
#define FFCONF_DEF  80386   /* Revision ID — do not change */

/*---------------------------------------------------------------------------
 * FILESYSTEM BEHAVIOUR
 *---------------------------------------------------------------------------*/

/* FF_FS_READONLY
 * 0: Read/Write  1: Read-only
 * We want read/write so assets can be patched at runtime. */
#define FF_FS_READONLY      0

/* FF_FS_MINIMIZE
 * 0: Full API    1: -stat/getfree/unlink/mkdir/rename
 * 2: -opendir    3: -lseek
 * Start with 0 (full), trim later if size matters. */
#define FF_FS_MINIMIZE      0

/*---------------------------------------------------------------------------
 * LONG FILE NAME (LFN)
 *---------------------------------------------------------------------------*/

/* FF_USE_LFN
 * 0: Disable LFN (8.3 names only — would break most asset filenames)
 * 1: Static buffer — NOT safe for re-entry, fine for DualFuse (single task now)
 * 2: Stack buffer
 * 3: Heap buffer
 * Use 1 until you have a heap. */
#define FF_USE_LFN          1
#define FF_MAX_LFN          255     /* Max LFN length (1-255) */

/* FF_LFN_UNICODE
 * 0: ANSI/OEM   1: UTF-16   2: UTF-8   3: UTF-32
 * UTF-8 is the right call for a modern OS. */
#define FF_LFN_UNICODE      2

/* FF_LFN_BUF / FF_SFN_BUF
 * Size of LFN/SFN string buffer in TCHAR units. */
#define FF_LFN_BUF          255
#define FF_SFN_BUF          12

/* FF_CODE_PAGE
 * 437 = US English. Smallest SBCS table. Fine since we use Unicode API. */
#define FF_CODE_PAGE        437

/*---------------------------------------------------------------------------
 * SECTOR / VOLUME SIZES
 *---------------------------------------------------------------------------*/

/* FF_MIN_SS / FF_MAX_SS
 * Our disk.img uses 512-byte sectors. Set both the same → no runtime check. */
#define FF_MIN_SS           512
#define FF_MAX_SS           512

/* FF_MULTI_PARTITION
 * 0: Single partition per drive (standard)
 * 1: Multiple partitions
 * disk.img has one FAT32 partition, keep 0. */
#define FF_MULTI_PARTITION  0

/*---------------------------------------------------------------------------
 * FEATURES WE DON'T NEED YET
 *---------------------------------------------------------------------------*/

#define FF_USE_MKFS         0   /* No f_mkfs — we build images on the host */
#define FF_USE_FASTSEEK     0   /* Fast lseek cluster map — not needed yet */
#define FF_USE_EXPAND       0   /* f_expand — not needed */
#define FF_USE_CHMOD        0   /* f_chmod / f_utime — not needed */
#define FF_USE_LABEL        0   /* f_getlabel / f_setlabel — not needed */
#define FF_USE_FORWARD      0   /* f_forward — not needed */
#define FF_USE_STRFUNC      0   /* f_gets/f_puts/f_printf — use your own */
#define FF_USE_FIND         1   /* f_findfirst / f_findnext — useful */
#define FF_USE_TRIM         0   /* CTRL_TRIM — pointless on a RAM image */

/*---------------------------------------------------------------------------
 * RTOS / RE-ENTRANCY
 *---------------------------------------------------------------------------*/

/* FF_FS_REENTRANT
 * 0: Not re-entrant (single task, no locks needed)
 * Enable when DualFuse gets a scheduler. */
#define FF_FS_REENTRANT     0
#define FF_FS_TIMEOUT       1000
#define FF_FS_LOCK          0   /* 0 = no lock control */

/*---------------------------------------------------------------------------
 * MEMORY (only needed when FF_USE_LFN == 3)
 *---------------------------------------------------------------------------*/
/* Not needed with FF_USE_LFN = 1, but stub these out to avoid link errors. */
/* #define ff_memalloc(sz)  your_malloc(sz) */
/* #define ff_memfree(ptr)  your_free(ptr)  */

/*---------------------------------------------------------------------------
 * RTC
 *---------------------------------------------------------------------------*/

/* FF_FS_NORTC
 * 1: Disable RTC — use a fixed timestamp instead.
 * We have no RTC driver yet. */
#define FF_FS_NORTC         1
#define FF_NORTC_MON        1   /* January */
#define FF_NORTC_MDAY       1
#define FF_NORTC_YEAR       2025

/*---------------------------------------------------------------------------
 * TINY BUFFER
 *---------------------------------------------------------------------------*/

/* FF_FS_TINY
 * 0: Each file object has its own 512-byte sector buffer (faster)
 * 1: Shared buffer — saves memory but slower
 * Use 0 for now — we're not memory-starved at this stage. */
#define FF_FS_TINY          0

/*---------------------------------------------------------------------------
 * exFAT — disabled, we use FAT32
 *---------------------------------------------------------------------------*/
#define FF_FS_EXFAT         0

#define FF_VOLUMES 8
// This option configures number of volumes (logical drives up to 8) to be used.

#define FF_FS_RPATH 0

#endif /* FFCONF_DEF */