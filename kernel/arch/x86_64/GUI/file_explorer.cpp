#include <file_explorer.h>
#include <GUI.h>
#include <window.h>
#include <gui_primitives.h>
#include <framebufferutil.h>
#include <psf.h>
#include <liballoc.h>
#include <ff.h>
#include <fs.h>          // FatFs API
#include <string.h>
#include <ramdisk.h>
#include <timer.h>
#include <image_viewer.h>
#include <icons.h>


#define MAX_NAME_ATTEMPTS 100

static int strcasecmp(const char* a, const char* b) {
    for (; *a && *b; a++, b++) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return ca - cb;
    }
    return *a - *b;
}

static void int_to_str(int num, char* buf)
{
    int i = 0;
    if (num == 0) {
        buf[i++] = '0';
    } else {
        int temp = num;
        while (temp > 0) {
            buf[i++] = '0' + temp % 10;
            temp /= 10;
        }
        // reverse
        for (int j = 0; j < i / 2; j++) {
            char c = buf[j];
            buf[j] = buf[i - 1 - j];
            buf[i - 1 - j] = c;
        }
    }
    buf[i] = '\0';
}

extern uint64_t GUI_frame;
extern void fs_start(const char* path, void* arg);

static int explorer_client_area_x(XPFileExplorer* explorer)
{
    return explorer->window->x + WINDOW_BORDER_WIDTH;
}

static int explorer_toolbar_y(XPFileExplorer* explorer)
{
    return explorer->window->y + TITLE_BAR_HEIGHT + WINDOW_BORDER_WIDTH;
}

static int explorer_addressbar_y(XPFileExplorer* explorer)
{
    return explorer_toolbar_y(explorer) + FE_TOOLBAR_HEIGHT;
}

static int explorer_content_y(XPFileExplorer* explorer)
{
    return explorer_addressbar_y(explorer) + FE_ADDRESSBAR_HEIGHT;
}

static int explorer_client_area_width(XPFileExplorer* explorer)
{
    return explorer->window->width - 2 * WINDOW_BORDER_WIDTH;
}

static int explorer_client_area_height(XPFileExplorer* explorer)
{
    return explorer->window->height - TITLE_BAR_HEIGHT - 2 * WINDOW_BORDER_WIDTH;
}

static bool is_png_file(const char* filename) {
    const char* dot = strrchr(filename, '.');
    return (dot && strcasecmp(dot, ".png") == 0);
}

// --- Directory listing using FatFs ---
static void explorer_load_directory(XPFileExplorer* explorer)
{
    explorer->visible_count = 0;

    DIR dir;
    FILINFO fno;

    if (fs_opendir(&dir, explorer->current_path) != FR_OK)
        return;

    while (explorer->visible_count < FE_MAX_VISIBLE_FILES)
    {
        if (fs_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
            break;

        // Skip "." and ".."
        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0)
            continue;

        strncpy(explorer->visible_names[explorer->visible_count], fno.fname, 63);
        explorer->visible_names[explorer->visible_count][63] = '\0';
        explorer->visible_is_directory[explorer->visible_count] =
            (fno.fattrib & AM_DIR) != 0;

        // Store the file index (not used for FatFs, but we need it for context menu)
        // We'll use -1 for now; later we can map to something else if needed.
        explorer->visible_file_index[explorer->visible_count] = -1;

        explorer->visible_count++;
    }

    fs_closedir(&dir);

    // Reset UI state
    explorer->selected_index = -1;
    explorer->hovered_index = -1;
    explorer->scroll_offset = 0;
    explorer->ctx_open = false;
    explorer->bgx_open = false;
}

XPFileExplorer* create_file_explorer(XPWindow* window)
{
    XPFileExplorer* explorer = (XPFileExplorer*)malloc(sizeof(XPFileExplorer));
    if (!explorer) return nullptr;

    explorer->window = window;
    strcpy(explorer->current_path, "/");
    explorer->back_depth = 0;
    explorer->last_click_index = -1;
    explorer->last_click_frame = 0;
    explorer->selected_index = -1;
    explorer->hovered_index = -1;
    explorer->scroll_offset = 0;

    explorer_load_directory(explorer);
    return explorer;
}

void destroy_file_explorer(XPFileExplorer* explorer)
{
    if (explorer) free(explorer);
}

// -------------------------------------------------------------------------
// Drawing functions (classic XP style)
// -------------------------------------------------------------------------

static void draw_classic_toolbar(XPFileExplorer* explorer)
{
    int x = explorer_client_area_x(explorer);
    int y = explorer_toolbar_y(explorer);
    int w = explorer_client_area_width(explorer) / 10;

    fill_rectangle(x, y, w, FE_TOOLBAR_HEIGHT, 0xC0C0C0);
    draw_beveled_border_thick(x, y, w, FE_TOOLBAR_HEIGHT,0xFFFFFF, 0xC0C0C0, 0x808080, true);

    uint32_t text_color = (explorer->back_depth > 0) ? 0x000000 : 0x808080;
    draw_text("Back", x + 10, y + 4 + current_font_height, text_color, 0xC0C0C0);
}

static void draw_classic_addressbar(XPFileExplorer* explorer)
{
    int x = explorer_client_area_x(explorer);
    int y = explorer_addressbar_y(explorer) + 4;
    int w = explorer_client_area_width(explorer);

    fill_rectangle(x, y, w, FE_ADDRESSBAR_HEIGHT, 0xFFFFFF);
    draw_beveled_border_thick(x, y, w, FE_ADDRESSBAR_HEIGHT, 0x808080, 0xFFFFFF, 0xFFFFFF, false);

    draw_text(explorer->current_path, x + 6, y + (current_font_height), 0x000000, 0xFFFFFF);
}

static void draw_classic_icon(XPFileExplorer* explorer, int index, int x, int y)
{
    bool selected = (index == explorer->selected_index);
    uint32_t background_color = selected ? 0x316AC5 : 0xC0C0C0;

    //fill_rectangle(x, y, FE_ICON_SIZE, FE_ICON_SIZE, background_color);
    //draw_rect_outline(x, y, FE_ICON_SIZE, FE_ICON_SIZE, 0x808080, 1);

    // Draw icon
    if (explorer->visible_is_directory[index]) {
        if (folder_icon.resized_data) {
            image_draw(&folder_icon, x, y, FE_ICON_SIZE, FE_ICON_SIZE);
        } else {
            // fallback: yellow rectangle
            fill_rectangle(x + 10, y + 18, 28, 18, 0xFFCC00);
            draw_rect_outline(x + 10, y + 18, 28, 18, 0xAA8800, 1);
        }
    } else {
        if (file_icon.resized_data) {
            image_draw(&file_icon, x, y, FE_ICON_SIZE, FE_ICON_SIZE);
        } else {
            // fallback: white rectangle
            fill_rectangle(x + 12, y + 10, 24, 28, 0xFFFFFF);
            draw_rect_outline(x + 12, y + 10, 24, 28, 0x808080, 1);
        }
    }

    draw_text_centered(
        explorer->visible_names[index],
        x - 10,
        y + FE_ICON_SIZE + ((current_font_height + 6) / 2),
        FE_ICON_SIZE + 20,
        selected ? 0xFFFFFF : 0x000000,
        selected ? 0x316AC5 : 0xC0C0C0
    );
}

static void draw_menu(int x, int y, int w, int item_h,
                      const char** labels, int count, int hovered)
{
    int h = item_h * count;
    fill_rectangle(x, y, w, h, 0xF0F0F0);
    draw_beveled_border_thick(x, y, w, h, 0xFFFFFF, 0xF0F0F0, 0x808080, true);

    for (int i = 0; i < count; i++)
    {
        int iy = y + i * item_h;
        if (i == hovered)
            fill_rectangle(x + 1, iy, w - 2, item_h, 0x316AC5);

        draw_text(labels[i], x + 8, iy + current_font_height,
                  i == hovered ? 0xFFFFFF : 0x000000,
                  i == hovered ? 0x316AC5 : 0xF0F0F0);
    }
}

void file_explorer_draw_frame(XPFileExplorer* explorer)
{
    if (!explorer) return;

    draw_classic_toolbar(explorer);
    draw_classic_addressbar(explorer);

    int content_x = explorer_client_area_x(explorer);
    int content_y = explorer_content_y(explorer);
    int content_w = explorer_client_area_width(explorer);
    int content_h = explorer_client_area_height(explorer) - (FE_ADDRESSBAR_HEIGHT + FE_TOOLBAR_HEIGHT);

    fill_rectangle(content_x, content_y, content_w, content_h, 0xC0C0C0);

    // Get current font height (fallback to 16 if SSFN not loaded)
    int font_height = ssfn_get_font_height();
    if (font_height == 0) font_height = 16;

    int cell_w = FE_ICON_SIZE + (explorer->window->width - (FE_ICON_COLUMNS * FE_ICON_SIZE)) / (FE_ICON_COLUMNS + 1);
    int cell_h = FE_ICON_SIZE + FE_ICON_SPACING_Y + font_height + 4;   // +4 for extra padding

    for (int i = 0; i < explorer->visible_count; i++)
    {
        int col = i % FE_ICON_COLUMNS;
        int row = i / FE_ICON_COLUMNS;

        int ix = content_x + (explorer->window->width - (FE_ICON_COLUMNS * FE_ICON_SIZE)) / (FE_ICON_COLUMNS + 1) + col * cell_w;
        int iy = content_y + 6 + row * cell_h - explorer->scroll_offset;

        draw_classic_icon(explorer, i, ix, iy);
    }

    // Draw context menus if open
    if (explorer->ctx_open)
    {
        static const char* ctx_labels[FE_CTX_COUNT] = { "Open", "Start", "Delete" };
        draw_menu(explorer->ctx_x, explorer->ctx_y,
                  FE_CONTEXT_W, FE_CONTEXT_ITEM_H,
                  ctx_labels, FE_CTX_COUNT, explorer->ctx_hovered_item);
    }

    if (explorer->bgx_open)
    {
        static const char* bgx_labels[FE_BGX_COUNT] = { "New File", "New Folder", "Refresh" };
        draw_menu(explorer->bgx_x, explorer->bgx_y,
                  FE_CONTEXT_W, FE_CONTEXT_ITEM_H,
                  bgx_labels, FE_BGX_COUNT, explorer->bgx_hovered_item);
    }
}

// -------------------------------------------------------------------------
// Mouse handling
// -------------------------------------------------------------------------

static void explorer_go_back(XPFileExplorer* explorer)
{
    if (explorer->back_depth <= 0) return;
    strcpy(explorer->current_path, explorer->back_stack[--explorer->back_depth]);
    explorer_load_directory(explorer);
}

static void get_back_button_rect(XPFileExplorer* explorer, int* out_x, int* out_y, int* out_w, int* out_h)
{
    int toolbar_x = explorer_client_area_x(explorer);
    int toolbar_y = explorer_toolbar_y(explorer);
    *out_x = toolbar_x + 10;
    *out_y = toolbar_y + 8;
    *out_w = 40;
    *out_h = 16;
}

static int slot_at(XPFileExplorer* explorer, int mx, int my)
{
    int content_x = explorer_client_area_x(explorer);
    int content_y = explorer_content_y(explorer);
    int cell_w = FE_ICON_SIZE + (explorer->window->width - (FE_ICON_COLUMNS * FE_ICON_SIZE)) / (FE_ICON_COLUMNS + 1);
    int cell_h = FE_ICON_SIZE + FE_ICON_SPACING_Y + 12;

    for (int i = 0; i < explorer->visible_count; i++)
    {
        int col = i % FE_ICON_COLUMNS;
        int row = i / FE_ICON_COLUMNS;
        int ix = content_x + (explorer->window->width - (FE_ICON_COLUMNS * FE_ICON_SIZE)) / (FE_ICON_COLUMNS + 1) + col * cell_w;
        int iy = content_y + row * cell_h - explorer->scroll_offset;

        if (mx >= ix && mx <= ix + FE_ICON_SIZE &&
            my >= iy && my <= iy + FE_ICON_SIZE + 12)
            return i;
    }
    return -1;
}

static void execute_context_menu(XPFileExplorer* explorer, int item)
{
    if (explorer->ctx_target_slot < 0) return;
    char full_path[FE_MAX_PATH_LENGTH];
    strcpy(full_path, explorer->current_path);
    if (strcmp(full_path, "/") != 0)
        strcat(full_path, "/");
    strcat(full_path, explorer->visible_names[explorer->ctx_target_slot]);

    switch (item)
    {
        case 0: // Open
            if (explorer->visible_is_directory[explorer->ctx_target_slot])
            {
                if (explorer->back_depth < FE_BACK_HISTORY)
                    strcpy(explorer->back_stack[explorer->back_depth++], explorer->current_path);
                strcpy(explorer->current_path, full_path);
                explorer_load_directory(explorer);
            }
            else
            {
                if (is_png_file(explorer->visible_names[explorer->ctx_target_slot]))
                    open_image_in_viewer(full_path);
                else
                    fs_start(full_path, nullptr);
            }
            break;
        case 1: // Start (launch)
            if (!explorer->visible_is_directory[explorer->ctx_target_slot])
            {
                if (is_png_file(explorer->visible_names[explorer->ctx_target_slot]))
                    open_image_in_viewer(full_path);
                else
                    fs_start(full_path, nullptr);
            }
            break;
        case 2: // Delete
            // f_unlink(full_path); // optional
            break;
    }
}

static void execute_background_menu(XPFileExplorer* explorer, int item)
{
    char full_path[FE_MAX_PATH_LENGTH];
    char name[64];
    FRESULT res;

    switch (item)
    {
        case 0: // New File
        {
            char base_name[] = "New File";
            int attempt = 1;

            // Try "New File"
            snprintf(full_path, sizeof(full_path), "%s/%s", explorer->current_path, base_name);
            res = f_stat(full_path, NULL);
            if (res == FR_OK) {
                // File exists, try with a number suffix
                while (attempt < MAX_NAME_ATTEMPTS) {
                    char num[16];
                    int_to_str(attempt, num);
                    snprintf(name, sizeof(name), "New File %s", num);
                    snprintf(full_path, sizeof(full_path), "%s/%s", explorer->current_path, name);
                    res = f_stat(full_path, NULL);
                    if (res != FR_OK) break;
                    attempt++;
                }
            }

            // Create the file
            FIL fp;
            res = f_open(&fp, full_path, FA_CREATE_NEW | FA_WRITE);
            if (res == FR_OK) f_close(&fp);
            break;
        }

        case 1: // New Folder
        {
            char base_name[] = "New Folder";
            int attempt = 1;

            // Try "New Folder"
            snprintf(full_path, sizeof(full_path), "%s/%s", explorer->current_path, base_name);
            res = f_stat(full_path, NULL);
            if (res == FR_OK) {
                // Folder exists, try with a number suffix
                while (attempt < MAX_NAME_ATTEMPTS) {
                    char num[16];
                    int_to_str(attempt, num);
                    snprintf(name, sizeof(name), "New Folder %s", num);
                    snprintf(full_path, sizeof(full_path), "%s/%s", explorer->current_path, name);
                    res = f_stat(full_path, NULL);
                    if (res != FR_OK) break;
                    attempt++;
                }
            }
            f_mkdir(full_path);
            break;
        }

        case 2: // Refresh
            explorer_load_directory(explorer);
            return;
    }

    // After creation, reload the directory to show the new item
    explorer_load_directory(explorer);
}

void fe_handle_mouse(XPFileExplorer* explorer,
                     int mouse_x, int mouse_y,
                     bool left_clicked, bool right_clicked)
{
    if (!explorer) return;

    // ---- If a context menu is open, handle it first ----
    if (explorer->ctx_open)
    {
        int rel_y = mouse_y - explorer->ctx_y;
        int item = (mouse_x >= explorer->ctx_x && mouse_x <= explorer->ctx_x + FE_CONTEXT_W &&
                    rel_y >= 0 && rel_y < FE_CONTEXT_ITEM_H * FE_CTX_COUNT)
                   ? rel_y / FE_CONTEXT_ITEM_H : -1;
        explorer->ctx_hovered_item = item;
        if (left_clicked)
        {
            explorer->ctx_open = false;
            if (item >= 0) execute_context_menu(explorer, item);
        }
        return;
    }

    if (explorer->bgx_open)
    {
        int rel_y = mouse_y - explorer->bgx_y;
        int item = (mouse_x >= explorer->bgx_x && mouse_x <= explorer->bgx_x + FE_CONTEXT_W &&
                    rel_y >= 0 && rel_y < FE_CONTEXT_ITEM_H * FE_BGX_COUNT)
                   ? rel_y / FE_CONTEXT_ITEM_H : -1;
        explorer->bgx_hovered_item = item;
        if (left_clicked)
        {
            explorer->bgx_open = false;
            if (item >= 0) execute_background_menu(explorer, item);
        }
        return;
    }

    // ---- Back button ----
    int back_x, back_y, back_w, back_h;
    get_back_button_rect(explorer, &back_x, &back_y, &back_w, &back_h);
    if (left_clicked &&
        mouse_x >= back_x && mouse_x <= back_x + back_w &&
        mouse_y >= back_y && mouse_y <= back_y + back_h)
    {
        explorer_go_back(explorer);
        return;
    }

    // ---- Icon grid ----
    int slot = slot_at(explorer, mouse_x, mouse_y);
    explorer->hovered_index = slot;

    if (right_clicked)
    {
        if (slot >= 0)
        {
            explorer->ctx_open = true;
            explorer->ctx_x = mouse_x;
            explorer->ctx_y = mouse_y;
            explorer->ctx_target_slot = slot;
            explorer->ctx_hovered_item = -1;
            explorer->bgx_open = false;
        }
        else
        {
            explorer->bgx_open = true;
            explorer->bgx_x = mouse_x;
            explorer->bgx_y = mouse_y;
            explorer->bgx_hovered_item = -1;
            explorer->ctx_open = false;
        }
        return;
    }

    if (left_clicked)
    {
        if (slot < 0)
        {
            explorer->selected_index = -1;
            return;
        }

        explorer->selected_index = slot;

        // Single-click navigation
        if (explorer->visible_is_directory[slot])
        {
            if (explorer->back_depth < FE_BACK_HISTORY)
                strcpy(explorer->back_stack[explorer->back_depth++], explorer->current_path);
            // Build new path
            char new_path[FE_MAX_PATH_LENGTH];
            strcpy(new_path, explorer->current_path);
            if (strcmp(new_path, "/") != 0)
                strcat(new_path, "/");
            strcat(new_path, explorer->visible_names[slot]);
            strcpy(explorer->current_path, new_path);
            explorer_load_directory(explorer);
        }
        else
        {
            char full_path[FE_MAX_PATH_LENGTH];
            strcpy(full_path, explorer->current_path);
            if (strcmp(full_path, "/") != 0)
                strcat(full_path, "/");
            strcat(full_path, explorer->visible_names[slot]);
            if (is_png_file(explorer->visible_names[slot]))
                {open_image_in_viewer(full_path);}
            else
                {fs_start(full_path, nullptr);}
        }
    }
}

// -------------------------------------------------------------------------
// Window callbacks
// -------------------------------------------------------------------------

void file_explorer_draw_frame_wrapper(void* context)
{
    file_explorer_draw_frame((XPFileExplorer*)context);
}

void file_explorer_on_move(void* context, int x, int y) {}
void file_explorer_set_active(void* context) {}

void on_file_explorer_icon_click()
{
    XPWindowCallbacks* callbacks = (XPWindowCallbacks*)malloc(sizeof(XPWindowCallbacks));
    callbacks->draw_frame = file_explorer_draw_frame_wrapper;
    callbacks->on_move = file_explorer_on_move;
    callbacks->set_active = file_explorer_set_active;
    callbacks->context = nullptr;

    XPWindow* window = create_xp_window(120, 100, (46 * (SCREEN_WIDTH / 100)), (56 * (SCREEN_HEIGHT / 100)), "File Explorer", callbacks);
    window->window_type = WINDOW_TYPE_EXPLORER;

    XPFileExplorer* explorer = create_file_explorer(window);
    window->context = explorer;

    set_active_xp_window(window);
}