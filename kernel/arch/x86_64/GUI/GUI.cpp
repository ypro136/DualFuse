#include <GUI.h>

#include <cstdint>
#include <cstring>

#include <liballoc.h>
#include <gui_primitives.h>
#include <framebufferutil.h>
#include <psf.h>
#include <timer.h>
#include <console.h>
#include <bootloader.h>
#include <mouse.h>
#include <GUI_input.h>


// Taskbar constants
#define TASKBAR_HEIGHT 28
#define TASKBAR_Y (SCREEN_HEIGHT - TASKBAR_HEIGHT)

// Window constants
#define TITLE_BAR_HEIGHT 24
#define WINDOW_BORDER_WIDTH 4

XPWindow* window_arr[MAX_NUM_OF_WINDOWS] = {0};
XPButton* button_arr[MAX_NUM_OF_BUTTONS] = {0};

XPWindow* active_xp_window = {0};

XPWindow* create_xp_window(int x, int y, int width, int height, const char* title)
{
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_window: x:%d y:%d width:%d height:%d title:%s\n", x, y, width, height, title ? title : "NULL");
    #endif

    XPWindow* window = malloc(sizeof(XPWindow));
    if (!window)
    {
        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] create_xp_window: malloc FAILED\n");
        #endif
        return NULL;
    }

    for (int i = 0; i < MAX_NUM_OF_BUTTONS_FOR_WINDOWS; i++)
    {
        window->buttons[i] = NULL;
    }

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_window: malloc OK ptr:%p\n", window);
    #endif

    window->x         = x;
    window->y         = y;
    window->width     = width;
    window->height    = height;
    window->title     = title;
    window->active    = false;
    window->minimized = false;
    window->bg_color  = XP_BACKGROUND;

    window->on_move    = Console_on_move;
    window->draw_frame = Console_draw_frame_wrapper;
    window->context    = &console;

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_window: fields set, creating buttons\n");
    #endif

    int btn_y    = 4;
    int btn_size = 16;

    window->buttons[0] = create_xp_button(window, window->x + window->width - (btn_size + 4) * 1, btn_y, btn_size, btn_size, "X", close_xp_window);
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_window: close button ptr:%p\n", window->buttons[0]);
    #endif

    window->buttons[1] = create_xp_button(window, window->x + window->width - (btn_size + 4) * 2, btn_y, btn_size, btn_size, "[", maximize_xp_window);
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_window: maximize button ptr:%p\n", window->buttons[1]);
    #endif

    window->buttons[2] = create_xp_button(window, window->x + window->width - (btn_size + 4) * 3, btn_y, btn_size, btn_size, "_", minimize_xp_window);
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_window: minimize button ptr:%p\n", window->buttons[2]);
    #endif

    // Find a free slot in window_arr
    bool slot_found = false;
    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i] == NULL)
        {
            window_arr[i] = window;
            slot_found = true;
            #if defined(DEBUG_GUI)
            printf("[DEBUG_GUI] create_xp_window: registered in window_arr slot:%d\n", i);
            #endif
            break;
        }
    }

    #if defined(DEBUG_GUI)
    if (!slot_found)
        printf("[DEBUG_GUI] create_xp_window: window_arr is FULL, window not registered!\n");
    #endif

    set_active_xp_window(window);
    taskbar_sync_windows();

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_window: done\n");
    #endif

    return window;
}

// Check if mouse is on a specific window's buttons
XPButton* is_mouse_on_window_button(XPWindow* win, int x, int y)
{
    if (win == NULL)
    {
        #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] is_mouse_on_window_button: win is NULL\n");
        #endif
        return NULL;
    }

    for (int i = 0; i < MAX_NUM_OF_BUTTONS_FOR_WINDOWS; i++)
    {
        XPButton* btn = win->buttons[i];

        if (btn == NULL || btn->function == NULL)
            continue;

        if (x >= btn->x && x <= (btn->x + btn->width) &&
            y >= btn->y && y <= (btn->y + btn->height))
        {
            #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
            printf("[DEBUG_GUI] is_mouse_on_window_button: hit button:%d label:%s\n", i, btn->label ? btn->label : "NULL");
            #endif
            return btn;
        }
    }

    return NULL;
}

// Check standalone buttons only
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
            printf("[DEBUG_GUI] get_button_at: hit button:%d label:%s\n", i, btn->label ? btn->label : "NULL");
            #endif
            return btn;
        }
    }

    return NULL;
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

    // Remove from window array
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

    // Free buttons
    for (int i = 0; i < MAX_NUM_OF_BUTTONS_FOR_WINDOWS; i++)
    {
        if (window->buttons[i] != NULL)
        {
            #if defined(DEBUG_GUI)
            printf("[DEBUG_GUI] close_xp_window: freeing button:%d\n", i);
            #endif
            free(window->buttons[i]);
            window->buttons[i] = NULL;
        }
    }

    free(window);

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] close_xp_window: window freed, finding next active\n");
    #endif

    // Set active to the next available window
    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i] != NULL)
        {
            #if defined(DEBUG_GUI)
            printf("[DEBUG_GUI] close_xp_window: new active window slot:%d\n", i);
            #endif
            set_active_xp_window(window_arr[i]);
            taskbar_sync_windows();
            return;
        }
    }

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] close_xp_window: no windows left, active_xp_window = NULL\n");
    #endif
    active_xp_window = NULL;
}

void maximize_xp_window(void* ctx)
{
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] maximize_xp_window: ctx:%p\n", ctx);
    #endif

    XPWindow* window = (XPWindow*)ctx;
    if (!window)
    {
        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] maximize_xp_window: window is NULL, aborting\n");
        #endif
        return;
    }

    window->x      = 0;
    window->y      = 0;
    window->width  = SCREEN_WIDTH;
    window->height = SCREEN_HEIGHT - TASKBAR_HEIGHT;

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] maximize_xp_window: new size x:%d y:%d w:%d h:%d\n", window->x, window->y, window->width, window->height);
    #endif

    // Reposition buttons after resize
    int btn_y    = window->y + 4;
    int btn_size = 16;
    window->buttons[0]->x = window->x + window->width - (btn_size + 4) * 1;
    window->buttons[1]->x = window->x + window->width - (btn_size + 4) * 2;
    window->buttons[2]->x = window->x + window->width - (btn_size + 4) * 3;
    window->buttons[0]->y = btn_y;
    window->buttons[1]->y = btn_y;
    window->buttons[2]->y = btn_y;

    if (window->context != NULL && window->on_move != NULL)
    {
        window->on_move(window->context, window->x, window->y + TITLE_BAR_HEIGHT);
    }

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] maximize_xp_window: buttons repositioned\n");
    #endif
}

void minimize_xp_window(void* ctx)
{
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] minimize_xp_window: ctx:%p\n", ctx);
    #endif

    XPWindow* window = (XPWindow*)ctx;
    if (!window)
    {
        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] minimize_xp_window: window is NULL, aborting\n");
        #endif
        return;
    }

    window->minimized = !window->minimized;

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] minimize_xp_window: minimized is now:%d\n", window->minimized);
    #endif
}

void set_active_xp_window(XPWindow* win)
{
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] set_active_xp_window: win:%p\n", win);
    #endif

    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i] == NULL)
            break;
        window_arr[i]->active = false;
    }

    active_xp_window = win;
    win->active = true;

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] set_active_xp_window: done, title:%s\n", win->title ? win->title : "NULL");
    #endif
}

static void Console_on_move(void* ctx, int x, int y)
{
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] Console_on_move: x:%d y:%d\n", x, y);
    #endif
    Console* console = static_cast<Console*>(ctx);
    console->set_window_position((int32_t)x, (int32_t)y);
}

void draw_window_title_bar(XPWindow* win)
{
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_window_title_bar: x:%d y:%d w:%d\n", win->x, win->y, win->width);
    #endif
    draw_gradient(win->x, win->y, win->width, TITLE_BAR_HEIGHT, 0x000080, 0x1084D7, true);
    draw_line(win->x + 1, win->y + 1, win->x + win->width - 2, win->y + 1, 0x2E5C8A);
    draw_text(win->title, win->x + 8, win->y + 6, XP_WINDOW_TEXT, 0x000080);
}

void draw_window_button(int x, int y, int size, const char* label, bool active)
{
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_window_button: x:%d y:%d size:%d label:%s active:%d\n", x, y, size, label ? label : "NULL", active);
    #endif
    uint32_t button_color = XP_BUTTON_FACE;
    uint32_t highlight    = XP_BUTTON_HIGHLIGHT;
    uint32_t shadow       = XP_BUTTON_SHADOW;

    fill_rectangle(x, y, size, size, button_color);

    if (active) {
        draw_hline(x, y, size, highlight);
        draw_vline(x, y, size, highlight);
        draw_hline(x, y + size - 1, size, shadow);
        draw_vline(x + size - 1, y, size, shadow);
    } else {
        draw_hline(x, y, size, shadow);
        draw_vline(x, y, size, shadow);
        draw_hline(x, y + size - 1, size, highlight);
        draw_vline(x + size - 1, y, size, highlight);
    }

    if (label[0] == 'X') {
        draw_line(x + 4, y + 4, x + size - 5, y + size - 5, 0x000000);
        draw_line(x + size - 5, y + 4, x + 4, y + size - 5, 0x000000);
    } else if (label[0] == '_') {
        draw_hline(x + 4, y + size / 2, size - 8, 0x000000);
    } else if (label[0] == '[') {
        draw_rect_outline(x + 4, y + 4, size - 8, size - 8, 0x000000, 1);
    }
}

void draw_xp_window(XPWindow* win)
{
    if (!win || win->minimized)
    {
        #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_xp_window: skipping win:%p minimized:%d\n", win, win ? win->minimized : -1);
        #endif
        return;
    }

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_xp_window: x:%d y:%d w:%d h:%d title:%s\n", win->x, win->y, win->width, win->height, win->title ? win->title : "NULL");
    #endif

    fill_rectangle(win->x, win->y, win->width, win->height, XP_BACKGROUND);

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_xp_window: fill_rectangle done\n");
    #endif

    draw_window_title_bar(win);

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_xp_window: title bar done\n");
    #endif

    draw_beveled_border_thick(win->x, win->y, win->width, win->height,
                              XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE, XP_BUTTON_SHADOW, true);

    int client_x      = win->x + WINDOW_BORDER_WIDTH;
    int client_y      = win->y + TITLE_BAR_HEIGHT + WINDOW_BORDER_WIDTH;
    int client_width  = win->width  - 2 * WINDOW_BORDER_WIDTH;
    int client_height = win->height - TITLE_BAR_HEIGHT - 2 * WINDOW_BORDER_WIDTH;

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_xp_window: client area x:%d y:%d w:%d h:%d\n", client_x, client_y, client_width, client_height);
    #endif

    fill_rectangle(client_x, client_y, client_width, client_height, win->bg_color);

    int btn_y    = win->y + 4;
    int btn_size = 16;

    draw_window_button(win->x + win->width - btn_size - 4,          btn_y, btn_size, "X", true);
    draw_window_button(win->x + win->width - (btn_size + 4) * 2,    btn_y, btn_size, "[", true);
    draw_window_button(win->x + win->width - (btn_size + 4) * 3,    btn_y, btn_size, "_", true);

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_xp_window: buttons drawn\n");
    #endif

    if (win->context && win->draw_frame)
    {
        #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_xp_window: calling draw_frame context:%p\n", win->context);
        #endif
        win->draw_frame(win->context);
        #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_xp_window: draw_frame done\n");
        #endif
    }
    else
    {
        #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_xp_window: no draw_frame context:%p draw_frame:%p\n", win->context, win->draw_frame);
        #endif
    }
}

XPWindow* get_window_at(int x, int y)
{
    XPWindow* result = NULL;

    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i] == NULL)
            continue;

        XPWindow* win = window_arr[i];

        if (x >= win->x && x <= (win->x + win->width) &&
            y >= win->y && y <= (win->y + win->height))
        {
            if (win->active)
            {
                #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
                printf("[DEBUG_GUI] get_window_at: hit active window slot:%d\n", i);
                #endif
                return win;
            }
            result = win;
        }
    }

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] get_window_at: result:%p\n", result);
    #endif

    return result;
}

bool is_mouse_on_window_title_bar(XPWindow* win, int x, int y)
{
    if (!win) return false;
    bool hit = (x >= win->x && x <= (win->x + win->width) &&
                y >= win->y && y <= (win->y + win->height) &&
                y <= (win->y + TITLE_BAR_HEIGHT));
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] is_mouse_on_window_title_bar: hit:%d\n", hit);
    #endif
    return hit;
}

void move_xp_window(XPWindow* win, int x, int y)
{
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] move_xp_window: x:%d y:%d\n", x, y);
    #endif

    win->x = x;
    win->y = y;

    if (win->context != NULL && win->on_move != NULL)
    {
        win->on_move(win->context, x, y + TITLE_BAR_HEIGHT);
    }

    // Reposition window buttons
    int btn_y    = win->y + 4;
    int btn_size = 16;
    if (win->buttons[0]) { win->buttons[0]->x = win->x + win->width - (btn_size + 4) * 1; win->buttons[0]->y = btn_y; }
    if (win->buttons[1]) { win->buttons[1]->x = win->x + win->width - (btn_size + 4) * 2; win->buttons[1]->y = btn_y; }
    if (win->buttons[2]) { win->buttons[2]->x = win->x + win->width - (btn_size + 4) * 3; win->buttons[2]->y = btn_y; }

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] move_xp_window: done, buttons repositioned\n");
    #endif
}

XPButton* create_xp_button(XPWindow* win, int x, int y, int width, int height, const char* label, void (*func)(void*))
{
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_xp_button: x:%d y:%d w:%d h:%d label:%s\n", x, y, width, height, label ? label : "NULL");
    #endif

    XPButton* button = malloc(sizeof(XPButton));
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
    printf("[DEBUG_GUI] create_xp_button: done ptr:%p final x:%d y:%d\n", button, button->x, button->y);
    #endif

    return button;
}

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

    if (!button->pressed) {
        draw_beveled_border_thick(button->x, button->y, button->width, button->height,
                                  highlight, button_color, shadow, true);
    } else {
        draw_beveled_border_thick(button->x, button->y, button->width, button->height,
                                  shadow, button_color, highlight, false);
    }

    if (button->label != NULL)
        draw_text_centered(button->label,
                           button->x, button->y + (button->height - 8) / 2,
                           button->width, XP_WINDOW_TEXT, XP_BACKGROUND);
}

static void reverse_string(char* str, int len) {
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = temp;
    }
}

char* u64toa(uint64_t value, char* str, int base) {
    if (!str || base < 2 || base > 36)
        return str;

    char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }

    int i = 0;
    while (value > 0) {
        str[i++] = digits[value % base];
        value /= base;
    }

    str[i] = '\0';
    reverse_string(str, i);
    return str;
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
        printf("[DEBUG_GUI] draw_all_xp_buttons: drawing button:%d label:%s\n", i, button_arr[i]->label ? button_arr[i]->label : "NULL");
        #endif
    }

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_all_xp_buttons: buttons_drawn:%d\n", buttons_drawn);
    #endif

    return buttons_drawn;
}

XPButton* register_xp_button(XPButton* btn)
{
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] register_xp_button: ptr:%p label:%s\n", btn, btn && btn->label ? btn->label : "NULL");
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

XPTaskbar* taskbar = NULL;

void on_start_click(void* ctx)
{
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] on_start_click\n");
    #endif
    (void)ctx;
    // TODO: open start menu
}

void on_taskbar_window_click(void* ctx)
{
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] on_taskbar_window_click: ctx:%p\n", ctx);
    #endif

    XPWindow* win = (XPWindow*)ctx;
    if (!win)
    {
        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] on_taskbar_window_click: win is NULL\n");
        #endif
        return;
    }

    set_active_xp_window(win);
    if (win->minimized)
    {
        win->minimized = false;
        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] on_taskbar_window_click: unminimized window\n");
        #endif
    }
}

XPTaskbar* create_taskbar()
{
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_taskbar\n");
    #endif

    XPTaskbar* tb = (XPTaskbar*)malloc(sizeof(XPTaskbar));
    if (!tb)
    {
        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] create_taskbar: malloc FAILED\n");
        #endif
        return NULL;
    }

    tb->y        = TASKBAR_Y;
    tb->height   = TASKBAR_HEIGHT;
    tb->bg_color = 0xC0C0C0;

    tb->start_button = create_xp_button(NULL, 2, TASKBAR_Y + 2, 60, TASKBAR_HEIGHT - 4, "Start", on_start_click);

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_taskbar: start_button ptr:%p\n", tb->start_button);
    #endif

    register_xp_button(tb->start_button);

    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
        tb->window_buttons[i] = NULL;

    taskbar = tb;

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_taskbar: done ptr:%p\n", taskbar);
    #endif

    return tb;
}

void taskbar_sync_windows()
{
    if (!taskbar) return;

    // Unregister and free old taskbar window buttons
    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (taskbar->window_buttons[i] == NULL)
            continue;

        for (int j = 0; j < MAX_NUM_OF_BUTTONS; j++)
        {
            if (button_arr[j] == taskbar->window_buttons[i])
            {
                button_arr[j] = NULL;
                break;
            }
        }

        free(taskbar->window_buttons[i]);
        taskbar->window_buttons[i] = NULL;
    }

    int btn_x      = 2 + 60 + 10;   // after start button
    int btn_y      = TASKBAR_Y + 4;
    int btn_width  = 150;
    int btn_height = TASKBAR_HEIGHT - 8;

    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i] == NULL)
            continue;

        // NULL parent = absolute coordinates, no offset applied
        XPButton* btn = create_xp_button(NULL, btn_x, btn_y, btn_width, btn_height,
                                          window_arr[i]->title, on_taskbar_window_click);
        btn->window                = window_arr[i];  // still link to window for the click handler
        taskbar->window_buttons[i] = btn;

        register_xp_button(btn);

        btn_x += btn_width + 5;

        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] taskbar_sync_windows: registered button for window slot:%d title:%s at x:%d y:%d\n",
               i, window_arr[i]->title ? window_arr[i]->title : "NULL", btn->x, btn->y);
        #endif
    }
}

void draw_taskbar()
{
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_taskbar\n");
    printf("[DEBUG_GUI] draw_taskbar: TASKBAR_Y:%d SCREEN_WIDTH:%d TASKBAR_HEIGHT:%d\n", TASKBAR_Y, SCREEN_WIDTH, TASKBAR_HEIGHT);
    #endif

    if (!taskbar)
    {
        #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_taskbar: taskbar is NULL, skipping\n");
        #endif
        return;
    }

    draw_gradient(0, TASKBAR_Y, SCREEN_WIDTH, TASKBAR_HEIGHT, 0xC0C0C0, 0xA0A0A0, false);
    draw_hline(0, TASKBAR_Y, SCREEN_WIDTH, XP_BUTTON_HIGHLIGHT);

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_taskbar: background drawn, drawing start button\n");
    #endif

    draw_xp_button(taskbar->start_button);

    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (taskbar->window_buttons[i] == NULL)
            continue;

        if (window_arr[i] && window_arr[i]->active)
            taskbar->window_buttons[i]->pressed = true;
        else
            taskbar->window_buttons[i]->pressed = false;

        draw_xp_button(taskbar->window_buttons[i]);

        int bx = taskbar->window_buttons[i]->x;
        int by = taskbar->window_buttons[i]->y;
        draw_rect_outline(bx + 5, by + 5, 14, 14, XP_BUTTON_SHADOW, 1);

        #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_taskbar: drew window button slot:%d\n", i);
        #endif
    }

    // Clock
    int clock_x = SCREEN_WIDTH - 80;
    int clock_y = TASKBAR_Y + 5;

    uint64_t total_seconds = timerTicks / 60;
    uint64_t hours         = total_seconds / 3600;
    uint64_t minutes       = (total_seconds % 3600) / 60;
    uint64_t seconds       = total_seconds % 60;

    char string[64];
    draw_text(u64toa(hours,   string, 10), clock_x,      clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(":",                         clock_x + 14, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(u64toa(minutes, string, 10), clock_x + 17, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(":",                         clock_x + 31, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(u64toa(seconds, string, 10), clock_x + 34, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_taskbar: clock drawn h:%llu m:%llu s:%llu\n", hours, minutes, seconds);
    #endif
}

void draw_desktop_icon(int x, int y, const char* label, uint32_t icon_color)
{
    int icon_size = 32;

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] draw_desktop_icon: x:%d y:%d label:%s\n", x, y, label ? label : "NULL");
    #endif

    fill_rectangle(x, y, icon_size, icon_size, XP_BUTTON_HIGHLIGHT);
    draw_rect_outline(x, y, icon_size, icon_size, XP_BUTTON_SHADOW, 1);
    fill_rectangle(x + 6, y + 6, icon_size - 12, icon_size - 12, icon_color);
    draw_text_centered(label, x - 20, y + icon_size + 5, icon_size + 40, XP_WINDOW_TEXT, 0x008DD5);

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] draw_desktop_icon: done\n");
    #endif
}

void draw_desktop_background()
{
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_desktop_background\n");
    #endif

    draw_gradient(0, 0, SCREEN_WIDTH, TASKBAR_Y, 0x008DD5, 0x0078D7, true);

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_desktop_background: done\n");
    #endif
}

void draw_scrollbar(int x, int y, int height, int scroll_pos, int max_scroll)
{
    int track_height = height - 32;

    fill_rectangle(x, y, 16, 16, XP_BUTTON_FACE);
    draw_beveled_border_thick(x, y, 16, 16, XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE, XP_BUTTON_SHADOW, true);
    draw_line(x + 6, y + 9, x + 10, y + 5, 0x000000);
    draw_line(x + 10, y + 5, x + 10, y + 9, 0x000000);

    fill_rectangle(x, y + 16, 16, track_height, 0xC0C0C0);

    int thumb_height = (track_height * track_height) / max_scroll;
    if (thumb_height < 8) thumb_height = 8;

    int thumb_y = y + 16 + (scroll_pos * track_height) / max_scroll;
    fill_rectangle(x, thumb_y, 16, thumb_height, XP_BUTTON_FACE);
    draw_beveled_border_thick(x, thumb_y, 16, thumb_height, XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE, XP_BUTTON_SHADOW, true);

    int down_y = y + height - 16;
    fill_rectangle(x, down_y, 16, 16, XP_BUTTON_FACE);
    draw_beveled_border_thick(x, down_y, 16, 16, XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE, XP_BUTTON_SHADOW, true);
}

static void Console_draw_frame_wrapper(void* context)
{
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] Console_draw_frame_wrapper: context:%p\n", context);
    #endif
    Console* console = static_cast<Console*>(context);
    console->draw_frame();
}

int draw_all_xp_windows_but_active()
{
    int windows_drawn = 0;

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_all_xp_windows_but_active\n");
    #endif

    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i] == NULL)
            break;

        if (window_arr[i]->active == false)
        {
            draw_xp_window(window_arr[i]);
            windows_drawn++;
        }
        else
        {
            #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
            printf("[DEBUG_GUI] draw_all_xp_windows_but_active: skipping active window slot:%d\n", i);
            #endif
        }
    }

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_all_xp_windows_but_active: windows_drawn:%d\n", windows_drawn);
    #endif

    return windows_drawn;
}

void draw_active_xp_window()
{
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_active_xp_window: ptr:%p\n", active_xp_window);
    #endif

    if (active_xp_window)
    {
        draw_xp_window(active_xp_window);
        #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_active_xp_window: done\n");
        #endif
    }
}

#if defined(DEBUG_GUI)
int l = 0;
void direct_clear_screen_dbg()
{
    struct limine_framebuffer *fb = bootloader.framebuffer;
    uint32_t *addr = (uint32_t*)fb->address;
    size_t pixel_count = fb->pitch / 4 * fb->height;

    l++;
    int mod = l % 3;
    switch (mod)
    {
        case 0: for (size_t i = 0; i < pixel_count; i++) addr[i] = 0x0F0F00; break;
        case 1: for (size_t i = 0; i < pixel_count; i++) addr[i] = 0x0F000F; break;
        case 2: for (size_t i = 0; i < pixel_count; i++) addr[i] = 0x00F0F;  break;
        default: break;
    }
}
#endif

void initialize_xp_desktop()
{
    frame_ready = false;

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] initialize_xp_desktop: start\n");
    #endif

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] initialize_xp_desktop: creating taskbar first\n");
    #endif

    create_taskbar();

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] initialize_xp_desktop: creating window\n");
    #endif

    XPWindow* window = create_xp_window(350, 175, 800, 500, "Kernel Console");

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] initialize_xp_desktop: window ptr:%p\n", window);
    #endif

    if (!console_initialized)
    {
        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] initialize_xp_desktop: initializing console\n");
        #endif

        console = Console(window->width, window->height - TITLE_BAR_HEIGHT,
                          window->x, window->y + TITLE_BAR_HEIGHT);
        console_initialize();
        console.clear_screen();
        console.set_bg_color(XP_BACKGROUND);
        console.set_text_color(XP_WINDOW_TEXT);

        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] initialize_xp_desktop: console initialized\n");
        #endif
    }
    else
    {
        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] initialize_xp_desktop: console already initialized, skipping\n");
        #endif
    }

    taskbar_sync_windows();

    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] initialize_xp_desktop: done\n");
    #endif

    frame_ready = true;
}

void render_xp_desktop()
{
    frame_ready = false;

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] render_xp_desktop: start\n");
    #endif

    draw_desktop_background();
    draw_all_xp_windows_but_active();
    draw_active_xp_window();
    draw_taskbar();
    draw_cursor(mouse_position_x, mouse_position_y);

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] render_xp_desktop: done\n");
    #endif

    frame_ready = true;
}