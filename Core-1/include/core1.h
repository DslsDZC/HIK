/*
 * HIK Core-1 (Privileged-1 Layer) Header
 * 
 * Core-1 is the privileged service layer that runs services in
 * physical memory with direct mapping. Each service is an isolated
 * domain with its own physical memory region.
 */

#ifndef HIK_CORE1_H
#define HIK_CORE1_H

#include "stdint.h"
#include "stddef.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

/* Service entry point type */
typedef void (*service_entry_t)(void);

/* Service information passed from Core-0 */
typedef struct {
    uint64_t service_id;         /* Service ID */
    uint64_t domain_id;          /* Domain ID */
    uint64_t code_base;          /* Physical code base address */
    uint64_t code_size;          /* Code size */
    uint64_t data_base;          /* Physical data base address */
    uint64_t data_size;          /* Data size */
    uint64_t stack_base;         /* Physical stack base address */
    uint64_t stack_size;         /* Stack size */
    uint64_t heap_base;          /* Physical heap base address */
    uint64_t heap_size;          /* Heap size */
    uint32_t num_caps;           /* Number of capabilities */
    uint64_t cap_handles[64];    /* Capability handles */
} __attribute__((packed)) service_info_t;

/* Core-0 API structure */
typedef struct {
    /* Capability operations */
    uint64_t (*cap_grant)(uint64_t cap, uint64_t target_service_id);
    int (*cap_revoke)(uint64_t cap, uint64_t domain_id);
    int (*cap_check)(uint64_t cap, uint32_t permission);
    
    /* Memory operations */
    void* (*mem_alloc)(uint64_t size, uint64_t align);
    void (*mem_free)(void *ptr);
    int (*mem_map)(uint64_t phys_addr, uint64_t size, uint32_t permissions);
    void (*mem_unmap)(void *virt_addr);
    
    /* IPC operations */
    int (*ipc_call)(uint64_t endpoint, void *request, void *response);
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
} __attribute__((packed)) core0_api_t;

/* Global service info and API pointer */
extern service_info_t *g_service_info;
extern core0_api_t *g_core0_api;

/* Initialize Core-1 service */
int core1_init(service_info_t *info, core0_api_t *api);

/* Main service entry point */
void core1_main(void);

/* Service cleanup */
void core1_cleanup(void);

/* Panic handler */
void core1_panic(const char *message) __attribute__((noreturn));

#endif /* HIK_CORE1_H */