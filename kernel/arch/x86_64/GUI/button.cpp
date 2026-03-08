#include <button.h>
#include <window.h> 

#include <gui_primitives.h>
#include <framebufferutil.h>
#include <psf.h>
#include <liballoc.h>

//   Global registry                   ─
XPButton* button_arr[MAX_NUM_OF_BUTTONS] = {0};

//   Lifecycle                     ─
XPButton* create_xp_button(XPWindow* win,
                            int x, int y,
                            int width, int height,
                            const char* label,
                            void (*func)(void*))
{
#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_button: x:%d y:%d w:%d h:%d label:%s\n",
           x, y, width, height, label ? label : "NULL");
#endif

    XPButton* button = (XPButton*)malloc(sizeof(XPButton));
    if (!button)
    {
#if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] create_xp_button: malloc FAILED\n");
#endif
        return NULL;
    }

    button->x        = win ? (win->x + x) : x;
    button->y        = win ? (win->y + y) : y;
    button->width    = width;
    button->height   = height;
    button->bg_color = XP_BUTTON_FACE;
    button->label    = label;
    button->pressed  = false;
    button->function = func;
    button->window   = win;

#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_button: done ptr:%p final x:%d y:%d\n",
           button, button->x, button->y);
#endif

    return button;
}

XPButton* register_xp_button(XPButton* btn)
{
#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] register_xp_button: ptr:%p label:%s\n",
           btn, btn && btn->label ? btn->label : "NULL");
#endif

    for (int i = 0; i < MAX_NUM_OF_BUTTONS; i++)
    {
        if (button_arr[i] == NULL)
        {
            button_arr[i] = btn;
#if defined(DEBUG_GUI)
            printf("[DEBUG_GUI] register_xp_button: registered at slot:%d\n", i);
#endif
            return btn;
        }
    }

#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] register_xp_button: button_arr is FULL!\n");
#endif
    return NULL;
}

//   Rendering                     ─
void draw_xp_button(XPButton* button)
{
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_xp_button: ptr:%p\n", button);
#endif

    if (button == NULL)
    {
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_xp_button: NULL button, skipping\n");
#endif
        return;
    }

#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_xp_button: x:%d y:%d w:%d h:%d label:%s pressed:%d\n",
           button->x, button->y, button->width, button->height,
           button->label ? button->label : "NULL", button->pressed);
#endif

    uint32_t button_color = button->bg_color ? button->bg_color : XP_BUTTON_FACE;
    uint32_t highlight    = XP_BUTTON_HIGHLIGHT;
    uint32_t shadow       = XP_BUTTON_SHADOW;

    fill_rectangle(button->x, button->y, button->width, button->height, button_color);

    if (!button->pressed)
    {
        draw_beveled_border_thick(button->x, button->y, button->width, button->height,
                                  highlight, button_color, shadow, true);
    }
    else
    {
        draw_beveled_border_thick(button->x, button->y, button->width, button->height,
                                  shadow, button_color, highlight, false);
    }

    if (button->label != NULL)
        draw_text_centered(button->label,
                           button->x,
                           button->y + (button->height - 12) / 2,
                           button->width,
                           XP_WINDOW_TEXT,
                           XP_BACKGROUND);
}

void draw_window_button(int x, int y, int size, const char* label, bool active)
{
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_window_button: x:%d y:%d size:%d label:%s active:%d\n",
           x, y, size, label ? label : "NULL", active);
#endif

    uint32_t button_color = XP_BUTTON_FACE;
    uint32_t highlight    = XP_BUTTON_HIGHLIGHT;
    uint32_t shadow       = XP_BUTTON_SHADOW;

    fill_rectangle(x, y, size, size, button_color);

    if (active)
    {
        draw_hline(x, y, size, highlight);
        draw_vline(x, y, size, highlight);
        draw_hline(x, y + size - 1, size, shadow);
        draw_vline(x + size - 1, y, size, shadow);
    }
    else
    {
        draw_hline(x, y, size, shadow);
        draw_vline(x, y, size, shadow);
        draw_hline(x, y + size - 1, size, highlight);
        draw_vline(x + size - 1, y, size, highlight);
    }

    if (label[0] == 'X')
    {
        draw_line(x + 4, y + 4, x + size - 5, y + size - 5, 0x000000);
        draw_line(x + size - 5, y + 4, x + 4, y + size - 5, 0x000000);
    }
    else if (label[0] == '_')
    {
        draw_hline(x + 4, y + size / 2, size - 8, 0x000000);
    }
    else if (label[0] == '[')
    {
        draw_rect_outline(x + 4, y + 4, size - 8, size - 8, 0x000000, 1);
    }
}

int draw_all_xp_buttons()
{
    int buttons_drawn = 0;

    for (int i = 0; i < MAX_NUM_OF_BUTTONS; i++)
    {
        if (button_arr[i] == NULL)
            continue;

        draw_xp_button(button_arr[i]);
        buttons_drawn++;

#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_all_xp_buttons: drawing button:%d label:%s\n",
               i, button_arr[i]->label ? button_arr[i]->label : "NULL");
#endif
    }

#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_all_xp_buttons: buttons_drawn:%d\n", buttons_drawn);
#endif

    return buttons_drawn;
}

//   Hit-testing                    ──
XPButton* get_button_at(int x, int y)
{
    for (int i = 0; i < MAX_NUM_OF_BUTTONS; i++)
    {
        if (button_arr[i] == NULL)
            continue;

        XPButton* btn = button_arr[i];

        if (x >= btn->x && x <= (btn->x + btn->width) &&
            y >= btn->y && y <= (btn->y + btn->height))
        {
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
            printf("[DEBUG_GUI] get_button_at: hit button:%d label:%s\n",
                   i, btn->label ? btn->label : "NULL");
#endif
            return btn;
        }
    }

    return NULL;
}