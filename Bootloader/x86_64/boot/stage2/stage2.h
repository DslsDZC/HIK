/*
 * HIK BIOS Bootloader - Stage 2 Header
 */

#ifndef STAGE2_H
#define STAGE2_H

#include <stdint.h>

/* Stage 2 load address */
#define STAGE2_LOAD_ADDR 0x10000

/* Stage 2 size (32KB) */
#define STAGE2_SIZE 0x8000

/* Kernel load address */
#define KERNEL_LOAD_ADDR 0x100000

/* Maximum kernel size (64MB) */
#define MAX_KERNEL_SIZE 0x4000000

/* Boot information structure address */
#define BOOT_INFO_ADDR 0x90000

/* Memory map address */
#define MEMORY_MAP_ADDR 0x95000

/* Maximum memory map entries */
#define MAX_MEMORY_MAP_ENTRIES 64

/* Memory map entry types */
#define MEMORY_TYPE_USABLE        1
#define MEMORY_TYPE_RESERVED      2
#define MEMORY_TYPE_ACPI_RECLAIM  3
#define MEMORY_TYPE_NVS           4
#define MEMORY_TYPE_UNUSABLE      5

/* Boot information structure */
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
} __attribute__((packed)) hik_boot_info_t;

/* Memory map entry */
typedef struct {
    uint64_t base_address;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
} __attribute__((packed)) memory_map_entry_t;

/* Kernel image header - matches UEFI version */
typedef struct {
    uint64_t signature;          /* HIK_KERNEL_MAGIC (0x48494B00) */
    uint32_t version;            /* Kernel version */
    uint32_t flags;              /* Flags (e.g., HIK_FLAG_SIGNED) */
    uint64_t entry_point;        /* Entry point offset */
    uint64_t code_offset;        /* Code section offset */
    uint64_t code_size;          /* Code section size */
    uint64_t data_offset;        /* Data section offset */
    uint64_t data_size;          /* Data section size */
    uint64_t config_offset;      /* Config section offset */
    uint64_t config_size;        /* Config section size */
    uint64_t signature_offset;   /* Signature section offset */
    uint64_t signature_size;     /* Signature section size */
    uint8_t  reserved[32];       /* Reserved for future use */
} __attribute__((packed)) kernel_header_t;

/* Section entry */
typedef struct {
    uint32_t type;               /* Section type */
    uint32_t flags;              /* Section flags */
    uint64_t file_offset;        /* File offset */
    uint64_t memory_offset;      /* Memory offset */
    uint64_t file_size;          /* File size */
    uint64_t memory_size;        /* Memory size */
    uint64_t alignment;          /* Alignment */
} __attribute__((packed)) section_entry_t;

/* Section types */
#define SECTION_TYPE_CODE    1
#define SECTION_TYPE_DATA    2
#define SECTION_TYPE_RODATA  3
#define SECTION_TYPE_BSS     4

/* Boot flags */
#define BOOT_FLAG_GRAPHICS    0x01
#define BOOT_FLAG_SERIAL      0x02
#define BOOT_FLAG_DEBUG       0x04
#define BOOT_FLAG_SECURE      0x08

/* Architecture IDs */
#define ARCH_ID_X86_64  1

/* Kernel magic */
#define HIK_KERNEL_MAGIC 0x48494B00  /* "HIK\0" */

/* Kernel flags */
#define HIK_FLAG_SIGNED    0x00000001

#endif /* STAGE2_H */