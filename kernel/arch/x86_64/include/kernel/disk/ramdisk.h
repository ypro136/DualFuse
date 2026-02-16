#ifndef FS_H
#define FS_H

#include <stdint.h>

#include <stdint.h>

#define BLOCK_SIZE 512   // size of each block

// Initialize RAM disk
void block_init();

// Read a block into buffer, return 0 on success
int block_read(uint32_t block, uint8_t *buffer);

// Write a block from buffer, return 0 on success
int block_write(uint32_t block, uint8_t *buffer);


#define MAX_FILES 16       // max number of files in RAM disk
#define FILE_NAME_LEN 32   // max file name length

typedef struct {
    uint8_t used;                 // 0 = free, 1 = used
    char name[FILE_NAME_LEN];     // file name
    uint32_t size;                // file size in bytes
    uint32_t block_index;         // which block holds the data
} file_entry_t;

typedef struct {
    file_entry_t files[MAX_FILES];
} fs_t;

extern fs_t fs;

// filesystem API
int fs_create(const char *name);                     // create empty file
int fs_write(const char *name, const char *data);   // write data to file
int fs_read(const char *name, char *buffer);        // read file data
int fs_delete(const char *name); 
void fs_list(void);
                   // delete file

#endif