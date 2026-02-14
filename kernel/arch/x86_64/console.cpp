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

// Global instance
__attribute__((used))
Console console;

//int bg_color = 0xC0C0C0; //0x1B262C; dualfuse
__attribute__((used))
bool console_initialized = false;

static Spinlock LOCK_CONSOLE = ATOMIC_FLAG_INIT;

Console::Console(uint32_t width, uint32_t height, uint32_t start_x, uint32_t start_y) : window_x(start_x), window_y(start_y)
{
    bg_color = 0x1B262C;
    textcolor = 0xBBE1FA;
    border_color = 0xBBBBBB;
    border_thickness = 2;
    cursorHidden = false;
    cursor_position_x = 0;
    cursor_position_y = 0;
    window_width = width ? width : screen_width;
    window_height = height ? height : screen_height;
    screen_width_char = 0;
    
    memset(is_char, 0, sizeof(is_char));
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
    if (!psf) return;
    #if defined(DEBUG_CONSOLE)
    printf("[console] increment_cursor_y\n");
    #endif
    cursor_position_y += CHAR_HEIGHT;
}

void Console::decrement_cursor_y()
{
    if (!psf) return;
    if (cursor_position_y >= CHAR_HEIGHT){
        cursor_position_y -= CHAR_HEIGHT;
    }
}

void Console::advance_line()
{
    cursor_position_x = CHAR_WIDTH;
    increment_cursor_y();
}

void Console::move_to_line_start()
{
    #if defined(DEBUG_CONSOLE)
    printf("[console] move_to_line_start\n");
    #endif
    cursor_position_x = CHAR_WIDTH;
}

  
void Console::draw_rect(int x, int y, int w, int h, int rgb)
{
    #if defined(DEBUG_CONSOLE)
    printf("[console] x:%d ,y:%d,w:%d,h:%d,rgb:%d\n", x , y, w,  h,  rgb);
    #endif

    if (!tempframebuffer || !tempframebuffer->address || screen_width == 0 && console_initialized)
    {
        printf("[console] (!tempframebuffer || !tempframebuffer->address || screen_width == 0) = false");
        return;
    }
    // Adjust coordinates to window offset
    x += window_x;
    y += window_y;
    
    // Clamp rectangle to window boundaries
    int clamp_x = clamp(x, window_x, window_x + window_width - 1);
    int clamp_y = clamp(y, window_y, window_y + window_height - 1);
    int clamp_x_end = clamp(x + w - 1, window_x, window_x + window_width - 1);
    int clamp_y_end = clamp(y + h - 1, window_y, window_y + window_height - 1);
    
    // Calculate clamped width and height
    int clamped_w = (clamp_x_end - clamp_x) + 1;
    int clamped_h = (clamp_y_end - clamp_y) + 1;
    
    // Skip if rectangle is outside bounds
    if (clamped_w <= 0 || clamped_h <= 0)
        return;
    
    // Use pitch (bytes per line) and bytes-per-pixel to compute addresses
    uint32_t bpp = (screen_width != 0) ? (pitch / screen_width) : 4;
    uint8_t *base = (uint8_t*)tempframebuffer->address;
    
    for (int i = 0; i < clamped_h; i++)
    {
        uint8_t *row = base + (size_t)(clamp_y + i) * (size_t)pitch;
        uint32_t *pixel = (uint32_t*)(row + (size_t)clamp_x * (size_t)bpp);
        for (int j = 0; j < clamped_w; j++)
        {
            pixel[j] = (uint32_t)rgb;
        }
    }
}

bool Console::scroll_console()
{
    if (!psf) return;
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
    draw_rect(0, window_height - CHAR_HEIGHT, window_width, CHAR_HEIGHT, console.bg_color);
    
    // Redraw all borders after scrolling
    draw_box(0, 0, window_width, border_thickness, border_color);                              // Top
    //draw_box(0, window_height - border_thickness, window_width, border_thickness, border_color); // Bottom
    draw_box(0, 0, border_thickness, window_height, border_color);                             // Left
    draw_box(window_width - border_thickness, 0, border_thickness, window_height, border_color); // Right

    
    decrement_cursor_y();
    return true;
}

void Console::erase_cursor()
{
    if (!psf) return;
    if (cursorHidden)
        return;
    draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, console.bg_color);
}

void Console::update_cursor()
{
    if (!psf) return;
    #if defined(DEBUG_CONSOLE)
    printf("[console] update_cursor\n");
    #endif
    if (cursorHidden)
    {        
        return;
    }
    if (cursorHidden)
    {        
        return;
    }
    #if defined(DEBUG_CONSOLE)
    printf("[console] cursor_position_x:%d ,window_width:%d ,textcolor:%lx\n", cursor_position_x , window_width, textcolor);
    #endif
    if (cursor_position_x >= window_width - CHAR_WIDTH)
    {
        bool neededScrolling = scroll_console();
        if (!neededScrolling)
        {
            increment_cursor_y();
        }
        move_to_line_start();
    }
    #if defined(DEBUG_CONSOLE)
    printf("[console] draw_rect");
    #endif

    if (cursor_position_x && cursor_position_y  && CHAR_HEIGHT && textcolor)
    {
    #if defined(DEBUG_CONSOLE)
    printf("[console] cursor_position_x:%d ,cursor_position_y:%d,CHAR_WIDTH:%d,CHAR_HEIGHT:%d,textcolor:%d\n", cursor_position_x , cursor_position_y, CHAR_WIDTH,  CHAR_HEIGHT,  textcolor);
    #endif

    draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT, textcolor);
    }
}

 
// Public Interface
 
void Console::initialize()
{
    // place cursor inside the border (avoid overlapping)
    cursor_position_x = CHAR_WIDTH; // start with one char margin from left
    cursor_position_y = border_thickness + 2; // just below the border
    screen_width_char = window_width / CHAR_WIDTH;
    textcolor = 0xBBE1FA;

    bool PSF_state = psfLoadDefaults();

    if (!PSF_state)
    {
        printf("[console:error] Failed to load PSF font, using default\n");
    }

    #if defined(DEBUG_CONSOLE)
        printf("[console] PSF font loaded successfully dumping first 32 bytes\n");
        for (int i = 0; i < 32; i++)
        {
            printf("%02X ", ((uint8_t*)psf)[i]);
        }
        printf("\n");
        printf("[console] tempframebuffer=%p psf=%p window_width=%u window_height=%u\n", tempframebuffer, psf, window_width, window_height);
        // One-time debug: print console window position and framebuffer info
        printf("[console:debug] Window position: (%u, %u), Size: %u x %u pixels\n", window_x, window_y, window_width, window_height);
        printf("[console:debug] Framebuffer address: %p, screen_width: %u, screen_height: %u, pitch: %u\n", tempframebuffer->address, screen_width, screen_height, pitch);
        // Compute the byte-per-pixel (guard against zero) and the framebuffer memory
        // address for the first character (window_x, window_y).
        uint32_t bpp = (screen_width != 0) ? (pitch / screen_width) : 4;
        void* first_char_addr = (void*)((uintptr_t)tempframebuffer->address + (uintptr_t)window_y * (uintptr_t)pitch + (uintptr_t)window_x * (uintptr_t)bpp);
        printf("[console:debug] First character will render at addr: %p (pixel: (%u, %u))\n", first_char_addr, window_x, window_y);
    
        printf("[console] bg_color: %lx, textcolor: %lx, console_initialized: %d\n", console.bg_color, console.textcolor, console_initialized);
    #endif
    console_initialized = true;
    clear_screen();
    printf("[console] bg_color: %lx, textcolor: %lx, console_initialized: %d\n", console.bg_color, console.textcolor, console_initialized);

}

void Console::clear_screen()
{
#if defined(DEBUG_CONSOLE)
    printf("[console] clear_screen\n");
#endif

    // place cursor inside the border to avoid overlapping
    cursor_position_x = CHAR_WIDTH;
    cursor_position_y = border_thickness + 2;
    #if defined(DEBUG_GUI)
    printf("[console] cursor_position_x:%d ,cursor_position_y:%d ,window_width:%d , window_height:%d \n", cursor_position_x, cursor_position_y , window_width, window_height);
    #endif
    draw_rect(0, 0, window_width, window_height,console.bg_color);
    // Redraw frame and border
    draw_box(0, 0, window_width, window_height,console.bg_color);
    //draw_box(0, 0, window_width, border_thickness, border_color);
    //draw_box(0, window_height - border_thickness, window_width, border_thickness, border_color);//bottom
    //draw_box(0, 0, border_thickness, window_height, border_color);
    //draw_box(window_width - border_thickness, 0, border_thickness, window_height, border_color);

    update_cursor();
}

void Console::set_bg_color(int rgb)
{
   console.bg_color = rgb;
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


void Console::draw_character(int charnum)
{
    if (!psf) return;
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
        draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT,console.bg_color);
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
        draw_rect(cursor_position_x, cursor_position_y, CHAR_WIDTH, CHAR_HEIGHT,console.bg_color);
        break;
    case '\t':
        for (int i = 0; i < 4; i++)
            draw_character(' ');
        break;
    case ' ':
        erase_cursor();
        psfPutC(charnum, window_x + cursor_position_x, window_y + cursor_position_y, textcolor,console.bg_color);
        is_char[cursor_position_x / CHAR_WIDTH][cursor_position_y / CHAR_HEIGHT] = 0;
        increment_cursor_x();
        break;
    default:
        erase_cursor();
        psfPutC(charnum, window_x + cursor_position_x, window_y + cursor_position_y, textcolor,console.bg_color);
        is_char[cursor_position_x / CHAR_WIDTH][cursor_position_y / CHAR_HEIGHT] = 1;
        increment_cursor_x();
        break;
    }
    update_cursor();
}

void Console::print_char(char character)
{
    spinlock_acquire(&LOCK_CONSOLE);
    draw_character(character);
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


void drawCharacter(int charnum)
{
    console.draw_character(charnum);
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

void clear_screen()
{
    console.clear_screen();
}

void printfch(char character)
{
    console.print_char(character);
}

extern "C" void putchar_(char c)
{
    serial_write(c);
    if (!psf) return;
    if (console_initialized && tempframebuffer && tempframebuffer->address && screen_width != 0)
    {
        printfch(c);
    }
}
