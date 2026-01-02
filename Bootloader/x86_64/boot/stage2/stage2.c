/*
 * HIK BIOS Bootloader - Stage 2
 * 
 * This is the second stage bootloader that runs in 32-bit protected mode.
 * It performs the following tasks:
 * 1. Initialize hardware (GDT, IDT, paging)
 * 2. Detect hardware (memory, VESA, keyboard)
 * 3. Load kernel from file system
 * 4. Verify kernel signature
 * 5. Setup boot information structure
 * 6. Switch to long mode
 * 7. Jump to kernel
 */

#include "stage2.h"
#include "../hal/hal.h"
#include "../fs/fs.h"
#include "../security/verify.h"
#include "../util/util.h"

/* Boot information structure */
static hik_boot_info_t boot_info;

/* Kernel header */
static kernel_header_t kernel_header;

/* External functions from assembly */
/* Note: switch_to_long_mode is not implemented yet */

/* Forward declarations for static functions */
static int load_kernel(void);
static int load_sections(void);
static void setup_boot_info(void);

/*
 * Stage 2 main entry point
 */
void stage2_main(void) {
    /* Clear screen */
    hal_clear_screen();
    
    /* Print banner */
    hal_print("HIK Bootloader Stage 2\n");
    hal_print("========================\n\n");
    
    /* Initialize HAL */
    hal_print("Initializing hardware...\n");
    if (!hal_init()) {
        hal_print("ERROR: HAL initialization failed!\n");
        halt();
    }
    hal_print("Hardware initialized.\n\n");
    
    /* Detect memory */
    hal_print("Detecting memory...\n");
    if (!hal_detect_memory(&boot_info)) {
        hal_print("ERROR: Memory detection failed!\n");
        halt();
    }
    hal_print("Memory detected.\n\n");
    
    /* Detect ACPI */
    hal_print("Detecting ACPI tables...\n");
    boot_info.rsdp = hal_find_rsdp();
    if (boot_info.rsdp != 0) {
        hal_print("ACPI RSDP found at 0x");
        hal_print_hex(boot_info.rsdp);
        hal_print("\n");
    } else {
        hal_print("WARNING: ACPI not found.\n");
    }
    hal_print("\n");
    
    /* Initialize file system */
    hal_print("Initializing file system...\n");
    if (!fs_init()) {
        hal_print("ERROR: File system initialization failed!\n");
        halt();
    }
    hal_print("File system initialized.\n\n");
    
    /* Load kernel */
    hal_print("Loading kernel...\n");
    if (!load_kernel()) {
        hal_print("ERROR: Kernel load failed!\n");
        halt();
    }
    hal_print("Kernel loaded successfully.\n\n");
    
    /* Verify kernel */
    hal_print("Verifying kernel signature...\n");
    if (!verify_kernel(&kernel_header)) {
        hal_print("ERROR: Kernel verification failed!\n");
        halt();
    }
    hal_print("Kernel verified.\n\n");
    
    /* Setup boot information */
    hal_print("Setting up boot information...\n");
    setup_boot_info();
    hal_print("Boot information ready.\n\n");

    /* For now, just halt - long mode switching is not implemented yet */
    hal_print("Bootloader completed successfully.\n");
    hal_print("Halting system.\n");
    halt();
}

/*
 * Load kernel from file system
 */
static int load_kernel(void) {
    const char *kernel_path = "/HIK/kernel.hik";
    uint64_t file_size;
    uint64_t bytes_read;
    
    /* Open kernel file */
    if (!fs_open(kernel_path)) {
        hal_print("ERROR: Cannot open kernel file: ");
        hal_print(kernel_path);
        hal_print("\n");
        return 0;
    }
    
    /* Get file size */
    file_size = fs_get_size();
    if (file_size > MAX_KERNEL_SIZE) {
        hal_print("ERROR: Kernel too large: ");
        hal_print_hex(file_size);
        hal_print("\n");
        return 0;
    }
    
    /* Read kernel header */
    if (!fs_read(&kernel_header, sizeof(kernel_header), &bytes_read) || 
        bytes_read != sizeof(kernel_header)) {
        hal_print("ERROR: Cannot read kernel header\n");
        return 0;
    }
    
    /* Verify kernel magic */
    if (kernel_header.signature != HIK_KERNEL_MAGIC) {
        hal_print("ERROR: Invalid kernel magic\n");
        return 0;
    }
    
    hal_print("Kernel version: ");
    hal_print_dec(kernel_header.version);
    hal_print("\n");

    /* Calculate total kernel size (code + data + config + signature) */
    uint64_t kernel_image_size = kernel_header.code_size + kernel_header.data_size +
                                  kernel_header.config_size + kernel_header.signature_size;

    hal_print("Kernel size: ");
    hal_print_dec(kernel_image_size / 1024);
    hal_print(" KB\n");

    /* Read entire kernel image */
    if (!fs_seek(0)) {
        hal_print("ERROR: Cannot seek in kernel file\n");
        return 0;
    }

    if (!fs_read((void*)KERNEL_LOAD_ADDR, kernel_image_size, &bytes_read) ||
        bytes_read != kernel_image_size) {
        hal_print("ERROR: Cannot read kernel image\n");
        return 0;
    }
    
    /* Parse and load sections */
    if (!load_sections()) {
        hal_print("ERROR: Cannot load kernel sections\n");
        return 0;
    }
    
    return 1;
}

/*
 * Load kernel sections
 */
static int load_sections(void) {
    /* New format uses direct offsets instead of section table */
    hal_print("Loading kernel sections...\n");

    /* Code section */
    if (kernel_header.code_size > 0) {
        uintptr_t code_addr = (uintptr_t)KERNEL_LOAD_ADDR + kernel_header.code_offset;
        (void)code_addr;  /* Suppress unused variable warning */
        /* Already in place, just verify */
        hal_print("  Code section: ");
        hal_print_dec(kernel_header.code_size);
        hal_print(" bytes\n");
    }

    /* Data section */
    if (kernel_header.data_size > 0) {
        uintptr_t data_addr = (uintptr_t)KERNEL_LOAD_ADDR + kernel_header.data_offset;
        (void)data_addr;  /* Suppress unused variable warning */
        /* Already in place, just verify */
        hal_print("  Data section: ");
        hal_print_dec(kernel_header.data_size);
        hal_print(" bytes\n");
    }

    return 1;
}

/*
 * Setup boot information structure
 */
static void setup_boot_info(void) {
    /* Set magic and version */
    boot_info.magic = 0x214B4948;  /* "HIK!" */
    boot_info.version = 1;
    boot_info.flags = BOOT_FLAG_SERIAL | BOOT_FLAG_DEBUG;
    
    /* Memory information */
    boot_info.memory_map_base = MEMORY_MAP_ADDR;
    boot_info.memory_map_size = hal_get_memory_map_size();
    boot_info.memory_map_desc_size = sizeof(memory_map_entry_t);
    boot_info.memory_map_count = hal_get_memory_map_count();
    
    /* BIOS information */
    boot_info.bios_data_area = 0x400;
    boot_info.vbe_info = 0;
    
    /* Kernel information */
    boot_info.kernel_base = KERNEL_LOAD_ADDR;
    /* Calculate total kernel size */
    boot_info.kernel_size = kernel_header.code_size + kernel_header.data_size +
                           kernel_header.config_size + kernel_header.signature_size;
    boot_info.entry_point = kernel_header.entry_point;
    
    /* Command line */
    strcpy(boot_info.cmdline, "console=ttyS0,115200");
    
    /* Module information */
    boot_info.modules = 0;
    boot_info.module_count = 0;
}

/*
 * Halt the system
 */

/*
 * String functions
 */