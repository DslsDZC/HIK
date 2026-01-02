/*
 * HIK Core-1 Inter-Process Communication
 * 
 * IPC mechanism for Core-1 services to communicate with
 * each other, Core-0, and Application-3.
 */

#ifndef HIK_CORE1_IPC_H
#define HIK_CORE1_IPC_H

#include "stdint.h"
#include "stddef.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

/* Maximum message size */
#define IPC_MAX_MSG_SIZE 4096

/* Maximum endpoints per service */
#define IPC_MAX_ENDPOINTS 32

/* IPC message types */
typedef enum {
    IPC_TYPE_REQUEST = 0,
    IPC_TYPE_RESPONSE = 1,
    IPC_TYPE_NOTIFICATION = 2,
    IPC_TYPE_ERROR = 3
} ipc_msg_type_t;

/* IPC endpoint types */
typedef enum {
    IPC_ENDPOINT_CLIENT = 0,    /* Client endpoint */
    IPC_ENDPOINT_SERVER = 1,    /* Server endpoint */
    IPC_ENDPOINT_BROADCAST = 2  /* Broadcast endpoint */
} ipc_endpoint_type_t;

/* IPC message header */
typedef struct {
    uint32_t msg_type;          /* Message type */
    uint32_t msg_id;            /* Message ID */
    uint32_t src_service;       /* Source service ID */
    uint32_t dst_service;       /* Destination service ID */
    uint32_t data_size;         /* Data size */
    uint32_t flags;             /* Message flags */
    uint64_t timestamp;         /* Timestamp */
} __attribute__((packed)) ipc_msg_header_t;

/* IPC message */
typedef struct {
    ipc_msg_header_t header;    /* Message header */
    uint8_t data[IPC_MAX_MSG_SIZE]; /* Message data */
} __attribute__((packed)) ipc_msg_t;

/* IPC endpoint */
struct ipc_endpoint {
    uint64_t endpoint_id;       /* Endpoint ID */
    char name[64];              /* Endpoint name */
    ipc_endpoint_type_t type;   /* Endpoint type */
    uint64_t service_id;        /* Owning service ID */
    void (*handler)(ipc_msg_t *msg); /* Message handler */
    struct ipc_endpoint *next;  /* Next endpoint */
};

typedef struct ipc_endpoint ipc_endpoint_t;

/* IPC channel (for direct shared memory communication) */
typedef struct {
    uint64_t channel_id;        /* Channel ID */
    uint64_t phys_addr;         /* Physical address of shared memory */
    uint64_t size;              /* Shared memory size */
    uint64_t cap_handle;        /* Capability handle */
    volatile uint64_t read_ptr; /* Read pointer */
    volatile uint64_t write_ptr; /* Write pointer */
    uint64_t lock;              /* Spinlock */
} ipc_channel_t;

/* Ring buffer structure for zero-copy IPC */
typedef struct {
    uint64_t capacity;          /* Ring buffer capacity */
    uint64_t mask;              /* Capacity - 1 (must be power of 2) */
    volatile uint64_t head;     /* Head index */
    volatile uint64_t tail;     /* Tail index */
    uint8_t data[];             /* Data buffer */
} __attribute__((packed)) ring_buffer_t;

/* Initialize IPC subsystem */
int ipc_init(void);

/* Register an endpoint */
int ipc_register_endpoint(const char *name, ipc_endpoint_type_t type, 
                         void (*handler)(ipc_msg_t *msg));

/* Unregister an endpoint */
int ipc_unregister_endpoint(const char *name);

/* Find endpoint by name */
struct ipc_endpoint* ipc_find_endpoint(const char *name);

/* Send IPC message */
int ipc_send(uint64_t endpoint_id, ipc_msg_t *msg);

/* Receive IPC message (blocking) */
int ipc_recv(ipc_msg_t *msg, uint64_t timeout_ms);

/* Receive IPC message (non-blocking) */
int ipc_try_recv(ipc_msg_t *msg);

/* Create shared memory channel */
int ipc_create_channel(uint64_t size, ipc_channel_t **channel);

/* Destroy shared memory channel */
int ipc_destroy_channel(ipc_channel_t *channel);

/* Write to ring buffer (zero-copy) */
int ipc_ring_write(ring_buffer_t *ring, const void *data, uint64_t size);

/* Read from ring buffer (zero-copy) */
int ipc_ring_read(ring_buffer_t *ring, void *data, uint64_t size);

/* Get available space in ring buffer */
uint64_t ipc_ring_available(ring_buffer_t *ring);

/* Get used space in ring buffer */
uint64_t ipc_ring_used(ring_buffer_t *ring);

/* Memory barrier for IPC synchronization */
void ipc_memory_barrier(void);

/* Read memory barrier */
void ipc_read_barrier(void);

/* Write memory barrier */
void ipc_write_barrier(void);

#endif /* HIK_CORE1_IPC_H */