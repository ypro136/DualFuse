#pragma once

#include <cstdint>
#include <gui_defs.h>
#include <psf.h>

#define TEXT_EDITOR_MAX_LINES                 512
#define TEXT_EDITOR_MAX_LINE_LENGTH           256
#define TEXT_EDITOR_CONTEXT_MENU_WIDTH        120
#define TEXT_EDITOR_CONTEXT_MENU_ITEM_HEIGHT   18
#define TEXT_EDITOR_CONTEXT_MENU_ITEM_COUNT     4

#define TEXT_EDITOR_SENTINEL_ARROW_UP    '\x01'
#define TEXT_EDITOR_SENTINEL_ARROW_DOWN  '\x02'
#define TEXT_EDITOR_SENTINEL_ARROW_LEFT  '\x03'
#define TEXT_EDITOR_SENTINEL_ARROW_RIGHT '\x04'

struct XPWindow;

struct XPTextEditor {
    char    line_buffer[TEXT_EDITOR_MAX_LINES][TEXT_EDITOR_MAX_LINE_LENGTH];
    int     total_line_count;

    int     cursor_line_index;
    int     cursor_column_index;
    int     vertical_scroll_offset;

    int     client_area_x;
    int     client_area_y;
    int     client_area_width;
    int     client_area_height;

    bool    has_unsaved_changes;
    bool    all_text_is_selected;

    bool    context_menu_is_open;
    int     context_menu_screen_x;
    int     context_menu_screen_y;
    int     context_menu_hovered_item_index;

    XPWindow* owner_window;
};

XPTextEditor* text_editor_create(XPWindow* owner_window);
void          text_editor_destroy(XPTextEditor* editor);
void          text_editor_open_window(const char* optional_file_path);

void          text_editor_draw_frame(void* context);
void          text_editor_on_move(void* context, int new_window_x, int new_window_y);
void          text_editor_set_active(void* context);

void          text_editor_handle_key_input(XPTextEditor* editor, char input_character);
void          text_editor_handle_mouse(XPTextEditor* editor,
                                       int mouse_x, int mouse_y,
                                       bool left_clicked, bool right_clicked);