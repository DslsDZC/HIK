/*
 * HIK Core-3 Virtual Memory Management
 *
 * Core-3 applications run in virtual address spaces managed by Core-0.
 * This module provides virtual memory allocation services.
 */

#ifndef HIK_CORE3_VIRTUAL_MEM_H
#define HIK_CORE3_VIRTUAL_MEM_H

#include "stdint.h"
#include "stddef.h"

/* Page size */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

/* Memory protection flags */
#define PROT_READ    0x01
#define PROT_WRITE   0x02
#define PROT_EXEC    0x04
#define PROT_NONE    0x00

/* Mapping flags */
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20

/* Memory allocation flags */
#define MALLOC_ALIGN 16

/* Heap structure */
typedef struct {
    uint64_t base;               /* Virtual base address */
    uint64_t size;               /* Total heap size */
    uint64_t used;               /* Used bytes */
    uint64_t lock;               /* Spinlock */
} vmm_heap_t;

/* Initialize virtual memory manager */
int vmm_init(uint64_t base, uint64_t size);

/* Allocate virtual memory */
void* vmm_alloc(uint64_t size);

/* Allocate aligned virtual memory */
void* vmm_alloc_aligned(uint64_t size, uint64_t alignment);

/* Free virtual memory */
void vmm_free(void *ptr);

/* Map memory region */
void* vmm_mmap(void *addr, uint64_t length, int prot, int flags, 
               int fd, uint64_t offset);

/* Unmap memory region */
int vmm_munmap(void *addr, uint64_t length);

/* Change memory protection */
int vmm_mprotect(void *addr, uint64_t len, int prot);

/* Get heap statistics */
void vmm_stats(uint64_t *total, uint64_t *used, uint64_t *free);

/* Validate heap integrity */
int vmm_validate(void);

/* Dump heap state (for debugging) */
void vmm_dump(void);

/* Standard library compatibility */
void* malloc(size_t size);
void free(void *ptr);
void* realloc(void *ptr, size_t size);
void* calloc(size_t nmemb, size_t size);

#endif /* HIK_CORE3_VIRTUAL_MEM_H */