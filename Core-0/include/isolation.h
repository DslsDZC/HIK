/*
 * HIK Core-0 Isolation Enforcement
 * 
 * This file defines the isolation enforcement mechanisms including
 * page table configuration and call gates for service isolation.
 */

#ifndef HIK_CORE0_ISOLATION_H
#define HIK_CORE0_ISOLATION_H

#include "stdint.h"

/* Page table entry flags */
#define PT_FLAG_PRESENT     0x01
#define PT_FLAG_WRITABLE    0x02
#define PT_FLAG_USER        0x04
#define PT_FLAG_PWT         0x08
#define PT_FLAG_PCD         0x10
#define PT_FLAG_ACCESSED    0x20
#define PT_FLAG_DIRTY       0x40
#define PT_FLAG_PS          0x80
#define PT_FLAG_GLOBAL      0x100
#define PT_FLAG_NX          0x8000000000000000ULL

/* Page table structure */
typedef struct {
    uint64_t entries[512];
} __attribute__((aligned(4096))) page_table_t;

/* Domain page table configuration */
typedef struct {
    page_table_t *pml4;           /* PML4 table */
    uint64_t domain_id;           /* Domain ID */
    uint64_t capabilities;        /* Capabilities for this domain */
    uint32_t flags;               /* Domain flags */
} domain_page_table_t;

/* Domain flags */
#define DOMAIN_FLAG_KERNEL    0x01  /* Kernel domain */
#define DOMAIN_FLAG_SERVICE   0x02  /* Service domain */
#define DOMAIN_FLAG_APP       0x04  /* Application domain */

/* Call gate descriptor (x86-64 call gate mechanism) */
typedef struct {
    uint64_t offset_low : 16;     /* Offset bits 0-15 */
    uint64_t selector : 16;       /* Code segment selector */
    uint64_t ist : 3;             /* Interrupt stack table */
    uint64_t type : 5;            /* Descriptor type */
    uint64_t dpl : 2;             /* Descriptor privilege level */
    uint64_t present : 1;         /* Present flag */
    uint64_t offset_high : 48;    /* Offset bits 16-63 */
} __attribute__((packed)) call_gate_t;

/* Call gate types */
#define CALL_GATE_TYPE_AVAILABLE 0xC  /* Available call gate */

/* Maximum number of call gates */
#define MAX_CALL_GATES 16

/* Call gate table */
typedef struct {
    call_gate_t gates[MAX_CALL_GATES];
    uint32_t num_gates;
    uint32_t lock;
} call_gate_table_t;

/* Memory mapping types */
typedef enum {
    MAP_TYPE_CODE = 0,        /* Executable code */
    MAP_TYPE_DATA = 1,        /* Read/write data */
    MAP_TYPE_READONLY = 2,    /* Read-only data */
    MAP_TYPE_DEVICE = 3,      /* Device MMIO */
    MAP_TYPE_SHARED = 4       /* Shared memory */
} map_type_t;

/* Address space layout */
#define KERNEL_BASE            0xFFFF800000000000ULL  /* Kernel base address */
#define KERNEL_CODE_BASE       0xFFFFFFFF80000000ULL  /* Kernel code base */
#define KERNEL_DATA_BASE       0xFFFF880000000000ULL  /* Kernel data base */
#define USER_BASE              0x0000000000400000ULL  /* User base address (4MB) */
#define USER_LIMIT             0x00007FFFFFFFFFFFULL  /* User limit (128TB) */
#define DEVICE_BASE            0xFFFFFE0000000000ULL  /* Device MMIO base */

/* Page table indices */
#define PML4_INDEX(vaddr)      (((vaddr) >> 39) & 0x1FF)
#define PDPT_INDEX(vaddr)      (((vaddr) >> 30) & 0x1FF)
#define PD_INDEX(vaddr)        (((vaddr) >> 21) & 0x1FF)
#define PT_INDEX(vaddr)        (((vaddr) >> 12) & 0x1FF)

/* Page table entry helpers */
#define PTE_GET_ADDRESS(pte)   ((pte) & 0x000FFFFFFFFFF000ULL)
#define PTE_SET_ADDRESS(pte, addr) (((pte) & 0xFFF0000000000FFFULL) | ((addr) & 0x000FFFFFFFFFF000ULL))
#define PTE_IS_PRESENT(pte)    ((pte) & PT_FLAG_PRESENT)
#define PTE_IS_WRITABLE(pte)   ((pte) & PT_FLAG_WRITABLE)
#define PTE_IS_USER(pte)       ((pte) & PT_FLAG_USER)

/* Initialize isolation system */
int isolation_init(void);

/* Create page tables for a domain */
int isolation_create_page_tables(uint64_t domain_id, uint32_t flags);

/* Destroy page tables for a domain */
int isolation_destroy_page_tables(uint64_t domain_id);

/* Map memory into domain's address space */
int isolation_map_memory(uint64_t domain_id, uint64_t virt_addr, uint64_t phys_addr,
                         uint64_t size, map_type_t map_type, uint64_t cap_id);

/* Unmap memory from domain's address space */
int isolation_unmap_memory(uint64_t domain_id, uint64_t virt_addr, uint64_t size);

/* Verify domain has access to memory */
int isolation_verify_access(uint64_t domain_id, uint64_t addr, uint64_t size, uint32_t access);

/* Create call gate for service-to-service communication */
int isolation_create_call_gate(uint64_t target_domain, uint64_t entry_point, uint64_t cap_id);

/* Call through call gate */
int isolation_call_gate(uint64_t gate_id, uint64_t *args);

/* Get domain page tables */
domain_page_table_t* isolation_get_page_tables(uint64_t domain_id);

/* Get call gate table */
call_gate_table_t* isolation_get_call_gates(void);

/* Page table allocation and management */
page_table_t* pt_alloc_page_table(void);
void pt_free_page_table(page_table_t *pt);
int pt_clear_page_table(page_table_t *pt);
uint64_t pt_get_entry(page_table_t *pt, uint64_t index);
void pt_set_entry(page_table_t *pt, uint64_t index, uint64_t entry);
int pt_is_entry_present(page_table_t *pt, uint64_t index);

/* Page table walking */
page_table_t* pt_walk_get_pml4(uint64_t domain_id);
page_table_t* pt_walk_get_pdpt(page_table_t *pml4, uint64_t vaddr);
page_table_t* pt_walk_get_pd(page_table_t *pdpt, uint64_t vaddr);
page_table_t* pt_walk_get_pt(page_table_t *pd, uint64_t vaddr);
uint64_t pt_walk_get_pte(page_table_t *pml4, uint64_t vaddr);

/* TLB management */
void tlb_invalidate_page(uint64_t addr);
void tlb_invalidate_all(void);
void tlb_invalidate_asid(uint64_t asid);

/* Identity mapping */
int pt_setup_identity_map(uint64_t domain_id, uint64_t start, uint64_t size, uint64_t flags);
int pt_setup_kernel_map(uint64_t domain_id, uint64_t kernel_start, uint64_t kernel_size);
int pt_setup_user_map(uint64_t domain_id, uint64_t user_start, uint64_t user_size);

/* Address space management */
int is_kernel_address(uint64_t addr);
int is_user_address(uint64_t addr);
int is_device_address(uint64_t addr);

#endif /* HIK_CORE0_ISOLATION_H */