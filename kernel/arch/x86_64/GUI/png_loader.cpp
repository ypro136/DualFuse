#include <png_loader.h>
#include <lodepng.h>
#include <liballoc.h>
#include <framebufferutil.h>
#include <string.h>
#include <stdio.h>

// LodePNG allocator hooks
void* lodepng_malloc(size_t size)             { return malloc(size);        }
void* lodepng_realloc(void* ptr, size_t size) { return realloc(ptr, size);  }
void  lodepng_free(void* ptr)                 { free(ptr);                  }

// Helper: nearest‑neighbor resize from source to destination (both RGBA 32‑bit)
static void resize_rgba(const uint32_t* src, uint32_t src_w, uint32_t src_h,
                        uint32_t* dst, uint32_t dst_w, uint32_t dst_h)
{
    for (uint32_t y = 0; y < dst_h; y++) {
        uint32_t src_y = (y * src_h) / dst_h;
        for (uint32_t x = 0; x < dst_w; x++) {
            uint32_t src_x = (x * src_w) / dst_w;
            dst[y * dst_w + x] = src[src_y * src_w + src_x];
        }
    }
}

// Load PNG into Image (original only)
bool image_load_from_png(Image* image, const unsigned char* png_data, unsigned int png_len)
{
    if (!image) return false;
    memset(image, 0, sizeof(Image));

    unsigned char* rgba = NULL;
    unsigned int w = 0, h = 0;
    unsigned err = lodepng_decode32(&rgba, &w, &h, png_data, png_len);
    if (err || !rgba) return false;

    image->original_width = w;
    image->original_height = h;
    image->original_data = (uint32_t*)malloc(w * h * sizeof(uint32_t));
    if (!image->original_data) {
        free(rgba);
        return false;
    }

    // Pack each pixel as ARGB (little‑endian friendly)
    for (uint32_t i = 0; i < w * h; i++) {
        uint8_t r = rgba[i*4 + 0];
        uint8_t g = rgba[i*4 + 1];
        uint8_t b = rgba[i*4 + 2];
        uint8_t a = rgba[i*4 + 3];
        image->original_data[i] = ((uint32_t)a << 24) |
                                  ((uint32_t)r << 16) |
                                  ((uint32_t)g <<  8) |
                                  (uint32_t)b;
    }
    free(rgba);
    return true;
}

void image_free(Image* image)
{
    if (!image) return;
    if (image->original_data) free(image->original_data);
    if (image->resized_data) free(image->resized_data);
    memset(image, 0, sizeof(Image));
}

bool image_resize(Image* image, uint32_t new_width, uint32_t new_height)
{
    if (!image || !image->original_data) return false;
    if (new_width == 0 || new_height == 0) return false;

    // If already cached with exact dimensions, nothing to do
    if (image->resized_data &&
        image->resized_width == new_width &&
        image->resized_height == new_height) {
        return true;
    }

    // Free previous resized data
    if (image->resized_data) {
        free(image->resized_data);
        image->resized_data = NULL;
    }

    uint32_t* new_data = (uint32_t*)malloc(new_width * new_height * sizeof(uint32_t));
    if (!new_data) return false;

    resize_rgba(image->original_data, image->original_width, image->original_height,
                new_data, new_width, new_height);

    image->resized_data = new_data;
    image->resized_width = new_width;
    image->resized_height = new_height;
    return true;
}

void image_draw(const Image* image, int x, int y, uint32_t target_width, uint32_t target_height)
{
    if (!image || !image->original_data) return;

    uint32_t draw_width, draw_height;
    const uint32_t* source_data;
    bool using_resized = false;

    if (target_width == 0 || target_height == 0) {
        draw_width = image->original_width;
        draw_height = image->original_height;
        source_data = image->original_data;
    } else if (image->resized_data &&
               image->resized_width == target_width &&
               image->resized_height == target_height) {
        draw_width = target_width;
        draw_height = target_height;
        source_data = image->resized_data;
        using_resized = true;
    } else {
        // No matching cached resized version - perform real‑time scaling
        draw_width = target_width;
        draw_height = target_height;
        // Allocate temporary buffer for the scaled image (could be large)
        uint32_t* temp = (uint32_t*)malloc(draw_width * draw_height * sizeof(uint32_t));
        if (!temp) return;
        resize_rgba(image->original_data, image->original_width, image->original_height,
                    temp, draw_width, draw_height);
        source_data = temp;
    }

    // Draw pixel by pixel (skip fully transparent pixels)
    for (uint32_t row = 0; row < draw_height; row++) {
        for (uint32_t col = 0; col < draw_width; col++) {
            uint32_t pixel = source_data[row * draw_width + col];
            if ((pixel >> 24) == 0) continue; // skip transparent
            draw_pixel(x + col, y + row, pixel & 0x00FFFFFF);
        }
    }

    if (!using_resized) {
        // Free the temporary buffer if we used real‑time scaling
        free((void*)source_data);
    }
}

// ---------- Legacy wallpaper support (kept for compatibility) ----------
static Image wallpaper_image = {0};

bool png_wallpaper_loaded(void)
{
    return wallpaper_image.original_data != NULL;
}

bool png_load_wallpaper(const unsigned char* png_data, unsigned int png_len)
{
    image_free(&wallpaper_image);
    if (!image_load_from_png(&wallpaper_image, png_data, png_len))
        return false;
    // Pre‑resize to full screen for faster drawing
    return image_resize(&wallpaper_image, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void png_blit_wallpaper(void)
{
    image_draw(&wallpaper_image, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}