#ifndef FS_H
#define FS_H

#include <stdint.h>

#include <stdint.h>

#define BLOCK_SIZE  512
#define MAX_FILES   128
#define MAX_NAME    32
#define MAX_PATH    256

typedef struct {
    char     name[MAX_NAME];
    char     path[MAX_PATH];
    uint8_t  used;
    uint8_t  is_dir;
    uint32_t size;
    uint32_t block_index;
    int      parent_index;  // -1 = root
} file_entry_t;

typedef struct {
    file_entry_t files[MAX_FILES];
} fs_t;

extern fs_t fs;
extern int  current_dir_index;

void block_init();
int  block_read(uint32_t block, uint8_t* buffer);
int  block_write(uint32_t block, uint8_t* buffer);

int  fs_create(const char* name);
int  fs_mkdir(const char* name);
int  fs_write(const char* name, const char* data);
int  fs_read(const char* name, char* buffer);
int  fs_delete(const char* name);
void fs_list(void);
int  fs_cd(const char* name);
void fs_pwd(char* out_buf);
void fs_search(const char* term);
void fs_start(const char* name, void* shell_ctx);


#endif