/*
 * HIK Core-3 IPC Library
 *
 * IPC library for applications to communicate with services.
 */

#ifndef HIK_CORE3_IPC_H
#define HIK_CORE3_IPC_H

#include "stdint.h"
#include "stddef.h"

/* Maximum message size */
#define IPC_MAX_MSG_SIZE 4096

/* IPC message types */
typedef enum {
    IPC_TYPE_REQUEST = 0,
    IPC_TYPE_RESPONSE = 1,
    IPC_TYPE_NOTIFICATION = 2,
    IPC_TYPE_ERROR = 3
} ipc_msg_type_t;

/* IPC message header */
typedef struct {
    uint32_t msg_type;          /* Message type */
    uint32_t msg_id;            /* Message ID */
    uint32_t src_process;       /* Source process ID */
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

/* IPC endpoint handle */
typedef uint64_t ipc_endpoint_t;

/* Initialize IPC library */
int ipc_init(void);

/* Connect to a service endpoint */
int ipc_connect(const char *service_name, ipc_endpoint_t *endpoint);

/* Disconnect from endpoint */
int ipc_disconnect(ipc_endpoint_t endpoint);

/* Send IPC message */
int ipc_send(ipc_endpoint_t endpoint, ipc_msg_t *msg);

/* Receive IPC message (blocking) */
int ipc_recv(ipc_endpoint_t endpoint, ipc_msg_t *msg, uint64_t timeout_ms);

/* Receive IPC message (non-blocking) */
int ipc_try_recv(ipc_endpoint_t endpoint, ipc_msg_t *msg);

/* Simple RPC call */
int ipc_call(const char *service_name, void *request, void *response, size_t size);

/* Register notification handler */
int ipc_register_handler(const char *service_name, void (*handler)(ipc_msg_t *msg));

/* Unregister notification handler */
int ipc_unregister_handler(const char *service_name);

#endif /* HIK_CORE3_IPC_H */