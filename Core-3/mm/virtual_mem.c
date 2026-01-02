/*
 * HIK Core-3 Virtual Memory Management Implementation
 * 
 * Implements a heap allocator for virtual memory.
 * Applications run in virtual address spaces managed by Core-0.
 */

#include "../include/virtual_mem.h"
#include "../include/string.h"

/* Global heap structure */
static vmm_heap_t g_heap = {0};

/* Spinlock implementation */
static inline void spin_lock(uint64_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        __asm__ volatile ("pause");
    }
}

static inline void spin_unlock(uint64_t *lock) {
    __sync_lock_release(lock);
}

/* Memory block header */
typedef struct mem_block {
    uint64_t size;               /* Block size including header */
    uint8_t used;                /* 1 if used, 0 if free */
    struct mem_block *next;      /* Next block */
    struct mem_block *prev;      /* Previous block */
} __attribute__((packed)) mem_block_t;

/* Initialize virtual memory manager */
int vmm_init(uint64_t base, uint64_t size) {
    if (size < sizeof(mem_block_t) * 2) {
        return -1;  /* Not enough space */
    }

    /* Initialize heap structure */
    g_heap.base = base;
    g_heap.size = size;
    g_heap.used = 0;
    g_heap.lock = 0;

    /* Create initial free block */
    mem_block_t *initial_block = (mem_block_t*)base;
    initial_block->size = size;
    initial_block->used = 0;
    initial_block->next = NULL;
    initial_block->prev = NULL;

    return 0;
}

/* Allocate virtual memory */
void* vmm_alloc(uint64_t size) {
    return vmm_alloc_aligned(size, MALLOC_ALIGN);
}

/* Allocate aligned virtual memory */
void* vmm_alloc_aligned(uint64_t size, uint64_t alignment) {
    if (size == 0) {
        return NULL;
    }

    /* Align size to 8 bytes */
    size = (size + 7) & ~7;

    /* Add header size */
    uint64_t total_size = size + sizeof(mem_block_t);

    spin_lock(&g_heap.lock);

    mem_block_t *block = (mem_block_t*)g_heap.base;
    mem_block_t *best_block = NULL;

    /* First-fit allocation */
    while (block != NULL) {
        if (!block->used && block->size >= total_size) {
            best_block = block;
            break;
        }
        block = block->next;
    }

    if (best_block == NULL) {
        spin_unlock(&g_heap.lock);
        return NULL;  /* Out of memory */
    }

    /* Check if we need to split the block */
    if (best_block->size >= total_size + sizeof(mem_block_t) + 8) {
        /* Create new free block */
        mem_block_t *new_block = (mem_block_t*)((uint8_t*)best_block + total_size);
        new_block->size = best_block->size - total_size;
        new_block->used = 0;
        new_block->next = best_block->next;
        new_block->prev = best_block;

        if (best_block->next != NULL) {
            best_block->next->prev = new_block;
        }

        best_block->next = new_block;
        best_block->size = total_size;
    }

    /* Mark block as used */
    best_block->used = 1;
    g_heap.used += best_block->size;

    spin_unlock(&g_heap.lock);

    /* Return pointer after header */
    return (void*)((uint8_t*)best_block + sizeof(mem_block_t));
}

/* Free virtual memory */
void vmm_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    /* Get block header */
    mem_block_t *block = (mem_block_t*)((uint8_t*)ptr - sizeof(mem_block_t));

    spin_lock(&g_heap.lock);

    /* Mark block as free */
    block->used = 0;
    g_heap.used -= block->size;

    /* Merge with next block if free */
    if (block->next != NULL && !block->next->used) {
        mem_block_t *next = block->next;
        block->size += next->size;
        block->next = next->next;
        if (next->next != NULL) {
            next->next->prev = block;
        }
    }

    /* Merge with previous block if free */
    if (block->prev != NULL && !block->prev->used) {
        mem_block_t *prev = block->prev;
        prev->size += block->size;
        prev->next = block->next;
        if (block->next != NULL) {
            block->next->prev = prev;
        }
    }

    spin_unlock(&g_heap.lock);
}

/* Map memory region */
void* vmm_mmap(void *addr, uint64_t length, int prot, int flags, 
               int fd, uint64_t offset) {
    /* For now, just allocate memory */
    /* In a full implementation, this would make a syscall to Core-0 */
    return vmm_alloc(length);
}

/* Unmap memory region */
int vmm_munmap(void *addr, uint64_t length) {
    /* For now, just free memory */
    /* In a full implementation, this would make a syscall to Core-0 */
    vmm_free(addr);
    return 0;
}

/* Change memory protection */
int vmm_mprotect(void *addr, uint64_t len, int prot) {
    /* In a full implementation, this would make a syscall to Core-0 */
    return 0;
}

/* Get heap statistics */
void vmm_stats(uint64_t *total, uint64_t *used, uint64_t *free) {
    spin_lock(&g_heap.lock);

    if (total) *total = g_heap.size;
    if (used) *used = g_heap.used;
    if (free) *free = g_heap.size - g_heap.used;

    spin_unlock(&g_heap.lock);
}

/* Validate heap integrity */
int vmm_validate(void) {
    spin_lock(&g_heap.lock);

    mem_block_t *block = (mem_block_t*)g_heap.base;
    uint64_t offset = 0;

    while (block != NULL) {
        /* Check block alignment */
        if ((uint64_t)block != g_heap.base + offset) {
            spin_unlock(&g_heap.lock);
            return -1;  /* Misaligned block */
        }

        /* Check block size */
        if (block->size < sizeof(mem_block_t)) {
            spin_unlock(&g_heap.lock);
            return -2;  /* Invalid block size */
        }

        /* Check total size */
        if (offset + block->size > g_heap.size) {
            spin_unlock(&g_heap.lock);
            return -3;  /* Block exceeds heap size */
        }

        /* Check next/prev pointers */
        if (block->next != NULL && block->next->prev != block) {
            spin_unlock(&g_heap.lock);
            return -4;  /* Broken forward link */
        }

        if (block->prev != NULL && block->prev->next != block) {
            spin_unlock(&g_heap.lock);
            return -5;  /* Broken backward link */
        }

        offset += block->size;
        block = block->next;
    }

    spin_unlock(&g_heap.lock);
    return 0;
}

/* Dump heap state (for debugging) */
void vmm_dump(void) {
    uint64_t total, used, free;
    vmm_stats(&total, &used, &free);
}

/* Standard library compatibility */
void* malloc(size_t size) {
    return vmm_alloc(size);
}

void free(void *ptr) {
    vmm_free(ptr);
}

void* realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return vmm_alloc(size);
    }

    if (size == 0) {
        vmm_free(ptr);
        return NULL;
    }

    /* Get old block size */
    mem_block_t *block = (mem_block_t*)((uint8_t*)ptr - sizeof(mem_block_t));
    uint64_t old_size = block->size - sizeof(mem_block_t);

    if (size <= old_size) {
        return ptr;  /* Shrink or same size, just return */
    }

    /* Allocate new block */
    void *new_ptr = vmm_alloc(size);
    if (!new_ptr) {
        return NULL;
    }

    /* Copy data */
    memcpy(new_ptr, ptr, old_size);

    /* Free old block */
    vmm_free(ptr);

    return new_ptr;
}

void* calloc(size_t nmemb, size_t size) {
    uint64_t total = nmemb * size;
    void *ptr = vmm_alloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}