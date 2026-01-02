/*
 * HIK Core-0 Isolation Enforcement Implementation
 * 
 * This file implements page table configuration and call gates
 * for service isolation as specified in the documentation.
 */

#include "../include/isolation.h"
#include "../include/capability.h"
#include "../include/mm.h"
#include "../include/string.h"

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
 * Allocate a page table
 */
page_table_t* pt_alloc_page_table(void) {
    uint64_t phys_addr = mm_alloc(PAGE_SIZE, PAGE_SIZE, MEM_TYPE_KERNEL, 0);
    if (phys_addr == 0) {
        return NULL;
    }
    
    page_table_t *pt = (page_table_t *)phys_addr;
    memset(pt, 0, sizeof(page_table_t));
    
    return pt;
}

/*
 * Free a page table
 */
void pt_free_page_table(page_table_t *pt) {
    if (pt != NULL) {
        mm_free((uint64_t)pt);
    }
}

/*
 * Clear a page table
 */
int pt_clear_page_table(page_table_t *pt) {
    if (pt == NULL) {
        return -1;
    }
    
    memset(pt, 0, sizeof(page_table_t));
    return 0;
}

/*
 * Get page table entry
 */
uint64_t pt_get_entry(page_table_t *pt, uint64_t index) {
    if (pt == NULL || index >= 512) {
        return 0;
    }
    
    return pt->entries[index];
}

/*
 * Set page table entry
 */
void pt_set_entry(page_table_t *pt, uint64_t index, uint64_t entry) {
    if (pt != NULL && index < 512) {
        pt->entries[index] = entry;
    }
}

/*
 * Check if page table entry is present
 */
int pt_is_entry_present(page_table_t *pt, uint64_t index) {
    if (pt == NULL || index >= 512) {
        return 0;
    }
    
    return (pt->entries[index] & PT_FLAG_PRESENT) != 0;
}

/*
 * Get PML4 table for domain
 */
page_table_t* pt_walk_get_pml4(uint64_t domain_id) {
    if (domain_id >= MAX_DOMAINS) {
        return NULL;
    }
    
    return g_domain_tables[domain_id].pml4;
}

/*
 * Get PDPT table for virtual address
 */
page_table_t* pt_walk_get_pdpt(page_table_t *pml4, uint64_t vaddr) {
    if (pml4 == NULL) {
        return NULL;
    }
    
    uint64_t pml4_idx = PML4_INDEX(vaddr);
    uint64_t pml4_entry = pml4->entries[pml4_idx];
    
    if (!(pml4_entry & PT_FLAG_PRESENT)) {
        return NULL;
    }
    
    return (page_table_t *)PTE_GET_ADDRESS(pml4_entry);
}

/*
 * Get PD table for virtual address
 */
page_table_t* pt_walk_get_pd(page_table_t *pdpt, uint64_t vaddr) {
    if (pdpt == NULL) {
        return NULL;
    }
    
    uint64_t pdpt_idx = PDPT_INDEX(vaddr);
    uint64_t pdpt_entry = pdpt->entries[pdpt_idx];
    
    if (!(pdpt_entry & PT_FLAG_PRESENT)) {
        return NULL;
    }
    
    return (page_table_t *)PTE_GET_ADDRESS(pdpt_entry);
}

/*
 * Get PT table for virtual address
 */
page_table_t* pt_walk_get_pt(page_table_t *pd, uint64_t vaddr) {
    if (pd == NULL) {
        return NULL;
    }
    
    uint64_t pd_idx = PD_INDEX(vaddr);
    uint64_t pd_entry = pd->entries[pd_idx];
    
    if (!(pd_entry & PT_FLAG_PRESENT)) {
        return NULL;
    }
    
    return (page_table_t *)PTE_GET_ADDRESS(pd_entry);
}

/*
 * Get page table entry for virtual address
 */
uint64_t pt_walk_get_pte(page_table_t *pml4, uint64_t vaddr) {
    page_table_t *pdpt = pt_walk_get_pdpt(pml4, vaddr);
    if (pdpt == NULL) {
        return 0;
    }
    
    page_table_t *pd = pt_walk_get_pd(pdpt, vaddr);
    if (pd == NULL) {
        return 0;
    }
    
    page_table_t *pt = pt_walk_get_pt(pd, vaddr);
    if (pt == NULL) {
        return 0;
    }
    
    uint64_t pt_idx = PT_INDEX(vaddr);
    return pt->entries[pt_idx];
}

/*
 * Create page tables for a domain
 */
int isolation_create_page_tables(uint64_t domain_id, uint32_t flags) {
    if (domain_id >= MAX_DOMAINS) {
        return -1;
    }
    
    /* Allocate PML4 table */
    page_table_t *pml4 = pt_alloc_page_table();
    if (pml4 == NULL) {
        return -1;
    }
    
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
    
    /* TODO: Recursively free all page tables */
    /* For now, just free PML4 */
    pt_free_page_table(domain->pml4);
    
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
    
    /* Map pages */
    uint64_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t current_virt = virt_addr + i * PAGE_SIZE;
        uint64_t current_phys = phys_addr + i * PAGE_SIZE;
        
        /* Walk page tables and create missing levels */
        page_table_t *pml4 = domain->pml4;
        uint64_t pml4_idx = PML4_INDEX(current_virt);
        
        /* Get or create PDPT */
        page_table_t *pdpt = pt_walk_get_pdpt(pml4, current_virt);
        if (pdpt == NULL) {
            pdpt = pt_alloc_page_table();
            if (pdpt == NULL) {
                return -1;
            }
            uint64_t pdpt_phys = (uint64_t)pdpt;
            pt_set_entry(pml4, pml4_idx, pdpt_phys | PT_FLAG_PRESENT | PT_FLAG_WRITABLE | PT_FLAG_USER);
        }
        
        /* Get or create PD */
        uint64_t pdpt_idx = PDPT_INDEX(current_virt);
        page_table_t *pd = pt_walk_get_pd(pdpt, current_virt);
        if (pd == NULL) {
            pd = pt_alloc_page_table();
            if (pd == NULL) {
                return -1;
            }
            uint64_t pd_phys = (uint64_t)pd;
            pt_set_entry(pdpt, pdpt_idx, pd_phys | PT_FLAG_PRESENT | PT_FLAG_WRITABLE | PT_FLAG_USER);
        }
        
        /* Get or create PT */
        uint64_t pd_idx = PD_INDEX(current_virt);
        page_table_t *pt = pt_walk_get_pt(pd, current_virt);
        if (pt == NULL) {
            pt = pt_alloc_page_table();
            if (pt == NULL) {
                return -1;
            }
            uint64_t pt_phys = (uint64_t)pt;
            pt_set_entry(pd, pd_idx, pt_phys | PT_FLAG_PRESENT | PT_FLAG_WRITABLE | PT_FLAG_USER);
        }
        
        /* Set page table entry */
        uint64_t pt_idx = PT_INDEX(current_virt);
        pt_set_entry(pt, pt_idx, current_phys | pt_flags);
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
    
    /* Unmap pages */
    uint64_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t current_virt = virt_addr + i * PAGE_SIZE;
        
        /* Walk page tables */
        page_table_t *pt = pt_walk_get_pt(
            pt_walk_get_pd(
                pt_walk_get_pdpt(domain->pml4, current_virt),
                current_virt
            ),
            current_virt
        );
        
        if (pt != NULL) {
            uint64_t pt_idx = PT_INDEX(current_virt);
            pt_set_entry(pt, pt_idx, 0);
        }
    }
    
    /* Invalidate TLB */
    tlb_invalidate_page(virt_addr);
    
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
    
    /* Check each page */
    uint64_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t current_addr = addr + i * PAGE_SIZE;
        
        uint64_t pte = pt_walk_get_pte(domain->pml4, current_addr);
        
        if (!(pte & PT_FLAG_PRESENT)) {
            return -1;  /* Page not present */
        }
        
        if ((access & 0x01) && !(pte & PT_FLAG_USER)) {
            return -1;  /* Need user access but not allowed */
        }
        
        if ((access & 0x02) && !(pte & PT_FLAG_WRITABLE)) {
            return -1;  /* Need write access but not allowed */
        }
    }
    
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

/*
 * Invalidate TLB for a single page
 */
void tlb_invalidate_page(uint64_t addr) {
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

/*
 * Invalidate entire TLB
 */
void tlb_invalidate_all(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

/*
 * Invalidate TLB for an ASID (not supported on x86_64)
 */
void tlb_invalidate_asid(uint64_t asid) {
    (void)asid;
    tlb_invalidate_all();
}

/*
 * Setup identity mapping
 */
int pt_setup_identity_map(uint64_t domain_id, uint64_t start, uint64_t size, uint64_t flags) {
    return isolation_map_memory(domain_id, start, start, size, MAP_TYPE_DATA, 0);
}

/*
 * Setup kernel mapping
 */
int pt_setup_kernel_map(uint64_t domain_id, uint64_t kernel_start, uint64_t kernel_size) {
    uint64_t kernel_virt = KERNEL_CODE_BASE;
    return isolation_map_memory(domain_id, kernel_virt, kernel_start, kernel_size, MAP_TYPE_CODE, 0);
}

/*
 * Setup user mapping
 */
int pt_setup_user_map(uint64_t domain_id, uint64_t user_start, uint64_t user_size) {
    return isolation_map_memory(domain_id, USER_BASE, user_start, user_size, MAP_TYPE_DATA, 0);
}

/*
 * Check if address is in kernel space
 */
int is_kernel_address(uint64_t addr) {
    return addr >= KERNEL_BASE;
}

/*
 * Check if address is in user space
 */
int is_user_address(uint64_t addr) {
    return addr >= USER_BASE && addr < USER_LIMIT;
}

/*
 * Check if address is in device space
 */
int is_device_address(uint64_t addr) {
    return addr >= DEVICE_BASE;
}