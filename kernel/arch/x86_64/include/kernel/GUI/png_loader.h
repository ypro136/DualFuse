#pragma once
#include <cstdint>

bool png_load_wallpaper(const unsigned char* png_data, unsigned int png_len);

void png_blit_wallpaper();

bool png_wallpaper_loaded();