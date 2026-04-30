#pragma once

#include <cstdint>
#include <gui_defs.h>
#include <button.h>  

enum XPWindowType {
    WINDOW_TYPE_NONE         = 0,
    WINDOW_TYPE_CONSOLE      = 1,
    WINDOW_TYPE_EXPLORER     = 2,
    WINDOW_TYPE_CALC         = 3,
    WINDOW_TYPE_IMAGE_VIEWER = 4,
    WINDOW_TYPE_TEXT_EDITOR  = 5,
};

typedef struct {
    void*  context;
    void (*on_move)(void* context, int x, int y);
    void (*draw_frame)(void* context);
    void (*set_active)(void* ctx);
} XPWindowCallbacks;

struct XPWindow {
    int       x, y;
    int       width, height;
    const char* title;
    bool      active;
    bool      minimized;
    uint32_t  bg_color;
    XPWindowType window_type;

    XPButton* buttons[MAX_NUM_OF_BUTTONS_FOR_WINDOWS];

    void  (*draw_frame)(void*);
    void* context;
    void  (*on_move)(void*, int x, int y);
    void  (*set_active)(void* ctx);
};

extern XPWindow* window_arr[MAX_NUM_OF_WINDOWS];
extern XPWindow* active_xp_window;

XPWindow* create_xp_window(int x, int y, int width, int height,
                            const char* title,
                            const XPWindowCallbacks* callbacks);

void close_xp_window(void* ctx);
void maximize_xp_window(void* ctx);
void minimize_xp_window(void* ctx);
void set_active_xp_window(XPWindow* win);
void move_xp_window(XPWindow* win, int x, int y);

void draw_xp_window(XPWindow* win);
void draw_window_title_bar(XPWindow* win);

int  draw_all_xp_windows_but_active();
void draw_active_xp_window();

XPWindow* get_window_at(int x, int y);
bool      is_mouse_on_window_title_bar(XPWindow* win, int x, int y);
XPButton* is_mouse_on_window_button(XPWindow* win, int x, int y);