/*
 * HIK Core-1 Isolation Enforcement
 * 
 * Core-1 services run in isolated physical memory regions.
 * This module provides isolation verification and enforcement.
 */

#ifndef HIK_CORE1_ISOLATION_H
#define HIK_CORE1_ISOLATION_H

#include "stdint.h"
#include "stddef.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

/* Memory region types */
typedef enum {
    MEM_REGION_CODE = 0,        /* Code region */
    MEM_REGION_DATA = 1,        /* Data region */
    MEM_REGION_STACK = 2,       /* Stack region */
    MEM_REGION_HEAP = 3,        /* Heap region */
    MEM_REGION_SHARED = 4,      /* Shared memory region */
    MEM_REGION_DEVICE = 5       /* Device MMIO region */
} mem_region_type_t;

/* Memory region descriptor */
typedef struct {
    mem_region_type_t type;     /* Region type */
    uint64_t phys_base;         /* Physical base address */
    uint64_t virt_base;         /* Virtual base address (identity mapped) */
    uint64_t size;              /* Region size */
    uint32_t permissions;       /* Access permissions */
    uint64_t cap_handle;        /* Capability handle */
} __attribute__((packed)) mem_region_t;

/* Access permissions */
#define MEM_PERM_READ    0x01
#define MEM_PERM_WRITE   0x02
#define MEM_PERM_EXECUTE 0x04
#define MEM_PERM_DEVICE  0x08

/* Maximum memory regions per service */
#define MAX_MEM_REGIONS 16

/* Isolation context */
typedef struct {
    uint64_t service_id;         /* Service ID */
    uint64_t domain_id;          /* Domain ID */
    mem_region_t regions[MAX_MEM_REGIONS]; /* Memory regions */
    uint32_t num_regions;        /* Number of regions */
    uint64_t lock;               /* Spinlock */
    uint32_t enabled;            /* Isolation enabled flag */
} isolation_context_t;

/* Initialize isolation */
int isolation_init(uint64_t service_id, uint64_t domain_id);

/* Add memory region */
int isolation_add_region(mem_region_type_t type, uint64_t phys_base, 
                        uint64_t size, uint32_t permissions, uint64_t cap_handle);

/* Remove memory region */
int isolation_remove_region(uint64_t phys_base);

/* Verify memory access */
int isolation_verify_access(uint64_t addr, uint64_t size, uint32_t required_perms);

/* Check if address is in service's memory space */
int isolation_is_service_memory(uint64_t addr);

/* Get memory region containing address */
mem_region_t* isolation_get_region(uint64_t addr);

/* Enable isolation enforcement */
int isolation_enable(void);

/* Disable isolation enforcement (for debugging) */
int isolation_disable(void);

/* Validate all memory regions */
int isolation_validate(void);

/* Dump isolation state (for debugging) */
void isolation_dump(void);

/* Memory access wrapper functions */
uint8_t isolation_read8(uint64_t addr);
uint16_t isolation_read16(uint64_t addr);
uint32_t isolation_read32(uint64_t addr);
uint64_t isolation_read64(uint64_t addr);

void isolation_write8(uint64_t addr, uint8_t value);
void isolation_write16(uint64_t addr, uint16_t value);
void isolation_write32(uint64_t addr, uint32_t value);
void isolation_write64(uint64_t addr, uint64_t value);

/* Memory copy with isolation check */
int isolation_memcpy(void *dst, const void *src, uint64_t size);

/* Memory set with isolation check */
int isolation_memset(void *dst, uint8_t value, uint64_t size);

/* Memory compare with isolation check */
int isolation_memcmp(const void *ptr1, const void *ptr2, uint64_t size);

#endif /* HIK_CORE1_ISOLATION_H */