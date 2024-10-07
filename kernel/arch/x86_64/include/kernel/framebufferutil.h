
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <string.h>

#include <utility/hcf.hpp>


#include <limine.h>

typedef struct vector2{
    uint64_t x;
    uint64_t y;
};

int framebuffer_initialize();

void test_framebuffer(uint32_t test_color);