/*
 * HIK Core-1 Service Framework
 * 
 * Base framework for Core-1 services. Provides common
 * functionality for service lifecycle, IPC, and resource management.
 */

#ifndef HIK_CORE1_SERVICE_H
#define HIK_CORE1_SERVICE_H

#include "stdint.h"
#include "stddef.h"
#include "core1.h"
#include "ipc.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

/* Service states */
typedef enum {
    SERVICE_STATE_INIT = 0,     /* Initializing */
    SERVICE_STATE_RUNNING = 1,  /* Running */
    SERVICE_STATE_STOPPING = 2, /* Stopping */
    SERVICE_STATE_STOPPED = 3,  /* Stopped */
    SERVICE_STATE_ERROR = 4     /* Error */
} service_state_t;

/* Service configuration */
typedef struct {
    char name[64];              /* Service name */
    char version[32];           /* Service version */
    uint32_t flags;             /* Service flags */
    uint32_t priority;          /* Service priority */
} __attribute__((packed)) service_config_t;

/* Service context */
typedef struct {
    service_config_t config;    /* Service configuration */
    service_state_t state;      /* Current state */
    uint64_t start_time;        /* Start timestamp */
    uint64_t uptime;            /* Uptime in ms */
    uint32_t error_count;       /* Error count */
    uint64_t last_error;        /* Last error code */
} service_context_t;

/* Service lifecycle callbacks */
typedef struct {
    int (*init)(service_context_t *ctx);      /* Initialization callback */
    int (*start)(service_context_t *ctx);     /* Start callback */
    int (*stop)(service_context_t *ctx);      /* Stop callback */
    int (*cleanup)(service_context_t *ctx);   /* Cleanup callback */
    void (*error_handler)(uint64_t error);    /* Error handler */
} service_callbacks_t;

/* Service flags */
#define SERVICE_FLAG_AUTO_START    0x01
#define SERVICE_FLAG_RESTARTABLE   0x02
#define SERVICE_FLAG_CRITICAL      0x04
#define SERVICE_FLAG_PRIVILEGED    0x08

/* Initialize service framework */
int service_framework_init(const char *name, const char *version, uint32_t flags);

/* Register service callbacks */
int service_register_callbacks(const service_callbacks_t *callbacks);

/* Start service */
int service_start(void);

/* Stop service */
int service_stop(void);

/* Restart service */
int service_restart(void);

/* Get service context */
service_context_t* service_get_context(void);

/* Report error */
void service_report_error(uint64_t error_code);

/* Update uptime */
void service_update_uptime(void);

/* Yield CPU to other services */
void service_yield(void);

/* Sleep for specified milliseconds */
void service_sleep(uint64_t milliseconds);

/* Log service message */
void service_log(const char *message);

/* Log service error */
void service_log_error(const char *message, uint64_t error_code);

/* Default service entry point */
void service_main(void);

#endif /* HIK_CORE1_SERVICE_H */