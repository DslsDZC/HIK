/*
 * HIK BIOS Bootloader - Boot Manager
 */

#ifndef BOOTMGR_H
#define BOOTMGR_H

#include <stdint.h>

/* Boot configuration */
typedef struct {
    char title[64];
    char kernel_path[128];
    char initrd_path[128];
    char cmdline[256];
    int timeout;
    int default_entry;
} boot_config_t;

/* Initialize boot manager */
int bootmgr_init(void);

/* Load boot configuration */
int bootmgr_load_config(const char *path);

/* Get boot configuration */
boot_config_t* bootmgr_get_config(void);

/* Display boot menu */
int bootmgr_display_menu(void);

/* Boot default entry */
int bootmgr_boot_default(void);

#endif /* BOOTMGR_H */