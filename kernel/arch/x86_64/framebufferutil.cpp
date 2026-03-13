#include <framebufferutil.h>

#include <bootloader.h>
#include <liballoc.h>

volatile struct limine_framebuffer tempframebuffer_data;

volatile struct limine_framebuffer *framebuffer;
volatile struct limine_framebuffer *tempframebuffer = &tempframebuffer_data;

uint64_t screen_width;
uint64_t screen_height;


volatile uint64_t buffer_size;


volatile uint32_t *buffer = nullptr;

uint32_t color = 0xffffff;

uint32_t SCREEN_WIDTH;
uint32_t SCREEN_HEIGHT;


int framebuffer_initialize()
{

#if defined(DEBUG_FRAMEBUFFER)
    printf("[framebuffer_initialize] begin\n");
#endif

    framebuffer = bootloader.framebuffer;

#if defined(DEBUG_FRAMEBUFFER)
printf("[framebuffer] struct=%lx pixel_addr=%lx\n",
       (uint64_t)framebuffer,
       (uint64_t)framebuffer->address);
#endif

    if (framebuffer == nullptr) {
        printf("[framebuffer] FATAL: bootloader.framebuffer is null!\n");
        return -1;
    }
    screen_width = framebuffer->width;
    screen_height = framebuffer->height;

    
    tempframebuffer->width = framebuffer->width;
    tempframebuffer->height = framebuffer->height;
    tempframebuffer->pitch = framebuffer->pitch;
    
    buffer_size = (tempframebuffer->height * tempframebuffer->pitch);
    // Dynamically allocate the back buffer
    buffer = (volatile uint32_t*)malloc(buffer_size);
    if (buffer == nullptr) {
        printf("[framebuffer] FATAL: failed to allocate back buffer!\n");
        return -1;
    }

    if (buffer == nullptr)
    {
        // FALLBACK: draw directly to real framebuffer, no back buffer
        // This fixes the black screen while you fix malloc
        tempframebuffer->address = framebuffer->address;
        // also copy the real fb struct address fields
        SCREEN_WIDTH  = framebuffer->width;
        SCREEN_HEIGHT = framebuffer->height;
        return 0;  // continue without back buffer
    }

    tempframebuffer->address = buffer;

    SCREEN_WIDTH  = framebuffer->width;
    SCREEN_HEIGHT = framebuffer->height;

    printf("limine framebuffer initialized.\n");

#if defined(DEBUG_FRAMEBUFFER)
    printf("[framebuffer_initialize] success buffer_size=%d buffer=%lx\n", (unsigned long long)buffer_size, (void*)buffer);
    printf("[framebuffer_initialize] end\n");
#endif

    return 0;
}

uint64_t screen_width_cap(uint64_t number)
{
    if (number >= screen_width)
    {
        return screen_width;
    }
    return number;
}

uint64_t screen_height_cap(uint64_t number)
{
    if (number >= screen_height)
    {
        return screen_height;
    }
    return number;
}


void draw_vertical_line(vector2 start, vector2 end)
    {
        struct vector2 _end;
        _end.y = screen_height_cap(end.y);
        volatile uint32_t *fb_ptr = tempframebuffer->address;

        for (size_t i = start.y; i < _end.y; i++) 
        {
        fb_ptr[i * (tempframebuffer->pitch / 4) + start.x] = color;
        }
    }

void draw_horizontal_line(vector2 start, vector2 end)
    {
        struct vector2 _end;
        _end.x = screen_width_cap(end.x);
        volatile uint32_t *fb_ptr = tempframebuffer->address;

        for (size_t i = start.x; i < _end.x; i++) 
        {
        fb_ptr[start.y * (tempframebuffer->pitch / 4) + i] = color;
        }
    }

void draw_rectangle(vector2 start, vector2 end)
{
    struct vector2 line_start = {start.x,start.y};
    struct vector2 line_end = {start.x,end.y};
    draw_vertical_line(line_start, line_end);
    line_start = {end.x,start.y};
    line_end = {end.x,end.y};
    draw_vertical_line(line_start, line_end);

    line_start = {start.x,start.y};
    line_end = {end.x,start.y};
    draw_horizontal_line(line_start, line_end);
    line_start = {start.x,end.y};
    line_end = {end.x,end.y};
    draw_horizontal_line(line_start, line_end);
}

void clear()
{
    memset((void*)tempframebuffer->address, 0, buffer_size);
}


void test_framebuffer(uint32_t test_color)
{

    printf("testing framebuffer...\n");

    color = test_color;

     // framebuffer model is assumed to be RGB with 32-bit pixels
    // for (size_t j = 0; j < 10; j++) {
    //     for (size_t i = 0; i < 100; i++) {
    //         volatile uint32_t *fb_ptr = tempframebuffer->address;
    //         fb_ptr[i * (tempframebuffer->pitch / 4) + i] = color;
    //     }
    // }

    struct vector2 line_start = {0,0};
    struct vector2 line_end = {0,0};

    int x_step = 7;
    int y_step = 7;
    uint32_t x = 100;
    uint32_t y = 100;
    uint32_t cube_size = 73;
    line_start = {x_step,y_step};

    while(true)
    {
        if ((line_start.y + cube_size + y_step) > screen_height)
        {
            y_step = y_step * (-1);
        }

        if ((line_start.x + cube_size + x_step) > (screen_width - 1))
        {
            x_step = x_step * (-1);
        }

        if ((line_start.y + y_step) <= 0)
        {
            y_step = y_step * (-1);
        }

        if ((line_start.x + x_step) <= 0)
        {
            x_step = x_step * (-1);
        }

        line_start.x = line_start.x + x_step;
        line_start.y = line_start.y + y_step;
        line_end = {line_start.x + cube_size,line_start.y + cube_size};
        draw_rectangle(line_start, line_end);

        copy_buffer_to_screan();

        //clear();
    }
}

void copy_buffer_to_screan()
{
    checkpoint(35, 0xFFFF00); // yellow at pos 820 — entered copy
    if (tempframebuffer->address)
    {
        checkpoint(36, 0xFF6600); // orange at pos 840 — about to memcpy
        memcpy((void*)framebuffer->address,
               (void*)tempframebuffer->address,
               buffer_size);
        checkpoint(37, 0x00FF00); // green at pos 860 — memcpy done
    }
}

void draw_pixel(int x, int y, uint32_t rgb)
{                         
    // Bounds check - critical to prevent buffer overflow
    if (x < 0 || y < 0) return;
    if ((uint64_t)x >= screen_width || (uint64_t)y >= screen_height) return;

    volatile uint32_t *tfb_ptr = tempframebuffer->address;
    tfb_ptr[x + y * (tempframebuffer->pitch / 4)] = rgb;
}



#if defined(DEBUG_FRAMEBUFFER)
void early_debug_bars()
{
    struct limine_framebuffer* fb = bootloader.framebuffer;
    if (!fb || !fb->address) return;

    uint8_t*  base  = (uint8_t*)fb->address;
    uint64_t  pitch = fb->pitch;
    uint64_t  w     = fb->width;
    uint64_t  h     = fb->height;
    uint32_t  bpp   = fb->bpp / 8;   // bytes per pixel

    // Helper: fill a horizontal band with a solid color
    auto fill_band = [&](uint64_t y_start, uint64_t band_h, uint32_t color) {
        for (uint64_t y = y_start; y < y_start + band_h && y < h; y++) {
            uint32_t* row = (uint32_t*)(base + y * pitch);
            for (uint64_t x = 0; x < w; x++)
                row[x] = color;
        }
    };

    // Paint whole screen magenta so we know SOMETHING wrote
    fill_band(0, h, 0xFF00FF);

    // Encode width  as a green bar (1 pixel wide per 4 pixels of width)
    // e.g. 1920 -> 480px bar,  1366 -> 341px bar
    uint64_t w_bar = w / 4;
    for (uint64_t y = 10; y < 30; y++) {
        uint32_t* row = (uint32_t*)(base + y * pitch);
        for (uint64_t x = 0; x < w_bar && x < w; x++)
            row[x] = 0x00FF00;
    }

    // Encode height as a blue bar (1 pixel per 4 pixels of height)
    // e.g. 1080 -> 270px,  768 -> 192px
    uint64_t h_bar = h / 4;
    for (uint64_t y = 40; y < 60; y++) {
        uint32_t* row = (uint32_t*)(base + y * pitch);
        for (uint64_t x = 0; x < h_bar && x < w; x++)
            row[x] = 0x0000FF;
    }

    // Encode pitch as a red bar (1 pixel per 16 bytes of pitch)
    // e.g. pitch=7680 -> 480px,  pitch=5504 -> 344px
    uint64_t p_bar = pitch / 16;
    for (uint64_t y = 70; y < 90; y++) {
        uint32_t* row = (uint32_t*)(base + y * pitch);
        for (uint64_t x = 0; x < p_bar && x < w; x++)
            row[x] = 0xFF0000;
    }

    // Encode bpp as a white bar (20px per byte, so 4 bytes = 80px)
    uint64_t bpp_bar = bpp * 20;
    for (uint64_t y = 100; y < 120; y++) {
        uint32_t* row = (uint32_t*)(base + y * pitch);
        for (uint64_t x = 0; x < bpp_bar && x < w; x++)
            row[x] = 0xFFFFFF;
    }

        // Encode bpp as a white bar (20px per byte, so 4 bytes = 80px)
    uint64_t full_bar = w;
    for (uint64_t y = 100; y < 120; y++) {
        uint32_t* row = (uint32_t*)(base + y * pitch);
        for (uint64_t x = 0; x < full_bar && x < w; x++)
            row[x] = 0xFFFFFF;
    }

}
#endif

void checkpoint(int n, uint32_t color)
{
    #if defined(DEBUG_FRAMEBUFFER)
    struct limine_framebuffer* fb = bootloader.framebuffer;
    if (!fb || !fb->address) return;
    uint8_t* base = (uint8_t*)fb->address;
    int x_start = n * 20;
    for (uint64_t y = 0; y < fb->height; y++) {
        uint32_t* row = (uint32_t*)(base + y * fb->pitch);
        for (int x = x_start; x < x_start + 18 && x < (int)fb->width; x++)
            row[x] = color;
    }
    #endif
    return;

}