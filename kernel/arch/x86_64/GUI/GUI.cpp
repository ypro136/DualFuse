#include <gui_primitives.h>
#include <cstdint>
#include <cstring>

#include <framebufferutil.h>
#include <psf.h>
#include <timer.h>
#include <console.h>


#include <GUI.h>

 
// Taskbar constants
#define TASKBAR_HEIGHT 28
#define TASKBAR_Y (SCREEN_HEIGHT - TASKBAR_HEIGHT)

// Window constants
#define TITLE_BAR_HEIGHT 24
#define WINDOW_BORDER_WIDTH 4


void draw_window_title_bar(const XPWindow& win) {
    draw_gradient(win.x, win.y, win.width, TITLE_BAR_HEIGHT, 0x000080, 0x1084D7, true);
    draw_line(win.x + 1, win.y + 1, win.x + win.width - 2, win.y + 1, 0x2E5C8A);
    draw_text(win.title, win.x + 8, win.y + 6, XP_WINDOW_TEXT, 0x000080);
}

void draw_window_button(int x, int y, int size, const char* label, bool active) {
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

  

void draw_xp_window(const XPWindow& win) 
{
    fill_rectangle(win.x, win.y, win.width, win.height, XP_BACKGROUND);
    
    draw_window_title_bar(win);
    
    draw_beveled_border_thick(win.x, win.y, win.width, win.height,
                              XP_BUTTON_HIGHLIGHT, XP_BUTTON_FACE, XP_BUTTON_SHADOW, true);
    
    int client_x = win.x + WINDOW_BORDER_WIDTH;
    int client_y = win.y + TITLE_BAR_HEIGHT + WINDOW_BORDER_WIDTH;
    int client_width = win.width - 2 * WINDOW_BORDER_WIDTH;
    int client_height = win.height - TITLE_BAR_HEIGHT - 2 * WINDOW_BORDER_WIDTH;
    
    fill_rectangle(client_x, client_y, client_width, client_height, win.bg_color);
    
    int btn_y = win.y + 4;
    int btn_size = 16;
    
    draw_window_button(win.x + win.width - btn_size - 4, btn_y, btn_size, "X", true);
    draw_window_button(win.x + win.width - (btn_size + 4) * 2, btn_y, btn_size, "[", true);
    draw_window_button(win.x + win.width - (btn_size + 4) * 3, btn_y, btn_size, "_", true);
}


void draw_xp_button(int x, int y, int width, int height, const char* label, bool pressed) {
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

void draw_taskbar() {
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
 

void draw_desktop_background() {

    draw_gradient(0, 0, SCREEN_WIDTH, TASKBAR_Y,0x008DD5, 0x0078D7, true); 
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] desktop background done\n");
    #endif 
    
    draw_desktop_icon(10, 10, "Computer", 0x808080);
    draw_desktop_icon(10, 60, "Documents", 0x808080);
    draw_desktop_icon(10, 110, "Network", 0x808080);
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

int animation = 105;
int step = 0;
void render_xp_desktop() 
    {
        frame_ready = false;
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Clearing screen %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    #endif
        //clear_screen(SCREEN_WIDTH, SCREEN_HEIGHT, 0x000000);
        
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Drawing desktop background\n");
    #endif
        draw_desktop_background();
        
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Creating window1 at (%d,%d) size %dx%d\n", 100, 100, 400, 300);
    #endif
    if(animation > 200 || animation < 100)
    {
        step = (-1 * step);
        animation += step;
    }else{
        animation += step;
    }
        XPWindow window1 = {
            .x = animation,
            .y = 100,
            .width = 400,
            .height = 300,
            .title = "Example window",
            .active = true,
            .minimized = false,
            .bg_color = 0xECE9D8
        };
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Drawing window1\n");
    #endif
        draw_xp_window(window1);
        
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Drawing window1 client area rectangle\n");
    #endif
        fill_rectangle(window1.x + 10, window1.y + 40, window1.width - 20, window1.height - 60, 0xECE9D8);
        
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Drawing window1 button: Open\n");
    #endif
        draw_xp_button(window1.x + 20, window1.y + 50, 100, 25, "Open", false);
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Drawing window1 button: Delete\n");
    #endif
        draw_xp_button(window1.x + 130, window1.y + 50, 100, 25, "Delete", false);
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Drawing window1 button: Cancel\n");
    #endif
        draw_xp_button(window1.x + 240, window1.y + 50, 100, 25, "Cancel", false);
        
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Creating window2 at (%d,%d) size %dx%d\n", 550, 150, 350, 250);
    #endif
        XPWindow window2 = {
            .x = 350,
            .y = 175,
            .width = 800,
            .height = 500,
            .title = "Kernel Console",
            .active = false,
            .minimized = false,
            .bg_color = XP_BACKGROUND
        };
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Drawing window2\n");
    #endif
        draw_xp_window(window2);

    // Initialize the global console as a window at position (100,50)
    if(!console_initialized)
    {    
    console = Console(window2.width, window2.height - TITLE_BAR_HEIGHT, window2.x, window2.y + TITLE_BAR_HEIGHT);
    console_initialize();
    console.clear_screen();
    console.set_bg_color(XP_BACKGROUND);
    console.set_text_color(XP_WINDOW_TEXT);
    }
    console.draw_frame();
    //console.set_title("Kernel Console");        
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Drawing window2 text line 1\n");
    #endif
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Drawing window2 scrollbar at x=%d, y=%d, height=%d\n", 
            window2.x + window2.width - 20, window2.y + TITLE_BAR_HEIGHT + 4, 
            window2.height - TITLE_BAR_HEIGHT - 10);
    #endif
        draw_scrollbar(window2.x + window2.width - 20, window2.y + TITLE_BAR_HEIGHT + 4, window2.height - TITLE_BAR_HEIGHT - 10, 0, window2.height);
        
        // Draw taskbar (always last, on top)
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Drawing taskbar\n");
    #endif
        draw_taskbar();
    #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] Render complete\n");
    #endif
    frame_ready = true;
}

 