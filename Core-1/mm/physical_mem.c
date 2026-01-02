/*
 * HIK Core-1 Physical Memory Management Implementation
 * 
 * Implements a simple heap allocator for physical memory.
 * Services run with direct physical memory mapping.
 */

#include "../include/physical_mem.h"
#include "../include/string.h"

/* Global heap structure */
static heap_t g_heap = {0};

/* Spinlock implementation (simple test-and-set) */
static inline void spin_lock(uint64_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        __asm__ volatile ("pause");
    }
}

static inline void spin_unlock(uint64_t *lock) {
    __sync_lock_release(lock);
}

/* Initialize physical memory heap */
int pmm_init(uint64_t base, uint64_t size) {
    if (size < sizeof(mem_block_t) * 2) {
        return -1;  /* Not enough space */
    }

    /* Initialize heap structure */
    g_heap.base = base;
    g_heap.size = size;
    g_heap.lock = 0;

    /* Create initial free block */
    mem_block_t *initial_block = (mem_block_t*)base;
    initial_block->size = size;
    initial_block->used = 0;
    initial_block->next = NULL;
    initial_block->prev = NULL;

    g_heap.first_block = initial_block;

    return 0;
}

/* Allocate physical memory */
void* pmm_alloc(uint64_t size) {
    return pmm_alloc_aligned(size, 8);  /* Default 8-byte alignment */
}

/* Allocate aligned physical memory */
void* pmm_alloc_aligned(uint64_t size, uint64_t alignment) {
    if (size == 0) {
        return NULL;
    }

    /* Align size to 8 bytes */
    size = (size + 7) & ~7;

    /* Add header size */
    uint64_t total_size = size + sizeof(mem_block_t);

    spin_lock(&g_heap.lock);

    mem_block_t *block = g_heap.first_block;
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

    spin_unlock(&g_heap.lock);

    /* Return pointer after header */
    return (void*)((uint8_t*)best_block + sizeof(mem_block_t));
}

/* Free physical memory */
void pmm_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    /* Get block header */
    mem_block_t *block = (mem_block_t*)((uint8_t*)ptr - sizeof(mem_block_t));

    spin_lock(&g_heap.lock);

    /* Mark block as free */
    block->used = 0;

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

/* Get heap statistics */
void pmm_stats(uint64_t *total, uint64_t *used, uint64_t *free) {
    spin_lock(&g_heap.lock);

    uint64_t total_size = 0;
    uint64_t used_size = 0;
    uint64_t free_size = 0;

    mem_block_t *block = g_heap.first_block;
    while (block != NULL) {
        total_size += block->size;
        if (block->used) {
            used_size += block->size;
        } else {
            free_size += block->size;
        }
        block = block->next;
    }

    spin_unlock(&g_heap.lock);

    if (total) *total = total_size;
    if (used) *used = used_size;
    if (free) *free = free_size;
}

/* Validate heap integrity */
int pmm_validate(void) {
    spin_lock(&g_heap.lock);

    mem_block_t *block = g_heap.first_block;
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
void pmm_dump(void) {
    uint64_t total, used, free;
    pmm_stats(&total, &used, &free);

    spin_lock(&g_heap.lock);

    mem_block_t *block = g_heap.first_block;
    uint32_t block_num = 0;

    while (block != NULL) {
        block_num++;
        block = block->next;
    }

    spin_unlock(&g_heap.lock);
}