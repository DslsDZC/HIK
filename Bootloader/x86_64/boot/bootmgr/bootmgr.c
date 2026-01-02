/*
 * HIK BIOS Bootloader - Boot Manager Implementation
 */

#include "bootmgr.h"
#include "../hal/hal.h"
#include "../fs/fs.h"
#include "../util/util.h"

/* Boot configuration */
static boot_config_t boot_config;

/*
 * Initialize boot manager
 */
int bootmgr_init(void) {
    /* Set default configuration */
    strcpy(boot_config.title, "HIK Boot Manager");
    strcpy(boot_config.kernel_path, "/HIK/kernel.hik");
    strcpy(boot_config.initrd_path, "");
    strcpy(boot_config.cmdline, "console=ttyS0,115200");
    boot_config.timeout = 5;
    boot_config.default_entry = 0;
    
    return 1;
}

/*
 * Load boot configuration from file
 */
int bootmgr_load_config(const char *path) {
    char buffer[1024];
    uint64_t bytes_read;
    
    if (!fs_open(path)) {
        hal_print("WARNING: Cannot open boot config file: ");
        hal_print(path);
        hal_print("\n");
        hal_print("Using default configuration.\n");
        return 0;
    }
    
    if (!fs_read(buffer, sizeof(buffer) - 1, &bytes_read)) {
        hal_print("WARNING: Cannot read boot config file\n");
        fs_close();
        return 0;
    }
    
    buffer[bytes_read] = '\0';
    fs_close();
    
    /* Parse configuration (simplified) */
    char *line = buffer;
    while (*line) {
        char *end = line;
        while (*end && *end != '\n') {
            end++;
        }
        *end = '\0';
        
        /* Parse key=value */
        char *key = line;
        char *value = strchr(line, '=');
        if (value) {
            *value++ = '\0';
            
            /* Trim whitespace */
            while (*value == ' ') value++;
            
            /* Parse configuration options */
            if (strcmp(key, "title") == 0) {
                strcpy(boot_config.title, value);
            } else if (strcmp(key, "kernel") == 0) {
                strcpy(boot_config.kernel_path, value);
            } else if (strcmp(key, "initrd") == 0) {
                strcpy(boot_config.initrd_path, value);
            } else if (strcmp(key, "args") == 0) {
                strcpy(boot_config.cmdline, value);
            } else if (strcmp(key, "timeout") == 0) {
                boot_config.timeout = strtoul(value, NULL, 10);
            }
        }
        
        line = end + 1;
    }
    
    hal_print("Boot configuration loaded:\n");
    hal_print("  Title: ");
    hal_print(boot_config.title);
    hal_print("\n");
    hal_print("  Kernel: ");
    hal_print(boot_config.kernel_path);
    hal_print("\n");
    hal_print("  Args: ");
    hal_print(boot_config.cmdline);
    hal_print("\n");
    
    return 1;
}

/*
 * Get boot configuration
 */
boot_config_t* bootmgr_get_config(void) {
    return &boot_config;
}

/*
 * Display boot menu
 */
int bootmgr_display_menu(void) {
    hal_print("\n");
    hal_print("========================================\n");
    hal_print(boot_config.title);
    hal_print("\n");
    hal_print("========================================\n");
    hal_print("\n");
    hal_print("Boot entries:\n");
    hal_print("  [0] HIK Kernel\n");
    hal_print("\n");
    hal_print("Default: [0] (timeout in ");
    hal_print_dec(boot_config.timeout);
    hal_print(" seconds)\n");
    hal_print("\n");
    
    /* Wait for user input or timeout */
    for (int i = boot_config.timeout; i > 0; i--) {
        hal_print("\rBooting in ");
        hal_print_dec(i);
        hal_print(" seconds... Press any key to stop");
        
        /* Check for key press (simplified) */
        if (inb(0x64) & 1) {
            inb(0x60);  /* Clear key */
            hal_print("\n\nBoot menu activated (not implemented)\n");
            return 1;
        }
        
        delay(1000);
    }
    
    hal_print("\n\n");
    return 0;
}

/*
 * Boot default entry
 */
int bootmgr_boot_default(void) {
    hal_print("Booting default entry...\n");
    return 1;
}