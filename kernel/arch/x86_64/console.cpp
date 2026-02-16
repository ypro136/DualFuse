#include <console.h>

#include <framebufferutil.h>
#include <bootloader.h>
#include <serial.h>

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
int _bg_color = 0xC0C0C0; //0x1B262C;
int textcolor = 0x000000;//0xBBE1FA;
bool console_initialized = false;

static Spinlock LOCK_CONSOLE = ATOMIC_FLAG_INIT;

 
// Console Constructor
 
Console::Console(uint32_t width, uint32_t height, uint32_t start_x, uint32_t start_y)
    : _bg_color(0xC0C0C0),
      textcolor(0x000000),
    border_color(0xBBBBBB),
    border_thickness(2),
      is_initialized(false),
      cursorHidden(false),
      cursor_position_x(0),
      cursor_position_y(0),
      window_width(width ? width : screen_width),    // Default to full screen width
      window_height(height ? height : screen_height), // Default to full screen height
      window_x(start_x),
      window_y(start_y),
      screen_width_char(0),
      framebuffer(nullptr)
{
    memset(is_char, 0, sizeof(is_char));
    title[0] = '\0';
}

 
// Position Management Helper Methods
 
void Console::increment_cursor_x()
{
    cursor_position_x += CHAR_WIDTH;
}

void Console::decrement_cursor_x()
{
    if (cursor_position_x >= CHAR_WIDTH)
        cursor_position_x -= CHAR_WIDTH;
}

void Console::increment_cursor_y()
{
    cursor_position_y += CHAR_HEIGHT;
}

void Console::decrement_cursor_y()
{
    if (cursor_position_y >= CHAR_HEIGHT)
        cursor_position_y -= CHAR_HEIGHT;
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

void Console::buffer_character(char c)
{
    spinlock_acquire(&console_buffer.lock);
    
    if (console_buffer.write_index < CONSOLE_BUFFER_SIZE - 1)
    {
        console_buffer.characters[console_buffer.write_index] = c;
        console_buffer.write_index++;
    }
    
    spinlock_release(&console_buffer.lock);
}

void Console::flush_buffer()
{
    spinlock_acquire(&console_buffer.lock);
    cursor_position_x = CHAR_WIDTH; // start with one char margin from left
    cursor_position_y = border_thickness + 2; // just below the border
    // Process all buffered characters
    for (uint32_t i = 0; i < console_buffer.write_index; i++)
    {
        draw_character(console_buffer.characters[i]);
    }
    
    
    
    spinlock_release(&console_buffer.lock);
}

void Console::draw_frame()
{
    if (!is_initialized)
        return;
    
    spinlock_acquire(&LOCK_CONSOLE);
    
    // Process all buffered characters this frame
    flush_buffer();
    
    // Ensure cursor is visible/updated
    update_cursor();
    
    spinlock_release(&LOCK_CONSOLE);
}
// Internal Rendering Methods
 
void Console::draw_rect(int x, int y, int w, int h, int rgb)
{
#if defined(DEBUG_CONSOLE)
    if (!tempframebuffer || !tempframebuffer->address || screen_width == 0)
        return;
#endif
    // Adjust coordinates to window offset
    x += window_x;
    y += window_y;
    
    // Clamp to window boundaries
    if (x < window_x || y < window_y || x + w > window_x + window_width || y + h > window_y + window_height)
        return;
    
    // Use pitch (bytes per line) and bytes-per-pixel to compute addresses
    uint32_t bpp = (screen_width != 0) ? (pitch / screen_width) : 4;
    uint8_t *base = (uint8_t*)tempframebuffer->address;
    for (int i = 0; i < h; i++)
    {
        uint8_t *row = base + (size_t)(y + i) * (size_t)pitch;
        uint32_t *pixel = (uint32_t*)(row + (size_t)x * (size_t)bpp);
        for (int j = 0; j < w; j++)
        {
            pixel[j] = (uint32_t)rgb;
        }
    }
}

bool Console::scroll_console()
{
    if (!(cursor_position_y >= (window_height - CHAR_HEIGHT)))
    {
        return false;
    }
    uint32_t bpp = (screen_width != 0) ? (pitch / screen_width) : 4;
    uint8_t *base = (uint8_t*)tempframebuffer->address;
    for (int y = CHAR_HEIGHT; y < window_height; y++)
    {
        uint8_t *src = base + (size_t)(window_y + y) * (size_t)pitch + (size_t)window_x * (size_t)bpp;
        uint8_t *dest = base + (size_t)(window_y + y - CHAR_HEIGHT) * (size_t)pitch + (size_t)window_x * (size_t)bpp;
        memcpy(dest, src, (size_t)window_width * (size_t)bpp);
    }
    draw_rect(0, window_height - CHAR_HEIGHT, window_width, CHAR_HEIGHT, _bg_color);
    
    // Redraw all borders after scrolling
    draw_box(0, 0, window_width, border_thickness, border_color);                              // Top
    //draw_box(0, window_height - border_thickness, window_width, border_thickness, border_color); // Bottom
    draw_box(0, 0, border_thickness, window_height, border_color);                             // Left
    draw_box(window_width - border_thickness, 0, border_thickness, window_height, border_color); // Right

    draw_title();
    
    decrement_cursor_y();
    return true;
}

void Console::erase_cursor()
{
    if (cursorHidden)
        return;
    draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, _bg_color);
}

void Console::update_cursor()
{
    if (cursorHidden)
        return;
    if (cursor_position_x >= window_width - CHAR_WIDTH)
    {
        bool neededScrolling = scroll_console();
        if (!neededScrolling)
            increment_cursor_y();
        move_to_line_start();
    }
    draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, textcolor);
}

 
// Public Interface
 
void Console::initialize()
{
    // place cursor inside the border (avoid overlapping)
    cursor_position_x = CHAR_WIDTH; // start with one char margin from left
    cursor_position_y = border_thickness + 2; // just below the border
    screen_width_char = window_width / CHAR_WIDTH;

    bool PSF_state = psfLoadDefaults();
#if defined(DEBUG_CONSOLE)
    if (PSF_state)
    {
        printf("[console] PSF font loaded successfully dumping first 32 bytes\n");
        for (int i = 0; i < 32; i++)
        {
            printf("%02X ", ((uint8_t*)psf)[i]);
        }
        printf("\n");
    }
    else
    {
        printf("[console:error] Failed to load PSF font, using default\n");
    }

    printf("[console] console_initialized=%d tempframebuffer=%p psf=%p window_width=%u window_height=%u\n",
           is_initialized, tempframebuffer, psf, window_width, window_height);
           
           // One-time debug: print console window position and framebuffer info
           printf("[console:debug] Window position: (%u, %u), Size: %u x %u pixels\n", window_x, window_y, window_width, window_height);
           printf("[console:debug] Framebuffer address: %p, screen_width: %u, screen_height: %u, pitch: %u\n", 
            tempframebuffer->address, screen_width, screen_height, pitch);
            
            // Compute the byte-per-pixel (guard against zero) and the framebuffer memory
            // address for the first character (window_x, window_y).
            uint32_t bpp = (screen_width != 0) ? (pitch / screen_width) : 4;
            void* first_char_addr = (void*)((uintptr_t)tempframebuffer->address + (uintptr_t)window_y * (uintptr_t)pitch + (uintptr_t)window_x * (uintptr_t)bpp);
            printf("[console:debug] First character will render at addr: %p (pixel: (%u, %u))\n", first_char_addr, window_x, window_y);
            
#endif
        // draw title over the border if present
        draw_title();

        is_initialized = true;
}

void Console::clear_screen()
{
#if defined(DEBUG_CONSOLE)
    printf("[console] clearing screen\n");
#endif
    // Reset buffer
    console_buffer.write_index = 0;
    memset(console_buffer.characters, 0, CONSOLE_BUFFER_SIZE);
    // place cursor inside the border to avoid overlapping
    cursor_position_x = CHAR_WIDTH;
    cursor_position_y = border_thickness + 2;
    draw_rect(0, 0, window_width, window_height, _bg_color);
    // Redraw frame and border
    draw_box(0, 0, window_width, window_height, _bg_color);
    draw_box(0, 0, window_width, border_thickness, border_color);
    //draw_box(0, window_height - border_thickness, window_width, border_thickness, border_color);//bottom
    draw_box(0, 0, border_thickness, window_height, border_color);
    draw_box(window_width - border_thickness, 0, border_thickness, window_height, border_color);

    // Draw title over the border (if set)
    draw_title();
#if defined(DEBUG_CONSOLE)
    printf("[console]:[clear_screen] draw_rect done\n");
#endif
    update_cursor();
#if defined(DEBUG_CONSOLE)
    printf("[console]:[clear_screen] update_cursor done\n");
#endif
}

void Console::set_bg_color(int rgb)
{
    _bg_color = rgb;
}

void Console::set_text_color(int rgb)
{
    textcolor = rgb;
}

void Console::set_color(int color)
{
    textcolor = color;
}

void Console::set_cursor_x(uint32_t x)
{
    erase_cursor();
    cursor_position_x = x;
    update_cursor();
}

void Console::set_cursor_y(uint32_t y)
{
    erase_cursor();
    cursor_position_y = y;
    update_cursor();
}

void Console::set_title(const char* t)
{
    if (!t) return;
    strncpy(title, t, sizeof(title) - 1);
    title[sizeof(title) - 1] = '\0';
    // if initialized, draw immediately
    if (is_initialized)
    {
      draw_title();
    }
}


void Console::draw_title()
{
    if (!title[0]) return;
    // draw title starting a few pixels from the left border, overlapping the top border
    uint32_t tx = border_thickness + 4;
    uint32_t ty = 0; // draw at the top so text overlays border
    for (size_t i = 0; title[i] != '\0'; ++i) {
        psfPutC(title[i], window_x + tx + (i * CHAR_WIDTH), window_y + ty - 8, textcolor, _bg_color);
    }
}

void Console::draw_character(int charnum)
{
    if (!charnum)
        return;

    if (cursor_position_x > (window_width - CHAR_WIDTH * 2))
    {
        move_to_line_start();
        increment_cursor_y();
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
            if (cursor_position_x == 0)
            {
                if (cursor_position_y != 0)
                {
                    decrement_cursor_y();
                    cursor_position_x = (screen_width_char - 2) * CHAR_WIDTH;
                }
            }
            else
            {
                decrement_cursor_x();
            }
        } while (is_char[cursor_position_x / CHAR_WIDTH][cursor_position_y / CHAR_HEIGHT] == 0);
        draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, _bg_color);
        break;
    case '\t':
        for (int i = 0; i < 4; i++)
            draw_character(' ');
        break;
    case ' ':
        erase_cursor();
        psfPutC(charnum, window_x + cursor_position_x, window_y + cursor_position_y, textcolor, _bg_color);
        is_char[cursor_position_x / CHAR_WIDTH][cursor_position_y / CHAR_HEIGHT] = 0;
        increment_cursor_x();
        break;
    default:
        erase_cursor();
        psfPutC(charnum, window_x + cursor_position_x, window_y + cursor_position_y, textcolor, _bg_color);
        is_char[cursor_position_x / CHAR_WIDTH][cursor_position_y / CHAR_HEIGHT] = 1;
        increment_cursor_x();
        break;
    }
    update_cursor();
}void Console::print_char(char character)
{
    spinlock_acquire(&LOCK_CONSOLE);
    buffer_character(character);
    spinlock_release(&LOCK_CONSOLE);
}

// Draw a filled rectangle relative to the console window origin
void Console::draw_box(int x, int y, int w, int h, int rgb)
{
    // public wrapper that uses the console's draw_rect (which applies window offset)
    draw_rect(x, y, w, h, rgb);
}

void Console::set_window_size(uint32_t width, uint32_t height)
{
    window_width = width ? width : screen_width;
    window_height = height ? height : screen_height;
    screen_width_char = window_width / CHAR_WIDTH;
    // keep cursor inside border after resizing
    cursor_position_x = CHAR_WIDTH;
    cursor_position_y = border_thickness + 2;
}

void Console::set_window_position(uint32_t x, uint32_t y)
{
    window_x = x;
    window_y = y;
}

 
// Legacy C API Wrappers for Compatibility
 
void console_initialize()
{
    console.initialize();
    console_initialized = console.is_ready();
    _bg_color = 0xC0C0C0;// 0x1B262C;
    textcolor = 0x000000;//0xBBE1FA;
}

void drawCharacter(int charnum)
{
    console.draw_character(charnum);
}

void changeBg(int rgb)
{
    console.set_bg_color(rgb);
    _bg_color = rgb;
}

void changeTextColor(int rgb)
{
    console.set_text_color(rgb);
    textcolor = rgb;
}

uint32_t getConsoleX()
{
    return console.get_cursor_x();
}

uint32_t getConsoleY()
{
    return console.get_cursor_y();
}

void setConsoleX(uint32_t x)
{
    console.set_cursor_x(x);
}

void setConsoleY(uint32_t y)
{
    console.set_cursor_y(y);
}

void eraseBull()
{
    // Legacy function; no direct equivalent in class
    // Can be extended if needed
}

void updateBull()
{
    // Legacy function; no direct equivalent in class
    // Can be extended if needed
}

void clear_screen()
{
    console.clear_screen();
}

void printfch(char character)
{
    console.print_char(character);
}

extern "C" void console_draw_frame()
{
    if (console_initialized && tempframebuffer && psf && tempframebuffer->address && screen_width != 0)
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
    if (console_initialized && tempframebuffer && psf && tempframebuffer->address && screen_width != 0)
    {
        printfch(c);
    }
    serial_write(c);
}
