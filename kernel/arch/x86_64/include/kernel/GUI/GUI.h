#ifndef XP_DESKTOP_H
#define XP_DESKTOP_H

#include <cstdint>

#include <framebufferutil.h>
#include <psf.h>

#include <gui_primitives.h>

 
// CONFIGURATION CONSTANTS
 

#define TASKBAR_HEIGHT 28
#define TASKBAR_Y (SCREEN_HEIGHT - TASKBAR_HEIGHT)
#define TITLE_BAR_HEIGHT 24
#define WINDOW_BORDER_WIDTH 4

 
// WINDOW STRUCTURE
 

struct XPWindow {
    int x, y;
    int width, height;
    const char* title;
    bool active;
    bool minimized;
    uint32_t bg_color;
};

 
// RENDERING FUNCTIONS
#if defined(DEBUG_GUI)
void direct_clear_screen_dbg ();
#endif


// Main render function - draws complete XP desktop
void render_xp_desktop();

// Window rendering
void draw_xp_window(const XPWindow& win);
void draw_window_title_bar(const XPWindow& win);
void draw_window_button(int x, int y, int size, const char* label, bool active);

// Controls
void draw_xp_button(int x, int y, int width, int height, const char* label, bool pressed);
void draw_scrollbar(int x, int y, int height, int scroll_pos, int max_scroll);

// Desktop elements
void draw_desktop_background();
void draw_desktop_icon(int x, int y, const char* label, uint32_t icon_color);
void draw_taskbar();

#endif