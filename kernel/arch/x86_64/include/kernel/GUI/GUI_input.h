#pragma once

#include <gui_primitives.h>
#include <GUI.h>
#include <cstdint>
#include <cstring>
#include <timer.h>
#include <console.h>
#include <bootloader.h>
#include <mouse.h>

bool mouse_down_left();
bool mouse_up_left();
bool mouse_down_right();
bool mouse_up_right();

bool GUI_input_loop();
void GUI_dispatch_key(char c);