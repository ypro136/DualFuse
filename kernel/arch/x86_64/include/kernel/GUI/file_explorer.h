#pragma once

#include <cstdint>
#include <gui_defs.h>
#include <window.h>
#include <ramdisk.h>

#define FE_ICON_SIZE      48
#define FE_ICON_COLS      6
#define FE_ICON_PAD_X     20
#define FE_ICON_PAD_Y     20
#define FE_TOOLBAR_H      28
#define FE_ADDRESS_H      24
#define FE_CONTEXT_W      150
#define FE_CONTEXT_ITEM_H 20

#define FE_CTX_OPEN   0
#define FE_CTX_START  1
#define FE_CTX_DELETE 2
#define FE_CTX_COUNT  3

#define FE_BGX_NEW_FILE 0
#define FE_BGX_NEW_DIR  1
#define FE_BGX_REFRESH  2
#define FE_BGX_COUNT    3

#define FE_BACK_HISTORY 16

struct XPFileExplorer {
    XPWindow* window;

    int  dir_index;
    char address[MAX_PATH];

    int  selected_slot;
    int  hovered_slot;

    int  visible_files[MAX_FILES];
    int  visible_count;

    int      last_clicked_slot;
    uint64_t last_click_frame;

    bool ctx_open;
    int  ctx_x, ctx_y;
    int  ctx_target_slot;
    int  ctx_hovered_item;

    bool bgx_open;
    int  bgx_x, bgx_y;
    int  bgx_hovered_item;

    int back_stack[FE_BACK_HISTORY];
    int back_depth;

    int scroll_y;
};

XPFileExplorer* create_file_explorer(XPWindow* window);
void            destroy_file_explorer(XPFileExplorer* fe);
void            fe_draw_frame(XPFileExplorer* fe);
void            fe_handle_mouse(XPFileExplorer* fe,
                                int mouse_x, int mouse_y,
                                bool left_clicked, bool right_clicked);

void on_file_explorer_icon_click();
void fe_draw_frame_wrapper(void* ctx);
void fe_on_move(void* ctx, int x, int y);
void fe_set_active(void* ctx);