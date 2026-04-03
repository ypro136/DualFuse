#include <GUI.h>

#include <cstdint>
#include <liballoc.h>
#include <gui_primitives.h>
#include <framebufferutil.h>
#include <psf.h>
#include <timer.h>
#include <console.h>
#include <bootloader.h>
#include <mouse.h>
#include <GUI_input.h>
#include <panel.h>
#include <file_explorer.h>
#include <calculator.h>
#include <apic.h>

#include <png_loader.h>
#include <ramdisk.h>



//   Globals                      
XPTaskbar*     taskbar                           = NULL;
XPDesktopIcon* desktop_icons[MAX_DESKTOP_ICONS]  = {0};


// Console callbacks
// These live here because they reference Console, which is a GUI.cpp concern.
// They are passed as function pointers to create_xp_window() and are
// therefore not exported in any header.
 

static void Console_draw_frame_wrapper(void* ctx)
{
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] Console_draw_frame_wrapper: ctx:%p\n", ctx);
#endif
    static_cast<Console*>(ctx)->draw_frame();
}

static void Console_on_move(void* ctx, int x, int y)
{
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] Console_on_move: x:%d y:%d\n", x, y);
#endif
    static_cast<Console*>(ctx)->set_window_position((int32_t)x, (int32_t)y);
}

static void Console_set_active(void* ctx)
{
    active_console = static_cast<Console*>(ctx);
}

//   Desktop-icon click handlers  

void on_console_icon_click()
{
    XPWindowCallbacks* cb = (XPWindowCallbacks*)malloc(sizeof(XPWindowCallbacks));
    cb->draw_frame = Console_draw_frame_wrapper;
    cb->on_move    = Console_on_move;
    cb->set_active = Console_set_active;
    cb->context    = NULL;   // filled in after console creation below

    XPWindow* window    = create_xp_window(100, 100, screen_width - 200, screen_height - 200, "Kernel Console", cb);
    Console*  console   = create_console(window);
    window->context     = (void*)console;
    window->window_type = WINDOW_TYPE_CONSOLE;

    // Fire set_active now that context is valid
    set_active_xp_window(window);
}

//   Taskbar                      
static bool g_start_menu_open = false;
static void on_start_click(void* /*ctx*/)
{
    g_start_menu_open = !g_start_menu_open;
}
 
static int start_menu_x()  { return 2; }
static int start_menu_y()  { return TASKBAR_Y - (START_MENU_ITEMS * START_MENU_ITEM_HEIGHT) - START_MENU_PADDING * 2; }
static int start_menu_h()  { return START_MENU_ITEMS * START_MENU_ITEM_HEIGHT + START_MENU_PADDING * 2; }

static StartMenuItem  g_start_items[START_MENU_ITEMS] = {
    { "Console", 0x000080, on_console_icon_click       },
    { "Files",   0xFFCC00, on_file_explorer_icon_click },
    { "Calc",    0x007F00, on_calculator_icon_click    },
};

static int start_menu_render_y()
{
    return start_menu_y() - TASKBAR_HEIGHT;
}

static int start_menu_render_height()
{
    return start_menu_h() + TASKBAR_HEIGHT;
}

static int start_menu_hovered_item_index = -1;
void draw_start_menu()
{
    if (!g_start_menu_open) return;
 
    int start_menu_position_x = start_menu_x();
    int start_menu_position_y = start_menu_render_y();
    int start_menu_width      = START_MENU_WIDTH;
    int start_menu_height     = start_menu_render_height();
 
    fill_rectangle(start_menu_position_x, start_menu_position_y,
                   start_menu_width, start_menu_height, XP_BUTTON_FACE);

    draw_beveled_border_thick(start_menu_position_x, start_menu_position_y,
                              start_menu_width, start_menu_height,
                              XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE,
                              XP_BUTTON_SHADOW, true);
 
    draw_gradient(start_menu_position_x + 2, start_menu_position_y + 2,
                  start_menu_width - 4, 24,
                  0x0A246A, 0x3A6EA5, false);

    draw_text("DualFuse",
              start_menu_position_x + 8,
              start_menu_position_y + 6,
              0xFFFFFF, 0x0A246A);
 
    for (int item_index = 0; item_index < START_MENU_ITEMS; item_index++)
    {
        int item_position_y =
            start_menu_position_y + 28 + START_MENU_PADDING +
            item_index * START_MENU_ITEM_HEIGHT;

        bool item_is_hovered =
            (item_index == start_menu_hovered_item_index);

        if (item_is_hovered)
        {
            fill_rectangle(start_menu_position_x + 2,
                           item_position_y,
                           start_menu_width - 4,
                           START_MENU_ITEM_HEIGHT,
                           0x316AC5);
        }

        fill_rectangle(start_menu_position_x + START_MENU_PADDING,
                       item_position_y +
                       (START_MENU_ITEM_HEIGHT - START_MENU_ICON_SIZE) / 2,
                       START_MENU_ICON_SIZE,
                       START_MENU_ICON_SIZE,
                       g_start_items[item_index].icon_color);

        draw_rect_outline(start_menu_position_x + START_MENU_PADDING,
                          item_position_y +
                          (START_MENU_ITEM_HEIGHT - START_MENU_ICON_SIZE) / 2,
                          START_MENU_ICON_SIZE,
                          START_MENU_ICON_SIZE,
                          XP_BUTTON_SHADOW, 1);

        draw_text(g_start_items[item_index].label,
                  start_menu_position_x + START_MENU_PADDING +
                  START_MENU_ICON_SIZE + 8,
                  item_position_y + START_MENU_ITEM_HEIGHT / 2 - 6,
                  item_is_hovered ? 0xFFFFFF : XP_WINDOW_TEXT,
                  item_is_hovered ? 0x316AC5 : XP_BUTTON_FACE);

        if (item_index < START_MENU_ITEMS - 1)
        {
            draw_hline(start_menu_position_x + 4,
                       item_position_y + START_MENU_ITEM_HEIGHT - 1,
                       start_menu_width - 8,
                       XP_BUTTON_SHADOW);
        }
    }
}
 

/* Returns true if the mouse was consumed (inside the menu) */
bool start_menu_handle_mouse(int mouse_position_x,
                             int mouse_position_y,
                             bool left_mouse_clicked)
{
    if (!g_start_menu_open) return false;

    int start_menu_position_x = start_menu_x();
    int start_menu_position_y = start_menu_render_y();
    int start_menu_width      = START_MENU_WIDTH;
    int start_menu_height     = start_menu_render_height();

    start_menu_hovered_item_index = -1;

    if (left_mouse_clicked &&
        (mouse_position_x < start_menu_position_x ||
         mouse_position_x > start_menu_position_x + start_menu_width ||
         mouse_position_y < start_menu_position_y ||
         mouse_position_y > start_menu_position_y + start_menu_height))
    {
        g_start_menu_open = false;
        return false;
    }

    for (int item_index = 0; item_index < START_MENU_ITEMS; item_index++)
    {
        int item_position_y =
            start_menu_position_y + 28 + START_MENU_PADDING +
            item_index * START_MENU_ITEM_HEIGHT;

        bool mouse_is_over_item =
            (mouse_position_x >= start_menu_position_x &&
             mouse_position_x <= start_menu_position_x + start_menu_width &&
             mouse_position_y >= item_position_y &&
             mouse_position_y <= item_position_y + START_MENU_ITEM_HEIGHT);

        if (mouse_is_over_item)
        {
            start_menu_hovered_item_index = item_index;

            if (left_mouse_clicked)
            {
                g_start_menu_open = false;

                if (g_start_items[item_index].on_click)
                    g_start_items[item_index].on_click();

                return true;
            }
        }
    }

    return (mouse_position_x >= start_menu_position_x &&
            mouse_position_x <= start_menu_position_x + start_menu_width &&
            mouse_position_y >= start_menu_position_y &&
            mouse_position_y <= start_menu_position_y + start_menu_height);
}
static void on_taskbar_window_click(void* ctx)
{
#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] on_taskbar_window_click: ctx:%p\n", ctx);
#endif

    XPWindow* win = (XPWindow*)ctx;
    if (!win) return;

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

    tb->start_button = create_xp_button(NULL, 2, TASKBAR_Y + 3,
                                         60, TASKBAR_HEIGHT - 6,
                                         "Start", on_start_click);
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

    // Tear down old per-window buttons
    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (taskbar->window_buttons[i] == NULL) continue;

        // Unregister from button_arr
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

    int btn_x      = 2 + 60 + 10;   // right of the Start button
    int btn_y      = TASKBAR_Y + 4;
    int btn_width  = 150;
    int btn_height = TASKBAR_HEIGHT - 8;

    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (window_arr[i] == NULL) continue;

        // NULL parent → absolute coordinates
        XPButton* btn = create_xp_button(NULL, btn_x, btn_y,
                                          btn_width, btn_height,
                                          window_arr[i]->title,
                                          on_taskbar_window_click);
        btn->window                = window_arr[i];
        taskbar->window_buttons[i] = btn;

        register_xp_button(btn);
        btn_x += btn_width + 5;

#if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] taskbar_sync_windows: slot:%d title:%s btn x:%d y:%d\n",
               i, window_arr[i]->title ? window_arr[i]->title : "NULL", btn->x, btn->y);
#endif
    }
}

void draw_taskbar()
{
#if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] draw_taskbar: TASKBAR_Y:%d SCREEN_WIDTH:%d TASKBAR_HEIGHT:%d\n",
           TASKBAR_Y, SCREEN_WIDTH, TASKBAR_HEIGHT);
#endif

    if (!taskbar) return;

    draw_gradient(0, TASKBAR_Y, SCREEN_WIDTH, TASKBAR_HEIGHT, 0xC0C0C0, 0xA0A0A0, false);
    draw_hline(0, TASKBAR_Y, SCREEN_WIDTH, XP_BUTTON_HIGHLIGHT);

    draw_xp_button(taskbar->start_button);

    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++)
    {
        if (taskbar->window_buttons[i] == NULL) continue;

        taskbar->window_buttons[i]->pressed = (window_arr[i] && window_arr[i]->active);

        draw_xp_button(taskbar->window_buttons[i]);

        int bx = taskbar->window_buttons[i]->x;
        int by = taskbar->window_buttons[i]->y;
        draw_rect_outline(bx + 5, by + 5, 14, 14, XP_BUTTON_SHADOW, 1);
    }

    //  Clock 
    const int clock_x = SCREEN_WIDTH - 100;
    const int clock_y = TASKBAR_Y + 7;

    uint64_t now     = timerBootUnix + (timerTicks / frequency) + (2 * 3600);
    uint64_t hours24 = (now % 86400) / 3600;
    uint64_t minutes =  (now % 3600) / 60;
    uint64_t seconds =   now % 60;

    const char* period = (hours24 >= 12) ? "PM" : "AM";
    uint64_t hours12   = hours24 % 12;
    if (hours12 == 0) hours12 = 12;

    // Zero-pad a value into buf, always producing exactly 2 digits
    auto pad2 = [](uint64_t val, char* buf) -> char* {
        buf[0] = '0' + (val / 10);
        buf[1] = '0' + (val % 10);
        buf[2] = '\0';
        return buf;
    };

    char hbuf[4], mbuf[4], sbuf[4];

    draw_text(pad2(hours12,  hbuf), clock_x,      clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(":",                   clock_x + 16, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(pad2(minutes,  mbuf), clock_x + 24, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(":",                   clock_x + 40, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(pad2(seconds,  sbuf), clock_x + 48, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(period,                clock_x + 64, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
}

//   Desktop background  

void draw_desktop_background()
{
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_desktop_background\n");
    #endif

    if (png_wallpaper_loaded())
    {
        png_blit_wallpaper();
    }
    else
    {
        // Fallback: original XP-blue gradient
        draw_gradient(0, 0, SCREEN_WIDTH, TASKBAR_Y, 0x008DD5, 0x0078D7, true);
    }
}

//   Desktop icons                    

XPDesktopIcon* create_desktop_icon(int x, int y, const char* label,
                                    uint32_t icon_color, void (*on_click)(void))
{
    XPDesktopIcon* icon = (XPDesktopIcon*)malloc(sizeof(XPDesktopIcon));
    if (!icon) return NULL;

    icon->x          = x;
    icon->y          = y;
    icon->size       = 32;
    icon->label      = label;
    icon->icon_color = icon_color;
    icon->pressed    = false;
    icon->hovered    = false;
    icon->on_click   = on_click;

    for (int i = 0; i < MAX_DESKTOP_ICONS; i++)
    {
        if (desktop_icons[i] == NULL)
        {
            desktop_icons[i] = icon;
            break;
        }
    }

#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] create_desktop_icon: x:%d y:%d label:%s ptr:%p\n",
           x, y, label ? label : "NULL", icon);
#endif

    return icon;
}

XPDesktopIcon* get_desktop_icon_at(int x, int y)
{
    for (int i = 0; i < MAX_DESKTOP_ICONS; i++)
    {
        if (desktop_icons[i] && desktop_icon_hit_test(desktop_icons[i], x, y))
            return desktop_icons[i];
    }
    return NULL;
}

bool desktop_icon_hit_test(XPDesktopIcon* icon, int mouse_x, int mouse_y)
{
    if (!icon) return false;
    return (mouse_x >= icon->x && mouse_x <= icon->x + icon->size &&
            mouse_y >= icon->y && mouse_y <= icon->y + icon->size);
}

void draw_desktop_icon(XPDesktopIcon* icon)
{
    if (!icon || !icon->label) return;

    int x    = icon->x;
    int y    = icon->y;
    int size = icon->size;

    uint32_t bg = icon->pressed ? XP_BUTTON_SHADOW
                : icon->hovered ? XP_BUTTON_FACE
                :                 XP_BUTTON_HIGHLIGHT;

    fill_rectangle(x, y, size, size, bg);
    draw_rect_outline(x, y, size, size, XP_BUTTON_SHADOW, 1);
    fill_rectangle(x + 6, y + 6, size - 12, size - 12, icon->icon_color);
    draw_text_centered(icon->label, x - 20, y + size + 5, size + 40,
                       XP_WINDOW_TEXT, 0x008DD5);
}

void draw_all_desktop_icons()
{
    for (int i = 0; i < MAX_DESKTOP_ICONS; i++)
    {
        if (desktop_icons[i])
            draw_desktop_icon(desktop_icons[i]);
    }
}

void desktop_icons_handle_mouse(int mouse_x, int mouse_y, bool clicked)
{
    for (int i = 0; i < MAX_DESKTOP_ICONS; i++)
    {
        XPDesktopIcon* icon = desktop_icons[i];
        if (!icon) continue;

        bool hit     = desktop_icon_hit_test(icon, mouse_x, mouse_y);
        icon->hovered = hit;

        if (hit && clicked)
        {
            icon->pressed = true;
            draw_desktop_icon(icon);
            if (icon->on_click) icon->on_click();
            icon->pressed = false;
        }

        draw_desktop_icon(icon);
    }
}

//   Misc widgets        

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
    draw_beveled_border_thick(x, thumb_y, 16, thumb_height,
                              XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE, XP_BUTTON_SHADOW, true);

    int down_y = y + height - 16;
    fill_rectangle(x, down_y, 16, 16, XP_BUTTON_FACE);
    draw_beveled_border_thick(x, down_y, 16, 16,
                              XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE, XP_BUTTON_SHADOW, true);
}

//   Utility                      

static void reverse_string(char* str, int len)
{
    for (int i = 0; i < len / 2; i++)
    {
        char tmp        = str[i];
        str[i]          = str[len - 1 - i];
        str[len - 1 - i] = tmp;
    }
}

char* u64toa(uint64_t value, char* str, int base)
{
    if (!str || base < 2 || base > 36) return str;

    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    if (value == 0) { str[0] = '0'; str[1] = '\0'; return str; }

    int i = 0;
    while (value > 0) { str[i++] = digits[value % base]; value /= base; }
    str[i] = '\0';
    reverse_string(str, i);
    return str;
}

static void draw_clock_panel(int x, int y, int /*w*/, int /*h*/, void* /*ctx*/)
{
    uint64_t now     = timerBootUnix + (timerTicks / frequency) + (2 * 3600);
    uint64_t hours24 = (now % 86400) / 3600;
    uint64_t minutes =  (now % 3600) / 60;
    uint64_t seconds =   now % 60;

    const char* period = (hours24 >= 12) ? "PM" : "AM";
    uint64_t hours12   = hours24 % 12;
    if (hours12 == 0) hours12 = 12;

    auto pad2 = [](uint64_t val, char* buf) -> char* {
        buf[0] = '0' + (val / 10);
        buf[1] = '0' + (val % 10);
        buf[2] = '\0';
        return buf;
    };

    char hbuf[4], mbuf[4], sbuf[4];
    draw_text(pad2(hours12, hbuf), x,      y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(":",                  x + 16, y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(pad2(minutes, mbuf), x + 24, y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(":",                  x + 40, y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(pad2(seconds, sbuf), x + 48, y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(period,               x + 64, y, XP_WINDOW_TEXT, 0xA0A0A0);
}

//   Desktop lifecycle  

void initialize_xp_desktop()
{
    frame_ready = false;

#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] initialize_xp_desktop: start\n");
#endif

    // // Load wallpaper from Limine module if present
    // const struct limine_file* wallpaper = findModule("wallpaper.png");
    // if (wallpaper) 
    // {
    //     if (png_load_wallpaper((const unsigned char*)wallpaper->address, (unsigned int)wallpaper->size))
    //     {
    //         printf("[GUI::png] module wallpaper loaded (%d bytes)\n", wallpaper->size);
    //     }
    //     else
    //     {
    //         printf("[GUI::png] module wallpaper decode failed trying static wallpaper\n");
    //         if (!png_load_wallpaper(wallpaper_png, wallpaper_png_len))
    //         {
    //             printf("[GUI] static wallpaper load failed, using gradient fallback\n");
    //         }
    //     }
    // } else 
    // {
    //     printf("[GUI::png] no wallpaper module found, using gradient fallback\n");
    // }


    create_taskbar();
    // create_desktop_icon(20,  15, "Console", 0x000080, on_console_icon_click);
    // create_desktop_icon(20,  70, "Files",   0xFFCC00, on_file_explorer_icon_click);
    // create_desktop_icon(20, 125, "Calc",    0x007F00, on_calculator_icon_click);

    if (!console_initialized)
    {
        console = Console(400, 400, 200, 200);
        console_initialize();
        console.clear_screen();
        console.set_bg_color(XP_BACKGROUND);
        console.set_text_color(XP_WINDOW_TEXT);
        console.set_visible(false);

#if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] initialize_xp_desktop: console initialized\n");
#endif
    }

    XPPanel* clock_panel = create_xp_panel(
    SCREEN_WIDTH - 100, TASKBAR_Y + 5,
    80, 14,
    200, 
    36,
    draw_clock_panel,
    nullptr
);
    register_xp_panel(clock_panel);

    on_console_icon_click();

    taskbar_sync_windows();

#if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] initialize_xp_desktop: done\n");
#endif

    frame_ready = true;
    printf("xp desktop initialized.\n");
}

void render_xp_desktop()
{
    frame_ready = false;

    draw_desktop_background();

    draw_all_desktop_icons();

    draw_all_xp_windows_but_active();

    draw_active_xp_window();

    draw_all_xp_panels();

    draw_taskbar();

    draw_start_menu();
 
    draw_cursor(mouse_position_x, mouse_position_y);

    frame_ready = true;
}

//   Debug   

#if defined(DEBUG_GUI)
static int dbg_frame_counter = 0;

void direct_clear_screen_dbg()
{
    struct limine_framebuffer* fb   = bootloader.framebuffer;
    uint32_t*                  addr = (uint32_t*)fb->address;
    size_t pixel_count              = fb->pitch / 4 * fb->height;

    dbg_frame_counter++;
    switch (dbg_frame_counter % 3)
    {
        case 0: for (size_t i = 0; i < pixel_count; i++) addr[i] = 0x0F0F00; break;
        case 1: for (size_t i = 0; i < pixel_count; i++) addr[i] = 0x0F000F; break;
        case 2: for (size_t i = 0; i < pixel_count; i++) addr[i] = 0x000F0F; break;
    }
}
#endif