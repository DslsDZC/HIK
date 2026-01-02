/*
 * HIK Core-1 IPC Implementation
 * 
 * Implements inter-process communication for Core-1 services.
 * Supports message passing and shared memory channels.
 */

#include "../include/ipc.h"
#include "../include/string.h"
#include "../include/physical_mem.h"
#include "../include/core1.h"

/* Global endpoint table */
static struct ipc_endpoint *g_endpoints = NULL;
static uint32_t g_num_endpoints = 0;
static uint64_t g_next_endpoint_id = 1;
static uint64_t g_next_msg_id = 1;
static uint64_t g_ipc_lock = 0;

/* Spinlock implementation */
static inline void spin_lock(uint64_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        __asm__ volatile ("pause");
    }
}

static inline void spin_unlock(uint64_t *lock) {
    __sync_lock_release(lock);
}

/* Initialize IPC subsystem */
int ipc_init(void) {
    g_endpoints = NULL;
    g_num_endpoints = 0;
    g_next_endpoint_id = 1;
    g_next_msg_id = 1;
    g_ipc_lock = 0;
    return 0;
}

/* Register an endpoint */
int ipc_register_endpoint(const char *name, ipc_endpoint_type_t type,
                         void (*handler)(ipc_msg_t *msg)) {
    if (!name || !handler) {
        return -1;
    }

    spin_lock(&g_ipc_lock);

    /* Check if endpoint already exists */
    struct ipc_endpoint *ep = g_endpoints;
    while (ep != NULL) {
        if (strcmp(ep->name, name) == 0) {
            spin_unlock(&g_ipc_lock);
            return -2;  /* Already exists */
        }
        ep = ep->next;
    }

    /* Allocate new endpoint */
    struct ipc_endpoint *new_ep = (struct ipc_endpoint*)pmm_alloc(sizeof(struct ipc_endpoint));
    if (!new_ep) {
        spin_unlock(&g_ipc_lock);
        return -3;  /* Out of memory */
    }

    /* Initialize endpoint */
    new_ep->endpoint_id = g_next_endpoint_id++;
    strncpy(new_ep->name, name, sizeof(new_ep->name) - 1);
    new_ep->name[sizeof(new_ep->name) - 1] = '\0';
    new_ep->type = type;
    new_ep->service_id = g_service_info->service_id;
    new_ep->handler = handler;
    new_ep->next = g_endpoints;

    g_endpoints = new_ep;
    g_num_endpoints++;

    spin_unlock(&g_ipc_lock);

    return new_ep->endpoint_id;
}

/* Unregister an endpoint */
int ipc_unregister_endpoint(const char *name) {
    if (!name) {
        return -1;
    }

    spin_lock(&g_ipc_lock);

    struct ipc_endpoint *prev = NULL;
    struct ipc_endpoint *ep = g_endpoints;

    while (ep != NULL) {
        if (strcmp(ep->name, name) == 0) {
            if (prev) {
                prev->next = ep->next;
            } else {
                g_endpoints = ep->next;
            }
            pmm_free(ep);
            g_num_endpoints--;
            spin_unlock(&g_ipc_lock);
            return 0;
        }
        prev = ep;
        ep = ep->next;
    }

    spin_unlock(&g_ipc_lock);
    return -2;  /* Not found */
}

/* Find endpoint by name */
struct ipc_endpoint* ipc_find_endpoint(const char *name) {
    if (!name) {
        return NULL;
    }

    spin_lock(&g_ipc_lock);

    struct ipc_endpoint *ep = g_endpoints;
    while (ep != NULL) {
        if (strcmp(ep->name, name) == 0) {
            spin_unlock(&g_ipc_lock);
            return ep;
        }
        ep = ep->next;
    }

    spin_unlock(&g_ipc_lock);
    return NULL;
}

/* Send IPC message */
int ipc_send(uint64_t endpoint_id, ipc_msg_t *msg) {
    if (!msg) {
        return -1;
    }

    spin_lock(&g_ipc_lock);

    /* Find endpoint */
    struct ipc_endpoint *ep = g_endpoints;
    while (ep != NULL) {
        if (ep->endpoint_id == endpoint_id) {
            break;
        }
        ep = ep->next;
    }

    if (!ep) {
        spin_unlock(&g_ipc_lock);
        return -2;  /* Endpoint not found */
    }

    /* Setup message header */
    msg->header.msg_id = g_next_msg_id++;
    msg->header.src_service = g_service_info->service_id;
    msg->header.dst_service = ep->service_id;
    msg->header.timestamp = 0;  /* Would get from system timer */

    /* Call handler */
    if (ep->handler) {
        ep->handler(msg);
    }

    spin_unlock(&g_ipc_lock);
    return 0;
}

/* Receive IPC message (blocking) */
int ipc_recv(ipc_msg_t *msg, uint64_t timeout_ms) {
    /* For now, return not implemented */
    /* In a full implementation, this would block on a message queue */
    return -1;
}

/* Receive IPC message (non-blocking) */
int ipc_try_recv(ipc_msg_t *msg) {
    /* For now, return not implemented */
    /* In a full implementation, this would check a message queue */
    return -1;
}

/* Create shared memory channel */
int ipc_create_channel(uint64_t size, ipc_channel_t **channel) {
    if (!channel || size == 0) {
        return -1;
    }

    /* Allocate channel structure */
    ipc_channel_t *ch = (ipc_channel_t*)(uint64_t)pmm_alloc(sizeof(ipc_channel_t));
    if (!ch) {
        return -2;
    }

    /* Allocate shared memory buffer */
    /* In a real implementation, this would request memory from Core-0 */
    ch->phys_addr = 0;  /* Would be set by Core-0 */
    ch->size = size;
    ch->cap_handle = 0;
    ch->read_ptr = 0;
    ch->write_ptr = 0;
    ch->lock = 0;

    *channel = ch;
    return 0;
}

/* Destroy shared memory channel */
int ipc_destroy_channel(ipc_channel_t *channel) {
    if (!channel) {
        return -1;
    }

    pmm_free(channel);
    return 0;
}

/* Write to ring buffer (zero-copy) */
int ipc_ring_write(ring_buffer_t *ring, const void *data, uint64_t size) {
    if (!ring || !data || size == 0) {
        return -1;
    }

    uint64_t available = ring->capacity - (ring->head - ring->tail);
    if (size > available) {
        return -2;  /* Not enough space */
    }

    uint64_t pos = ring->head & ring->mask;
    uint64_t first_part = ring->capacity - pos;

    if (size <= first_part) {
        /* Single write */
        memcpy(&ring->data[pos], data, size);
    } else {
        /* Two-part write */
        memcpy(&ring->data[pos], data, first_part);
        memcpy(&ring->data[0], (uint8_t*)data + first_part, size - first_part);
    }

    ipc_write_barrier();
    ring->head += size;

    return 0;
}

/* Read from ring buffer (zero-copy) */
int ipc_ring_read(ring_buffer_t *ring, void *data, uint64_t size) {
    if (!ring || !data || size == 0) {
        return -1;
    }

    uint64_t used = ring->head - ring->tail;
    if (size > used) {
        return -2;  /* Not enough data */
    }

    uint64_t pos = ring->tail & ring->mask;
    uint64_t first_part = ring->capacity - pos;

    if (size <= first_part) {
        /* Single read */
        memcpy(data, &ring->data[pos], size);
    } else {
        /* Two-part read */
        memcpy(data, &ring->data[pos], first_part);
        memcpy((uint8_t*)data + first_part, &ring->data[0], size - first_part);
    }

    ipc_read_barrier();
    ring->tail += size;

    return 0;
}

/* Get available space in ring buffer */
uint64_t ipc_ring_available(ring_buffer_t *ring) {
    if (!ring) {
        return 0;
    }
    return ring->capacity - (ring->head - ring->tail);
}

/* Get used space in ring buffer */
uint64_t ipc_ring_used(ring_buffer_t *ring) {
    if (!ring) {
        return 0;
    }
    return ring->head - ring->tail;
}

/* Memory barrier for IPC synchronization */
void ipc_memory_barrier(void) {
    __sync_synchronize();
}

/* Read memory barrier */
void ipc_read_barrier(void) {
    __sync_synchronize();
}

/* Write memory barrier */
void ipc_write_barrier(void) {
    __sync_synchronize();
}