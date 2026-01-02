/*
 * HIK Core-1 Isolation Implementation
 * 
 * Enforces memory isolation for Core-1 services running in
 * physical memory regions.
 */

#include "../include/isolation.h"
#include "../include/string.h"

/* Global isolation context */
static isolation_context_t g_isolation = {0};

/* Spinlock implementation */
static inline void spin_lock(uint64_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        __asm__ volatile ("pause");
    }
}

static inline void spin_unlock(uint64_t *lock) {
    __sync_lock_release(lock);
}

/* Initialize isolation */
int isolation_init(uint64_t service_id, uint64_t domain_id) {
    g_isolation.service_id = service_id;
    g_isolation.domain_id = domain_id;
    g_isolation.num_regions = 0;
    g_isolation.lock = 0;
    g_isolation.enabled = 0;

    /* Clear regions */
    for (int i = 0; i < MAX_MEM_REGIONS; i++) {
        g_isolation.regions[i].type = 0;
        g_isolation.regions[i].phys_base = 0;
        g_isolation.regions[i].virt_base = 0;
        g_isolation.regions[i].size = 0;
        g_isolation.regions[i].permissions = 0;
        g_isolation.regions[i].cap_handle = 0;
    }

    return 0;
}

/* Add memory region */
int isolation_add_region(mem_region_type_t type, uint64_t phys_base,
                        uint64_t size, uint32_t permissions, uint64_t cap_handle) {
    if (g_isolation.num_regions >= MAX_MEM_REGIONS) {
        return -1;  /* Too many regions */
    }

    spin_lock(&g_isolation.lock);

    /* Check for overlap with existing regions */
    for (uint32_t i = 0; i < g_isolation.num_regions; i++) {
        mem_region_t *region = &g_isolation.regions[i];
        if (phys_base < region->phys_base + region->size &&
            phys_base + size > region->phys_base) {
            spin_unlock(&g_isolation.lock);
            return -2;  /* Overlap detected */
        }
    }

    /* Add new region */
    mem_region_t *region = &g_isolation.regions[g_isolation.num_regions];
    region->type = type;
    region->phys_base = phys_base;
    region->virt_base = phys_base;  /* Identity mapping */
    region->size = size;
    region->permissions = permissions;
    region->cap_handle = cap_handle;

    g_isolation.num_regions++;

    spin_unlock(&g_isolation.lock);
    return 0;
}

/* Remove memory region */
int isolation_remove_region(uint64_t phys_base) {
    spin_lock(&g_isolation.lock);

    for (uint32_t i = 0; i < g_isolation.num_regions; i++) {
        if (g_isolation.regions[i].phys_base == phys_base) {
            /* Shift remaining regions */
            for (uint32_t j = i; j < g_isolation.num_regions - 1; j++) {
                g_isolation.regions[j] = g_isolation.regions[j + 1];
            }
            g_isolation.num_regions--;
            spin_unlock(&g_isolation.lock);
            return 0;
        }
    }

    spin_unlock(&g_isolation.lock);
    return -1;  /* Not found */
}

/* Verify memory access */
int isolation_verify_access(uint64_t addr, uint64_t size, uint32_t required_perms) {
    if (!g_isolation.enabled) {
        return 1;  /* Isolation disabled, allow access */
    }

    spin_lock(&g_isolation.lock);

    for (uint32_t i = 0; i < g_isolation.num_regions; i++) {
        mem_region_t *region = &g_isolation.regions[i];

        /* Check if address is within region */
        if (addr >= region->virt_base &&
            addr + size <= region->virt_base + region->size) {

            /* Check permissions */
            if ((region->permissions & required_perms) == required_perms) {
                spin_unlock(&g_isolation.lock);
                return 1;  /* Access granted */
            }
            spin_unlock(&g_isolation.lock);
            return 0;  /* Access denied */
        }
    }

    spin_unlock(&g_isolation.lock);
    return 0;  /* Address not in any region */
}

/* Check if address is in service's memory space */
int isolation_is_service_memory(uint64_t addr) {
    spin_lock(&g_isolation.lock);

    for (uint32_t i = 0; i < g_isolation.num_regions; i++) {
        mem_region_t *region = &g_isolation.regions[i];
        if (addr >= region->virt_base &&
            addr < region->virt_base + region->size) {
            spin_unlock(&g_isolation.lock);
            return 1;
        }
    }

    spin_unlock(&g_isolation.lock);
    return 0;
}

/* Get memory region containing address */
mem_region_t* isolation_get_region(uint64_t addr) {
    spin_lock(&g_isolation.lock);

    for (uint32_t i = 0; i < g_isolation.num_regions; i++) {
        mem_region_t *region = &g_isolation.regions[i];
        if (addr >= region->virt_base &&
            addr < region->virt_base + region->size) {
            spin_unlock(&g_isolation.lock);
            return region;
        }
    }

    spin_unlock(&g_isolation.lock);
    return NULL;
}

/* Enable isolation enforcement */
int isolation_enable(void) {
    g_isolation.enabled = 1;
    return 0;
}

/* Disable isolation enforcement (for debugging) */
int isolation_disable(void) {
    g_isolation.enabled = 0;
    return 0;
}

/* Validate all memory regions */
int isolation_validate(void) {
    spin_lock(&g_isolation.lock);

    for (uint32_t i = 0; i < g_isolation.num_regions; i++) {
        mem_region_t *region = &g_isolation.regions[i];

        /* Check for valid size */
        if (region->size == 0) {
            spin_unlock(&g_isolation.lock);
            return -1;
        }

        /* Check for overlap */
        for (uint32_t j = i + 1; j < g_isolation.num_regions; j++) {
            mem_region_t *other = &g_isolation.regions[j];
            if (region->phys_base < other->phys_base + other->size &&
                region->phys_base + region->size > other->phys_base) {
                spin_unlock(&g_isolation.lock);
                return -2;  /* Overlap detected */
            }
        }
    }

    spin_unlock(&g_isolation.lock);
    return 0;
}

/* Dump isolation state (for debugging) */
void isolation_dump(void) {
    spin_lock(&g_isolation.lock);
    spin_unlock(&g_isolation.lock);
}

/* Memory access wrapper functions */
uint8_t isolation_read8(uint64_t addr) {
    if (!isolation_verify_access(addr, 1, MEM_PERM_READ)) {
        return 0;
    }
    return *(volatile uint8_t*)addr;
}

uint16_t isolation_read16(uint64_t addr) {
    if (!isolation_verify_access(addr, 2, MEM_PERM_READ)) {
        return 0;
    }
    return *(volatile uint16_t*)addr;
}

uint32_t isolation_read32(uint64_t addr) {
    if (!isolation_verify_access(addr, 4, MEM_PERM_READ)) {
        return 0;
    }
    return *(volatile uint32_t*)addr;
}

uint64_t isolation_read64(uint64_t addr) {
    if (!isolation_verify_access(addr, 8, MEM_PERM_READ)) {
        return 0;
    }
    return *(volatile uint64_t*)addr;
}

void isolation_write8(uint64_t addr, uint8_t value) {
    if (!isolation_verify_access(addr, 1, MEM_PERM_WRITE)) {
        return;
    }
    *(volatile uint8_t*)addr = value;
}

void isolation_write16(uint64_t addr, uint16_t value) {
    if (!isolation_verify_access(addr, 2, MEM_PERM_WRITE)) {
        return;
    }
    *(volatile uint16_t*)addr = value;
}

void isolation_write32(uint64_t addr, uint32_t value) {
    if (!isolation_verify_access(addr, 4, MEM_PERM_WRITE)) {
        return;
    }
    *(volatile uint32_t*)addr = value;
}

void isolation_write64(uint64_t addr, uint64_t value) {
    if (!isolation_verify_access(addr, 8, MEM_PERM_WRITE)) {
        return;
    }
    *(volatile uint64_t*)addr = value;
}

/* Memory copy with isolation check */
int isolation_memcpy(void *dst, const void *src, uint64_t size) {
    if (!dst || !src || size == 0) {
        return -1;
    }

    uint64_t dst_addr = (uint64_t)dst;
    uint64_t src_addr = (uint64_t)src;

    /* Verify source access */
    if (!isolation_verify_access(src_addr, size, MEM_PERM_READ)) {
        return -2;
    }

    /* Verify destination access */
    if (!isolation_verify_access(dst_addr, size, MEM_PERM_WRITE)) {
        return -3;
    }

    /* Perform copy */
    memcpy(dst, src, size);
    return 0;
}

/* Memory set with isolation check */
int isolation_memset(void *dst, uint8_t value, uint64_t size) {
    if (!dst || size == 0) {
        return -1;
    }

    uint64_t dst_addr = (uint64_t)dst;

    /* Verify access */
    if (!isolation_verify_access(dst_addr, size, MEM_PERM_WRITE)) {
        return -2;
    }

    /* Perform set */
    memset(dst, value, size);
    return 0;
}

/* Memory compare with isolation check */
int isolation_memcmp(const void *ptr1, const void *ptr2, uint64_t size) {
    if (!ptr1 || !ptr2 || size == 0) {
        return -1;
    }

    uint64_t addr1 = (uint64_t)ptr1;
    uint64_t addr2 = (uint64_t)ptr2;

    /* Verify access */
    if (!isolation_verify_access(addr1, size, MEM_PERM_READ)) {
        return -2;
    }

    if (!isolation_verify_access(addr2, size, MEM_PERM_READ)) {
        return -3;
    }

    /* Perform compare */
    return memcmp(ptr1, ptr2, size);
}