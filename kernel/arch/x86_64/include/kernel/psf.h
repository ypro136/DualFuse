 #include <types.h>

#include <limine.h>
#define __KERNEL__
#include <ssfn.h>
#undef __KERNEL__


extern ssfn_t g_ssfn_ctx;      // renderer context
extern ssfn_buf_t g_ssfn_buf;  // framebuffer destination
extern int current_font_height;

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

bool ssfn_is_loaded(void);
void draw_text_ssfn(int x, int y, uint32_t fg_color, uint32_t bg_color, const char *str);

void init_ssfn(void);
int  load_ssfn(const char *font_path, int pixel_height);

int get_text_width(const char* text);
int ssfn_get_font_height(void);

void ssfnPutCScaled(char c, uint32_t x, uint32_t y, uint32_t rgb, uint32_t bg_color, uint32_t scale);


void psfPutCScaled(char c, uint32_t x, uint32_t y, uint32_t rgb, uint32_t bg_color, uint32_t scale);
uint32_t psfPutC(char c, uint32_t x, uint32_t y, uint32_t rgb, uint32_t bg_color);

#endif
