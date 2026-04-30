#ifndef DUALFUSE_FILE_EXPLORER_H
#define DUALFUSE_FILE_EXPLORER_H

#include <window.h>

#define FE_MAX_VISIBLE_FILES 256
#define FE_MAX_PATH_LENGTH   256
#define FE_BACK_HISTORY      32

#define FE_ICON_SIZE        48
#define FE_ICON_SPACING_Y   20
#define FE_ICON_COLUMNS     4
#define FE_TOOLBAR_HEIGHT   28
#define FE_ADDRESSBAR_HEIGHT 24

#define FE_CONTEXT_W        120
#define FE_CONTEXT_ITEM_H   24
#define FE_CTX_COUNT        3
#define FE_BGX_COUNT        3

struct XPFileExplorer
{
    XPWindow* window;
    char current_path[FE_MAX_PATH_LENGTH];   // address bar text

    int dir_index;                           // current directory index in fs.files
    char back_stack[FE_BACK_HISTORY][FE_MAX_PATH_LENGTH];
    int back_depth;

    // Visible files (filled by explorer_load_directory)
    char visible_names[FE_MAX_VISIBLE_FILES][64];
    bool visible_is_directory[FE_MAX_VISIBLE_FILES];
    int visible_file_index[FE_MAX_VISIBLE_FILES];   // index into fs.files
    int visible_count;

    // UI state
    int selected_index;
    int hovered_index;
    int scroll_offset;

    // Double-click detection (optional, but keep)
    int last_click_index;
    uint64_t last_click_frame;

    // Context menus
    bool ctx_open;
    int ctx_x, ctx_y;
    int ctx_target_slot;
    int ctx_hovered_item;

    bool bgx_open;
    int bgx_x, bgx_y;
    int bgx_hovered_item;
};

XPFileExplorer* create_file_explorer(XPWindow* window);
void destroy_file_explorer(XPFileExplorer* explorer);

void file_explorer_draw_frame(XPFileExplorer* explorer);
void fe_handle_mouse(XPFileExplorer* explorer, int mouse_x, int mouse_y,
                     bool left_clicked, bool right_clicked);

void file_explorer_draw_frame_wrapper(void* context);
void file_explorer_on_move(void* context, int x, int y);
void file_explorer_set_active(void* context);
void on_file_explorer_icon_click();

#endif