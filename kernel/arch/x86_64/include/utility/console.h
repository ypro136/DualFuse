#include <types.h>
#include <limine.h>
#include <shell.h>
#include <spinlock.h>
#include <GUI.h>
#include <atomic>

#ifndef CONSOLE_H
#define CONSOLE_H

#define TTY_CHARACTER_WIDTH  8
#define TTY_CHARACTER_HEIGHT 16
#define MAX_NUM_OF_CONSOLES 32

#define CHAR_HEIGHT (psf->height)
#define CHAR_WIDTH (8)
#define CONSOLE_BUFFER_SIZE (8192)

extern bool g_console_output_enabled;

struct ConsoleBuffer {
    char characters[CONSOLE_BUFFER_SIZE];
    uint32_t write_index;
    Spinlock lock;
};

class Console
{
private:
    ConsoleBuffer buffer;
    int  _bg_color;
    int  textcolor;
    int  border_color;
    int  border_thickness;

    Shell* shell;

    bool     is_initialized;
    bool     visible;
    bool     cursorHidden;
    uint32_t cursor_position_x;
    uint32_t cursor_position_y;

    uint32_t window_width;
    uint32_t window_height;
    int32_t  window_x;
    int32_t  window_y;
    uint32_t screen_width_char;

    void* framebuffer;

    void clamp_cursor();
    void draw_rect(int x, int y, int w, int h, int rgb);
    void safe_put_char(int charnum, int rel_x, int rel_y, int fg, int bg);
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
    void draw_character(int charnum);
    void print_char(char character);

    void set_bg_color(int rgb);
    void set_text_color(int rgb);
    void set_color(int color);

    uint32_t get_cursor_x() const { return cursor_position_x; }
    uint32_t get_cursor_y() const { return cursor_position_y; }
    void set_cursor_x(uint32_t x);
    void set_cursor_y(uint32_t y);

    void set_window_size(uint32_t width, uint32_t height);
    void set_window_position(int32_t x, int32_t y);
    uint32_t get_window_width()  const { return window_width;  }
    uint32_t get_window_height() const { return window_height; }
    int32_t  get_window_x()      const { return window_x;      }
    int32_t  get_window_y()      const { return window_y;      }

    void  set_framebuffer(void* fb) { framebuffer = fb; }
    void* get_framebuffer() const   { return framebuffer; }
    bool  is_ready() const          { return is_initialized; }
    bool  is_visible() const        { return visible; }
    void  set_visible(bool val);

    Shell* get_shell() const  { return shell; }
    void   set_shell(Shell* s){ shell = s;    }
};

extern Console console;
extern Console* console_arr[MAX_NUM_OF_CONSOLES];
extern Console* active_console;

Console* create_console(XPWindow* window);
void destroy_console(Console* c);

// Legacy C API (global console only)
void console_initialize();
void drawCharacter(int charnum);
void changeBg(int rgb);
void changeTextColor(int rgb);
uint32_t getConsoleX();
uint32_t getConsoleY();
void setConsoleX(uint32_t x);
void setConsoleY(uint32_t y);
void clear_screen();
void printfch(char character);
void console_buffer_char(char c);
void set_window_size(uint32_t width, uint32_t height);

// Console output suppression
bool console_is_output_enabled(void);
void console_set_output_enabled(bool enabled);

extern "C" void putchar_(char c);

#endif