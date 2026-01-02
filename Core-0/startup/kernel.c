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
#include "../include/longmode.h"
#include "../include/irq.h"
#include "../include/isolation.h"
#include <string.h>

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

/*
 * Kernel log
 */
void kernel_log(const char *message) {
    /* In real implementation, would write to serial port or VGA */
    /* For now, just use inline assembly to write to VGA */
    const char *p = message;
    uint16_t *vga = (uint16_t*)0xB8000;
    static int pos = 0;
    
    while (*p && pos < 80 * 25) {
        vga[pos++] = (uint16_t)(*p++ | 0x0F00);
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
    g_boot_info = boot_info;
    
    kernel_log("HIK Core-0 Kernel v1.0\n");
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
    
    /* Initialize service manager */
    kernel_log("Initializing service manager...\n");
    if (service_init() != 0) {
        kernel_panic("Failed to initialize service manager");
    }
    kernel_log("Service manager initialized\n\n");
    
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