/*
 * HIK BIOS Bootloader - File System Implementation (FAT32)
 */

#include "fs.h"
#include "../hal/hal.h"
#include "../util/util.h"
#include <stddef.h>

/* Forward declarations for static functions */
static int read_disk_sector(uint32_t lba, uint8_t *buffer);
static int read_disk_sectors(uint32_t lba, uint32_t count, uint8_t *buffer);
static uint32_t get_fat_entry(uint32_t cluster);
static char toupper(char c);
static int find_file(const char *path, file_handle_t *file);

/* FAT32 Boot Sector (BPB) */
typedef struct {
    uint8_t  jump[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  ext_signature;
    uint32_t volume_serial;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
    uint8_t  boot_code[420];
    uint16_t signature;
} __attribute__((packed)) fat32_bpb_t;

/* FAT32 Directory Entry */
typedef struct {
    uint8_t  name[11];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat32_dentry_t;

/* File system state */
static int fs_initialized = 0;
static int fs_type = FS_TYPE_NONE;
static fat32_bpb_t *fat32_bpb = NULL;
static uint8_t *fat_table = NULL;
static uint32_t bytes_per_cluster;
static uint32_t data_start;

/* Current file handle */
static file_handle_t current_file;
static int file_open = 0;

/* Disk read buffer */
static uint8_t disk_buffer[512];

/*
 * Initialize file system
 */
int fs_init(void) {
    /* Read FAT32 boot sector */
    if (!read_disk_sector(0, disk_buffer)) {
        return 0;
    }
    
    fat32_bpb = (fat32_bpb_t*)disk_buffer;
    
    /* Verify boot sector signature */
    if (fat32_bpb->signature != 0xAA55) {
        hal_print("ERROR: Invalid boot sector signature\n");
        return 0;
    }
    
    /* Check if FAT32 */
    if (fat32_bpb->fat_size_32 == 0) {
        hal_print("ERROR: Not a FAT32 file system\n");
        return 0;
    }
    
    fs_type = FS_TYPE_FAT32;
    
    /* Calculate cluster size */
    bytes_per_cluster = fat32_bpb->bytes_per_sector * fat32_bpb->sectors_per_cluster;
    
    /* Calculate data start */
    uint32_t fat_start = fat32_bpb->reserved_sectors;
    uint32_t fat_size = fat32_bpb->fat_size_32;
    uint32_t root_dir_sectors = 0;
    data_start = fat_start + (fat32_bpb->fat_count * fat_size) + root_dir_sectors;
    
    /* Load FAT table (first sector for simplicity) */
    if (!read_disk_sector(fat_start, disk_buffer)) {
        return 0;
    }
    
    fat_table = (uint8_t*)0x80000;  /* Load FAT to 0x80000 */
    if (!read_disk_sectors(fat_start, fat32_bpb->fat_size_32, fat_table)) {
        return 0;
    }
    
    fs_initialized = 1;
    
    hal_print("FAT32 file system initialized\n");
    hal_print("  Cluster size: ");
    hal_print_dec(bytes_per_cluster);
    hal_print(" bytes\n");
    
    return 1;
}

/*
 * Open file
 */
int fs_open(const char *path) {
    if (!fs_initialized) {
        return 0;
    }
    
    /* Parse path and find file */
    if (!find_file(path, &current_file)) {
        return 0;
    }
    
    file_open = 1;
    current_file.position = 0;
    
    hal_print("File opened: ");
    hal_print(path);
    hal_print(" (");
    hal_print_dec(current_file.size);
    hal_print(" bytes)\n");
    
    return 1;
}

/*
 * Close file
 */
void fs_close(int fd) {
    (void)fd;  /* Suppress unused parameter warning */
    file_open = 0;
    current_file.in_use = 0;
}

/*
 * Read from file
 */
int fs_read(void *buffer, uint64_t size, uint64_t *bytes_read) {
    if (!file_open) {
        return 0;
    }
    
    uint64_t remaining = current_file.size - current_file.position;
    uint64_t to_read = size < remaining ? size : remaining;
    
    memcpy(buffer, current_file.data + current_file.position, to_read);
    current_file.position += to_read;
    
    if (bytes_read) {
        *bytes_read = to_read;
    }
    
    return to_read > 0;
}

/*
 * Seek in file
 */
int fs_seek(uint64_t offset) {
    if (!file_open) {
        return 0;
    }
    
    if (offset > current_file.size) {
        offset = current_file.size;
    }
    
    current_file.position = offset;
    return 1;
}

/*
 * Get file size
 */
uint64_t fs_get_size(void) {
    if (!file_open) {
        return 0;
    }
    
    return current_file.size;
}

/*
 * Get current position
 */
uint64_t fs_get_position(void) {
    if (!file_open) {
        return 0;
    }
    
    return current_file.position;
}

/*
 * Find file in FAT32
 */
static int find_file(const char *path, file_handle_t *file) {
    /* Simplified: assume file is in root directory */
    /* Parse path to get filename */
    const char *filename = path;
    while (*filename && *filename != '/') {
        filename++;
    }
    if (*filename == '/') {
        filename++;
    }
    
    /* Convert to 8.3 format */
    char short_name[11];
    memset(short_name, ' ', 11);
    int name_len = 0;
    int ext_len = 0;
    
    while (*filename && *filename != '.' && name_len < 8) {
        short_name[name_len++] = toupper(*filename++);
    }
    
    if (*filename == '.') {
        filename++;
        while (*filename && ext_len < 3) {
            short_name[8 + ext_len++] = toupper(*filename++);
        }
    }
    
    /* Search root directory */
    uint32_t cluster = fat32_bpb->root_cluster;
    uint8_t dir_buffer[512];
    
    while (cluster < 0x0FFFFFF8) {
        /* Read cluster */
        uint64_t cluster_offset = data_start + (cluster - 2) * fat32_bpb->sectors_per_cluster;
        
        if (!read_disk_sector(cluster_offset, dir_buffer)) {
            return 0;
        }
        
        /* Parse directory entries */
        fat32_dentry_t *entries = (fat32_dentry_t*)dir_buffer;
        int entries_per_sector = 512 / sizeof(fat32_dentry_t);
        
        for (int i = 0; i < entries_per_sector; i++) {
            if (entries[i].name[0] == 0x00) {
                /* End of directory */
                break;
            }
            
            if (entries[i].name[0] == 0xE5) {
                /* Deleted entry */
                continue;
            }
            
            if (entries[i].attributes & 0x10) {
                /* Directory */
                continue;
            }
            
            /* Check if this is our file */
            if (memcmp(entries[i].name, short_name, 11) == 0) {
                /* Found file */
                file->size = entries[i].file_size;
                file->in_use = 1;
                
                /* Load file data */
                uint32_t first_cluster = (entries[i].first_cluster_high << 16) | 
                                         entries[i].first_cluster_low;
                
                file->data = (uint8_t*)0x90000;  /* Load to 0x90000 */
                
                /* Read file clusters */
                uint8_t *buffer = file->data;
                uint32_t current_cluster = first_cluster;
                uint64_t bytes_read = 0;
                
                while (current_cluster < 0x0FFFFFF8 && bytes_read < file->size) {
                    uint64_t cluster_offset = data_start + (current_cluster - 2) * 
                                               fat32_bpb->sectors_per_cluster;
                    
                    if (!read_disk_sectors(cluster_offset, fat32_bpb->sectors_per_cluster, buffer)) {
                        return 0;
                    }
                    
                    uint64_t cluster_bytes = bytes_per_cluster;
                    if (bytes_read + cluster_bytes > file->size) {
                        cluster_bytes = file->size - bytes_read;
                    }
                    
                    buffer += cluster_bytes;
                    bytes_read += cluster_bytes;
                    
                    /* Get next cluster from FAT */
                    current_cluster = get_fat_entry(current_cluster);
                }
                
                return 1;
            }
        }
        
        /* Get next cluster */
        cluster = get_fat_entry(cluster);
    }
    
    return 0;
}

/*
 * Read disk sector
 */
static int read_disk_sector(uint32_t lba, uint8_t *buffer) {
    return read_disk_sectors(lba, 1, buffer);
}

/*
 * Read multiple disk sectors
 */
static int read_disk_sectors(uint32_t lba, uint32_t count, uint8_t *buffer) {
    /* Use BIOS INT 13h, AH=42h (Extended Read) */
    /* This is a simplified implementation */
    
    /* For now, assume success */
    /* In a real implementation, this would call BIOS INT 13h */
    
    return 1;
}

/*
 * Get FAT entry
 */
static uint32_t get_fat_entry(uint32_t cluster) {
    if (cluster * 4 >= fat32_bpb->fat_size_32 * 512) {
        return 0x0FFFFFF8;
    }
    
    return *((uint32_t*)(fat_table + cluster * 4)) & 0x0FFFFFFF;
}

/*
 * Convert character to uppercase
 */
static char toupper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

/*
 * List directory
 */
int fs_list_dir(const char *path, char *buffer, uint64_t buffer_size) {
    /* Simplified implementation */
    if (buffer && buffer_size > 0) {
        buffer[0] = '\0';
    }
    return 0;
}
