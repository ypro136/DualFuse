#include <GUI_input.h>
#include <gui_primitives.h>
#include <GUI.h>
#include <cstdint>
#include <cstring>
#include <timer.h>
#include <console.h>
#include <bootloader.h>
#include <mouse.h>

bool should_exit      = false;
bool old_clickedLeft  = false;
bool old_clickedRight = false;

int       x_offset_to_window = 0;
int       y_offset_to_window = 0;
XPWindow* grabbed_window     = NULL;
XPButton* pressed_button     = NULL;

bool mouse_down_left()
{
    return clickedLeft && !old_clickedLeft;
}

bool mouse_up_left()
{
    return !clickedLeft && old_clickedLeft;
}

void mouse_update()
{
    old_clickedLeft  = clickedLeft;
    old_clickedRight = clickedRight;
}

bool GUI_input_loop()
{
    if (mouse_down_left())
    {
        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] GUI_input_loop: mouse down at x:%d y:%d\n", mouse_position_x, mouse_position_y);
        #endif

        XPWindow* win = get_window_at(mouse_position_x, mouse_position_y);

        if (win != NULL && !win->minimized)
        {
            #if defined(DEBUG_GUI)
            printf("[DEBUG_GUI] GUI_input_loop: clicked window:%s\n", win->title ? win->title : "NULL");
            #endif

            set_active_xp_window(win);

            // Check window title bar buttons first
            XPButton* win_btn = is_mouse_on_window_button(win, mouse_position_x, mouse_position_y);
            if (win_btn != NULL)
            {
                #if defined(DEBUG_GUI)
                printf("[DEBUG_GUI] GUI_input_loop: pressed window button label:%s\n", win_btn->label ? win_btn->label : "NULL");
                #endif
                pressed_button         = win_btn;
                pressed_button->pressed = true;
            }
            else if (is_mouse_on_window_title_bar(win, mouse_position_x, mouse_position_y))
            {
                // Grab window for dragging
                #if defined(DEBUG_GUI)
                printf("[DEBUG_GUI] GUI_input_loop: grabbing window for drag\n");
                #endif
                grabbed_window       = win;
                x_offset_to_window   = mouse_position_x - grabbed_window->x;
                y_offset_to_window   = mouse_position_y - grabbed_window->y;
            }
            // else: clicked window body, do nothing for now
        }
        else
        {
            // Check standalone buttons (taskbar Start, etc.)
            XPButton* btn = get_button_at(mouse_position_x, mouse_position_y);
            if (btn != NULL)
            {
                #if defined(DEBUG_GUI)
                printf("[DEBUG_GUI] GUI_input_loop: pressed standalone button label:%s\n", btn->label ? btn->label : "NULL");
                #endif
                pressed_button          = btn;
                pressed_button->pressed = true;
            }
        }
    }

    if (mouse_up_left())
    {
        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] GUI_input_loop: mouse up\n");
        #endif

        grabbed_window = NULL;

        if (pressed_button != NULL)
        {
            pressed_button->pressed = false;

            if (pressed_button->function != NULL)
            {
                #if defined(DEBUG_GUI)
                printf("[DEBUG_GUI] GUI_input_loop: firing button function label:%s\n", pressed_button->label ? pressed_button->label : "NULL");
                #endif
                pressed_button->function(pressed_button->window);
            }
            else
            {
                #if defined(DEBUG_GUI)
                printf("[DEBUG_GUI] GUI_input_loop: button has no function\n");
                #endif
            }

            pressed_button = NULL;
        }
    }

    // Drag window
    if (clickedLeft && grabbed_window != NULL)
    {
        move_xp_window(grabbed_window,
                       mouse_position_x - x_offset_to_window,
                       mouse_position_y - y_offset_to_window);
    }

    mouse_update();
    return should_exit;
}