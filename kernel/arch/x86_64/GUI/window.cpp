#include <window.h>

#include <gui_primitives.h>
#include <framebufferutil.h>
#include <psf.h>
#include <liballoc.h>

// taskbar_sync_windows() is declared in GUI.h — forward-declare here
// to avoid a circular include (GUI.h → window.h → GUI.h).
extern void taskbar_sync_windows();

//   Global registry                   ─
XPWindow* window_arr[MAX_NUM_OF_WINDOWS]  = {0};
XPWindow* active_xp_window               = NULL;

//   Lifecycle                     ─
XPWindow* create_xp_window(int x, int y, int width, int height,
                            const char* title,
                            const XPWindowCallbacks* callbacks)
{
#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_window: x:%d y:%d width:%d height:%d title:%s\n",
           x, y, width, height, title ? title : "NULL");
#endif

    XPWindow* window = (XPWindow*)malloc(sizeof(XPWindow));
    if (!window)
    {
#if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] create_xp_window: malloc FAILED\n");
#endif
        return NULL;
    }

    for (int i = 0; i < MAX_NUM_OF_BUTTONS_FOR_WINDOWS; i++)
        window->buttons[i] = NULL;

    window->x         = x;
    window->y         = y;
    window->width     = width;
    window->height    = height;
    window->title     = title;
    window->active    = false;
    window->minimized = false;
    window->bg_color  = XP_BACKGROUND;

    if (callbacks)
    {
        window->on_move    = callbacks->on_move;
        window->draw_frame = callbacks->draw_frame;
        window->context    = callbacks->context;
        window->set_active = callbacks->set_active;
    }
    else
    {
        window->on_move    = NULL;
        window->draw_frame = NULL;
        window->context    = NULL;
        window->set_active = NULL;
    }

#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_window: malloc OK ptr:%p, creating title-bar buttons\n", window);
#endif

    const int btn_y    = 4;
    const int btn_size = 16;

    window->buttons[0] = create_xp_button(window, window->width - (btn_size + 4) * 1,
                                           btn_y, btn_size, btn_size, "X", close_xp_window);
    window->buttons[1] = create_xp_button(window, window->width - (btn_size + 4) * 2,
                                           btn_y, btn_size, btn_size, "[", maximize_xp_window);
    window->buttons[2] = create_xp_button(window, window->width - (btn_size + 4) * 3,
                                           btn_y, btn_size, btn_size, "_", minimize_xp_window);

#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_window: buttons: close=%p maximize=%p minimize=%p\n",
           window->buttons[0], window->buttons[1], window->buttons[2]);
#endif

    // Register in global window array
    bool slot_found = false;
    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i] == NULL)
        {
            window_arr[i] = window;
            slot_found    = true;
#if defined(DEBUG_GUI)
            printf("[DEBUG_GUI] create_xp_window: registered in window_arr slot:%d\n", i);
#endif
            break;
        }
    }

#if defined(DEBUG_GUI)
    if (!slot_found)
        printf("[DEBUG_GUI] create_xp_window: window_arr FULL, window not registered!\n");
#endif

    set_active_xp_window(window);
    taskbar_sync_windows();

#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_window: done\n");
#endif

    return window;
}

void close_xp_window(void* ctx)
{
#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] close_xp_window: ctx:%p\n", ctx);
#endif

    XPWindow* window = (XPWindow*)ctx;
    if (!window)
    {
#if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] close_xp_window: window is NULL, aborting\n");
#endif
        return;
    }

    // Remove from global array
    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i] == window)
        {
            window_arr[i] = NULL;
#if defined(DEBUG_GUI)
            printf("[DEBUG_GUI] close_xp_window: removed from window_arr slot:%d\n", i);
#endif
            break;
        }
    }

    // Free title-bar buttons
    for (int i = 0; i < MAX_NUM_OF_BUTTONS_FOR_WINDOWS; i++)
    {
        if (window->buttons[i] != NULL)
        {
            free(window->buttons[i]);
            window->buttons[i] = NULL;
        }
    }

    free(window->context);
    free(window);

#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] close_xp_window: freed, searching for next active\n");
#endif

    // Promote the first remaining window to active
    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i] != NULL)
        {
            set_active_xp_window(window_arr[i]);
            taskbar_sync_windows();
            return;
        }
    }

    active_xp_window = NULL;
#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] close_xp_window: no windows left\n");
#endif
}

void maximize_xp_window(void* ctx)
{
#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] maximize_xp_window: ctx:%p\n", ctx);
#endif

    XPWindow* window = (XPWindow*)ctx;
    if (!window) return;

    window->x      = 0;
    window->y      = 0;
    window->width  = SCREEN_WIDTH;
    window->height = SCREEN_HEIGHT - TASKBAR_HEIGHT;

    // Reposition title-bar buttons
    const int btn_y    = window->y + 4;
    const int btn_size = 16;
    for (int i = 0; i < 3; i++)
    {
        if (window->buttons[i])
        {
            window->buttons[i]->x = window->x + window->width - (btn_size + 4) * (i + 1);
            window->buttons[i]->y = btn_y;
        }
    }

    if (window->context && window->on_move)
        window->on_move(window->context, window->x, window->y + TITLE_BAR_HEIGHT);

#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] maximize_xp_window: x:%d y:%d w:%d h:%d\n",
           window->x, window->y, window->width, window->height);
#endif
}

void minimize_xp_window(void* ctx)
{
    XPWindow* window = (XPWindow*)ctx;
    if (!window) return;

    window->minimized = !window->minimized;

#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] minimize_xp_window: minimized=%d\n", window->minimized);
#endif
}

void set_active_xp_window(XPWindow* win)
{
    if (!win) return;

    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i])
            window_arr[i]->active = false;
    }

    active_xp_window = win;
    win->active      = true;

    if (win->set_active && win->context)
        win->set_active(win->context);

#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] set_active_xp_window: title:%s\n", win->title ? win->title : "NULL");
#endif
}

void move_xp_window(XPWindow* win, int x, int y)
{
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] move_xp_window: x:%d y:%d\n", x, y);
#endif

    win->x = x;
    win->y = y;

    if (win->context && win->on_move)
        win->on_move(win->context, x, y + TITLE_BAR_HEIGHT);

    // Reposition title-bar buttons
    const int btn_y    = win->y + 4;
    const int btn_size = 16;
    for (int i = 0; i < 3; i++)
    {
        if (win->buttons[i])
        {
            win->buttons[i]->x = win->x + win->width - (btn_size + 4) * (i + 1);
            win->buttons[i]->y = btn_y;
        }
    }
}

//   Rendering 
void draw_window_title_bar(XPWindow* win)
{
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_window_title_bar: x:%d y:%d w:%d\n", win->x, win->y, win->width);
#endif

    draw_gradient(win->x, win->y, win->width, TITLE_BAR_HEIGHT, 0x000080, 0x1084D7, true);
    draw_line(win->x + 1, win->y + 1, win->x + win->width - 2, win->y + 1, 0x2E5C8A);
    draw_text(win->title, win->x + 8, win->y + 6, XP_WINDOW_TEXT, 0x4040f0);
}

void draw_xp_window(XPWindow* win)
{
    if (!win || win->minimized)
    {
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_xp_window: skipping win:%p minimized:%d\n",
               win, win ? win->minimized : -1);
#endif
        return;
    }

#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_xp_window: x:%d y:%d w:%d h:%d title:%s\n",
           win->x, win->y, win->width, win->height, win->title ? win->title : "NULL");
#endif

    fill_rectangle(win->x, win->y, win->width, win->height, XP_BACKGROUND);

    draw_window_title_bar(win);

    draw_beveled_border_thick(win->x, win->y, win->width, win->height,
                              XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE, XP_BUTTON_SHADOW, true);

    const int client_x      = win->x + WINDOW_BORDER_WIDTH;
    const int client_y      = win->y + TITLE_BAR_HEIGHT + WINDOW_BORDER_WIDTH;
    const int client_width  = win->width  - 2 * WINDOW_BORDER_WIDTH;
    const int client_height = win->height - TITLE_BAR_HEIGHT - 2 * WINDOW_BORDER_WIDTH;

    fill_rectangle(client_x, client_y, client_width, client_height, win->bg_color);

    // Draw title-bar control buttons
    const int btn_y    = win->y + 4;
    const int btn_size = 16;
    draw_window_button(win->x + win->width - (btn_size + 4) * 1, btn_y, btn_size, "X", true);
    draw_window_button(win->x + win->width - (btn_size + 4) * 2, btn_y, btn_size, "[", true);
    draw_window_button(win->x + win->width - (btn_size + 4) * 3, btn_y, btn_size, "_", true);

    // Draw content
    if (win->context && win->draw_frame)
    {
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_xp_window: calling draw_frame context:%p\n", win->context);
#endif
        win->draw_frame(win->context);
    }
}

int draw_all_xp_windows_but_active()
{
    int drawn = 0;

    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i] == NULL)
            break;

        if (!window_arr[i]->active)
        {
            draw_xp_window(window_arr[i]);
            drawn++;
        }
    }

    return drawn;
}

void draw_active_xp_window()
{
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_active_xp_window: ptr:%p\n", active_xp_window);
#endif

    if (active_xp_window)
        draw_xp_window(active_xp_window);
}

//   Hit-testing                    ──
XPWindow* get_window_at(int x, int y)
{
    XPWindow* result = NULL;

    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i] == NULL) continue;
        XPWindow* win = window_arr[i];

        if (x >= win->x && x <= (win->x + win->width) &&
            y >= win->y && y <= (win->y + win->height))
        {
            if (win->active) return win;   // prefer the active window
            result = win;
        }
    }

    return result;
}

bool is_mouse_on_window_title_bar(XPWindow* win, int x, int y)
{
    if (!win) return false;
    return (x >= win->x && x <= (win->x + win->width) &&
            y >= win->y && y <= (win->y + TITLE_BAR_HEIGHT));
}

XPButton* is_mouse_on_window_button(XPWindow* win, int x, int y)
{
    if (!win) return NULL;

    for (int i = 0; i < MAX_NUM_OF_BUTTONS_FOR_WINDOWS; i++)
    {
        XPButton* btn = win->buttons[i];
        if (!btn || !btn->function) continue;

        if (x >= btn->x && x <= (btn->x + btn->width) &&
            y >= btn->y && y <= (btn->y + btn->height))
        {
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
            printf("[DEBUG_GUI] is_mouse_on_window_button: hit button:%d label:%s\n",
                   i, btn->label ? btn->label : "NULL");
#endif
            return btn;
        }
    }

    return NULL;
}