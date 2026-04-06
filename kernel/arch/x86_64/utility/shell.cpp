#include <shell.h>
#include <console.h>
#include <task.h>
#include <liballoc.h>
#include <framebufferutil.h>
#include <timer.h>
#include <bootloader.h>
#include <utility.h>
#include <ramdisk.h>
#include <apic.h>
#include <minimal_acpi.h>
#include <i2c.h>
#include <hid_i2c.h>
#include <serial.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <task.h>
#include <fs.h>
#include <png_loader.h>

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#define FAT32_MAX_FILE_SIZE_BYTES 0xFFFFFFFFUL

static char current_working_directory[MAX_PATH] = "/";

Shell::Shell(Console* output_console) : console(output_console) {}

void Shell::print(const char* string_to_print)
{
    if (!string_to_print || !console) return;
    for (const char* character_pointer = string_to_print; *character_pointer; ++character_pointer) {
        console->print_char(*character_pointer);
        serial_write(*character_pointer);
    }
}

void Shell::println(const char* string_to_print)
{
    print(string_to_print);
    console->print_char('\n');
    serial_write('\n');
}

int atoi(const char* numeric_string)
{
    int result = 0;
    int sign   = 1;
    if (*numeric_string == '-') { sign = -1; numeric_string++; }
    while (*numeric_string >= '0' && *numeric_string <= '9') {
        result = result * 10 + (*numeric_string - '0');
        numeric_string++;
    }
    return sign * result;
}

int Shell::tokenize(char* input_command_line, char* argument_vector[], int maximum_argument_count)
{
    int argument_count = 0;
    char* scan_pointer = input_command_line;
    while (*scan_pointer && argument_count < maximum_argument_count) {
        while (*scan_pointer == ' ') scan_pointer++;
        if (*scan_pointer == '\0') break;
        argument_vector[argument_count++] = scan_pointer;
        while (*scan_pointer != ' ' && *scan_pointer != '\0') scan_pointer++;
        if (*scan_pointer == ' ') *scan_pointer++ = '\0';
    }
    return argument_count;
}

void Shell::str_toupper(char* mutable_string)
{
    for (; *mutable_string; ++mutable_string)
        if (*mutable_string >= 'a' && *mutable_string <= 'z')
            *mutable_string -= 32;
}

void Shell::execute(char* input_command_line)
{
    if (!input_command_line || !console) return;
    char* argument_vector[100];
    int   argument_count = tokenize(input_command_line, argument_vector, 100);
    if (argument_count == 0) return;
    str_toupper(argument_vector[0]);

    if      (strcmp(argument_vector[0], "HELP")    == 0) cmd_help();
    else if (strcmp(argument_vector[0], "CLEAR")   == 0) cmd_clear();
    else if (strcmp(argument_vector[0], "ECHO")    == 0) cmd_echo(argument_count, argument_vector);
    else if (strcmp(argument_vector[0], "INFO")    == 0) cmd_info();
    else if (strcmp(argument_vector[0], "PAGE")    == 0) cmd_page();
    else if (strcmp(argument_vector[0], "MK")      == 0) cmd_mk(argument_count, argument_vector);
    else if (strcmp(argument_vector[0], "CAT")     == 0) cmd_cat(argument_count, argument_vector);
    else if (strcmp(argument_vector[0], "LS")      == 0) cmd_ls();
    else if (strcmp(argument_vector[0], "RM")      == 0) cmd_rm(argument_count, argument_vector);
    else if (strcmp(argument_vector[0], "END")     == 0) cmd_end();
    else if (strcmp(argument_vector[0], "MKDIR")   == 0) cmd_mkdir(argument_count, argument_vector);
    else if (strcmp(argument_vector[0], "CD")      == 0) cmd_cd(argument_count, argument_vector);
    else if (strcmp(argument_vector[0], "PWD")     == 0) cmd_pwd();
    else if (strcmp(argument_vector[0], "SEARCH")  == 0) cmd_search(argument_count, argument_vector);
    else if (strcmp(argument_vector[0], "START")   == 0) cmd_start(argument_count, argument_vector);
    else if (strcmp(argument_vector[0], "CALC")    == 0) cmd_calc(argument_count, argument_vector);
    else if (strcmp(argument_vector[0], "MADT")    == 0) cmd_madt();
    else if (strcmp(argument_vector[0], "I2C")     == 0) cmd_i2c();
    else if (strcmp(argument_vector[0], "I2CHID")  == 0) cmd_i2chid();
    else if (strcmp(argument_vector[0], "I2CPOLL") == 0) cmd_i2cpoll();
    else if (strcmp(argument_vector[0], "TASKS")   == 0) cmd_tasks();
    else if (strcmp(argument_vector[0], "SPAWN")   == 0) cmd_spawn();
    else if (strcmp(argument_vector[0], "KILL")    == 0) cmd_kill(argument_count, argument_vector);
    else if (strcmp(argument_vector[0], "BG")      == 0) cmd_bg();
    else if (strcmp(argument_vector[0], "MOUNT")   == 0) cmd_mount();
    else { print("Unknown command: "); println(argument_vector[0]); }
}

static void build_absolute_path(const char* user_supplied_path,
                                 char* output_absolute_path,
                                 int   output_buffer_size)
{
    if (!user_supplied_path || user_supplied_path[0] == '/') {
        strncpy(output_absolute_path,
                user_supplied_path ? user_supplied_path : "/",
                output_buffer_size - 1);
        output_absolute_path[output_buffer_size - 1] = '\0';
        return;
    }
    int current_directory_length = strlen(current_working_directory);
    if (strcmp(current_working_directory, "/") == 0) {
        output_absolute_path[0] = '/';
        strncpy(output_absolute_path + 1, user_supplied_path, output_buffer_size - 2);
    } else {
        strncpy(output_absolute_path, current_working_directory, output_buffer_size - 1);
        int written_length = strlen(output_absolute_path);
        if (written_length < output_buffer_size - 2) {
            output_absolute_path[written_length] = '/';
            strncpy(output_absolute_path + written_length + 1,
                    user_supplied_path,
                    output_buffer_size - written_length - 2);
        }
    }
    output_absolute_path[output_buffer_size - 1] = '\0';
}

static void navigate_path_to_parent(char* mutable_directory_path)
{
    if (strcmp(mutable_directory_path, "/") == 0) return;
    int path_length = strlen(mutable_directory_path);
    int last_slash_index = -1;
    for (int character_index = path_length - 1; character_index >= 0; character_index--) {
        if (mutable_directory_path[character_index] == '/') {
            last_slash_index = character_index;
            break;
        }
    }
    if (last_slash_index <= 0) {
        mutable_directory_path[0] = '/';
        mutable_directory_path[1] = '\0';
    } else {
        mutable_directory_path[last_slash_index] = '\0';
    }
}

static void print_right_aligned_size(Shell* shell_instance,
                                      uint32_t file_size_bytes,
                                      int column_width)
{
    char size_string_buffer[16];
    u64toa(file_size_bytes, size_string_buffer, 10);
    int number_length = strlen(size_string_buffer);
    for (int padding_index = number_length; padding_index < column_width; padding_index++)
        shell_instance->print(" ");
    shell_instance->print(size_string_buffer);
}

void Shell::cmd_help()
{
    println("Commands:");
    println("  CLEAR ECHO INFO PAGE END HELP");
    println("  MK <file> <content>  CAT <file>  RM <file>  MKDIR <dir>");
    println("  LS  CD <dir>  PWD  SEARCH <term>  START <file>");
    println("  CALC <n> <op> <n> [<op> <n> ...]  ops: + - * / %");
    println("  MADT  I2C  I2CHID  I2CPOLL");
    println("  TASKS  SPAWN  KILL <id>");
    println("  BG     - load wallpaper module, fallback to static, fallback to gradient");
    println("  MOUNT  - mount the FAT32 disk.img loaded as a Limine module");
}

void Shell::cmd_mount()
{
    char address_string_buffer[32];
    char size_string_buffer[32];

    if (!bootloader.modules || bootloader.modules->module_count == 0) {
        println("no modules loaded");
        return;
    }

    void*         module_base_address = (void*)bootloader.modules->modules[0]->address;
    unsigned long module_size_bytes   =        bootloader.modules->modules[0]->size;

    int mount_error_code = fs_init(module_base_address, module_size_bytes);
    if (mount_error_code == 0) 
    {
        println("mounted fat32");
        cmd_ls();
    }
    else { print("mount failed: "); println(fs_strerr((FRESULT)mount_error_code)); }
}

static const char* format_file_size(uint32_t size)
{
    static char buffer[16];
    
    if (size < 1024) {
        snprintf(buffer, sizeof(buffer), "%u B", size);
    } else if (size < 1024 * 1024) {
        uint32_t kb = size / 1024;
        uint32_t remainder = size % 1024;
        uint32_t tenths = (remainder * 10) / 1024;   // 0‑9
        snprintf(buffer, sizeof(buffer), "%u.%u KB", kb, tenths);
    } else if (size < 1024 * 1024 * 1024) {
        uint32_t mb = size / (1024 * 1024);
        uint32_t remainder = size % (1024 * 1024);
        uint32_t tenths = (remainder * 10) / (1024 * 1024);
        snprintf(buffer, sizeof(buffer), "%u.%u MB", mb, tenths);
    } else {
        uint32_t gb = size / (1024 * 1024 * 1024);
        uint32_t remainder = size % (1024 * 1024 * 1024);
        uint32_t tenths = (remainder * 10) / (1024 * 1024 * 1024);
        snprintf(buffer, sizeof(buffer), "%u.%u GB", gb, tenths);
    }
    return buffer;
}

static void print_right_aligned_str(Shell* shell, const char* str, int width)
{
    int len = strlen(str);
    for (int i = len; i < width; i++)
        shell->print(" ");
    shell->print(str);
}

void Shell::cmd_ls()
{
    DIR     directory_handle;
    FILINFO directory_entry_info;
    FRESULT open_result = f_opendir(&directory_handle, current_working_directory);

    if (open_result != FR_OK) {
        print("ls failed: ");
        println(fs_strerr(open_result));
        return;
    }

    while (f_readdir(&directory_handle, &directory_entry_info) == FR_OK
           && directory_entry_info.fname[0] != '\0')
    {
        bool entry_is_directory = (directory_entry_info.fattrib & AM_DIR) != 0;
        if (entry_is_directory) {
            print("  <DIR>           ");
        } else {
            const char* size_str = format_file_size(directory_entry_info.fsize);
            print("  ");
            print_right_aligned_str(this, size_str, 10);  // width 10 characters
            print("  ");
        }
        println(directory_entry_info.fname);
    }

    f_closedir(&directory_handle);
}

void Shell::cmd_mk(int argument_count, char* argument_vector[])
{
    if (argument_count < 3) { println("Usage: MK <filename> <content>"); return; }

    char target_absolute_path[MAX_PATH];
    build_absolute_path(argument_vector[1], target_absolute_path, MAX_PATH);

    char concatenated_content_buffer[512];
    concatenated_content_buffer[0] = '\0';
    for (int argument_index = 2; argument_index < argument_count; argument_index++) {
        strcat(concatenated_content_buffer, argument_vector[argument_index]);
        if (argument_index != argument_count - 1) strcat(concatenated_content_buffer, " ");
    }

    FIL   file_handle;
    UINT  bytes_written_count;
    FRESULT create_result = f_open(&file_handle, target_absolute_path,
                                    FA_CREATE_ALWAYS | FA_WRITE);
    if (create_result != FR_OK) {
        print("MK failed: ");
        println(fs_strerr(create_result));
        return;
    }

    FRESULT write_result = f_write(&file_handle,
                                    concatenated_content_buffer,
                                    strlen(concatenated_content_buffer),
                                    &bytes_written_count);
    f_close(&file_handle);

    if (write_result != FR_OK) { print("write failed: "); println(fs_strerr(write_result)); }
    else println("file created.");
}

void Shell::cmd_cat(int argument_count, char* argument_vector[])
{
    if (argument_count < 2) { println("Usage: CAT <filename>"); return; }

    char target_absolute_path[MAX_PATH];
    build_absolute_path(argument_vector[1], target_absolute_path, MAX_PATH);

    FIL     file_handle;
    FRESULT open_result = f_open(&file_handle, target_absolute_path, FA_READ);
    if (open_result != FR_OK) {
        print("CAT failed: ");
        println(fs_strerr(open_result));
        return;
    }

    uint32_t file_size_bytes = (uint32_t)f_size(&file_handle);
    if (file_size_bytes > FAT32_MAX_FILE_SIZE_BYTES) {
        println("file too large");
        f_close(&file_handle);
        return;
    }

    char* file_content_buffer = (char*)malloc(file_size_bytes + 1);
    if (!file_content_buffer) {
        println("out of memory");
        f_close(&file_handle);
        return;
    }

    UINT    bytes_read_count;
    FRESULT read_result = f_read(&file_handle, file_content_buffer,
                                  file_size_bytes, &bytes_read_count);
    f_close(&file_handle);

    if (read_result != FR_OK) {
        print("read failed: ");
        println(fs_strerr(read_result));
        free(file_content_buffer);
        return;
    }

    file_content_buffer[bytes_read_count] = '\0';
    println(file_content_buffer);
    free(file_content_buffer);
}

void Shell::cmd_rm(int argument_count, char* argument_vector[])
{
    if (argument_count < 2) { println("Usage: RM <name>"); return; }

    char target_absolute_path[MAX_PATH];
    build_absolute_path(argument_vector[1], target_absolute_path, MAX_PATH);

    FRESULT delete_result = f_unlink(target_absolute_path);
    if      (delete_result == FR_OK)     println("deleted.");
    else if (delete_result == FR_DENIED) println("error: directory not empty.");
    else { print("RM failed: "); println(fs_strerr(delete_result)); }
}

void Shell::cmd_mkdir(int argument_count, char* argument_vector[])
{
    if (argument_count < 2) { println("Usage: MKDIR <name>"); return; }

    char target_absolute_path[MAX_PATH];
    build_absolute_path(argument_vector[1], target_absolute_path, MAX_PATH);

    FRESULT create_result = f_mkdir(target_absolute_path);
    if (create_result == FR_OK) println("directory created.");
    else { print("MKDIR failed: "); println(fs_strerr(create_result)); }
}

void Shell::cmd_cd(int argument_count, char* argument_vector[])
{
    if (argument_count < 2) { println("Usage: CD <directory|..>"); return; }

    if (strcmp(argument_vector[1], "..") == 0) {
        navigate_path_to_parent(current_working_directory);
        return;
    }

    if (strcmp(argument_vector[1], "/") == 0) {
        current_working_directory[0] = '/';
        current_working_directory[1] = '\0';
        return;
    }

    char candidate_absolute_path[MAX_PATH];
    build_absolute_path(argument_vector[1], candidate_absolute_path, MAX_PATH);

    FILINFO entry_stat_result;
    FRESULT stat_result = f_stat(candidate_absolute_path, &entry_stat_result);

    if (stat_result != FR_OK) {
        print("CD failed: ");
        println(fs_strerr(stat_result));
        return;
    }
    if (!(entry_stat_result.fattrib & AM_DIR)) {
        println("not a directory.");
        return;
    }

    strncpy(current_working_directory, candidate_absolute_path, MAX_PATH - 1);
    current_working_directory[MAX_PATH - 1] = '\0';
}

void Shell::cmd_pwd()
{
    println(current_working_directory);
}

static void search_directory_tree_recursively(Shell*       shell_instance,
                                               const char*  absolute_search_root_path,
                                               const char*  search_term_string)
{
    DIR     directory_handle;
    FILINFO directory_entry_info;

    if (f_opendir(&directory_handle, absolute_search_root_path) != FR_OK) return;

    while (f_readdir(&directory_handle, &directory_entry_info) == FR_OK
           && directory_entry_info.fname[0] != '\0')
    {
        char entry_absolute_path[MAX_PATH];
        if (strcmp(absolute_search_root_path, "/") == 0) {
            entry_absolute_path[0] = '/';
            strncpy(entry_absolute_path + 1, directory_entry_info.fname, MAX_PATH - 2);
        } else {
            strncpy(entry_absolute_path, absolute_search_root_path, MAX_PATH - 1);
            int root_path_length = strlen(entry_absolute_path);
            if (root_path_length < MAX_PATH - 2) {
                entry_absolute_path[root_path_length] = '/';
                strncpy(entry_absolute_path + root_path_length + 1,
                        directory_entry_info.fname,
                        MAX_PATH - root_path_length - 2);
            }
        }
        entry_absolute_path[MAX_PATH - 1] = '\0';

        if (strstr(directory_entry_info.fname, search_term_string))
            shell_instance->println(entry_absolute_path);

        if (directory_entry_info.fattrib & AM_DIR)
            search_directory_tree_recursively(shell_instance,
                                               entry_absolute_path,
                                               search_term_string);
    }

    f_closedir(&directory_handle);
}
 
void Shell::cmd_search(int argument_count, char* argument_vector[])
{
    if (argument_count < 2) { println("Usage: SEARCH <term>"); return; }
    search_directory_tree_recursively(this, "/", argument_vector[1]);
}

void Shell::cmd_start(int argument_count, char* argument_vector[])
{
    if (argument_count < 2) { println("Usage: START <file>"); return; }

    char target_absolute_path[MAX_PATH];
    build_absolute_path(argument_vector[1], target_absolute_path, MAX_PATH);

    FIL     file_handle;
    FRESULT open_result = f_open(&file_handle, target_absolute_path, FA_READ);
    if (open_result != FR_OK) {
        print("START failed: ");
        println(fs_strerr(open_result));
        return;
    }

    uint32_t file_size_bytes = (uint32_t)f_size(&file_handle);
    if (file_size_bytes > FAT32_MAX_FILE_SIZE_BYTES) {
        println("file too large");
        f_close(&file_handle);
        return;
    }

    char* file_content_buffer = (char*)malloc(file_size_bytes + 1);
    if (!file_content_buffer) {
        println("out of memory");
        f_close(&file_handle);
        return;
    }

    UINT    bytes_read_count;
    FRESULT read_result = f_read(&file_handle, file_content_buffer,
                                  file_size_bytes, &bytes_read_count);
    f_close(&file_handle);

    if (read_result != FR_OK) {
        print("read failed: ");
        println(fs_strerr(read_result));
        free(file_content_buffer);
        return;
    }
    file_content_buffer[bytes_read_count] = '\0';

    const char* file_name_only = argument_vector[1];
    int         file_name_length = strlen(file_name_only);
    bool        file_is_shell_script = (file_name_length > 3
                                        && strcmp(file_name_only + file_name_length - 3, ".sh") == 0);

    if (file_is_shell_script) {
        char* current_line_start = file_content_buffer;
        for (char* scan_pointer = file_content_buffer; ; scan_pointer++) {
            if (*scan_pointer == '\n' || *scan_pointer == '\0') {
                char saved_character = *scan_pointer;
                *scan_pointer = '\0';
                if (*current_line_start != '\0')
                    execute(current_line_start);
                if (saved_character == '\0') break;
                current_line_start = scan_pointer + 1;
            }
        }
    } else {
        println(file_content_buffer);
    }

    free(file_content_buffer);
}

void Shell::cmd_calc(int argument_count, char* argument_vector[])
{
    if (argument_count < 4 || (argument_count & 1) != 0) {
        println("Usage: CALC <num> <op> <num> [<op> <num> ...]");
        println("  ops: + - * / %");
        return;
    }

    int accumulated_result = atoi(argument_vector[1]);

    for (int operand_index = 2; operand_index + 1 < argument_count; operand_index += 2) {
        char operator_character = argument_vector[operand_index][0];
        int  right_hand_operand = atoi(argument_vector[operand_index + 1]);

        switch (operator_character) {
            case '+': accumulated_result += right_hand_operand; break;
            case '-': accumulated_result -= right_hand_operand; break;
            case '*': accumulated_result *= right_hand_operand; break;
            case '/':
                if (right_hand_operand == 0) { println("error: division by zero"); return; }
                accumulated_result /= right_hand_operand; break;
            case '%':
                if (right_hand_operand == 0) { println("error: modulo by zero"); return; }
                accumulated_result %= right_hand_operand; break;
            default:
                print("error: unknown operator '");
                console->print_char(operator_character);
                println("'");
                return;
        }
    }

    char result_string_buffer[32];
    if (accumulated_result < 0) {
        print("-");
        u64toa((uint64_t)(-(int64_t)accumulated_result), result_string_buffer, 10);
    } else {
        u64toa((uint64_t)accumulated_result, result_string_buffer, 10);
    }
    println(result_string_buffer);
}

void Shell::cmd_bg()
{
    FIL     wallpaper_file_handle;
    FRESULT open_result = f_open(&wallpaper_file_handle, "/assets/wallpaper.png", FA_READ);
    if (open_result != FR_OK) {
        println("no wallpaper found at /assets/wallpaper.png, using gradient fallback");
        return;
    }

    uint32_t wallpaper_file_size_bytes = (uint32_t)f_size(&wallpaper_file_handle);
    uint8_t* wallpaper_pixel_data_buffer = (uint8_t*)malloc(wallpaper_file_size_bytes);
    if (!wallpaper_pixel_data_buffer) {
        println("out of memory for wallpaper, using gradient fallback");
        f_close(&wallpaper_file_handle);
        return;
    }

    UINT    bytes_read_count;
    FRESULT read_result = f_read(&wallpaper_file_handle,
                                  wallpaper_pixel_data_buffer,
                                  wallpaper_file_size_bytes,
                                  &bytes_read_count);
    f_close(&wallpaper_file_handle);

    if (read_result != FR_OK) {
        print("wallpaper read failed: ");
        println(fs_strerr(read_result));
        free(wallpaper_pixel_data_buffer);
        return;
    }

    if (png_load_wallpaper(wallpaper_pixel_data_buffer, bytes_read_count)) {
        println("wallpaper loaded from /assets/wallpaper.png");
    } else {
        println("wallpaper decode failed, using gradient fallback");
    }

    free(wallpaper_pixel_data_buffer);
}

static const char* task_state_to_string(uint8_t task_state_enum_value)
{
    switch (task_state_enum_value) {
        case 0:  return "DEAD";
        case 1:  return "READY";
        case 2:  return "IDLE";
        case 3:  return "WAITING_INPUT";
        case 4:  return "CREATED";
        case 5:  return "WAITING_CHILD";
        case 6:  return "WAITING_CHILD_SPECIFIC";
        case 7:  return "WAITING_VFORK";
        case 8:  return "BLOCKED";
        case 9:  return "SIGKILLED";
        case 10: return "FUTEX";
        case 69: return "DUMMY";
        default: return "UNKNOWN";
    }
}

void Shell::cmd_tasks()
{
    char id_string_buffer[32];
    println("ID   STATE                  TYPE");
    println("---- ---------------------- -------");

    spinlock_cnt_read_acquire(&TASK_LL_MODIFY);
    Task* current_task_node = firstTask;
    while (current_task_node) {
        u64toa(current_task_node->id, id_string_buffer, 10);
        print(id_string_buffer);
        for (int padding_index = strlen(id_string_buffer); padding_index < 5; padding_index++)
            print(" ");

        const char* state_name_string = task_state_to_string(current_task_node->state);
        print(state_name_string);
        for (int padding_index = strlen(state_name_string); padding_index < 23; padding_index++)
            print(" ");

        println(current_task_node->kernel_task ? "kernel" : "user");
        current_task_node = current_task_node->next;
    }
    spinlock_cnt_read_release(&TASK_LL_MODIFY);
}

static void infinite_pause_loop_kernel_task_entry(uint64_t unused_parameter)
{
    while (true) asm volatile("pause");
}

void Shell::cmd_spawn()
{
    char spawned_task_id_buffer[32];
    Task* new_kernel_task = task_create_kernel(
        (uint64_t)infinite_pause_loop_kernel_task_entry, 0);
    if (!new_kernel_task) { println("task_create_kernel failed"); return; }
    task_name_kernel(new_kernel_task, "shell-test", 10);
    print("spawned task id=");
    u64toa(new_kernel_task->id, spawned_task_id_buffer, 10);
    println(spawned_task_id_buffer);
}

void Shell::cmd_kill(int argument_count, char* argument_vector[])
{
    if (argument_count < 2) { println("Usage: KILL <id>"); return; }
    char id_string_buffer[32];
    uint32_t target_task_id = (uint32_t)atoi(argument_vector[1]);
    if (target_task_id == KERNEL_TASK_ID) { println("cannot kill kernel task 0."); return; }
    Task* target_task_node = task_get(target_task_id);
    if (!target_task_node) {
        print("no task with id=");
        u64toa(target_task_id, id_string_buffer, 10);
        println(id_string_buffer);
        return;
    }
    task_kill(target_task_id, 0);
    print("killed task id=");
    u64toa(target_task_id, id_string_buffer, 10);
    println(id_string_buffer);
}

void Shell::cmd_i2c()
{
    char register_value_string_buffer[64];
    println("I2C controller (MSI Modern 14 Alder Lake):");
    uint64_t i2c_physical_base_address = 0x4017000000ULL;
    uint64_t i2c_virtual_base_address  = bootloader.hhdmOffset + i2c_physical_base_address;
    uint32_t comp_type_register   = i2cRead(i2c_virtual_base_address, DW_IC_COMP_TYPE);
    uint32_t control_register     = i2cRead(i2c_virtual_base_address, DW_IC_CON);
    uint32_t target_address_register = i2cRead(i2c_virtual_base_address, DW_IC_TAR);
    uint32_t status_register      = i2cRead(i2c_virtual_base_address, DW_IC_STATUS);
    uint32_t enable_register      = i2cRead(i2c_virtual_base_address, DW_IC_ENABLE);
    print("  phys base  : 0x"); u64toa(i2c_physical_base_address, register_value_string_buffer, 16); println(register_value_string_buffer);
    print("  controller : ");
    println(comp_type_register == DW_IC_COMP_TYPE_VALUE ? "DesignWare (OK)" : "NOT FOUND / wrong base");
    print("  IC_COMP_TYPE: 0x"); u64toa(comp_type_register,         register_value_string_buffer, 16); println(register_value_string_buffer);
    print("  IC_ENABLE   : 0x"); u64toa(enable_register,            register_value_string_buffer, 16); println(register_value_string_buffer);
    print("  IC_CON      : 0x"); u64toa(control_register,           register_value_string_buffer, 16); println(register_value_string_buffer);
    print("  IC_TAR      : 0x"); u64toa(target_address_register,    register_value_string_buffer, 16); println(register_value_string_buffer);
    print("  IC_STATUS   : 0x"); u64toa(status_register,            register_value_string_buffer, 16); println(register_value_string_buffer);
    print("  touchpad    : ELAN 04F3:3282 at I2C addr 0x");
    u64toa(I2C_ADDR_ELAN_TOUCHPAD, register_value_string_buffer, 16); println(register_value_string_buffer);
    println("  GSI         : 27 (IR-IO-APIC 27-fasteoi, confirmed from Linux)");
}

void Shell::cmd_madt()
{
    AcpiMadt* madt_table_pointer = acpiGetMadt();
    if (!madt_table_pointer) { println("MADT not found."); return; }
    println("MADT entries:");
    uint8_t* entry_scan_pointer = (uint8_t*)madt_table_pointer + sizeof(AcpiMadt);
    uint8_t* table_end_pointer  = (uint8_t*)madt_table_pointer + madt_table_pointer->hdr.length;
    char value_string_buffer[128];
    while (entry_scan_pointer < table_end_pointer) {
        AcpiEntryHdr* entry_header = (AcpiEntryHdr*)entry_scan_pointer;
        switch (entry_header->type) {
            case 0: {
                AcpiMadtLapic* lapic_entry = (AcpiMadtLapic*)entry_header;
                print("  LAPIC: uid=");   u64toa(lapic_entry->processor_uid, value_string_buffer, 10); print(value_string_buffer);
                print(" id=");            u64toa(lapic_entry->lapic_id,      value_string_buffer, 10); print(value_string_buffer);
                print(" flags=0x");       u64toa(lapic_entry->flags,         value_string_buffer, 16); println(value_string_buffer);
            } break;
            case 1: {
                AcpiMadtIoapic* ioapic_entry = (AcpiMadtIoapic*)entry_header;
                print("  IOAPIC: id=");  u64toa(ioapic_entry->id,       value_string_buffer, 10); print(value_string_buffer);
                print(" addr=0x");       u64toa(ioapic_entry->address,  value_string_buffer, 16); print(value_string_buffer);
                print(" gsi=");          u64toa(ioapic_entry->gsi_base, value_string_buffer, 10); println(value_string_buffer);
            } break;
            case 2: {
                AcpiMadtInterruptSourceOverride* iso_entry = (AcpiMadtInterruptSourceOverride*)entry_header;
                print("  ISO: bus=");  u64toa(iso_entry->bus,    value_string_buffer, 10); print(value_string_buffer);
                print(" src=");        u64toa(iso_entry->source, value_string_buffer, 10); print(value_string_buffer);
                print(" gsi=");        u64toa(iso_entry->gsi,    value_string_buffer, 10); print(value_string_buffer);
                print(" flags=0x");    u64toa(iso_entry->flags,  value_string_buffer, 16); println(value_string_buffer);
            } break;
            default:
                print("  type="); u64toa(entry_header->type, value_string_buffer, 10); println(value_string_buffer);
                break;
        }
        entry_scan_pointer += entry_header->length;
    }
}

void Shell::cmd_clear()  { console->clear_screen(); }
void Shell::cmd_end()    { println("Stopping CPU..."); Halt(); }

void Shell::cmd_echo(int argument_count, char* argument_vector[])
{
    for (int argument_index = 1; argument_index < argument_count; argument_index++) {
        print(argument_vector[argument_index]);
        if (argument_index != argument_count - 1) print(" ");
    }
    console->print_char('\n');
}

void Shell::cmd_info()
{
    char value_string_buffer[128];
    println("DualFuse OS - 64-bit");
    print("  width:   "); u64toa(tempframebuffer ? tempframebuffer->width  : 0, value_string_buffer, 10); println(value_string_buffer);
    print("  height:  "); u64toa(tempframebuffer ? tempframebuffer->height : 0, value_string_buffer, 10); println(value_string_buffer);
    print("  FPS:     "); u64toa(frequency,                                     value_string_buffer, 10); println(value_string_buffer);
    print("  lapicID: "); u64toa(apicGetBspLapicId(),                           value_string_buffer, 10); println(value_string_buffer);
}

void Shell::cmd_page()
{
    void* allocated_test_page = malloc(1000);
    char address_string_buffer[32];
    print("allocated at: 0x");
    u64toa((uint64_t)(uintptr_t)allocated_test_page, address_string_buffer, 16);
    println(address_string_buffer);
}

void Shell::cmd_i2chid()
{
    char value_string_buffer[64];
    println("=== I2CHID: HID init ===");
    uint64_t i2c_virtual_base_address = i2cGetBase();
    if (i2c_virtual_base_address == 0) { println("no controller at boot\n"); return; }
    print("  base=0x"); u64toa(i2c_virtual_base_address, value_string_buffer, 16); println(value_string_buffer);
    if (hidI2cIsActive()) {
        println("  HID already active from boot — touchpad running.");
        print("  vendor=0x");   u64toa(hidI2cGetDesc()->wVendorID,     value_string_buffer, 16); println(value_string_buffer);
        print("  product=0x");  u64toa(hidI2cGetDesc()->wProductID,    value_string_buffer, 16); println(value_string_buffer);
        print("  inputReg=0x"); u64toa(hidI2cGetDesc()->wInputRegister,value_string_buffer, 16); println(value_string_buffer);
        return;
    }
    HidI2cDescriptor hid_descriptor;
    if (hidI2cInit(i2c_virtual_base_address, I2C_ADDR_ELAN_TOUCHPAD, &hid_descriptor) < 0) {
        println("hidI2cInit failed"); return;
    }
    println("HID ready. Raw report dump (5 reads)...");
    uint8_t  input_register_address_bytes[2] = {
        (uint8_t)(hid_descriptor.wInputRegister & 0xFF),
        (uint8_t)(hid_descriptor.wInputRegister >> 8)
    };
    uint16_t maximum_input_report_length = hid_descriptor.wMaxInputLength;
    if (maximum_input_report_length < 4 || maximum_input_report_length > 64)
        maximum_input_report_length = 32;
    for (int report_read_index = 0; report_read_index < 5; report_read_index++) {
        sleep(120);
        uint8_t report_receive_buffer[64] = {0};
        int bytes_received = i2cWriteRead(i2c_virtual_base_address,
                                           input_register_address_bytes, 2,
                                           report_receive_buffer, maximum_input_report_length);
        if (bytes_received < 0) { println("  read error"); continue; }
        uint16_t reported_packet_length = (uint16_t)report_receive_buffer[0]
                                        | ((uint16_t)report_receive_buffer[1] << 8);
        print("  ["); u64toa(report_read_index, value_string_buffer, 10); print(value_string_buffer);
        print("] id=0x"); u64toa(report_receive_buffer[2], value_string_buffer, 16); print(value_string_buffer);
        print(" len=");   u64toa(reported_packet_length,   value_string_buffer, 10); print(value_string_buffer);
        print(" : ");
        for (int byte_index = 0; byte_index < bytes_received && byte_index < 12; byte_index++) {
            if (report_receive_buffer[byte_index] < 0x10) print("0");
            u64toa(report_receive_buffer[byte_index], value_string_buffer, 16);
            print(value_string_buffer); print(" ");
        }
        println("");
    }
    println("done.");
}

void Shell::cmd_i2cpoll()
{
    char value_string_buffer[64];
    println("=== I2CPOLL: 30 reports ===");
    uint64_t i2c_virtual_base_address = i2cGetBase();
    if (i2c_virtual_base_address == 0) { println("no controller"); return; }
    if (!hidI2cIsActive()) {
        HidI2cDescriptor hid_descriptor;
        if (hidI2cInit(i2c_virtual_base_address, I2C_ADDR_ELAN_TOUCHPAD, &hid_descriptor) < 0) {
            println("hidI2cInit failed"); return;
        }
    }
    uint8_t input_register_address_bytes[2] = {
        (uint8_t)(hidI2cGetDesc()->wInputRegister & 0xFF),
        (uint8_t)(hidI2cGetDesc()->wInputRegister >> 8)
    };
    uint16_t maximum_input_report_length = hidI2cGetDesc()->wMaxInputLength;
    if (maximum_input_report_length < 4 || maximum_input_report_length > 64)
        maximum_input_report_length = 32;
    for (int poll_iteration_index = 0; poll_iteration_index < 30; poll_iteration_index++) {
        sleep(100);
        uint8_t report_receive_buffer[64] = {0};
        int bytes_received = i2cWriteRead(i2c_virtual_base_address,
                                           input_register_address_bytes, 2,
                                           report_receive_buffer, maximum_input_report_length);
        if (bytes_received < 0) { println("read error"); continue; }
        uint16_t reported_packet_length = (uint16_t)report_receive_buffer[0]
                                        | ((uint16_t)report_receive_buffer[1] << 8);
        uint8_t  report_id_byte = report_receive_buffer[2];
        if (reported_packet_length < 3) { println("  [--] idle"); continue; }
        if (report_id_byte == 0x04) {
            bool finger_tip_contact = (report_receive_buffer[3] & 0x02) != 0;
            uint16_t absolute_x_position = (uint16_t)report_receive_buffer[4]
                                          | ((uint16_t)report_receive_buffer[5] << 8);
            uint16_t absolute_y_position = (uint16_t)report_receive_buffer[6]
                                          | ((uint16_t)report_receive_buffer[7] << 8);
            print(finger_tip_contact ? "  [ABS] x=" : "  [ABS-notip] x=");
            u64toa(absolute_x_position, value_string_buffer, 10); print(value_string_buffer);
            print(" y="); u64toa(absolute_y_position, value_string_buffer, 10); println(value_string_buffer);
        } else if (report_id_byte == 0x01) {
            int8_t relative_x_delta = (int8_t)report_receive_buffer[4];
            int8_t relative_y_delta = (int8_t)report_receive_buffer[5];
            print("  [REL] dx="); u64toa((uint64_t)(int64_t)relative_x_delta, value_string_buffer, 10); print(value_string_buffer);
            print(" dy=");        u64toa((uint64_t)(int64_t)relative_y_delta, value_string_buffer, 10); println(value_string_buffer);
        } else {
            print("  [0x"); u64toa(report_id_byte, value_string_buffer, 16); print(value_string_buffer); println("] unknown");
        }
    }
    println("poll done.");
}