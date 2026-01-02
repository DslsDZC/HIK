/*
 * HIK BIOS Bootloader - File System Support
 */

#ifndef FS_H
#define FS_H

#include <stdint.h>

/* File system types */
#define FS_TYPE_NONE  0
#define FS_TYPE_FAT32 1

/* Maximum path length */
#define MAX_PATH 256

/* Maximum files open at once */
#define MAX_OPEN_FILES 4

/* File handle */
typedef struct {
    int     in_use;
    uint64_t size;
    uint64_t position;
    uint8_t *data;
} file_handle_t;

/* Initialize file system */
int fs_init(void);

/* Open file */
int fs_open(const char *path);

/* Close file */
void fs_close(int fd);

/* Read from file */
int fs_read(void *buffer, uint64_t size, uint64_t *bytes_read);

/* Seek in file */
int fs_seek(uint64_t offset);

/* Get file size */
uint64_t fs_get_size(void);

/* Get current position */
uint64_t fs_get_position(void);

/* List directory */
int fs_list_dir(const char *path, char *buffer, uint64_t buffer_size);

#endif /* FS_H */