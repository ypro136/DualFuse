#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

#include <png_loader.h>
#include <window.h>

typedef struct {
    XPWindow* window;
    Image     image;
    char      file_path[256];
    float     zoom;               // not used yet, reserved
} XPImageViewer;

XPImageViewer* create_image_viewer(XPWindow* window, const char* path);
void destroy_image_viewer(XPImageViewer* viewer);
void image_viewer_draw_frame(void* context);
void image_viewer_on_move(void* context, int x, int y);
void image_viewer_set_active(void* context);

void open_image_in_viewer(const char* path);   // convenience: creates window + viewer

#endif