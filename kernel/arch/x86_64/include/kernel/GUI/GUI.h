#pragma once

#include <cstdint>
#include <framebufferutil.h>
#include <psf.h>
#include <gui_primitives.h>

#include <gui_defs.h>
#include <button.h>     
#include <window.h>     

//   Taskbar                      
struct XPTaskbar {
    int       y, height;
    uint32_t  bg_color;
    XPButton* start_button;
    XPButton* window_buttons[MAX_NUM_OF_WINDOWS];
};

extern XPTaskbar* taskbar;

//   Desktop icon                    ─
typedef struct {
    int         x, y, size;
    const char* label;
    uint32_t    icon_color;
    bool        pressed, hovered;
    void      (*on_click)(void);
} XPDesktopIcon;

extern XPDesktopIcon* desktop_icons[MAX_DESKTOP_ICONS];

//   Input loop (implemented in GUI_input.cpp)          ──
extern bool GUI_input_loop();

//   Desktop lifecycle                  ──
void initialize_xp_desktop();
void render_xp_desktop();

//   Desktop background                  ─
void draw_desktop_background();

//   Taskbar                      
XPTaskbar* create_taskbar();
void       draw_taskbar();
void       taskbar_sync_windows();

//   Desktop icons                    
XPDesktopIcon* create_desktop_icon(int x, int y, const char* label,
                                    uint32_t icon_color, void (*on_click)(void));
XPDesktopIcon* get_desktop_icon_at(int x, int y);

void draw_desktop_icon(XPDesktopIcon* icon);
void draw_all_desktop_icons();

bool desktop_icon_hit_test(XPDesktopIcon* icon, int mouse_x, int mouse_y);
void desktop_icons_handle_mouse(int mouse_x, int mouse_y, bool clicked);

//   Misc widgets                    ─
void draw_scrollbar(int x, int y, int height, int scroll_pos, int max_scroll);

//   Utility                      
char* u64toa(uint64_t value, char* str, int base);

//   Debug                      ──
#if defined(DEBUG_GUI)
void direct_clear_screen_dbg();
#endif