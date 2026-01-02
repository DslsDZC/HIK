/*
 * HIK Core-0 Isolation Enforcement Implementation
 * 
 * This file implements page table configuration and call gates
 * for service isolation as specified in the documentation.
 */

#include "../include/isolation.h"
#include "../include/capability.h"
#include "../include/mm.h"
#include <string.h>

/* Maximum number of domains */
#define MAX_DOMAINS 256

/* Global domain page table array */
static domain_page_table_t g_domain_tables[MAX_DOMAINS];

/* Global call gate table */
static call_gate_table_t g_call_gate_table;

/* Spinlock operations */
static inline void spin_lock(uint32_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        /* Spin */
    }
}

static inline void spin_unlock(uint32_t *lock) {
    __sync_lock_release(lock);
}

/*
 * Initialize isolation system
 */
int isolation_init(void) {
    /* Initialize domain page tables */
    memset(g_domain_tables, 0, sizeof(g_domain_tables));
    
    /* Initialize call gate table */
    memset(&g_call_gate_table, 0, sizeof(call_gate_table_t));
    g_call_gate_table.num_gates = 0;
    g_call_gate_table.lock = 0;
    
    return 0;
}

/*
 * Create page tables for a domain
 */
int isolation_create_page_tables(uint64_t domain_id, uint32_t flags) {
    if (domain_id >= MAX_DOMAINS) {
        return -1;
    }
    
    /* Allocate PML4 table */
    uint64_t pml4_phys = mm_alloc(PAGE_SIZE, PAGE_SIZE, MEM_TYPE_KERNEL, domain_id);
    if (pml4_phys == 0) {
        return -1;
    }
    
    /* Map PML4 to virtual address */
    page_table_t *pml4 = (page_table_t *)pml4_phys;
    memset(pml4, 0, sizeof(page_table_t));
    
    /* Initialize domain page table structure */
    g_domain_tables[domain_id].pml4 = pml4;
    g_domain_tables[domain_id].domain_id = domain_id;
    g_domain_tables[domain_id].capabilities = 0;
    g_domain_tables[domain_id].flags = flags;
    
    return 0;
}

/*
 * Destroy page tables for a domain
 */
int isolation_destroy_page_tables(uint64_t domain_id) {
    if (domain_id >= MAX_DOMAINS) {
        return -1;
    }
    
    domain_page_table_t *domain = &g_domain_tables[domain_id];
    
    if (domain->pml4 == NULL) {
        return -1;
    }
    
    /* Free PML4 table */
    mm_free((uint64_t)domain->pml4);
    
    /* Clear domain entry */
    memset(domain, 0, sizeof(domain_page_table_t));
    
    return 0;
}

/*
 * Map memory into domain's address space
 */
int isolation_map_memory(uint64_t domain_id, uint64_t virt_addr, uint64_t phys_addr,
                         uint64_t size, map_type_t map_type, uint64_t cap_id) {
    if (domain_id >= MAX_DOMAINS) {
        return -1;
    }
    
    /* Verify capability */
    if (cap_check(cap_id, CAP_TYPE_MEMORY, CAP_PERM_READ | CAP_PERM_WRITE) != 0) {
        return -1;
    }
    
    domain_page_table_t *domain = &g_domain_tables[domain_id];
    
    if (domain->pml4 == NULL) {
        return -1;
    }
    
    /* Calculate page table flags based on map type */
    uint64_t pt_flags = PT_FLAG_PRESENT;
    
    switch (map_type) {
        case MAP_TYPE_CODE:
            pt_flags |= PT_FLAG_USER;  /* User mode executable */
            break;
        case MAP_TYPE_DATA:
            pt_flags |= PT_FLAG_WRITABLE | PT_FLAG_USER;
            break;
        case MAP_TYPE_READONLY:
            pt_flags |= PT_FLAG_USER;
            break;
        case MAP_TYPE_DEVICE:
            pt_flags |= PT_FLAG_WRITABLE | PT_FLAG_PCD | PT_FLAG_PWT;
            break;
        case MAP_TYPE_SHARED:
            pt_flags |= PT_FLAG_WRITABLE | PT_FLAG_USER;
            break;
    }
    
    /* Map pages (simplified - in real implementation would walk page tables) */
    uint64_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t current_virt = virt_addr + i * PAGE_SIZE;
        uint64_t current_phys = phys_addr + i * PAGE_SIZE;
        
        /* In real implementation, would walk PML4 -> PDPT -> PD -> PT */
        /* and set appropriate entries with pt_flags */
        /* For now, this is a placeholder */
        (void)current_virt;
        (void)current_phys;
        (void)pt_flags;
    }
    
    return 0;
}

/*
 * Unmap memory from domain's address space
 */
int isolation_unmap_memory(uint64_t domain_id, uint64_t virt_addr, uint64_t size) {
    if (domain_id >= MAX_DOMAINS) {
        return -1;
    }
    
    domain_page_table_t *domain = &g_domain_tables[domain_id];
    
    if (domain->pml4 == NULL) {
        return -1;
    }
    
    /* Unmap pages (simplified) */
    uint64_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t current_virt = virt_addr + i * PAGE_SIZE;
        
        /* In real implementation, would walk page tables and clear entries */
        (void)current_virt;
    }
    
    return 0;
}

/*
 * Verify domain has access to memory
 */
int isolation_verify_access(uint64_t domain_id, uint64_t addr, uint64_t size, uint32_t access) {
    if (domain_id >= MAX_DOMAINS) {
        return -1;
    }
    
    domain_page_table_t *domain = &g_domain_tables[domain_id];
    
    if (domain->pml4 == NULL) {
        return -1;
    }
    
    /* In real implementation, would walk page tables and verify permissions */
    /* For now, return success */
    (void)addr;
    (void)size;
    (void)access;
    
    return 0;
}

/*
 * Create call gate for service-to-service communication
 */
int isolation_create_call_gate(uint64_t target_domain, uint64_t entry_point, uint64_t cap_id) {
    if (target_domain >= MAX_DOMAINS) {
        return -1;
    }
    
    /* Verify capability */
    if (cap_check(cap_id, CAP_TYPE_SERVICE, CAP_PERM_EXECUTE) != 0) {
        return -1;
    }
    
    spin_lock(&g_call_gate_table.lock);
    
    if (g_call_gate_table.num_gates >= MAX_CALL_GATES) {
        spin_unlock(&g_call_gate_table.lock);
        return -1;
    }
    
    uint32_t gate_id = g_call_gate_table.num_gates;
    
    /* Initialize call gate */
    g_call_gate_table.gates[gate_id].offset_low = entry_point & 0xFFFF;
    g_call_gate_table.gates[gate_id].offset_high = (entry_point >> 16) & 0xFFFFFFFFFFFFULL;
    g_call_gate_table.gates[gate_id].selector = 0x08;  /* Kernel code segment */
    g_call_gate_table.gates[gate_id].ist = 0;
    g_call_gate_table.gates[gate_id].type = CALL_GATE_TYPE_AVAILABLE;
    g_call_gate_table.gates[gate_id].dpl = 3;  /* User level */
    g_call_gate_table.gates[gate_id].present = 1;
    
    g_call_gate_table.num_gates++;
    
    spin_unlock(&g_call_gate_table.lock);
    
    return gate_id;
}

/*
 * Call through call gate
 */
int isolation_call_gate(uint64_t gate_id, uint64_t *args) {
    if (gate_id >= g_call_gate_table.num_gates) {
        return -1;
    }
    
    call_gate_t *gate = &g_call_gate_table.gates[gate_id];
    
    if (!gate->present) {
        return -1;
    }
    
    /* In real implementation, would:
     * 1. Verify caller has capability for target service
     * 2. Switch stack
     * 3. Jump to entry point
     * 4. Handle return
     */
    (void)args;
    
    return 0;
}

/*
 * Get domain page tables
 */
domain_page_table_t* isolation_get_page_tables(uint64_t domain_id) {
    if (domain_id >= MAX_DOMAINS) {
        return NULL;
    }
    
    return &g_domain_tables[domain_id];
}

/*
 * Get call gate table
 */
call_gate_table_t* isolation_get_call_gates(void) {
    return &g_call_gate_table;
}