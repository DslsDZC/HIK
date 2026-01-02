/*
 * HIK Core-0 Capability System
 * 
 * This file defines the capability system for resource access control.
 * Capabilities are unforgeable tokens that grant specific permissions
 * to access system resources.
 */

#ifndef HIK_CORE0_CAPABILITY_H
#define HIK_CORE0_CAPABILITY_H

#include "stdint.h"

/* Capability types */
typedef enum {
    CAP_TYPE_MEMORY = 1,        /* Memory range access */
    CAP_TYPE_IO_PORT = 2,       /* I/O port access */
    CAP_TYPE_IRQ = 3,           /* Interrupt request line */
    CAP_TYPE_IPC_ENDPOINT = 4,  /* IPC communication endpoint */
    CAP_TYPE_SERVICE = 5,       /* Service control */
    CAP_TYPE_DEVICE = 6,        /* Device MMIO access */
    CAP_TYPE_CUSTOM = 99        /* Custom capability type */
} cap_type_t;

/* Capability permissions */
#define CAP_PERM_READ    0x01
#define CAP_PERM_WRITE   0x02
#define CAP_PERM_EXECUTE 0x04
#define CAP_PERM_GRANT   0x08
#define CAP_PERM_REVOKE  0x10

/* Capability handle */
typedef uint32_t cap_handle_t;

/* Maximum number of capabilities */
#define MAX_CAPABILITIES 1024
#define MAX_DOMAINS 256

/* Domain capability space size */
#define DOMAIN_CAP_SPACE_SIZE 64

/* Capability structure */
typedef struct {
    uint64_t magic;              /* HIK_CAP_MAGIC (0x43415000) */
    cap_type_t type;            /* Capability type */
    uint32_t permissions;       /* Permission flags */
    uint64_t resource_id;       /* Resource identifier */
    uint64_t resource_base;     /* Base address/number */
    uint64_t resource_size;     /* Size/count */
    uint64_t owner_domain;      /* Owning domain ID */
    uint32_t ref_count;         /* Reference count */
    uint32_t flags;             /* Capability flags */
} __attribute__((packed)) capability_t;

#define HIK_CAP_MAGIC 0x43415000  /* "CAP\0" */

/* Domain structure */
typedef struct {
    uint64_t domain_id;         /* Domain ID */
    uint64_t memory_base;       /* Physical memory base */
    uint64_t memory_size;       /* Memory size */
    cap_handle_t cap_space[DOMAIN_CAP_SPACE_SIZE]; /* Capability handles */
    uint32_t num_caps;          /* Number of capabilities */
    uint32_t state;             /* Domain state */
} domain_t;

/* Domain states */
#define DOMAIN_STATE_STOPPED    0
#define DOMAIN_STATE_STARTING   1
#define DOMAIN_STATE_RUNNING    2
#define DOMAIN_STATE_STOPPING   3
#define DOMAIN_STATE_ERROR      4

/* Capability system state */
typedef struct {
    capability_t capabilities[MAX_CAPABILITIES]; /* Global capability table */
    domain_t domains[MAX_DOMAINS];              /* Domain table */
    uint32_t next_cap_handle;                    /* Next capability handle */
    uint32_t next_domain_id;                     /* Next domain ID */
    uint32_t num_caps;                          /* Number of capabilities */
    uint32_t num_domains;                       /* Number of domains */
    uint64_t lock;                              /* Spinlock for synchronization */
} cap_system_t;

/* Initialize capability system */
int cap_init(void);

/* Create a new capability */
cap_handle_t cap_create(cap_type_t type, uint32_t permissions,
                       uint64_t resource_id, uint64_t base, uint64_t size,
                       uint64_t domain_id);

/* Delete a capability */
int cap_delete(cap_handle_t handle);

/* Grant a capability to a domain */
cap_handle_t cap_grant(cap_handle_t handle, uint64_t target_domain_id);

/* Revoke a capability from a domain */
int cap_revoke(cap_handle_t handle, uint64_t domain_id);

/* Check if a domain has a capability */
int cap_check(uint64_t domain_id, cap_handle_t handle, uint32_t permission);

/* Create a new domain */
uint64_t cap_create_domain(uint64_t memory_base, uint64_t memory_size);

/* Delete a domain */
int cap_delete_domain(uint64_t domain_id);

/* Get domain by ID */
domain_t* cap_get_domain(uint64_t domain_id);

/* Add capability to domain's capability space */
int cap_domain_add_cap(uint64_t domain_id, cap_handle_t handle);

/* Remove capability from domain's capability space */
int cap_domain_remove_cap(uint64_t domain_id, cap_handle_t handle);

/* Get capability by handle */
capability_t* cap_get_capability(cap_handle_t handle);

/* Derive a new capability with restricted permissions */
cap_handle_t cap_derive(cap_handle_t handle, uint32_t new_permissions);

/* Dump capability table (for debugging) */
void cap_dump(void);

/* Dump domain information (for debugging) */
void cap_dump_domain(uint64_t domain_id);

#endif /* HIK_CORE0_CAPABILITY_H */