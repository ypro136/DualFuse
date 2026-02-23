#include <algorithm>

#include <framebufferutil.h>
#include <psf.h>

#include <gui_primitives.h>
#include <console.h>



 
inline int abs(int x) {
    return (x < 0) ? -x : x;
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline int clamp(int val, int min_val, int max_val) {
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

uint32_t blend_colors(uint32_t color1, uint32_t color2, int alpha_256) {
    uint8_t r1 = (color1 >> 16) & 0xFF;
    uint8_t g1 = (color1 >> 8) & 0xFF;
    uint8_t b1 = color1 & 0xFF;
    
    uint8_t r2 = (color2 >> 16) & 0xFF;
    uint8_t g2 = (color2 >> 8) & 0xFF;
    uint8_t b2 = color2 & 0xFF;
    
    // Clamp alpha to 0-256
    if (alpha_256 < 0) alpha_256 = 0;
    if (alpha_256 > 256) alpha_256 = 256;
    
    // Integer interpolation: result = c1 + (c2 - c1) * alpha / 256
    uint8_t r = (uint8_t)(r1 + ((r2 - r1) * alpha_256) / 256);
    uint8_t g = (uint8_t)(g1 + ((g2 - g1) * alpha_256) / 256);
    uint8_t b = (uint8_t)(b1 + ((b2 - b1) * alpha_256) / 256);
    
    return (r << 16) | (g << 8) | b;
}

void fill_rectangle(int x, int y, int width, int height, uint32_t color) 
{
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] fill_rectangle\n");
    #endif
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            draw_pixel(x + i, y + j, color);
        }
    }
}

 
void draw_rect_outline(int x, int y, int width, int height, uint32_t color, int thickness) 
{

    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] draw_rect_outline\n");
    #endif
    // Top edge
    for (int t = 0; t < thickness; t++) {
        for (int i = 0; i < width; i++) {
            draw_pixel(x + i, y + t, color);
        }
    }
    
    // Bottom edge
    for (int t = 0; t < thickness; t++) {
        for (int i = 0; i < width; i++) {
            draw_pixel(x + i, y + height - 1 - t, color);
        }
    }
    
    // Left edge
    for (int t = 0; t < thickness; t++) {
        for (int j = 0; j < height; j++) {
            draw_pixel(x + t, y + j, color);
        }
    }
    
    // Right edge
    for (int t = 0; t < thickness; t++) {
        for (int j = 0; j < height; j++) {
            draw_pixel(x + width - 1 - t, y + j, color);
        }
    }
}

 
// Bresenham's
 

void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    int x = x0;
    int y = y0;
    
    while (true) {
        draw_pixel(x, y, color);
        
        if (x == x1 && y == y1) break;
        
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


void draw_circle(int cx, int cy, int radius, uint32_t color) {
    int x = radius;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        draw_pixel(cx + x, cy + y, color);
        draw_pixel(cx + y, cy + x, color);
        draw_pixel(cx - y, cy + x, color);
        draw_pixel(cx - x, cy + y, color);
        draw_pixel(cx - x, cy - y, color);
        draw_pixel(cx - y, cy - x, color);
        draw_pixel(cx + y, cy - x, color);
        draw_pixel(cx + x, cy - y, color);
        
        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}


void fill_circle(int cx, int cy, int radius, uint32_t color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                draw_pixel(cx + x, cy + y, color);
            }
        }
    }
}



void draw_gradient(int x, int y, int width, int height, uint32_t color1, uint32_t color2, bool vertical) {
#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] draw_gradient at (%d,%d) size %dx%d color1=0x%06X color2=0x%06X %s\n", 
           x, y, width, height, color1, color2, vertical ? "vertical" : "horizontal");
#endif
    if (vertical) {
        for (int j = 0; j < height; j++) {
            int alpha = (height > 1) ? (j * 256) / (height - 1) : 0;
            uint32_t blended = blend_colors(color1, color2, alpha);
            for (int i = 0; i < width; i++) {
                draw_pixel(x + i, y + j, blended);
            }
        }
    } else {
        for (int i = 0; i < width; i++) {
            int alpha = (width > 1) ? (i * 256) / (width - 1) : 0;
            uint32_t blended = blend_colors(color1, color2, alpha);
            for (int j = 0; j < height; j++) {
                draw_pixel(x + i, y + j, blended);
            }
        }
    }
}


void draw_beveled_border(int x, int y, int width, int height, uint32_t light, uint32_t dark, bool raised) {
    if (raised) {
        // Raised effect: light on top/left, dark on bottom/right
        for (int i = 0; i < width; i++) {
            draw_pixel(x + i, y, light);                    // Top edge
            draw_pixel(x + i, y + height - 1, dark);        // Bottom edge
        }
        for (int j = 0; j < height; j++) {
            draw_pixel(x, y + j, light);                    // Left edge
            draw_pixel(x + width - 1, y + j, dark);         // Right edge
        }
    } else {
        // Inset effect: dark on top/left, light on bottom/right
        for (int i = 0; i < width; i++) {
            draw_pixel(x + i, y, dark);                     // Top edge
            draw_pixel(x + i, y + height - 1, light);       // Bottom edge
        }
        for (int j = 0; j < height; j++) {
            draw_pixel(x, y + j, dark);                     // Left edge
            draw_pixel(x + width - 1, y + j, light);        // Right edge
        }
    }
}

void draw_beveled_border_thick(int x, int y, int width, int height, 
                               uint32_t highlight, uint32_t face, uint32_t shadow, bool raised) {
    if (raised) {
        // Top-left: highlight
        draw_line(x, y, x + width - 1, y, highlight);
        draw_line(x, y, x, y + height - 1, highlight);
        draw_line(x + 1, y + 1, x + width - 2, y + 1, highlight);
        draw_line(x + 1, y + 1, x + 1, y + height - 2, highlight);
        
        // Bottom-right: shadow
        draw_line(x, y + height - 1, x + width - 1, y + height - 1, shadow);
        draw_line(x + width - 1, y, x + width - 1, y + height - 1, shadow);
        draw_line(x + 1, y + height - 2, x + width - 2, y + height - 2, shadow);
        draw_line(x + width - 2, y + 1, x + width - 2, y + height - 2, shadow);
    } else {
        // Top-left: shadow (inset)
        draw_line(x, y, x + width - 1, y, shadow);
        draw_line(x, y, x, y + height - 1, shadow);
        draw_line(x + 1, y + 1, x + width - 2, y + 1, shadow);
        draw_line(x + 1, y + 1, x + 1, y + height - 2, shadow);
        
        // Bottom-right: highlight
        draw_line(x, y + height - 1, x + width - 1, y + height - 1, highlight);
        draw_line(x + width - 1, y, x + width - 1, y + height - 1, highlight);
        draw_line(x + 1, y + height - 2, x + width - 2, y + height - 2, highlight);
        draw_line(x + width - 2, y + 1, x + width - 2, y + height - 2, highlight);
    }
}

void draw_text(const char* text, int x, int y, uint32_t color, uint32_t bg_color) {
#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] draw_text at (%d,%d) color=0x%06X\n", x, y, color);
#endif
    if (!text) {
#if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] draw_text: text pointer is NULL\n");
#endif
        return;
    }
    if (!console_initialized)
    {
        return;
    }
    int char_x = x;
    const char* p = text;
    int char_count = 0;
    
    while (*p != '\0') 
    {
#if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] drawing char %d: '%c' (0x%02X) at x=%d, y=%d\n", char_count, *p, (unsigned char)*p, char_x, y);
#endif
        psfPutC(*p, (uint32_t)char_x, (uint32_t)y, color, bg_color);
#if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] char %d drawn successfully\n", char_count);
#endif
        char_x += 8;
        p++;
        char_count++;
    }
#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] draw_text complete - drew %d characters\n", char_count);
#endif
}


void draw_text_centered(const char* text, int x, int y, int width, uint32_t color, uint32_t bg_color) {
    int text_len = 0;
    const char* p = text;
    while (*p) {
        text_len++;
        p++;
    }
    
    int text_pixel_width = text_len * 8;  // 8 pixels per character
    int start_x = x + (width - text_pixel_width) / 2;
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] draw_text_centered ready to draw text\n");
    #endif 
    draw_text(text,  clamp(start_x, 0, screen_width), clamp(y, 0, screen_height), color, bg_color);
}

 
void draw_rectangle_with_shadow(int x, int y, int width, int height, uint32_t color, uint32_t shadow_color, int shadow_size) {
    // Draw shadow first (offset bottom-right)
    fill_rectangle(x + shadow_size, y + shadow_size, width, height, shadow_color);
    
    // Draw main rectangle
    fill_rectangle(x, y, width, height, color);
}

 
void draw_hline(int x, int y, int length, uint32_t color) 
{
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] draw_hline\n");
    #endif
    for (int i = 0; i < length; i++) {
        draw_pixel(x + i, y, color);
    }
}

void draw_vline(int x, int y, int length, uint32_t color) {
    for (int i = 0; i < length; i++) {
        draw_pixel(x, y + i, color);
    }
}

 
void draw_rect_filled_border(int x, int y, int width, int height, 
                             uint32_t fill_color, uint32_t border_color, int border_width) {
    fill_rectangle(x, y, width, height, fill_color);
    draw_rect_outline(x, y, width, height, border_color, border_width);
}

 
void fill_diamond(int cx, int cy, int size, uint32_t color) {
    for (int y = -size; y <= size; y++) {
        for (int x = -size; x <= size; x++) {
            if (abs(x) + abs(y) <= size) {
                draw_pixel(cx + x, cy + y, color);
            }
        }
    }
}

 
void draw_checkerboard(int x, int y, int width, int height, uint32_t color1, uint32_t color2, int square_size) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            int checker_x = (i / square_size) % 2;
            int checker_y = (j / square_size) % 2;
            uint32_t color = ((checker_x + checker_y) % 2 == 0) ? color1 : color2;
            draw_pixel(x + i, y + j, color);
        }
    }
}

 

void fill_rounded_rectangle(int x, int y, int width, int height, int corner_radius, uint32_t color) {
    // Fill center rectangle
    fill_rectangle(x + corner_radius, y, width - 2 * corner_radius, height, color);
    fill_rectangle(x, y + corner_radius, width, height - 2 * corner_radius, color);
    
    // Fill corner circles (approximate)
    fill_circle(x + corner_radius, y + corner_radius, corner_radius, color);
    fill_circle(x + width - corner_radius - 1, y + corner_radius, corner_radius, color);
    fill_circle(x + corner_radius, y + height - corner_radius - 1, corner_radius, color);
    fill_circle(x + width - corner_radius - 1, y + height - corner_radius - 1, corner_radius, color);
}

 

void clear_screen(int width, int height, uint32_t color) {
    fill_rectangle(0, 0, width, height, color);
}