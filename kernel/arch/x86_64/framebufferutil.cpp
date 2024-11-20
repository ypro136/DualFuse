#include <framebufferutil.h>

#include <bootloader.h>

volatile struct limine_framebuffer tempframebuffer_data;

volatile struct limine_framebuffer *framebuffer;
volatile struct limine_framebuffer *tempframebuffer = &tempframebuffer_data;

uint64_t screen_width;
uint64_t screen_height;

uint64_t pitch;


volatile uint64_t buffer_size;


volatile uint32_t buffer[2000 * 2000];

uint32_t color = 0xffffff;





int framebuffer_initialize()
{

    framebuffer = bootloader.framebuffer;
    screen_width = framebuffer->width;
    screen_height = framebuffer->height;
    pitch = framebuffer->pitch;

    buffer_size = (screen_width * screen_height * 4);

    tempframebuffer->width = framebuffer->width;
    tempframebuffer->height = framebuffer->height;
    tempframebuffer->pitch = framebuffer->pitch;

    memset(buffer, 0, buffer_size);
    tempframebuffer->address = buffer;

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
        for (size_t i = 0; i < ((screen_width - 1) * (screen_height - 1)); i++) 
        {
            volatile uint32_t *fb_ptr = tempframebuffer->address;
            fb_ptr[i] = 0x000000;
        }
}


// void test_framebuffer(uint32_t test_color)
// {

//     printf("testing framebuffer...\n");

//     color = test_color;

//      // framebuffer model is assumed to be RGB with 32-bit pixels
//     for (size_t j = 0; j < 10; j++) {
//         for (size_t i = 0; i < 100; i++) {
//             volatile uint32_t *fb_ptr = tempframebuffer->address;
//             fb_ptr[i * (tempframebuffer->pitch / 4) + i] = color;
//         }
//     }

//     struct vector2 line_start = {0,0};
//     struct vector2 line_end = {0,0};

//     int x_step = 7;
//     int y_step = 7;
//     uint32_t x = 100;
//     uint32_t y = 100;
//     uint32_t cube_size = 73;
//     line_start = {x_step,y_step};

//     while(true)
//     {
//         if ((line_start.y + cube_size + y_step) > screen_height)
//         {
//             y_step = y_step * (-1);
//         }

//         if ((line_start.x + cube_size + x_step) > (screen_width - 1))
//         {
//             x_step = x_step * (-1);
//         }

//         if ((line_start.y + y_step) <= 0)
//         {
//             y_step = y_step * (-1);
//         }

//         if ((line_start.x + x_step) <= 0)
//         {
//             x_step = x_step * (-1);
//         }

//         line_start.x = line_start.x + x_step;
//         line_start.y = line_start.y + y_step;
//         line_end = {line_start.x + cube_size,line_start.y + cube_size};
//         draw_rectangle(line_start, line_end);

//         copy_buffer_to_screan();

//         clear();
//     }
// }

void copy_buffer_to_screan()
{
    volatile uint32_t *fb_ptr = framebuffer->address;
    volatile uint32_t *tfb_ptr = tempframebuffer->address;
    memcpy(fb_ptr, tfb_ptr, buffer_size);
}

void draw_pixel(int x,int y,int rgb) 
{                         
    volatile int *tfb_ptr = tempframebuffer->address;                                                                                      
    tfb_ptr[((x) + (y) * screen_width)] = (rgb);                     

}