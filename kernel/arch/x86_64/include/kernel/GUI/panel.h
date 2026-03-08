#pragma once
#include <cstdint>

struct XPPanel {
    int anchor_x, anchor_y;
    int anchor_w, anchor_h;

    int expand_scale_pct;  // e.g. 150 = 1.5x, 200 = 2x
    int lift;

    bool hovered;
    int  lerp_t;           // 0–256 fixed point (256 = fully expanded)

    void (*draw_content)(int x, int y, int w, int h, void* ctx);
    void* context;
};

#define MAX_PANELS 16
extern XPPanel* panel_arr[MAX_PANELS];

// Lifecycle
XPPanel* create_xp_panel(int x, int y, int w, int h,
                          int expand_scale_pct, int lift,
                          void (*draw_content)(int, int, int, int, void*),
                          void* ctx);

XPPanel* register_xp_panel(XPPanel* panel);
void     update_all_xp_panels(int mouse_x, int mouse_y);
void     draw_all_xp_panels();
XPPanel* register_xp_panel(XPPanel* panel);

// Call every frame before drawing
void update_xp_panel(XPPanel* panel, int mouse_x, int mouse_y);

// Call every frame to draw
void draw_xp_panel(XPPanel* panel);

// Hit-test against the ANCHOR rect (not the expanded one)
bool xp_panel_hit_test(XPPanel* panel, int mouse_x, int mouse_y);