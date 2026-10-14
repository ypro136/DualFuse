#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

#include <png_loader.h>
#include <image_processor.h>
#include <window.h>

#define IMAGE_VIEWER_FILTER_MENU_ITEM_HEIGHT 18
#define IMAGE_VIEWER_FILTER_MENU_WIDTH       140

typedef struct {
    XPWindow* window;
    Image     image;
    char      file_path[256];
    float     zoom;

    bool      filter_context_menu_open;
    int       filter_context_menu_x;
    int       filter_context_menu_y;
    int       filter_context_menu_hovered_item;

    bool      histogram_overlay_visible;
    uint32_t  cached_histogram_bins[256];
    bool      cached_histogram_bins_valid;
} XPImageViewer;

XPImageViewer* create_image_viewer(XPWindow* window, const char* path);
void           destroy_image_viewer(XPImageViewer* viewer);
void           image_viewer_draw_frame(void* context);
void           image_viewer_on_move(void* context, int x, int y);
void           image_viewer_set_active(void* context);
void           image_viewer_handle_mouse(void* context, int mouse_x, int mouse_y,
                                         bool left_clicked, bool right_clicked);

void open_image_in_viewer(const char* path);

#endif