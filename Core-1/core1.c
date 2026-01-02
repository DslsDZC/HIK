/*
 * HIK Core-1 Main Implementation
 * 
 * Core-1 is the privileged service layer that provides a framework
 * for running isolated services in physical memory.
 */

#include "core1.h"
#include "physical_mem.h"
#include "ipc.h"
#include "service.h"
#include "isolation.h"
#include "string.h"

/* Global service info and API pointer */
service_info_t *g_service_info = NULL;
core0_api_t *g_core0_api = NULL;

/* Initialize Core-1 service */
int core1_init(service_info_t *info, core0_api_t *api) {
    if (!info || !api) {
        return -1;
    }

    g_service_info = info;
    g_core0_api = api;

    /* Log initialization start */
    if (g_core0_api->log) {
        g_core0_api->log("Core-1: Initializing service...");
    }

    /* Initialize physical memory manager */
    if (pmm_init(info->heap_base, info->heap_size) != 0) {
        if (g_core0_api->log) {
            g_core0_api->log("Core-1: Failed to initialize memory manager");
        }
        return -1;
    }

    /* Initialize isolation */
    if (isolation_init(info->service_id, info->domain_id) != 0) {
        if (g_core0_api->log) {
            g_core0_api->log("Core-1: Failed to initialize isolation");
        }
        return -1;
    }

    /* Add memory regions to isolation */
    isolation_add_region(MEM_REGION_CODE, info->code_base, info->code_size,
                        MEM_PERM_READ | MEM_PERM_EXECUTE, 0);
    isolation_add_region(MEM_REGION_DATA, info->data_base, info->data_size,
                        MEM_PERM_READ | MEM_PERM_WRITE, 0);
    isolation_add_region(MEM_REGION_STACK, info->stack_base, info->stack_size,
                        MEM_PERM_READ | MEM_PERM_WRITE, 0);
    isolation_add_region(MEM_REGION_HEAP, info->heap_base, info->heap_size,
                        MEM_PERM_READ | MEM_PERM_WRITE, 0);

    /* Initialize IPC subsystem */
    if (ipc_init() != 0) {
        if (g_core0_api->log) {
            g_core0_api->log("Core-1: Failed to initialize IPC");
        }
        return -1;
    }

    /* Enable isolation */
    isolation_enable();

    /* Log initialization complete */
    if (g_core0_api->log) {
        g_core0_api->log("Core-1: Initialization complete");
    }

    return 0;
}

/* Main service entry point */
void core1_main(void) {
    /* Initialize service framework */
    service_framework_init("Core-1 Service", "1.0.0", 0);

    /* Call service-specific main */
    service_main();

    /* Main loop - keep service alive */
    while (1) {
        service_yield();
        service_sleep(100);
    }
}

/* Service cleanup */
void core1_cleanup(void) {
    /* Stop service framework */
    service_stop();

    /* Cleanup IPC */
    /* Note: IPC cleanup would be implemented here */

    /* Disable isolation */
    isolation_disable();

    /* Log cleanup complete */
    if (g_core0_api && g_core0_api->log) {
        g_core0_api->log("Core-1: Cleanup complete");
    }
}

/* Panic handler */
void core1_panic(const char *message) {
    /* Disable interrupts */
    __asm__ volatile ("cli");

    /* Log panic message */
    if (g_core0_api && g_core0_api->log) {
        g_core0_api->log("Core-1 PANIC: ");
        if (message) {
            g_core0_api->log(message);
        }
    }

    /* Infinite loop */
    while (1) {
        __asm__ volatile ("hlt");
    }
}