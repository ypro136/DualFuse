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
XPDesktopIcon* pressed_icon = NULL;

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

            XPButton* win_btn = is_mouse_on_window_button(win, mouse_position_x, mouse_position_y);
            if (win_btn != NULL)
            {
#if defined(DEBUG_GUI)
                printf("[DEBUG_GUI] GUI_input_loop: pressed window button label:%s\n", win_btn->label ? win_btn->label : "NULL");
#endif
                pressed_button          = win_btn;
                pressed_button->pressed = true;
            }
            else if (is_mouse_on_window_title_bar(win, mouse_position_x, mouse_position_y))
            {
#if defined(DEBUG_GUI)
                printf("[DEBUG_GUI] GUI_input_loop: grabbing window for drag\n");
#endif
                grabbed_window     = win;
                x_offset_to_window = mouse_position_x - grabbed_window->x;
                y_offset_to_window = mouse_position_y - grabbed_window->y;
            }
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
            else
            {
                // Check desktop icons
                XPDesktopIcon* icon = get_desktop_icon_at(mouse_position_x, mouse_position_y);
                if (icon != NULL)
                {
#if defined(DEBUG_GUI)
                    printf("[DEBUG_GUI] GUI_input_loop: pressed desktop icon label:%s\n", icon->label ? icon->label : "NULL");
#endif
                    pressed_icon          = icon;
                    pressed_icon->pressed = true;
                    draw_desktop_icon(pressed_icon); // redraw sunken
                }
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

        if (pressed_icon != NULL)
        {
            pressed_icon->pressed = false;
            draw_desktop_icon(pressed_icon); // redraw normal

            XPDesktopIcon* fired_icon = pressed_icon;
            pressed_icon = NULL;

            // Only fire if mouse is still over the icon (click, not drag-off)
            if (desktop_icon_hit_test(fired_icon, mouse_position_x, mouse_position_y))
            {
                if (fired_icon->on_click != NULL)
                {
                    #if defined(DEBUG_GUI)
                    printf("[DEBUG_GUI] GUI_input_loop: firing desktop icon on_click label:%s\n", fired_icon->label ? fired_icon->label : "NULL");
                    #endif
                    fired_icon->on_click();
                }
                else
                {
                    #if defined(DEBUG_GUI)
                    printf("[DEBUG_GUI] GUI_input_loop: desktop icon has no on_click\n");
                    #endif
                }
            }
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