#define SSFN_IMPLEMENTATION
#include <psf.h>

#include <framebufferutil.h>
#include <memory.h>
#include <console.h>
#include <vfs.h>
#include <fs.h> 
#include <bootloader.h>
#include <liballoc.h>




ssfn_t    g_ssfn_ctx = {0};
ssfn_buf_t g_ssfn_buf = {0};
static uint8_t   *g_font_data = NULL;   // keep allocated for SSFN

static int current_font_height = 0;



// ONLY include this here!
#include <gohufont.h>

PSF1Header *psf;




bool psfLoad(void *buffer) {
  PSF1Header *header = (PSF1Header *)buffer;

  if (header->magic != PSF1_MAGIC) {
    printf("[console:error] Invalid PSF magic! Only PSF1 is supported{0x0436} "
           "supplied{%04X}\n",
           header->magic);
    return false;
  }

  if (!(header->mode & PSF1_MODE512) && !(header->mode & PSF1_MODEHASTAB)) {
    printf("[console:error] Invalid PSF mode! No unicode table found... mode{%02X}\n",
           header->mode);
    return false;
  }

  psf = buffer;

  #if defined(DEBUG_CONSOLE)
  printf("[console] Initiated with font: dim(xy){%dx%d}\n", 8, psf->height);
  #endif

  return true;
}

bool psfLoadDefaults() 
{ 
    return psfLoad(&u_vga16_psf[0]); 
}

void init_ssfn(void) {
#if defined(DEBUG_SSFN)
    printf("[DEBUG_SSFN] init_ssfn: entering\n");
#endif
    // Use the back buffer that the rest of the GUI uses
    if (!tempframebuffer || !tempframebuffer->address) {
#if defined(DEBUG_SSFN)
        printf("[DEBUG_SSFN] init_ssfn: tempframebuffer is NULL!\n");
#endif
        return;
    }

    g_ssfn_buf.ptr = (uint8_t*)tempframebuffer->address;
    g_ssfn_buf.w   = tempframebuffer->width;
    g_ssfn_buf.h   = tempframebuffer->height;
    g_ssfn_buf.p   = tempframebuffer->pitch;
    g_ssfn_buf.fg  = 0xFFFFFF;
    g_ssfn_buf.bg  = 0x000000;
    g_ssfn_buf.x   = 0;
    g_ssfn_buf.y   = 0;

#if defined(DEBUG_SSFN)
    printf("[DEBUG_SSFN] init_ssfn: back buffer %dx%d, pitch=%d, ptr=%p\n",
           tempframebuffer->width, tempframebuffer->height,
           tempframebuffer->pitch, (void*)tempframebuffer->address);
#endif
}

int load_ssfn(const char *font_path, int pixel_height) {
#if defined(DEBUG_SSFN)
    printf("[DEBUG_SSFN] load_ssfn: path=%s, height=%d\n", font_path, pixel_height);
#endif
    uint32_t font_size;
    g_font_data = load_ssfn_font(font_path, &font_size);
    if (!g_font_data) {
#if defined(DEBUG_SSFN)
        printf("[DEBUG_SSFN] load_ssfn: load_ssfn_font failed\n");
#endif
        return -1;
    }

    if (ssfn_load(&g_ssfn_ctx, g_font_data) != SSFN_OK) {
        printf("[ssfn] ssfn_load failed\n");
#if defined(DEBUG_SSFN)
        printf("[DEBUG_SSFN] load_ssfn: ssfn_load returned error\n");
#endif
        free(g_font_data);
        g_font_data = NULL;
        return -1;
    }

    const ssfn_font_t *font = (const ssfn_font_t*)g_font_data;
    printf("[ssfn] font magic: %c%c%c%c, size=%u, type=0x%02X, height=%u\n",
           font->magic[0], font->magic[1], font->magic[2], font->magic[3],
           font->size, font->type, font->height);

    int ret = ssfn_select(&g_ssfn_ctx, SSFN_FAMILY_ANY, NULL,
                          SSFN_STYLE_REGULAR, pixel_height);
    if (ret != SSFN_OK) {
        printf("[ssfn] ssfn_select failed with error %d\n", ret);
#if defined(DEBUG_SSFN)
        printf("[DEBUG_SSFN] load_ssfn: ssfn_select returned %d\n", ret);
#endif
        return -1;
    }

    // Store the pixel height for later use
    current_font_height = pixel_height;

    printf("[ssfn] Font loaded and set to %d pixels\n", pixel_height);
#if defined(DEBUG_SSFN)
    printf("[DEBUG_SSFN] load_ssfn: success, ctx.fnt=%p\n", (void*)g_ssfn_ctx.fnt);
#endif
    return 0;
}

void unload_ssfn(void) {
    ssfn_free(&g_ssfn_ctx);
    if (g_font_data) {
        free(g_font_data);
        g_font_data = NULL;
    }
}

// bool psfLoadFromFile(char *path) {
//   OpenFile *dir = fsKernelOpen(path, O_RDONLY, 0);
//   if (!dir)
//     return false;

//   uint32_t filesize = fsGetFilesize(dir);
//   uint8_t *out = (uint8_t *)malloc(filesize);

//   fsRead(dir, out, fsGetFilesize(dir));
//   fsKernelClose(dir);

//   bool res = psfLoad(out);
//   if (!res)
//     free(out);

//   return res;
// }
int ssfn_get_font_height(void) {
    return current_font_height;
}

bool ssfn_is_loaded(void) {
    return g_ssfn_ctx.fnt != NULL;
}

static int get_text_width(const char* text) {
    if (!text || !*text) return 0;

    int scale = (SCREEN_WIDTH > 1300) ? 2 : 1;

    if (ssfn_is_loaded()) {
        int width, height, left, top;
        if (ssfn_bbox(&g_ssfn_ctx, text, &width, &height, &left, &top) == SSFN_OK) {
            return width;
        }
        // fallback – should not happen if font is valid
        return 8 * scale * (int)strlen(text);
    } else {
        // PSF fallback: monospaced 8 pixels per character (scaled)
        return 8 * scale * (int)strlen(text);
    }
}

void draw_text_ssfn(int x, int y, uint32_t fg_color, uint32_t bg_color, const char *str) {
    if (!str) return;

    // This volatile dummy prevents the crash (compiler optimization workaround)
    volatile int dummy = 0;
    (void)dummy;  // silence unused warning

    ssfn_buf_t buf = g_ssfn_buf;
    buf.x = x;
    buf.y = y;
    buf.fg = fg_color;
    buf.bg = bg_color;

    for (const char *p = str; *p; p++) {
        char single[2] = { *p, '\0' };
        int ret = ssfn_render(&g_ssfn_ctx, &buf, single);
        if (ret <= 0) break;
    }
}

void psfPutCScaled(char c, uint32_t x, uint32_t y, uint32_t rgb, uint32_t bg_color, uint32_t scale)
{
    if (!psf) return;
    if (scale < 1) scale = 1;

    uint8_t *glyph = (uint8_t *)((size_t)psf + sizeof(PSF1Header) + c * psf->height);

    for (int row = 0; row < psf->height; row++) {
        for (int col = 0; col < 8; col++) {
            int pixel_set = (glyph[row] >> (7 - col)) & 1;  // big‑endian bit order
            uint32_t color = pixel_set ? rgb : bg_color;

            // Draw a scaled block
            for (uint32_t dy = 0; dy < scale; dy++) {
                for (uint32_t dx = 0; dx < scale; dx++) {
                    draw_pixel(x + col * scale + dx, y + row * scale + dy, color);
                }
            }
        }
    }
}

uint32_t psfPutC(char c, uint32_t x, uint32_t y, uint32_t rgb, uint32_t bg_color)
{
    // If SSFN is loaded and ready, use it
    if (g_ssfn_ctx.fnt != NULL) {
        char str[2] = {c, '\0'};
        uint32_t fg = (rgb & 0x00FFFFFF) | 0xFF000000;
        uint32_t bg = (bg_color & 0x00FFFFFF) | 0xFF000000;

        ssfn_buf_t buf = g_ssfn_buf;
        buf.x = x;
        buf.y = y;
        buf.fg = fg;
        buf.bg = bg;

        int ret = ssfn_render(&g_ssfn_ctx, &buf, str);
        if (ret > 0) {
            // Return the advance in pixels (buf.x - original x)
            return buf.x - x;
        }
        return 0; // error, no advance
    }

    // Fallback to PSF scaling
    if (!psf) return 0;
    psfPutCScaled(c, x, y, rgb, bg_color, 1);
    int scale = (SCREEN_WIDTH > 1300) ? 2 : 1;
    return 8 * scale; // fixed width
}
