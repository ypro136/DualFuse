
#pragma once

#include <stdio.h>
#include <stdlib.h>
 #include <types.h>

#include <string.h>

#include <hcf.hpp>


#include <limine.h>

extern volatile struct limine_framebuffer *tempframebuffer;

extern uint64_t screen_width;
extern uint64_t screen_height;

extern uint64_t pitch;




typedef struct vector2{
    uint64_t x;
    uint64_t y;
};

int framebuffer_initialize();

void draw_pixel(int x,int y,int rgb);
void copy_buffer_to_screan();


void test_framebuffer(uint32_t test_color);