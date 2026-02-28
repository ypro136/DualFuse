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

#define MAX_NUM_OF_BUTTONS_FOR_WINDOWS 4

#define MAX_NUM_OF_WINDOWS 64
#define MAX_NUM_OF_BUTTONS 32


 
struct XPWindow;

// BUTTON STRUCTURE
struct XPButton {
    int x, y;
    int width, height;
    uint32_t bg_color;
    const char* label;
    bool pressed;
    void (*function)(void*);
    XPWindow* window;       // pointer is fine with forward declaration
};

// WINDOW STRUCTURE
struct XPWindow {
    int x, y;
    int width, height;
    const char* title;
    bool active;
    bool minimized;
    uint32_t bg_color;
    XPButton* buttons[MAX_NUM_OF_BUTTONS_FOR_WINDOWS];
    void (*draw_frame)(void*);
    void* context;
    void (*on_move)(void*, int x, int y);
};

struct XPTaskbar {
    int y;
    int height;
    uint32_t bg_color;
    XPButton* start_button;
    XPButton* window_buttons[MAX_NUM_OF_WINDOWS];
};

extern XPTaskbar* taskbar;

extern XPWindow* window_arr[64];

 
// RENDERING FUNCTIONS
#if defined(DEBUG_GUI)
void direct_clear_screen_dbg ();
#endif

XPWindow* create_xp_window(int x, int y, int width, int height, const char* title);

static void Console_draw_frame_wrapper(void* context);
static void Console_on_move(void* ctx, int x, int y);
void set_active_xp_window(XPWindow* win);

void close_xp_window(void* ctx);
void maximize_xp_window(void* ctx);
void minimize_xp_window(void* ctx);

XPButton* get_button_at(int x, int y);
XPButton* is_mouse_on_window_button(XPWindow* win, int x, int y);

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
XPButton* create_xp_button(XPWindow* win, int x, int y, int width, int height, const char* label, void (*func)(void*));
XPButton* register_xp_button(XPButton* btn);
void draw_xp_button(int x, int y, int width, int height, const char* label, bool pressed);
void draw_scrollbar(int x, int y, int height, int scroll_pos, int max_scroll);

void taskbar_sync_windows();

// Desktop elements
void draw_desktop_background();
void draw_desktop_icon(int x, int y, const char* label, uint32_t icon_color);
void draw_taskbar();

#endif