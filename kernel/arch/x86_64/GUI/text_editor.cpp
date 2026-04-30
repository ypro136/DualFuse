#include <text_editor.h>
#include <window.h>
#include <gui_primitives.h>
#include <framebufferutil.h>
#include <psf.h>
#include <liballoc.h>
#include <string.h>
#include <stdio.h>
#include <fs.h>
#include <ff.h>

extern uint64_t GUI_frame;

static const int TEXT_EDITOR_LINE_VERTICAL_PADDING = 3;

static char  text_editor_shared_clipboard[TEXT_EDITOR_MAX_LINES * TEXT_EDITOR_MAX_LINE_LENGTH];
static bool  text_editor_clipboard_contains_text = false;

static const char* context_menu_item_label_strings[TEXT_EDITOR_CONTEXT_MENU_ITEM_COUNT] = {
    "Select All",
    "Copy All",
    "Paste",
    "Clear"
};

static int text_editor_compute_line_pixel_height()
{
    return current_font_height + TEXT_EDITOR_LINE_VERTICAL_PADDING;
}

static int text_editor_compute_visible_row_count(XPTextEditor* editor)
{
    return (editor->client_area_height - 8) / text_editor_compute_line_pixel_height();
}

static void text_editor_sync_client_area_from_window(XPTextEditor* editor)
{
    editor->client_area_x      = editor->owner_window->x + WINDOW_BORDER_WIDTH;
    editor->client_area_y      = editor->owner_window->y + TITLE_BAR_HEIGHT + WINDOW_BORDER_WIDTH;
    editor->client_area_width  = editor->owner_window->width  - 2 * WINDOW_BORDER_WIDTH;
    editor->client_area_height = editor->owner_window->height - TITLE_BAR_HEIGHT - 2 * WINDOW_BORDER_WIDTH;
}

static void text_editor_clamp_cursor_column_to_line_end(XPTextEditor* editor)
{
    int current_line_length = (int)strlen(editor->line_buffer[editor->cursor_line_index]);
    if (editor->cursor_column_index > current_line_length)
        editor->cursor_column_index = current_line_length;
}

static void text_editor_scroll_to_keep_cursor_visible(XPTextEditor* editor)
{
    int visible_row_count = text_editor_compute_visible_row_count(editor);
    if (editor->cursor_line_index < editor->vertical_scroll_offset)
        editor->vertical_scroll_offset = editor->cursor_line_index;
    if (editor->cursor_line_index >= editor->vertical_scroll_offset + visible_row_count)
        editor->vertical_scroll_offset = editor->cursor_line_index - visible_row_count + 1;
}

static void text_editor_draw_context_menu_overlay(XPTextEditor* editor)
{
    int total_menu_pixel_height = TEXT_EDITOR_CONTEXT_MENU_ITEM_HEIGHT * TEXT_EDITOR_CONTEXT_MENU_ITEM_COUNT;

    fill_rectangle(editor->context_menu_screen_x, editor->context_menu_screen_y,
                   TEXT_EDITOR_CONTEXT_MENU_WIDTH, total_menu_pixel_height, 0xF0F0F0);
    draw_beveled_border_thick(editor->context_menu_screen_x, editor->context_menu_screen_y,
                              TEXT_EDITOR_CONTEXT_MENU_WIDTH, total_menu_pixel_height,
                              0xFFFFFF, 0xF0F0F0, 0x808080, true);

    for (int item_index = 0; item_index < TEXT_EDITOR_CONTEXT_MENU_ITEM_COUNT; item_index++)
    {
        int    item_top_y      = editor->context_menu_screen_y + item_index * TEXT_EDITOR_CONTEXT_MENU_ITEM_HEIGHT;
        bool   item_is_hovered = (item_index == editor->context_menu_hovered_item_index);
        uint32_t item_bg_color = item_is_hovered ? 0x316AC5 : 0xF0F0F0;
        uint32_t item_fg_color = item_is_hovered ? 0xFFFFFF : 0x000000;

        if (item_is_hovered)
            fill_rectangle(editor->context_menu_screen_x + 1, item_top_y,
                           TEXT_EDITOR_CONTEXT_MENU_WIDTH - 2,
                           TEXT_EDITOR_CONTEXT_MENU_ITEM_HEIGHT, item_bg_color);

        draw_text(context_menu_item_label_strings[item_index],
                  editor->context_menu_screen_x + 8,
                  item_top_y + current_font_height,
                  item_fg_color, item_bg_color);
    }
}

static void text_editor_execute_context_menu_action(XPTextEditor* editor, int item_index)
{
    if (item_index == 0)
    {
        editor->all_text_is_selected = true;
    }
    else if (item_index == 1)
    {
        int clipboard_write_cursor = 0;
        int clipboard_capacity     = (int)sizeof(text_editor_shared_clipboard) - 1;

        for (int line_index = 0; line_index < editor->total_line_count; line_index++)
        {
            int line_length    = (int)strlen(editor->line_buffer[line_index]);
            int remaining_space = clipboard_capacity - clipboard_write_cursor;
            if (remaining_space <= 0) break;

            if (line_length > remaining_space) line_length = remaining_space;
            memcpy(text_editor_shared_clipboard + clipboard_write_cursor,
                   editor->line_buffer[line_index], line_length);
            clipboard_write_cursor += line_length;

            if (line_index < editor->total_line_count - 1 && clipboard_write_cursor < clipboard_capacity)
                text_editor_shared_clipboard[clipboard_write_cursor++] = '\n';
        }
        text_editor_shared_clipboard[clipboard_write_cursor] = '\0';
        text_editor_clipboard_contains_text = (clipboard_write_cursor > 0);
    }
    else if (item_index == 2)
    {
        if (!text_editor_clipboard_contains_text) return;
        for (int char_index = 0;
             text_editor_shared_clipboard[char_index] != '\0';
             char_index++)
        {
            text_editor_handle_key_input(editor, text_editor_shared_clipboard[char_index]);
        }
    }
    else if (item_index == 3)
    {
        memset(editor->line_buffer, 0, sizeof(editor->line_buffer));
        editor->total_line_count        = 1;
        editor->cursor_line_index       = 0;
        editor->cursor_column_index     = 0;
        editor->vertical_scroll_offset  = 0;
        editor->all_text_is_selected    = false;
        editor->has_unsaved_changes     = true;
    }
}

XPTextEditor* text_editor_create(XPWindow* owner_window)
{
    XPTextEditor* editor = (XPTextEditor*)malloc(sizeof(XPTextEditor));
    if (!editor) return nullptr;

    memset(editor, 0, sizeof(XPTextEditor));
    editor->owner_window     = owner_window;
    editor->total_line_count = 1;

    text_editor_sync_client_area_from_window(editor);
    return editor;
}

void text_editor_destroy(XPTextEditor* editor)
{
    if (editor) free(editor);
}

void text_editor_open_window(const char* optional_file_path)
{
    XPWindowCallbacks window_callback_table;
    window_callback_table.draw_frame = text_editor_draw_frame;
    window_callback_table.on_move    = text_editor_on_move;
    window_callback_table.set_active = text_editor_set_active;
    window_callback_table.context    = nullptr;

    const char* window_title = (optional_file_path && optional_file_path[0] != '\0')
                               ? optional_file_path
                               : "Untitled - Text Editor";

    XPWindow* new_editor_window = create_xp_window(
        80, 80,
        55 * (SCREEN_WIDTH  / 100),
        60 * (SCREEN_HEIGHT / 100),
        window_title,
        &window_callback_table);

    new_editor_window->window_type = WINDOW_TYPE_TEXT_EDITOR;

    XPTextEditor* editor = text_editor_create(new_editor_window);
    new_editor_window->context = editor;

    if (optional_file_path && optional_file_path[0] != '\0')
    {
        FIL  file_handle;
        UINT bytes_read_count;
        if (f_open(&file_handle, optional_file_path, FA_READ) == FR_OK)
        {
            int current_load_line_index  = 0;
            int current_load_column      = 0;
            char single_byte_read_buffer;

            while (current_load_line_index < TEXT_EDITOR_MAX_LINES)
            {
                if (f_read(&file_handle, &single_byte_read_buffer, 1, &bytes_read_count) != FR_OK
                    || bytes_read_count == 0)
                    break;

                if (single_byte_read_buffer == '\n')
                {
                    current_load_line_index++;
                    current_load_column = 0;
                }
                else if (single_byte_read_buffer != '\r' &&
                         current_load_column < TEXT_EDITOR_MAX_LINE_LENGTH - 1)
                {
                    editor->line_buffer[current_load_line_index][current_load_column++] = single_byte_read_buffer;
                    editor->line_buffer[current_load_line_index][current_load_column]   = '\0';
                }
            }
            editor->total_line_count = current_load_line_index + 1;
            f_close(&file_handle);
        }
    }

    set_active_xp_window(new_editor_window);
}

void text_editor_draw_frame(void* context)
{
    XPTextEditor* editor = (XPTextEditor*)context;
    if (!editor) return;

    int line_pixel_height  = text_editor_compute_line_pixel_height();
    int visible_row_count  = text_editor_compute_visible_row_count(editor);
    bool cursor_blink_on   = (GUI_frame / 30) % 2 == 0;

    fill_rectangle(editor->client_area_x, editor->client_area_y,
                   editor->client_area_width, editor->client_area_height, 0xFFFFFF);
    draw_beveled_border_thick(editor->client_area_x, editor->client_area_y,
                              editor->client_area_width, editor->client_area_height,
                              0x808080, 0xFFFFFF, 0xFFFFFF, false);

    for (int visible_row_index = 0; visible_row_index < visible_row_count; visible_row_index++)
    {
        int source_line_index = visible_row_index + editor->vertical_scroll_offset;
        if (source_line_index >= editor->total_line_count) break;

        int line_top_pixel_y = editor->client_area_y + 4 + visible_row_index * line_pixel_height;
        int text_draw_x      = editor->client_area_x + 4;
        int text_baseline_y  = line_top_pixel_y + current_font_height;

        bool line_is_selection_highlighted = editor->all_text_is_selected;

        if (line_is_selection_highlighted)
            fill_rectangle(editor->client_area_x + 2, line_top_pixel_y,
                           editor->client_area_width - 4, line_pixel_height, 0x316AC5);

        draw_text(editor->line_buffer[source_line_index],
                  text_draw_x, text_baseline_y,
                  line_is_selection_highlighted ? 0xFFFFFF : 0x000000,
                  line_is_selection_highlighted ? 0x316AC5 : 0xFFFFFF);

        if (!editor->all_text_is_selected &&
            source_line_index == editor->cursor_line_index &&
            cursor_blink_on)
        {
            char cursor_prefix_text[TEXT_EDITOR_MAX_LINE_LENGTH];
strncpy(cursor_prefix_text, editor->line_buffer[source_line_index], editor->cursor_column_index);
cursor_prefix_text[editor->cursor_column_index] = '\0';
int cursor_pixel_x = text_draw_x + get_text_width(cursor_prefix_text);
            fill_rectangle(cursor_pixel_x, line_top_pixel_y, 2, current_font_height + 2, 0x000000);
        }
    }

    if (editor->context_menu_is_open)
        text_editor_draw_context_menu_overlay(editor);
}

void text_editor_on_move(void* context, int new_window_x, int new_window_y)
{
    XPTextEditor* editor = (XPTextEditor*)context;
    if (!editor) return;
    text_editor_sync_client_area_from_window(editor);
}

void text_editor_set_active(void* context) {}

void text_editor_handle_key_input(XPTextEditor* editor, char input_character)
{
    if (!editor) return;

    bool is_navigation_key = (input_character == TEXT_EDITOR_SENTINEL_ARROW_UP   ||
                              input_character == TEXT_EDITOR_SENTINEL_ARROW_DOWN  ||
                              input_character == TEXT_EDITOR_SENTINEL_ARROW_LEFT  ||
                              input_character == TEXT_EDITOR_SENTINEL_ARROW_RIGHT);

    if (editor->all_text_is_selected && !is_navigation_key)
        editor->all_text_is_selected = false;

    if (input_character == '\n')
    {
        if (editor->total_line_count >= TEXT_EDITOR_MAX_LINES) return;

        char* current_line        = editor->line_buffer[editor->cursor_line_index];
        int   current_line_length = (int)strlen(current_line);

        for (int shift_down_index = editor->total_line_count;
             shift_down_index > editor->cursor_line_index + 1;
             shift_down_index--)
        {
            memcpy(editor->line_buffer[shift_down_index],
                   editor->line_buffer[shift_down_index - 1],
                   TEXT_EDITOR_MAX_LINE_LENGTH);
        }

        char tail_of_split_line[TEXT_EDITOR_MAX_LINE_LENGTH];
        strncpy(tail_of_split_line, current_line + editor->cursor_column_index,
                TEXT_EDITOR_MAX_LINE_LENGTH - 1);
        tail_of_split_line[TEXT_EDITOR_MAX_LINE_LENGTH - 1] = '\0';

        current_line[editor->cursor_column_index] = '\0';
        strncpy(editor->line_buffer[editor->cursor_line_index + 1],
                tail_of_split_line, TEXT_EDITOR_MAX_LINE_LENGTH - 1);

        editor->total_line_count++;
        editor->cursor_line_index++;
        editor->cursor_column_index = 0;
        editor->has_unsaved_changes  = true;
    }
    else if (input_character == '\b')
    {
        if (editor->cursor_column_index > 0)
        {
            char* current_line        = editor->line_buffer[editor->cursor_line_index];
            int   current_line_length = (int)strlen(current_line);
            memmove(current_line + editor->cursor_column_index - 1,
                    current_line + editor->cursor_column_index,
                    current_line_length - editor->cursor_column_index + 1);
            editor->cursor_column_index--;
            editor->has_unsaved_changes = true;
        }
        else if (editor->cursor_line_index > 0)
        {
            char* previous_line        = editor->line_buffer[editor->cursor_line_index - 1];
            char* current_line         = editor->line_buffer[editor->cursor_line_index];
            int   previous_line_length = (int)strlen(previous_line);
            int   current_line_length  = (int)strlen(current_line);

            if (previous_line_length + current_line_length < TEXT_EDITOR_MAX_LINE_LENGTH - 1)
            {
                strncat(previous_line, current_line,
                        TEXT_EDITOR_MAX_LINE_LENGTH - previous_line_length - 1);

                for (int shift_up_index = editor->cursor_line_index;
                     shift_up_index < editor->total_line_count - 1;
                     shift_up_index++)
                {
                    memcpy(editor->line_buffer[shift_up_index],
                           editor->line_buffer[shift_up_index + 1],
                           TEXT_EDITOR_MAX_LINE_LENGTH);
                }
                memset(editor->line_buffer[editor->total_line_count - 1], 0,
                       TEXT_EDITOR_MAX_LINE_LENGTH);

                editor->total_line_count--;
                editor->cursor_line_index--;
                editor->cursor_column_index = previous_line_length;
                editor->has_unsaved_changes  = true;
            }
        }
    }
    else if (input_character == TEXT_EDITOR_SENTINEL_ARROW_UP)
    {
        if (editor->cursor_line_index > 0)
        {
            editor->cursor_line_index--;
            text_editor_clamp_cursor_column_to_line_end(editor);
        }
    }
    else if (input_character == TEXT_EDITOR_SENTINEL_ARROW_DOWN)
    {
        if (editor->cursor_line_index < editor->total_line_count - 1)
        {
            editor->cursor_line_index++;
            text_editor_clamp_cursor_column_to_line_end(editor);
        }
    }
    else if (input_character == TEXT_EDITOR_SENTINEL_ARROW_LEFT)
    {
        if (editor->cursor_column_index > 0)
        {
            editor->cursor_column_index--;
        }
        else if (editor->cursor_line_index > 0)
        {
            editor->cursor_line_index--;
            editor->cursor_column_index = (int)strlen(editor->line_buffer[editor->cursor_line_index]);
        }
    }
    else if (input_character == TEXT_EDITOR_SENTINEL_ARROW_RIGHT)
    {
        int current_line_length = (int)strlen(editor->line_buffer[editor->cursor_line_index]);
        if (editor->cursor_column_index < current_line_length)
        {
            editor->cursor_column_index++;
        }
        else if (editor->cursor_line_index < editor->total_line_count - 1)
        {
            editor->cursor_line_index++;
            editor->cursor_column_index = 0;
        }
    }
    else if ((unsigned char)input_character >= 0x20)
    {
        char* current_line        = editor->line_buffer[editor->cursor_line_index];
        int   current_line_length = (int)strlen(current_line);
        if (current_line_length >= TEXT_EDITOR_MAX_LINE_LENGTH - 1) return;

        memmove(current_line + editor->cursor_column_index + 1,
                current_line + editor->cursor_column_index,
                current_line_length - editor->cursor_column_index + 1);
        current_line[editor->cursor_column_index] = input_character;
        editor->cursor_column_index++;
        editor->has_unsaved_changes = true;
    }

    text_editor_scroll_to_keep_cursor_visible(editor);
}

void text_editor_handle_mouse(XPTextEditor* editor,
                               int mouse_x, int mouse_y,
                               bool left_clicked, bool right_clicked)
{
    if (!editor) return;

    if (editor->context_menu_is_open)
    {
        int relative_y_in_menu   = mouse_y - editor->context_menu_screen_y;
        int hovered_item_index   = -1;

        bool cursor_is_over_menu =
            mouse_x >= editor->context_menu_screen_x &&
            mouse_x <= editor->context_menu_screen_x + TEXT_EDITOR_CONTEXT_MENU_WIDTH &&
            relative_y_in_menu >= 0 &&
            relative_y_in_menu < TEXT_EDITOR_CONTEXT_MENU_ITEM_HEIGHT * TEXT_EDITOR_CONTEXT_MENU_ITEM_COUNT;

        if (cursor_is_over_menu)
            hovered_item_index = relative_y_in_menu / TEXT_EDITOR_CONTEXT_MENU_ITEM_HEIGHT;

        editor->context_menu_hovered_item_index = hovered_item_index;

        if (left_clicked)
        {
            editor->context_menu_is_open = false;
            if (hovered_item_index >= 0)
                text_editor_execute_context_menu_action(editor, hovered_item_index);
        }
        return;
    }

    if (right_clicked)
    {
        int clamped_menu_x         = mouse_x;
        int clamped_menu_y         = mouse_y;
        int total_menu_height      = TEXT_EDITOR_CONTEXT_MENU_ITEM_HEIGHT * TEXT_EDITOR_CONTEXT_MENU_ITEM_COUNT;
        int client_right_edge      = editor->client_area_x + editor->client_area_width;
        int client_bottom_edge     = editor->client_area_y + editor->client_area_height;

        if (clamped_menu_x + TEXT_EDITOR_CONTEXT_MENU_WIDTH > client_right_edge)
            clamped_menu_x = client_right_edge - TEXT_EDITOR_CONTEXT_MENU_WIDTH;
        if (clamped_menu_y + total_menu_height > client_bottom_edge)
            clamped_menu_y = client_bottom_edge - total_menu_height;

        editor->context_menu_is_open             = true;
        editor->context_menu_screen_x            = clamped_menu_x;
        editor->context_menu_screen_y            = clamped_menu_y;
        editor->context_menu_hovered_item_index  = -1;
        return;
    }

    if (left_clicked)
    {
        editor->all_text_is_selected = false;
        editor->context_menu_is_open = false;

        int line_pixel_height   = text_editor_compute_line_pixel_height();
        int click_relative_y    = mouse_y - (editor->client_area_y + 4);
        int click_relative_x    = mouse_x - (editor->client_area_x + 4);

        if (click_relative_x < 0 || click_relative_y < 0) return;

        int clicked_visible_row = click_relative_y / line_pixel_height;
        int clicked_line_index  = clicked_visible_row + editor->vertical_scroll_offset;
        char* clicked_line_text = editor->line_buffer[clicked_line_index];
        int   clicked_line_length = (int)strlen(clicked_line_text);
        int   clicked_column = clicked_line_length;
        char  column_prefix_buffer[TEXT_EDITOR_MAX_LINE_LENGTH];

        for (int column_candidate = 0; column_candidate <= clicked_line_length; column_candidate++)
        {
            strncpy(column_prefix_buffer, clicked_line_text, column_candidate);
            column_prefix_buffer[column_candidate] = '\0';
            if (get_text_width(column_prefix_buffer) >= click_relative_x)
            {
                clicked_column = column_candidate;
                break;
            }
        }

        if (clicked_line_index >= editor->total_line_count)
            clicked_line_index = editor->total_line_count - 1;

        editor->cursor_line_index   = clicked_line_index;
        editor->cursor_column_index = clicked_column;
        text_editor_clamp_cursor_column_to_line_end(editor);
    }
}