
#pragma once

#include <stdio.h>
#include <stdlib.h>
 #include <types.h>

#include <string.h>

#include <hcf.hpp>


#include <limine.h>

extern volatile struct limine_framebuffer *tempframebuffer;
extern volatile struct limine_framebuffer tempframebuffer_data;


extern uint64_t screen_width;
extern uint64_t screen_height;

extern uint64_t pitch;

extern uint32_t SCREEN_WIDTH;
extern uint32_t SCREEN_HEIGHT;

extern volatile uint32_t *buffer;

extern volatile uint64_t buffer_size;




typedef struct vector2{
    uint64_t x;
    uint64_t y;
};

int framebuffer_initialize();

void draw_pixel(int x, int y, uint32_t rgb);
void copy_buffer_to_screan();


void test_framebuffer(uint32_t test_color);