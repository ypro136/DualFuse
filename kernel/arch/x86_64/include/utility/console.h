#include <types.h>
#include <limine.h>

#include <GUI.h>

#ifndef CONSOLE_H
#define CONSOLE_H

#define TTY_CHARACTER_WIDTH  8
#define TTY_CHARACTER_HEIGHT 16

#define MAX_NUM_OF_CONSOLES 32

class Console
{
private:
    int  _bg_color;
    int  textcolor;
    int  border_color;
    int  border_thickness;
    char title[64];

    bool     is_initialized;
    bool     visible;
    bool     cursorHidden;
    uint32_t cursor_position_x;
    uint32_t cursor_position_y;

    // Window dimensions
    uint32_t window_width;
    uint32_t window_height;
    int32_t window_x;
    int32_t window_y;
    uint32_t screen_width_char;

    bool is_char[200][200];

    void* framebuffer;

    // Position helpers
    void increment_cursor_x();
    void decrement_cursor_x();
    void increment_cursor_y();
    void decrement_cursor_y();
    void advance_line();
    void move_to_line_start();

    // Clamp cursor to valid range inside window
    void clamp_cursor();

    // Internal rendering
    void draw_rect(int x, int y, int w, int h, int rgb);
    void safe_put_char(int charnum, int rel_x, int rel_y, int fg, int bg); // bounds-checked psfPutC
    bool scroll_console();
    void erase_cursor();
    void update_cursor();

public:
    Console(uint32_t width = 0, uint32_t height = 0,
            uint32_t start_x = 0, uint32_t start_y = 0);
    ~Console() = default;

    void buffer_character(char c);
    void draw_frame();
    void flush_buffer();
    void initialize();
    void clear_screen();
    void set_title(const char* t);
    void draw_title();
    void draw_character(int charnum);
    void print_char(char character);
    void draw_box(int x, int y, int w, int h, int rgb);

    // Color management
    void set_bg_color(int rgb);
    void set_text_color(int rgb);
    void set_color(int color);

    // Cursor position management
    uint32_t get_cursor_x() const { return cursor_position_x; }
    uint32_t get_cursor_y() const { return cursor_position_y; }
    void set_cursor_x(uint32_t x);
    void set_cursor_y(uint32_t y);

    // Window management
    void set_window_size(uint32_t width, uint32_t height);
    void set_window_position(int32_t x, int32_t y);
    uint32_t get_window_width()  const { return window_width;  }
    uint32_t get_window_height() const { return window_height; }
    int32_t get_window_x()      const { return window_x;      }
    int32_t get_window_y()      const { return window_y;      }
    
    // Framebuffer management
    void  set_framebuffer(void* fb) { framebuffer = fb; }
    void* get_framebuffer() const   { return framebuffer; }
    bool  is_ready() const          { return is_initialized; }
    bool  is_visible() const          { return visible; }
    void set_visible(bool val);
};

// Global instance
extern Console console;

extern Console* console_arr[MAX_NUM_OF_CONSOLES];

extern Console* active_console;

Console* create_console(XPWindow* window);
void destroy_console(Console* c);

// Legacy C API
extern int  _bg_color;
extern int  textcolor;
extern bool console_initialized;

void     console_initialize();
void     drawCharacter(int charnum);
void     changeBg(int rgb);
void     changeTextColor(int rgb);
uint32_t getConsoleX();
uint32_t getConsoleY();
void     setConsoleX(uint32_t x);
void     setConsoleY(uint32_t y);
void     eraseBull();
void     updateBull();
void     clear_screen();
void     printfch(char character);

extern "C" void putchar_(char c);

#endif