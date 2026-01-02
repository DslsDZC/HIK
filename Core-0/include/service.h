/*
 * HIK Core-0 Service Management
 * 
 * This file defines the service management system for Core-0.
 * It manages Core-1 service domains and provides isolation.
 */

#ifndef HIK_CORE0_SERVICE_H
#define HIK_CORE0_SERVICE_H

#include <stdint.h>
#include "../include/capability.h"

/* Maximum number of services */
#define MAX_SERVICES 64

/* Service state */
typedef enum {
    SERVICE_STATE_STOPPED = 0,
    SERVICE_STATE_STARTING = 1,
    SERVICE_STATE_RUNNING = 2,
    SERVICE_STATE_STOPPING = 3,
    SERVICE_STATE_ERROR = 4
} service_state_t;

/* Service structure */
typedef struct {
    uint64_t service_id;         /* Service ID */
    char name[64];               /* Service name */
    service_state_t state;       /* Service state */
    uint64_t domain_id;          /* Associated domain ID */
    uint64_t entry_point;        /* Service entry point */
    uint64_t code_base;          /* Code base address */
    uint64_t code_size;          /* Code size */
    uint64_t data_base;          /* Data base address */
    uint64_t data_size;          /* Data size */
    uint64_t stack_base;         /* Stack base address */
    uint64_t stack_size;         /* Stack size */
    uint32_t num_threads;        /* Number of threads */
    uint32_t restart_count;      /* Restart count */
    uint64_t uptime;             /* Service uptime */
    uint64_t last_error;         /* Last error code */
    cap_handle_t cap_handle;     /* Service capability handle */
} service_t;

/* Service manager state */
typedef struct {
    service_t services[MAX_SERVICES];  /* Service table */
    uint32_t num_services;              /* Number of services */
    uint64_t next_service_id;           /* Next service ID */
    uint64_t lock;                      /* Spinlock */
} service_manager_t;

/* Core-0 API for services (exported to Core-1) */
typedef struct {
    /* Capability operations */
    cap_handle_t (*cap_grant)(cap_handle_t cap, uint64_t target_service_id);
    int (*cap_revoke)(cap_handle_t cap, uint64_t domain_id);
    int (*cap_check)(cap_handle_t cap, uint32_t permission);
    
    /* Memory operations */
    void* (*mem_alloc)(uint64_t size, uint64_t align);
    void (*mem_free)(void *ptr);
    int (*mem_map)(uint64_t phys_addr, uint64_t size, uint32_t permissions);
    void (*mem_unmap)(void *virt_addr);
    
    /* IPC operations */
    int (*ipc_call)(cap_handle_t endpoint, void *request, void *response);
    int (*ipc_register)(const char *name, void *handler);
    int (*ipc_unregister)(const char *name);
    
    /* Thread operations */
    int (*thread_create)(void (*func)(void*), void *arg);
    void (*thread_exit)(int code);
    void (*thread_yield)(void);
    void (*thread_sleep)(uint64_t milliseconds);
    
    /* I/O operations */
    uint8_t (*inb)(uint16_t port);
    void (*outb)(uint16_t port, uint8_t value);
    uint16_t (*inw)(uint16_t port);
    void (*outw)(uint16_t port, uint16_t value);
    uint32_t (*inl)(uint16_t port);
    void (*outl)(uint16_t port, uint32_t value);
    
    /* Logging */
    void (*log)(const char *message);
    void (*log_hex)(uint64_t value);
    
    /* Service lifecycle */
    int (*service_start)(uint64_t service_id);
    int (*service_stop)(uint64_t service_id);
    int (*service_restart)(uint64_t service_id);
} core0_api_t;

/* Initialize service manager */
int service_init(void);

/* Create a new service */
uint64_t service_create(const char *name, uint64_t entry_point,
                       uint64_t code_base, uint64_t code_size,
                       uint64_t data_base, uint64_t data_size);

/* Start a service */
int service_start(uint64_t service_id);

/* Stop a service */
int service_stop(uint64_t service_id);

/* Restart a service */
int service_restart(uint64_t service_id);

/* Terminate a service */
int service_terminate(uint64_t service_id);

/* Get service by ID */
service_t* service_get(uint64_t service_id);

/* Get service by name */
service_t* service_get_by_name(const char *name);

/* Handle service fault */
int service_handle_fault(uint64_t service_id, uint64_t error_code);

/* Dump service state (for debugging) */
void service_dump(uint64_t service_id);

/* Get Core-0 API pointer */
core0_api_t* service_get_api(void);

#endif /* HIK_CORE0_SERVICE_H */