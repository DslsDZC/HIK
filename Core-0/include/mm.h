/*
 * HIK Core-0 Memory Management
 * 
 * This file defines the physical memory management system for Core-0.
 * It manages physical memory frames and provides allocation services.
 */

#ifndef HIK_CORE0_MM_H
#define HIK_CORE0_MM_H

#include <stdint.h>

/* Page size */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

/* Maximum physical memory (256GB) */
#define MAX_PHYSICAL_MEMORY (256ULL * 1024 * 1024 * 1024)

/* Maximum number of pages */
#define MAX_PAGES (MAX_PHYSICAL_MEMORY / PAGE_SIZE)

/* Memory region types */
typedef enum {
    MEM_TYPE_RESERVED = 0,    /* Reserved (BIOS, ACPI, etc.) */
    MEM_TYPE_AVAILABLE = 1,   /* Available for allocation */
    MEM_TYPE_KERNEL = 2,      /* Kernel code/data */
    MEM_TYPE_SERVICE = 3,     /* Core-1 service */
    MEM_TYPE_APPLICATION = 4, /* Application */
    MEM_TYPE_DEVICE = 5,      /* Device MMIO */
    MEM_TYPE_CUSTOM = 99      /* Custom type */
} mem_type_t;

/* Memory frame descriptor (global bitmap entry) */
typedef struct {
    mem_type_t type;          /* Memory type */
    uint64_t owner;           /* Owner domain ID */
} mem_frame_t;

/* Memory manager state */
typedef struct {
    mem_frame_t frames[MAX_PAGES];     /* Global frame bitmap */
    uint64_t total_pages;            /* Total number of pages */
    uint64_t available_pages;        /* Available pages */
    uint64_t allocated_pages;        /* Allocated pages */
    uint64_t lock;                    /* Spinlock */
} mm_state_t;

/* Initialize memory manager */
int mm_init(uint64_t total_memory);

/* Allocate memory */
uint64_t mm_alloc(uint64_t size, uint64_t align, mem_type_t type, uint64_t owner);

/* Free memory */
int mm_free(uint64_t addr);

/* Reserve memory region */
int mm_reserve(uint64_t base, uint64_t size, mem_type_t type, uint64_t owner);

/* Get memory type */
mem_type_t mm_get_type(uint64_t addr);

/* Get memory frame */
mem_frame_t* mm_get_frame(uint64_t addr);

/* Dump memory map (for debugging) */
void mm_dump(void);

/* Get available pages */
uint64_t mm_get_available(void);

/* Get allocated pages */
uint64_t mm_get_allocated(void);

#endif /* HIK_CORE0_MM_H */