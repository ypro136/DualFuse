#include <console.h>

#include <framebufferutil.h>
#include <bootloader.h>
#include <serial.h>
#include <gui_primitives.h>
#include <liballoc.h>


#include <utility.h>
#include <spinlock.h>
#include <psf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>

#define CHAR_HEIGHT (psf->height)
#define CHAR_WIDTH (8)
#define CONSOLE_BUFFER_SIZE (2048)

// Global instance
Console console;

Console* console_arr[MAX_NUM_OF_CONSOLES] = {0};
Console* active_console;

// Character buffer for frame-based rendering
struct ConsoleBuffer {
    char characters[CONSOLE_BUFFER_SIZE];
    uint32_t write_index;
    Spinlock lock;
};

static ConsoleBuffer console_buffer = {
    .characters = {},
    .write_index = 0,
    .lock = ATOMIC_FLAG_INIT
};

// Legacy globals for C compatibility
int _bg_color = 0xC0C0C0;
int textcolor = 0x000000;
bool console_initialized = false;

static Spinlock LOCK_CONSOLE = ATOMIC_FLAG_INIT;

//   Helpers                      ─

// Clamp integer to [lo, hi]
static inline int clamp_int(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Is a pixel coordinate inside the screen entirely?
static inline bool screen_pixel_valid(int x, int y)
{
    return (x >= 0 && y >= 0 &&
            (uint32_t)x < screen_width &&
            (uint32_t)y < screen_height);
}

//   Constructor                     

Console::Console(uint32_t width, uint32_t height, uint32_t start_x, uint32_t start_y)
    : _bg_color(0xC0C0C0),
      textcolor(0x000000),
      border_color(0xBBBBBB),
      border_thickness(2),
      is_initialized(false),
      cursorHidden(false),
      cursor_position_x(0),
      cursor_position_y(0),
      window_width(width  ? width  : screen_width),
      window_height(height ? height : screen_height),
      window_x(start_x),
      window_y(start_y),
      screen_width_char(0),
      framebuffer(nullptr)
{
    memset(is_char, 0, sizeof(is_char));
    title[0] = '\0';
}

//   Position Helpers                   ─

void Console::increment_cursor_x() { cursor_position_x += CHAR_WIDTH; }

void Console::decrement_cursor_x()
{
    if (cursor_position_x >= (uint32_t)CHAR_WIDTH)
        cursor_position_x -= CHAR_WIDTH;
    else
        cursor_position_x = 0;
}

void Console::increment_cursor_y() { cursor_position_y += CHAR_HEIGHT; }

void Console::decrement_cursor_y()
{
    if (cursor_position_y >= (uint32_t)CHAR_HEIGHT)
        cursor_position_y -= CHAR_HEIGHT;
    else
        cursor_position_y = 0;
}

void Console::advance_line()
{
    cursor_position_x = CHAR_WIDTH;
    increment_cursor_y();
}

void Console::move_to_line_start()
{
    cursor_position_x = CHAR_WIDTH;
}

//   Cursor clamping                   ──

// Call this after ANY cursor change to keep it inside the window
void Console::clamp_cursor()
{
    uint32_t min_x = CHAR_WIDTH;
    uint32_t min_y = (uint32_t)(border_thickness + 2);
    uint32_t max_x = (window_width  >= (uint32_t)CHAR_WIDTH)  ? (window_width  - CHAR_WIDTH)  : 0;
    uint32_t max_y = (window_height >= (uint32_t)CHAR_HEIGHT) ? (window_height - CHAR_HEIGHT) : 0;

    if (cursor_position_x < min_x) cursor_position_x = min_x;
    if (cursor_position_x > max_x) cursor_position_x = max_x;
    if (cursor_position_y < min_y) cursor_position_y = min_y;
    if (cursor_position_y > max_y) cursor_position_y = max_y;
}

//   Buffer                      ──

void Console::buffer_character(char c)
{
    spinlock_acquire(&console_buffer.lock);
    if (is_visible())
    {
        if (console_buffer.write_index < CONSOLE_BUFFER_SIZE - 1)
        {
            console_buffer.characters[console_buffer.write_index] = c;
            console_buffer.write_index++;
        }
    }
    spinlock_release(&console_buffer.lock);
}

void Console::flush_buffer()
{
    spinlock_acquire(&console_buffer.lock);
    cursor_position_x = CHAR_WIDTH;
    cursor_position_y = border_thickness + 2;
    clamp_cursor();
    for (uint32_t i = 0; i < console_buffer.write_index; i++)
        draw_character(console_buffer.characters[i]);
    spinlock_release(&console_buffer.lock);
}

void Console::draw_frame()
{
    if (!is_initialized)
        return;
    spinlock_acquire(&LOCK_CONSOLE);
    flush_buffer();
    update_cursor();
    spinlock_release(&LOCK_CONSOLE);
}

//   draw_rect                     ──
// Coordinates are RELATIVE to the window origin.
// We clamp to both the window rectangle AND the screen rectangle.

void Console::draw_rect(int x, int y, int w, int h, int rgb)
{
    if (!tempframebuffer || !tempframebuffer->address || screen_width == 0 || screen_height == 0)
        return;
    if (w <= 0 || h <= 0)
        return;

    int abs_x = window_x + x;   // window_x is now int32_t, no wrap
    int abs_y = window_y + y;

    int x0 = clamp_int(abs_x,     0, (int)screen_width);
    int y0 = clamp_int(abs_y,     0, (int)screen_height);
    int x1 = clamp_int(abs_x + w, 0, (int)screen_width);
    int y1 = clamp_int(abs_y + h, 0, (int)screen_height);

    // Also clamp to window
    int wx0 = clamp_int(window_x,                        0, (int)screen_width);
    int wy0 = clamp_int(window_y,                        0, (int)screen_height);
    int wx1 = clamp_int(window_x + (int)window_width,    0, (int)screen_width);
    int wy1 = clamp_int(window_y + (int)window_height,   0, (int)screen_height);

    x0 = clamp_int(x0, wx0, wx1);
    y0 = clamp_int(y0, wy0, wy1);
    x1 = clamp_int(x1, wx0, wx1);
    y1 = clamp_int(y1, wy0, wy1);

    int clamped_w = x1 - x0;
    int clamped_h = y1 - y0;
    if (clamped_w <= 0 || clamped_h <= 0)
        return;

    uint32_t bpp  = tempframebuffer->pitch / screen_width;
    uint8_t* base = (uint8_t*)tempframebuffer->address;

    for (int row = 0; row < clamped_h; row++)
    {
        uint8_t*  line  = base + (size_t)(y0 + row) * (size_t)tempframebuffer->pitch;
        uint32_t* pixel = (uint32_t*)(line + (size_t)x0 * (size_t)bpp);
        for (int col = 0; col < clamped_w; col++)
            pixel[col] = (uint32_t)rgb;
    }
}

//   psfPutC wrapper with bounds check             ─
// All character rendering goes through here.

void Console::safe_put_char(int charnum, int rel_x, int rel_y, int fg, int bg)
{
    if (!tempframebuffer || !tempframebuffer->address)
        return;

    int abs_x = (int)window_x + rel_x;
    int abs_y = (int)window_y + rel_y;

    // Skip if completely off screen
    if (abs_x + CHAR_WIDTH  <= 0) return;
    if (abs_y + CHAR_HEIGHT <= 0) return;
    if ((uint32_t)abs_x >= screen_width)  return;
    if ((uint32_t)abs_y >= screen_height) return;

    // Skip if completely outside window
    if (abs_x + CHAR_WIDTH  <= (int)window_x) return;
    if (abs_y + CHAR_HEIGHT <= (int)window_y) return;
    if ((uint32_t)abs_x >= window_x + window_width)  return;
    if ((uint32_t)abs_y >= window_y + window_height) return;

    psfPutC(charnum, abs_x, abs_y, fg, bg);
}

//   Scrolling                     ──

bool Console::scroll_console()
{
    if (!tempframebuffer || !tempframebuffer->address || screen_width == 0)
        return false;
    if (!(cursor_position_y >= (window_height - CHAR_HEIGHT)))
        return false;

    uint32_t bpp  = tempframebuffer->pitch / screen_width;
    uint8_t* base = (uint8_t*)tempframebuffer->address;

    for (uint32_t row = CHAR_HEIGHT; row < window_height; row++)
    {
        int src_abs_y  = window_y + (int)row;
        int dest_abs_y = window_y + (int)row - CHAR_HEIGHT;

        // Skip rows outside screen vertically
        if (src_abs_y  < 0 || (uint32_t)src_abs_y  >= screen_height) continue;
        if (dest_abs_y < 0 || (uint32_t)dest_abs_y >= screen_height) continue;

        // Clamp horizontal copy to screen bounds
        int copy_x_start = window_x;
        int copy_x_end   = window_x + (int)window_width;

        if (copy_x_start < 0)          copy_x_start = 0;
        if (copy_x_end   < 0)          continue;  // entire row off screen left
        if ((uint32_t)copy_x_end > screen_width)
            copy_x_end = (int)screen_width;
        if (copy_x_start >= copy_x_end) continue;  // nothing to copy

        int copy_width = copy_x_end - copy_x_start;

        uint8_t* src  = base + (size_t)src_abs_y  * (size_t)tempframebuffer->pitch
                             + (size_t)copy_x_start * (size_t)bpp;
        uint8_t* dest = base + (size_t)dest_abs_y * (size_t)tempframebuffer->pitch
                             + (size_t)copy_x_start * (size_t)bpp;

        memcpy(dest, src, (size_t)copy_width * (size_t)bpp);
    }

    draw_rect(0, window_height - CHAR_HEIGHT, window_width, CHAR_HEIGHT, _bg_color);

    draw_box(0, 0, window_width, border_thickness, border_color);
    draw_box(0, 0, border_thickness, window_height, border_color);
    draw_box(window_width - border_thickness, 0, border_thickness, window_height, border_color);

    draw_title();
    decrement_cursor_y();
    clamp_cursor();
    return true;
}

//   Public Interface                   ─

void Console::initialize()
{
    cursor_position_x = CHAR_WIDTH;
    cursor_position_y = border_thickness + 2;
    screen_width_char = window_width / CHAR_WIDTH;

    bool PSF_state = psfLoadDefaults();

#if defined(DEBUG_CONSOLE)
    if (PSF_state)
        printf("[console] PSF font loaded successfully\n");
    else
        printf("[console:error] Failed to load PSF font\n");

    printf("[console] window=(%u,%u) size=%ux%u\n", window_x, window_y, window_width, window_height);
    printf("[console] screen=%ux%u pitch=%u\n", screen_width, screen_height, tempframebuffer ? tempframebuffer->pitch : 0);
#endif

    draw_title();
    is_initialized = true;

    set_visible(true);
}

void Console::clear_screen()
{
    if(!is_visible())
    {
        return;
    }
    console_buffer.write_index = 0;
    memset(console_buffer.characters, 0, CONSOLE_BUFFER_SIZE);

    cursor_position_x = CHAR_WIDTH;
    cursor_position_y = border_thickness + 2;
    clamp_cursor();

    draw_rect(0, 0, window_width, window_height, _bg_color);
    draw_box(0, 0, window_width, window_height,  _bg_color);
    draw_box(0, 0, window_width, border_thickness, border_color);
    draw_box(0, 0, border_thickness, window_height, border_color);
    draw_box(window_width - border_thickness, 0, border_thickness, window_height, border_color);

    draw_title();
    update_cursor();
}

void Console::set_bg_color(int rgb)   { _bg_color  = rgb; }
void Console::set_text_color(int rgb) { textcolor  = rgb; }
void Console::set_color(int color)    { textcolor  = color; }

void Console::set_cursor_x(uint32_t x)
{
    erase_cursor();
    cursor_position_x = x;
    clamp_cursor();
    update_cursor();
}

void Console::set_cursor_y(uint32_t y)
{
    erase_cursor();
    cursor_position_y = y;
    clamp_cursor();
    update_cursor();
}

void Console::set_title(const char* t)
{
    if (!t) return;
    strncpy(title, t, sizeof(title) - 1);
    title[sizeof(title) - 1] = '\0';
    if (is_initialized)
        draw_title();
}

void Console::draw_title()
{
    if(!is_visible())
    {
        return;
    }
    if (!title[0]) return;
    uint32_t tx = border_thickness + 4;
    uint32_t ty = 0;
    for (size_t i = 0; title[i] != '\0'; ++i)
        safe_put_char(title[i], tx + (i * CHAR_WIDTH), ty - 8, textcolor, _bg_color);
}

void Console::erase_cursor()
{
    if(!is_visible())
    {
        return;
    }
    if (cursorHidden) return;
    draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, _bg_color);
}

void Console::update_cursor()
{
    if(!is_visible())
    {
        return;
    }
    if (cursorHidden) return;

    if (cursor_position_x >= window_width - CHAR_WIDTH)
    {
        bool scrolled = scroll_console();
        if (!scrolled)
            increment_cursor_y();
        move_to_line_start();
    }

    clamp_cursor();
    draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, textcolor);
}

void Console::draw_character(int charnum)
{
    if (!charnum)
        return;

    if(!is_visible())
    {
        return;
    }

    // Wrap if past right edge
    if (cursor_position_x > (window_width - CHAR_WIDTH * 2))
    {
        move_to_line_start();
        increment_cursor_y();
        clamp_cursor();
    }

    scroll_console();

    switch (charnum)
    {
    case -1:
        draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, _bg_color);
        increment_cursor_x();
        break;

    case '\n':
        erase_cursor();
        if (cursor_position_x / CHAR_WIDTH < 200 && cursor_position_y / CHAR_HEIGHT < 200)
            is_char[cursor_position_x / CHAR_WIDTH][cursor_position_y / CHAR_HEIGHT] = 0;
        move_to_line_start();
        increment_cursor_y();
        break;

    case 0xd:
        erase_cursor();
        move_to_line_start();
        break;

    case 0xf:
        break;

    case '\b':
        erase_cursor();
        do
        {
            if (cursor_position_x < (uint32_t)CHAR_WIDTH)
            {
                if (cursor_position_y >= (uint32_t)CHAR_HEIGHT)
                {
                    decrement_cursor_y();
                    cursor_position_x = (screen_width_char - 2) * CHAR_WIDTH;
                }
                else
                    break; // Already at top-left, nothing to backspace
            }
            else
            {
                decrement_cursor_x();
            }
        } while (cursor_position_x / CHAR_WIDTH < 200 &&
                 cursor_position_y / CHAR_HEIGHT < 200 &&
                 is_char[cursor_position_x / CHAR_WIDTH][cursor_position_y / CHAR_HEIGHT] == 0);
        draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, _bg_color);
        break;

    case '\t':
        for (int i = 0; i < 4; i++)
            draw_character(' ');
        return; // update_cursor called recursively

    case ' ':
        erase_cursor();
        safe_put_char(charnum, cursor_position_x, cursor_position_y, textcolor, _bg_color);
        if (cursor_position_x / CHAR_WIDTH < 200 && cursor_position_y / CHAR_HEIGHT < 200)
            is_char[cursor_position_x / CHAR_WIDTH][cursor_position_y / CHAR_HEIGHT] = 0;
        increment_cursor_x();
        break;

    default:
        erase_cursor();
        safe_put_char(charnum, cursor_position_x, cursor_position_y, textcolor, _bg_color);
        if (cursor_position_x / CHAR_WIDTH < 200 && cursor_position_y / CHAR_HEIGHT < 200)
            is_char[cursor_position_x / CHAR_WIDTH][cursor_position_y / CHAR_HEIGHT] = 1;
        increment_cursor_x();
        break;
    }

    clamp_cursor();
    update_cursor();
}

void Console::print_char(char character)
{
    spinlock_acquire(&LOCK_CONSOLE);
    buffer_character(character);
    spinlock_release(&LOCK_CONSOLE);
}

void Console::draw_box(int x, int y, int w, int h, int rgb)
{
    draw_rect(x, y, w, h, rgb);
}

void Console::set_window_size(uint32_t width, uint32_t height)
{
    window_width  = width  ? width  : screen_width;
    window_height = height ? height : screen_height;
    screen_width_char = window_width / CHAR_WIDTH;
    cursor_position_x = CHAR_WIDTH;
    cursor_position_y = border_thickness + 2;
    clamp_cursor();
}

void Console::set_visible(bool val)
{
    visible = val;
}

void Console::set_window_position(int32_t x, int32_t y)
{
    window_x = x;
    window_y = y;
}

Console* create_console(XPWindow* window)
{
#if defined(DEBUG_CONSOLE)
    printf("[DEBUG_CONSOLE] create_console: window ptr:%p x:%d y:%d width:%d height:%d\n",window, window->x, window->y, window->width, window->height);
#endif

    Console* temp_console = new Console(window->width, window->height - TITLE_BAR_HEIGHT,window->x, window->y + TITLE_BAR_HEIGHT);
#if defined(DEBUG_CONSOLE)
    printf("[DEBUG_CONSOLE] create_console: Console() ptr:%p\n", temp_console);
#endif

    temp_console->initialize();
#if defined(DEBUG_CONSOLE)
    printf("[DEBUG_CONSOLE] create_console: initialized\n");
#endif

    temp_console->clear_screen();
#if defined(DEBUG_CONSOLE)
    printf("[DEBUG_CONSOLE] create_console: screen cleared\n");
#endif

    temp_console->set_bg_color(XP_BACKGROUND);
#if defined(DEBUG_CONSOLE)
    printf("[DEBUG_CONSOLE] create_console: bg_color set to XP_BACKGROUND\n");
#endif

    temp_console->set_text_color(XP_WINDOW_TEXT);
#if defined(DEBUG_CONSOLE)
    printf("[DEBUG_CONSOLE] create_console: text_color set to XP_WINDOW_TEXT\n");
#endif

    bool slot_found = false;
    for (int i = 0; i < MAX_NUM_OF_CONSOLES; i++)
    {
        if (console_arr[i] == NULL)
        {
            console_arr[i] = temp_console;
            slot_found = true;
#if defined(DEBUG_CONSOLE)
            printf("[DEBUG_CONSOLE] create_console: registered in console_arr slot:%d\n", i);
#endif
            break;
        }
    }

#if defined(DEBUG_CONSOLE)
    if (!slot_found)
        printf("[DEBUG_CONSOLE] create_console: console_arr is FULL, console not registered!\n");

    printf("[DEBUG_CONSOLE] create_console: done, returning ptr:%p\n", temp_console);
#endif

    return temp_console;
}

void destroy_console(Console* c)
{
    if (!c) return;
    for (int i = 0; i < MAX_NUM_OF_CONSOLES; i++)
    {
        if (console_arr[i] == c)
        {
            console_arr[i] = NULL;
            break;
        }
    }
    delete c;
}


//   Legacy C API                    ──

void console_initialize()
{
    console.initialize();
    console_initialized = console.is_ready();
    active_console = &console;
    _bg_color = 0xC0C0C0;
    textcolor = 0x000000;
}

void drawCharacter(int charnum)    { console.draw_character(charnum); }
void changeBg(int rgb)             { console.set_bg_color(rgb); _bg_color = rgb; }
void changeTextColor(int rgb)      { console.set_text_color(rgb); textcolor = rgb; }
uint32_t getConsoleX()             { return console.get_cursor_x(); }
uint32_t getConsoleY()             { return console.get_cursor_y(); }
void setConsoleX(uint32_t x)       { console.set_cursor_x(x); }
void setConsoleY(uint32_t y)       { console.set_cursor_y(y); }
void eraseBull()                   { }
void updateBull()                  { }
void clear_screen()                { console.clear_screen(); }

void printfch(char character) { 
    console.print_char(character); 
    active_console->print_char(character);
}

extern "C" void console_draw_frame()
{
    if (console_initialized && tempframebuffer && psf &&
        tempframebuffer->address && screen_width != 0)
    {
        console.draw_frame();
    }
}

extern "C" void console_buffer_char(char c)
{
    console.buffer_character(c);
}

extern "C" void putchar_(char c)
{
    if (console_initialized && tempframebuffer && psf &&
        tempframebuffer->address && screen_width != 0)
    {
        printfch(c);
    }
    serial_write(c);
}