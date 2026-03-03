#include <ramdisk.h>
#include <shell.h>
#include <liballoc.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

fs_t fs;
int  current_dir_index = -1;  // -1 = root

static uint8_t* ram_disk;


void block_init()
{
    ram_disk = (uint8_t*)malloc(1024 * BLOCK_SIZE);
}

int block_read(uint32_t block, uint8_t* buffer)
{
    for (int i = 0; i < BLOCK_SIZE; i++)
        buffer[i] = ram_disk[block * BLOCK_SIZE + i];
    return 0;
}

int block_write(uint32_t block, uint8_t* buffer)
{
    for (int i = 0; i < BLOCK_SIZE; i++)
        ram_disk[block * BLOCK_SIZE + i] = buffer[i];
    return 0;
}


static void build_path(int parent_index, const char* name, char* out)
{
    if (parent_index == -1)
    {
        out[0] = '/';
        strncpy(out + 1, name, MAX_PATH - 2);
        out[MAX_PATH - 1] = '\0';
        return;
    }
    char parent_path[MAX_PATH];
    strncpy(parent_path, fs.files[parent_index].path, MAX_PATH);
    int plen = strlen(parent_path);
    if (parent_path[plen - 1] != '/')
    {
        parent_path[plen]     = '/';
        parent_path[plen + 1] = '\0';
    }
    strncpy(out, parent_path, MAX_PATH);
    strncat(out, name, MAX_PATH - strlen(out) - 1);
}

static int find_in_current(const char* name, int is_dir)
{
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (fs.files[i].used &&
            fs.files[i].parent_index == current_dir_index &&
            fs.files[i].is_dir == is_dir &&
            strcmp(fs.files[i].name, name) == 0)
            return i;
    }
    return -1;
}


static int fs_create_entry(const char* name, uint8_t is_dir)
{
    // reject if name already exists in current dir
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (fs.files[i].used &&
            fs.files[i].parent_index == current_dir_index &&
            strcmp(fs.files[i].name, name) == 0)
            return -2;  // already exists
    }

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (!fs.files[i].used)
        {
            fs.files[i].used         = 1;
            fs.files[i].is_dir       = is_dir;
            fs.files[i].size         = 0;
            fs.files[i].block_index  = i;
            fs.files[i].parent_index = current_dir_index;
            strncpy(fs.files[i].name, name, MAX_NAME - 1);
            build_path(current_dir_index, name, fs.files[i].path);
            return 0;
        }
    }
    return -1;
}

int fs_create(const char* name) { return fs_create_entry(name, 0); }
int fs_mkdir(const char* name)  { return fs_create_entry(name, 1); }


int fs_write(const char* name, const char* data)
{
    int i = find_in_current(name, 0);
    if (i < 0) return -1;
    uint32_t len = strlen(data);
    if (len > BLOCK_SIZE) len = BLOCK_SIZE;
    fs.files[i].size = len;
    block_write(fs.files[i].block_index, (uint8_t*)data);
    return 0;
}

int fs_read(const char* name, char* buffer)
{
    int i = find_in_current(name, 0);
    if (i < 0) return -1;
    block_read(fs.files[i].block_index, (uint8_t*)buffer);
    buffer[fs.files[i].size] = '\0';
    return 0;
}

int fs_delete(const char* name)
{
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (!fs.files[i].used)                          continue;
        if (fs.files[i].parent_index != current_dir_index) continue;
        if (strcmp(fs.files[i].name, name) != 0)        continue;

        // if directory
        if (fs.files[i].is_dir)
        {
            for (int j = 0; j < MAX_FILES; j++)
            {
                if (fs.files[j].used && fs.files[j].parent_index == i)
                    return -2;
            }
            fs.files[i].used = 0;
            return 0;
        }

        fs.files[i].used = 0;
        fs.files[i].size = 0;
        uint8_t empty[BLOCK_SIZE] = {0};
        block_write(fs.files[i].block_index, empty);
        return 0;
    }
    return -1;
}


void fs_list(void)
{
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (fs.files[i].used && fs.files[i].parent_index == current_dir_index)
        {
            if (fs.files[i].is_dir)
                printf("[DIR] %s\n", fs.files[i].name);
            else
                printf("      %s\n", fs.files[i].name);
        }
    }
}


int fs_cd(const char* name)
{
    if (strcmp(name, "..") == 0)
    {
        if (current_dir_index == -1) return 0;  // already root
        current_dir_index = fs.files[current_dir_index].parent_index;
        return 0;
    }

    int i = find_in_current(name, 1);
    if (i < 0) return -1;
    current_dir_index = i;
    return 0;
}

void fs_pwd(char* out_buf)
{
    if (current_dir_index == -1)
    {
        out_buf[0] = '/';
        out_buf[1] = '\0';
        return;
    }
    strncpy(out_buf, fs.files[current_dir_index].path, MAX_PATH);
}

void fs_search(const char* term)
{
    bool found = false;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (!fs.files[i].used) continue;
        if (strstr(fs.files[i].name, term))
        {
            printf("%s %s\n", fs.files[i].is_dir ? "[DIR]" : "     ",
                   fs.files[i].path);
            found = true;
        }
    }
    if (!found)
        printf("no results for \"%s\"\n", term);
}


void fs_start(const char* name, void* shell_ctx)
{
    int i = find_in_current(name, 0);
    if (i < 0) { printf("file not found: %s\n", name); return; }

    char buf[BLOCK_SIZE + 1];
    block_read(fs.files[i].block_index, (uint8_t*)buf);
    buf[fs.files[i].size] = '\0';

    // Check for .sh extension
    int nlen = strlen(name);
    bool is_script = (nlen > 3 && strcmp(name + nlen - 3, ".sh") == 0);

    if (is_script)
    {
        Shell* shell = static_cast<Shell*>(shell_ctx);
        char* line = buf;
        for (char* p = buf; ; p++)
        {
            if (*p == '\n' || *p == '\0')
            {
                char end = *p;
                *p = '\0';
                if (*line != '\0')
                    shell->execute(line);
                if (end == '\0') break;
                line = p + 1;
            }
        }
    }
    else
    {
        printf("%s\n", buf);
    }
}