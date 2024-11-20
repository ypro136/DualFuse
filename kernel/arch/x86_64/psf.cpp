#include <psf.h>

#include <framebufferutil.h>
#include <memory.h>
#include <console.h>



// ONLY include this here!
#include <gohufont.h>

PSF1Header *psf;




bool psfLoad(void *buffer) {
  PSF1Header *header = (PSF1Header *)buffer;

  if (header->magic != PSF1_MAGIC) {
    printf("[console] Invalid PSF magic! Only PSF1 is supported{0x0436} "
           "supplied{%04X}\n",
           header->magic);
    return false;
  }

  if (!(header->mode & PSF1_MODE512) && !(header->mode & PSF1_MODEHASTAB)) {
    printf("[console] Invalid PSF mode! No unicode table found... mode{%02X}\n",
           header->mode);
    return false;
  }

  psf = buffer;

  printf("[console] Initiated with font: dim(xy){%dx%d}\n", 8, psf->height);

  return true;
}

bool psfLoadDefaults() 
    { 

        return psfLoad(&u_vga16_psf[0]); 
    }

// bool psfLoadFromFile(char *path) {
//   OpenFile *dir = file_system_kernel_open(path, O_RDONLY, 0);
//   if (!dir)
//     return false;

//   uint32_t filesize = file_system_get_filesize(dir);
//   uint8_t *out = (uint8_t *)malloc(filesize);

//   file_system_read_full_file(dir, out);
//   file_system_kernel_close(dir);

//   bool res = psfLoad(out);
//   if (!res)
//     free(out);

//   return res;
// }

void psfPutC(char c, uint32_t x, uint32_t y, uint32_t rgb) {
  uint8_t *targ = (uint8_t *)((size_t)psf + sizeof(PSF1Header) + c * psf->height);
  for (int i = 0; i < psf->height; i++) {
    for (int j = 0; j < 8; j++) {
      if (targ[i] & (1 << (8 - j))) // NOT little endian
        draw_pixel(x + j, y + i, rgb);
      else
        draw_pixel(x + j, y + i, bg_color);
    }
  }
}
