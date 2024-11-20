 #include <types.h>

#include <limine.h>

#ifndef PSF_H
#define PSF_H

#define PSF1_MAGIC 0x0436



typedef struct PSF1Header {
  uint16_t magic;
  uint8_t  mode;
  uint8_t  height; // width is always 8px
} PSF1Header;

typedef enum PSF1_MODES {
  PSF1_MODE512 = 0x01,
  PSF1_MODEHASTAB = 0x02,
  PSF1_MODESEQ = 0x04,
} PSF1_MODES;

extern PSF1Header *psf;

bool psfLoadDefaults();
bool psfLoadFromFile(char *path);
bool psfLoad(void *buffer);

void psfPutC(char c, uint32_t x, uint32_t y, uint32_t rgb);

#endif
