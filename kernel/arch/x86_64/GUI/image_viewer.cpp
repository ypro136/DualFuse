#include <image_viewer.h>
#include <image_processor.h>
#include <gui_primitives.h>
#include <framebufferutil.h>
#include <psf.h>
#include <liballoc.h>
#include <fs.h>
#include <lodepng.h>
#include <string.h>
#include <stdio.h>

static const char* filter_context_menu_labels[IMAGE_FILTER_COUNT] = {
    "Red Channel",
    "Green Channel",
    "Blue Channel",
    "Grayscale",
    "Histogram",
    "Equalize Histogram",
    "Gaussian Blur",
    "Sobel Edges",
    "Brightness +30",
    "Negative",
    "Median 3x3",
    "Laplacian",
    "Sharpen",
    "Motion Blur",
    "Bilateral 5x5",
    "Emboss",
    "Canny Edges",
    "Contrast 1.5x"
};

static void image_viewer_build_output_file_path(const char* source_path,
                                                 const char* suffix,
                                                 char* output_buffer,
                                                 int output_buffer_size)
{
    strncpy(output_buffer, source_path, output_buffer_size - 1);
    output_buffer[output_buffer_size - 1] = '\0';

    char* last_dot = (char*)0;
    for (char* scan = output_buffer; *scan; scan++) {
        if (*scan == '.') last_dot = scan;
    }

    if (last_dot) *last_dot = '\0';

    int current_length = 0;
    for (; output_buffer[current_length]; current_length++);

    int suffix_length = 0;
    for (; suffix[suffix_length]; suffix_length++);

    if (current_length + suffix_length + 5 < output_buffer_size) {
        memcpy(output_buffer + current_length, suffix, suffix_length);
        memcpy(output_buffer + current_length + suffix_length, ".png", 5);
    }

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_build_output_file_path: src:%s out:%s\n", source_path, output_buffer);
    #endif
}

static bool image_viewer_convert_argb_pixels_to_rgba_bytes(const uint32_t* argb_pixels,
                                                             uint32_t pixel_count,
                                                             uint8_t* rgba_output_bytes)
{
    if (!argb_pixels || !rgba_output_bytes) return false;
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint32_t pixel = argb_pixels[i];
        rgba_output_bytes[i * 4 + 0] = (pixel >> 16) & 0xFF;
        rgba_output_bytes[i * 4 + 1] = (pixel >>  8) & 0xFF;
        rgba_output_bytes[i * 4 + 2] =  pixel        & 0xFF;
        rgba_output_bytes[i * 4 + 3] = (pixel >> 24) & 0xFF;
    }
    return true;
}

static bool image_viewer_save_argb_pixels_as_png_to_fatfs(const uint32_t* argb_pixels,
                                                            uint32_t width,
                                                            uint32_t height,
                                                            const char* destination_path)
{
    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_save_argb_pixels_as_png_to_fatfs: w:%u h:%u dst:%s\n", width, height, destination_path);
    #endif

    uint32_t pixel_count      = width * height;
    uint8_t* rgba_byte_buffer = (uint8_t*)malloc(pixel_count * 4);
    if (!rgba_byte_buffer) {
        #if defined(DEBUG_IMAGE_VIEWER)
        printf("[DEBUG_IMAGE_VIEWER] image_viewer_save_argb_pixels_as_png_to_fatfs: rgba malloc FAILED pixel_count:%u\n", pixel_count);
        #endif
        return false;
    }

    image_viewer_convert_argb_pixels_to_rgba_bytes(argb_pixels, pixel_count, rgba_byte_buffer);

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_save_argb_pixels_as_png_to_fatfs: calling lodepng_encode32\n");
    #endif

    unsigned char* encoded_png_data   = (unsigned char*)0;
    size_t         encoded_png_size   = 0;
    unsigned       lodepng_error_code = lodepng_encode32(&encoded_png_data, &encoded_png_size,
                                                          rgba_byte_buffer, width, height);
    free(rgba_byte_buffer);

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_save_argb_pixels_as_png_to_fatfs: lodepng err:%u size:%u data:%p\n",
           lodepng_error_code, (uint32_t)encoded_png_size, encoded_png_data);
    #endif

    if (lodepng_error_code || !encoded_png_data) {
        #if defined(DEBUG_IMAGE_VIEWER)
        printf("[DEBUG_IMAGE_VIEWER] image_viewer_save_argb_pixels_as_png_to_fatfs: encode FAILED\n");
        #endif
        return false;
    }

    FIL output_file;
    FRESULT fatfs_result = f_open(&output_file, destination_path, FA_CREATE_ALWAYS | FA_WRITE);

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_save_argb_pixels_as_png_to_fatfs: f_open result:%d\n", (int)fatfs_result);
    #endif

    if (fatfs_result != FR_OK) {
        free(encoded_png_data);
        return false;
    }

    UINT bytes_written = 0;
    f_write(&output_file, encoded_png_data, (UINT)encoded_png_size, &bytes_written);
    f_close(&output_file);
    free(encoded_png_data);

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_save_argb_pixels_as_png_to_fatfs: bytes_written:%u expected:%u\n",
           (uint32_t)bytes_written, (uint32_t)encoded_png_size);
    #endif

    return (bytes_written == (UINT)encoded_png_size);
}

static void image_viewer_apply_filter_and_open_result(XPImageViewer* viewer, ImageFilterType filter)
{
    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_apply_filter_and_open_result: filter:%d\n", (int)filter);
    #endif

    if (!viewer->image.original_data) {
        #if defined(DEBUG_IMAGE_VIEWER)
        printf("[DEBUG_IMAGE_VIEWER] image_viewer_apply_filter_and_open_result: original_data NULL, aborting\n");
        #endif
        return;
    }

    uint32_t width       = viewer->image.original_width;
    uint32_t height      = viewer->image.original_height;
    uint32_t pixel_count = width * height;

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_apply_filter_and_open_result: w:%u h:%u pixel_count:%u\n", width, height, pixel_count);
    #endif

    uint32_t* working_pixel_buffer = (uint32_t*)malloc(pixel_count * sizeof(uint32_t));
    if (!working_pixel_buffer) {
        #if defined(DEBUG_IMAGE_VIEWER)
        printf("[DEBUG_IMAGE_VIEWER] image_viewer_apply_filter_and_open_result: working buffer malloc FAILED\n");
        #endif
        return;
    }

    memcpy(working_pixel_buffer, viewer->image.original_data, pixel_count * sizeof(uint32_t));

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_apply_filter_and_open_result: calling image_processor_apply\n");
    #endif

    image_processor_apply(filter, working_pixel_buffer, width, height);

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_apply_filter_and_open_result: image_processor_apply done\n");
    #endif

    char output_file_path[256];
    image_viewer_build_output_file_path(viewer->file_path,
                                         image_processor_filter_file_suffix(filter),
                                         output_file_path,
                                         sizeof(output_file_path));

    bool save_succeeded = image_viewer_save_argb_pixels_as_png_to_fatfs(
        working_pixel_buffer, width, height, output_file_path);

    free(working_pixel_buffer); 

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_apply_filter_and_open_result: save_succeeded:%d path:%s\n",
           (int)save_succeeded, output_file_path);
    #endif

    if (save_succeeded) {
        #if defined(DEBUG_IMAGE_VIEWER)
        printf("[DEBUG_IMAGE_VIEWER] image_viewer_apply_filter_and_open_result: calling open_image_in_viewer\n");
        #endif
        open_image_in_viewer(output_file_path);
    }
}

static void image_viewer_draw_histogram_overlay(XPImageViewer* viewer,
                                                 int client_x, int client_y,
                                                 int client_width, int client_height)
{
    if (!viewer->cached_histogram_bins_valid) {
        #if defined(DEBUG_IMAGE_VIEWER)
        printf("[DEBUG_IMAGE_VIEWER] image_viewer_draw_histogram_overlay: computing histogram pixel_count:%u\n",
               viewer->image.original_width * viewer->image.original_height);
        #endif
        image_processor_compute_luminance_histogram(viewer->image.original_data,
                                                     viewer->image.original_width * viewer->image.original_height,
                                                     viewer->cached_histogram_bins);
        viewer->cached_histogram_bins_valid = true;
    }

    int overlay_width  = client_width  * 20 / 100;
    int overlay_height = client_height * 20 / 100;
    if (overlay_width  < 64)  overlay_width  = 64;
    if (overlay_height < 48)  overlay_height = 48;

    int overlay_x = client_x + 4;
    int overlay_y = client_y + 4;

    fill_rectangle(overlay_x, overlay_y, overlay_width, overlay_height, 0x1A1A1A);
    draw_rect_outline(overlay_x, overlay_y, overlay_width, overlay_height, 0x606060, 1);

    uint32_t maximum_bin_count = 1;
    for (int bin = 0; bin < 256; bin++) {
        if (viewer->cached_histogram_bins[bin] > maximum_bin_count)
            maximum_bin_count = viewer->cached_histogram_bins[bin];
    }

    int histogram_draw_area_height = overlay_height - 2;

    for (int bin = 0; bin < 256; bin++) {
        int bar_x = overlay_x + 1 + (bin * (overlay_width - 2)) / 256;
        int bar_width = ((bin + 1) * (overlay_width - 2)) / 256
                      - (bin      * (overlay_width - 2)) / 256;
        if (bar_width < 1) bar_width = 1;

        int bar_height = (int)(
            (uint64_t)viewer->cached_histogram_bins[bin]
            * (uint32_t)histogram_draw_area_height
            / maximum_bin_count);
        if (bar_height < 1 && viewer->cached_histogram_bins[bin] > 0) bar_height = 1;

        int bar_y = overlay_y + overlay_height - 1 - bar_height;
        fill_rectangle(bar_x, bar_y, bar_width, bar_height, 0xD4D4D4);
    }
}

static void image_viewer_draw_filter_context_menu(XPImageViewer* viewer)
{
    int total_menu_height = IMAGE_VIEWER_FILTER_MENU_ITEM_HEIGHT * IMAGE_FILTER_COUNT;

    fill_rectangle(viewer->filter_context_menu_x,
                   viewer->filter_context_menu_y,
                   IMAGE_VIEWER_FILTER_MENU_WIDTH,
                   total_menu_height,
                   0xF0F0F0);

    draw_beveled_border_thick(viewer->filter_context_menu_x,
                               viewer->filter_context_menu_y,
                               IMAGE_VIEWER_FILTER_MENU_WIDTH,
                               total_menu_height,
                               0xFFFFFF, 0xF0F0F0, 0x808080, true);

    for (int item_index = 0; item_index < IMAGE_FILTER_COUNT; item_index++) {
        int item_y = viewer->filter_context_menu_y + item_index * IMAGE_VIEWER_FILTER_MENU_ITEM_HEIGHT;
        bool item_is_hovered = (item_index == viewer->filter_context_menu_hovered_item);

        if (item_is_hovered)
            fill_rectangle(viewer->filter_context_menu_x + 1,
                           item_y,
                           IMAGE_VIEWER_FILTER_MENU_WIDTH - 2,
                           IMAGE_VIEWER_FILTER_MENU_ITEM_HEIGHT,
                           0x316AC5);

        draw_text(filter_context_menu_labels[item_index],
                  viewer->filter_context_menu_x + 8,
                  item_y + current_font_height,
                  item_is_hovered ? 0xFFFFFF : 0x000000,
                  item_is_hovered ? 0x316AC5 : 0xF0F0F0);
    }
}

XPImageViewer* create_image_viewer(XPWindow* window, const char* path)
{
    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] create_image_viewer: path:%s window:%p\n", path ? path : "NULL", window);
    #endif

    if (!window || !path) return (XPImageViewer*)0;

    XPImageViewer* viewer = (XPImageViewer*)malloc(sizeof(XPImageViewer));
    if (!viewer) {
        #if defined(DEBUG_IMAGE_VIEWER)
        printf("[DEBUG_IMAGE_VIEWER] create_image_viewer: viewer malloc FAILED\n");
        #endif
        return (XPImageViewer*)0;
    }

    memset(viewer, 0, sizeof(XPImageViewer));
    viewer->window = window;
    strncpy(viewer->file_path, path, sizeof(viewer->file_path) - 1);
    viewer->zoom = 1.0f;

    FIL source_file;
    FRESULT open_result = f_open(&source_file, path, FA_READ);

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] create_image_viewer: f_open result:%d\n", (int)open_result);
    #endif

    if (open_result != FR_OK) {
        free(viewer);
        return (XPImageViewer*)0;
    }

    uint32_t file_size_bytes = f_size(&source_file);

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] create_image_viewer: file_size:%u\n", file_size_bytes);
    #endif

    uint8_t* file_read_buffer = (uint8_t*)malloc(file_size_bytes);
    if (!file_read_buffer) {
        #if defined(DEBUG_IMAGE_VIEWER)
        printf("[DEBUG_IMAGE_VIEWER] create_image_viewer: file_read_buffer malloc FAILED\n");
        #endif
        f_close(&source_file);
        free(viewer);
        return (XPImageViewer*)0;
    }

    UINT bytes_read = 0;
    FRESULT read_result = f_read(&source_file, file_read_buffer, file_size_bytes, &bytes_read);
    f_close(&source_file);

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] create_image_viewer: f_read result:%d bytes_read:%u\n", (int)read_result, (uint32_t)bytes_read);
    #endif

    if (read_result != FR_OK || bytes_read != file_size_bytes) {
        #if defined(DEBUG_IMAGE_VIEWER)
        printf("[DEBUG_IMAGE_VIEWER] create_image_viewer: f_read FAILED or short read\n");
        #endif
        free(file_read_buffer);
        free(viewer);
        return (XPImageViewer*)0;
    }

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] create_image_viewer: calling image_load_from_png\n");
    #endif

    bool image_load_succeeded = image_load_from_png(&viewer->image, file_read_buffer, file_size_bytes);
    free(file_read_buffer);

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] create_image_viewer: image_load_from_png result:%d w:%u h:%u data:%p\n",
           (int)image_load_succeeded,
           viewer->image.original_width,
           viewer->image.original_height,
           viewer->image.original_data);
    #endif

    if (!image_load_succeeded) {
        free(viewer);
        return (XPImageViewer*)0;
    }

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] create_image_viewer: done viewer:%p\n", viewer);
    #endif

    return viewer;
}

void destroy_image_viewer(XPImageViewer* viewer)
{
    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] destroy_image_viewer: viewer:%p\n", viewer);
    #endif

    if (!viewer) return;
    image_free(&viewer->image);
    free(viewer);
}

void image_viewer_draw_frame(void* context)
{
    XPImageViewer* viewer = (XPImageViewer*)context;

    #if defined(DEBUG_IMAGE_VIEWER) && defined(DEBUG_LOOPING)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_draw_frame: viewer:%p\n", viewer);
    #endif 

    if (!viewer || !viewer->window) return;

    int client_x = viewer->window->x + WINDOW_BORDER_WIDTH;
    int client_y = viewer->window->y + TITLE_BAR_HEIGHT + WINDOW_BORDER_WIDTH;
    int client_w = viewer->window->width  - 2 * WINDOW_BORDER_WIDTH;
    int client_h = viewer->window->height - TITLE_BAR_HEIGHT - 2 * WINDOW_BORDER_WIDTH;

    fill_rectangle(client_x, client_y, client_w, client_h, XP_BACKGROUND);

    if (!viewer->image.original_data) {
        #if defined(DEBUG_IMAGE_VIEWER) && defined(DEBUG_LOOPING)
        printf("[DEBUG_IMAGE_VIEWER] image_viewer_draw_frame: original_data NULL, skipping draw\n");
        #endif
        return;
    }

    uint32_t img_w = viewer->image.original_width;
    uint32_t img_h = viewer->image.original_height;

    #if defined(DEBUG_IMAGE_VIEWER) && defined(DEBUG_LOOPING)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_draw_frame: img:%ux%u client:%dx%d\n", img_w, img_h, client_w, client_h);
    #endif

    if (img_w == 0 || img_h == 0 || client_w <= 0 || client_h <= 0) {
        #if defined(DEBUG_IMAGE_VIEWER) && defined(DEBUG_LOOPING)
        printf("[DEBUG_IMAGE_VIEWER] image_viewer_draw_frame: zero dimension, skipping\n");
        #endif
        return;
    }

    uint32_t draw_w, draw_h;
    if (img_w * (uint32_t)client_h > (uint32_t)client_w * img_h) {
        draw_w = (uint32_t)client_w;
        draw_h = ((uint32_t)client_w * img_h) / img_w;
    } else {
        draw_h = (uint32_t)client_h;
        draw_w = ((uint32_t)client_h * img_w) / img_h;
    }
    if (draw_w == 0) draw_w = 1;
    if (draw_h == 0) draw_h = 1;

    int image_draw_x = client_x + (client_w - (int)draw_w) / 2;
    int image_draw_y = client_y + (client_h - (int)draw_h) / 2;

    #if defined(DEBUG_IMAGE_VIEWER) && defined(DEBUG_LOOPING)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_draw_frame: draw_at:%d,%d draw_size:%ux%u\n",
           image_draw_x, image_draw_y, draw_w, draw_h);
    #endif

    image_draw(&viewer->image, image_draw_x, image_draw_y, draw_w, draw_h);

    #if defined(DEBUG_IMAGE_VIEWER) && defined(DEBUG_LOOPING)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_draw_frame: image_draw done\n");
    #endif

    if (viewer->histogram_overlay_visible)
        image_viewer_draw_histogram_overlay(viewer, client_x, client_y, client_w, client_h);

    if (viewer->filter_context_menu_open)
        image_viewer_draw_filter_context_menu(viewer);
}

void image_viewer_on_move(void* context, int x, int y) {}
void image_viewer_set_active(void* context) {}

void image_viewer_handle_mouse(void* context, int mouse_x, int mouse_y,
                                bool left_clicked, bool right_clicked)
{
    XPImageViewer* viewer = (XPImageViewer*)context;

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] image_viewer_handle_mouse: viewer:%p mx:%d my:%d lc:%d rc:%d\n",
           viewer, mouse_x, mouse_y, (int)left_clicked, (int)right_clicked);
    #endif

    if (!viewer) return;

    if (viewer->filter_context_menu_open) {
        int relative_y = mouse_y - viewer->filter_context_menu_y;
        bool mouse_over_menu = (mouse_x >= viewer->filter_context_menu_x &&
                                mouse_x <= viewer->filter_context_menu_x + IMAGE_VIEWER_FILTER_MENU_WIDTH &&
                                relative_y >= 0 &&
                                relative_y < IMAGE_VIEWER_FILTER_MENU_ITEM_HEIGHT * IMAGE_FILTER_COUNT);

        viewer->filter_context_menu_hovered_item = mouse_over_menu
            ? relative_y / IMAGE_VIEWER_FILTER_MENU_ITEM_HEIGHT
            : -1;

        if (left_clicked) {
            viewer->filter_context_menu_open = false;
            if (mouse_over_menu && viewer->filter_context_menu_hovered_item >= 0) {
                ImageFilterType selected_filter = (ImageFilterType)viewer->filter_context_menu_hovered_item;

                #if defined(DEBUG_IMAGE_VIEWER)
                printf("[DEBUG_IMAGE_VIEWER] image_viewer_handle_mouse: menu item selected:%d label:%s\n",
                       (int)selected_filter, filter_context_menu_labels[(int)selected_filter]);
                #endif

                if (selected_filter == IMAGE_FILTER_HISTOGRAM_TOGGLE) {
                    viewer->histogram_overlay_visible = !viewer->histogram_overlay_visible;
                    #if defined(DEBUG_IMAGE_VIEWER)
                    printf("[DEBUG_IMAGE_VIEWER] image_viewer_handle_mouse: histogram_visible:%d\n",
                           (int)viewer->histogram_overlay_visible);
                    #endif
                } else {
                    image_viewer_apply_filter_and_open_result(viewer, selected_filter);
                }
            }
        }
        return;
    }

    if (right_clicked) {
        if (!viewer->image.original_data) {
            #if defined(DEBUG_IMAGE_VIEWER)
            printf("[DEBUG_IMAGE_VIEWER] image_viewer_handle_mouse: no image, closing window\n");
            #endif
            close_xp_window((void*)viewer->window);
            return;
        }
        #if defined(DEBUG_IMAGE_VIEWER)
        printf("[DEBUG_IMAGE_VIEWER] image_viewer_handle_mouse: opening filter menu at %d,%d\n", mouse_x, mouse_y);
        #endif
        viewer->filter_context_menu_open         = true;
        viewer->filter_context_menu_x            = mouse_x;
        viewer->filter_context_menu_y            = mouse_y;
        viewer->filter_context_menu_hovered_item = -1;
    }

    if (left_clicked && viewer->filter_context_menu_open == false) {
        viewer->filter_context_menu_open = false;
    }
}

void open_image_in_viewer(const char* path)
{
    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] open_image_in_viewer: path:%s\n", path ? path : "NULL");
    #endif

    XPWindowCallbacks* callbacks = (XPWindowCallbacks*)malloc(sizeof(XPWindowCallbacks));
    callbacks->draw_frame  = image_viewer_draw_frame;
    callbacks->on_move     = image_viewer_on_move;
    callbacks->set_active  = image_viewer_set_active;
    callbacks->context     = (void*)0;

    int win_w = (SCREEN_WIDTH  > 800) ? 600 : SCREEN_WIDTH  - 40;
    int win_h = (SCREEN_HEIGHT > 600) ? 500 : SCREEN_HEIGHT - 60;
    int win_x = (SCREEN_WIDTH  - win_w) / 2;
    int win_y = (SCREEN_HEIGHT - win_h) / 2;

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] open_image_in_viewer: window x:%d y:%d w:%d h:%d\n", win_x, win_y, win_w, win_h);
    #endif

    XPWindow* window = create_xp_window(win_x, win_y, win_w, win_h, "Image Viewer", callbacks);
    window->window_type = WINDOW_TYPE_IMAGE_VIEWER;

    XPImageViewer* viewer = create_image_viewer(window, path);

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] open_image_in_viewer: viewer:%p\n", viewer);
    #endif

    window->context    = viewer;
    callbacks->context = viewer;

    set_active_xp_window(window);

    #if defined(DEBUG_IMAGE_VIEWER)
    printf("[DEBUG_IMAGE_VIEWER] open_image_in_viewer: done\n");
    #endif
}