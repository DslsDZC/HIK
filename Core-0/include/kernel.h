/*
 * HIK Core-0 Kernel Header
 * 
 * This file defines the main kernel interfaces and structures.
 */

#ifndef HIK_CORE0_KERNEL_H
#define HIK_CORE0_KERNEL_H

#include "stdint.h"

/* Boot information structure (from bootloader) */
typedef struct {
    uint32_t magic;              /* "HIK!" */
    uint32_t version;            /* Structure version */
    uint64_t flags;              /* Feature flags */
    
    /* Memory information */
    uint64_t memory_map_base;
    uint64_t memory_map_size;
    uint64_t memory_map_desc_size;
    uint32_t memory_map_count;
    
    /* ACPI information */
    uint64_t rsdp;               /* ACPI RSDP address */
    
    /* BIOS information */
    uint64_t bios_data_area;     /* BDA pointer */
    uint32_t vbe_info;           /* VESA info block */
    
    /* Kernel information */
    uint64_t kernel_base;
    uint64_t kernel_size;
    uint64_t entry_point;
    
    /* Command line */
    char cmdline[256];
    
    /* Module information */
    uint64_t modules;
    uint32_t module_count;
} __attribute__((packed)) boot_info_t;

/* Kernel initialization */
int kernel_init(boot_info_t *boot_info);

/* Kernel main loop */
void kernel_main(void);

/* Kernel panic */
void kernel_panic(const char *message) __attribute__((noreturn));

/* Kernel log */
void kernel_log(const char *message);

/* Kernel log hex */
void kernel_log_hex(uint64_t value);

/* Get boot information */
boot_info_t* kernel_get_boot_info(void);

#endif /* HIK_CORE0_KERNEL_H */