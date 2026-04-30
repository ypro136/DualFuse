#include <image_viewer.h>
#include <gui_primitives.h>
#include <framebufferutil.h>
#include <psf.h>
#include <liballoc.h>
#include <fs.h>
#include <string.h>
#include <stdio.h>

XPImageViewer* create_image_viewer(XPWindow* window, const char* path)
{
    if (!window || !path) return NULL;

    XPImageViewer* viewer = (XPImageViewer*)malloc(sizeof(XPImageViewer));
    if (!viewer) return NULL;

    memset(viewer, 0, sizeof(XPImageViewer));
    viewer->window = window;
    strncpy(viewer->file_path, path, sizeof(viewer->file_path) - 1);
    viewer->zoom = 1.0f;

    // Load the image file
    FIL fp;
    FRESULT res = f_open(&fp, path, FA_READ);
    if (res != FR_OK) {
        printf("[image_viewer] Failed to open %s\n", path);
        free(viewer);
        return NULL;
    }

    uint32_t size = f_size(&fp);
    uint8_t* buffer = (uint8_t*)malloc(size);
    if (!buffer) {
        f_close(&fp);
        free(viewer);
        return NULL;
    }

    UINT br;
    res = f_read(&fp, buffer, size, &br);
    f_close(&fp);
    if (res != FR_OK || br != size) {
        free(buffer);
        free(viewer);
        return NULL;
    }

    if (!image_load_from_png(&viewer->image, buffer, size)) {
        free(buffer);
        free(viewer);
        return NULL;
    }
    free(buffer);

    return viewer;
}

void destroy_image_viewer(XPImageViewer* viewer)
{
    if (!viewer) return;
    image_free(&viewer->image);
    free(viewer);
}

void image_viewer_draw_frame(void* context)
{
    XPImageViewer* viewer = (XPImageViewer*)context;
    if (!viewer || !viewer->window) return;

    int client_x = viewer->window->x + WINDOW_BORDER_WIDTH;
    int client_y = viewer->window->y + TITLE_BAR_HEIGHT + WINDOW_BORDER_WIDTH;
    int client_w = viewer->window->width - 2 * WINDOW_BORDER_WIDTH;
    int client_h = viewer->window->height - TITLE_BAR_HEIGHT - 2 * WINDOW_BORDER_WIDTH;

    fill_rectangle(client_x, client_y, client_w, client_h, XP_BACKGROUND);

    uint32_t img_w = viewer->image.original_width;
    uint32_t img_h = viewer->image.original_height;

    if (img_w == 0 || img_h == 0 || client_w <= 0 || client_h <= 0) return;

    uint32_t draw_w, draw_h;

    // Compare aspect ratios using cross multiplication (no floats)
    if (img_w * client_h > client_w * img_h) {
        draw_w = client_w;
        draw_h = (client_w * img_h) / img_w;
    } else {
        draw_h = client_h;
        draw_w = (client_h * img_w) / img_h;
    }

    if (draw_w == 0) draw_w = 1;
    if (draw_h == 0) draw_h = 1;
 
    int img_x = client_x + (client_w - draw_w) / 2;
    int img_y = client_y + (client_h - draw_h) / 2;

    image_draw(&viewer->image, img_x, img_y, draw_w, draw_h);
}

void image_viewer_on_move(void* context, int x, int y)
{
    // Nothing to do - window position changed, but the image will be redrawn anyway.
    // The draw_frame will be called after the move.
}

void image_viewer_set_active(void* context)
{
    // Not needed for now
}

void open_image_in_viewer(const char* path)
{
    XPWindowCallbacks* callbacks = (XPWindowCallbacks*)malloc(sizeof(XPWindowCallbacks));
    callbacks->draw_frame = image_viewer_draw_frame;
    callbacks->on_move    = image_viewer_on_move;
    callbacks->set_active = image_viewer_set_active;
    callbacks->context    = NULL;

    // Create a window of moderate size - you can adjust
    int win_w = (SCREEN_WIDTH > 800) ? 600 : SCREEN_WIDTH - 40;
    int win_h = (SCREEN_HEIGHT > 600) ? 500 : SCREEN_HEIGHT - 60;
    int win_x = (SCREEN_WIDTH - win_w) / 2;
    int win_y = (SCREEN_HEIGHT - win_h) / 2;

    XPWindow* window = create_xp_window(win_x, win_y, win_w, win_h, "Image Viewer", callbacks);
    window->window_type = WINDOW_TYPE_IMAGE_VIEWER;

    XPImageViewer* viewer = create_image_viewer(window, path);
    window->context = viewer;

    set_active_xp_window(window);
}