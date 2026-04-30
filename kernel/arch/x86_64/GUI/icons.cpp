#include <icons.h>
#include <fs.h>
#include <liballoc.h>
#include <stdio.h>
#include <string.h>
#include <GUI.h>
#include <framebufferutil.h>


Image folder_icon = {0};
Image file_icon = {0};
Image terminal_icon = {0};
Image calculator_icon = {0};
Image logo_image = {0};

static const char* icon_paths[] = {
    "/assets/folder_icon.png",
    "/assets/file_icon.png",
    "/assets/terminal_icon.png",
    "/assets/calculator_icon.png"
};

#define STANDARD_ICON_SIZE  (4 * (SCREEN_WIDTH / 100))


static Image* load_single_icon(const char* path) {
    FIL fp;
    FRESULT res = f_open(&fp, path, FA_READ);
    if (res != FR_OK) {
        printf("[icons] Failed to open %s\n", path);
        return NULL;
    }

    uint32_t size = f_size(&fp);
    uint8_t* buffer = (uint8_t*)malloc(size);
    if (!buffer) {
        f_close(&fp);
        return NULL;
    }

    UINT br;
    res = f_read(&fp, buffer, size, &br);
    f_close(&fp);
    if (res != FR_OK || br != size) {
        free(buffer);
        return NULL;
    }

    Image* img = (Image*)malloc(sizeof(Image));
    if (!img) {
        free(buffer);
        return NULL;
    }

    if (!image_load_from_png(img, buffer, size)) {
        free(buffer);
        free(img);
        return NULL;
    }
    free(buffer);

    // Resize to standard size (cached)
    if (!image_resize(img, STANDARD_ICON_SIZE, STANDARD_ICON_SIZE)) {
        printf("[icons] Failed to resize %s\n", path);
        image_free(img);
        free(img);
        return NULL;
    }

    return img;
}

void load_all_icons(void) {
    Image* folder = load_single_icon("/assets/folder_icon.png");
    if (folder) folder_icon = *folder; else free(folder);

    Image* file = load_single_icon("/assets/file_icon.png");
    if (file) file_icon = *file; else free(file);

    Image* term = load_single_icon("/assets/terminal_icon.png");
    if (term) terminal_icon = *term; else free(term);

    Image* calc = load_single_icon("/assets/calculator_icon.png");
    if (calc) calculator_icon = *calc; else free(calc);

    Image* logo = load_single_icon("/assets/logo.png");
    if (logo) {
        // Override the resize to a larger size
        image_free(logo);  // free the small version 
        Image* large_logo = load_single_icon("/assets/logo.png"); // reload
        if (large_logo && image_resize(large_logo, START_MENU_WIDTH - 10, (START_MENU_WIDTH - 10) / 5)) {
            logo_image = *large_logo;
            free(large_logo);
        } else {
            free(large_logo);
        }
    } else {
        free(logo);
    }

    printf("[icons] Icons loaded (folder, file, terminal, calculator)\n");
}

void free_all_icons(void) {
    image_free(&folder_icon);
    image_free(&file_icon);
    image_free(&terminal_icon);
    image_free(&calculator_icon);
    image_free(&logo_image);
}