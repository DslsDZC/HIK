/*
 * HIK Core-0 Memory Management Implementation
 * 
 * This file implements physical memory management using global bitmaps
 * as specified in the documentation.
 */

#include "../include/mm.h"
#include "../include/string.h"

/* Global memory manager state */
static mm_state_t g_mm_state;

/* Spinlock operations */
static inline void spin_lock(uint64_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        /* Spin */
    }
}

static inline void spin_unlock(uint64_t *lock) {
    __sync_lock_release(lock);
}

/*
 * Initialize memory manager
 */
int mm_init(uint64_t total_memory) {
    memset(&g_mm_state, 0, sizeof(mm_state_t));
    
    g_mm_state.total_pages = total_memory / PAGE_SIZE;
    g_mm_state.available_pages = g_mm_state.total_pages;
    g_mm_state.allocated_pages = 0;
    g_mm_state.lock = 0;
    
    /* Mark all frames as reserved initially */
    for (uint64_t i = 0; i < g_mm_state.total_pages; i++) {
        g_mm_state.frames[i].type = MEM_TYPE_RESERVED;
        g_mm_state.frames[i].owner = 0;
    }
    
    return 0;
}

/*
 * Allocate memory using global bitmap
 */
uint64_t mm_alloc(uint64_t size, uint64_t align, mem_type_t type, uint64_t owner) {
    spin_lock(&g_mm_state.lock);
    
    /* Calculate number of pages needed */
    uint64_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t align_pages = (align + PAGE_SIZE - 1) / PAGE_SIZE;
    
    /* Find contiguous available pages */
    uint64_t start_page = 0;
    uint64_t found_pages = 0;
    
    for (uint64_t i = 0; i < g_mm_state.total_pages; i++) {
        if (g_mm_state.frames[i].type == MEM_TYPE_AVAILABLE) {
            if (found_pages == 0) {
                start_page = i;
            }
            found_pages++;
            
            if (found_pages >= pages_needed) {
                /* Check alignment */
                if ((start_page % align_pages) == 0) {
                    /* Allocate pages */
                    for (uint64_t j = start_page; j < start_page + pages_needed; j++) {
                        g_mm_state.frames[j].type = type;
                        g_mm_state.frames[j].owner = owner;
                    }
                    
                    g_mm_state.available_pages -= pages_needed;
                    g_mm_state.allocated_pages += pages_needed;
                    
                    spin_unlock(&g_mm_state.lock);
                    
                    return start_page * PAGE_SIZE;
                } else {
                    /* Not aligned, reset search */
                    found_pages = 0;
                }
            }
        } else {
            found_pages = 0;
        }
    }
    
    spin_unlock(&g_mm_state.lock);
    
    return 0;  /* Allocation failed */
}

/*
 * Free memory
 */
int mm_free(uint64_t addr) {
    spin_lock(&g_mm_state.lock);
    
    uint64_t page = addr / PAGE_SIZE;
    
    if (page >= g_mm_state.total_pages) {
        spin_unlock(&g_mm_state.lock);
        return -1;
    }
    
    /* Mark frames as available */
    g_mm_state.frames[page].type = MEM_TYPE_AVAILABLE;
    g_mm_state.frames[page].owner = 0;
    
    g_mm_state.available_pages++;
    g_mm_state.allocated_pages--;
    
    spin_unlock(&g_mm_state.lock);
    
    return 0;
}

/*
 * Reserve memory region
 */
int mm_reserve(uint64_t base, uint64_t size, mem_type_t type, uint64_t owner) {
    spin_lock(&g_mm_state.lock);
    
    uint64_t start_page = base / PAGE_SIZE;
    uint64_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint64_t i = start_page; i < start_page + num_pages; i++) {
        if (i >= g_mm_state.total_pages) {
            spin_unlock(&g_mm_state.lock);
            return -1;
        }
        
        g_mm_state.frames[i].type = type;
        g_mm_state.frames[i].owner = owner;
    }
    
    if (type == MEM_TYPE_AVAILABLE) {
        g_mm_state.available_pages += num_pages;
    } else {
        g_mm_state.available_pages -= num_pages;
        g_mm_state.allocated_pages += num_pages;
    }
    
    spin_unlock(&g_mm_state.lock);
    
    return 0;
}

/*
 * Get memory type
 */
mem_type_t mm_get_type(uint64_t addr) {
    uint64_t page = addr / PAGE_SIZE;
    
    if (page >= g_mm_state.total_pages) {
        return MEM_TYPE_RESERVED;
    }
    
    return g_mm_state.frames[page].type;
}

/*
 * Get memory frame
 */
mem_frame_t* mm_get_frame(uint64_t addr) {
    uint64_t page = addr / PAGE_SIZE;
    
    if (page >= g_mm_state.total_pages) {
        return NULL;
    }
    
    return &g_mm_state.frames[page];
}

/*
 * Dump memory map (for debugging)
 */
void mm_dump(void) {
    /* Would print to console in real implementation */
    /* Simplified for now */
}

/*
 * Get available pages
 */
uint64_t mm_get_available(void) {
    return g_mm_state.available_pages * PAGE_SIZE;
}

/*
 * Get allocated pages
 */
uint64_t mm_get_allocated(void) {
    return g_mm_state.allocated_pages * PAGE_SIZE;
}