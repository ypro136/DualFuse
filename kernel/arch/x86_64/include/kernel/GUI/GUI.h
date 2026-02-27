#ifndef XP_DESKTOP_H
#define XP_DESKTOP_H

#include <cstdint>

#include <framebufferutil.h>
#include <psf.h>

#include <gui_primitives.h>

 
extern bool GUI_input_loop();

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
    void (*draw_frame)(void*);   // function pointer + context pointer
    void* context;               // pointer to the object (e.g. Console*)
    void (*on_move)(void*, int x, int y);
};

extern XPWindow* window_arr[64];

 
// RENDERING FUNCTIONS
#if defined(DEBUG_GUI)
void direct_clear_screen_dbg ();
#endif

XPWindow* create_xp_window(int x, int y, int width, int height, const char* title);

static void Console_draw_frame_wrapper(void* context);
static void Console_on_move(void* ctx, int x, int y);
void set_active_xp_window(XPWindow* win);

// Main render function - draws complete XP desktop
void initialize_xp_desktop();
void render_xp_desktop();

// Window rendering
void draw_xp_window(XPWindow* win);
void draw_window_title_bar(XPWindow* win);
void draw_window_button(int x, int y, int size, const char* label, bool active);

XPWindow* get_window_at(int x, int y);
bool is_mouse_on_window_title_bar(XPWindow* win, int x, int y);
void move_xp_window(XPWindow* win, int x, int y);

// Controls
void draw_xp_button(int x, int y, int width, int height, const char* label, bool pressed);
void draw_scrollbar(int x, int y, int height, int scroll_pos, int max_scroll);

// Desktop elements
void draw_desktop_background();
void draw_desktop_icon(int x, int y, const char* label, uint32_t icon_color);
void draw_taskbar();

#endif