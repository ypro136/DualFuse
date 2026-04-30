#ifndef PNG_LOADER_H
#define PNG_LOADER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t* original_data;      // RGBA (0xAARRGGBB)
    uint32_t  original_width;
    uint32_t  original_height;

    uint32_t* resized_data;        // cached resized version (if any)
    uint32_t  resized_width;
    uint32_t  resized_height;
} Image;

// Load a PNG from memory into an Image struct (original only).
// Returns true on success, false on error.
bool image_load_from_png(Image* image, const unsigned char* png_data, unsigned int png_len);

// Free all resources associated with an Image.
void image_free(Image* image);

// Pre‑resize the image to a specific size, storing the result in the resized fields.
// Returns true on success, false on error (e.g., out of memory).
bool image_resize(Image* image, uint32_t new_width, uint32_t new_height);

// Draw the image to the framebuffer (via draw_pixel) at (x, y) with optional scaling.
// If target_width and target_height are zero, draw the original size.
// If a resized version matching target dimensions exists, it is used; otherwise,
// real‑time nearest‑neighbor scaling is applied directly to the framebuffer.
void image_draw(const Image* image, int x, int y, uint32_t target_width, uint32_t target_height);

// Legacy wallpaper functions (kept for compatibility, but now use the new system)
bool png_wallpaper_loaded(void);
bool png_load_wallpaper(const unsigned char* png_data, unsigned int png_len);
void png_blit_wallpaper(void);

#endif