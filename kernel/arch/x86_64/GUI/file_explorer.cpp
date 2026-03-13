#include <file_explorer.h>
#include <GUI.h>
#include <window.h>
#include <button.h>
#include <gui_primitives.h>
#include <framebufferutil.h>
#include <psf.h>
#include <liballoc.h>
#include <ramdisk.h>
#include <timer.h>
#include <string.h>
#include <stdio.h>


static int client_x(XPFileExplorer* fe)
{
    return fe->window->x + WINDOW_BORDER_WIDTH;
}

static int toolbar_y(XPFileExplorer* fe)
{
    return fe->window->y + TITLE_BAR_HEIGHT + WINDOW_BORDER_WIDTH;
}

static int address_y(XPFileExplorer* fe)
{
    return toolbar_y(fe) + FE_TOOLBAR_H;
}

static int grid_y(XPFileExplorer* fe)
{
    return address_y(fe) + FE_ADDRESS_H + FE_ICON_PAD_Y;
}

static int client_w(XPFileExplorer* fe)
{
    return fe->window->width - 2 * WINDOW_BORDER_WIDTH;
}

static int client_bottom(XPFileExplorer* fe)
{
    return fe->window->y + fe->window->height - WINDOW_BORDER_WIDTH;
}

static void slot_pos(XPFileExplorer* fe, int slot, int* ox, int* oy)
{
    int cell = FE_ICON_SIZE + FE_ICON_PAD_X;
    *ox = client_x(fe) + FE_ICON_PAD_X + (slot % FE_ICON_COLS) * cell;
    *oy = grid_y(fe)   + (slot / FE_ICON_COLS) * (FE_ICON_SIZE + FE_ICON_PAD_Y + 12)
          - fe->scroll_y;
}

static int slot_at(XPFileExplorer* fe, int mx, int my)
{
    for (int s = 0; s < fe->visible_count; s++)
    {
        int sx, sy;
        slot_pos(fe, s, &sx, &sy);
        if (mx >= sx && mx <= sx + FE_ICON_SIZE &&
            my >= sy && my <= sy + FE_ICON_SIZE + 12)
            return s;
    }
    return -1;
}

static bool in_back_button(XPFileExplorer* fe, int mx, int my)
{
    int x = client_x(fe) + 4;
    int y = toolbar_y(fe) + 4;
    return (mx >= x && mx <= x + 48 && my >= y && my <= y + 20);
}


static void fe_load_dir(XPFileExplorer* fe)
{
    fe->visible_count = 0;
    fe->selected_slot = -1;
    fe->hovered_slot  = -1;
    fe->scroll_y      = 0;
    fe->ctx_open      = false;
    fe->bgx_open      = false;

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (fs.files[i].used && fs.files[i].parent_index == fe->dir_index)
            fe->visible_files[fe->visible_count++] = i;
    }

    if (fe->dir_index == -1) { fe->address[0] = '/'; fe->address[1] = '\0'; }
    else strncpy(fe->address, fs.files[fe->dir_index].path, MAX_PATH);
}

static void fe_navigate(XPFileExplorer* fe, int new_dir)
{
    if (fe->back_depth < FE_BACK_HISTORY)
        fe->back_stack[fe->back_depth++] = fe->dir_index;
    fe->dir_index = new_dir;
    fe_load_dir(fe);
}

static void fe_go_back(XPFileExplorer* fe)
{
    if (fe->back_depth <= 0) return;
    fe->dir_index = fe->back_stack[--fe->back_depth];
    fe_load_dir(fe);
}


XPFileExplorer* create_file_explorer(XPWindow* window)
{
    XPFileExplorer* fe = (XPFileExplorer*)malloc(sizeof(XPFileExplorer));
    if (!fe) return nullptr;

    fe->window            = window;
    fe->dir_index         = current_dir_index;
    fe->selected_slot     = -1;
    fe->hovered_slot      = -1;
    fe->visible_count     = 0;
    fe->last_clicked_slot = -1;
    fe->last_click_frame  = 0;
    fe->ctx_open          = false;
    fe->ctx_target_slot   = -1;
    fe->ctx_hovered_item  = -1;
    fe->bgx_open          = false;
    fe->bgx_hovered_item  = -1;
    fe->back_depth        = 0;
    fe->scroll_y          = 0;

    fe_load_dir(fe);
    return fe;
}

void destroy_file_explorer(XPFileExplorer* fe) { if (fe) free(fe); }


static void fe_draw_toolbar(XPFileExplorer* fe)
{
    int x = client_x(fe);
    int y = toolbar_y(fe);
    int w = client_w(fe);

    fill_rectangle(x, y, w, FE_TOOLBAR_H, 0xD4D0C8);
    draw_hline(x, y + FE_TOOLBAR_H - 1, w, 0x808080);

    bool can_back = (fe->back_depth > 0);
    uint32_t btn_bg = can_back ? 0xD4D0C8 : 0xBBB8B0;

    fill_rectangle(x + 4, y + 4, 64, 20, btn_bg);
    draw_beveled_border_thick(x + 4, y + 4, 64, 20,
                              can_back ? 0xFFFFFF : 0xD4D0C8,
                              btn_bg,
                              can_back ? 0x808080 : 0xD4D0C8,
                              true);

    uint32_t arrow_col = can_back ? 0x000000 : 0x999999;
    draw_text("< Back", x + 8, y + 8, arrow_col, btn_bg);
}

static void fe_draw_address_bar(XPFileExplorer* fe)
{
    int x = client_x(fe);
    int y = address_y(fe);
    int w = client_w(fe);

    fill_rectangle(x, y, w, FE_ADDRESS_H, 0xFFFFFF);
    draw_beveled_border_thick(x, y, w, FE_ADDRESS_H,
                              0x808080, 0xFFFFFF, 0xFFFFFF, false);
    draw_text(fe->address, x + 4, y + 6, 0x000000, 0xFFFFFF);
}

static void fe_draw_icon(XPFileExplorer* fe, int slot, int sx, int sy)
{
    int  fi     = fe->visible_files[slot];
    bool is_dir = fs.files[fi].is_dir;
    bool sel    = (slot == fe->selected_slot);
    bool hov    = (slot == fe->hovered_slot);

    uint32_t bg = sel ? 0x316AC5 : hov ? 0xD5E4F7 : XP_BACKGROUND;

    fill_rectangle(sx, sy, FE_ICON_SIZE, FE_ICON_SIZE, bg);

    if (is_dir)
    {
        fill_rectangle(sx + 8,  sy + 16, 32, 22, 0xFFCC00);
        fill_rectangle(sx + 8,  sy + 12, 14,  6, 0xFFCC00);
        draw_rect_outline(sx + 8, sy + 12, 32, 26, 0xCC8800, 1);
    }
    else
    {
        fill_rectangle(sx + 12, sy + 8, 24, 30, 0xFFFFFF);
        draw_rect_outline(sx + 12, sy + 8, 24, 30, 0x808080, 1);
        draw_line(sx + 28, sy + 8,  sx + 36, sy + 16, 0x808080);
        fill_rectangle(sx + 28, sy + 8, 8, 8, XP_BACKGROUND);
        draw_line(sx + 28, sy + 8,  sx + 28, sy + 16, 0x808080);
        draw_line(sx + 28, sy + 16, sx + 36, sy + 16, 0x808080);
    }

    char label[10];
    strncpy(label, fs.files[fi].name, 8);
    label[8] = '\0';

    draw_text_centered(label, sx, sy + FE_ICON_SIZE + 2,
                       FE_ICON_SIZE,
                       sel ? 0xFFFFFF : 0x000000,
                       sel ? 0x316AC5 : XP_BACKGROUND);
}

static void fe_draw_menu(int x, int y, int w, int item_h,
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

        draw_text(labels[i], x + 8, iy + 4,
                  i == hovered ? 0xFFFFFF : 0x000000,
                  i == hovered ? 0x316AC5 : 0xF0F0F0);
    }
}

void fe_draw_frame(XPFileExplorer* fe)
{
    if (!fe || !fe->window) return;

    fe_draw_toolbar(fe);
    fe_draw_address_bar(fe);

    int gx = client_x(fe);
    int gy = address_y(fe) + FE_ADDRESS_H;
    int gw = client_w(fe);
    int gh = client_bottom(fe) - gy;
    fill_rectangle(gx, gy, gw, gh, XP_BACKGROUND);

    for (int s = 0; s < fe->visible_count; s++)
    {
        int sx, sy;
        slot_pos(fe, s, &sx, &sy);
        if (sy + FE_ICON_SIZE + 12 < gy || sy > client_bottom(fe)) continue;
        fe_draw_icon(fe, s, sx, sy);
    }

    if (fe->ctx_open)
    {
        static const char* ctx_labels[FE_CTX_COUNT] = { "Open", "Start", "Delete" };
        fe_draw_menu(fe->ctx_x, fe->ctx_y,
                     FE_CONTEXT_W, FE_CONTEXT_ITEM_H,
                     ctx_labels, FE_CTX_COUNT, fe->ctx_hovered_item);
    }

    if (fe->bgx_open)
    {
        static const char* bgx_labels[FE_BGX_COUNT] = { "New File", "New Folder", "Refresh" };
        fe_draw_menu(fe->bgx_x, fe->bgx_y,
                     FE_CONTEXT_W, FE_CONTEXT_ITEM_H,
                     bgx_labels, FE_BGX_COUNT, fe->bgx_hovered_item);
    }
}


static void fe_ctx_execute(XPFileExplorer* fe, int item)
{
    if (fe->ctx_target_slot < 0) return;
    int fi = fe->visible_files[fe->ctx_target_slot];

    int saved = current_dir_index;
    current_dir_index = fe->dir_index;

    switch (item)
    {
        case FE_CTX_OPEN:
            if (fs.files[fi].is_dir)
            {
                current_dir_index = saved;
                fe_navigate(fe, fi);
                return;
            }
            break;
        case FE_CTX_START:
            if (!fs.files[fi].is_dir)
                fs_start(fs.files[fi].name, nullptr);
            break;
        case FE_CTX_DELETE:
            fs_delete(fs.files[fi].name);
            current_dir_index = saved;
            fe_load_dir(fe);
            return;
    }

    current_dir_index = saved;
}

static void fe_bgx_execute(XPFileExplorer* fe, int item)
{
    int saved = current_dir_index;
    current_dir_index = fe->dir_index;

    char name[MAX_NAME];

    switch (item)
    {
        case FE_BGX_NEW_FILE:
        {
            strncpy(name, "newfile", MAX_NAME);
            int n = 1;
            while (fs_create(name) == -2)
            {
                name[0] = '\0';
                strcat(name, "newfile");
                char num[8];
                int tmp = n++;
                int i = 0;
                if (tmp == 0) { num[i++] = '0'; }
                else { while (tmp > 0) { num[i++] = '0' + tmp % 10; tmp /= 10; } }
                num[i] = '\0';
                // reverse
                for (int a = 0, b = i-1; a < b; a++, b--)
                { char t = num[a]; num[a] = num[b]; num[b] = t; }
                strcat(name, num);
            }
            break;
        }
        case FE_BGX_NEW_DIR:
        {
            strncpy(name, "NewFolder", MAX_NAME);
            int n = 1;
            while (fs_mkdir(name) == -2)
            {
                name[0] = '\0';
                strcat(name, "NewFolder");
                char num[8];
                int tmp = n++;
                int i = 0;
                if (tmp == 0) { num[i++] = '0'; }
                else { while (tmp > 0) { num[i++] = '0' + tmp % 10; tmp /= 10; } }
                num[i] = '\0';
                for (int a = 0, b = i-1; a < b; a++, b--)
                { char t = num[a]; num[a] = num[b]; num[b] = t; }
                strcat(name, num);
            }
            break;
        }
        case FE_BGX_REFRESH:
            break;
    }

    current_dir_index = saved;
    fe_load_dir(fe);
}


void fe_handle_mouse(XPFileExplorer* fe,
                     int mx, int my,
                     bool left_clicked, bool right_clicked)
{
    if (!fe) return;

    if (fe->ctx_open)
    {
        int rel_y = my - fe->ctx_y;
        int item  = (mx >= fe->ctx_x && mx <= fe->ctx_x + FE_CONTEXT_W &&
                     rel_y >= 0 && rel_y < FE_CONTEXT_ITEM_H * FE_CTX_COUNT)
                    ? rel_y / FE_CONTEXT_ITEM_H : -1;
        fe->ctx_hovered_item = item;
        if (left_clicked) { fe->ctx_open = false; if (item >= 0) fe_ctx_execute(fe, item); }
        return;
    }

    if (fe->bgx_open)
    {
        int rel_y = my - fe->bgx_y;
        int item  = (mx >= fe->bgx_x && mx <= fe->bgx_x + FE_CONTEXT_W &&
                     rel_y >= 0 && rel_y < FE_CONTEXT_ITEM_H * FE_BGX_COUNT)
                    ? rel_y / FE_CONTEXT_ITEM_H : -1;
        fe->bgx_hovered_item = item;
        if (left_clicked) { fe->bgx_open = false; if (item >= 0) fe_bgx_execute(fe, item); }
        return;
    }

    if (left_clicked && in_back_button(fe, mx, my))
    {
        fe_go_back(fe);
        return;
    }

    fe->hovered_slot = slot_at(fe, mx, my);

    if (right_clicked)
    {
        int s = slot_at(fe, mx, my);
        if (s >= 0)
        {
            fe->ctx_open        = true;
            fe->ctx_x           = mx;
            fe->ctx_y           = my;
            fe->ctx_target_slot = s;
            fe->ctx_hovered_item = -1;
            fe->bgx_open        = false;
        }
        else
        {
            fe->bgx_open         = true;
            fe->bgx_x            = mx;
            fe->bgx_y            = my;
            fe->bgx_hovered_item = -1;
            fe->ctx_open         = false;
        }
        return;
    }

    if (left_clicked)
    {
        int s = slot_at(fe, mx, my);
        if (s < 0) { fe->selected_slot = -1; return; }

        bool dbl = (s == fe->last_clicked_slot &&
                    GUI_frame - fe->last_click_frame < 30);

        fe->last_clicked_slot = s;
        fe->last_click_frame  = GUI_frame;
        fe->selected_slot     = s;

        if (dbl)
        {
            int fi = fe->visible_files[s];
            if (fs.files[fi].is_dir)
            {
                fe_navigate(fe, fi);
            }
            else
            {
                int saved = current_dir_index;
                current_dir_index = fe->dir_index;
                fs_start(fs.files[fi].name, nullptr);
                current_dir_index = saved;
            }
        }
    }
}


void fe_draw_frame_wrapper(void* ctx) { fe_draw_frame(static_cast<XPFileExplorer*>(ctx)); }
void fe_on_move(void* /*ctx*/, int /*x*/, int /*y*/) {}
void fe_set_active(void* /*ctx*/) {}

void on_file_explorer_icon_click()
{
    XPWindowCallbacks* cb = (XPWindowCallbacks*)malloc(sizeof(XPWindowCallbacks));
    cb->draw_frame = fe_draw_frame_wrapper;
    cb->on_move    = fe_on_move;
    cb->set_active = fe_set_active;
    cb->context    = nullptr;

    XPWindow*       win = create_xp_window(100, 80, 600, 450, "File Explorer", cb);
    win->window_type    = WINDOW_TYPE_EXPLORER;
    XPFileExplorer* fe  = create_file_explorer(win);
    win->context        = (void*)fe;

    set_active_xp_window(win);
}