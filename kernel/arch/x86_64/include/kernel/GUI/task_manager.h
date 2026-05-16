#pragma once

#include <cstdint>
#include <window.h>

#define TASK_MANAGER_MAX_SNAPSHOT_TASKS                64
#define TASK_MANAGER_TASK_NAME_BUFFER_LEN              48
#define TASK_MANAGER_ROW_PADDING                        4
#define TASK_MANAGER_MEMORY_BAR_SECTION_H             52
#define TASK_MANAGER_SYSINFO_SECTION_H                80
#define TASK_MANAGER_SCROLLBAR_W                       16
#define TASK_MANAGER_COL_PID_W                         50
#define TASK_MANAGER_COL_STATE_W                      110
#define TASK_MANAGER_COL_CORE_W                       110
#define TASK_MANAGER_COL_TYPE_W                        70
#define TASK_MANAGER_BUTTON_W                         120
#define TASK_MANAGER_BUTTON_H                          24
#define TASK_MANAGER_STATUS_MESSAGE_DISPLAY_FRAMES    180
#define TASK_MANAGER_SNAPSHOT_REFRESH_INTERVAL_FRAMES  60
#define TASK_MANAGER_CPU_BRAND_STRING_BUFFER_LEN       49

struct TaskManagerSnapshotEntry {
    uint64_t task_id;
    uint8_t  task_state;
    bool     is_kernel_task;
    char     task_display_name[TASK_MANAGER_TASK_NAME_BUFFER_LEN];
    int      running_core;      // LAPIC ID of the core running this task, or -1
};

struct TaskManagerState {
    XPWindow* window;

    TaskManagerSnapshotEntry snapshot_entries[TASK_MANAGER_MAX_SNAPSHOT_TASKS];
    int      snapshot_task_count;
    uint64_t snapshot_last_refresh_frame;

    int selected_snapshot_index;
    int scroll_offset_rows;

    char     status_message[80];
    uint64_t status_message_expire_frame;

    XPButton* kill_selected_button;
    XPButton* refresh_button;

    char cpu_brand_string[TASK_MANAGER_CPU_BRAND_STRING_BUFFER_LEN];
    bool cpu_brand_string_loaded;
};

TaskManagerState* create_task_manager(XPWindow* window);
void              destroy_task_manager(TaskManagerState* state);

void task_manager_refresh_snapshot(TaskManagerState* state);
void task_manager_draw_frame(TaskManagerState* state);
void task_manager_handle_mouse(TaskManagerState* state, int mouse_x, int mouse_y,
                               bool left_button_clicked, bool right_button_clicked);
void task_manager_handle_keyboard(TaskManagerState* state, char key_char);

void task_manager_draw_frame_wrapper(void* ctx);
void task_manager_on_move(void* ctx, int x, int y);
void task_manager_set_active(void* ctx);

void on_task_manager_icon_click();