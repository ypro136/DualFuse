#pragma once

#include <cstdint>
#include <gui_defs.h>

// Forward declaration — XPWindow is defined in window.h
struct XPWindow;

//   Button structure                   
struct XPButton {
    int       x, y;
    int       width, height;
    uint32_t  bg_color;
    const char* label;
    bool      pressed;
    void    (*function)(void*);
    XPWindow* window;   // owning window (NULL for standalone buttons)
};

//   Global button registry                 
extern XPButton* button_arr[MAX_NUM_OF_BUTTONS];

//   Lifecycle                     ─
/**
 * Allocate and populate a button.
 * If @win is non-NULL the x/y are treated as offsets from the window origin.
 * @func receives the owning XPWindow* as its void* argument.
 */
XPButton* create_xp_button(XPWindow* win,
                            int x, int y,
                            int width, int height,
                            const char* label,
                            void (*func)(void*));

/**
 * Register a button in button_arr so it participates in hit-testing
 * and draw_all_xp_buttons(). Returns @btn on success, NULL if array full.
 */
XPButton* register_xp_button(XPButton* btn);

//   Rendering                     ─
void draw_xp_button(XPButton* button);

/**
 * Low-level helper used by draw_xp_window() to render the title-bar
 * close / maximise / minimise glyphs without a full XPButton object.
 */
void draw_window_button(int x, int y, int size, const char* label, bool active);

/** Draw every button in button_arr. Returns the number drawn. */
int  draw_all_xp_buttons();

//   Hit-testing                    ──
/** Return the first standalone button in button_arr that contains (x,y). */
XPButton* get_button_at(int x, int y);