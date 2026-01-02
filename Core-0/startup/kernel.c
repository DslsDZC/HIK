/*
 * HIK Core-0 Kernel Initialization
 * 
 * This file contains the kernel initialization code.
 */

#include "../include/kernel.h"
#include "../include/mm.h"
#include "../include/capability.h"
#include "../include/sched.h"
#include "../include/service.h"
#include "../include/process.h"
#include "../include/longmode.h"
#include "../include/irq.h"
#include "../include/isolation.h"
#include "../include/string.h"

/* External test function */
extern int mmu_run_tests(void);

/* Boot information */
static boot_info_t *g_boot_info = NULL;

/*
 * Kernel panic
 */
void kernel_panic(const char *message) {
    kernel_log("KERNEL PANIC: ");
    kernel_log(message);
    kernel_log("\n");
    
    /* Halt */
    __asm__ volatile("cli");
    while (1) {
        __asm__ volatile("hlt");
    }
}

/* Global VGA position */
static int g_vga_pos = 0;

/*
 * Kernel log
 */
void kernel_log(const char *message) {
    /* Debug: Write 'L' and 'M' using inline assembly */
    __asm__ volatile(
        "movw $0x0F4C, %%ax\n"
        "movw %%ax, 0xB816\n"
        "movw $0x0F4D, %%ax\n"
        "movw %%ax, 0xB818\n"
        ::: "ax", "memory"
    );

    /* Infinite loop to pause */
    while (1) {
        __asm__ volatile("hlt");
    }

    /* Initialize pos if needed */
    if (g_vga_pos == 0) {
        g_vga_pos = 13;
    }

    /* Write message directly */
    volatile uint16_t *vga = (volatile uint16_t*)0xB8000;
    const char *p = message;
    while (*p && g_vga_pos < 80 * 25) {
        vga[g_vga_pos++] = (uint16_t)(*p++ | 0x0F00);
    }
}

/*
 * Kernel log hex
 */
void kernel_log_hex(uint64_t value) {
    char hex[17];
    const char *digits = "0123456789ABCDEF";
    
    for (int i = 15; i >= 0; i--) {
        hex[i] = digits[value & 0xF];
        value >>= 4;
    }
    hex[16] = '\0';
    
    kernel_log(hex);
}

/*
 * Get boot information
 */
boot_info_t* kernel_get_boot_info(void) {
    return g_boot_info;
}

/*
 * Initialize kernel
 */
int kernel_init(boot_info_t *boot_info) {
    /* Direct VGA write to confirm we're here */
    uint16_t *vga = (uint16_t*)0xB8000;
    vga[5] = (uint16_t)'K' | 0x0F00;  /* Write 'K' at position 5 */
    vga[6] = (uint16_t)'E' | 0x0F00;  /* Write 'E' at position 6 */
    vga[7] = (uint16_t)'R' | 0x0F00;  /* Write 'R' at position 7 */

    /* Debug: Step 1 */
    vga[8] = (uint16_t)'1' | 0x0F00;

    g_boot_info = boot_info;

    /* Debug: Step 2 */
    vga[9] = (uint16_t)'2' | 0x0F00;

    kernel_log("HIK Core-0 Kernel v1.0\n");

    /* Debug: Step 3 */
    vga[10] = (uint16_t)'3' | 0x0F00;

    /* Infinite loop to pause */
    while (1) {
        __asm__ volatile("hlt");
    }

    kernel_log("boot_info pointer: ");
    kernel_log_hex((uint64_t)boot_info);
    kernel_log("\n");

    if (boot_info == NULL) {
        kernel_log("WARNING: boot_info is NULL!\n");
        kernel_log("Using default values...\n");
    } else {
        kernel_log("boot_info magic: ");
        kernel_log_hex(boot_info->magic);
        kernel_log("\n");
    }

    kernel_log("Initializing...\n\n");
    
    /* Initialize memory manager */
    kernel_log("Initializing memory manager...\n");
    if (mm_init(boot_info->memory_map_size) != 0) {
        kernel_panic("Failed to initialize memory manager");
    }
    kernel_log("Memory manager initialized\n");
    kernel_log("Total memory: ");
    kernel_log_hex(boot_info->memory_map_size);
    kernel_log(" bytes\n\n");
    
    /* Initialize capability system */
    kernel_log("Initializing capability system...\n");
    if (cap_init() != 0) {
        kernel_panic("Failed to initialize capability system");
    }
    kernel_log("Capability system initialized\n\n");
    
    /* Initialize scheduler */
    kernel_log("Initializing scheduler...\n");
    if (sched_init() != 0) {
        kernel_panic("Failed to initialize scheduler");
    }
    kernel_log("Scheduler initialized\n\n");
    
    /* Initialize interrupt routing table */
    kernel_log("Initializing interrupt routing table...\n");
    if (irq_init() != 0) {
        kernel_panic("Failed to initialize interrupt routing table");
    }
    kernel_log("Interrupt routing table initialized\n\n");
    
    /* Initialize isolation system */
    kernel_log("Initializing isolation system...\n");
    if (isolation_init() != 0) {
        kernel_panic("Failed to initialize isolation system");
    }
    kernel_log("Isolation system initialized\n\n");
    
    /* Setup MMU for kernel domain */
    kernel_log("Setting up MMU for kernel domain...\n");
    if (isolation_create_page_tables(0, DOMAIN_FLAG_KERNEL) != 0) {
        kernel_panic("Failed to create kernel page tables");
    }
    
    /* Setup identity mapping for low memory */
    kernel_log("Setting up identity mapping...\n");
    if (pt_setup_identity_map(0, 0, 0x100000, PT_FLAG_PRESENT | PT_FLAG_WRITABLE) != 0) {
        kernel_panic("Failed to setup identity mapping");
    }
    
    /* Setup kernel mapping */
    kernel_log("Setting up kernel mapping...\n");
    if (pt_setup_kernel_map(0, 0x100000, 0x100000) != 0) {
        kernel_panic("Failed to setup kernel mapping");
    }
    
    kernel_log("MMU setup complete\n\n");
    
    /* Run MMU tests */
    kernel_log("Running MMU tests...\n");
    mmu_run_tests();
    
    /* Initialize service manager */
    kernel_log("Initializing service manager...\n");
    if (service_init() != 0) {
        kernel_panic("Failed to initialize service manager");
    }
    kernel_log("Service manager initialized\n\n");
    
    /* Initialize process manager */
    kernel_log("Initializing process manager...\n");
    if (process_init() != 0) {
        kernel_panic("Failed to initialize process manager");
    }
    kernel_log("Process manager initialized\n\n");
    
    /* Initialize long mode */
    kernel_log("Initializing long mode...\n");
    if (longmode_check_support() == 0) {
        kernel_panic("CPU does not support long mode");
    }
    
    longmode_enable_pae();
    longmode_setup_page_tables();
    
    kernel_log("Long mode initialized\n\n");
    
    kernel_log("Kernel initialization complete\n");
    kernel_log("Starting Core-1 services...\n\n");
    
    return 0;
}

/*
 * Start Core-1 services
 */
static void start_core1_services(void) {
    /* Create monitor service */
    kernel_log("Creating monitor service...\n");
    uint64_t monitor_id = service_create("monitor", 0x100000, 0x100000, 0x1000,
                                         0x101000, 0x1000);
    if (monitor_id == 0) {
        kernel_log("Failed to create monitor service\n");
        return;
    }
    
    /* Start monitor service */
    service_start(monitor_id);
    kernel_log("Monitor service started (ID: ");
    kernel_log_hex(monitor_id);
    kernel_log(")\n\n");
    
    /* Create console service */
    kernel_log("Creating console service...\n");
    uint64_t console_id = service_create("console", 0x102000, 0x102000, 0x1000,
                                         0x103000, 0x1000);
    if (console_id == 0) {
        kernel_log("Failed to create console service\n");
        return;
    }
    
    /* Start console service */
    service_start(console_id);
    kernel_log("Console service started (ID: ");
    kernel_log_hex(console_id);
    kernel_log(")\n\n");
}

/*
 * Kernel main loop
 */
void kernel_main(void) {
    kernel_log("HIK Core-0 kernel is running\n");
    kernel_log("============================\n\n");
    
    /* Start Core-1 services */
    start_core1_services();
    
    kernel_log("Starting Core-3 applications...\n");
    /* In a full implementation, would start user applications here */
    kernel_log("Core-3 applications ready\n\n");
    
    kernel_log("System ready\n");
    kernel_log("Press Ctrl+C to stop (not implemented)\n\n");
    
    /* Main kernel loop */
    while (1) {
        /* Handle interrupts */
        /* Schedule threads */
        /* Process events */
        
        /* Sleep for a bit */
        sched_sleep(1000);
    }
}