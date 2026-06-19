#include <GUI_input.h>
#include <gui_primitives.h>
#include <GUI.h>
#include <cstdint>
#include <cstring>
#include <timer.h>
#include <console.h>
#include <bootloader.h> 
#include <mouse.h>
#include <panel.h>
#include <file_explorer.h>
#include <calculator.h>
#include <text_editor.h>
#include <image_viewer.h>
#include <task_manager.h>

typedef void (*gui_window_mouse_handler_fn)(void* ctx, int x, int y, bool left, bool right);
typedef void (*gui_window_key_handler_fn)(void* ctx, char c);

struct GUIWindowInputHandlers {
    gui_window_mouse_handler_fn on_mouse;
    gui_window_key_handler_fn   on_key;
};

static constexpr int GUI_WINDOW_TYPE_TABLE_SIZE = WINDOW_TYPE_TASK_MANAGER + 1;
static GUIWindowInputHandlers window_input_handler_table[GUI_WINDOW_TYPE_TABLE_SIZE];

bool should_exit      = false;
bool old_clickedLeft  = false;
bool old_clickedRight = false;

int            x_offset_to_window = 0;
int            y_offset_to_window = 0;
XPWindow*      grabbed_window     = NULL;
XPButton*      pressed_button     = NULL;
XPDesktopIcon* pressed_icon       = NULL;

XPWindow*      resize_window          = NULL;
int            resize_start_mouse_x   = 0;
int            resize_start_mouse_y   = 0;
int            resize_start_width     = 0;
int            resize_start_height    = 0;

bool mouse_down_left()  { return  clickedLeft && !old_clickedLeft;  }
bool mouse_up_left()    { return !clickedLeft &&  old_clickedLeft;  }
bool mouse_down_right() { return  clickedRight && !old_clickedRight; }
bool mouse_up_right()   { return !clickedRight &&  old_clickedRight; }

static void mouse_update()
{
    old_clickedLeft  = clickedLeft;
    old_clickedRight = clickedRight;
}

static void gui_input_mouse_adapter_explorer(void* ctx, int x, int y, bool left, bool right)
{
    fe_handle_mouse(static_cast<XPFileExplorer*>(ctx), x, y, left, right);
}

static void gui_input_mouse_adapter_calc(void* ctx, int x, int y, bool left, bool right)
{
    if (left)
        calc_handle_mouse(static_cast<XPCalculator*>(ctx), x, y);
}

static void gui_input_mouse_adapter_image_viewer(void* ctx, int x, int y, bool left, bool right)
{
    image_viewer_handle_mouse(ctx, x, y, left, right);
}

static void gui_input_mouse_adapter_text_editor(void* ctx, int x, int y, bool left, bool right)
{
    text_editor_handle_mouse(static_cast<XPTextEditor*>(ctx), x, y, left, right);
}

static void gui_input_mouse_adapter_task_manager(void* ctx, int x, int y, bool left, bool right)
{
    task_manager_handle_mouse(static_cast<TaskManagerState*>(ctx), x, y, left, right);
}

static void gui_input_key_adapter_calc(void* ctx, char c)
{
    calc_input(static_cast<XPCalculator*>(ctx), c);
}

static void gui_input_key_adapter_text_editor(void* ctx, char c)
{
    text_editor_handle_key_input(static_cast<XPTextEditor*>(ctx), c);
}

static void gui_input_key_adapter_task_manager(void* ctx, char c)
{
    task_manager_handle_keyboard(static_cast<TaskManagerState*>(ctx), c);
}

void gui_input_init()
{
    memset(window_input_handler_table, 0, sizeof(window_input_handler_table));

    window_input_handler_table[WINDOW_TYPE_EXPLORER]     = { gui_input_mouse_adapter_explorer,     nullptr };
    window_input_handler_table[WINDOW_TYPE_CALC]         = { gui_input_mouse_adapter_calc,          gui_input_key_adapter_calc };
    window_input_handler_table[WINDOW_TYPE_IMAGE_VIEWER] = { gui_input_mouse_adapter_image_viewer,  nullptr };
    window_input_handler_table[WINDOW_TYPE_TEXT_EDITOR]  = { gui_input_mouse_adapter_text_editor,   gui_input_key_adapter_text_editor };
    window_input_handler_table[WINDOW_TYPE_TASK_MANAGER] = { gui_input_mouse_adapter_task_manager,  gui_input_key_adapter_task_manager };
}

static void dispatch_to_active_window(bool left_clicked, bool right_clicked)
{
    if (!active_xp_window || !active_xp_window->context) return;

    int   type = active_xp_window->window_type;
    void* ctx  = active_xp_window->context;

    if (type >= GUI_WINDOW_TYPE_TABLE_SIZE) return;
    if (!window_input_handler_table[type].on_mouse) return;

    window_input_handler_table[type].on_mouse(ctx, mouse_position_x, mouse_position_y, left_clicked, right_clicked);
}
 
void GUI_dispatch_key(char c) 
{
    if (!active_xp_window || !active_xp_window->context) return;

    int   type = active_xp_window->window_type;
    void* ctx  = active_xp_window->context;

    if (type >= GUI_WINDOW_TYPE_TABLE_SIZE) return;
    if (!window_input_handler_table[type].on_key) return;

    window_input_handler_table[type].on_key(ctx, c);
}

bool GUI_input_loop()
{
    bool left_clicked  = mouse_down_left();
    bool right_clicked = mouse_down_right();

    if (start_menu_handle_mouse(mouse_position_x, mouse_position_y, left_clicked))
    {
        mouse_update();
        return should_exit;
    }

    if (left_clicked)
    {
        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] GUI_input_loop: mouse down at x:%d y:%d\n",
               mouse_position_x, mouse_position_y);
        #endif
        XPWindow* win = get_window_at(mouse_position_x, mouse_position_y);
        if (win != NULL && !win->minimized)
        {
            #if defined(DEBUG_GUI)
            printf("[DEBUG_GUI] GUI_input_loop: clicked window:%s\n",
                   win->title ? win->title : "NULL");
            #endif
            set_active_xp_window(win);

            XPButton* win_btn = is_mouse_on_window_button(
                win, mouse_position_x, mouse_position_y);

            if (win_btn != NULL)
            {
                #if defined(DEBUG_GUI)
                printf("[DEBUG_GUI] GUI_input_loop: pressed window button label:%s\n",
                       win_btn->label ? win_btn->label : "NULL");
                #endif
                pressed_button          = win_btn;
                pressed_button->pressed = true;
            }
            else if (is_mouse_on_window_title_bar(
                         win, mouse_position_x, mouse_position_y))
            {
                #if defined(DEBUG_GUI)
                printf("[DEBUG_GUI] GUI_input_loop: grabbing window for drag\n");
                #endif
                grabbed_window     = win;
                x_offset_to_window = mouse_position_x - grabbed_window->x;
                y_offset_to_window = mouse_position_y - grabbed_window->y;
            }
            else if (is_mouse_on_window_resize_triangle(
                         win, mouse_position_x, mouse_position_y))
            {
                #if defined(DEBUG_GUI)
                printf("[DEBUG_GUI] GUI_input_loop: resize grab on window:%s\n",
                       win->title ? win->title : "NULL");
                #endif
                resize_window        = win;
                resize_start_mouse_x = mouse_position_x;
                resize_start_mouse_y = mouse_position_y;
                resize_start_width   = win->width;
                resize_start_height  = win->height;
            }
            else
            {
                dispatch_to_active_window(true, false);
            }
        }
        else
        {
            XPButton* btn = get_button_at(mouse_position_x, mouse_position_y);
            if (btn != NULL)
            {
                #if defined(DEBUG_GUI)
                printf("[DEBUG_GUI] GUI_input_loop: pressed standalone button label:%s\n",
                       btn->label ? btn->label : "NULL");
                #endif
                pressed_button          = btn;
                pressed_button->pressed = true;
            }
            else
            {
                XPDesktopIcon* icon = get_desktop_icon_at(
                    mouse_position_x, mouse_position_y);
                if (icon != NULL)
                {
                    #if defined(DEBUG_GUI)
                    printf("[DEBUG_GUI] GUI_input_loop: pressed desktop icon label:%s\n",
                           icon->label ? icon->label : "NULL");
                    #endif
                    pressed_icon          = icon;
                    pressed_icon->pressed = true;
                    draw_desktop_icon(pressed_icon);
                }
            }
        }
    }

    if (right_clicked)
    {
        XPWindow* win = get_window_at(mouse_position_x, mouse_position_y);
        if (win != NULL && !win->minimized)
        {
            set_active_xp_window(win);
            dispatch_to_active_window(false, true);
        }
    }

    if (mouse_up_left())
    {
        #if defined(DEBUG_GUI)
        printf("[DEBUG_GUI] GUI_input_loop: mouse up\n");
        #endif
        grabbed_window = NULL;
        resize_window  = NULL;

        if (pressed_button != NULL)
        {
            pressed_button->pressed = false;
            if (pressed_button->function != NULL)
            {
                #if defined(DEBUG_GUI)
                printf("[DEBUG_GUI] GUI_input_loop: firing button label:%s\n",
                       pressed_button->label ? pressed_button->label : "NULL");
                #endif
                pressed_button->function(pressed_button->window);
            }
            pressed_button = NULL;
        }

        if (pressed_icon != NULL)
        {
            pressed_icon->pressed = false;
            draw_desktop_icon(pressed_icon);

            XPDesktopIcon* fired_icon = pressed_icon;
            pressed_icon = NULL;

            if (desktop_icon_hit_test(fired_icon, mouse_position_x, mouse_position_y))
            {
                if (fired_icon->on_click != NULL)
                {
                    #if defined(DEBUG_GUI)
                    printf("[DEBUG_GUI] GUI_input_loop: firing desktop icon label:%s\n",
                           fired_icon->label ? fired_icon->label : "NULL");
                    #endif
                    fired_icon->on_click();
                }
            }
        }
    }

    if (clickedLeft && grabbed_window != NULL)
    {
        move_xp_window(grabbed_window,
                       mouse_position_x - x_offset_to_window,
                       mouse_position_y - y_offset_to_window);
    }

    if (clickedLeft && resize_window != NULL)
    {
        int new_width  = resize_start_width  + (mouse_position_x - resize_start_mouse_x);
        int new_height = resize_start_height + (mouse_position_y - resize_start_mouse_y);
        resize_xp_window(resize_window, new_width, new_height);
    }

    update_all_xp_panels(mouse_position_x, mouse_position_y);

    mouse_update();

    return should_exit;
}