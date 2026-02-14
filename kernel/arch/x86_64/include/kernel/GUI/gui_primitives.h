#ifndef GUI_PRIMITIVES_H
#define GUI_PRIMITIVES_H

#include <cstdint>

 
// EXTERNAL DEPENDENCIES
 
// You must provide these functions:
extern void draw_pixel(int x, int y, uint32_t rgb);
extern void psfPutC(char c, uint32_t x, uint32_t y, uint32_t rgb);

 
// BASIC UTILITY FUNCTIONS
 

inline float lerp(float a, float b, float t);
inline int clamp(int val, int min_val, int max_val);
uint32_t blend_colors(uint32_t color1, uint32_t color2, float alpha);

 
// FILLED SHAPES
 

void fill_rectangle(int x, int y, int width, int height, uint32_t color);
void fill_circle(int cx, int cy, int radius, uint32_t color);
void fill_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);
void fill_diamond(int cx, int cy, int size, uint32_t color);
void fill_rounded_rectangle(int x, int y, int width, int height, int corner_radius, uint32_t color);

 
// OUTLINES AND BORDERS
 

void draw_rect_outline(int x, int y, int width, int height, uint32_t color, int thickness);
void draw_circle(int cx, int cy, int radius, uint32_t color);
void draw_beveled_border(int x, int y, int width, int height, uint32_t light, uint32_t dark, bool raised);
void draw_beveled_border_thick(int x, int y, int width, int height, 
                               uint32_t highlight, uint32_t face, uint32_t shadow, bool raised);
void draw_rect_filled_border(int x, int y, int width, int height, 
                             uint32_t fill_color, uint32_t border_color, int border_width);

 
// LINES
 

void draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void draw_hline(int x, int y, int length, uint32_t color);
void draw_vline(int x, int y, int length, uint32_t color);

 
// GRADIENTS AND PATTERNS
 

void draw_gradient(int x, int y, int width, int height, uint32_t color1, uint32_t color2, bool vertical);
void draw_checkerboard(int x, int y, int width, int height, uint32_t color1, uint32_t color2, int square_size);

 
// TEXT RENDERING
 

void draw_text(const char* text, int x, int y, uint32_t color);
void draw_text_centered(const char* text, int x, int y, int width, uint32_t color);

 
// ADVANCED
 

void draw_rectangle_with_shadow(int x, int y, int width, int height, uint32_t color, uint32_t shadow_color, int shadow_size);
void clear_screen(int width, int height, uint32_t color);

 
// XP COLOR PALETTE CONSTANTS
 

static const uint32_t XP_BACKGROUND = 0xC0C0C0;      // Light gray
static const uint32_t XP_BUTTON_FACE = 0xBFBFBF;     // Button face
static const uint32_t XP_BUTTON_HIGHLIGHT = 0xDFDFDF; // Light highlight
static const uint32_t XP_BUTTON_SHADOW = 0x808080;    // Dark shadow
static const uint32_t XP_BUTTON_DARK_SHADOW = 0x000000; // Very dark
static const uint32_t XP_TITLE_BAR = 0x000080;       // Dark blue
static const uint32_t XP_TITLE_TEXT = 0xFFFFFF;      // White
static const uint32_t XP_WINDOW_TEXT = 0x000000;     // Black
static const uint32_t XP_TASKBAR = 0xC0C0C0;         // Light gray

#endif // GUI_PRIMITIVES_H