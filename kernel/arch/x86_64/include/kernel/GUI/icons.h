#ifndef ICONS_H
#define ICONS_H

#include <png_loader.h>

extern Image folder_icon;
extern Image file_icon;
extern Image terminal_icon;
extern Image calculator_icon;
extern Image logo_image;

// Load all icons from /assets/ (call once after filesystem_mount)
void load_all_icons(void);

// Free all icon resources (optional, for cleanup)
void free_all_icons(void);

#endif