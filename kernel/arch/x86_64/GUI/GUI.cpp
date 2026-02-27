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

#define MAX_NUM_OF_WINDOWS 64

XPWindow* window_arr[MAX_NUM_OF_WINDOWS] = {0};

XPWindow* active_xp_window = {0};

XPWindow* create_xp_window(int x, int y, int width, int height, const char* title)
{
    XPWindow* window = malloc(sizeof(XPWindow));
    if (!window) return NULL;

    window->x        = x;
    window->y        = y;
    window->width    = width;
    window->height   = height;
    window->title    = title;
    window->active   = false;
    window->minimized = false;
    window->bg_color = XP_BACKGROUND;

    window->on_move  = Console_on_move;
    window->draw_frame = Console_draw_frame_wrapper;
    window->context    = &console;

    // Find a free slot in window_arr
    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++) {
        if (window_arr[i] == NULL) {
            window_arr[i] = window;
            break;
        }
    }

    set_active_xp_window(window);
    return window;
}

void set_active_xp_window(XPWindow* win)
{
    // Find a free slot in window_arr
    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++) 
    {
        if (window_arr[i] == NULL) 
        {
            break;
        }
        window_arr[i]->active = false;
    }

    active_xp_window = win;
    win->active = true;
}

static void Console_on_move(void* ctx, int x, int y)
{
    Console* console = static_cast<Console*>(ctx);
    console->set_window_position(x, y);
}

void draw_window_title_bar(XPWindow* win) {
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_window_title_bar\n");
    #endif
    draw_gradient(win->x, win->y, win->width, TITLE_BAR_HEIGHT, 0x000080, 0x1084D7, true);
    draw_line(win->x + 1, win->y + 1, win->x + win->width - 2, win->y + 1, 0x2E5C8A);
    draw_text(win->title, win->x + 8, win->y + 6, XP_WINDOW_TEXT, 0x000080);
}

void draw_window_button(int x, int y, int size, const char* label, bool active) {
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_window_button\n");
    #endif
    uint32_t button_color = XP_BUTTON_FACE;
    uint32_t highlight = XP_BUTTON_HIGHLIGHT;
    uint32_t shadow = XP_BUTTON_SHADOW;
    
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
    
    // X symbol for close button
    if (label[0] == 'X') {
        draw_line(x + 4, y + 4, x + size - 5, y + size - 5, 0x000000);
        draw_line(x + size - 5, y + 4, x + 4, y + size - 5, 0x000000);
    }
    // _ for minimize
    else if (label[0] == '_') {
        draw_hline(x + 4, y + size / 2, size - 8, 0x000000);
    }
    // [] for maximize
    else if (label[0] == '[') {
        draw_rect_outline(x + 4, y + 4, size - 8, size - 8, 0x000000, 1);
    }
}

  

void draw_xp_window(XPWindow* win) 
{
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_xp_window\n");
        printf("[DEBUG_GUI] win->x:%d, win->y:%d, win->width:%d, win->height:%d \n",win->x, win->y, win->width, win->height);
    #endif
    fill_rectangle(win->x, win->y, win->width, win->height, XP_BACKGROUND);
    
    draw_window_title_bar(win);
    
    draw_beveled_border_thick(win->x, win->y, win->width, win->height,
                              XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE, XP_BUTTON_SHADOW, true);
    
    int client_x = win->x + WINDOW_BORDER_WIDTH;
    int client_y = win->y + TITLE_BAR_HEIGHT + WINDOW_BORDER_WIDTH;
    int client_width = win->width - 2 * WINDOW_BORDER_WIDTH;
    int client_height = win->height - TITLE_BAR_HEIGHT - 2 * WINDOW_BORDER_WIDTH;
    
    fill_rectangle(client_x, client_y, client_width, client_height, win->bg_color);
    
    int btn_y = win->y + 4;
    int btn_size = 16;
    
    draw_window_button(win->x + win->width - btn_size - 4, btn_y, btn_size, "X", true);
    draw_window_button(win->x + win->width - (btn_size + 4) * 2, btn_y, btn_size, "[", true);
    draw_window_button(win->x + win->width - (btn_size + 4) * 3, btn_y, btn_size, "_", true);

    if (win->context && win->draw_frame)
    {
        #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] Drawing frame of context in window\n");
        #endif
        win->draw_frame(win->context);
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
            // If this window is active, return it immediately
            if (win->active)
                return win;

            // Otherwise keep it as a candidate and continue
            result = win;
        }
    }

    return result; // returns last matched window, or NULL if none
}

bool is_mouse_on_window_title_bar(XPWindow* win, int x, int y)
{
    return (x >= win->x && x <= (win->x + win->width) &&
            y >= win->y && y <= (win->y + win->height) &&
            y <= (win->y + TITLE_BAR_HEIGHT));
}

void move_xp_window(XPWindow* win, int x, int y)
{
    win->x = x;
    win->y = y;
    if (win->context != NULL && win->on_move != NULL)
    {
        win->on_move(win->context, x, y + TITLE_BAR_HEIGHT);
    }
}


void draw_xp_button(int x, int y, int width, int height, const char* label, bool pressed) 
{
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_xp_button\n");
    #endif

    uint32_t button_color = XP_BUTTON_FACE;
    uint32_t highlight = XP_BUTTON_HIGHLIGHT;
    uint32_t shadow = XP_BUTTON_SHADOW;
    
    // Button background
    fill_rectangle(x, y, width, height, button_color);
    
    // 3D beveled border
    if (!pressed) {
        // Raised button
        draw_beveled_border_thick(x, y, width, height, 
                                  highlight, button_color, shadow, true);
    } else {
        // Pressed button
        draw_beveled_border_thick(x, y, width, height, 
                                  shadow, button_color, highlight, false);
    }
    
    // Button text (centered)
    draw_text_centered(label, x, y + (height - 8) / 2, width, XP_WINDOW_TEXT, XP_BACKGROUND);
}

 static void reverse_string(char* str, int len) {
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = temp;
    }
}

// Convert unsigned 64-bit integer to string
char* u64toa(uint64_t value, char* str, int base) {
    if (!str || base < 2 || base > 36) {
        return str;
    }
    
    char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    
    // Handle zero
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

void draw_taskbar() 
{   
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] draw_taskbar\n");
    #endif
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] TASKBAR_Y:%d SCREEN_WIDTH:%d TASKBAR_HEIGHT:%d\n", TASKBAR_Y, SCREEN_WIDTH, TASKBAR_HEIGHT);
    #endif
    draw_gradient(0, TASKBAR_Y, SCREEN_WIDTH, TASKBAR_HEIGHT, 0xC0C0C0, 0xA0A0A0, false);
    
    draw_hline(0, TASKBAR_Y, SCREEN_WIDTH, XP_BUTTON_HIGHLIGHT);
    
    int start_btn_x = 2;
    int start_btn_y = TASKBAR_Y + 2;
    int start_btn_width = 60;
    int start_btn_height = TASKBAR_HEIGHT - 4;
    
    draw_xp_button(start_btn_x, start_btn_y, start_btn_width, start_btn_height, "Start", false);
    
    int clock_x = SCREEN_WIDTH - 80;
    int clock_y = TASKBAR_Y + 5;

    // Total seconds elapsed
    uint64_t total_seconds = timerTicks / 60;
    
    // Calculate hours, minutes, seconds
    uint64_t hours = total_seconds / 3600;
    uint64_t minutes = (total_seconds % 3600) / 60;
    uint64_t seconds = total_seconds % 60;
    char string[64];
    draw_text(u64toa(hours, string, 10), clock_x, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(u64toa(minutes, string, 10), clock_x + 17, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text(u64toa(seconds, string, 10), clock_x + 34, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    draw_text("AM", clock_x + 51, clock_y, XP_WINDOW_TEXT, 0xA0A0A0);
    

    int win_btn_x = start_btn_x + start_btn_width + 10;
    int win_btn_y = TASKBAR_Y + 4;
    int win_btn_width = 150;
    int win_btn_height = TASKBAR_HEIGHT - 8;
    
    fill_rectangle(win_btn_x, win_btn_y, win_btn_width, win_btn_height, XP_BUTTON_FACE);
    draw_rect_outline(win_btn_x, win_btn_y, win_btn_width, win_btn_height, XP_BUTTON_SHADOW, 1);
    draw_text("Example window", win_btn_x + 5, win_btn_y + 4, XP_WINDOW_TEXT, 0xC0C0C0);
    win_btn_x = win_btn_x + win_btn_width + 5;
    draw_line(win_btn_x + 16, win_btn_y + 9,win_btn_x + 10, win_btn_y + 4, XP_WINDOW_TEXT);
    draw_line(win_btn_x + 16, win_btn_y + 9,win_btn_x + 9, win_btn_y + 14, XP_WINDOW_TEXT);
    draw_line(win_btn_x + 14, win_btn_y + 14,win_btn_x + 24, win_btn_y + 14, XP_WINDOW_TEXT);
    draw_rect_outline(win_btn_x, win_btn_y, win_btn_width, win_btn_height, XP_BUTTON_SHADOW, 1);
    draw_text("Kernel Console", win_btn_x + 26, win_btn_y + 4, XP_WINDOW_TEXT, 0xC0C0C0);
}


void draw_desktop_icon(int x, int y, const char* label, uint32_t icon_color) {
    int icon_size = 32;
    
    // Icon background (light square)
    fill_rectangle(x, y, icon_size, icon_size, XP_BUTTON_HIGHLIGHT);
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] draw_desktop_icon fill_rectangle done\n");
    #endif 
    // Icon border
    draw_rect_outline(x, y, icon_size, icon_size, XP_BUTTON_SHADOW, 1);
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] draw_desktop_icon draw_rect_outline done\n");
    #endif 
    // Icon content (simple colored square)
    fill_rectangle(x + 6, y + 6, icon_size - 12, icon_size - 12, icon_color);
    
    // Icon label (below icon)
    draw_text_centered(label, x - 20, y + icon_size + 5, icon_size + 40, XP_WINDOW_TEXT, 0x008DD5);
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] draw_desktop_icon draw_text_centered done\n");
    #endif 
}

 
// DRAW DESKTOP BACKGROUND
 

void draw_desktop_background() 
{
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] Drawing desktop background\n");
    #endif

    draw_gradient(0, 0, SCREEN_WIDTH, TASKBAR_Y,0x008DD5, 0x0078D7, true); 

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] desktop background done\n");
    #endif 
}

 
// DRAW SCROLLBAR
 

void draw_scrollbar(int x, int y, int height, int scroll_pos, int max_scroll) {
    int track_height = height - 32;  // Account for buttons
    
    // Up button
    fill_rectangle(x, y, 16, 16, XP_BUTTON_FACE);
    draw_beveled_border_thick(x, y, 16, 16, XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE, XP_BUTTON_SHADOW, true);
    draw_line(x + 6, y + 9, x + 10, y + 5, 0x000000);
    draw_line(x + 10, y + 5, x + 10, y + 9, 0x000000);
    
    // Track
    fill_rectangle(x, y + 16, 16, track_height, 0xC0C0C0);
    
    // Thumb
    int thumb_height = (track_height * track_height) / max_scroll;
    if (thumb_height < 8) thumb_height = 8;
    
    int thumb_y = y + 16 + (scroll_pos * track_height) / max_scroll;
    fill_rectangle(x, thumb_y, 16, thumb_height, XP_BUTTON_FACE);
    draw_beveled_border_thick(x, thumb_y, 16, thumb_height, XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE, XP_BUTTON_SHADOW, true);
    
    // Down button
    int down_y = y + height - 16;
    fill_rectangle(x, down_y, 16, 16, XP_BUTTON_FACE);
    draw_beveled_border_thick(x, down_y, 16, 16, XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE, XP_BUTTON_SHADOW, true);
    // draw_line(x + 6, y + down_y + 4, x + 10, down_y + 8, 0x000000);
    // draw_line(x + 10, down_y + 8, x + 10, down_y + 4, 0x000000);
}

// Static wrapper that bridges the function pointer to the member function
static void Console_draw_frame_wrapper(void* context)
{
    Console* console = static_cast<Console*>(context);
    console->draw_frame();
}

int draw_all_xp_windows_but_active()
{
    int windows_drawn = 0;
    // Find a free slot in window_arr
    for (int i = 0; i < MAX_NUM_OF_WINDOWS; i++) 
    {
        if (window_arr[i] == NULL) 
        {
            break;
        }
        if (window_arr[i]->active == false) 
        {
            draw_xp_window(window_arr[i]);
            windows_drawn++;
        }
        else 
        {
            #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
                printf("[DEBUG_GUI] active window is :%d\n", i);
            #endif
        }
    }
    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
        printf("[DEBUG_GUI] windows_drawn is :%d\n", windows_drawn);
    #endif
    
    return windows_drawn;
}

void draw_active_xp_window()
{
    if (active_xp_window)
    {
        draw_xp_window(active_xp_window);
        #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
            printf("[DEBUG_GUI] draw active_xp_window\n");
        #endif
    }
}


#if defined(DEBUG_GUI)
int l = 0;
void direct_clear_screen_dbg ()
{
    // Fill entire screen red so you know the kernel started
    //tempframebuffer->address  bootloader.framebuffer;
    struct limine_framebuffer *fb = bootloader.framebuffer;
    uint32_t *addr = (uint32_t*)fb->address;
    size_t pixel_count = fb->pitch / 4 * fb->height;
    
    l++;
    int mod = l % 3;
    switch (mod)
    {
        case 0:
        for (size_t i = 0; i < pixel_count; i++)
        addr[i] = 0x0F0F00;
        break;
        case 1:
        for (size_t i = 0; i < pixel_count; i++)
        addr[i] = 0x0F000F;
        break;
        case 2:
        for (size_t i = 0; i < pixel_count; i++)
        addr[i] = 0x00F0F;
        break;
        
        default:
        break;
    }
}
#endif


void initialize_xp_desktop() 
{
    frame_ready = false;
    
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] init background\n");
    #endif
    
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] Creating window at (%d,%d) size %dx%d\n", 100, 100, 400, 300);
    #endif

    XPWindow* window = create_xp_window(350, 175, 800, 500, "Kernel Console");
        
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] init console\n");
    #endif
    // Initialize the global console as a window at window position 
    if(!console_initialized)
    {    
        console = Console(window->width, window->height - TITLE_BAR_HEIGHT, window->x, window->y + TITLE_BAR_HEIGHT);
        console_initialize();
        console.clear_screen();
        console.set_bg_color(XP_BACKGROUND);
        console.set_text_color(XP_WINDOW_TEXT);
    }
        
    #if defined(DEBUG_GUI)
    printf("[DEBUG_GUI] initialize_xp_desktop done\n");
    #endif
    frame_ready = true;
}

void render_xp_desktop() 
{
    frame_ready = false;

    draw_desktop_background();

    draw_all_xp_windows_but_active();

    draw_active_xp_window();
    
    draw_taskbar();

    draw_cursor(mouse_position_x, mouse_position_y);

    #if defined(DEBUG_GUI) && defined(DEBUG_LOOPING)
    printf("[DEBUG_GUI] render_xp_desktop done\n");
    #endif
    frame_ready = true;
}

 