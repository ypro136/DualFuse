#include <ramdisk.h>
#include <liballoc.h>

#include <string.h>
#include <stdint.h>
#include <stdio.h>


fs_t fs; // the filesystem table

// RAM disk pointer
static uint8_t* ram_disk;

void block_init() {
    // Allocate 1024 blocks of 512 bytes each = 512 KB
    ram_disk = (uint8_t*)malloc(1024 * BLOCK_SIZE);
}

int block_read(uint32_t block, uint8_t* buffer) {
    // Copy one block from RAM disk to buffer
    for (int i = 0; i < BLOCK_SIZE; i++)
        buffer[i] = ram_disk[block * BLOCK_SIZE + i];
    return 0;
}

int block_write(uint32_t block, uint8_t* buffer) {
    // Copy one block from buffer to RAM disk
    for (int i = 0; i < BLOCK_SIZE; i++)
        ram_disk[block * BLOCK_SIZE + i] = buffer[i];
    return 0;
}

/**
 * Create a new empty file
 */
int fs_create(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs.files[i].used) {
            fs.files[i].used = 1;
            fs.files[i].size = 0;
            fs.files[i].block_index = i; // assign a block (1:1 mapping)
            strcpy(fs.files[i].name, name);
            return 0;
        }
    }
    return -1; // no free file slot
}

/**
 * Write data to an existing file
 */
int fs_write(const char *name, const char *data) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].used && strcmp(fs.files[i].name, name) == 0) 
        {
            uint32_t len = strlen(data);
            if (len > 512) len = 512; // truncate if too big
            fs.files[i].size = len;
            block_write(fs.files[i].block_index, (uint8_t*)data);
            return 0;
        }
    }
    return -1; // file not found
}

/**
 * Read data from a file
 */
int fs_read(const char *name, char *buffer) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].used && strcmp(fs.files[i].name, name) == 0) {
            block_read(fs.files[i].block_index, (uint8_t*)buffer);
            buffer[fs.files[i].size] = '\0'; // null-terminate
            return 0;
        }
    }
    return -1; // file not found
}

/**
 * Delete a file
 */
int fs_delete(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].used && strcmp(fs.files[i].name, name) == 0) {
            fs.files[i].used = 0;
            fs.files[i].size = 0;
            // optionally clear block content
            uint8_t empty[512] = {0};
            block_write(fs.files[i].block_index, empty);
            return 0;
        }
    }
    return -1; // file not found
}

void fs_list(void)
{
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].used) {
            printf(fs.files[i].name);
            printf("\n");
        }
    }
}

