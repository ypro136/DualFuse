#include <graphic_composer.h>
#include <stdlib.h>
#include <liballoc.h>
#include <string.h>
#include <stddef.h>
#include <framebufferutil.h>

/*
GraphicComposer only manages direct pixel manipulation on RGBAPixel buffers
and convertion to system format and coping too system double buffer 
for window manipulation resizing moving 
for other operations see window manager 
*/

 
// Constructor and Destructor
 

GraphicComposer::GraphicComposer(uint32_t width, uint32_t height)
    : buffer_width(width), buffer_height(height), buffer_size(width * height)
{
    // Allocate composition buffer (RGBA - 64-bit per pixel)
    composition_buffer = (RGBAPixel*)malloc(buffer_size * sizeof(RGBAPixel));
    
    // Do NOT allocate a separate system buffer. We will write converted
    // 32-bit RGB output directly into the kernel's temporary framebuffer
    // (`tempframebuffer->address`) to avoid duplicating the full framebuffer
    // and to match the existing framebuffer utility behavior.
    if (composition_buffer) {
        memset(composition_buffer, 0, buffer_size * sizeof(RGBAPixel));
    }
    // TODO: graphic_composer_initialized = true;  // Will be added later
}

GraphicComposer::~GraphicComposer()
{
    if (composition_buffer) {
        free(composition_buffer);
        composition_buffer = nullptr;
    }
}

 
// Inline Helper Methods
 

inline RGBAPixel GraphicComposer::rgba_encode(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return rgba_make(r, g, b, a);
}

inline void GraphicComposer::rgba_decode(RGBAPixel pixel, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a)
{
    r = rgba_get_r(pixel);
    g = rgba_get_g(pixel);
    b = rgba_get_b(pixel);
    a = rgba_get_a(pixel);
}

inline uint32_t GraphicComposer::blend_pixels(RGBAPixel fg, RGBAPixel bg)
{
    uint8_t fg_r, fg_g, fg_b, fg_a;
    uint8_t bg_r, bg_g, bg_b, bg_a;
    
    rgba_decode(fg, fg_r, fg_g, fg_b, fg_a);
    rgba_decode(bg, bg_r, bg_g, bg_b, bg_a);
    
    // Alpha blending formula: out = fg + bg * (1 - alpha)
    uint32_t alpha = fg_a + 1;  // 0-256 range
    uint32_t inv_alpha = 257 - alpha;
    
    uint8_t out_r = (uint8_t)((fg_r * alpha + bg_r * inv_alpha) >> 8);
    uint8_t out_g = (uint8_t)((fg_g * alpha + bg_g * inv_alpha) >> 8);
    uint8_t out_b = (uint8_t)((fg_b * alpha + bg_b * inv_alpha) >> 8);
    
    return rgba_to_rgb(rgba_make(out_r, out_g, out_b, 0xFF));
}

inline bool GraphicComposer::is_within_bounds(uint32_t x, uint32_t y) const
{
    return (x < buffer_width) && (y < buffer_height);
}

 
// RGBA Pixel Manipulation
 

void GraphicComposer::set_pixel(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!is_within_bounds(x, y) || !composition_buffer) {
        return;
    }
    
    uint64_t index = (uint64_t)y * buffer_width + x;
    composition_buffer[index] = rgba_encode(r, g, b, a);
}

RGBAPixel GraphicComposer::get_pixel(uint32_t x, uint32_t y) const
{
    if (!is_within_bounds(x, y) || !composition_buffer) {
        return 0;
    }
    
    uint64_t index = (uint64_t)y * buffer_width + x;
    return composition_buffer[index];
}

void GraphicComposer::set_pixel_encoded(uint32_t x, uint32_t y, RGBAPixel pixel)
{
    if (!is_within_bounds(x, y) || !composition_buffer) {
        return;
    }
    
    uint64_t index = (uint64_t)y * buffer_width + x;
    composition_buffer[index] = pixel;
}

 
// Buffer Operations
 

void GraphicComposer::clear_buffer(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!composition_buffer) {
        return;
    }
    
    RGBAPixel color = rgba_encode(r, g, b, a);
    for (uint64_t i = 0; i < buffer_size; i++) {
        composition_buffer[i] = color;
    }
}

void GraphicComposer::clear_transparent()
{
    clear_buffer(0, 0, 0, 0);
}

void GraphicComposer::fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!composition_buffer) {
        return;
    }
    
    // Clamp rectangle to buffer bounds
    if (x >= buffer_width || y >= buffer_height) {
        return;
    }
    
    uint32_t end_x = (x + width > buffer_width) ? buffer_width : x + width;
    uint32_t end_y = (y + height > buffer_height) ? buffer_height : y + height;
    
    RGBAPixel color = rgba_encode(r, g, b, a);
    
    for (uint32_t row = y; row < end_y; row++) {
        uint64_t row_offset = (uint64_t)row * buffer_width;
        for (uint32_t col = x; col < end_x; col++) {
            composition_buffer[row_offset + col] = color;
        }
    }
}

void GraphicComposer::draw_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                                uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!composition_buffer) {
        return;
    }
    // Integer Bresenham's line algorithm (avoids floating-point/SSE)
    int ix0 = (int)x0;
    int iy0 = (int)y0;
    int ix1 = (int)x1;
    int iy1 = (int)y1;

    int dx = ix1 - ix0;
    if (dx < 0) dx = -dx;
    int dy = iy1 - iy0;
    if (dy < 0) dy = -dy;
    int sx = (ix0 < ix1) ? 1 : -1;
    int sy = (iy0 < iy1) ? 1 : -1;
    int err = dx - dy;

    RGBAPixel color = rgba_encode(r, g, b, a);

    int x = ix0;
    int y = iy0;

    while (true) {
        if (x >= 0 && y >= 0 && (uint32_t)x < buffer_width && (uint32_t)y < buffer_height) {
            uint64_t index = (uint64_t)y * buffer_width + (uint32_t)x;
            composition_buffer[index] = color;
        }

        if (x == ix1 && y == iy1) {
            break;
        }

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

void GraphicComposer::draw_rect_outline(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                        uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint32_t thickness)
{
    if (!composition_buffer || thickness == 0) {
        return;
    }
    
    // Top edge
    fill_rect(x, y, width, thickness, r, g, b, a);
    
    // Bottom edge
    if (y + height > thickness) {
        fill_rect(x, y + height - thickness, width, thickness, r, g, b, a);
    }
    
    // Left edge
    fill_rect(x, y, thickness, height, r, g, b, a);
    
    // Right edge
    if (x + width > thickness) {
        fill_rect(x + width - thickness, y, thickness, height, r, g, b, a);
    }
}

void GraphicComposer::draw_circle(uint32_t cx, uint32_t cy, uint32_t radius,
                                  uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!composition_buffer) {
        return;
    }
    
    RGBAPixel color = rgba_encode(r, g, b, a);
    
    // Midpoint circle algorithm
    int x = (int)radius;
    int y = 0;
    int d = 3 - 2 * (int)radius;
    
    while (x >= y) {
        // 8-way symmetry
        if (is_within_bounds(cx + x, cy + y)) composition_buffer[((uint64_t)(cy + y)) * buffer_width + (cx + x)] = color;
        if (is_within_bounds(cx - x, cy + y)) composition_buffer[((uint64_t)(cy + y)) * buffer_width + (cx - x)] = color;
        if (is_within_bounds(cx + x, cy - y)) composition_buffer[((uint64_t)(cy - y)) * buffer_width + (cx + x)] = color;
        if (is_within_bounds(cx - x, cy - y)) composition_buffer[((uint64_t)(cy - y)) * buffer_width + (cx - x)] = color;
        if (is_within_bounds(cx + y, cy + x)) composition_buffer[((uint64_t)(cy + x)) * buffer_width + (cx + y)] = color;
        if (is_within_bounds(cx - y, cy + x)) composition_buffer[((uint64_t)(cy + x)) * buffer_width + (cx - y)] = color;
        if (is_within_bounds(cx + y, cy - x)) composition_buffer[((uint64_t)(cy - x)) * buffer_width + (cx + y)] = color;
        if (is_within_bounds(cx - y, cy - x)) composition_buffer[((uint64_t)(cy - x)) * buffer_width + (cx - y)] = color;
        
        if (d < 0) {
            d = d + 4 * y + 6;
        } else {
            d = d + 4 * (y - x) + 10;
            x--;
        }
        y++;
    }
}

 
// Frame Buffer Composition
 

FrameBuffer* GraphicComposer::create_framebuffer(uint32_t width, uint32_t height)
{
    FrameBuffer* fb = (FrameBuffer*)malloc(sizeof(FrameBuffer));
    if (!fb) {
        return nullptr;
    }
    
    fb->width = width;
    fb->height = height;
    fb->x_offset = 0;
    fb->y_offset = 0;
    fb->data = (RGBAPixel*)malloc(width * height * sizeof(RGBAPixel));
    
    if (!fb->data) {
        free(fb);
        return nullptr;
    }
    
    memset(fb->data, 0, width * height * sizeof(RGBAPixel));
    return fb;
}

void GraphicComposer::destroy_framebuffer(FrameBuffer* fb)
{
    if (!fb) {
        return;
    }
    
    if (fb->data) {
        free(fb->data);
    }
    
    free(fb);
}

void GraphicComposer::composite_framebuffer(const FrameBuffer* fb)
{
    if (!fb || !fb->data || !composition_buffer) {
        return;
    }
    
    uint32_t start_x = fb->x_offset;
    uint32_t start_y = fb->y_offset;
    
    // Clamp to buffer bounds
    if (start_x >= buffer_width || start_y >= buffer_height) {
        return;
    }
    
    uint32_t end_x = (start_x + fb->width > buffer_width) ? buffer_width : start_x + fb->width;
    uint32_t end_y = (start_y + fb->height > buffer_height) ? buffer_height : start_y + fb->height;
    
    for (uint32_t y = start_y; y < end_y; y++) {
        uint32_t fb_y = y - start_y;
        uint64_t comp_offset = (uint64_t)y * buffer_width;
        uint64_t fb_offset = (uint64_t)fb_y * fb->width;
        
        for (uint32_t x = start_x; x < end_x; x++) {
            uint32_t fb_x = x - start_x;
            RGBAPixel fg = fb->data[fb_offset + fb_x];
            RGBAPixel bg = composition_buffer[comp_offset + x];
            
            uint8_t alpha = rgba_get_a(fg);
            
            // Only composite if foreground has some opacity
            if (alpha > 0) {
                composition_buffer[comp_offset + x] = fg;
            }
        }
    }
}

void GraphicComposer::composite_multiple(const FrameBuffer** framebuffers, uint32_t count)
{
    if (!framebuffers || count == 0) {
        return;
    }
    
    for (uint32_t i = 0; i < count; i++) {
        if (framebuffers[i]) {
            composite_framebuffer(framebuffers[i]);
        }
    }
}

void GraphicComposer::fill_framebuffer(FrameBuffer* fb, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    if (!fb || !fb->data) {
        return;
    }
    
    RGBAPixel color = rgba_encode(r, g, b, a);
    uint64_t size = (uint64_t)fb->width * fb->height;
    
    for (uint64_t i = 0; i < size; i++) {
        fb->data[i] = color;
    }
}

void GraphicComposer::copy_framebuffer(const FrameBuffer* src, FrameBuffer* dst,
                                       uint32_t dst_x, uint32_t dst_y)
{
    if (!src || !dst || !src->data || !dst->data) {
        return;
    }
    
    // Clamp to destination buffer bounds
    if (dst_x >= dst->width || dst_y >= dst->height) {
        return;
    }
    
    uint32_t end_x = (dst_x + src->width > dst->width) ? dst->width : dst_x + src->width;
    uint32_t end_y = (dst_y + src->height > dst->height) ? dst->height : dst_y + src->height;
    
    for (uint32_t y = dst_y; y < end_y; y++) {
        uint32_t src_y = y - dst_y;
        uint64_t src_offset = (uint64_t)src_y * src->width;
        uint64_t dst_offset = (uint64_t)y * dst->width;
        
        for (uint32_t x = dst_x; x < end_x; x++) {
            uint32_t src_x = x - dst_x;
            dst->data[dst_offset + x] = src->data[src_offset + src_x];
        }
    }
}

 
// System Framebuffer Conversion
 

void GraphicComposer::convert_to_system_format(uint32_t bg_color)
{
    if (!composition_buffer || !tempframebuffer || !tempframebuffer->address) {
        return;
    }

    volatile uint32_t *tfb_ptr = (volatile uint32_t*)tempframebuffer->address;
    RGBAPixel bg_rgba = rgb_to_rgba(bg_color);

    // Ensure we don't write past the physical framebuffer; use min of sizes
    uint64_t fb_pixels = (uint64_t)screen_width * (uint64_t)screen_height;
    uint64_t max_pixels = buffer_size < fb_pixels ? buffer_size : fb_pixels;

    for (uint64_t i = 0; i < max_pixels; i++) {
        RGBAPixel rgba_pixel = composition_buffer[i];
        uint8_t alpha = rgba_get_a(rgba_pixel);

        if (alpha == 0) {
            tfb_ptr[i] = bg_color;
        } else if (alpha == 0xFF) {
            tfb_ptr[i] = rgba_to_rgb(rgba_pixel);
        } else {
            tfb_ptr[i] = blend_pixels(rgba_pixel, bg_rgba);
        }
    }
}

const uint32_t* GraphicComposer::get_system_buffer() const
{
    if (!tempframebuffer || !tempframebuffer->address) 
    {
        return nullptr;
    }
    return (const uint32_t*)tempframebuffer->address;
}

void GraphicComposer::copy_to_framebuffer(void* framebuffer_ptr, uint64_t framebuffer_size)
{
    if (!tempframebuffer || !tempframebuffer->address || !framebuffer_ptr) {
        return;
    }

    // Compute number of pixels available in both buffers
    uint64_t fb_pixels = (uint64_t)screen_width * (uint64_t)screen_height;
    uint64_t max_pixels = buffer_size < fb_pixels ? buffer_size : fb_pixels;
    uint64_t max_bytes = max_pixels * sizeof(uint32_t);

    uint64_t copy_size = framebuffer_size < max_bytes ? framebuffer_size : max_bytes;
    memcpy(framebuffer_ptr, (void*)tempframebuffer->address, copy_size);
}

 
// Buffer Access and Information
 

RGBAPixel* GraphicComposer::get_composition_buffer() const
{
    return composition_buffer;
}
