/*
 * HIK Core-1 Physical Memory Management
 * 
 * Core-1 services run with direct physical memory mapping.
 * This module provides physical memory allocation services.
 */

#ifndef HIK_CORE1_PHYSICAL_MEM_H
#define HIK_CORE1_PHYSICAL_MEM_H

#include "stdint.h"
#include "stddef.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

/* Page size */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

/* Alignment helpers */
#define ALIGN_UP(addr, align)    (((addr) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(addr, align)  ((addr) & ~((align) - 1))
#define IS_ALIGNED(addr, align)  (((addr) & ((align) - 1)) == 0)

/* Memory block header */
typedef struct mem_block {
    uint64_t size;               /* Block size including header */
    uint8_t used;                /* 1 if used, 0 if free */
    struct mem_block *next;      /* Next block */
    struct mem_block *prev;      /* Previous block */
} __attribute__((packed)) mem_block_t;

/* Heap structure */
typedef struct {
    uint64_t base;               /* Physical base address */
    uint64_t size;               /* Total heap size */
    mem_block_t *first_block;    /* First block in heap */
    uint64_t lock;               /* Spinlock */
} heap_t;

/* Initialize physical memory heap */
int pmm_init(uint64_t base, uint64_t size);

/* Allocate physical memory */
void* pmm_alloc(uint64_t size);

/* Allocate aligned physical memory */
void* pmm_alloc_aligned(uint64_t size, uint64_t alignment);

/* Free physical memory */
void pmm_free(void *ptr);

/* Get heap statistics */
void pmm_stats(uint64_t *total, uint64_t *used, uint64_t *free);

/* Validate heap integrity */
int pmm_validate(void);

/* Dump heap state (for debugging) */
void pmm_dump(void);

#endif /* HIK_CORE1_PHYSICAL_MEM_H */