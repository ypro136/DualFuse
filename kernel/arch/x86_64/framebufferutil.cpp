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

Spinlock gui_lock = {0};


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
    printf("[framebuffer] calculated buffer_size: %d bytes\n", (unsigned long long)buffer_size);
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

void clear() {
    spinlock_acquire(&gui_lock);
    memset((void*)tempframebuffer->address, 0, buffer_size);
    spinlock_release(&gui_lock);
}


void test_framebuffer(uint32_t test_color)
{

    printf("testing framebuffer...\n");

    color = test_color;

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


void copy_buffer_to_screan() {
    if (!tempframebuffer->address) return;
    spinlock_acquire(&gui_lock);
    memcpy((void*)framebuffer->address,
           (void*)tempframebuffer->address,
           buffer_size);
    spinlock_release(&gui_lock);
}

void draw_pixel(int x, int y, uint32_t rgb)
{                         
    // Bounds check - critical to prevent buffer overflow
    if (x < 0 || y < 0) return;
    if ((uint64_t)x >= screen_width || (uint64_t)y >= screen_height) return;

    volatile uint32_t *tfb_ptr = tempframebuffer->address;
    tfb_ptr[x + y * (tempframebuffer->pitch / 4)] = rgb;
}

