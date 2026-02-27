#include <GUI_input.h>

#include <gui_primitives.h>
#include <GUI.h>

#include <cstdint>
#include <cstring>

#include <timer.h>
#include <console.h>
#include <bootloader.h>
#include <mouse.h>

bool should_exit = false;

// extern bool clickedLeft;
// extern bool clickedRight;
// extern int mouse_position_x;
// extern int mouse_position_y;

bool old_clickedLeft = false;
bool old_clickedRight = false;

int x_offset_to_window = 0;
int y_offset_to_window = 0;

XPWindow* grabed_window = NULL;

bool mouse_down_left()
{
    bool pressed = clickedLeft && !old_clickedLeft;
    return pressed;
}

bool mouse_up_left()
{
    bool released = !clickedLeft && old_clickedLeft;
    return released;
}


// Call this at the END of every frame to update old state
void mouse_update()
{
    old_clickedLeft  = clickedLeft;
    old_clickedRight = clickedRight;
}

bool GUI_input_loop()
{

    if (mouse_down_left())
    {
        XPWindow* win = get_window_at(mouse_position_x, mouse_position_y);
        if (win != NULL)
        {
            win->active = true;
            if (is_mouse_on_window_title_bar(win, mouse_position_x, mouse_position_y))
            {
                // mouse is on that window's title bar
                grabed_window = win;
                x_offset_to_window = (mouse_position_x - grabed_window->x);
                y_offset_to_window = (mouse_position_y - grabed_window->y);
            }

        }
    }

    if (mouse_up_left())
    {
        grabed_window = NULL;
    }

    if (clickedLeft && grabed_window != NULL)
    {   
        move_xp_window(grabed_window, (mouse_position_x - x_offset_to_window), (mouse_position_y - y_offset_to_window));
    }
    
    mouse_update();
    return should_exit;
}