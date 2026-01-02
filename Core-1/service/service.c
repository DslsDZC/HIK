/*
 * HIK Core-1 Service Framework Implementation
 * 
 * Provides common functionality for Core-1 services including
 * lifecycle management, IPC, and resource management.
 */

#include "../include/service.h"
#include "../include/core1.h"
#include "../include/string.h"

/* Global service context */
static service_context_t g_service_ctx = {0};
static service_callbacks_t g_callbacks = {0};
static uint64_t g_service_lock = 0;

/* Spinlock implementation */
static inline void spin_lock(uint64_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        __asm__ volatile ("pause");
    }
}

static inline void spin_unlock(uint64_t *lock) {
    __sync_lock_release(lock);
}

/* Initialize service framework */
int service_framework_init(const char *name, const char *version, uint32_t flags) {
    if (!name || !version) {
        return -1;
    }

    spin_lock(&g_service_lock);

    /* Initialize service context */
    strncpy(g_service_ctx.config.name, name, sizeof(g_service_ctx.config.name) - 1);
    g_service_ctx.config.name[sizeof(g_service_ctx.config.name) - 1] = '\0';
    
    strncpy(g_service_ctx.config.version, version, sizeof(g_service_ctx.config.version) - 1);
    g_service_ctx.config.version[sizeof(g_service_ctx.config.version) - 1] = '\0';
    
    g_service_ctx.config.flags = flags;
    g_service_ctx.config.priority = 0;
    
    g_service_ctx.state = SERVICE_STATE_INIT;
    g_service_ctx.start_time = 0;  /* Would get from system timer */
    g_service_ctx.uptime = 0;
    g_service_ctx.error_count = 0;
    g_service_ctx.last_error = 0;

    spin_unlock(&g_service_lock);

    return 0;
}

/* Register service callbacks */
int service_register_callbacks(const service_callbacks_t *callbacks) {
    if (!callbacks) {
        return -1;
    }

    spin_lock(&g_service_lock);
    g_callbacks = *callbacks;
    spin_unlock(&g_service_lock);

    return 0;
}

/* Start service */
int service_start(void) {
    spin_lock(&g_service_lock);

    if (g_service_ctx.state != SERVICE_STATE_INIT && 
        g_service_ctx.state != SERVICE_STATE_STOPPED) {
        spin_unlock(&g_service_lock);
        return -1;  /* Invalid state */
    }

    g_service_ctx.state = SERVICE_STATE_RUNNING;
    g_service_ctx.start_time = 0;  /* Would get from system timer */

    spin_unlock(&g_service_lock);

    /* Call start callback if registered */
    if (g_callbacks.start) {
        return g_callbacks.start(&g_service_ctx);
    }

    return 0;
}

/* Stop service */
int service_stop(void) {
    spin_lock(&g_service_lock);

    if (g_service_ctx.state != SERVICE_STATE_RUNNING) {
        spin_unlock(&g_service_lock);
        return -1;  /* Invalid state */
    }

    g_service_ctx.state = SERVICE_STATE_STOPPING;

    spin_unlock(&g_service_lock);

    /* Call stop callback if registered */
    if (g_callbacks.stop) {
        g_callbacks.stop(&g_service_ctx);
    }

    spin_lock(&g_service_lock);
    g_service_ctx.state = SERVICE_STATE_STOPPED;
    spin_unlock(&g_service_lock);

    return 0;
}

/* Restart service */
int service_restart(void) {
    int ret = service_stop();
    if (ret != 0) {
        return ret;
    }

    return service_start();
}

/* Get service context */
service_context_t* service_get_context(void) {
    return &g_service_ctx;
}

/* Report error */
void service_report_error(uint64_t error_code) {
    spin_lock(&g_service_lock);
    g_service_ctx.error_count++;
    g_service_ctx.last_error = error_code;
    spin_unlock(&g_service_lock);

    /* Call error handler if registered */
    if (g_callbacks.error_handler) {
        g_callbacks.error_handler(error_code);
    }
}

/* Update uptime */
void service_update_uptime(void) {
    /* In a real implementation, this would get the current time
       from the system timer and update the uptime */
}

/* Yield CPU to other services */
void service_yield(void) {
    __asm__ volatile ("pause");
}

/* Sleep for specified milliseconds */
void service_sleep(uint64_t milliseconds) {
    /* In a real implementation, this would use the system timer
       to sleep for the specified duration */
    for (volatile uint64_t i = 0; i < milliseconds * 1000; i++) {
        __asm__ volatile ("pause");
    }
}

/* Log service message */
void service_log(const char *message) {
    if (!message) {
        return;
    }

    /* Prefix with service name */
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "[%s] %s", g_service_ctx.config.name, message);

    /* Call Core-0 log function */
    if (g_core0_api && g_core0_api->log) {
        g_core0_api->log(log_msg);
    }
}

/* Log service error */
void service_log_error(const char *message, uint64_t error_code) {
    if (!message) {
        return;
    }

    /* Prefix with service name and error code */
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "[%s] ERROR: %s (0x%lx)", 
             g_service_ctx.config.name, message, error_code);

    /* Call Core-0 log function */
    if (g_core0_api && g_core0_api->log) {
        g_core0_api->log(log_msg);
    }

    /* Report error */
    service_report_error(error_code);
}

/* Default service entry point */
void service_main(void) {
    /* Initialize callbacks */
    service_callbacks_t callbacks = {
        .init = NULL,
        .start = NULL,
        .stop = NULL,
        .cleanup = NULL,
        .error_handler = NULL
    };
    service_register_callbacks(&callbacks);

    /* Start service */
    service_start();

    /* Log service start */
    service_log("Service started");
}