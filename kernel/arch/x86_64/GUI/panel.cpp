#include <panel.h>
#include <gui_primitives.h>
#include <liballoc.h>

XPPanel* panel_arr[MAX_PANELS] = {0};


XPPanel* register_xp_panel(XPPanel* panel)
{
    for (int i = 0; i < MAX_PANELS; i++)
    {
        if (panel_arr[i] == NULL)
        {
            panel_arr[i] = panel;
            return panel;
        }
    }
    return NULL;  // array full
}

void update_all_xp_panels(int mouse_x, int mouse_y)
{
    for (int i = 0; i < MAX_PANELS; i++)
    {
        if (panel_arr[i])
            update_xp_panel(panel_arr[i], mouse_x, mouse_y);
    }
}

void draw_all_xp_panels()
{
    for (int i = 0; i < MAX_PANELS; i++)
    {
        if (panel_arr[i])
            draw_xp_panel(panel_arr[i]);
    }
}

#define LERP_MAX  256
#define LERP_SPEED 18    // tune this for faster/slower animation

static inline int lerp_int(int a, int b, int t)
{
    return a + ((b - a) * t) / LERP_MAX;
}

XPPanel* create_xp_panel(int x, int y, int w, int h,
                          int expand_scale_pct, int lift,
                          void (*draw_content)(int, int, int, int, void*),
                          void* ctx)
{
    XPPanel* p = (XPPanel*)malloc(sizeof(XPPanel));
    if (!p) return nullptr;

    p->anchor_x        = x;
    p->anchor_y        = y;
    p->anchor_w        = w;
    p->anchor_h        = h;
    p->expand_scale_pct = expand_scale_pct;
    p->lift            = lift;
    p->hovered         = false;
    p->lerp_t          = 0;
    p->draw_content    = draw_content;
    p->context         = ctx;

    return p;
}

bool xp_panel_hit_test(XPPanel* panel, int mouse_x, int mouse_y)
{
    return (mouse_x >= panel->anchor_x &&
            mouse_x <= panel->anchor_x + panel->anchor_w &&
            mouse_y >= panel->anchor_y &&
            mouse_y <= panel->anchor_y + panel->anchor_h);
}

void update_xp_panel(XPPanel* panel, int mouse_x, int mouse_y)
{
    panel->hovered = xp_panel_hit_test(panel, mouse_x, mouse_y);

    if (panel->hovered)
    {
        panel->lerp_t += LERP_SPEED;
        if (panel->lerp_t > LERP_MAX) panel->lerp_t = LERP_MAX;
    }
    else
    {
        panel->lerp_t -= LERP_SPEED;
        if (panel->lerp_t < 0) panel->lerp_t = 0;
    }
}

void draw_xp_panel(XPPanel* panel)
{
    if (panel->lerp_t <= 0)
        return;

    int t = panel->lerp_t;  // 0-256

    int cur_w = lerp_int(panel->anchor_w, (panel->anchor_w * panel->expand_scale_pct) / 100, t);
    int cur_h = lerp_int(panel->anchor_h, (panel->anchor_h * panel->expand_scale_pct) / 100, t);

    int cur_x = panel->anchor_x - (cur_w - panel->anchor_w) / 2;
    int cur_y = panel->anchor_y - (panel->lift * t) / LERP_MAX
                                - (cur_h - panel->anchor_h);

    //  Background 
    draw_rectangle_with_shadow(cur_x, cur_y, cur_w, cur_h, 0xF0F0F0, 0x444444, 3);
    draw_beveled_border_thick(cur_x, cur_y, cur_w, cur_h,
                              0xFFFFFF, 0xF0F0F0, 0x808080, true);

    //  Content 
    if (panel->draw_content)
        panel->draw_content(cur_x + 4, cur_y + 4, cur_w  - 8, cur_h  - 8, panel->context);
}