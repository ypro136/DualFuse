#include <png_loader.h>

#define LODEPNG_NO_COMPILE_CPP
#define LODEPNG_NO_COMPILE_ALLOCATORS
#include <lodepng.h>
#include <png_loader.h>
#include <liballoc.h>
#include <framebufferutil.h>


// Provide the three symbols LodePNG expects
void* lodepng_malloc(size_t size)             { return malloc(size);        }
void* lodepng_realloc(void* ptr, size_t size) { return realloc(ptr, size);  }
void  lodepng_free(void* ptr)                 { free(ptr);                  }

// Pre-scaled, framebuffer-ready pixel buffer.
// Format: 0xAARRGGBB  (A stored so blit can fast-path skip alpha=0 pixels)
static uint32_t* scaled_wallpaper = nullptr;
static uint32_t  wall_w           = 0;
static uint32_t  wall_h           = 0;

bool png_wallpaper_loaded()
{
    return scaled_wallpaper != nullptr;
}

bool png_load_wallpaper(const unsigned char* png_data, unsigned int png_len)
{
    // Decode PNG → raw RGBA bytes via LodePNG
    unsigned char* rgba  = nullptr;
    unsigned       img_w = 0, img_h = 0;

    unsigned err = lodepng_decode32(&rgba, &img_w, &img_h, png_data, png_len);
    if (err || !rgba)
        return false;

    // Target = full screen; taskbar draws on top so bleed-through is fine
    wall_w = (uint32_t)SCREEN_WIDTH;
    wall_h = (uint32_t)SCREEN_HEIGHT;

    scaled_wallpaper = (uint32_t*)malloc(wall_w * wall_h * sizeof(uint32_t));
    if (!scaled_wallpaper)
    {
        free(rgba);
        return false;
    }

    // Nearest-neighbor scale — works for upscale, downscale, and exact match
    for (uint32_t sy = 0; sy < wall_h; sy++)
    {
        // Map screen row → source row once per screen row (avoid redividing)
        unsigned src_y = (sy * img_h) / wall_h;

        for (uint32_t sx = 0; sx < wall_w; sx++)
        {
            unsigned src_x = (sx * img_w) / wall_w;

            // LodePNG output: tightly packed [R,G,B,A,R,G,B,A,...]
            unsigned  idx = (src_y * img_w + src_x) * 4;
            uint8_t   r   = rgba[idx + 0];
            uint8_t   g   = rgba[idx + 1];
            uint8_t   b   = rgba[idx + 2];
            uint8_t   a   = rgba[idx + 3];

            // Pack alpha into top byte so blit can skip transparent pixels
            // without a separate lookup table
            scaled_wallpaper[sy * wall_w + sx] =
                ((uint32_t)a << 24) |
                ((uint32_t)r << 16) |
                ((uint32_t)g <<  8) |
                 (uint32_t)b;
        }
    }

    // Original RGBA buffer no longer needed
    free(rgba);
    return true;
}

void png_blit_wallpaper()
{
    if (!scaled_wallpaper) return;

    for (uint32_t y = 0; y < wall_h; y++)
    {
        for (uint32_t x = 0; x < wall_w; x++)
        {
            uint32_t px = scaled_wallpaper[y * wall_w + x];

            // Skip fully transparent pixels
            if ((px >> 24) == 0) continue;

            // draw_pixel expects 0x00RRGGBB
            draw_pixel((int)x, (int)y, px & 0x00FFFFFF);
        }
    }
}