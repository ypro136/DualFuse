#include <task_manager.h>

#include <cstdint>
#include <liballoc.h>
#include <string.h>
#include <stdio.h>

#include <gui_primitives.h>
#include <framebufferutil.h>
#include <GUI.h>
#include <psf.h>
#include <timer.h>
#include <window.h>
#include <button.h>
#include <task.h>
#include <pmm.h>
#include <system.h>
#include <bootloader.h>

extern int current_font_height;

struct TaskManagerLayout {
    int client_x, client_y, client_w, client_h;
    int row_h;
    int header_row_h;
    int process_list_area_h;
    int scrollable_area_h;
    int visible_row_count;
    int list_content_w;
    int scrollbar_x, scrollbar_y, scrollbar_h;
    int col_pid_x, col_state_x, col_type_x, col_name_x;
    int memory_section_y;
    int sysinfo_section_y;
};

static TaskManagerLayout task_manager_compute_layout(TaskManagerState* state) {
    TaskManagerLayout layout;
    XPWindow* window = state->window;

    layout.client_x = window->x + WINDOW_BORDER_WIDTH;
    layout.client_y = window->y + TITLE_BAR_HEIGHT + WINDOW_BORDER_WIDTH;
    layout.client_w = window->width  - 2 * WINDOW_BORDER_WIDTH;
    layout.client_h = window->height - TITLE_BAR_HEIGHT - 2 * WINDOW_BORDER_WIDTH;

    layout.row_h        = current_font_height + 2 * TASK_MANAGER_ROW_PADDING;
    layout.header_row_h = layout.row_h;

    int bottom_strip_h         = TASK_MANAGER_MEMORY_BAR_SECTION_H + TASK_MANAGER_SYSINFO_SECTION_H;
    layout.process_list_area_h = layout.client_h - bottom_strip_h;
    layout.scrollable_area_h   = layout.process_list_area_h - layout.header_row_h;
    layout.visible_row_count   = (layout.scrollable_area_h > 0) ? (layout.scrollable_area_h / layout.row_h) : 0;
    layout.list_content_w      = layout.client_w - TASK_MANAGER_SCROLLBAR_W;

    layout.scrollbar_x = layout.client_x + layout.client_w - TASK_MANAGER_SCROLLBAR_W;
    layout.scrollbar_y = layout.client_y + layout.header_row_h;
    layout.scrollbar_h = layout.scrollable_area_h;

    layout.col_pid_x   = layout.client_x;
    layout.col_state_x = layout.col_pid_x   + TASK_MANAGER_COL_PID_W;
    layout.col_type_x  = layout.col_state_x + TASK_MANAGER_COL_STATE_W;
    layout.col_name_x  = layout.col_type_x  + TASK_MANAGER_COL_TYPE_W;

    layout.memory_section_y  = layout.client_y + layout.process_list_area_h;
    layout.sysinfo_section_y = layout.memory_section_y + TASK_MANAGER_MEMORY_BAR_SECTION_H;

    return layout;
}

static const char* task_manager_state_to_string(uint8_t task_state) {
    switch (task_state) {
        case TASK_STATE_DEAD:                   return "DEAD";
        case TASK_STATE_READY:                  return "READY";
        case TASK_STATE_IDLE:                   return "IDLE";
        case TASK_STATE_WAITING_INPUT:          return "WAIT INPUT";
        case TASK_STATE_CREATED:                return "CREATED";
        case TASK_STATE_WAITING_CHILD:          return "WAIT CHILD";
        case TASK_STATE_WAITING_CHILD_SPECIFIC: return "WAIT PID";
        case TASK_STATE_WAITING_VFORK:          return "WAIT VFORK";
        case TASK_STATE_BLOCKED:                return "BLOCKED";
        case TASK_STATE_SIGKILLED:              return "SIGKILLED";
        case TASK_STATE_FUTEX:                  return "FUTEX";
        case TASK_STATE_DUMMY:                  return "DUMMY";
        default:                                return "UNKNOWN";
    }
}

static void task_manager_load_cpu_brand_string(TaskManagerState* state) {
    char* brand_buffer = state->cpu_brand_string;
    uint32_t eax, ebx, ecx, edx;

    for (int cpuid_leaf_offset = 0; cpuid_leaf_offset < 3; cpuid_leaf_offset++) {
        eax = 0x80000002u + (uint32_t)cpuid_leaf_offset;
        ebx = 0;
        ecx = 0;
        edx = 0;
        cpuid(&eax, &ebx, &ecx, &edx);

        char* dest_ptr = brand_buffer + cpuid_leaf_offset * 16;
        *(uint32_t*)(dest_ptr +  0) = eax;
        *(uint32_t*)(dest_ptr +  4) = ebx;
        *(uint32_t*)(dest_ptr +  8) = ecx;
        *(uint32_t*)(dest_ptr + 12) = edx;
    }
    brand_buffer[48] = '\0';

    char* trimmed_brand_start = brand_buffer;
    while (*trimmed_brand_start == ' ') trimmed_brand_start++;
    if (trimmed_brand_start != brand_buffer) {
        int trimmed_len = strlen(trimmed_brand_start);
        memmove(brand_buffer, trimmed_brand_start, (size_t)(trimmed_len + 1));
    }

    state->cpu_brand_string_loaded = true;
}

static void task_manager_update_button_positions(TaskManagerState* state, const TaskManagerLayout& layout) {
    int button_y = layout.sysinfo_section_y + TASK_MANAGER_SYSINFO_SECTION_H - TASK_MANAGER_BUTTON_H - 4;

    if (state->kill_selected_button) {
        state->kill_selected_button->x = layout.client_x + layout.client_w - TASK_MANAGER_BUTTON_W - 4;
        state->kill_selected_button->y = button_y;
    }
    if (state->refresh_button) {
        state->refresh_button->x = layout.client_x + layout.client_w - 2 * TASK_MANAGER_BUTTON_W - 10;
        state->refresh_button->y = button_y;
    }
}

static void task_manager_draw_process_list(TaskManagerState* state, const TaskManagerLayout& layout) {
    int max_scroll_offset = state->snapshot_task_count - layout.visible_row_count;
    if (max_scroll_offset < 0) max_scroll_offset = 0;
    if (state->scroll_offset_rows > max_scroll_offset) state->scroll_offset_rows = max_scroll_offset;
    if (state->scroll_offset_rows < 0)                 state->scroll_offset_rows = 0;

    fill_rectangle(layout.client_x, layout.client_y, layout.list_content_w, layout.header_row_h, XP_BUTTON_SHADOW);

    int header_text_baseline_y = layout.client_y + TASK_MANAGER_ROW_PADDING + current_font_height;
    draw_text("PID",   layout.col_pid_x   + 2, header_text_baseline_y, 0xFFFFFF, XP_BUTTON_SHADOW);
    draw_text("State", layout.col_state_x + 2, header_text_baseline_y, 0xFFFFFF, XP_BUTTON_SHADOW);
    draw_text("Type",  layout.col_type_x  + 2, header_text_baseline_y, 0xFFFFFF, XP_BUTTON_SHADOW);
    draw_text("Name",  layout.col_name_x  + 2, header_text_baseline_y, 0xFFFFFF, XP_BUTTON_SHADOW);

    int rows_start_y = layout.client_y + layout.header_row_h;

    for (int visible_row_index = 0; visible_row_index < layout.visible_row_count; visible_row_index++) {
        int snapshot_index = visible_row_index + state->scroll_offset_rows;
        int row_y = rows_start_y + visible_row_index * layout.row_h;

        if (snapshot_index >= state->snapshot_task_count) {
            fill_rectangle(layout.client_x, row_y, layout.list_content_w, layout.row_h, XP_BUTTON_FACE);
            continue;
        }

        bool row_is_selected = (snapshot_index == state->selected_snapshot_index);
        bool row_index_is_even = (visible_row_index % 2 == 0);

        uint32_t row_background_color = row_is_selected  ? 0x316AC5u
                                      : row_index_is_even ? (uint32_t)XP_BUTTON_FACE
                                      :                     0xD8D8D8u;
        uint32_t row_foreground_color = row_is_selected ? 0xFFFFFFu : (uint32_t)XP_WINDOW_TEXT;

        fill_rectangle(layout.client_x, row_y, layout.list_content_w, layout.row_h, row_background_color);

        int text_baseline_y = row_y + TASK_MANAGER_ROW_PADDING + current_font_height;

        char pid_string[16];
        snprintf(pid_string, sizeof(pid_string), "%d", (int)state->snapshot_entries[snapshot_index].task_id);
        draw_text(pid_string, layout.col_pid_x + 2, text_baseline_y, row_foreground_color, row_background_color);

        const char* state_string = task_manager_state_to_string(state->snapshot_entries[snapshot_index].task_state);
        draw_text(state_string, layout.col_state_x + 2, text_baseline_y, row_foreground_color, row_background_color);

        const char* type_string = state->snapshot_entries[snapshot_index].is_kernel_task ? "KERNEL" : "USER";
        draw_text(type_string, layout.col_type_x + 2, text_baseline_y, row_foreground_color, row_background_color);

        draw_text(state->snapshot_entries[snapshot_index].task_display_name,
                  layout.col_name_x + 2, text_baseline_y,
                  row_foreground_color, row_background_color);
    }

    if (state->snapshot_task_count > layout.visible_row_count) {
        int scroll_pos_pixels = state->scroll_offset_rows * layout.row_h;
        int max_scroll_pixels = state->snapshot_task_count * layout.row_h;
        draw_scrollbar(layout.scrollbar_x, layout.scrollbar_y, layout.scrollbar_h,
                       scroll_pos_pixels, max_scroll_pixels);
    } else {
        fill_rectangle(layout.scrollbar_x, layout.scrollbar_y,
                       TASK_MANAGER_SCROLLBAR_W, layout.scrollbar_h, XP_BUTTON_FACE);
    }
}

static void task_manager_draw_memory_section(TaskManagerState* state, const TaskManagerLayout& layout) {
    int section_x = layout.client_x;
    int section_y = layout.memory_section_y;
    int section_w = layout.client_w;

    fill_rectangle(section_x, section_y, section_w, TASK_MANAGER_MEMORY_BAR_SECTION_H, XP_BUTTON_FACE);

    uint64_t used_mib  = (physical_used_blocks_count  * 4096ULL) / (1024ULL * 1024ULL);
    uint64_t total_mib = (physical_total_blocks_count * 4096ULL) / (1024ULL * 1024ULL);
    if (total_mib == 0) total_mib = 1;

    char memory_label_string[64];
    snprintf(memory_label_string, sizeof(memory_label_string),
             "Memory: %d MiB used / %d MiB total", (int)used_mib, (int)total_mib);

    int label_baseline_y = section_y + TASK_MANAGER_ROW_PADDING + current_font_height;
    draw_text(memory_label_string, section_x + 8, label_baseline_y, XP_WINDOW_TEXT, XP_BUTTON_FACE);

    int bar_x = section_x + 8;
    int bar_y = label_baseline_y + 6;
    int bar_w = section_w - 80;
    int bar_h = 14;
    if (bar_w < 0) bar_w = 0;

    fill_rectangle(bar_x, bar_y, bar_w, bar_h, 0xFFFFFF);
    draw_rect_outline(bar_x, bar_y, bar_w, bar_h, XP_BUTTON_SHADOW, 1);

    int filled_bar_pixel_width = (physical_total_blocks_count > 0)
        ? (int)((uint64_t)bar_w * physical_used_blocks_count / physical_total_blocks_count)
        : 0;
    if (filled_bar_pixel_width > bar_w) filled_bar_pixel_width = bar_w;
    if (filled_bar_pixel_width > 0)
        fill_rectangle(bar_x, bar_y, filled_bar_pixel_width, bar_h, 0x316AC5);

    int memory_percent = (physical_total_blocks_count > 0)
        ? (int)(physical_used_blocks_count * 100ULL / physical_total_blocks_count)
        : 0;
    char percent_string[8];
    snprintf(percent_string, sizeof(percent_string), "%d%%", memory_percent);
    draw_text(percent_string, bar_x + bar_w + 6, bar_y + current_font_height, XP_WINDOW_TEXT, XP_BUTTON_FACE);
}

static void task_manager_draw_sysinfo_and_buttons_section(TaskManagerState* state, const TaskManagerLayout& layout) {
    int section_x = layout.client_x;
    int section_y = layout.sysinfo_section_y;
    int section_w = layout.client_w;

    fill_rectangle(section_x, section_y, section_w, TASK_MANAGER_SYSINFO_SECTION_H, XP_BUTTON_FACE);

    int sysinfo_line_baseline_y = section_y + TASK_MANAGER_ROW_PADDING + current_font_height;

    if (state->cpu_brand_string_loaded) {
        draw_text(state->cpu_brand_string, section_x + 8, sysinfo_line_baseline_y, XP_WINDOW_TEXT, XP_BUTTON_FACE);
    }

    char resolution_freq_string[48];
    snprintf(resolution_freq_string, sizeof(resolution_freq_string),
             "%dx%d  %dHz", SCREEN_WIDTH, SCREEN_HEIGHT, (int)frequency);
    int resolution_text_x = section_x + section_w
                          - get_text_width(resolution_freq_string)
                          - 2 * TASK_MANAGER_BUTTON_W - 14;
    if (resolution_text_x > section_x + 8)
        draw_text(resolution_freq_string, resolution_text_x, sysinfo_line_baseline_y, XP_WINDOW_TEXT, XP_BUTTON_FACE);

    if (state->status_message[0] != '\0') {
        int status_bg_top_y  = sysinfo_line_baseline_y + 6;
        int status_bg_height = current_font_height + 4;
        int status_text_y    = status_bg_top_y + current_font_height;
        fill_rectangle(section_x + 8, status_bg_top_y, 340, status_bg_height, 0xFFE0E0);
        draw_rect_outline(section_x + 8, status_bg_top_y, 340, status_bg_height, 0xCC8080, 1);
        draw_text(state->status_message, section_x + 12, status_text_y, 0xCC0000, 0xFFE0E0);
    }

    task_manager_update_button_positions(state, layout);
    if (state->kill_selected_button) draw_xp_button(state->kill_selected_button);
    if (state->refresh_button)       draw_xp_button(state->refresh_button);
}

void task_manager_refresh_snapshot(TaskManagerState* state) {
    state->snapshot_task_count = 0;

    spinlock_cnt_read_acquire(&TASK_LL_MODIFY);

    Task* task_list_entry = firstTask;
    while (task_list_entry != nullptr && state->snapshot_task_count < TASK_MANAGER_MAX_SNAPSHOT_TASKS) {
        TaskManagerSnapshotEntry* snapshot_entry = &state->snapshot_entries[state->snapshot_task_count];

        snapshot_entry->task_id        = task_list_entry->id;
        snapshot_entry->task_state     = task_list_entry->state;
        snapshot_entry->is_kernel_task = task_list_entry->kernel_task;

        const char* task_name_source = nullptr;
        if (task_list_entry->cmdline && task_list_entry->cmdline[0] != '\0') {
            task_name_source = task_list_entry->cmdline;
        } else if (task_list_entry->execname && task_list_entry->execname[0] != '\0') {
            task_name_source = task_list_entry->execname;
        } else {
            task_name_source = task_list_entry->kernel_task ? "kernel" : "unknown";
        }

        strncpy(snapshot_entry->task_display_name, task_name_source, TASK_MANAGER_TASK_NAME_BUFFER_LEN - 1);
        snapshot_entry->task_display_name[TASK_MANAGER_TASK_NAME_BUFFER_LEN - 1] = '\0';

        state->snapshot_task_count++;
        task_list_entry = task_list_entry->next;
    }

    spinlock_cnt_read_release(&TASK_LL_MODIFY);

    state->snapshot_last_refresh_frame = GUI_frame;
}

TaskManagerState* create_task_manager(XPWindow* window) {
    TaskManagerState* state = (TaskManagerState*)malloc(sizeof(TaskManagerState));
    if (!state) return nullptr;

    state->window                      = window;
    state->snapshot_task_count         = 0;
    state->snapshot_last_refresh_frame = 0;
    state->selected_snapshot_index     = -1;
    state->scroll_offset_rows          = 0;
    state->status_message[0]           = '\0';
    state->status_message_expire_frame = 0;
    state->cpu_brand_string_loaded     = false;

    state->kill_selected_button = create_xp_button(nullptr, 0, 0,
                                                   TASK_MANAGER_BUTTON_W, TASK_MANAGER_BUTTON_H,
                                                   "Kill Selected", nullptr);
    state->refresh_button       = create_xp_button(nullptr, 0, 0,
                                                   TASK_MANAGER_BUTTON_W, TASK_MANAGER_BUTTON_H,
                                                   "Refresh", nullptr);

    task_manager_load_cpu_brand_string(state);
    task_manager_refresh_snapshot(state);

    return state;
}

void destroy_task_manager(TaskManagerState* state) {
    if (!state) return;

    if (state->kill_selected_button) {
        free(state->kill_selected_button);
        state->kill_selected_button = nullptr;
    }
    if (state->refresh_button) {
        free(state->refresh_button);
        state->refresh_button = nullptr;
    }
    free(state);
}

void task_manager_draw_frame(TaskManagerState* state) {
    if (!state || !state->window) return;

    if (GUI_frame - state->snapshot_last_refresh_frame >= TASK_MANAGER_SNAPSHOT_REFRESH_INTERVAL_FRAMES)
        task_manager_refresh_snapshot(state);

    if (state->status_message[0] != '\0' && GUI_frame > state->status_message_expire_frame)
        state->status_message[0] = '\0';

    TaskManagerLayout layout = task_manager_compute_layout(state);

    fill_rectangle(layout.client_x, layout.client_y, layout.client_w, layout.client_h, XP_BUTTON_FACE);

    task_manager_draw_process_list(state, layout);

    draw_hline(layout.client_x, layout.memory_section_y, layout.client_w, XP_BUTTON_SHADOW);
    task_manager_draw_memory_section(state, layout);

    draw_hline(layout.client_x, layout.sysinfo_section_y, layout.client_w, XP_BUTTON_SHADOW);
    task_manager_draw_sysinfo_and_buttons_section(state, layout);
}

void task_manager_handle_mouse(TaskManagerState* state, int mouse_x, int mouse_y,
                               bool left_button_clicked, bool right_button_clicked) {
    (void)right_button_clicked;
    if (!state || !state->window || !left_button_clicked) return;

    TaskManagerLayout layout = task_manager_compute_layout(state);

    if (state->kill_selected_button) {
        XPButton* kill_btn = state->kill_selected_button;
        bool mouse_is_over_kill_button = (mouse_x >= kill_btn->x &&
                                          mouse_x <= kill_btn->x + kill_btn->width &&
                                          mouse_y >= kill_btn->y &&
                                          mouse_y <= kill_btn->y + kill_btn->height);
        if (mouse_is_over_kill_button) {
            if (state->selected_snapshot_index < 0) {
                strncpy(state->status_message, "No task selected.",
                        sizeof(state->status_message) - 1);
            } else {
                uint64_t selected_task_id = state->snapshot_entries[state->selected_snapshot_index].task_id;
                if (selected_task_id == 0) {
                    strncpy(state->status_message, "Cannot kill kernel task (id 0).",
                            sizeof(state->status_message) - 1);
                } else {
                    task_kill((uint32_t)selected_task_id, 0);
                    state->selected_snapshot_index = -1;
                    task_manager_refresh_snapshot(state);
                    return;
                }
            }
            state->status_message[sizeof(state->status_message) - 1] = '\0';
            state->status_message_expire_frame = GUI_frame + TASK_MANAGER_STATUS_MESSAGE_DISPLAY_FRAMES;
            return;
        }
    }

    if (state->refresh_button) {
        XPButton* refresh_btn = state->refresh_button;
        bool mouse_is_over_refresh_button = (mouse_x >= refresh_btn->x &&
                                             mouse_x <= refresh_btn->x + refresh_btn->width &&
                                             mouse_y >= refresh_btn->y &&
                                             mouse_y <= refresh_btn->y + refresh_btn->height);
        if (mouse_is_over_refresh_button) {
            task_manager_refresh_snapshot(state);
            return;
        }
    }

    bool mouse_is_over_scrollbar_x_range = (mouse_x >= layout.scrollbar_x &&
                                            mouse_x <= layout.scrollbar_x + TASK_MANAGER_SCROLLBAR_W);

    if (mouse_is_over_scrollbar_x_range &&
        mouse_y >= layout.scrollbar_y &&
        mouse_y <= layout.scrollbar_y + 16) {
        if (state->scroll_offset_rows > 0) state->scroll_offset_rows--;
        return;
    }

    if (mouse_is_over_scrollbar_x_range &&
        mouse_y >= layout.scrollbar_y + layout.scrollbar_h - 16 &&
        mouse_y <= layout.scrollbar_y + layout.scrollbar_h) {
        int max_scroll_offset = state->snapshot_task_count - layout.visible_row_count;
        if (max_scroll_offset < 0) max_scroll_offset = 0;
        if (state->scroll_offset_rows < max_scroll_offset) state->scroll_offset_rows++;
        return;
    }

    int rows_start_y = layout.client_y + layout.header_row_h;
    bool mouse_is_in_process_list_rows = (mouse_x >= layout.client_x &&
                                          mouse_x <  layout.scrollbar_x &&
                                          mouse_y >= rows_start_y &&
                                          mouse_y <  layout.client_y + layout.process_list_area_h);
    if (mouse_is_in_process_list_rows) {
        int clicked_visible_row_index = (mouse_y - rows_start_y) / layout.row_h;
        int clicked_snapshot_index    = clicked_visible_row_index + state->scroll_offset_rows;
        if (clicked_snapshot_index >= 0 && clicked_snapshot_index < state->snapshot_task_count)
            state->selected_snapshot_index = clicked_snapshot_index;
    }
}

void task_manager_handle_keyboard(TaskManagerState* state, char key_char) {
    (void)state;
    (void)key_char;
}

void task_manager_draw_frame_wrapper(void* ctx) {
    task_manager_draw_frame(static_cast<TaskManagerState*>(ctx));
}

void task_manager_on_move(void* ctx, int x, int y) {
    (void)ctx; (void)x; (void)y;
}

void task_manager_set_active(void* ctx) {
    (void)ctx;
}

void on_task_manager_icon_click() {
    XPWindowCallbacks* task_manager_window_callbacks = (XPWindowCallbacks*)malloc(sizeof(XPWindowCallbacks));
    if (!task_manager_window_callbacks) return;

    task_manager_window_callbacks->draw_frame = task_manager_draw_frame_wrapper;
    task_manager_window_callbacks->on_move    = task_manager_on_move;
    task_manager_window_callbacks->set_active = task_manager_set_active;
    task_manager_window_callbacks->context    = nullptr;

    int window_w = 55 * (SCREEN_WIDTH  / 100);
    int window_h = 60 * (SCREEN_HEIGHT / 100);

    XPWindow* window = create_xp_window(120, 80, window_w, window_h,
                                        "Task Manager", task_manager_window_callbacks);
    if (!window) {
        free(task_manager_window_callbacks);
        return;
    }

    TaskManagerState* state = create_task_manager(window);
    if (!state) {
        free(task_manager_window_callbacks);
        return;
    }

    window->context     = (void*)state;
    window->window_type = WINDOW_TYPE_TASK_MANAGER;

    set_active_xp_window(window);
}