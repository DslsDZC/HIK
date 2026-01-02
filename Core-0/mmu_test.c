/*
 * HIK Core-0 MMU Test Functions
 * 
 * This file contains test functions for verifying MMU functionality.
 */

#include "../include/isolation.h"
#include "../include/mm.h"
#include "../include/kernel.h"
#include "../include/string.h"

/*
 * Test page table allocation
 */
static int test_pt_allocation(void) {
    kernel_log("Testing page table allocation...\n");
    
    page_table_t *pt = pt_alloc_page_table();
    if (pt == NULL) {
        kernel_log("FAILED: Could not allocate page table\n");
        return -1;
    }
    
    /* Check if page table is cleared */
    for (int i = 0; i < 512; i++) {
        if (pt->entries[i] != 0) {
            kernel_log("FAILED: Page table not cleared\n");
            pt_free_page_table(pt);
            return -1;
        }
    }
    
    pt_free_page_table(pt);
    kernel_log("PASSED: Page table allocation\n");
    return 0;
}

/*
 * Test page table entry operations
 */
static int test_pt_entry_ops(void) {
    kernel_log("Testing page table entry operations...\n");
    
    page_table_t *pt = pt_alloc_page_table();
    if (pt == NULL) {
        kernel_log("FAILED: Could not allocate page table\n");
        return -1;
    }
    
    /* Set entry */
    uint64_t test_addr = 0x1000;
    uint64_t test_flags = PT_FLAG_PRESENT | PT_FLAG_WRITABLE | PT_FLAG_USER;
    pt_set_entry(pt, 0, test_addr | test_flags);
    
    /* Get entry */
    uint64_t entry = pt_get_entry(pt, 0);
    if (entry != (test_addr | test_flags)) {
        kernel_log("FAILED: Entry mismatch\n");
        pt_free_page_table(pt);
        return -1;
    }
    
    /* Check if present */
    if (!pt_is_entry_present(pt, 0)) {
        kernel_log("FAILED: Entry not marked as present\n");
        pt_free_page_table(pt);
        return -1;
    }
    
    pt_free_page_table(pt);
    kernel_log("PASSED: Page table entry operations\n");
    return 0;
}

/*
 * Test page table walking
 */
static int test_pt_walking(void) {
    kernel_log("Testing page table walking...\n");
    
    /* Create page tables for test domain */
    uint64_t test_domain = 1;
    if (isolation_create_page_tables(test_domain, DOMAIN_FLAG_KERNEL) != 0) {
        kernel_log("FAILED: Could not create page tables\n");
        return -1;
    }
    
    page_table_t *pml4 = pt_walk_get_pml4(test_domain);
    if (pml4 == NULL) {
        kernel_log("FAILED: Could not get PML4\n");
        return -1;
    }
    
    /* Test walking with missing tables */
    page_table_t *pdpt = pt_walk_get_pdpt(pml4, 0x1000);
    if (pdpt != NULL) {
        kernel_log("FAILED: Should not find PDPT for unmapped address\n");
        return -1;
    }
    
    kernel_log("PASSED: Page table walking\n");
    return 0;
}

/*
 * Test memory mapping
 */
static int test_memory_mapping(void) {
    kernel_log("Testing memory mapping...\n");
    
    /* Create page tables for test domain */
    uint64_t test_domain = 2;
    if (isolation_create_page_tables(test_domain, DOMAIN_FLAG_KERNEL) != 0) {
        kernel_log("FAILED: Could not create page tables\n");
        return -1;
    }
    
    /* Allocate physical memory */
    uint64_t phys_addr = mm_alloc(PAGE_SIZE, PAGE_SIZE, MEM_TYPE_KERNEL, test_domain);
    if (phys_addr == 0) {
        kernel_log("FAILED: Could not allocate physical memory\n");
        return -1;
    }
    
    /* Map memory */
    uint64_t virt_addr = 0x1000000;
    if (isolation_map_memory(test_domain, virt_addr, phys_addr, PAGE_SIZE, MAP_TYPE_DATA, 0) != 0) {
        kernel_log("FAILED: Could not map memory\n");
        return -1;
    }
    
    /* Verify mapping */
    page_table_t *pml4 = pt_walk_get_pml4(test_domain);
    uint64_t pte = pt_walk_get_pte(pml4, virt_addr);
    
    if (!(pte & PT_FLAG_PRESENT)) {
        kernel_log("FAILED: Mapped page not present\n");
        return -1;
    }
    
    if (PTE_GET_ADDRESS(pte) != phys_addr) {
        kernel_log("FAILED: Physical address mismatch\n");
        return -1;
    }
    
    if (!(pte & PT_FLAG_WRITABLE)) {
        kernel_log("FAILED: Page not writable\n");
        return -1;
    }
    
    kernel_log("PASSED: Memory mapping\n");
    return 0;
}

/*
 * Test memory unmapping
 */
static int test_memory_unmapping(void) {
    kernel_log("Testing memory unmapping...\n");
    
    /* Create page tables for test domain */
    uint64_t test_domain = 3;
    if (isolation_create_page_tables(test_domain, DOMAIN_FLAG_KERNEL) != 0) {
        kernel_log("FAILED: Could not create page tables\n");
        return -1;
    }
    
    /* Allocate and map memory */
    uint64_t phys_addr = mm_alloc(PAGE_SIZE, PAGE_SIZE, MEM_TYPE_KERNEL, test_domain);
    uint64_t virt_addr = 0x2000000;
    
    if (isolation_map_memory(test_domain, virt_addr, phys_addr, PAGE_SIZE, MAP_TYPE_DATA, 0) != 0) {
        kernel_log("FAILED: Could not map memory\n");
        return -1;
    }
    
    /* Unmap memory */
    if (isolation_unmap_memory(test_domain, virt_addr, PAGE_SIZE) != 0) {
        kernel_log("FAILED: Could not unmap memory\n");
        return -1;
    }
    
    /* Verify unmapping */
    page_table_t *pml4 = pt_walk_get_pml4(test_domain);
    uint64_t pte = pt_walk_get_pte(pml4, virt_addr);
    
    if (pte & PT_FLAG_PRESENT) {
        kernel_log("FAILED: Page still present after unmapping\n");
        return -1;
    }
    
    kernel_log("PASSED: Memory unmapping\n");
    return 0;
}

/*
 * Test address space checks
 */
static int test_address_space_checks(void) {
    kernel_log("Testing address space checks...\n");
    
    /* Test kernel address */
    if (!is_kernel_address(KERNEL_BASE)) {
        kernel_log("FAILED: Kernel base not recognized\n");
        return -1;
    }
    
    if (is_kernel_address(USER_BASE)) {
        kernel_log("FAILED: User base incorrectly recognized as kernel\n");
        return -1;
    }
    
    /* Test user address */
    if (!is_user_address(USER_BASE)) {
        kernel_log("FAILED: User base not recognized\n");
        return -1;
    }
    
    if (is_user_address(KERNEL_BASE)) {
        kernel_log("FAILED: Kernel base incorrectly recognized as user\n");
        return -1;
    }
    
    /* Test device address */
    if (!is_device_address(DEVICE_BASE)) {
        kernel_log("FAILED: Device base not recognized\n");
        return -1;
    }
    
    kernel_log("PASSED: Address space checks\n");
    return 0;
}

/*
 * Run all MMU tests
 */
int mmu_run_tests(void) {
    kernel_log("\n");
    kernel_log("========================================\n");
    kernel_log("Running MMU Tests\n");
    kernel_log("========================================\n\n");
    
    int failures = 0;
    
    if (test_pt_allocation() != 0) failures++;
    if (test_pt_entry_ops() != 0) failures++;
    if (test_pt_walking() != 0) failures++;
    if (test_memory_mapping() != 0) failures++;
    if (test_memory_unmapping() != 0) failures++;
    if (test_address_space_checks() != 0) failures++;
    
    kernel_log("\n");
    kernel_log("========================================\n");
    if (failures == 0) {
        kernel_log("All MMU tests PASSED\n");
    } else {
        kernel_log_hex(failures);
        kernel_log(" test(s) FAILED\n");
    }
    kernel_log("========================================\n\n");
    
    return failures;
}